#ifndef __USER_PREFERENCE_H__
#define __USER_PREFERENCE_H__

#include "registrationHelper.h"

namespace schwabcpp {

using json = nlohmann::json;

struct UserPreference {

    struct Account {

        std::string accountNumber;
        bool        primaryAccount = false;
        std::string type;
        std::string nickName;
        std::string accountColor;
        std::string displayAcctId;
        bool        autoPositionEffect = false;

    static void to_json(json& j, const Account& self);
    static void from_json(const json& j, Account& data);
    };

    struct StreamerInfo {

        std::string streamerSocketUrl;
        std::string schwabClientCustomerId;
        std::string schwabClientCorrelId;
        std::string schwabClientChannel;
        std::string schwabClientFunctionId;

    static void to_json(json& j, const StreamerInfo& self);
    static void from_json(const json& j, StreamerInfo& data);
    };

    struct Offer {

        bool        level2Permissions = false;
        std::string mktDataPermission;

    static void to_json(json& j, const Offer& self);
    static void from_json(const json& j, Offer& data);
    };

    std::vector<Account>        accounts;
    std::vector<StreamerInfo>   streamerInfo;
    std::vector<Offer>          offers;

static void to_json(json& j, const UserPreference& self);
static void from_json(const json& j, UserPreference& data);
};

} // namespace schwabcpp

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::UserPreference::Account)
REGISTER_TO_JSON(schwabcpp::UserPreference::StreamerInfo)
REGISTER_TO_JSON(schwabcpp::UserPreference::Offer)
REGISTER_TO_JSON(schwabcpp::UserPreference)

#endif
