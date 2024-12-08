#include "streamer.h"
#include "client.h"
#include "nlohmann/json.hpp"
#include "utils/logger.h"

namespace schwabcpp {

using json = nlohmann::json;

auto defaultStreamerDataHandler = [](const std::string& data) {
    try {
        LOG_INFO("Data: \n{}", data.empty() ? "" : json::parse(data).dump(4));
    } catch (...) {
        LOG_WARN("Data: corrupted.");
    }
};

enum class Streamer::StreamerInfoKey {
    SocketURL = 0,
    CustomerId = 1,
    CorrelId = 2,
    Channel = 3,
    FunctionId = 4,
};

Streamer::Streamer(Client* client)
    : m_client(client)
    , m_requestId(0)
    , m_dataHandler(defaultStreamerDataHandler)
    , m_state(CVState::Inactive)
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
    LOG_INFO("Starting streamer...");

    // create the websocket
    m_websocket = std::make_unique<Websocket>(m_streamerInfo[StreamerInfoKey::SocketURL]);
    // connect and login
    m_websocket->asyncConnect(std::bind(&Streamer::onWebsocketConnected, this));

    // set the run sender daemon flag
    // don't need mutex here, sender not launched yet
    m_state.setFlag(CVState::RunRequestDaemon, true);

    // launch the request daemon
    m_requestDaemon = std::thread(
        [this] { sendRequests(); }
    );

    // TEST: test subscription
    std::string testSubRequest1 = constructStreamRequest(
        // RequestServiceType::LEVELONE_EQUITIES,
        // RequestCommandType::SUBS,
        // {
        //     { "keys", "SPY" },
        //     { "fields", "0,1,2,3,33"  },
        // }
            RequestServiceType::OPTIONS_BOOK,
            RequestCommandType::SUBS,
            {
                { "keys", "NVDA  241220C00140000" },
                { "fields", "0,1,2,3" },
            }
    );
    std::string testSubRequest2 = batchStreamRequests(
        constructStreamRequest(
            RequestServiceType::LEVELONE_EQUITIES,
            RequestCommandType::SUBS,
            {
                { "keys", "SPY" },
                { "fields", "0,1,2,3,33"  },
            }
        )
        // constructStreamRequest(
        //     RequestServiceType::NASDAQ_BOOK,
        //     RequestCommandType::ADD,
        //     {
        //         { "keys", "NVDA" },
        //         { "fields", "0,1,2,3" },
        //     }
        // ),
        // constructStreamRequest(
        //     RequestServiceType::NYSE_BOOK,
        //     RequestCommandType::ADD,
        //     {
        //         { "keys", "PLTR" },
        //         { "fields", "0,1,2,3" },
        //     }
        // )
    );
    // asyncRequest(testSubRequest1);
    // asyncRequest(testSubRequest2);
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

                                // notify the status
                                {
                                    std::lock_guard<std::mutex> lock(m_mutex_state);
                                    m_state.setState(CVState::Active);
                                    m_cv.notify_all();
                                }

                                // now that we're logged in, start the receiver loop
                                m_websocket->startReceiverLoop(m_dataHandler);
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
    LOG_TRACE("Stopping streamer...");

    // update the flags
    {
        std::lock_guard lock(m_mutex_state);
        m_state.setState(CVState::Inactive);
        m_state.setFlag(CVState::RunRequestDaemon, false);
    }

    // stop the request daemon if it is running
    if (m_requestDaemon.joinable()) {
        LOG_TRACE("Stopping streamer request daemon.");
        m_cv.notify_all();
    }

    // release websocket
    m_websocket.reset();
}

void Streamer::pause()
{
    std::unique_lock lock(m_mutex_state);

    // we can only pause if we are active
    if (m_state.testState(CVState::Active)) {
        LOG_DEBUG("Pausing streamer.");

        // for pausing the request sender
        // just change the state
        // no need to notify since it is already running
        m_state.setState(CVState::Paused);
        lock.unlock();

        // stop the websocket receiver loop
        m_websocket->stopReceiverLoop();
    } else {
        LOG_DEBUG("Streamer not streaming, cannot pause.");
    }
}

void Streamer::resume()
{
    std::unique_lock lock(m_mutex_state);

    // we can only resume if we are paused
    if (m_state.testState(CVState::Paused)) {
        LOG_DEBUG("Resuming streamer.");
        lock.unlock();

        // start the receiver loop
        m_websocket->startReceiverLoop(m_dataHandler);

        // change state and notify sender
        lock.lock();
        m_state.setState(CVState::Active);
        lock.unlock();

        m_cv.notify_all();
    } else {
        LOG_DEBUG("Streamer not paused, cannot resume.");
    }
}

bool Streamer::isActive() const
{
    std::lock_guard lock(m_mutex_state);
    return m_state.testState(CVState::Active);
}

bool Streamer::isPaused() const
{
    std::lock_guard lock(m_mutex_state);
    return m_state.testState(CVState::Paused);
}

void Streamer::asyncRequest(const std::string& request, std::function<void()> callback)
{
    // enqueue the request
    {
        std::lock_guard<std::mutex> lock(m_mutex_requestQ);
        m_requestQueue.emplace(std::move(request), callback);
    }
    // set the flag
    {
        std::lock_guard lock(m_mutex_state);
        m_state.setFlag(CVState::RequestQueueNotEmpty, true);
    }
    // notify
    m_cv.notify_one();
}

void Streamer::sendRequests()
{
    std::unique_lock<std::mutex> stateLock(m_mutex_state);
    while (m_state.testFlag(CVState::RunRequestDaemon)) {

        // now we will wati until the state flag is notified and post check should wake sender
        m_cv.wait(stateLock, [this] { return m_state.shouldWakeSender(); });

        // if we woke up because the run flag is unset, break
        if (!m_state.testFlag(CVState::RunRequestDaemon)) break;

        LOG_TRACE("Streamer request queue size: {}", m_requestQueue.size());

        // send all if not interrupted
        std::unique_lock<std::mutex> queueLock(m_mutex_requestQ);
        while (!m_requestQueue.empty() && m_state.testState(CVState::Active)) {
            // done checking state, release
            stateLock.unlock();

            RequestData payload = m_requestQueue.front();
            m_requestQueue.pop();

            // dequeue complete, release
            queueLock.unlock();

            // we can do async send here, this essentially pushes the payload to the
            // websocket's internal message queue. It will schedule the send automatically.
            // No need to sync.
            m_websocket->asyncSend(payload.request, payload.callback);

            // lock again before checking the queue and state
            queueLock.lock();
            stateLock.lock();
        }

        // queue and state are locked at this point
        if (m_requestQueue.empty()) {
            m_state.setFlag(CVState::RequestQueueNotEmpty, false);
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

std::string Streamer::batchStreamRequests(const std::vector<std::string>& requests)
{
    // This batches the reuqests into a list with the key "requests"
    json requestJson;
    std::vector<json> tmp(requests.size());
    std::transform(requests.cbegin(), requests.cend(), tmp.begin(), [](const std::string& req){ return json::parse(req); });
    requestJson["requests"] = tmp;

    return std::move(requestJson.dump(-1));
}

std::string Streamer::requestServiceType2String(RequestServiceType type)
{
    switch (type) {
        case RequestServiceType::ADMIN:             return "ADMIN";
        case RequestServiceType::LEVELONE_EQUITIES: return "LEVELONE_EQUITIES";
        case RequestServiceType::NYSE_BOOK:         return "NYSE_BOOK";
        case RequestServiceType::NASDAQ_BOOK:       return "NASDAQ_BOOK";
        case RequestServiceType::OPTIONS_BOOK:      return "OPTIONS_BOOK";
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

// -- CVState
Streamer::CVState::CVState(State state)
    : _flag(0x0)
    , _state(State::Inactive)
{}

void Streamer::CVState::setFlag(Flag flag, bool b)
{
    if (b) {
        _flag |= flag;
    } else {
        _flag &= ~flag;
    }
}

void Streamer::CVState::setState(State state)
{
    _state = state;
}

bool Streamer::CVState::testFlag(Flag flag) const
{
    return _flag & flag;
}

bool Streamer::CVState::testState(State state) const
{
    return _state == state;
}

bool Streamer::CVState::shouldWakeSender() const
{
    // when should we wake the sender?
    // 1.active and messeage queue not empty
    // 2. when the run sender flag is unset 
    return testState(CVState::Active) && testFlag(CVState::RequestQueueNotEmpty)
        || !testFlag(CVState::RunRequestDaemon);
}

}
