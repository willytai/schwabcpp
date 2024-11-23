#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include <functional>
#include <string>

namespace schwabcpp {

enum class EventType
{
    OAuthUrlRequest,
    OAuthComplete,
};

class Event
{
public:
    Event() = default;

    virtual EventType type() const = 0;
    virtual const char* name() const = 0;
    virtual std::string toString() const { return this->name(); }

    inline bool getHandled() const { return m_handled; }

    // some events accepts reply
    inline void reply(const std::string& s) { m_reply = s; }

    template <typename T>
    friend T& operator<<(T& os, const Event& event) {
        return os << event.toString();
    }

private:
    friend class Client;
    std::string m_reply = "";
    const std::string& getReply() const { return m_reply; }

private:
    friend class EventDispatcher;
    bool m_handled = false;
};

#define DEFINE_EVENT_CLASS(EventName) \
    static EventType StaticType() { return EventType::EventName; } \
    EventType type() const override { return StaticType(); } \
    const char* name() const override { return #EventName; }


class EventDispatcher
{
    template <typename T>
    using eventFn = std::function<bool(T&)>;
public:
    EventDispatcher(Event& event) : m_event(event) {}

    template <typename T>
    bool dispatch(eventFn<T> fn) {
        if (m_event.type() == T::StaticType()) {
            m_event.m_handled = fn(*(T*)&m_event);
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

} // namespace schwabcpp

#endif
