#include "websocket.h"
#include "utils/logger.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>

static std::string __port = "443";
static std::string __path = "/ws";

namespace l2viz {

Websocket::Websocket(const std::string& url)
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

    connect();
}

Websocket::~Websocket()
{
    disconnect();
}

void Websocket::connect()
{
    // This is very annoying. Had to do this from the very level to make it work.
    // The ssl ca certificate file path doesn't load automatically. Need to manually specifiy.
    try {
        using tcp = boost::asio::ip::tcp;

        boost::asio::io_context ioContext;

        // address resolver
        tcp::resolver resolver(ioContext);

        // ssl context
        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        ctx.set_verify_mode(boost::asio::ssl::verify_peer);
        ctx.load_verify_file("/etc/ssl/cert.pem");  // THIS IS REQUIRED

        // ssl stream
        boost::asio::ssl::stream<beast::tcp_stream> stream(ioContext, ctx);

        // resolve and connect
        beast::get_lowest_layer(stream).connect(resolver.resolve(m_host, __port));

        // ssl handshake
        stream.handshake(boost::asio::ssl::stream_base::client);

        m_socketStream = std::make_unique<ConnectionHandle>(std::move(stream));
        m_socketStream->handshake(m_host, __path);

        LOG_INFO("Connection established with {}.", m_host);
    } catch (const std::exception& e) {
        m_socketStream.reset();
        LOG_ERROR("Failed to connect to {}. Error: {}", m_host, e.what());
    }
}

void Websocket::send(const std::string& request)
{
    if (m_socketStream->is_open()) {
        m_socketStream->write(boost::asio::buffer(request));
    }
}

void Websocket::receive(std::function<void(const std::string&)> receiver)
{
    beast::flat_buffer buffer;
    m_socketStream->read(buffer);
    receiver(beast::buffers_to_string(buffer.data()));
}

void Websocket::disconnect()
{
    if (isConnected()) {
        LOG_TRACE("Closing websocket.");
        try {
            // close the connection
            m_socketStream->close(beast::websocket::close_code::normal);

            // shutdown the ssl stream
            beast::get_lowest_layer(*m_socketStream).close();

            m_socketStream.reset();

            LOG_INFO("Disconnected from {}.", m_host);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to disconnect from {}, Error: {}", m_host, e.what());
        }
        m_socketStream->close(beast::websocket::close_code::normal);
    } else {
        LOG_TRACE("No active connection to disconnect.");
    }
}

}
