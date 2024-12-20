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

Streamer::Streamer(Client* client)
    : m_client(client)
    , m_requestId(0)
    , m_dataHandler(defaultStreamerDataHandler)
    , m_state(CVState::Inactive)
{
    LOG_DEBUG("Initializing streamer...");

    // get the streamer info
    try {
        m_streamerInfo = m_client->getStreamerInfo();
        LOG_DEBUG("Streamer info copied.");
    } catch (...) {
        LOG_ERROR("Failed to retrieve streamer info.");
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

void Streamer::updateStreamerInfo(const UserPreference::StreamerInfo& info)
{
    m_streamerInfo = info;

    LOG_DEBUG("Streamer info updated.");
}

void Streamer::start()
{
    LOG_DEBUG("Starting streamer...");

    // create the websocket
    m_websocket = std::make_unique<Websocket>(m_streamerInfo.streamerSocketUrl);
    // connect and login
    m_websocket->asyncConnect(
        std::bind(&Streamer::onWebsocketConnected, this),
        std::bind(&Streamer::onWebsocketReconnected, this)
    );

    // set the run sender daemon flag
    // don't need mutex here, sender not launched yet
    m_state.setFlag(CVState::RunRequestDaemon, true);

    // launch the request daemon
    m_requestDaemon = std::thread(
        [this] { sendRequests(); }
    );
}

void Streamer::onWebsocketConnected()
{
    // After websocket connected, we start the login and receive procedure
    startLoginAndReceiveProcedure();
}

void Streamer::onWebsocketReconnected()
{
    // resubscribe the subscribed data after calling onWebsocketConnected

    // also update the state (we are not logged in at this point -> inactive)
    {
        std::lock_guard<std::mutex> lock(m_mutex_state);
        m_state.setState(CVState::Inactive);
    }

    onWebsocketConnected();

    LOG_DEBUG("Restoring subscription...");
    for (const auto& request : m_subscriptionRecord) {
        asyncRequest(request);
    }
}

void Streamer::startLoginAndReceiveProcedure()
{
    // 1. login
    // 2. start the receiving loop

    // queue the login request
    m_websocket->asyncSend(
        constructLoginRequest(),
        [] { LOG_DEBUG("Streamer logging in..."); }
    );

    // queue the login response handler
    m_websocket->asyncReceive(
        [this](const std::string& response) {
            LOG_TRACE("Login response: {}", response);

            // bunch of error handling
            try {
                json responseData = json::parse(response);
                if (!responseData.contains("response")) {
                    LOG_ERROR("No response received. (Will retry in 5 seconds...)");
                } else {
                    responseData = responseData["response"];
                    if (!responseData.is_array()) {
                        LOG_ERROR("Received corrupted login response. (Will retry in 5 seconds...)");
                    } else {
                        responseData = responseData.get<std::vector<json>>().at(0);
                        if (!responseData.contains("content")) {
                            LOG_ERROR("No content found in the login response. (Will retry in 5 seconds...)");
                        } else {
                            json contentData = responseData["content"];
                            if (!contentData.contains("code") ||
                                !contentData.contains("msg")) {
                                LOG_ERROR("Login response contenet corrupted. (Will retry in 5 seconds...)");
                            } else {
                                int code = contentData["code"];
                                std::string msg = contentData["msg"];
                                if (code != 0) {
                                    // failed, retry in 10 seconds
                                    LOG_ERROR("Login failed. Error code: {}, Msg: {}. (Will retry in 5 seconds...)", code, msg);
                                } else {
                                    LOG_DEBUG("Successfully logged in.");

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
            } catch (const json::exception& e) {
                LOG_ERROR("Unable to parse login response:  {}.  (Will retry in 5 seconds...)", e.what());
            }

            std::unique_lock lock(m_mutex_state);
            if (!m_state.testState(CVState::Active)) {
                lock.unlock();
                // restart procedure if something failed
                std::this_thread::sleep_for(std::chrono::seconds(5));
                startLoginAndReceiveProcedure();
            }
        }
    );
}

void Streamer::subscribeLevelOneEquities(const std::vector<std::string>& tickers,
                                         const std::vector<StreamerField::LevelOneEquity>& fields)
{
    // NOTE: The streamer requires the symbol field to exist.
    //       It also requires the fields to be sorted in ascending order.
    //       Another thing to note is that the streamer does not support overwriting the existing subscribed fields.
    //       If you try to add a new subscription with different fields of the same service type, they would not go through.
    //       You will only get data of the old subscribed fields.
    //       To add or change new fields to the streamer, you have to do a complete new subscription for all the tickers.
    //       (What a trash API...)
    std::vector<StreamerField::LevelOneEquity>& tmp = const_cast<std::vector<StreamerField::LevelOneEquity>&>(fields);
    std::sort(tmp.begin(), tmp.end(), [](StreamerField::LevelOneEquity left, StreamerField::LevelOneEquity right) {
        return left < right;
    });
    if (tmp.front() != StreamerField::LevelOneEquity::Symbol) {
        tmp.insert(tmp.begin(), StreamerField::LevelOneEquity::Symbol);
    }
    std::string request = constructStreamRequest(
        Streamer::RequestServiceType::LEVELONE_EQUITIES,
        Streamer::RequestCommandType::ADD,
        {
            { "keys", std::accumulate(tickers.begin(), tickers.end(), std::string(), [](std::string acc, const std::string& val) {
                if (!acc.empty()) {
                    acc += ",";
                }
                return acc + val;
            }) },
            { "fields", std::accumulate(fields.begin(), fields.end(), std::string(), [](std::string acc, const StreamerField::LevelOneEquity val) {
                if (!acc.empty()) {
                    acc += ",";
                }
                return acc + std::to_string(static_cast<int>(val));
            }) },
        }
    );

    // record the subscription request incase of reconnection
    m_subscriptionRecord.push_back(request);

    // send
    asyncRequest(request);
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
        LOG_TRACE("Stopping streamer request daemon...");
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
        LOG_DEBUG("Pausing streamer...");

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

std::string Streamer::constructLoginRequest() const
{
    // TODO: make this thread safe
    return constructStreamRequest(
        RequestServiceType::ADMIN,
        RequestCommandType::LOGIN,
        {
            { "Authorization", m_client->getAccessToken() },
            { "SchwabClientChannel", m_streamerInfo.schwabClientChannel },
            { "SchwabClientFunctionId", m_streamerInfo.schwabClientFunctionId },
        }
    );
}

std::string Streamer::constructStreamRequest(
    RequestServiceType service,
    RequestCommandType command,
    const RequestParametersType& parameters) const
{
    json requestJson;

    requestJson["service"] = requestServiceType2String(service);
    requestJson["command"] = requestCommandType2String(command);
    requestJson["requestid"] = m_requestId++;
    requestJson["SchwabClientCustomerId"] = m_streamerInfo.schwabClientCustomerId;
    requestJson["SchwabClientCorrelId"] = m_streamerInfo.schwabClientCorrelId;

    if (!parameters.empty()) {
        requestJson["parameters"] = parameters;
    }

    LOG_TRACE("Streamer request: \n{}", requestJson.dump(4));

    return std::move(requestJson.dump(-1));
}

std::string Streamer::batchStreamRequests(const std::vector<std::string>& requests) const
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

    return "";
}

std::string Streamer::requestCommandType2String(RequestCommandType type)
{
    switch (type) {
        case RequestCommandType::LOGIN:  return "LOGIN";
        case RequestCommandType::LOGOUT: return "LOGOUT";
        case RequestCommandType::SUBS:   return "SUBS";
        case RequestCommandType::ADD:    return "ADD";
    }

    return "";
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
