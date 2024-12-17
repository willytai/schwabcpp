
#ifndef __MARKET_HOURS_H__
#define __MARKET_HOURS_H__

#include "registrationHelper.h"

namespace schwabcpp {

using json = nlohmann::json;

struct MarketHours {

    std::string date;   // YYYY-MM-DD
    std::string marketType;
    // std::string exchange;
    // std::string category;
    std::string product;
    std::string productName;
    bool        isOpen;

    struct SessionHours {

        struct Interval {
            std::string start;  // ISO 8601 format
            std::string end;    // ISO 8601 format

        static void to_json(json& j, const Interval& self);
        static void from_json(const json& j, Interval& data);
        };

        std::optional<std::vector<Interval>>    preMarket;
        std::optional<std::vector<Interval>>    regularMarket;
        std::optional<std::vector<Interval>>    postMarket;

    static void to_json(json& j, const SessionHours& self);
    static void from_json(const json& j, SessionHours& data);
    }           sessionHours;

static void to_json(json& j, const MarketHours& self);
static void from_json(const json& j, MarketHours& data);
};

}

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::MarketHours)
REGISTER_TO_JSON(schwabcpp::MarketHours::SessionHours)
REGISTER_TO_JSON(schwabcpp::MarketHours::SessionHours::Interval)

#endif

