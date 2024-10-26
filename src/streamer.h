#ifndef __STREAMER_H__
#define __STREAMER_H__

#include <unordered_map>
#include <thread>
#include "websocket.h"

namespace l2viz {

class Client;

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
    };
    static std::string requestCommandType2String(RequestCommandType type);

    using RequestParametersType = std::unordered_map<std::string, std::string>;

public:
                                Streamer(Client* client);
                                ~Streamer();

    void                        start();
    void                        stop();

private:
    std::string                 streamRequest(
                                    RequestServiceType service,
                                    RequestCommandType command,
                                    const RequestParametersType& parameters = {}
                                );

private:
    // -- the job the receiver daemon runs
    void                        receiver();

private:
    Client*                     m_client;  // since the client "owns" the streamer, this is always valid
    std::unique_ptr<Websocket>  m_websocket;                  

    std::unordered_map<StreamerInfoKey, std::string>
                                m_streamerInfo;
    size_t                      m_requestId;

    std::atomic<bool>           m_isActive;
    std::thread                 m_receiverDaemon;
};

}

#endif
