#ifndef __STREAMER_H__
#define __STREAMER_H__

#include <unordered_map>
#include "websocket.h"

namespace l2viz {

class Client;

//
// * To use the streamer class, the client has to call `start()` to initiate the connection.
//   This launches the starting procedure and returns immediately.
//
// * You can call `stop()` to terminate the streamer. If stop() is not called before the streamer
//   object is destroyed, the destructor will call it automatically. It will cleanup after itself.
//   You don't have to call it if you don't care when the streamer is destroyed.
//
// * After `start` is called, you can use `asyncRequest(...)` ANYTIME to send your request.
//   Note that this simply queues the requests and return immediately. The sender will send the request
//   when appropriate (after connection established and successfully logged in).
//   The callback will be triggered when the request is actually sent.
//
// * TODO:
//   Currently, the streamer streams the data to my global spdlog logger with the LOG_INFO level.
//   Should change it so that it streams to the stream the client requested.
//
// * TODO:
//   Create APIs to generate request for the supported subscriptions.
//
class Streamer
{
    enum class StreamerInfoKey {
        SocketURL = 0,
        CustomerId = 1,
        CorrelId = 2,
        Channel = 3,
        FunctionId = 4,
    };

    enum class RequestServiceType : char {
        ADMIN,
        LEVELONE_EQUITIES,
        NYSE_BOOK,
    };
    static std::string requestServiceType2String(RequestServiceType type);

    enum class RequestCommandType : char {
        LOGIN,
        LOGOUT,
        SUBS,
        ADD,
    };
    static std::string requestCommandType2String(RequestCommandType type);

    using RequestParametersType = std::unordered_map<std::string, std::string>;

public:
                                Streamer(Client* client);
                                ~Streamer();


    void                        start();
    void                        asyncRequest(const std::string& request, std::function<void()> callback = {});
    void                        stop();

private:
    std::string                 constructStreamRequest(
                                    RequestServiceType service,
                                    RequestCommandType command,
                                    const RequestParametersType& parameters = {}
                                );

private:
    void                        onWebsocketConnected();

private:
    // -- what the sender daemon runs
    void                        sendRequests();

private:
    Client*                     m_client;  // since the client "owns" the streamer, this is always valid
    std::unique_ptr<Websocket>  m_websocket;                  

    std::unordered_map<StreamerInfoKey, std::string>
                                m_streamerInfo;
    size_t                      m_requestId;

    // -- request queue control
    bool                        m_isActive;
    bool                        m_runRequestDaemon;
    std::thread                 m_requestDaemon;
    mutable std::mutex          m_mutex_active;
    mutable std::mutex          m_mutex_runRequestDaemon;
    mutable std::mutex          m_mutex_requestQ;
    std::condition_variable     m_cv;  // for notifying the request daemon
    struct RequestData {
        std::string request;
        std::function<void()> callback;
    };
    std::queue<RequestData>     m_requestQueue;
};

}

#endif
