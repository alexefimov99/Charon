#ifndef TCP_SERVER_H
#define TCP_SERVER_H

/*#include <boost/asio.hpp>
#include <iostream>
#include <string>

namespace basio = boost::asio;
namespace bsys = boost::system;
using basio_tcp = boost::asio::ip::tcp;

class NetworkConnecion : public std::enable_shared_from_this<NetworkConnecion> {
public:
    explicit NetworkConnecion(basio_tcp::socket socket);

public:
    void start();

private:
    std::string address_;
    char data_[1024];
    basio_tcp::socket socket_;

private:
    void read();
    void write(const std::size_t length);
};

class Server {
public:
    Server(basio::io_context& io_context, short port);
    ~Server();

private:
    basio_tcp::acceptor acceptor_;

    std::shared_ptr<NetworkConnecion> nc_;

private:
    void accept();
    void waiting_connection();

};*/

#include <boost/asio.hpp>
#include <unordered_map>

#include "tcp_connection.h"

class TcpServer : private TcpConnection::Observer {
public:
    struct Observer {
        // virtual void onConnectionAccepted(int connection_id);
        // virtual void onReceived(int connection_id, const char *data, size_t size);
        // virtual void onConnectionClosed(int connection_id);
    };

    TcpServer(basio::io_context &ioContext, Observer &observer);

    bool listen(const basio_tcp &protocol, basio::ip::port_type port);
    void startAcceptingConnections();
    void send(int connection_id, const char *data, size_t size);
    void close();

private:
    void accept();
    void onReceived(int connection_id, const char *data, size_t size) override;
    void onConnectionClosed(int connection_id) override;

    basio_tcp::acceptor acceptor_;
    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;
    Observer& observer_;
    int connection_count_;
    bool is_accepted_;
    bool is_closed_;
};

#endif // TCP_SERVER_H
