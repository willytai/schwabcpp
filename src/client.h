#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>
#include <chrono>
#include <mutex>
#include <memory>
#include <filesystem>
#include "schema/schema.h"
#include "utils/timer.h"

namespace spdlog {
class logger;
}

namespace schwabcpp {

using clock = std::chrono::system_clock;

class Streamer;

class Client
{
public:
    enum class LogLevel : char {
        Debug,
        Trace,

        Unspecified,
    };

    // --- allow named initialization with struct ---
    typedef std::function<std::string(const std::string&)> OAuthCallback;
    struct Spec {
        std::filesystem::path           appCredentialPath = "./.appCredentials.json";
        OAuthCallback                   oAuthCallback = {};
        std::shared_ptr<spdlog::logger> logger = nullptr;
        LogLevel                        logLevel = LogLevel::Unspecified;
    };

                                        Client(const Spec& spec);
                                        ~Client();

    // --- streamer api ---
    void                                startStreamer();
    void                                stopStreamer();

    void                                pauseStreamer();
    void                                resumeStreamer();

    // --- sync api --- (returns string response, user is responsible of parsing)
    using HttpRequestQueries = std::unordered_map<std::string, std::string>;
    AccountSummary                      accountSummary(const std::string& accountNumber);
    AccountsSummaryMap                  accountSummary();
    std::string                         syncRequest(std::string url, HttpRequestQueries queries = {});  // common helper

private:
    void                                loadCredentials(const std::filesystem::path& appCredentialPath);
    void                                init(const std::filesystem::path& appCredentialPath,
                                             LogLevel level,
                                             std::shared_ptr<spdlog::logger> logger = nullptr);

    // --- OAuth Flow ---
    enum class TokenStatus : char {
        Good,
        Failed,
    };
    enum class UpdateStatus : char {
        NotRequired,
        Succeeded,
        Failed,
    };
    TokenStatus                         runOAuth();
    std::string                         getAuthorizationCode();
    void                                getTokens(const std::string& grantType, const std::string& code, std::string& responseData);
    bool                                writeTokens(const clock::time_point& accessTokenTS, const clock::time_point& refreshTokenTS, const std::string& responseData);

    // loads the token from file, need to check after loading, they migh be expired
    bool                                loadTokens();
    // updates the token
    UpdateStatus                        updateTokens();

    // --- Token Access for Streamer Class (Thread-Safe) ---
    friend class Streamer;
    std::string                         getAccessToken() const;

    // -- User Preferences
    bool                                requestUserPreferences(std::string& responseData) const;

private:
    // --- active tokens ---
    std::string                         m_accessToken;
    clock::time_point                   m_accessTokenTS;

    std::string                         m_refreshToken;
    clock::time_point                   m_refreshTokenTS;

    // --- app credentials ---
    std::string                         m_key;
    std::string                         m_secret;

    // --- to protect access to the active tokens ---
    mutable std::mutex                  m_mutex;

    // --- token checker daemon ---
    Timer                               m_tokenCheckerDaemon;

    // --- streamer ---
    std::unique_ptr<Streamer>           m_streamer;

    // --- oath url request callback ---
    std::function<std::string(const std::string&)>
                                        m_oAuthUrlRequestCallback;
};

} // namespace schwabcpp

#endif // __CLIENT_H__
