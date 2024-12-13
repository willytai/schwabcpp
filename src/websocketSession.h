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

namespace schwabcpp {

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
{
    using WebsocketStream = websocket::stream<ssl::stream<beast::tcp_stream>>;
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

    void                                                asyncSend(const std::string& request, std::function<void()> callback = {});
    void                                                asyncReceive(std::function<void(const std::string&)> callback);

    void                                                startReceiverLoop(std::function<void(const std::string&)> callback);
    void                                                stopReceiverLoop();

    bool                                                isConnected() const;

    void                                                onReconnect(std::function<void()> callback) { m_onReconnection = callback; }

    void                                                shutdown();

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
    void                                                onClose(
                                                            std::function<void()> callback,
                                                            beast::error_code ec
                                                        );
    void                                                drainOnClose(
                                                            std::function<void()> callback,
                                                            beast::error_code ec,
                                                            std::size_t bytesTransferred
                                                        );

    void                                                asyncDisconnect(std::function<void()> callback = {});

    // this is for reconnecting when the read loop fails
    void                                                asyncReconnect();

    // what the sender daemon runs
private:
    void                                                sendMessages();

private:
    // -- need a reference to these to reconnect the stream
    boost::asio::io_context&                            m_ioContext;
    boost::asio::ssl::context&                          m_sslContext;

    // -- data
    std::string                                         m_host;
    std::string                                         m_port;
    std::string                                         m_path;
    beast::flat_buffer                                  m_buffer;

    // -- callback on reconnection
    std::function<void()>                               m_onReconnection;

    // -- handles
    tcp::resolver                                       m_resolver;
    std::unique_ptr<WebsocketStream>                    m_websocketStream;

    // -- receiver loop
    bool                                                m_receiverLoopRunning;
    bool                                                m_shouldReconnectReceiverLoop;

    // -- message queue and sender control
    class CVState {
        typedef uint8_t Flag;
    public:
        // flag for running the sender daemon
        inline static const Flag RunSenderDaemon = 1 << 0;

        // flag for running the receiver loop
        inline static const Flag RunReceiverLoop = 1 << 1;

        // whether message queue is empty
        inline static const Flag MessageQueueNotEmpty = 1 << 2;

        enum State {
            // connection info
            Disconnected        = 1,
            HostResolved        = 2,
            Connected           = 3,
            SSLHandshaked       = 4,
            WebsocketHandshaked = 5,  // we need to reach to this state to start sending requests
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
    CVState                                             m_state;
    std::thread                                         m_senderDaemon;
    mutable std::mutex                                  m_mutex_state;        // mutex for connection flag
    mutable std::mutex                                  m_mutex_messageQ;     // mutex for message queue
    std::condition_variable                             m_cv;                 // for notifying the sender daemon
    struct MessageData {
        std::string request;
        std::function<void()> callback;
    };
    std::queue<MessageData>                             m_messageQueue;
};

}

#endif
