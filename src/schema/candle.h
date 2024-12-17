#ifndef __CANDLE_H__
#define __CANDLE_H__

#include "registrationHelper.h"

namespace schwabcpp {

using json = nlohmann::json;

struct Candle {

    double  close;
    int64_t datetime;
    double  high;
    double  low;
    double  open;
    int64_t volume;

static void to_json(json& j, const Candle& self);
static void from_json(const json& j, Candle& data);
};

}

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::Candle)

#endif

