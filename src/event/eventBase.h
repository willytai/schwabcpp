#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include <string>

namespace schwabcpp {

enum class EventType
{
    OAuthUrlRequest,
    OAuthComplete,
};

class Event
{
    friend class Client;
public:
    Event() = default;

    virtual EventType type() const = 0;
    virtual const char* name() const = 0;
    virtual std::string toString() const { return this->name(); }

    inline bool getHandled() const { return m_handled; }
    inline void setHandled(bool b) { m_handled = b; }

    // some events accepts reply
    inline void reply(const std::string& s) { m_reply = s; }

    template <typename T>
    friend T& operator<<(T& os, const Event& event) {
        return os << event.toString();
    }

private:
    std::string m_reply = "";
    const std::string& getReply() const { return m_reply; }

private:
    bool m_handled = false;
};

#define DEFINE_EVENT_CLASS(EventName) \
    static EventType StaticType() { return EventType::EventName; } \
    EventType type() const override { return StaticType(); } \
    const char* name() const override { return #EventName; }

} // namespace schwabcpp

#endif
