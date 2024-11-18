#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>
#include <mutex>
#include <memory>
#include <filesystem>
#include "schwabcpp/schema/schema.h"
#include "schwabcpp/utils/timer.h"
#include "schwabcpp/utils/clock.h"

namespace spdlog {
class logger;
}

namespace schwabcpp {

class Streamer;

class Client
{
public:
    // --- allow custom callback when oauth is required ---
    enum class AuthRequestReason : char {
        InitialSetup,
        RefreshTokenExpired,
        PreviousAuthFailed,
    };
    enum class AuthStatus : char {
        Succeeded,
        Failed,
        NotRequired,
    };
    // params
    //   1. the url for the oauth
    //   2. the reason the request was triggered
    //   3. the number of chances left to run the oauth if something failed
    using OAuthUrlRequestCallback = std::function<std::string(const std::string&, AuthRequestReason, int)>;
    using OAuthCompleteCallback = std::function<void(AuthStatus)>;

    // --- allow named initialization with struct ---
    enum class LogLevel : char {
        Debug,
        Trace,

        Unspecified,
    };
    struct Spec {
        std::filesystem::path           appCredentialPath = "./.appCredentials.json";
        OAuthUrlRequestCallback         oAuthUrlRequestCallback = {};
        OAuthCompleteCallback           oAuthCompleteCallback = {};
        std::shared_ptr<spdlog::logger> logger = nullptr;
        LogLevel                        logLevel = LogLevel::Unspecified;
    };

                                        Client(const Spec& spec);
                                        ~Client();

    // --- api to set callabcks ---
    void                                onOAuthUrlRequest(OAuthUrlRequestCallback callback) { m_oAuthUrlRequestCallback = callback; }
    void                                onOAuthComplete(OAuthCompleteCallback callback) { m_oAuthCompleteCallback = callback; }

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
    enum class UpdateStatus : char {
        NotRequired,
        Succeeded,
        Failed,
    };
    AuthStatus                          runOAuth(AuthRequestReason reason, int chances = 3);  // you have 3 chances to run the oauth flow by default
    std::string                         getAuthorizationCode(AuthRequestReason reason, int chances);
    void                                getTokens(const std::string& grantType, const std::string& code, std::string& responseData);
    bool                                writeTokens(AccessTokenResponse response);
    bool                                writeTokens(RefreshTokenResponse response);

    // loads the token from file, need to check after loading, they migh be expired
    bool                                loadTokens();
    // updates the token
    UpdateStatus                        updateTokens();

    // --- Token Access for Streamer Class (Thread-Safe) ---
    friend class Streamer;
    std::string                         getAccessToken() const;

    // -- User Preferences
    bool                                requestUserPreferences(std::string& responseData) const;

    // -- Token Checker Daemon's Job ---
    void                                checkTokensAndReauth();

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

    // --- callbacks ---
    OAuthUrlRequestCallback             m_oAuthUrlRequestCallback;
    OAuthCompleteCallback               m_oAuthCompleteCallback;
};

} // namespace schwabcpp

#endif // __CLIENT_H__
