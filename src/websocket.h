#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <string>
#include <memory>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

namespace l2viz {

namespace beast = boost::beast;

class Websocket
{
    using ConnectionHandle = beast::websocket::stream<boost::asio::ssl::stream<beast::tcp_stream>>;

public:
                                        Websocket(const std::string& url);
                                        ~Websocket();

    // blocking send/receive
    void                                send(const std::string& request);
    void                                receive(std::function<void(const std::string&)> receiver);

    bool                                isConnected() const { return m_socketStream ? true : false; }

// private:
    void                                connect();
    void                                disconnect();

private:
    std::string                         m_host;
    std::unique_ptr<ConnectionHandle>   m_socketStream;
};

}

#endif
