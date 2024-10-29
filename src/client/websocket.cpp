#include "websocket.h"
#include "../utils/logger.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>

static std::string __port = "443";
static std::string __path = "/ws";

namespace l2viz {

Websocket::Websocket(const std::string& url)
    : m_sslContext(boost::asio::ssl::context::sslv23)
    , m_workGuard(net::make_work_guard(m_ioContext))
{
    LOG_INFO("Initializing websocket.");

    // parse the url
    auto pos = url.find("://");
    if (pos != std::string::npos) {
        m_host = url.substr(pos+3);
    } else {
        m_host = url;
    }
    pos = m_host.find("/");
    if (pos != std::string::npos) {
        m_host = m_host.substr(0, pos);
    }

    // ssl context settings
    m_sslContext.set_verify_mode(boost::asio::ssl::verify_peer);
    m_sslContext.load_verify_file("/etc/ssl/cert.pem");  // THIS IS REQUIRED
}

Websocket::~Websocket()
{
    LOG_TRACE("Disconnecting websocket session.");
    m_session->asyncDisconnect();

    LOG_TRACE("Stopping websocket io context.");
    // not using boost::asio::io_context::stop so that the io context
    // will wait for the pending disconnect call to finish before exiting
    m_workGuard.reset();
    if (m_ioContextThread.joinable()) {
        m_ioContextThread.join();
    }
}

void Websocket::asyncConnect(std::function<void()> onConnected)
{
    // session
    m_session = std::make_shared<WebsocketSession>(
        m_ioContext,
        m_sslContext,
        m_host,
        __port,
        __path
    );

    // queue connect call
    m_session->asyncConnect(onConnected);
    // run the io context
    m_ioContextThread = std::thread(
        [this]() {
            m_ioContext.run();
            LOG_TRACE("IO context thread exiting.");
        }
    );
}

void Websocket::asyncSend(const std::string& request, std::function<void()> callback)
{
    m_session->asyncSend(request, callback);
}

void Websocket::asyncReceive(std::function<void(const std::string&)> callback)
{
    m_session->asyncReceive(callback);
}

void Websocket::startReceiverLoop(std::function<void(const std::string&)> callback)
{
    m_session->startReceiverLoop(callback);
}

void Websocket::stopReceiverLoop()
{
    m_session->stopReceiverLoop();
}

}
