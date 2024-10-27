#include "websocketSession.h"
#include "utils/logger.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>

namespace l2viz {

WebsocketSession::WebsocketSession(
    net::io_context& ioContext,
    ssl::context& sslContext,
    const std::string& host,
    const std::string& port,
    const std::string& path
)
    : m_host(host)
    , m_port(port)
    , m_path(path)
    , m_isConnected(false)
    , m_resolver(net::make_strand(ioContext))
    , m_websocketStream(net::make_strand(ioContext), sslContext)
    , m_runSenderDaemon(false)
{
}

WebsocketSession::~WebsocketSession()
{
    asyncDisconnect();

    // join here
    if (m_senderDaemon.joinable()) {
        m_senderDaemon.join();
    }
}

void WebsocketSession::asyncConnect(std::function<void()> onFinalHandshake)
{
    // launch the sender
    // it will wait unitl connection established to start doing things
    m_runSenderDaemon = true;
    m_senderDaemon = std::thread(
        &WebsocketSession::sendMessages,
        shared_from_this()
    );

    m_resolver.async_resolve(
        m_host,
        m_port,
        beast::bind_front_handler(
            &WebsocketSession::onResolve,
            shared_from_this(),
            onFinalHandshake    // pass it down the chain
        )
    );
}

void WebsocketSession::onResolve(
    std::function<void()> onFinalHandshake,
    beast::error_code ec,
    tcp::resolver::results_type results)
{
    if (ec) {
        LOG_ERROR("Resolve failed. Error: {}", ec.message());
    } else {
        // Set a timeout on the operation
        beast::get_lowest_layer(m_websocketStream).expires_after(std::chrono::seconds(30));

        // Connect
        beast::get_lowest_layer(m_websocketStream).async_connect(
            results,
            beast::bind_front_handler(
                &WebsocketSession::onConnect,
                shared_from_this(),
                onFinalHandshake    // pass it down the chain
            )
        );
    }
}

void WebsocketSession::onConnect(
    std::function<void()> onFinalHandshake,
    beast::error_code ec,
    tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec) {
        LOG_ERROR("Connection failed. Error: {}", ec.message());
    } else {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(
                m_websocketStream.next_layer().native_handle(),
                m_host.c_str()))
        {
            ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category());
            LOG_ERROR("Connection failed. Error: {}", ec.message());
        }

        // Set a timeout on the operation
        beast::get_lowest_layer(m_websocketStream).expires_after(std::chrono::seconds(30));

        // Perform the ssl handshake
        m_websocketStream.next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &WebsocketSession::onSSLHandshake,
                shared_from_this(),
                onFinalHandshake    // pass it down the chain
            )
        );
    }
}

void WebsocketSession::onSSLHandshake(
    std::function<void()> onFinalHandshake,
    beast::error_code ec)
{
    if (ec) {
        LOG_ERROR("SSL handshake failed. Error: {}", ec.message());
    } else {
        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(m_websocketStream).expires_never();

        // Set suggested timeout settings for the websocket
        m_websocketStream.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::client
            )
        );

        // Set a decorator to change the User-Agent of the handshake
        m_websocketStream.set_option(
            websocket::stream_base::decorator(
                [](websocket::request_type& req)
                {
                    req.set(beast::http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-client-async-ssl");
                }
            )
        );

        // Perform the websocket handshake
        m_websocketStream.async_handshake(
            m_host + ":" + m_port,
            m_path,
            beast::bind_front_handler(
                &WebsocketSession::onWebsocketHandshake,
                shared_from_this(),
                onFinalHandshake    // pass it down the chain
            )
        );
    }
}

void WebsocketSession::onWebsocketHandshake(
    std::function<void()> onFinalHandshake,
    beast::error_code ec)
{
    if (ec) {
        LOG_ERROR("Websocket handshake failed. Error: {}", ec.message());
    } else {
        LOG_INFO("Websocket successfully connected to {}.", m_host);

        // set the flag
        {
            std::lock_guard<std::mutex> lock(m_mutex_connected);
            m_isConnected = true;
        }
        // notify the connection status
        m_cv.notify_all();
        // invoke the custom handler
        onFinalHandshake();
    }
}

void WebsocketSession::asyncDisconnect()
{
    // stop the sender daemon if it is running
    if (m_senderDaemon.joinable() && m_runSenderDaemon) {
        LOG_TRACE("Stopping websocket session sender daemon.");
        {
            std::lock_guard<std::mutex> lock(m_mutex_runSenderDaemon);
            m_runSenderDaemon = false;
        }
        m_cv.notify_all();
    }

    // set the m_isConnected flag
    // can't do it in the onClose handler
    // need to make sure the loop is stopped BEFORE close
    {
        std::lock_guard<std::mutex> lock(m_mutex_connected);
        m_isConnected = false;
    }

    // NOTE: Explicitly closing always results in stream truncated error for some unknown reason...
    //       Leaving it and just let the websocket stream be destroyed turns out to work normally, so
    //       I'll leave this part commented out.

    // if (m_websocketStream.is_open()) {
    //     LOG_TRACE("Closing websocket stream.");
    //     m_websocketStream.async_close(
    //         websocket::close_code::normal,
    //         beast::bind_front_handler(
    //             &WebsocketSession::onClose,
    //             shared_from_this()
    //         )
    //     );
    // }
}

void WebsocketSession::onClose(beast::error_code ec)
{
    if (ec) {
        LOG_ERROR("Close failed. Error: {}", ec.message());
    } else {
        LOG_INFO("Websocket successfully closed.");
    }
}

void WebsocketSession::asyncSend(const std::string& request, std::function<void()> callback)
{
    // put things in the message queue and notify the sender
    std::unique_lock lock(m_mutex_messageQ);
    m_messageQueue.emplace(std::move(request), callback);
    lock.unlock();
    m_cv.notify_one();
}

void WebsocketSession::onWrite(
    std::function<void()> callback,
    beast::error_code ec,
    std::size_t bytesTransferred)
{
    if (ec) {
        LOG_ERROR("Websocket write failed. Error: {}", ec.message());
    } else {
        if (callback) {
            callback();
        }
    }
}

void WebsocketSession::asyncReceive(std::function<void(const std::string&)> callback)
{
    m_websocketStream.async_read(
        m_buffer,
        beast::bind_front_handler(
            &WebsocketSession::onRead,
            shared_from_this(),
            callback
        )
    );
}

void WebsocketSession::onRead(
    std::function<void(const std::string&)> callback,
    beast::error_code ec,
    std::size_t bytesTransferred)
{
    if (ec) {
        LOG_ERROR("Websocket read failed. Error: {}", ec.message());
    } else {
        if (callback) {
            callback(beast::buffers_to_string(m_buffer.data()));
        }
        m_buffer.clear();
    }
}

void WebsocketSession::startReceiverLoop(std::function<void(const std::string&)> callback)
{
    LOG_INFO("Websocket session starting receiver loop.");

    m_websocketStream.async_read(
        m_buffer,
        beast::bind_front_handler(
            &WebsocketSession::onReceiveLoop,
            shared_from_this(),
            callback
        )
    );
}

void WebsocketSession::onReceiveLoop(
    std::function<void(const std::string&)> callback,
    beast::error_code ec,
    std::size_t bytesTransferred)
{
    if (ec) {
        LOG_ERROR("Websocket loop receive failed. Error: {}", ec.message());
    } else {
        // only trigger the callback when disconnect is not pending
        if (callback && m_isConnected) {
            callback(beast::buffers_to_string(m_buffer.data()));
        }
    }

    m_buffer.clear();

    // queue next receive if connected
    if (m_isConnected) {
        m_websocketStream.async_read(
            m_buffer,
            beast::bind_front_handler(
                &WebsocketSession::onReceiveLoop,
                shared_from_this(),
                callback
            )
        );
    } else {
        LOG_TRACE("Disconnecting, stopping websocket session receive loop.");
    }
}

bool WebsocketSession::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex_connected);
    return m_isConnected;
}

// -- sender daemon
void WebsocketSession::sendMessages()
{
    while (m_runSenderDaemon) {

        // wait until connected
        {
            std::unique_lock<std::mutex> lock(m_mutex_connected);
            m_cv.wait(lock, [self = shared_from_this()]{ return self->m_isConnected || !self->m_runSenderDaemon; });

            if (!m_runSenderDaemon) break;
        }

        // wait until messages pending
        std::unique_lock<std::mutex> lock(m_mutex_messageQ);
        m_cv.wait(lock, [self = shared_from_this()]{ return !self->m_messageQueue.empty() || !self->m_runSenderDaemon; });

        if (!m_runSenderDaemon) break;

        LOG_TRACE("Websocket session message queue size: {}", m_messageQueue.size());

        while (!m_messageQueue.empty()) {
            MessageData payload = m_messageQueue.front();
            m_messageQueue.pop();
            lock.unlock();  // release the lock here

            // consecutive writes has to be blocking
            m_websocketStream.write(net::buffer(payload.request));
            // run the callback
            if (payload.callback) {
                payload.callback();
            }

            // lock before checking the queue
            lock.lock();
        }
    }
}

}
