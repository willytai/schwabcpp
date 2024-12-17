#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>
#include <mutex>
#include <memory>
#include "schwabcpp/streamerField.h"
#include "schwabcpp/event/oAuthCompleteEvent.h"
#include "schwabcpp/event/oAuthUrlRequestEvent.h"
#include "schwabcpp/schema/accessTokenResponse.h"
#include "schwabcpp/schema/accountSummary.h"
#include "schwabcpp/schema/candleList.h"
#include "schwabcpp/schema/marketHours.h"
#include "schwabcpp/schema/refreshTokenResponse.h"
#include "schwabcpp/schema/userPreference.h"
#include "schwabcpp/types/marketType.h"
#include "schwabcpp/types/periodType.h"
#include "schwabcpp/types/frequencyType.h"
#include "schwabcpp/utils/timer.h"
#include "schwabcpp/utils/clock.h"

namespace spdlog {
class logger;
}

namespace schwabcpp {

class Streamer;

class Client
{
    using EventCallbackFn = std::function<void(Event&)>;
    using AuthStatus = OAuthCompleteEvent::Status;
    using AuthRequestReason = OAuthUrlRequestEvent::Reason;
public:
                                        Client(
                                            const std::string& key,
                                            const std::string& secret,
                                            std::shared_ptr<spdlog::logger> logger
                                        );
                                        ~Client();

    bool                                connect();

    // --- api to set custom event callabck ---
    void                                setEventCallback(EventCallbackFn fn) { m_eventCallback = fn; }

    // --- streamer api ---
    void                                startStreamer();
    void                                stopStreamer();

    void                                pauseStreamer();
    void                                resumeStreamer();

    void                                setStreamerDataHandler(std::function<void(const std::string&)> handler);

    // --- sync api --- (returns string response, user is responsible of parsing)
    using HttpRequestQueries = std::unordered_map<std::string, std::string>;
    AccountSummary                      accountSummary(const std::string& accountNumber) const;
    AccountsSummaryMap                  accountSummary() const;
    CandleList                          priceHistory(const std::string& ticker,
                                                     PeriodType periodType,
                                                     int period,
                                                     FrequencyType frequencyType,
                                                     int frequency,
                                                     std::optional<clock::time_point> start,
                                                     std::optional<clock::time_point> end,
                                                     bool needExtendedHoursData,
                                                     bool needPreviousClose) const;
    MarketHours                         marketHours(MarketType marketType, std::optional<clock::time_point> date = std::nullopt) const;

    // --- async api --- (mostly for interacting with the streamer)
    void                                subscribeLevelOneEquities(const std::vector<std::string>& tickers,
                                                                  const std::vector<StreamerField::LevelOneEquity>& fields);

    // --- getters to cached data, available if connection established (thread-safe)
    std::vector<std::string>            getLinkedAccounts() const;
    UserPreference                      getUserPreference() const;

private:
    // --- OAuth Flow ---
    enum class UpdateStatus : char {
        NotRequired,
        Succeeded,
        FailedExpired,
        FailedBadData,
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

    // --- Accessors for Streamer Class (Thread-Safe) ---
    friend class Streamer;
    std::string                         getAccessToken() const;
    UserPreference::StreamerInfo        getStreamerInfo() const;

    // -- Token Checker Daemon's Job ---
    void                                checkTokensAndReauth();

    // -- Helpers
    void                                updateLinkedAccounts();
    void                                updateUserPreference();

    std::string                         syncRequest(std::string url, HttpRequestQueries queries = {}) const;

private:
    // --- active tokens ---
    std::string                         m_accessToken;
    clock::time_point                   m_accessTokenTS;

    std::string                         m_refreshToken;
    clock::time_point                   m_refreshTokenTS;

    // --- app credentials ---
    std::string                         m_key;
    std::string                         m_secret;

    // --- to protect access to members ---
    mutable std::mutex                  m_mutexTokens;
    mutable std::mutex                  m_mutexLinkedAccounts;
    mutable std::mutex                  m_mutexUserPreference;

    // --- token checker daemon ---
    Timer                               m_tokenCheckerDaemon;

    // --- streamer ---
    std::unique_ptr<Streamer>           m_streamer;

    // --- event callback ---
    EventCallbackFn                     m_eventCallback;

    // --- cached data ---
    UserPreference                      m_userPreference;
    std::unordered_map<std::string, std::string>
                                        m_linkedAccounts;
};

} // namespace schwabcpp

#endif // __CLIENT_H__
