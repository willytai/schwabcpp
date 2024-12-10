#ifndef __STREAMER_FIELD_H__
#define __STREAMER_FIELD_H__

#include <string>

namespace schwabcpp {

struct StreamerField {

    enum class LevelOneEquity : int {
        Symbol = 0,
        BidPrice = 1,
        AskPrice = 2,
        LastPrice = 3,
        BidSize = 4,
        AskSize = 5,
        AskID = 6,
        BidID = 7,
        TotalVolume = 8,
        LastSize = 9,
        HighPrice = 10,
        LowPrice = 11,
        ClosePrice = 12,
        ExchangeID = 13,
        Marginable = 14,
        Description = 15,
        LastID = 16,
        OpenPrice = 17,
        NetChange = 18,
        _52WeekHigh = 19,
        _52WeekLow = 20,
        PERatio = 21,
        AnnualDividendAmount = 22,
        DividendYield = 23,
        NAV = 24,
        ExchangeName = 25,
        DividendDate = 26,
        RegularMarketQuote = 27,
        RegularMarketTrade = 28,
        RegularMarketLastPrice = 29,
        RegularMarketLastSize = 30,
        RegularMarketNetChange = 31,
        SecurityStatus = 32,
        MarkPrice = 33,
        QuoteTimeInLong = 34,
        TradeTimeInLong = 35,
        RegularMarketTradeTimeInLong = 36,
        BidTime = 37,
        AskTime = 38,
        AskMICID = 39,
        BidMICID = 40,
        LastMICID = 41,
        NetPercentChange = 42,
        RegularMarketPercentChange = 43,
        MarkPriceNetChange = 44,
        MarkPricePresentChange = 45,
        HardToBorrowQuantity = 46,
        HardToBorrowRate = 47,
        HardToBorrow = 48,
        Shortable = 49,
        PostMarketNetChange = 50,
        PostMarketPresentChange = 51,

        Unknown,
    };

    static LevelOneEquity toLevelOneEquityField(const std::string& key);

};

}

#endif
