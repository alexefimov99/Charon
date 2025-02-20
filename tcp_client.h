#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <boost/asio.hpp>
#include <memory>

#include "tcp_connection.h"

class TcpClient : private TcpConnection::Observer {
public:
    struct Observer {
    virtual void onConnected();
    virtual void onReceived(const char *data, size_t size);
    virtual void onDisconnected();
    };

    TcpClient(basio::io_context &io_context, Observer &observer);

    void connect(const basio_tcp::endpoint &endpoint);
    void send(const char *data, size_t size);
    void disconnect();

private:
    void onReceived(int connection_id, const char *data, size_t size) override;
    void onConnectionClosed(int connection_id) override;

    basio::io_context& io_context_;
    std::shared_ptr<TcpConnection> connection_;
    Observer& observer_;
};

#endif // TCP_CLIENT_H
