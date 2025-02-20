#include "tcp_server.h"

// void TcpServer::Observer::onConnectionAccepted([[maybe_unused]] int connection_id) {
// }

// void TcpServer::Observer::onReceived([[maybe_unused]] int connection_id,
//                                      [[maybe_unused]] const char *data,
//                                      [[maybe_unused]] const size_t size) {
// }

// void TcpServer::Observer::onConnectionClosed([[maybe_unused]] int connection_id) {
// }

TcpServer::TcpServer(basio::io_context &io_context, Observer &observer)
    : acceptor_{io_context}, connections_{}, observer_{observer},
      connection_count_{0}, is_accepted_{false}, is_closed_{false} {
}

bool TcpServer::listen(const basio_tcp &protocol, short port) {
    try {
        acceptor_.open(protocol);
        acceptor_.set_option(basio_tcp::acceptor::reuse_address(true));
        acceptor_.bind({protocol, port});
        acceptor_.listen(basio::socket_base::max_connections);
    } catch (const std::exception &e) {
        std::cerr << "TcpServer::listen() exception: "
                  << static_cast<std::string>(e.what()) << std::endl;
        return false;
    }

    return true;
}

void TcpServer::startAcceptingConnections() {
    if (!is_accepted_) {
        accept();
    }
}

void TcpServer::send(int connection_id, const char *data, size_t size) {
    if (connections_.count(connection_id) == 0) {
        std::cerr << "TcpServer::send() error: connection not found" << std::endl;
        return;
    }
    
    connections_.at(connection_id)->send(data, size);
}

void TcpServer::close() {
    is_closed_ = true;
    acceptor_.cancel();
    for (const auto &connection : connections_) {
        connection.second->close();
    }
    
    connections_.clear();
    is_closed_ = false;
    std::cout << "TcpServer disconnected" << std::endl;
}

void TcpServer::accept() {
    is_accepted_ = true;
    acceptor_.async_accept([this] (const bsys::error_code& error, basio_tcp::socket socket) {
        if (error) {
            std::cerr << "TcpServer::accept() error: " 
                        << error.message() << std::endl;
            is_accepted_ = false;
            return;
        } else {
            auto connection{TcpConnection::create(std::move(socket),
                                                  *this,
                                                  connection_count_)};
            connection->startReading();
            connections_.insert({connection_count_, std::move(connection)});
            std::cout << "TCPServer accepted connection" << std::endl;
            // observer_.onConnectionAccepted(connection_count_);
            connection_count_++;
        }
        accept();
    });
}

void TcpServer::onReceived(int connection_id, const char *data, size_t size) {
    // observer_.onReceived(connection_id, data, size);
}

void TcpServer::onConnectionClosed(int connection_id) {
    if (is_closed_) {
        return;
    }

    if (connections_.erase(connection_id) > 0) {
        std::cout << "TCPServer removed connection" << std::endl;
        // observer_.onConnectionClosed(connection_id);
    }
}

int main() {
    basio::io_context io_context;
    TcpServer::Observer observer;
    TcpServer server(io_context, observer);

    if (!server.listen(basio_tcp::v4(), 12345)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    server.startAcceptingConnections();
    io_context.run();
    return 0;
}