#include "tcp_client.h"

void TcpClient::Observer::onConnected() {
}

void TcpClient::Observer::onReceived([[maybe_unused]] const char *data, [[maybe_unused]] size_t size) {
}

void TcpClient::Observer::onDisconnected() {
}

TcpClient::TcpClient(basio::io_context &ioContext, Observer &observer)
                     : io_context_{ioContext}, connection_{}, observer_{observer} {
}

void TcpClient::connect(const basio_tcp::endpoint &endpoint) {
    if (connection_) {
        return;
    }

    auto socket = std::make_shared<basio_tcp::socket>(io_context_);
    socket->async_connect(endpoint, [this, socket](const auto &error) {
        if (error) {
            std::cerr << "TcpClient::connect() error: "
                      << error.message() << std::endl;
            observer_.onDisconnected();
            return;
        }

        connection_ = TcpConnection::create(std::move(*socket), *this);
        connection_->startReading();
        std::cout << "TcpClient connected" << std::endl;
        observer_.onConnected();
    });
}

void TcpClient::send(const char *data, size_t size) {
    if (!connection_) {
        std::cerr << "TcpClient::send() error: no connection" << std::endl;
        return;
    }
    connection_->send(data, size);
}

void TcpClient::disconnect() {
    if (connection_) {
        connection_->close();
    }
}

void TcpClient::onReceived([[maybe_unused]] int connection_id,
                           const char *data,
                           size_t size) {
    observer_.onReceived(data, size);
}

void TcpClient::onConnectionClosed([[maybe_unused]] int connection_id) {
    if (connection_) {
        connection_.reset();
        std::cout << "TcpClient disconnected" << std::endl;
        observer_.onDisconnected();
    }
}

int main() {
    basio::io_context io_context;
    TcpClient::Observer observer;
    TcpClient client(io_context, observer);

    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 12345;

    basio_tcp::endpoint endpoint(basio_tcp::v4(), server_port);
    client.connect(endpoint);

    std::thread io_thread([&]() { io_context.run(); });

    while (true) {
        std::string message;
        std::getline(std::cin, message);
        
        if (message == "/exit") {
            client.disconnect();
            break;
        }
        
        client.send(message.c_str(), message.size());
    }

    io_thread.join();
    return 0;
}