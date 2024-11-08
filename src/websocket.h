#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include "websocketSession.h"
#include <thread>

namespace schwabcpp {

namespace beast = boost::beast;

class Websocket
{
    using ConnectionHandle = beast::websocket::stream<boost::asio::ssl::stream<beast::tcp_stream>>;

public:
                                            Websocket(const std::string& url);
                                            ~Websocket();

    // This is the entry point. The constructor doesn't connect but configures the websocket.
    // This should be called explicitly to establish the connection.
    void                                    asyncConnect(std::function<void()> onConnected = {});
    void                                    asyncSend(const std::string& request, std::function<void()> callback = {});
    void                                    asyncReceive(std::function<void(const std::string&)> callback);

    void                                    startReceiverLoop(std::function<void(const std::string&)> callback);
    void                                    stopReceiverLoop();

    bool                                    isConnected() const { return m_session ? m_session->isConnected() : false; }

private:
    std::string                             m_host;

    boost::asio::io_context                 m_ioContext;
    boost::asio::ssl::context               m_sslContext;
    std::shared_ptr<WebsocketSession>       m_session;

    net::executor_work_guard
        <net::io_context::executor_type>    m_workGuard;
    std::thread                             m_ioContextThread;
};

}

#endif
