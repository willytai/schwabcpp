#include "client.h"
#include "streamer.h"
#include "base64.hpp"
#include "nlohmann/json.hpp"
#include "utils/logger.h"

#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>

namespace schwabcpp {

using json = nlohmann::json;

namespace {

// -- callbacks

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* buffer)
{
    size_t totalSize = size * nmemb;
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string defaultOAuthUrlRequestCallback(const std::string& url, Client::AuthRequestReason requestReason, int chances)
{
    // requests the redirected url from terminal
    switch (requestReason) {
        case Client::AuthRequestReason::InitialSetup: LOG_INFO("Please authorize to start the schwap client. You have {} chance(s) left.", chances); break;
        case Client::AuthRequestReason::RefreshTokenExpired: LOG_INFO("Token expired, please reauthorize. You have {} chance(s) left.", chances); break;
        case Client::AuthRequestReason::PreviousAuthFailed: LOG_ERROR("Previous authorization request failed. The redirected url expires rather fast. Make sure you paste it within 30 seconds. Please reauthorize. You have {} chance(s) left.", chances); break;
    }
    LOG_INFO("Go to: {} and login.", url);
    LOG_INFO("Paste the redirected url here after logging in:");

    std::string authorizationRedirectedURL;
    std::cin >> authorizationRedirectedURL;

    return authorizationRedirectedURL;
}

// -- some static variables

static std::string tokenCacheFile = ".tokens.json";

// -- utilities

static spdlog::level::level_enum to_spdlog_log_level(Client::LogLevel level)
{
    switch(level) {
        case Client::LogLevel::Debug: return spdlog::level::debug;
        case Client::LogLevel::Trace: return spdlog::level::trace;
        default: return spdlog::level::debug;
    }
}

const static std::string s_traderAPIBaseUrl = "https://api.schwabapi.com/trader/v1";

}

Client::Client(const Spec& spec)
{
    // create a logger unless one is already provided
    if (spec.logger) {
        Logger::setLogger(spec.logger);

        // if log level is also specified, overwite
        if (spec.logLevel != LogLevel::Unspecified) {
            Logger::setLogLevel(to_spdlog_log_level(spec.logLevel));
        }
    } else {
        // logger is not specified, create a default one with the provide log level
        // if log level not specified, defaults to debug
        Logger::init(to_spdlog_log_level(spec.logLevel));
    }

    LOG_INFO("Initializing client.");

    // we are going to to a bunch of curl, init it here
    curl_global_init(CURL_GLOBAL_DEFAULT);

    loadCredentials(spec.appCredentialPath);

    // set the OAuth callback
    m_oAuthUrlRequestCallback = spec.oAuthCallback ?
                                spec.oAuthCallback : defaultOAuthUrlRequestCallback;

    // authorization flow starts
    TokenStatus tokenStatus = TokenStatus::Failed;

    // try to get the tokens from the cache
    if (!loadTokens()) {
        // load failed (usually because of not cache data), run the full OAuth flow
        tokenStatus = runOAuth(AuthRequestReason::InitialSetup);
    } else {
        // loaded cached tokens, check if update required
        if (updateTokens() == UpdateStatus::Failed) {
            // this means tokens are loaded but refresh token expired, cannot update
            // need to reauthorize
            tokenStatus = runOAuth(AuthRequestReason::RefreshTokenExpired);
        } else {
            tokenStatus = TokenStatus::Good;
        }
    }

    if (tokenStatus == TokenStatus::Good) {
        // start the token checker daemon
        LOG_INFO("Launching token checker daemon.");
        m_tokenCheckerDaemon.start(
            std::chrono::seconds(30),                // update every 30 seconds
            std::bind(&Client::updateTokens, this)
        );

        // create the streamer
        m_streamer = std::make_unique<Streamer>(this);

        LOG_INFO("Client initialized.");
    } else {
        // TODO: client failed to initialize, should forbid any action on it.
        LOG_ERROR("Failed to initialize client, please try again later.");
    }

}

Client::~Client()
{
    LOG_INFO("Stopping client.");

    // reset streamer
    m_streamer.reset();

    // stop the token checker daemon
    LOG_TRACE("Shutting down token checker daemon...");
    m_tokenCheckerDaemon.stop();  // this blocks

    // curl cleanup
    curl_global_cleanup();

    // now, we relase the logger
    Logger::releaseLogger();
}

void Client::startStreamer()
{
    m_streamer->start();
}

void Client::stopStreamer()
{
    m_streamer->stop();
}

void Client::pauseStreamer()
{
    m_streamer->pause();
}

void Client::resumeStreamer()
{
    m_streamer->resume();
}

void Client::loadCredentials(const std::filesystem::path& appCredentialPath)
{
    if (std::filesystem::exists(appCredentialPath)) {
        std::ifstream file(appCredentialPath);
        if (file.is_open()) {
            json credentialData;
            file >> credentialData;

            if (credentialData.contains("app_key") &&
                credentialData.contains("app_secret")) {
                m_key = credentialData["app_key"];
                m_secret = credentialData["app_secret"];
            } else {
                LOG_FATAL("App credentials missing!!");
            }
        } else {
            LOG_FATAL("Unable to open the app credentials file: {}", appCredentialPath.string());
        }
    } else {
        LOG_FATAL("App credentials file: {} not found. Did you specify the right path?", appCredentialPath.string());
    }
}

// -- sync api
AccountSummary Client::accountSummary(const std::string& accountNumber)
{
    std::string finalUrl = s_traderAPIBaseUrl + "/accounts";

    // first, get the hash value for the account number
    if (!accountNumber.empty()) {
        std::string url = s_traderAPIBaseUrl + "/accounts/accountNumbers";
        std::vector<json> accountNumbersData = json::parse(syncRequest(url));
        // find the hash
        std::string accountHash;
        for (const auto& data : accountNumbersData) {
            if (data.at("accountNumber") == accountNumber) {
                accountHash = data.at("hashValue");
                break;
            }
        }
        // embed
        finalUrl += "/" + accountHash;
    }

    HttpRequestQueries queries = {
        // {"fields", "positions"}
    };

    // process the json data
    return json::parse(
        std::move(
            syncRequest(
                finalUrl, std::move(queries)
            )
        )
    ).get<AccountSummary>();
}

AccountsSummaryMap Client::accountSummary()
{
    std::string finalUrl = s_traderAPIBaseUrl + "/accounts";

    HttpRequestQueries queries = {
        // {"fields", "positions"}
    };

    // process the json data
    return json::parse(
        std::move(
            syncRequest(
                finalUrl, std::move(queries)
            )
        )
    ).get<AccountsSummaryMap>();
}


std::string Client::syncRequest(std::string url, HttpRequestQueries queries)
{

    std::string response;

    // init curl
    CURL* curl = curl_easy_init();
    if (curl) {
        // embed queries
        if (!queries.empty()) {
            std::string queryString = std::accumulate(
                queries.begin(),
                queries.end(),
                std::string(),
                [] (std::string acc, const std::pair<std::string, std::string>& val) {
                    if (!acc.empty()) {
                        acc += "&";
                    }
                    return acc + val.first + "=" + val.second;
                }
            );
            url += "?" + queryString;
        }

        // set the url for the request
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + getAccessToken()).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 5 seconds timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        // write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // send
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            response.clear();
            LOG_ERROR("curl_easy_perform(...) failed: {}", curl_easy_strerror(res));
        } else {
            LOG_TRACE("Response data: {}", response);
        }

        // cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return response;
}

// -- OAuth Flow

bool Client::loadTokens()
{
    bool result = false;

    LOG_INFO("Loading token cache.");

    if (std::filesystem::exists(tokenCacheFile)) {
        std::ifstream tokenCache(tokenCacheFile);
        if (tokenCache.good()) {
            json cachedData;
            tokenCache >> cachedData;

            if (cachedData.contains("access_token") &&
                cachedData.contains("access_token_ts") &&
                cachedData.contains("refresh_token") &&
                cachedData.contains("refresh_token_ts"))
            {
                m_accessToken  = cachedData["access_token"];
                m_refreshToken = cachedData["refresh_token"];

                // some annoying time point and duration conversion
                clock::rep accessTokenTSCount  = static_cast<clock::rep>(cachedData["access_token_ts"]);
                clock::rep refreshTokenTSCount = static_cast<clock::rep>(cachedData["refresh_token_ts"]);
                clock::duration accessTokenTSSinceEpoch(accessTokenTSCount);
                clock::duration refreshTokenTSSinceEpoch(refreshTokenTSCount);
                m_accessTokenTS = clock::time_point(accessTokenTSSinceEpoch);
                m_refreshTokenTS = clock::time_point(refreshTokenTSSinceEpoch);

                result = true;

                LOG_DEBUG("{} seconds passed since access token last generated.",
                          std::chrono::duration_cast<std::chrono::seconds>(clock::now() - m_accessTokenTS).count());
                LOG_DEBUG("{} hours passed since refresh token last generated.",
                          std::chrono::duration_cast<std::chrono::hours>(clock::now() - m_refreshTokenTS).count());
                LOG_INFO("Tokens loaded.");
            } else {
                LOG_INFO("Token data corruptedu please reauthorize.");
            }
        } else {
            LOG_INFO("Token cache corrupted, please reauthorize.");
        }
    } else {
        LOG_INFO("Token cache not found, authorization required.");
    }

    return result;
}

Client::TokenStatus Client::runOAuth(AuthRequestReason requestReqson, int chances)
{
    if (chances == 0) {
        LOG_ERROR("You have no more chances left to authorize the client.");
        return TokenStatus::Failed;
    }

    TokenStatus tokenStatus = TokenStatus::Failed;

    // Step 1 -- Get the authorization code
    std::string authorizationCode = getAuthorizationCode(requestReqson, chances);

    // Step 2 -- Get the tokens
    std::string responseData;
    getTokens("authorization_code", authorizationCode, responseData);

    // Step 3 -- Write the tokens
    if (writeTokens(std::move(json::parse(responseData).get<AccessTokenResponse>()))) {
        tokenStatus = TokenStatus::Good;
    }

    // Step 4 -- Rerun if failed with chances left
    if (tokenStatus != TokenStatus::Good) {
        runOAuth(requestReqson, chances-1);
    }

    return tokenStatus;
}

Client::UpdateStatus Client::updateTokens()
{
    // NOTE:
    // This automatically pauses the streamer while updating the tokens
    // and resumes it after a successful update if the streamer is active.

    UpdateStatus status = UpdateStatus::NotRequired;

    // 7 days according to schwab
    const clock::duration __refresh_token_timeout(std::chrono::hours(7 * 24));

    // 30 min according to schwab
    const clock::duration __access_token_timeout(std::chrono::minutes(30));

    // 1 hour buffer time for refresh token update
    const clock::duration __refresh_token_update_threshold(std::chrono::hours(1));

    // 1 minute buffer time for access token update
    const clock::duration __access_token_update_threshold(std::chrono::minutes(1));

    // check if we need to reauthorize
    clock::duration refreshTokenValidTime = (__refresh_token_timeout - (clock::now() - m_refreshTokenTS));
    if (refreshTokenValidTime < __refresh_token_update_threshold) {
        LOG_WARN("Refresh token expired, please reauthorize.");
        return UpdateStatus::Failed;
    }

    // check if we need to update access token
    clock::duration accessTokenValidTime = (__access_token_timeout - (clock::now() - m_accessTokenTS));
    if (accessTokenValidTime < __access_token_update_threshold) {
        LOG_INFO("Access token expired, updating automatically.");

        // pause streamer if active
        if (m_streamer && m_streamer->isActive()) {
            m_streamer->pause();
        }

        // request access token with the refresh token
        std::string responseData;
        getTokens("refresh_token", m_refreshToken, responseData);

        // save the tokens
        if (!writeTokens(std::move(json::parse(responseData).get<RefreshTokenResponse>()))) {
            status = UpdateStatus::Failed;
        } else {
            status = UpdateStatus::Succeeded;

            // resume streamer if paused
            if (m_streamer && m_streamer->isPaused()) {
                m_streamer->resume();
            }
        }
    }

    return status;
}

bool Client::writeTokens(AccessTokenResponse responseData)
{
    bool result = false;

    if (!responseData.isError) {
        // critical section
        {
            std::lock_guard<std::mutex> guard(m_mutex);

            m_accessToken    = responseData.data.accessToken;
            m_accessTokenTS  = clock::time_point(clock::duration(responseData.data.accessTokenTS));
            m_refreshToken   = responseData.data.refreshToken;
            m_refreshTokenTS = clock::time_point(clock::duration(responseData.data.refreshTokenTS));
        }

        // tokens written
        result = true;

        // cache the tokens
        std::ofstream tokenCache(tokenCacheFile, std::ofstream::trunc);
        if (tokenCache.is_open()) {
            tokenCache << json{responseData}.dump(4);

            LOG_DEBUG("Tokens cached to {}.", tokenCacheFile);
        } else {
            LOG_ERROR("Unable to open {} for caching.", tokenCacheFile);
        }
    } else {
        LOG_ERROR("Unable to get tokens. Error: {}, {}", responseData.error.error, responseData.error.description);
    }

    return result;
}

bool Client::writeTokens(RefreshTokenResponse responseData)
{
    bool result = false;

    if (!responseData.isError) {
        // when writing tokens as the RefreshTokenResponse type, the refresh token timestamp should stay the same
        // this is the flow where user is requesting new access token with the refresh token
        responseData.data.refreshTokenTS = m_refreshTokenTS.time_since_epoch().count();

        // critical section
        {
            std::lock_guard<std::mutex> guard(m_mutex);

            m_accessToken    = responseData.data.accessToken;
            m_accessTokenTS  = clock::time_point(clock::duration(responseData.data.accessTokenTS));
            m_refreshToken   = responseData.data.refreshToken;
            m_refreshTokenTS = clock::time_point(clock::duration(responseData.data.refreshTokenTS));
        }

        // tokens written
        result = true;

        // cache the tokens
        std::ofstream tokenCache(tokenCacheFile, std::ofstream::trunc);
        if (tokenCache.is_open()) {
            tokenCache << json{responseData}.dump(4);

            LOG_DEBUG("Tokens cached to {}.", tokenCacheFile);
        } else {
            LOG_ERROR("Unable to open {} for caching.", tokenCacheFile);
        }
    } else {
        LOG_ERROR("Unable to get access token. Error: {}, {}", responseData.error.error, responseData.error.description);
    }

    return result;
}

void Client::getTokens(const std::string& grantType, const std::string& code, std::string& responseData)
{
    const std::string __accessTokenURL = "https://api.schwabapi.com/v1/oauth/token";
    const std::string __redirectUri = "https://127.0.0.1";
    const std::string __base64Credentials = base64::to_base64(m_key + ":" + m_secret);

    // init curl
    CURL* curl = curl_easy_init();
    if (curl) {
        // set the url for the post request
        curl_easy_setopt(curl, CURLOPT_URL, __accessTokenURL.c_str());

        // headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Basic " + __base64Credentials).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // data
        std::string data;
        if (grantType == "authorization_code") {
            data = "grant_type=authorization_code&code=" + code + "&redirect_uri=" + __redirectUri;
        } else if (grantType == "refresh_token") {
            data = "grant_type=refresh_token&refresh_token=" + code;
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        // write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // send it
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseData.clear();
            LOG_ERROR("curl_easy_perform(...) failed: {}", curl_easy_strerror(res));
        } else {
            LOG_TRACE("Response data: {}", json::parse(responseData).dump(4));
        }

        // cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

std::string Client::getAuthorizationCode(AuthRequestReason reason, int chances)
{
    std::string result;

    const std::string authorizationURL =
        "https://api.schwabapi.com/v1/oauth/authorize?client_id=" + m_key +
        "&redirect_uri=https://127.0.0.1";

    // request the redirected url with the callback
    std::string authorizationRedirectedURL = m_oAuthUrlRequestCallback(authorizationURL, reason, chances);

    // this should be in the form of https://{APP_CALLBACK_URL}/?code={AUTHORIZATION_CODE}&session={SESSION_ID}
    size_t start = authorizationRedirectedURL.find("?code=");
    if (start != std::string::npos) {
        start += 6;
    }
    size_t end = authorizationRedirectedURL.find("&session=");
    if (start != std::string::npos && end != std::string::npos) {
        result = authorizationRedirectedURL.substr(start, end-start);
    } else {
        LOG_ERROR("Unable to extract authorization code from: {}.", authorizationRedirectedURL);
    }

    LOG_TRACE("authorizationCode: {}", result);

    return result;
}

// -- Thread Safe Token Access
std::string Client::getAccessToken() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_accessToken;
}

bool Client::requestUserPreferences(std::string& responseData) const
{
    const std::string __preferenceURL = s_traderAPIBaseUrl + "/userPreference";

    bool result = false;

    // init curl
    CURL* curl = curl_easy_init();
    if (curl) {
        // set the url for the request
        curl_easy_setopt(curl, CURLOPT_URL, __preferenceURL.c_str());

        // header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + getAccessToken()).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 5 seconds timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        // write callback
        responseData.clear();
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // send
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseData.clear();
            LOG_ERROR("curl_easy_perform(...) failed: {}", curl_easy_strerror(res));
        } else {
            result = true;
            LOG_TRACE("Response data: {}", responseData);
        }

        // cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return result;
}

} // namespace schwabcpp
