#ifndef __OAUTH_URL_REQUEST_EVENT_H__
#define __OAUTH_URL_REQUEST_EVENT_H__

#include "eventBase.h"

namespace schwabcpp {

class OAuthUrlRequestEvent : public Event
{
public:
    DEFINE_EVENT_CLASS(OAuthUrlRequest);

    enum class Reason : char {
        InitialSetup,
        RefreshTokenExpired,
        PreviousAuthFailed,
    };

                                  OAuthUrlRequestEvent(const std::string oAuthUrl, Reason reason)
                                      : m_oAuthUrl(oAuthUrl)
                                  {}

    virtual                       ~OAuthUrlRequestEvent() {}

    inline const std::string&     getAuthorizationUrl() const { return m_oAuthUrl; }
    inline Reason                 getReason() const { return m_reason; }
    inline int                    getChances() const { return m_chances; }

private:
    std::string                   m_oAuthUrl;
    Reason                        m_reason;
    int                           m_chances;
};

} // namespace schwabcpp

#endif
