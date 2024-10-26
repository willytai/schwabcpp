#include "streamer.h"
#include "client.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"

namespace l2viz {

using json = nlohmann::json;

Streamer::Streamer(Client* client) :
    m_client(client),
    m_requestId(0),
    m_isActive(false)
{
    LOG_INFO("Initializing streamer.");

    // get the user preference
    LOG_DEBUG("Requesting user preference.");
    std::string response;
    if (client->requestUserPreferences(response)) {
        json jsonData = json::parse(response);

        // extract the streamer info
        if (jsonData.contains("streamerInfo") &&
            jsonData["streamerInfo"].is_array()) {
            json streamerInfoData = jsonData["streamerInfo"].get<std::vector<json>>().at(0);

            // populate the feilds
            if (streamerInfoData.contains("streamerSocketUrl")) {
                m_streamerInfo[StreamerInfoKey::SocketURL] = streamerInfoData["streamerSocketUrl"];
            }
            if (streamerInfoData.contains("schwabClientCustomerId")) {
                m_streamerInfo[StreamerInfoKey::CustomerId] = streamerInfoData["schwabClientCustomerId"];
            }
            if (streamerInfoData.contains("schwabClientCorrelId")) {
                m_streamerInfo[StreamerInfoKey::CorrelId] = streamerInfoData["schwabClientCorrelId"];
            }
            if (streamerInfoData.contains("schwabClientChannel")) {
                m_streamerInfo[StreamerInfoKey::Channel] = streamerInfoData["schwabClientChannel"];
            }
            if (streamerInfoData.contains("schwabClientFunctionId")) {
                m_streamerInfo[StreamerInfoKey::FunctionId] = streamerInfoData["schwabClientFunctionId"];
            }

            // let's verify all info set
            static const std::array<StreamerInfoKey, 5> __requiredInfo = {
                StreamerInfoKey::SocketURL,
                StreamerInfoKey::CustomerId,
                StreamerInfoKey::CorrelId,
                StreamerInfoKey::Channel,
                StreamerInfoKey::FunctionId,
            };
            bool streamerInfoIncomplete = false;
            for (const auto& key : __requiredInfo) {
                if (!m_streamerInfo.contains(key)) {
                    streamerInfoIncomplete = true;
                    break;
                }
            }

            if (!streamerInfoIncomplete) {
                LOG_INFO("Streamer info updated.");
            } else {
                LOG_ERROR("Streamer info incomplete.");
            }
        }
    }
}

Streamer::~Streamer()
{
    m_websocket.reset();
}

void Streamer::start()
{
    LOG_INFO("Starting streamer.");

    // instantiating the websocket with the url triggers a "blocking connect" operation
    m_websocket = std::make_unique<Websocket>(m_streamerInfo[StreamerInfoKey::SocketURL]);

    // continue if connection successful
    if (m_websocket->isConnected()) {
        // login
        LOG_INFO("Logging in...");
        std::string loginRequest = streamRequest(
            RequestServiceType::ADMIN,
            RequestCommandType::LOGIN,
            {
                { "Authorization", m_client->getAccessToken() },
                { "SchwabClientChannel", m_streamerInfo[StreamerInfoKey::Channel] },
                { "SchwabClientFunctionId", m_streamerInfo[StreamerInfoKey::FunctionId] },
            }
        );

        // send (blocking)
        m_websocket->send(loginRequest);

        // receive (blocking)
        m_websocket->receive([this](const std::string& response) {
            LOG_TRACE("Login response: {}", response);

            // bunch of error handling
            json responseData = json::parse(response);
            if (!responseData.contains("response")) {
                LOG_ERROR("No response received.");
            } else {
                responseData = responseData["response"];
                if (!responseData.is_array()) {
                    LOG_ERROR("Received corrupted login response.");
                } else {
                    responseData = responseData.get<std::vector<json>>().at(0);
                    if (!responseData.contains("content")) {
                        LOG_ERROR("No content found in the login response.");
                    } else {
                        json contentData = responseData["content"];
                        if (!contentData.contains("code") ||
                            !contentData.contains("msg")) {
                            LOG_ERROR("Login response contenet corrupted.");
                        } else {
                            int code = contentData["code"];
                            std::string msg = contentData["msg"];
                            if (code != 0) {
                                LOG_ERROR("Login failed. Error code: {}, Msg: {}", code, msg);
                            } else {
                                m_isActive = true;
                                LOG_INFO("Successfully logged in.");
                            }
                        }
                    }
                }
            }
        });

        // start the receive loop if login successful
        if (m_isActive) {
            LOG_INFO("Launching stream receiver daemon.");
            m_receiverDaemon = std::thread(&Streamer::receiver, this);
        }


        // TEST: test subscription
        std::string testSubRequest = streamRequest(
            RequestServiceType::LEVELONE_EQUITIES,
            RequestCommandType::SUBS,
            {
                { "keys", "AAPL" },
                { "fields", "0,1,2,3,33"  },
            }
        );
        m_websocket->send(testSubRequest);  // blocking for now
        m_websocket->receive([](const std::string& response) {  // blocking for now
            LOG_TRACE("Test subscription response: {}", response);
        });
    }
}

void Streamer::receiver()
{
    while (m_isActive) {
        m_websocket->receive([](const std::string& response){
            LOG_INFO("Receiver daemon: {}", response);
        });
    }

    LOG_INFO("Receiver daemon stopped.");
}

void Streamer::stop()
{
    m_isActive = false;
    if (m_receiverDaemon.joinable()) {
        // send a logout request
        m_websocket->send(
            streamRequest(
                RequestServiceType::ADMIN,
                RequestCommandType::LOGOUT
            )
        );
        LOG_INFO("Logged out.");

        m_receiverDaemon.join();

        LOG_INFO("Streamer stopped.");
    }

    // testing
    m_websocket->disconnect();
}

std::string Streamer::streamRequest(
    RequestServiceType service,
    RequestCommandType command,
    const RequestParametersType& parameters)
{
    json requestJson;

    requestJson["service"] = requestServiceType2String(service);
    requestJson["command"] = requestCommandType2String(command);
    requestJson["requestid"] = m_requestId++;
    requestJson["SchwabClientCustomerId"] = m_streamerInfo[StreamerInfoKey::CustomerId];
    requestJson["SchwabClientCorrelId"] = m_streamerInfo[StreamerInfoKey::CorrelId];

    if (!parameters.empty()) {
        requestJson["parameters"] = parameters;
    }

    LOG_TRACE("Streamer request: \n{}", requestJson.dump(4));

    return std::move(requestJson.dump(-1));
}

std::string Streamer::requestServiceType2String(RequestServiceType type)
{
    switch (type) {
        case RequestServiceType::ADMIN: return "ADMIN";
        case RequestServiceType::LEVELONE_EQUITIES: return "LEVELONE_EQUITIES";
        case RequestServiceType::NYSE_BOOK: return "NYSE_BOOK";
    }
}

std::string Streamer::requestCommandType2String(RequestCommandType type)
{
    switch (type) {
        case RequestCommandType::LOGIN: return "LOGIN";
        case RequestCommandType::LOGOUT: return "LOGOUT";
        case RequestCommandType::SUBS: return "SUBS";
    }
}

}
