#include "tcp_connection.h"

void TcpConnection::Observer::onReceived([[maybe_unused]] int connection_id,
                                         [[maybe_unused]] const char* data,
                                         [[maybe_unused]] const size_t length) {
}

void TcpConnection::Observer::onConnectionClosed([[maybe_unused]] int connection_id) {
}

TcpConnection::TcpConnection(basio_tcp::socket&& socket, Observer& observer, int id)
                             : socket_{std::move(socket)}, read_buffer_{}, write_buffer_{}
                             , write_buffer_mutex_{}, observer_{observer}, is_reading_{false}
                             , is_writing_{false}, id_{id} {
}

std::shared_ptr<TcpConnection>
TcpConnection::create(basio_tcp::socket&& socket, Observer& observer, int id) {
    return std::shared_ptr<TcpConnection>(new TcpConnection{std::move(socket), observer, id});
}

void TcpConnection::startReading() {
    if (!is_reading_) {
        read();
    }
}

void TcpConnection::send(const char* data, size_t length) {
    std::lock_guard<std::mutex> lguard{write_buffer_mutex_};
    std::ostream buffer_stream{&write_buffer_};
    buffer_stream.write(data, length);

    if (!is_writing_) {
        write();
    }
}

void TcpConnection::close() {
    try {
        socket_.cancel();
        socket_.close();
    } catch(const std::exception& e) {
        std::cerr << "TcpConnection::close() exception: "
                  << static_cast<std::string>(e.what()) << std::endl;
        return;
    }
    
    observer_.onConnectionClosed(id_);
}

void TcpConnection::read() {
    is_reading_ = true;

    auto buffers = read_buffer_.prepare(MAX_PACKAGE_SIZE);
    auto self = shared_from_this();

    socket_.async_read_some(buffers, [this, self] (const bsys::error_code& error, const size_t bytes_transferred) {
        if (error) {
            std::cerr << "TcpConnection::read() error: "
                        << error.message() << std::endl;
            return close();
        }

        std::cout << "Someone sent: " << static_cast<const char*>(read_buffer_.data().data()) << std::endl;

        read_buffer_.commit(bytes_transferred);
        observer_.onReceived(id_, static_cast<const char*>(read_buffer_.data().data()),
                             bytes_transferred);
        read_buffer_.consume(bytes_transferred);
        
        read();
    });
}

void TcpConnection::write() {
    is_writing_ = true;
    auto self = shared_from_this();

    socket_.async_write_some(write_buffer_.data(), [this, self](const bsys::error_code& error, const size_t bytesTransferred) {
        if (error) {
            std::cerr << "TcpConnection::write() error: "
                    << error.message() << std::endl;
            return close();
        }
    
        std::lock_guard<std::mutex> lguard{write_buffer_mutex_};
        write_buffer_.consume(bytesTransferred);
        if (write_buffer_.size() == 0) {
            is_writing_ = false;
            return;
        }

        write();
    });
}