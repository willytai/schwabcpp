#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>
#include <chrono>
#include <mutex>
#include <thread>
#include <memory>
#include <filesystem>

namespace schwabcpp {

using clock = std::chrono::system_clock;

class Streamer;

class Client
{
public:
                                Client(const std::filesystem::path& appCredentialPath = "./.appCredentials.json");
                                ~Client();

    void                        startStreamer();
    void                        stopStreamer();

    void                        pauseStreamer();
    void                        resumeStreamer();

private:
    void                        loadCredentials(const std::filesystem::path& appCredentialPath);
    void                        init();

    // --- OAuth Flow ---
    enum class TokenStatus : char {
        UpdateNotRequired,
        UpdateSucceeded,
        UpdateFailed,
        NoTokens,
    };
    bool                        loadTokens();
    std::string                 getAuthorizationCode();
    void                        getTokens(const std::string& grantType, const std::string& code, std::string& responseData);
    bool                        writeTokens(const clock::time_point& accessTokenTS, const clock::time_point& refreshTokenTS, const std::string& responseData);
    TokenStatus                 updateTokens();

    // --- Token Checker Daemon's Job ---
    void                        checkTokens();

    // --- Token Access for Streamer Class (Thread-Safe) ---
    friend class Streamer;
    std::string                 getAccessToken() const;

    // -- User Preferences
    bool                        requestUserPreferences(std::string& responseData) const;

private:
    // --- active tokens ---
    std::string                 m_accessToken;
    clock::time_point           m_accessTokenTS;

    std::string                 m_refreshToken;
    clock::time_point           m_refreshTokenTS;

    // --- app credentials ---
    std::string                 m_key;
    std::string                 m_secret;

    // --- to protect access to the active tokens ---
    mutable std::mutex          m_mutex;

    // --- token checker daemon ---
    std::thread                 m_tokenCheckerDaemon;
    bool                        m_stopCheckerDaemon;
    mutable std::mutex          m_tokenCheckerMutex;
    std::condition_variable     m_tokenCheckerCV;

    // --- streamer ---
    std::unique_ptr<Streamer>   m_streamer;
};

} // namespace schwabcpp

#endif // __CLIENT_H__
