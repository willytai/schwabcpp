#ifndef __EVENT_H__
#define __EVENT_H__

#include <string>
#include <functional>
#include "../utils/logger.h"

namespace l2viz {

enum class EventType
{
    None = 0,
    WindowCloseEvent, WindowResize,
    KeyPressed, KeyReleased,
    MouseButtonPressed, MouseButtonReleased, MouseScrolled, MouseMoved,
};

class Event
{
    friend class EventDispatcher;
public:
    Event() = default;
    virtual ~Event() {}

    virtual EventType type() const = 0;
    virtual const char* name() const = 0;
    virtual std::string toString() const { return name(); }

    inline bool handled() const { return m_handled; }

    template <typename T>
    friend T& operator<<(T& os, const Event& event) {
        return os << event.toString();
    }

private:
    bool m_handled = false;
};

#define EVENT_CLASS(EventClass) \
    virtual ~EventClass() { LOG_TRACE("{} deleted.", name()); } \
    static EventType StaticType() { return EventType::EventClass; } \
    EventType type() const override { return StaticType(); } \
    const char* name() const override { return #EventClass; }

class EventDispatcher
{
    template <typename T>
    using eventFn = std::function<bool(T*)>;
public:
    EventDispatcher(Event* event) : m_event(event) {}

    template <typename T>
    bool dispatch(eventFn<T> fn) {
        if (m_event->type() == T::StaticType()) {
            m_event->m_handled = fn(static_cast<T*>(m_event));
            return true;
        }
        return false;
    }

private:
    Event* m_event;
};

}

#endif
