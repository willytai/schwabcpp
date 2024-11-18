#include "refreshTokenResponse.h"
#include "utils/logger.h"

namespace schwabcpp {

RefreshTokenResponse::RefreshTokenResponse(const RefreshTokenResponse& other)
{
    data = other.data;
    isError = other.isError;
}

void RefreshTokenResponse::to_json(json& j, const RefreshTokenResponse& self)
{
    if (self.isError) {
        j = json {
            {"error", self.error.error},
            {"error_description", self.error.description}
        };
    } else {
        j = json {
            {"access_token", self.data.accessToken},
            {"access_token_ts", self.data.accessTokenTS},
            {"expires_in", self.data.expiresIn},
            {"id_token", self.data.idToken},
            {"refresh_token", self.data.refreshToken},
            {"refresh_token_ts", self.data.refreshTokenTS},
            {"scope", self.data.scope},
            {"token_type", self.data.tokenType}
        };
    }
}

void RefreshTokenResponse::from_json(const json& j, RefreshTokenResponse& self)
{
    if (j.contains("error")) {
        self.isError = true;

        j.at("error").get_to(self.error.error);

        // raw error_description is messy, I'll do some cleanup
        std::string rawErrorDescription;
        j.at("error_description").get_to(rawErrorDescription);

        // remove the '\'
        std::erase(rawErrorDescription, '\\');
        // find the first '{', stuffs inside should be a josn data
        auto start = rawErrorDescription.find_first_of('{');
        auto end = rawErrorDescription.find_last_of('}');
        rawErrorDescription = rawErrorDescription.substr(start, end-start+1);
        // parse it and retrieve detailed error_description
        json::parse(rawErrorDescription).at("error_description").get_to(self.error.description);
    } else {
        self.isError = false;

        j.at("expires_in").get_to(self.data.expiresIn);
        j.at("token_type").get_to(self.data.tokenType);
        j.at("scope").get_to(self.data.scope);
        j.at("refreshToken").get_to(self.data.refreshToken);
        j.at("access_token").get_to(self.data.accessToken);
        j.at("id_token").get_to(self.data.idToken);

        auto ts = clock::now().time_since_epoch().count();
        self.data.refreshTokenTS = ts;
        self.data.accessTokenTS = ts;
    }
}

} // namespace schwabcpp
