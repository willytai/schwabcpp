#ifndef __MARKET_TYPE_H__
#define __MARKET_TYPE_H__

#include <string>

namespace schwabcpp {

class MarketType
{
public:
    enum MarketEnum {
        Equity,
        Option,
        Bond,
        Future,
        Forex,
    };

    MarketType(MarketEnum market) : m_market(market) {}
    MarketType(const std::string& str);

    std::string     toString() const;

private:
    MarketEnum      m_market;
};

}

#endif
