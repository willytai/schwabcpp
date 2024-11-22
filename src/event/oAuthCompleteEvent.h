#ifndef __OAUTH_COMPLETE_EVENT_H__
#define __OAUTH_COMPLETE_EVENT_H__

#include "eventBase.h"

namespace schwabcpp {

class OAuthCompleteEvent : public Event
{
public:
    DEFINE_EVENT_CLASS(OAuthComplete);

    enum class Status : char {
        Succeeded,
        Failed,
        NotRequired,
    };

                      OAuthCompleteEvent(Status status)
                          : m_status(status)
                      {}

    virtual           ~OAuthCompleteEvent() {}

    inline Status     getStatus() const { return m_status; }

private:
    Status            m_status;
};

}

#endif
