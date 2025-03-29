#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>

using boost::asio::ip::tcp;
namespace asio = boost::asio;

class ClientSession;
typedef boost::shared_ptr<ClientSession> ClientSessionPtr;

class ConnectionManager;

class ClientSession : public boost::enable_shared_from_this<ClientSession> {
public:
    ClientSession(tcp::socket socket, ConnectionManager& manager, size_t id)
        : socket_(std::move(socket)), connection_manager_(manager), client_id_(id) {
        buffer_.resize(8192);
    }

    void start() {
        auto self = shared_from_this();
        auto remote_endpoint = socket_.remote_endpoint();
        std::cout << "Новое подключение [ID: " << client_id_ << "] с " 
                  << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << std::endl;

        asio::socket_base::keep_alive option(true);
        socket_.set_option(option);

        do_read();
    }

    void send(const std::string& message) {
        auto self = shared_from_this();

        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(message);

        if (!write_in_progress) {
            do_write();
        }
    }

    tcp::socket& socket() {
        return socket_;
    }

    size_t get_id() const {
        return client_id_;
    }

private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string message(buffer_.data(), length);

                    std::cout << "Получено от клиента " << client_id_ << ": " << message;

                    auto now = std::chrono::system_clock::now();
                    auto now_time = std::chrono::system_clock::to_time_t(now);
                    std::string time_str = std::ctime(&now_time);
                    std::string response = "Сервер получил: " + message + "Время: " + time_str;

                    send(response);
                    do_read();
                } else {
                    handle_error(ec);
                }
            });
    }

    void do_write() {
        auto self = shared_from_this();
        asio::async_write(
            socket_,
            asio::buffer(write_queue_.front()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_queue_.erase(write_queue_.begin());
                    if (!write_queue_.empty()) {
                        do_write();
                    }
                } else {
                    handle_error(ec);
                }
            });
    }

    void handle_error(const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted) {
            std::cerr << "Ошибка для клиента " << client_id_ << ": " << ec.message() << std::endl;
            close();
        }
    }

    void close() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

    tcp::socket socket_;
    ConnectionManager& connection_manager_;
    std::vector<char> buffer_;
    std::vector<std::string> write_queue_;
    size_t client_id_;
};

class ConnectionManager {
public:
    void start(ClientSessionPtr client) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_[client->get_id()] = client;
        client->start();
    }

    void stop(size_t client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            clients_.erase(it);
        }
    }

    void stop_all() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            boost::system::error_code ec;
            client.second->socket().close(ec);
        }
        clients_.clear();
    }

    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            client.second->send(message);
        }
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_.size();
    }

private:
    std::unordered_map<size_t, ClientSessionPtr> clients_;
    mutable std::mutex clients_mutex_;
};

class Server {
public:
    Server(asio::io_context& io_context, const tcp::endpoint& endpoint)
        : io_context_(io_context),
          acceptor_(io_context, endpoint),
          signals_(io_context, SIGINT, SIGTERM),
          next_client_id_(0) {

        signals_.async_wait(
            [this](const boost::system::error_code& /*ec*/, int /*signo*/) {
                std::cout << "Завершение работы сервера..." << std::endl;
                acceptor_.close();
                connection_manager_.stop_all();
            });

        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        start_stats_reporter();
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    connection_manager_.start(
                        boost::make_shared<ClientSession>(
                            std::move(socket), 
                            connection_manager_, 
                            next_client_id_++));
                }

                if (acceptor_.is_open()) {
                    do_accept();
                }
            });
    }

    void start_stats_reporter() {
        auto timer = std::make_shared<asio::steady_timer>(io_context_, std::chrono::seconds(60));

        timer->async_wait([this, timer](const boost::system::error_code& /*ec*/) {
            std::cout << "[Статистика] Активных подключений: " << connection_manager_.count() 
                      << ", Всего подключений: " << next_client_id_ << std::endl;

            timer->expires_at(timer->expiry() + std::chrono::seconds(60));
            start_stats_reporter();
        });
    }

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    asio::signal_set signals_;
    ConnectionManager connection_manager_;
    std::atomic<size_t> next_client_id_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Использование: chrono <порт>\n";
            return 1;
        }

        unsigned short port = std::stoi(argv[1]);

        asio::io_context io_context;

        asio::executor_work_guard<asio::io_context::executor_type> work_guard = 
            asio::make_work_guard(io_context);

        const int num_threads = std::thread::hardware_concurrency();

        Server server(io_context, tcp::endpoint(tcp::v4(), port));

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        std::cout << "Сервер запущен на порту " << port << " с " << num_threads << " потоками обработки" << std::endl;

        for (auto& thread : threads) {
            thread.join();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Исключение: " << e.what() << "\n";
        return 1;
    }

    return 0;
}