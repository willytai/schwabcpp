#ifndef __WINDOW_CLOSE_EVENT_H__
#define __WINDOW_CLOSE_EVENT_H__

#include "event.h"

namespace l2viz {

class WindowCloseEvent : public Event {
public:
    EVENT_CLASS(WindowCloseEvent)
};

}

#endif
