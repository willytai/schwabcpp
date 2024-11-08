#include "websocketSession.h"
#include "utils/logger.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>

namespace schwabcpp {

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
    , m_state(CVState::Disconnected)
    , m_resolver(net::make_strand(ioContext))
    , m_receiverLoopRunning(false)
    , m_websocketStream(net::make_strand(ioContext), sslContext)
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
    // set the run sender daemon flag
    // don't need mutex here, sender not launched yet
    m_state.setFlag(CVState::RunSenderDaemon, true);

    // launch the sender
    // it will wait unitl connection established to start doing things
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
        {
            std::lock_guard<std::mutex> lock(m_mutex_state);
            // successfully resolved, update the state
            m_state.setState(CVState::HostResolved);
        }

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

        {
            std::lock_guard<std::mutex> lock(m_mutex_state);
            // Successfully connected
            m_state.setState(CVState::Connected);
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
        {
            std::lock_guard<std::mutex> lock(m_mutex_state);
            // SSL handshake passed
            m_state.setState(CVState::SSLHandshaked);
        }

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
            std::lock_guard<std::mutex> lock(m_mutex_state);
            // websocket handshake passed
            m_state.setState(CVState::WebsocketHandshaked);
        }

        // notify the connection status
        m_cv.notify_all();

        // invoke the custom handler
        onFinalHandshake();
    }
}

void WebsocketSession::asyncDisconnect()
{
    // update the flag to Disconnected
    // unset RunSenderDaemon and RunReceiverLoop
    // notify if sender daemon is running
    {
        std::lock_guard<std::mutex> lock(m_mutex_state);
        m_state.setState(CVState::Disconnected);
        m_state.setFlag(CVState::RunReceiverLoop, false);
        m_state.setFlag(CVState::RunSenderDaemon, false);
    }

    // stop the sender daemon if it is running
    if (m_senderDaemon.joinable()) {
        LOG_TRACE("Stopping websocket session sender daemon.");
        m_cv.notify_all();
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
    // put things in the message queue
    {
        std::lock_guard lock(m_mutex_messageQ);
        m_messageQueue.emplace(std::move(request), callback);
    }
    // set the flag
    {
        std::lock_guard lock(m_mutex_state);
        m_state.setFlag(CVState::MessageQueueNotEmpty, true);
    }
    // notify sender
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

    // set the flag
    {
        std::lock_guard<std::mutex> lock(m_mutex_state);
        m_state.setFlag(CVState::RunReceiverLoop, true);
    }

    if (!m_receiverLoopRunning) {
        m_receiverLoopRunning = true;
        m_websocketStream.async_read(
            m_buffer,
            beast::bind_front_handler(
                &WebsocketSession::onReceiveLoop,
                shared_from_this(),
                callback
            )
        );
    } else {
        LOG_TRACE("Websocket session receiver loop already running.");
    }
}

void WebsocketSession::stopReceiverLoop()
{
    std::lock_guard<std::mutex> lock(m_mutex_state);

    // no point stopping if loop not running
    if (m_state.testFlag(CVState::RunReceiverLoop)) {
        LOG_INFO("Websocket session stopping receiver loop.");

        // this stops the receiver loop but DOESN'T
        // stop the message sender
        // only asyncDisconnect does
        m_state.setFlag(CVState::RunReceiverLoop, false);
    } else {
        LOG_WARN("Receiver loop not running.");
    }
}

void WebsocketSession::onReceiveLoop(
    std::function<void(const std::string&)> callback,
    beast::error_code ec,
    std::size_t bytesTransferred)
{
    if (ec) {
        // update the flag
        {
            std::lock_guard lock(m_mutex_state);
            m_state.setFlag(CVState::RunReceiverLoop, false);
        }
        // receive loop will exit for now if an error is caught
        // TODO: reconnect when this happens
        LOG_ERROR("Websocket loop receive failed. Receiver loop stopped.");
        LOG_ERROR("Error code: {}", ec.value());
        LOG_ERROR("Error what: {}", ec.what());
        LOG_ERROR("Error message: {}", ec.message());
        LOG_ERROR("Error category.name: {}", ec.category().name());
        LOG_ERROR("Error to_string: {}", ec.to_string());
        LOG_ERROR("Error location.to_string: {}", ec.location().to_string());
    } else {
        // proceed if we are in the right state with the right flag
        std::unique_lock<std::mutex> lock(m_mutex_state);
        if (!m_state.testState(CVState::WebsocketHandshaked)) {
            lock.unlock();

            LOG_TRACE("Websocket not connected, stopping websocket session receiver loop.");
            m_buffer.clear();
        } else if (m_state.testFlag(CVState::RunReceiverLoop)) {
            lock.unlock();

            if (callback) {
                callback(beast::buffers_to_string(m_buffer.data()));
            }
            m_buffer.clear();

            // queue the next read
            m_websocketStream.async_read(
                m_buffer,
                beast::bind_front_handler(
                    &WebsocketSession::onReceiveLoop,
                    shared_from_this(),
                    callback
                )
            );
        } else {
            lock.unlock();
            m_receiverLoopRunning = false;

            LOG_TRACE("Websocket session receiver loop run flag unset. Stopping loop.");
        }
    }
}

bool WebsocketSession::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex_state);
    return m_state.testState(CVState::WebsocketHandshaked);
}

// -- sender daemon
void WebsocketSession::sendMessages()
{
    std::unique_lock<std::mutex> stateLock(m_mutex_state);
    while (m_state.testFlag(CVState::RunSenderDaemon)) {

        // now we will wati until the state flag is notified and post check should wake sender
        m_cv.wait(stateLock, [self = shared_from_this()] { return self->m_state.shouldWakeSender(); });

        // if we woke up because the run flag is unset, break
        if (!m_state.testFlag(CVState::RunSenderDaemon)) break;

        LOG_TRACE("Websocket session message queue size: {}", m_messageQueue.size());

        // send all while connected
        std::unique_lock<std::mutex> queueLock(m_mutex_messageQ);
        while (!m_messageQueue.empty() && m_state.testState(CVState::WebsocketHandshaked)) {
            // done checking state, release
            stateLock.unlock();

            MessageData payload = m_messageQueue.front();
            m_messageQueue.pop();

            // dequeue complete, release
            queueLock.unlock();

            // consecutive writes has to be blocking
            // TODO:
            // add error handling here, this is not guaranteed to succeed
            m_websocketStream.write(net::buffer(payload.request));
            // run the callback
            if (payload.callback) {
                payload.callback();
            }

            // lock before checking the queue and state
            queueLock.lock();
            stateLock.lock();
        }

        // queue and state are locked at this point
        if (m_messageQueue.empty()) {
            m_state.setFlag(CVState::MessageQueueNotEmpty, false);
        }
    }
}

// -- CVState
WebsocketSession::CVState::CVState(State state)
    : _flag(0x0)
    , _state(state)
{}

void WebsocketSession::CVState::setFlag(Flag flag, bool b)
{
    if (b) {
        _flag |= flag;
    } else {
        _flag &= ~flag;
    }
}

void WebsocketSession::CVState::setState(State state)
{
    _state = state;
}

bool WebsocketSession::CVState::testFlag(Flag flag) const
{
    return _flag & flag;
}

bool WebsocketSession::CVState::testState(State state) const
{
    return _state == state;
}

bool WebsocketSession::CVState::shouldWakeSender() const
{
    // when should we wake the sender?
    // 1. websocket handshake successful and messeage queue not empty
    // 2. when the run sender flag is unset 
    return testState(CVState::WebsocketHandshaked) && testFlag(CVState::MessageQueueNotEmpty)
        || !testFlag(CVState::RunSenderDaemon);
}

}
