#ifndef __WEBSOCKET_SESSION_H__
#define __WEBSOCKET_SESSION_H__

#include <memory>
#include <queue>
#include <thread>

// NOTE: boost is very heavy, maintain minimal include headers
// Required types are:
//      boost::asio::io_context
//      boost::asio::ssl::context
//      boost::asio::ip::tcp::resolver
//      boost::asio::ssl::stream
//      boost::beast::core::tcp_stream
//      boost::beast::websocket::stream
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace l2viz {

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
{
public:
    explicit                                            WebsocketSession(
                                                            net::io_context& ioContext,
                                                            ssl::context& sslContext,
                                                            const std::string& host,
                                                            const std::string& port,
                                                            const std::string& path
                                                        );
                                                        ~WebsocketSession();

    // Entry point, call this explicitly to start the connection process.
    // I can't make it auto connect during construnction due to the "enable_shared_from_this"
    // desgin.
    void                                                asyncConnect(std::function<void()> onFinalHandshake = {});
    void                                                asyncDisconnect();

    void                                                asyncSend(const std::string& request, std::function<void()> callback = {});
    void                                                asyncReceive(std::function<void(const std::string&)> callback);

    void                                                startReceiverLoop(std::function<void(const std::string&)> callback);

    bool                                                isConnected() const;

private:
    void                                                onResolve(
                                                            std::function<void()> onFinalHandshake,
                                                            beast::error_code ec,
                                                            tcp::resolver::results_type results
                                                        );
    void                                                onConnect(
                                                            std::function<void()> onFinalHandshake,
                                                            beast::error_code ec,
                                                            tcp::resolver::results_type::endpoint_type endpoint 
                                                        );
    void                                                onSSLHandshake(
                                                            std::function<void()> onFinalHandshake,
                                                            beast::error_code ec
                                                        );
    void                                                onWebsocketHandshake(
                                                            std::function<void()> onFinalHandshake,
                                                            beast::error_code ec
                                                        );
    void                                                onClose(
                                                            beast::error_code ec
                                                        );
    void                                                onRead(
                                                            std::function<void(const std::string&)> callback,
                                                            beast::error_code ec,
                                                            std::size_t bytesTransferred
                                                        );
    void                                                onWrite(
                                                            std::function<void()> callback,
                                                            beast::error_code ec,
                                                            std::size_t bytesTransferred
                                                        );
    void                                                onReceiveLoop(
                                                            std::function<void(const std::string&)> callback,
                                                            beast::error_code ec,
                                                            std::size_t bytesTransferred
                                                        );

    // what the sender daemon runs
private:
    void                                                sendMessages();

private:
    // -- data
    std::string                                         m_host;
    std::string                                         m_port;
    std::string                                         m_path;
    beast::flat_buffer                                  m_buffer;

    tcp::resolver                                       m_resolver;
    websocket::stream<ssl::stream<beast::tcp_stream>>   m_websocketStream;

    // -- message queue
    struct MessageData {
        std::string request;
        std::function<void()> callback;
    };
    std::queue<MessageData>                             m_messageQueue;

    // -- message queue control
    bool                                                m_isConnected;
    std::atomic<bool>                                   m_runSenderDaemon;
    std::thread                                         m_senderDaemon;
    mutable std::mutex                                  m_mutex_runSenderDaemon;  // mutex for connection flag
    mutable std::mutex                                  m_mutex_connected;        // mutex for connection flag
    mutable std::mutex                                  m_mutex_messageQ;         // mutex for message queue
    std::condition_variable                             m_cv;                     // for notifying the sender daemon
};

}

#endif
