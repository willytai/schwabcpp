#include "candleList.h"

namespace schwabcpp {

void CandleList::to_json(json &j, const CandleList &self)
{
    j = {
        {"symbol", self.symbol},
        {"empty", self.empty},
        {"candles", self.candles},
    };

    if (self.previousClose.has_value()) {
        j["previousClose"] = self.previousClose.value();
    }
    if (self.previousCloseDate.has_value()) {
        j["previousCloseDate"] = self.previousCloseDate.value();
    }
}

void CandleList::from_json(const json &j, CandleList &data)
{
    j.at("symbol").get_to(data.symbol);
    j.at("empty").get_to(data.empty);
    j.at("candles").get_to(data.candles);
    if (j.contains("previousClose")) {
        data.previousClose = j.at("previousClose");
    } else {
        data.previousClose = std::nullopt;
    }
    if (j.contains("previousCloseDate")) {
        data.previousCloseDate = j.at("previousCloseDate");
    } else {
        data.previousCloseDate = std::nullopt;
    }
}

}
