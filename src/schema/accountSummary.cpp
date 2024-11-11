#include "accountSummary.h"
#include "utils/logger.h"

namespace schwabcpp {

void AccountSummary::to_json(json& j, const AccountSummary& self)
{
    NOTIMPLEMENTEDERROR;
}

void AccountSummary::from_json(const json& j, AccountSummary& data)
{
    j.at("aggregatedBalance").at("currentLiquidationValue").get_to(data.aggregatedBalance.currentLiquidationValue);
    j.at("aggregatedBalance").at("liquidationValue").get_to(data.aggregatedBalance.liquidationValue);

    j.at("securitiesAccount").at("accountNumber").get_to(data.securitiesAccount.accountNumber);

    j.at("securitiesAccount").at("currentBalances").at("accruedInterest").get_to(data.securitiesAccount.currentBalances.accruedInterest);
    j.at("securitiesAccount").at("currentBalances").at("bondValue").get_to(data.securitiesAccount.currentBalances.bondValue);
    j.at("securitiesAccount").at("currentBalances").at("cashAvailableForTrading").get_to(data.securitiesAccount.currentBalances.cashAvailableForTrading);
    j.at("securitiesAccount").at("currentBalances").at("cashAvailableForWithdrawal").get_to(data.securitiesAccount.currentBalances.cashAvailableForWithdrawal);
    j.at("securitiesAccount").at("currentBalances").at("cashBalance").get_to(data.securitiesAccount.currentBalances.cashBalance);
    j.at("securitiesAccount").at("currentBalances").at("cashCall").get_to(data.securitiesAccount.currentBalances.cashCall);
    j.at("securitiesAccount").at("currentBalances").at("cashDebitCallValue").get_to(data.securitiesAccount.currentBalances.cashDebitCallValue);
    j.at("securitiesAccount").at("currentBalances").at("cashReceipts").get_to(data.securitiesAccount.currentBalances.cashReceipts);
    j.at("securitiesAccount").at("currentBalances").at("liquidationValue").get_to(data.securitiesAccount.currentBalances.liquidationValue);
    j.at("securitiesAccount").at("currentBalances").at("longMarketValue").get_to(data.securitiesAccount.currentBalances.longMarketValue);
    j.at("securitiesAccount").at("currentBalances").at("longNonMarginableMarketValue").get_to(data.securitiesAccount.currentBalances.longNonMarginableMarketValue);
    j.at("securitiesAccount").at("currentBalances").at("longOptionMarketValue").get_to(data.securitiesAccount.currentBalances.longOptionMarketValue);
    j.at("securitiesAccount").at("currentBalances").at("moneyMarketFund").get_to(data.securitiesAccount.currentBalances.moneyMarketFund);
    j.at("securitiesAccount").at("currentBalances").at("mutualFundValue").get_to(data.securitiesAccount.currentBalances.mutualFundValue);
    j.at("securitiesAccount").at("currentBalances").at("pendingDeposits").get_to(data.securitiesAccount.currentBalances.pendingDeposits);
    j.at("securitiesAccount").at("currentBalances").at("savings").get_to(data.securitiesAccount.currentBalances.savings);
    j.at("securitiesAccount").at("currentBalances").at("shortMarketValue").get_to(data.securitiesAccount.currentBalances.shortMarketValue);
    j.at("securitiesAccount").at("currentBalances").at("shortOptionMarketValue").get_to(data.securitiesAccount.currentBalances.shortOptionMarketValue);
    j.at("securitiesAccount").at("currentBalances").at("totalCash").get_to(data.securitiesAccount.currentBalances.totalCash);
    j.at("securitiesAccount").at("currentBalances").at("unsettledCash").get_to(data.securitiesAccount.currentBalances.unsettledCash);

    j.at("securitiesAccount").at("initialBalances").at("accountValue").get_to(data.securitiesAccount.initialBalances.accountValue);
    j.at("securitiesAccount").at("initialBalances").at("accruedInterest").get_to(data.securitiesAccount.initialBalances.accruedInterest);
    j.at("securitiesAccount").at("initialBalances").at("bondValue").get_to(data.securitiesAccount.initialBalances.bondValue);
    j.at("securitiesAccount").at("initialBalances").at("cashAvailableForTrading").get_to(data.securitiesAccount.initialBalances.cashAvailableForTrading);
    j.at("securitiesAccount").at("initialBalances").at("cashAvailableForWithdrawal").get_to(data.securitiesAccount.initialBalances.cashAvailableForWithdrawal);
    j.at("securitiesAccount").at("initialBalances").at("cashBalance").get_to(data.securitiesAccount.initialBalances.cashBalance);
    j.at("securitiesAccount").at("initialBalances").at("cashDebitCallValue").get_to(data.securitiesAccount.initialBalances.cashDebitCallValue);
    j.at("securitiesAccount").at("initialBalances").at("cashReceipts").get_to(data.securitiesAccount.initialBalances.cashReceipts);
    j.at("securitiesAccount").at("initialBalances").at("isInCall").get_to(data.securitiesAccount.initialBalances.isInCall);
    j.at("securitiesAccount").at("initialBalances").at("liquidationValue").get_to(data.securitiesAccount.initialBalances.liquidationValue);
    j.at("securitiesAccount").at("initialBalances").at("longOptionMarketValue").get_to(data.securitiesAccount.initialBalances.longOptionMarketValue);
    j.at("securitiesAccount").at("initialBalances").at("longStockValue").get_to(data.securitiesAccount.initialBalances.longStockValue);
    j.at("securitiesAccount").at("initialBalances").at("moneyMarketFund").get_to(data.securitiesAccount.initialBalances.moneyMarketFund);
    j.at("securitiesAccount").at("initialBalances").at("mutualFundValue").get_to(data.securitiesAccount.initialBalances.mutualFundValue);
    j.at("securitiesAccount").at("initialBalances").at("pendingDeposits").get_to(data.securitiesAccount.initialBalances.pendingDeposits);
    j.at("securitiesAccount").at("initialBalances").at("shortOptionMarketValue").get_to(data.securitiesAccount.initialBalances.shortOptionMarketValue);
    j.at("securitiesAccount").at("initialBalances").at("shortStockValue").get_to(data.securitiesAccount.initialBalances.shortStockValue);
    j.at("securitiesAccount").at("initialBalances").at("unsettledCash").get_to(data.securitiesAccount.initialBalances.unsettledCash);

    j.at("securitiesAccount").at("isClosingOnlyRestricted").get_to(data.securitiesAccount.isClosingOnlyRestricted);
    j.at("securitiesAccount").at("isDayTrader").get_to(data.securitiesAccount.isDayTrader);
    j.at("securitiesAccount").at("pfcbFlag").get_to(data.securitiesAccount.pfcbFlag);

    j.at("securitiesAccount").at("projectedBalances").at("cashAvailableForTrading").get_to(data.securitiesAccount.projectedBalances.cashAvailableForTrading);
    j.at("securitiesAccount").at("projectedBalances").at("cashAvailableForWithdrawal").get_to(data.securitiesAccount.projectedBalances.cashAvailableForWithdrawal);

    j.at("securitiesAccount").at("roundTrips").get_to(data.securitiesAccount.roundTrips);
    j.at("securitiesAccount").at("type").get_to(data.securitiesAccount.type);
}

void AccountsSummaryMap::to_json(json& j, const AccountsSummaryMap& self)
{
    NOTIMPLEMENTEDERROR;
}

void AccountsSummaryMap::from_json(const json& j, AccountsSummaryMap& data)
{
    data.summary.clear();

    // this is a list data
    std::vector<json> list;
    j.get_to(list);

    // loop through the accounts
    for (const json& d : list) {
        AccountSummary summary;
        d.get_to(summary);

        // move to map
        data.summary.emplace(summary.securitiesAccount.accountNumber, summary);
    }
}

}
