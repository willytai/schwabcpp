#ifndef __CANDLE_LIST_H__
#define __CANDLE_LIST_H__

#include "registrationHelper.h"
#include "schwabcpp/schema/candle.h"
#include "schwabcpp/utils/clock.h"
#include <vector>

namespace schwabcpp {

using json = nlohmann::json;

struct CandleList {

    std::vector<Candle>         candles;
    bool                        empty;
    std::optional<double>       previousClose;
    std::optional<clock::rep>   previousCloseDate;
    std::string                 symbol;

static void to_json(json& j, const CandleList& self);
static void from_json(const json& j, CandleList& data);
};

}

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::CandleList)

#endif

