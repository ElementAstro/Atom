#include "sockethub.hpp"

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::connection {

// Forward declaration
class SocketHubImpl;

/**
 * @class SocketException
 * @brief Custom exception for socket-related errors.
 */
class SocketException : public std::runtime_error {
public:
    explicit SocketException(const std::string& msg)
        : std::runtime_error(msg) {}
};

/**
 * @class ClientConnection
 * @brief Manages a single client connection asynchronously.
 *
 * This class encapsulates the socket, I/O operations, and timeout handling
 * for a connected client. It is designed to be managed by a `shared_ptr`
 * to handle its lifetime in asynchronous contexts.
 */
class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(asio::ip::tcp::socket socket, int id, SocketHubImpl& hub);
    ~ClientConnection();

    ClientConnection(const ClientConnection&) = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;

    void start();
    void send(std::string_view message);
    void disconnect(bool notifyHub = true);

    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] int getId() const noexcept;
    [[nodiscard]] const std::string& getAddress() const noexcept;
    [[nodiscard]] std::chrono::steady_clock::time_point getConnectedTime()
        const noexcept;
    [[nodiscard]] uint64_t getBytesReceived() const noexcept;
    [[nodiscard]] uint64_t getBytesSent() const noexcept;

private:
    void do_read();
    void do_write();
    void on_timeout(const asio::error_code& ec);
    void reset_timer();

    asio::ip::tcp::socket socket_;
    int id_;
    std::string address_;
    SocketHubImpl& hub_;
    asio::steady_timer timer_;

    std::atomic<bool> connected_;
    const std::chrono::steady_clock::time_point connectedTime_;
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint64_t> bytesSent_{0};

    std::vector<char> read_buffer_;
    static constexpr size_t read_buffer_size_ = 16384;

    std::deque<std::string> write_queue_;
    std::mutex write_mutex_;
};

/**
 * @class SocketHubImpl
 * @brief Private implementation of the SocketHub using Asio.
 *
 * This class contains the core logic for the socket hub, including the
 * Asio I/O context, acceptor, thread pool, and client management.
 */
class SocketHubImpl {
public:
    SocketHubImpl();
    ~SocketHubImpl() noexcept;

    SocketHubImpl(const SocketHubImpl&) = delete;
    SocketHubImpl& operator=(const SocketHubImpl&) = delete;

    void start(int port);
    void stop() noexcept;

    void addMessageHandler(std::function<void(std::string_view)> handler);
    void addConnectHandler(std::function<void(int, std::string_view)> handler);
    void addDisconnectHandler(
        std::function<void(int, std::string_view)> handler);

    size_t broadcast(std::string_view message);
    bool sendTo(int clientId, std::string_view message);
    std::vector<ClientInfo> getConnectedClients() const;
    size_t getClientCount() const noexcept;
    void setClientTimeout(std::chrono::seconds timeout);

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] int getPort() const noexcept;
    [[nodiscard]] std::chrono::seconds getClientTimeout() const;

    void removeClient(int clientId);
    void notifyMessage(std::string_view message);
    void notifyConnect(int clientId, std::string_view clientAddr);
    void notifyDisconnect(int clientId, std::string_view clientAddr);

private:
    void do_accept();

    std::atomic<bool> running_{false};
    int serverPort_{0};
    std::atomic<int> nextClientId_{1};
    std::chrono::seconds clientTimeout_;

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::jthread> thread_pool_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>>
        work_;

    std::unordered_map<int, std::shared_ptr<ClientConnection>> clients_;
    mutable std::shared_mutex clientsMutex_;

    std::function<void(std::string_view)> messageHandler_;
    std::function<void(int, std::string_view)> connectHandler_;
    std::function<void(int, std::string_view)> disconnectHandler_;
    std::mutex handlerMutex_;
};

// ClientConnection implementation
ClientConnection::ClientConnection(asio::ip::tcp::socket socket, int id,
                                   SocketHubImpl& hub)
    : socket_(std::move(socket)),
      id_(id),
      hub_(hub),
      timer_(socket_.get_executor()),
      connected_(true),
      connectedTime_(std::chrono::steady_clock::now()) {
    try {
        address_ = std::format("{}:{}",
                               socket_.remote_endpoint().address().to_string(),
                               socket_.remote_endpoint().port());
    } catch (const std::system_error& e) {
        spdlog::warn("Failed to get remote endpoint for client {}: {}", id,
                     e.what());
        address_ = "unknown";
    }
}

ClientConnection::~ClientConnection() {
    if (isConnected()) {
        disconnect(false);
    }
}

void ClientConnection::start() {
    read_buffer_.resize(read_buffer_size_);
    reset_timer();
    do_read();
}

void ClientConnection::disconnect(bool notifyHub) {
    if (!connected_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    asio::error_code ec;
    timer_.cancel();
    if (socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec)) {
        spdlog::warn("Socket shutdown failed: {}", ec.message());
    }
    if (socket_.close(ec)) {
        spdlog::warn("Failed to close socket: {}", ec.message());
    }

    if (notifyHub) {
        hub_.removeClient(id_);
    }
}

void ClientConnection::send(std::string_view message) {
    if (!isConnected()) {
        return;
    }

    bool write_in_progress;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        write_queue_.emplace_back(message);
    }

    if (!write_in_progress) {
        asio::post(socket_.get_executor(),
                   [self = shared_from_this()] { self->do_write(); });
    }
}

void ClientConnection::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buffer_),
        [self](const asio::error_code& ec, size_t bytes_transferred) {
            if (!ec) {
                self->bytesReceived_.fetch_add(bytes_transferred,
                                               std::memory_order_relaxed);
                self->reset_timer();
                self->hub_.notifyMessage(std::string_view(
                    self->read_buffer_.data(), bytes_transferred));
                self->do_read();
            } else if (ec != asio::error::operation_aborted) {
                self->disconnect();
            }
        });
}

void ClientConnection::do_write() {
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(write_queue_.front()),
        [self](const asio::error_code& ec, size_t bytes_transferred) {
            if (!ec) {
                self->bytesSent_.fetch_add(bytes_transferred,
                                           std::memory_order_relaxed);

                bool more_to_write;
                {
                    std::lock_guard<std::mutex> lock(self->write_mutex_);
                    self->write_queue_.pop_front();
                    more_to_write = !self->write_queue_.empty();
                }

                if (more_to_write) {
                    self->do_write();
                }
            } else if (ec != asio::error::operation_aborted) {
                self->disconnect();
            }
        });
}

void ClientConnection::on_timeout(const asio::error_code& ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    if (timer_.expiry() <= asio::steady_timer::clock_type::now()) {
        spdlog::info("Client timeout: {} (ID: {})", address_, id_);
        disconnect();
    }
}

void ClientConnection::reset_timer() {
    auto timeout = hub_.getClientTimeout();
    if (timeout.count() > 0) {
        timer_.expires_after(timeout);
        timer_.async_wait(
            [self = shared_from_this()](const asio::error_code& ec) {
                self->on_timeout(ec);
            });
    }
}

bool ClientConnection::isConnected() const noexcept {
    return connected_.load(std::memory_order_acquire);
}
int ClientConnection::getId() const noexcept { return id_; }
const std::string& ClientConnection::getAddress() const noexcept {
    return address_;
}
std::chrono::steady_clock::time_point ClientConnection::getConnectedTime()
    const noexcept {
    return connectedTime_;
}
uint64_t ClientConnection::getBytesReceived() const noexcept {
    return bytesReceived_.load(std::memory_order_relaxed);
}
uint64_t ClientConnection::getBytesSent() const noexcept {
    return bytesSent_.load(std::memory_order_relaxed);
}

// SocketHubImpl implementation
SocketHubImpl::SocketHubImpl()
    : acceptor_(io_context_), clientTimeout_(std::chrono::seconds(60)) {}

SocketHubImpl::~SocketHubImpl() noexcept {
    try {
        stop();
    } catch (const std::exception& e) {
        spdlog::error("Exception in SocketHubImpl destructor: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown exception in SocketHubImpl destructor");
    }
}

void SocketHubImpl::start(int port) {
    if (port <= 0 || port > 65535) {
        throw std::invalid_argument(std::format("Invalid port: {}", port));
    }

    if (running_.load(std::memory_order_acquire)) {
        spdlog::warn("SocketHub already running");
        return;
    }

    try {
        asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(),
                                         static_cast<uint16_t>(port));
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(asio::socket_base::max_listen_connections);
    } catch (const std::system_error& e) {
        throw SocketException(
            std::format("Failed to bind to port {}: {}", port, e.what()));
    }

    serverPort_ = port;
    running_.store(true, std::memory_order_release);
    spdlog::info("SocketHub started on port {}", port);

    do_accept();

    work_.emplace(asio::make_work_guard(io_context_));
    const auto thread_count = std::max(1u, std::thread::hardware_concurrency());
    thread_pool_.reserve(thread_count);
    for (unsigned i = 0; i < thread_count; ++i) {
        thread_pool_.emplace_back([this] {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                spdlog::error("Exception in worker thread: {}", e.what());
            }
        });
    }
}

void SocketHubImpl::stop() noexcept {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    spdlog::info("Stopping SocketHub...");

    asio::post(io_context_, [this]() { acceptor_.close(); });

    {
        std::unique_lock<std::shared_mutex> lock(clientsMutex_);
        for (auto const& [id, client] : clients_) {
            if (client) {
                client->disconnect(false);
            }
        }
        clients_.clear();
    }

    work_.reset();
    if (!io_context_.stopped()) {
        io_context_.stop();
    }

    for (auto& t : thread_pool_) {
        if (t.joinable()) {
            t.join();
        }
    }
    thread_pool_.clear();

    if (io_context_.stopped()) {
        io_context_.restart();
    }

    serverPort_ = 0;
    spdlog::info("SocketHub stopped");
}

void SocketHubImpl::do_accept() {
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                const int clientId =
                    nextClientId_.fetch_add(1, std::memory_order_relaxed);

                auto client = std::make_shared<ClientConnection>(
                    std::move(socket), clientId, *this);

                {
                    std::unique_lock<std::shared_mutex> lock(clientsMutex_);
                    clients_[clientId] = client;
                }

                notifyConnect(clientId, client->getAddress());
                client->start();

                do_accept();
            } else if (ec != asio::error::operation_aborted) {
                spdlog::error("Accept error: {}", ec.message());
            }
        });
}

void SocketHubImpl::removeClient(int clientId) {
    std::string clientAddr;
    {
        std::unique_lock<std::shared_mutex> lock(clientsMutex_);
        auto it = clients_.find(clientId);
        if (it != clients_.end()) {
            clientAddr = it->second->getAddress();
            clients_.erase(it);
        }
    }

    if (!clientAddr.empty()) {
        spdlog::info("Client disconnected: {} (ID: {})", clientAddr, clientId);
        notifyDisconnect(clientId, clientAddr);
    }
}

void SocketHubImpl::notifyMessage(std::string_view message) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    if (messageHandler_) {
        try {
            messageHandler_(message);
        } catch (const std::exception& e) {
            spdlog::error("Message handler exception: {}", e.what());
        }
    }
}

void SocketHubImpl::notifyConnect(int clientId, std::string_view clientAddr) {
    spdlog::info("New client: {} (ID: {})", clientAddr, clientId);
    std::lock_guard<std::mutex> lock(handlerMutex_);
    if (connectHandler_) {
        try {
            connectHandler_(clientId, clientAddr);
        } catch (const std::exception& e) {
            spdlog::error("Connect handler exception: {}", e.what());
        }
    }
}

void SocketHubImpl::notifyDisconnect(int clientId,
                                     std::string_view clientAddr) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    if (disconnectHandler_) {
        try {
            disconnectHandler_(clientId, clientAddr);
        } catch (const std::exception& e) {
            spdlog::error("Disconnect handler exception: {}", e.what());
        }
    }
}

void SocketHubImpl::addMessageHandler(
    std::function<void(std::string_view)> handler) {
    if (!handler) {
        throw std::invalid_argument("Invalid message handler");
    }
    std::lock_guard<std::mutex> lock(handlerMutex_);
    messageHandler_ = std::move(handler);
}

void SocketHubImpl::addConnectHandler(
    std::function<void(int, std::string_view)> handler) {
    if (!handler) {
        throw std::invalid_argument("Invalid connect handler");
    }
    std::lock_guard<std::mutex> lock(handlerMutex_);
    connectHandler_ = std::move(handler);
}

void SocketHubImpl::addDisconnectHandler(
    std::function<void(int, std::string_view)> handler) {
    if (!handler) {
        throw std::invalid_argument("Invalid disconnect handler");
    }
    std::lock_guard<std::mutex> lock(handlerMutex_);
    disconnectHandler_ = std::move(handler);
}

size_t SocketHubImpl::broadcast(std::string_view message) {
    if (message.empty() || !isRunning()) {
        return 0;
    }

    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    size_t successCount = 0;
    for (const auto& [_, client] : clients_) {
        if (client && client->isConnected()) {
            client->send(message);
            ++successCount;
        }
    }
    return successCount;
}

bool SocketHubImpl::sendTo(int clientId, std::string_view message) {
    if (message.empty() || !isRunning()) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    const auto it = clients_.find(clientId);
    if (it != clients_.end() && it->second && it->second->isConnected()) {
        it->second->send(message);
        return true;
    }
    return false;
}

std::vector<ClientInfo> SocketHubImpl::getConnectedClients() const {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    std::vector<ClientInfo> result;
    result.reserve(clients_.size());

    for (const auto& [id, client] : clients_) {
        if (client && client->isConnected()) {
            result.emplace_back(
                ClientInfo{.id = client->getId(),
                           .address = client->getAddress(),
                           .connected_time = client->getConnectedTime(),
                           .bytes_received = client->getBytesReceived(),
                           .bytes_sent = client->getBytesSent()});
        }
    }
    return result;
}

size_t SocketHubImpl::getClientCount() const noexcept {
    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
    return clients_.size();
}

void SocketHubImpl::setClientTimeout(std::chrono::seconds timeout) {
    if (timeout.count() > 0) {
        clientTimeout_ = timeout;
        spdlog::info("Client timeout set to {} seconds", timeout.count());
    } else {
        clientTimeout_ = std::chrono::seconds(0);
        spdlog::info("Client timeout disabled");
    }
}

bool SocketHubImpl::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}

int SocketHubImpl::getPort() const noexcept { return serverPort_; }

std::chrono::seconds SocketHubImpl::getClientTimeout() const {
    return clientTimeout_;
}

// SocketHub public API implementation
SocketHub::SocketHub() : impl_(std::make_unique<SocketHubImpl>()) {}
SocketHub::~SocketHub() noexcept = default;
SocketHub::SocketHub(SocketHub&&) noexcept = default;
SocketHub& SocketHub::operator=(SocketHub&&) noexcept = default;
void SocketHub::start(int port) { impl_->start(port); }
void SocketHub::stop() noexcept { impl_->stop(); }
void SocketHub::addHandlerImpl(std::function<void(std::string_view)> handler) {
    impl_->addMessageHandler(std::move(handler));
}
void SocketHub::addConnectHandlerImpl(
    std::function<void(int, std::string_view)> handler) {
    impl_->addConnectHandler(std::move(handler));
}
void SocketHub::addDisconnectHandlerImpl(
    std::function<void(int, std::string_view)> handler) {
    impl_->addDisconnectHandler(std::move(handler));
}
size_t SocketHub::broadcast(std::string_view message) {
    return impl_->broadcast(message);
}
bool SocketHub::sendTo(int clientId, std::string_view message) {
    return impl_->sendTo(clientId, message);
}
std::vector<ClientInfo> SocketHub::getConnectedClients() const {
    return impl_->getConnectedClients();
}
size_t SocketHub::getClientCount() const noexcept {
    return impl_->getClientCount();
}
bool SocketHub::isRunning() const noexcept { return impl_->isRunning(); }
void SocketHub::setClientTimeout(std::chrono::seconds timeout) {
    impl_->setClientTimeout(timeout);
}
int SocketHub::getPort() const noexcept { return impl_->getPort(); }

}  // namespace atom::connection
