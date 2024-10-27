#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>
#include <chrono>
#include <mutex>
#include <thread>
#include <memory>
#include <filesystem>

namespace l2viz {

using clock = std::chrono::system_clock;

class Streamer;

class Client
{
public:
                                Client(const std::filesystem::path& appCredentialPath = "./.appCredentials.json");
                                ~Client();

    void                        startStreamer();
    void                        stopStreamer();

private:
    void                        loadCredentials(const std::filesystem::path& appCredentialPath);
    void                        init();

    // --- OAuth Flow ---
    bool                        loadTokens();
    std::string                 getAuthorizationCode();
    void                        getTokens(const std::string& grantType, const std::string& code, std::string& responseData);
    void                        updateTokens();
    void                        writeTokens(const clock::time_point& accessTokenTS, const clock::time_point& refreshTokenTS, const std::string& responseData);

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
    std::atomic<bool>           m_stopCheckerDaemon;

    // --- streamer ---
    std::unique_ptr<Streamer>   m_streamer;
};

} // namespace l2viz

#endif // __CLIENT_H__
