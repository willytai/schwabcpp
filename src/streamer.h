#ifndef __STREAMER_H__
#define __STREAMER_H__

#include <unordered_map>
#include "websocket.h"

namespace schwabcpp {

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
        NASDAQ_BOOK,
        OPTIONS_BOOK,
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
    void                        stop();

    // does nothing if not running
    void                        pause();

    // does nothing if already running
    void                        resume();


    void                        asyncRequest(const std::string& request, std::function<void()> callback = {});

private:
    std::string                 constructStreamRequest(
                                    RequestServiceType service,
                                    RequestCommandType command,
                                    const RequestParametersType& parameters = {}
                                );

    // convenience function
    // usage:
    //      std::string requests = batchStreamRequests(
    //          constructStreamRequest(...),
    //          constructStreamRequest(...),
    //          .
    //          .
    //          .
    //      )
    template<typename... Args>
    std::string                 batchStreamRequests(Args... args) { return std::move(batchStreamRequests({args...})); }
    std::string                 batchStreamRequests(const std::vector<std::string>& requests);

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

    // -- request queue and sender control
    class CVState {
        typedef uint8_t Flag;
    public:
        // flag for running the request daemon
        inline static const Flag RunRequestDaemon = 1 << 0;

        // whether request queue is empty
        inline static const Flag RequestQueueNotEmpty = 1 << 1;

        // state
        enum State {
            Inactive = 1,   // before calling start()
            Active   = 2,   // after start() succeed
            Paused   = 3,   // when paused() called after start()
        };

        explicit CVState(State state);

        void setFlag(Flag flag, bool b);
        void setState(State state);
        bool testFlag(Flag flag) const;
        bool testState(State state) const;
        bool shouldWakeSender() const;

    private:
        Flag        _flag;
        uint8_t     _state;
    };
    CVState                     m_state;
    std::thread                 m_requestDaemon;
    mutable std::mutex          m_mutex_state;
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
