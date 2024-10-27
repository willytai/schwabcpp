#include "streamer.h"
#include "client.h"
#include "utils/logger.h"
#include "nlohmann/json.hpp"

namespace l2viz {

using json = nlohmann::json;

Streamer::Streamer(Client* client)
    : m_client(client)
    , m_requestId(0)
    , m_isActive(false)
    , m_runRequestDaemon(false)
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
    stop();

    // join here
    if (m_requestDaemon.joinable()) {
        m_requestDaemon.join();
    }
}

void Streamer::start()
{
    LOG_INFO("Starting streamer.");

    // create the websocket
    m_websocket = std::make_unique<Websocket>(m_streamerInfo[StreamerInfoKey::SocketURL]);
    // connect and login
    m_websocket->asyncConnect(std::bind(&Streamer::onWebsocketConnected, this));

    // launch the request daemon
    m_runRequestDaemon = true;
    m_requestDaemon = std::thread(
        [this] { sendRequests(); }
    );

    // TEST: test subscription
    std::string testSubRequest1 = constructStreamRequest(
        RequestServiceType::LEVELONE_EQUITIES,
        RequestCommandType::SUBS,
        {
            { "keys", "SPY" },
            { "fields", "0,1,2,3,33"  },
        }
    );
    std::string testSubRequest2 = constructStreamRequest(
        RequestServiceType::LEVELONE_EQUITIES,
        RequestCommandType::ADD,
        {
            { "keys", "NVDA" },
            { "fields", "0,1,2,3,33"  },
        }
    );
    asyncRequest(testSubRequest1);
    asyncRequest(testSubRequest2);
}

void Streamer::onWebsocketConnected()
{
    LOG_TRACE("Websocket conncted callback triggered.");

    // After websocket connected, we
    // 1. login
    // 2. start the receiving loop

    std::string loginRequest = constructStreamRequest(
        RequestServiceType::ADMIN,
        RequestCommandType::LOGIN,
        {
            { "Authorization", m_client->getAccessToken() },
            { "SchwabClientChannel", m_streamerInfo[StreamerInfoKey::Channel] },
            { "SchwabClientFunctionId", m_streamerInfo[StreamerInfoKey::FunctionId] },
        }
    );

    // queue the login request
    m_websocket->asyncSend(loginRequest, [](){ LOG_INFO("Logging in..."); });
    // queue the login response handler
    m_websocket->asyncReceive(
        [this](const std::string& response) {
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
                                LOG_INFO("Successfully logged in.");

                                // notify the active status
                                {
                                    std::lock_guard<std::mutex> lock(m_mutex_active);
                                    m_isActive = true;
                                    m_cv.notify_all();
                                }

                                // now that we're logged in, start the receiver loop
                                m_websocket->startReceiverLoop(
                                    // TODO: this is where the stream data handler should go
                                    [this](const std::string& data) {
                                        LOG_INFO("Data: {}", data);
                                    }
                                );
                            }
                        }
                    }
                }
            }
        }
    );
}

void Streamer::stop()
{
    // stop the request daemon if it is running
    if (m_requestDaemon.joinable() && m_runRequestDaemon) {
        LOG_TRACE("Stopping streamer request daemon.");
        {
            std::lock_guard<std::mutex> lock(m_mutex_runRequestDaemon);
            m_runRequestDaemon = false;
        }
        m_cv.notify_all();
    }

    // release websocket
    m_websocket.reset();
}

void Streamer::asyncRequest(const std::string& request, std::function<void()> callback)
{
    // enqueue the request
    std::unique_lock<std::mutex> lock(m_mutex_requestQ);
    m_requestQueue.emplace(std::move(request), callback);
    lock.unlock();
    // notify
    m_cv.notify_one();
}

void Streamer::sendRequests()
{
    while (m_runRequestDaemon) {

        // wait until active (logged in)
        {
            std::unique_lock<std::mutex> lock(m_mutex_active);
            m_cv.wait(lock, [this]{ return m_isActive || !m_runRequestDaemon; });

            if (!m_runRequestDaemon) break;
        }

        // wait until request pending
        std::unique_lock<std::mutex> lock(m_mutex_requestQ);
        m_cv.wait(lock, [this]{ return !m_requestQueue.empty() || !m_runRequestDaemon; });

        if (!m_runRequestDaemon) break;

        LOG_TRACE("Streamer request queue size: {}", m_requestQueue.size());

        // send all
        while (!m_requestQueue.empty()) {
            RequestData payload = m_requestQueue.front();
            m_requestQueue.pop();
            lock.unlock();

            // we can do async send here, this essentially pushes the payload to the
            // websocket's internal message queue. It will schedule the send automatically.
            // No need to sync.
            m_websocket->asyncSend(payload.request, payload.callback);

            // lock again before checking the queue
            lock.lock();
        }
    }
}

std::string Streamer::constructStreamRequest(
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
        case RequestServiceType::ADMIN:             return "ADMIN";
        case RequestServiceType::LEVELONE_EQUITIES: return "LEVELONE_EQUITIES";
        case RequestServiceType::NYSE_BOOK:         return "NYSE_BOOK";
    }
}

std::string Streamer::requestCommandType2String(RequestCommandType type)
{
    switch (type) {
        case RequestCommandType::LOGIN:  return "LOGIN";
        case RequestCommandType::LOGOUT: return "LOGOUT";
        case RequestCommandType::SUBS:   return "SUBS";
        case RequestCommandType::ADD:    return "ADD";
    }
}

}
