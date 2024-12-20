#ifndef __ACCESS_TOKEN_RESPONSE_H__
#define __ACCESS_TOKEN_RESPONSE_H__

#include "registrationHelper.h"
#include "schwabcpp/utils/clock.h"

namespace schwabcpp {

using json = nlohmann::json;

struct AccessTokenResponse {

    struct {
        std::string tokenType;
        std::string scope;
        std::string refreshToken;
        std::string accessToken;
        std::string idToken;
        int         expiresIn;

        // some other data that I want to keep
        clock::rep  refreshTokenTS;
        clock::rep  accessTokenTS;
    }   data;

    struct {
        std::string error;
        std::string description;
    }   error;

    bool    isError;

static void to_json(json& j, const AccessTokenResponse& self);
static void from_json(const json& j, AccessTokenResponse& data);
};

} // namespace schwabcpp

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::AccessTokenResponse)

#endif
