#include "marketHours.h"

namespace schwabcpp {

void MarketHours::to_json(json &j, const MarketHours &self)
{
    j = {
        {"date", self.date},
        {"marketType", self.marketType},
        // {"exchange", self.exchange},
        // {"category", self.category},
        {"product", self.product},
        {"productName", self.productName},
        {"isOpen", self.isOpen},
        {"sessionHours", self.sessionHours},
    };
}

void MarketHours::from_json(const json &j, MarketHours &data)
{
    j.at("date").get_to(data.date);
    j.at("marketType").get_to(data.marketType);
    // j.at("exchange").get_to(data.exchange);
    // j.at("category").get_to(data.category);
    j.at("product").get_to(data.product);
    j.at("productName").get_to(data.productName);
    j.at("isOpen").get_to(data.isOpen);
    j.at("sessionHours").get_to(data.sessionHours);

}

void MarketHours::SessionHours::to_json(json &j, const MarketHours::SessionHours& self)
{
    j = {};

    if (self.preMarket.has_value()) {
        j["preMarket"] = self.preMarket.value();
    }
    if (self.regularMarket.has_value()) {
        j["regularMarket"] = self.regularMarket.value();
    }
    if (self.postMarket.has_value()) {
        j["postMarket"] = self.postMarket.value();
    }
}

void MarketHours::SessionHours::from_json(const json &j, MarketHours::SessionHours& data)
{
    if (j.contains("preMarket")) {
        data.preMarket = j.at("preMarket").get<std::vector<Interval>>();
    } else {
        data.preMarket = std::nullopt;
    }
    if (j.contains("regularMarket")) {
        data.regularMarket = j.at("regularMarket").get<std::vector<Interval>>();
    } else {
        data.regularMarket = std::nullopt;
    }
    if (j.contains("postMarket")) {
        data.postMarket = j.at("postMarket").get<std::vector<Interval>>();
    } else {
        data.postMarket = std::nullopt;
    }
}

void MarketHours::SessionHours::Interval::to_json(json &j, const MarketHours::SessionHours::Interval& self)
{
    j = {
        {"start", self.start},
        {"end", self.end},
    };
}

void MarketHours::SessionHours::Interval::from_json(const json &j, MarketHours::SessionHours::Interval& data)
{
    j.at("start").get_to(data.start);
    j.at("end").get_to(data.end);
}

}
