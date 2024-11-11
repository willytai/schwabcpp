#ifndef __ACCOUNT_SUMMARY_H__
#define __ACCOUNT_SUMMARY_H__

#include "registrationHelper.h"
#include <unordered_map>

namespace schwabcpp {

using json = nlohmann::json;

struct AccountSummary {

    struct {
        float   currentLiquidationValue;
        float   liquidationValue;
    } aggregatedBalance;

    struct {
        std::string     accountNumber;

        struct {
            float   accruedInterest;
            float   bondValue;
            float   cashAvailableForTrading;
            float   cashAvailableForWithdrawal;
            float   cashBalance;
            float   cashCall;
            float   cashDebitCallValue;
            float   cashReceipts;
            float   liquidationValue;
            float   longMarketValue;
            float   longNonMarginableMarketValue;
            float   longOptionMarketValue;
            float   moneyMarketFund;
            float   mutualFundValue;
            float   pendingDeposits;
            float   savings;
            float   shortMarketValue;
            float   shortOptionMarketValue;
            float   totalCash;
            float   unsettledCash;
        }               currentBalances;

        struct {
            float   accountValue;
            float   accruedInterest;
            float   bondValue;
            float   cashAvailableForTrading;
            float   cashAvailableForWithdrawal;
            float   cashBalance;
            float   cashDebitCallValue;
            float   cashReceipts;
            float   isInCall;
            float   liquidationValue;
            float   longOptionMarketValue;
            float   longStockValue;
            float   moneyMarketFund;
            float   mutualFundValue;
            float   pendingDeposits;
            float   shortOptionMarketValue;
            float   shortStockValue;
            float   unsettledCash;
        }               initialBalances;

        bool            isClosingOnlyRestricted;
        bool            isDayTrader;
        bool            pfcbFlag;

        struct {
            float   cashAvailableForTrading;
            float   cashAvailableForWithdrawal;
        }               projectedBalances;

        int             roundTrips;
        std::string     type;
 
    } securitiesAccount;


static void to_json(json& j, const AccountSummary& self);
static void from_json(const json& j, AccountSummary& data);
};

struct AccountsSummaryMap {

    std::unordered_map<std::string, AccountSummary>     summary;


static void to_json(json& j, const AccountsSummaryMap& self);
static void from_json(const json& j, AccountsSummaryMap& data);
};

}

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::AccountSummary)
REGISTER_TO_JSON(schwabcpp::AccountsSummaryMap)

#endif
