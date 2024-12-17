#include "client.h"
#include "schema/userPreference.h"
#include "streamer.h"
#include "base64.hpp"
#include "nlohmann/json.hpp"
#include "utils/logger.h"

#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <string>

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

std::string defaultOAuthUrlRequestCallback(OAuthUrlRequestEvent& event)
{
    // requests the redirected url from terminal
    switch (event.getReason()) {
        case schwabcpp::OAuthUrlRequestEvent::Reason::InitialSetup: LOG_INFO("Please authorize to start the schwap client. You have {} chance(s) left.", event.getChances()); break;
        case schwabcpp::OAuthUrlRequestEvent::Reason::RefreshTokenExpired: LOG_INFO("Token expired, please reauthorize. You have {} chance(s) left.", event.getChances()); break;
        case schwabcpp::OAuthUrlRequestEvent::Reason::PreviousAuthFailed: LOG_ERROR("Previous authorization request failed. The redirected url expires rather fast. Make sure you paste it within 30 seconds. Please reauthorize. You have {} chance(s) left.", event.getChances()); break;
    }
    LOG_INFO("Go to: {} and login.", event.getAuthorizationUrl());
    LOG_INFO("Paste the redirected url here after logging in:");

    std::string authorizationRedirectedURL;
    std::cin >> authorizationRedirectedURL;

    return authorizationRedirectedURL;
}

void defaultOAuthCompleteCallback(OAuthCompleteEvent& event)
{
    switch (event.getStatus()) {
        case schwabcpp::OAuthCompleteEvent::Status::Succeeded: LOG_INFO("OAuth successful."); break;
        case schwabcpp::OAuthCompleteEvent::Status::Failed: LOG_ERROR("OAuth failed."); break;
        case schwabcpp::OAuthCompleteEvent::Status::NotRequired: LOG_INFO("OAuth not required."); break;
    }
}

// -- some static variables

const static std::string tokenCacheFile = ".tokens.json";
const static std::string s_traderAPIBaseUrl = "https://api.schwabapi.com/trader/v1";
const static std::string s_marketAPIBaseUrl = "https://api.schwabapi.com/marketdata/v1";

}

Client::Client(const std::string& key, const std::string& secret, std::shared_ptr<spdlog::logger> logger)
    : m_key(key)
    , m_secret(secret)
    , m_eventCallback({})   // default empty callback
{
    // create a logger unless one is already provided
    if (logger) {
        Logger::setLogger(logger);
    } else {
        // logger is not specified, create a default one with the provide log level
        // if log level not specified, defaults to debug
        Logger::init(spdlog::level::debug);
    }

    // we are going to to a bunch of curl, init it here
    curl_global_init(CURL_GLOBAL_DEFAULT);

    LOG_INFO("Schwab client initialized.");
}

Client::~Client()
{
    LOG_INFO("Stopping client...");

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

bool Client::connect()
{
    // authorization flow starts
    AuthStatus authStatus = AuthStatus::Failed;

    // try to get the tokens from the cache
    if (!loadTokens()) {
        // load failed (usually because of not cache data), run the full OAuth flow
        authStatus = runOAuth(AuthRequestReason::InitialSetup);
    } else {
        // loaded cached tokens, check if update required
        UpdateStatus updateStatus = updateTokens();
        switch (updateStatus) {
            case UpdateStatus::NotRequired: {
                authStatus = AuthStatus::NotRequired;
                break;
            }
            case UpdateStatus::Succeeded: {
                authStatus = AuthStatus::Succeeded;
                break;
            }
            case UpdateStatus::FailedExpired: {
                // this means tokens are loaded but refresh token expired, cannot update
                // need to reauthorize
                authStatus = runOAuth(AuthRequestReason::RefreshTokenExpired);
                break;
            }
            case UpdateStatus::FailedBadData: {
                // TODO: figure out what to do here
                break;
            }

            default: LOG_ERROR("Unrecognized UpdateStatus. Fix this!!"); break;
        }
    }

    bool result = authStatus == AuthStatus::Succeeded
               || authStatus == AuthStatus::NotRequired;

    if (result) {
        LOG_INFO("Schwab client authorized.");

        // cache linked accounts info as this does not change after authorization
        updateLinkedAccounts();

        // also cache user preference
        updateUserPreference();

        // start the token checker daemon
        LOG_DEBUG("Launching token checker daemon...");
        m_tokenCheckerDaemon.start(
            std::chrono::seconds(30),
            std::bind(&Client::checkTokensAndReauth, this)
        );

        // create the streamer (do this last so that the user preference is ready to use)
        m_streamer = std::make_unique<Streamer>(this);
    } else {
        // TODO: client failed to initialize, should forbid any action on it.
        LOG_ERROR("Failed to authorize client, please try again later.");
    }

    // create the event object
    OAuthCompleteEvent event(authStatus);

    // trigger callback
    if (m_eventCallback) {
        m_eventCallback(event);
    }

    // trigger default handler if not handled by the callback
    if (!event.getHandled()) {
        defaultOAuthCompleteCallback(event);
    }

    return result;
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

void Client::setStreamerDataHandler(std::function<void(const std::string&)> handler)
{
    m_streamer->setDataHandler(handler);
}

// -- sync api
AccountSummary Client::accountSummary(const std::string& accountNumber) const
{
    std::string finalUrl = s_traderAPIBaseUrl + "/accounts";

    // embed
    {
        std::lock_guard lock(m_mutexLinkedAccounts);
        if (m_linkedAccounts.contains(accountNumber)) {
            finalUrl += "/" + m_linkedAccounts.at(accountNumber);
        }
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

AccountsSummaryMap Client::accountSummary() const
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

CandleList Client::priceHistory(const std::string& ticker,
                                PeriodType periodType,
                                int period,
                                FrequencyType frequencyType,
                                int frequency,
                                std::optional<clock::time_point> start,
                                std::optional<clock::time_point> end,
                                bool needExtendedHoursData,
                                bool needPreviousClose) const
{
    std::string finalUrl = s_marketAPIBaseUrl + "/pricehistory";

    HttpRequestQueries queries = {
        {"symbol", ticker},
        {"periodType", periodType.toString()},
        {"period", std::to_string(period)},
        {"frequencyType", frequencyType.toString()},
        {"frequency", std::to_string(frequency)},
        {"needExtendedHoursData", needExtendedHoursData ? "true" : "false"},
        {"needPreviousClose", needPreviousClose ? "true" : "false"},
    };

    if (start.has_value()) {
        queries.emplace("startDate", std::to_string(start.value().time_since_epoch().count()));
    }
    if (end.has_value()) {
        queries.emplace("endDate", std::to_string(end.value().time_since_epoch().count()));
    }

    return json::parse(
        std::move(
            syncRequest(
                finalUrl, std::move(queries)
            )
        )
    ).get<CandleList>();
}

MarketHours Client::marketHours(MarketType marketType, std::optional<clock::time_point> date) const
{
    // NOTE:
    // The API is returning garbage when market type is anything but Equity for some reason.
    // Don't use the other!
    std::string finalUrl = s_marketAPIBaseUrl + "/markets/" + marketType.toString();

    // covert to time_t
    std::time_t time = clock::to_time_t(date.value_or(clock::now()));

    // local time
    std::tm tm = *std::localtime(&time);

    // format as YYYY-MM-DD
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");

    HttpRequestQueries queries = {
        {"date", oss.str()},
    };

    // send
    json response = json::parse(
        std::move(
            syncRequest(
                finalUrl, std::move(queries)
            )
        )
    );

    // response is in the form of
    // {
    //     <marketType>: {
    //         <product>: MarketHours
    //         <product>: MarketHours
    //         .
    //         .
    //     }
    // }
    //
    // I'll retrieve the 1st data matching the market type
    MarketHours result;
    if (response.contains(marketType.toString())) {
        auto matched = response[marketType.toString()];
        for (auto it = matched.begin(); it != matched.end(); ++it) {
            it.value().get_to(result);
            break;
        }
    }

    return std::move(result);
}

std::string Client::syncRequest(std::string url, HttpRequestQueries queries) const
{
    // initialize to empty
    std::string response("{}");

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

        LOG_TRACE("Request URL: {}", url);

        // set the url for the request
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + getAccessToken()).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 5 seconds timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        // write callback
        response.clear();
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // send
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            response = "{}";
            LOG_ERROR("In {}, curl_easy_perform(...) failed: {}", __PRETTY_FUNCTION__, curl_easy_strerror(res));
        } else {
            LOG_TRACE("Response data: {}", response);
        }

        // cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return response;
}

// async api (mostly for interacting with the streamer)
//
// It is safe to call all these functions before the streamer starts. All these requests are queued.
// The streamer will send them once it is online.
void Client::subscribeLevelOneEquities(const std::vector<std::string>& tickers,
                                       const std::vector<StreamerField::LevelOneEquity>& fields)
{
    m_streamer->subscribeLevelOneEquities(tickers, fields);
}

// -- Thread Safe Accessors
std::vector<std::string>
Client::getLinkedAccounts() const
{
    std::lock_guard lock(m_mutexLinkedAccounts);

    std::vector<std::string> result;
    for (const auto& [key, _] : m_linkedAccounts) {
        result.push_back(key);
    }
    return std::move(result);
}

UserPreference
Client::getUserPreference() const
{
    std::lock_guard lock(m_mutexUserPreference);

    return m_userPreference;
}

std::string
Client::getAccessToken() const
{
    std::lock_guard<std::mutex> guard(m_mutexTokens);
    return m_accessToken;
}

UserPreference::StreamerInfo
Client::getStreamerInfo() const
{
    std::lock_guard lock(m_mutexUserPreference);

    try {
        return m_userPreference.streamerInfo.at(0);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to retrieve streamer info: {}.", e.what());
        return UserPreference::StreamerInfo{};
    }
}

// -- OAuth Flow

bool Client::loadTokens()
{
    bool result = false;

    LOG_DEBUG("Loading token cache...");

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

                LOG_TRACE("{} seconds passed since access token last generated.",
                          std::chrono::duration_cast<std::chrono::seconds>(clock::now() - m_accessTokenTS).count());
                LOG_TRACE("{} hours passed since refresh token last generated.",
                          std::chrono::duration_cast<std::chrono::hours>(clock::now() - m_refreshTokenTS).count());
                LOG_DEBUG("Tokens loaded.");
            } else {
                LOG_DEBUG("Token data corrupted please reauthorize.");
            }
        } else {
            LOG_DEBUG("Token cache corrupted, please reauthorize.");
        }
    } else {
        LOG_DEBUG("Token cache not found, authorization required.");
    }

    return result;
}

Client::AuthStatus Client::runOAuth(AuthRequestReason requestReqson, int chances)
{
    if (chances == 0) {
        LOG_ERROR("You have no more chances left to authorize the client.");
        return AuthStatus::Failed;
    }

    AuthStatus authStatus = AuthStatus::Failed;

    // Step 1 -- Get the authorization code
    std::string authorizationCode = getAuthorizationCode(requestReqson, chances);

    // Step 2 -- Get the tokens
    std::string responseData;
    getTokens("authorization_code", authorizationCode, responseData);

    // Step 3 -- Write the tokens
    if (writeTokens(json::parse(responseData).get<AccessTokenResponse>())) {
        authStatus = AuthStatus::Succeeded;
    }

    // Step 4 -- Rerun if failed with chances left
    if (authStatus == AuthStatus::Failed) {
        authStatus = runOAuth(AuthRequestReason::PreviousAuthFailed, chances-1);
    }

    return authStatus;
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
        return UpdateStatus::FailedExpired;
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
        if (!writeTokens(json::parse(responseData).get<RefreshTokenResponse>())) {
            status = UpdateStatus::FailedBadData;
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
            std::lock_guard<std::mutex> guard(m_mutexTokens);

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
            tokenCache << json(responseData).dump(4);

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
            std::lock_guard<std::mutex> guard(m_mutexTokens);

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
            tokenCache << json(responseData).dump(4);

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

    // initialize to empty
    responseData = "{}";

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
        responseData.clear();
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        // send it
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseData = "{}";
            LOG_ERROR("In {}, curl_easy_perform(...) failed: {}", __PRETTY_FUNCTION__, curl_easy_strerror(res));
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

    // request the redirected url
    // create the event object
    OAuthUrlRequestEvent event(authorizationURL, reason, chances);

    // trigger callback
    if (m_eventCallback) {
        m_eventCallback(event);
    }

    // trigger default handler if not handled by the callback or no reply received
    std::string authorizationRedirectedURL;
    if (event.getHandled() && !event.getReply().empty()) {
        authorizationRedirectedURL = event.getReply();
    } else {
        authorizationRedirectedURL = defaultOAuthUrlRequestCallback(event);
    }

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

void Client::checkTokensAndReauth()
{
    // call updateTokens, rerun oauth if failed because expired
    // if failed because of bad data, we'll do nothing and let this function get triggered
    // again sometime later
    switch (updateTokens()) {

        case UpdateStatus::FailedExpired: {
            AuthStatus status = runOAuth(AuthRequestReason::RefreshTokenExpired);

            // update cached info
            if (status == AuthStatus::Succeeded) {
                updateLinkedAccounts();
                updateUserPreference();
            }

            // create the event object
            OAuthCompleteEvent event(status);

            // trigger callback
            if (m_eventCallback) {
                m_eventCallback(event);
            }

            // trigger default handler if not handled by the callback
            if (!event.getHandled()) {
                defaultOAuthCompleteCallback(event);
            }

            break;
        }

        case UpdateStatus::FailedBadData: {
            LOG_WARN("Failed to update tokens due to corrupted data. Will run the update again later. (Check your internet connection)");
            break;
        }

        case UpdateStatus::NotRequired: {
            // LOG_TRACE("Tokens update not required.");  // too annoying
            break;
        }

        case UpdateStatus::Succeeded: {
            LOG_INFO("Successfuly updated tokens.");

            // update preference
            updateUserPreference();

            break;
        }
    }
}

void Client::updateLinkedAccounts()
{
    std::string url = s_traderAPIBaseUrl + "/accounts/accountNumbers";
    try {
        std::vector<json> accountNumbersData = json::parse(syncRequest(url));
        {
            std::lock_guard lock(m_mutexLinkedAccounts);
            for (const auto& data : accountNumbersData) {
                m_linkedAccounts[data.at("accountNumber")] = data.at("hashValue");
            }
        }
    
        LOG_DEBUG("Linked accounts info cached.");
    } catch (...) {
        // TODO:
        NOTIMPLEMENTEDERROR;
    }
}

void Client::updateUserPreference()
{
    std::string url = s_traderAPIBaseUrl + "/userPreference";
    try {
        auto data = json::parse(syncRequest(url));
        {
            std::lock_guard lock(m_mutexUserPreference);
            data.get_to(m_userPreference);
        }
    
        LOG_DEBUG("User preference cached.");

        // update streamer
        if (m_streamer) {
            m_streamer->updateStreamerInfo(getStreamerInfo());
        }
    } catch (...) {
        // TODO:
        NOTIMPLEMENTEDERROR;
    }
}

} // namespace schwabcpp
