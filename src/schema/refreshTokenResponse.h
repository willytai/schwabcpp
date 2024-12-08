#ifndef __REFRESH_TOKEN_RESPONSE_H__
#define __REFRESH_TOKEN_RESPONSE_H__

#include "registrationHelper.h"
#include "schwabcpp/utils/clock.h"

namespace schwabcpp {

using json = nlohmann::json;

// This is an exact copy of AccessTokenResponse, they are essentially the same thing
// I am distinguishing these because I want slightly different behavior in the client oauth flow

struct RefreshTokenResponse {

    union {

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
    };

    bool    isError;

    // For some reason I have to explicitly define these because they are implicitly deleted
    // (has something to do with having unions inside, wtf..)
    RefreshTokenResponse() {}
    RefreshTokenResponse(const RefreshTokenResponse& other);
    ~RefreshTokenResponse() {}
    
static void to_json(json& j, const RefreshTokenResponse& self);
static void from_json(const json& j, RefreshTokenResponse& data);
};

} // namespace schwabcpp

// register with the nlohmann::json library
// this should be outside of our schwabcpp namespace
REGISTER_TO_JSON(schwabcpp::RefreshTokenResponse)

#endif
