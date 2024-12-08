#include "userPreference.h"
#include "utils/logger.h"

namespace schwabcpp {

void UserPreference::Account::to_json(json& j, const UserPreference::Account& self)
{
    NOTIMPLEMENTEDERROR;
}

void UserPreference::Account::from_json(const json& j, UserPreference::Account& data)
{
    j.at("accountNumber").get_to(data.accountNumber);
    j.at("primaryAccount").get_to(data.primaryAccount);
    j.at("type").get_to(data.type);
    j.at("nickName").get_to(data.nickName);
    j.at("accountColor").get_to(data.accountColor);
    j.at("displayAcctId").get_to(data.displayAcctId);
    j.at("autoPositionEffect").get_to(data.autoPositionEffect);
}

void UserPreference::StreamerInfo::to_json(json& j, const UserPreference::StreamerInfo& self)
{
    NOTIMPLEMENTEDERROR;
}

void UserPreference::StreamerInfo::from_json(const json& j, UserPreference::StreamerInfo& data)
{
    j.at("streamerSocketUrl").get_to(data.streamerSocketUrl);
    j.at("schwabClientCustomerId").get_to(data.schwabClientCustomerId);
    j.at("schwabClientCorrelId").get_to(data.schwabClientCorrelId);
    j.at("schwabClientChannel").get_to(data.schwabClientChannel);
    j.at("schwabClientFunctionId").get_to(data.schwabClientFunctionId);
}

void UserPreference::Offer::to_json(json& j, const UserPreference::Offer& self)
{
    NOTIMPLEMENTEDERROR;
}

void UserPreference::Offer::from_json(const json& j, UserPreference::Offer& data)
{
    j.at("level2Permissions").get_to(data.level2Permissions);
    j.at("mktDataPermission").get_to(data.mktDataPermission);
}

void UserPreference::to_json(json& j, const UserPreference& self)
{
    NOTIMPLEMENTEDERROR;
}

void UserPreference::from_json(const json& j, UserPreference& data)
{
    j.at("accounts").get_to(data.accounts);
    j.at("streamerInfo").get_to(data.streamerInfo);
    j.at("offers").get_to(data.offers);
}

}
