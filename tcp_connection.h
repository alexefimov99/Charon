#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <memory>
#include <mutex>

namespace basio = boost::asio;
namespace bsys = boost::system;
using basio_tcp = basio::ip::tcp;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    struct Observer {
        virtual void onReceived(int connection_id, const char* data, const size_t length);
        virtual void onConnectionClosed(int connection_id);
    };

    static std::shared_ptr<TcpConnection> create(basio_tcp::socket&& socket,
                                                 Observer& observer,
                                                 int id = 0);

    void startReading();
    void send(const char* data_, size_t length);
    void close();

private:
    TcpConnection(basio_tcp::socket&& socket, Observer& observer, int id);
    
    void read();
    void write();

    basio_tcp::socket socket_;
    basio::streambuf read_buffer_;
    basio::streambuf write_buffer_;

    std::mutex write_buffer_mutex_;
    Observer& observer_;
    bool is_reading_;
    bool is_writing_;
    int id_;
};

#endif // TCP_CONNECTION_H