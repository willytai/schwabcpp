#include "marketType.h"

namespace schwabcpp {

MarketType::MarketType(const std::string& str)
{
    if (str == "equity") {
        m_market = Equity;
    } else if (str == "option") {
        m_market = Option;
    } else if (str == "bond") {
        m_market = Bond;
    } else if (str == "future") {
        m_market = Future;
    } else if (str == "forex") {
        m_market = Forex;
    } else {
        m_market = Equity;  // default
    }
}

std::string
MarketType::toString() const
{
    switch (m_market) {
        case Equity: return "equity";
        case Option: return "option";
        case Bond: return "bond";
        case Future: return "future";
        case Forex: return "forex";
    }

    return "BAD TYPE";
}

}
