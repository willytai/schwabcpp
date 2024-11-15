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
        Trace
    };

                                        Client();
                                        Client(LogLevel level);
                                        Client(std::shared_ptr<spdlog::logger> logger);
                                        Client(const std::filesystem::path& appCredentialPath);
                                        Client(const std::filesystem::path& appCredentialPath, LogLevel level);
                                        Client(const std::filesystem::path& appCredentialPath, std::shared_ptr<spdlog::logger> logger);
                                        ~Client();

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
        UpdateNotRequired,
        UpdateSucceeded,
        UpdateFailed,
        NoTokens,
    };
    bool                                loadTokens();
    std::string                         getAuthorizationCode();
    void                                getTokens(const std::string& grantType, const std::string& code, std::string& responseData);
    bool                                writeTokens(const clock::time_point& accessTokenTS, const clock::time_point& refreshTokenTS, const std::string& responseData);
    TokenStatus                         updateTokens();

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
};

} // namespace schwabcpp

#endif // __CLIENT_H__
