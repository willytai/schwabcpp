#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <chrono>

namespace schwabcpp {

// global clock def for the client libary
using clock = std::chrono::system_clock;

} // namespace schwabcpp

#endif
