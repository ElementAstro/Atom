#include "async_sockethub.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

#include "rate_limiter.hpp"
#include "socket_client_base.hpp"

namespace atom::async::connection {

using atom::connection::RateLimiter;
using atom::connection::SocketClientBase;

// Client class using template base - TCP version
using TcpClientImpl = SocketClientBase<asio::ip::tcp::socket>;
// Client class using template base - SSL version
using SslClientImpl =
    SocketClientBase<asio::ssl::stream<asio::ip::tcp::socket>>;

/**
 * @class Client
 * @brief Wrapper class that handles both TCP and SSL clients uniformly
 */
class Client {
public:
    // TCP constructor
    Client(size_t id, std::shared_ptr<asio::ip::tcp::socket> socket)
        : id_(id), is_ssl_(false) {
        tcp_client_ = std::make_shared<TcpClientImpl>(id, std::move(socket));
    }

    // SSL constructor
    Client(size_t id,
           std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket)
        : id_(id), is_ssl_(true) {
        ssl_client_ =
            std::make_shared<SslClientImpl>(id, std::move(ssl_socket));
    }

    [[nodiscard]] size_t getId() const noexcept { return id_; }

    [[nodiscard]] bool isAuthenticated() const noexcept {
        return is_ssl_ ? ssl_client_->isAuthenticated()
                       : tcp_client_->isAuthenticated();
    }

    void setAuthenticated(bool auth) noexcept {
        is_ssl_ ? ssl_client_->setAuthenticated(auth)
                : tcp_client_->setAuthenticated(auth);
    }

    void setMetadata(const std::string& key, const std::string& value) {
        is_ssl_ ? ssl_client_->setMetadata(key, value)
                : tcp_client_->setMetadata(key, value);
    }

    [[nodiscard]] std::string getMetadata(const std::string& key) const {
        return is_ssl_ ? ssl_client_->getMetadata(key)
                       : tcp_client_->getMetadata(key);
    }

    [[nodiscard]] std::string getRemoteAddress() const {
        return is_ssl_ ? ssl_client_->getRemoteAddress()
                       : tcp_client_->getRemoteAddress();
    }

    [[nodiscard]] std::chrono::system_clock::time_point getConnectTime()
        const noexcept {
        return is_ssl_ ? ssl_client_->getConnectTime()
                       : tcp_client_->getConnectTime();
    }

    [[nodiscard]] std::chrono::system_clock::time_point getLastActivityTime()
        const noexcept {
        return is_ssl_ ? ssl_client_->getLastActivityTime()
                       : tcp_client_->getLastActivityTime();
    }

    void updateLastActivity() noexcept {
        is_ssl_ ? ssl_client_->updateLastActivity()
                : tcp_client_->updateLastActivity();
    }

    void send(const Message& message,
              std::function<void(bool success)> callback = nullptr) {
        is_ssl_ ? ssl_client_->send(message, std::move(callback))
                : tcp_client_->send(message, std::move(callback));
    }

    void startReading(std::function<void(const Message&)> message_handler,
                      std::function<void()> disconnect_handler) {
        is_ssl_ ? ssl_client_->startReading(std::move(message_handler),
                                            std::move(disconnect_handler))
                : tcp_client_->startReading(std::move(message_handler),
                                            std::move(disconnect_handler));
    }

    void disconnect() {
        is_ssl_ ? ssl_client_->disconnect() : tcp_client_->disconnect();
    }

    [[nodiscard]] size_t getMessagesSent() const noexcept {
        return is_ssl_ ? ssl_client_->getMessagesSent()
                       : tcp_client_->getMessagesSent();
    }

    [[nodiscard]] size_t getMessagesReceived() const noexcept {
        return is_ssl_ ? ssl_client_->getMessagesReceived()
                       : tcp_client_->getMessagesReceived();
    }

    [[nodiscard]] size_t getBytesSent() const noexcept {
        return is_ssl_ ? ssl_client_->getBytesSent()
                       : tcp_client_->getBytesSent();
    }

    [[nodiscard]] size_t getBytesReceived() const noexcept {
        return is_ssl_ ? ssl_client_->getBytesReceived()
                       : tcp_client_->getBytesReceived();
    }

private:
    size_t id_;
    bool is_ssl_;
    std::shared_ptr<TcpClientImpl> tcp_client_;
    std::shared_ptr<SslClientImpl> ssl_client_;
};

// RateLimiter is now imported from rate_limiter.hpp

// Task queue for thread pool
class TaskQueue {
public:
    explicit TaskQueue(size_t thread_count = 4) : running_(true) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                while (running_) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        condition_.wait(lock, [this] {
                            return !running_ || !tasks_.empty();
                        });

                        if (!running_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ~TaskQueue() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }

        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    template <class F>
    void enqueue(F&& task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace(std::forward<F>(task));
        }
        condition_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool running_;
};

// Enhanced implementation of SocketHub
class SocketHub::Impl {
public:
    Impl(const SocketHubConfig& config)
        : config_(config),
          io_context_(),
          acceptor_(io_context_),
          ssl_context_(asio::ssl::context::sslv23),
          work_guard_(asio::make_work_guard(io_context_)),
          is_running_(false),
          next_client_id_(1),
          rate_limiter_(config.max_connections_per_ip,
                        config.max_messages_per_minute),
          task_queue_(4),  // Use 4 worker threads
          require_authentication_(false) {
        if (config.use_ssl) {
            configureSSL();
        }

        // Start statistics timer
        startStatsTimer();
    }

    ~Impl() { stop(); }

    void start(int port) {
        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen(config_.backlog_size);

            is_running_ = true;
            doAccept();

            if (!io_thread_.joinable()) {
                io_thread_ = std::thread([this]() { io_context_.run(); });
            }

            log(LogLevel::INFO_LEVEL,
                "SocketHub started on port " + std::to_string(port));
            stats_.start_time = std::chrono::system_clock::now();

        } catch (const std::exception& e) {
            log(LogLevel::ERROR_LEVEL,
                "Failed to start SocketHub: " + std::string(e.what()));
            throw;
        }
    }

    void stop() {
        if (is_running_) {
            is_running_ = false;

            // Cancel the acceptor
            asio::error_code ec;
            acceptor_.cancel(ec);

            // Stop the work guard to allow io_context to stop
            work_guard_.reset();

            // Disconnect all clients
            disconnectAllClients("Server shutting down");

            // Stop the io_context
            io_context_.stop();

            // Join the thread
            if (io_thread_.joinable()) {
                io_thread_.join();
            }

            log(LogLevel::INFO_LEVEL, "SocketHub stopped.");
        }
    }

    void restart() {
        int port = 0;
        try {
            port = acceptor_.local_endpoint().port();
        } catch (...) {
            log(LogLevel::ERROR_LEVEL, "Could not determine port for restart");
            return;
        }

        stop();

        // Reset the io_context
        io_context_.restart();

        // Close and reset the acceptor if it's open
        if (acceptor_.is_open()) {
            asio::error_code ec;
            acceptor_.close(ec);
            if (ec) {
                log(LogLevel::WARNING_LEVEL,
                    "Error closing acceptor during restart: " + ec.message());
            }
        }

        // Start again
        start(port);
    }

    void addMessageHandler(
        const std::function<void(const Message&, size_t)>& handler) {
        std::unique_lock lock(handler_mutex_);
        message_handlers_.push_back(handler);
    }

    void addConnectHandler(
        const std::function<void(size_t, const std::string&)>& handler) {
        std::unique_lock lock(connect_handler_mutex_);
        connect_handlers_.push_back(handler);
    }

    void addDisconnectHandler(
        const std::function<void(size_t, const std::string&)>& handler) {
        std::unique_lock lock(disconnect_handler_mutex_);
        disconnect_handlers_.push_back(handler);
    }

    void addErrorHandler(
        const std::function<void(const std::string&, size_t)>& handler) {
        std::unique_lock lock(error_handler_mutex_);
        error_handlers_.push_back(handler);
    }

    void broadcastMessage(const Message& message) {
        std::vector<std::shared_ptr<Client>> client_copies;
        {
            std::unique_lock lock(client_mutex_);
            for (const auto& [id, client] : clients_) {
                client_copies.push_back(client);
            }
        }

        for (const auto& client : client_copies) {
            client->send(message);
        }

        stats_.messages_sent += client_copies.size();
        stats_.bytes_sent += message.data.size() * client_copies.size();

        log(LogLevel::DEBUG_LEVEL,
            "Broadcasted message of " + std::to_string(message.data.size()) +
                " bytes to " + std::to_string(client_copies.size()) +
                " clients");
    }

    void sendMessageToClient(size_t client_id, const Message& message) {
        std::shared_ptr<Client> client;
        {
            std::unique_lock lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (client) {
            client->send(message, [this, client_id](bool success) {
                if (!success) {
                    this->handleError("Failed to send message to client",
                                      client_id);
                }
            });

            stats_.messages_sent++;
            stats_.bytes_sent += message.data.size();

            log(LogLevel::DEBUG_LEVEL,
                "Sent message of " + std::to_string(message.data.size()) +
                    " bytes to client " + std::to_string(client_id));
        } else {
            log(LogLevel::WARNING_LEVEL,
                "Attempted to send message to non-existent client: " +
                    std::to_string(client_id));
        }
    }

    void disconnectClient(size_t client_id, const std::string& reason) {
        std::shared_ptr<Client> client;
        {
            std::unique_lock lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
                clients_.erase(it);

                // Remove from all groups
                for (auto& [group_name, clients] : groups_) {
                    clients.erase(client_id);
                }
            }
        }

        if (client) {
            client->disconnect();

            // Call disconnect handlers
            notifyDisconnect(client_id, reason);

            stats_.active_connections--;

            // Remove from rate limiter
            rate_limiter_.releaseConnection(client->getRemoteAddress());

            log(LogLevel::INFO_LEVEL, "Client " + std::to_string(client_id) +
                                          " disconnected. Reason: " + reason);
        }
    }

    void createGroup(const std::string& group_name) {
        std::unique_lock lock(group_mutex_);
        groups_[group_name] = std::unordered_set<size_t>();
        log(LogLevel::INFO_LEVEL, "Created group: " + group_name);
    }

    void addClientToGroup(size_t client_id, const std::string& group_name) {
        bool client_exists = false;
        {
            std::unique_lock lock(client_mutex_);
            client_exists = clients_.find(client_id) != clients_.end();
        }

        if (!client_exists) {
            log(LogLevel::WARNING_LEVEL, "Cannot add non-existent client " +
                                             std::to_string(client_id) +
                                             " to group " + group_name);
            return;
        }

        std::unique_lock lock(group_mutex_);
        auto it = groups_.find(group_name);
        if (it == groups_.end()) {
            // Create the group if it doesn't exist
            groups_[group_name] = std::unordered_set<size_t>{client_id};
            log(LogLevel::INFO_LEVEL, "Created group " + group_name +
                                          " and added client " +
                                          std::to_string(client_id));
        } else {
            it->second.insert(client_id);
            log(LogLevel::INFO_LEVEL, "Added client " +
                                          std::to_string(client_id) +
                                          " to group " + group_name);
        }
    }

    void removeClientFromGroup(size_t client_id,
                               const std::string& group_name) {
        std::unique_lock lock(group_mutex_);
        auto it = groups_.find(group_name);
        if (it != groups_.end()) {
            it->second.erase(client_id);
            log(LogLevel::INFO_LEVEL, "Removed client " +
                                          std::to_string(client_id) +
                                          " from group " + group_name);
        }
    }

    void broadcastToGroup(const std::string& group_name,
                          const Message& message) {
        std::vector<size_t> client_ids;
        {
            std::unique_lock lock(group_mutex_);
            auto it = groups_.find(group_name);
            if (it != groups_.end()) {
                client_ids.assign(it->second.begin(), it->second.end());
            }
        }

        for (size_t client_id : client_ids) {
            sendMessageToClient(client_id, message);
        }

        log(LogLevel::DEBUG_LEVEL,
            "Broadcasted message to group " + group_name + " (" +
                std::to_string(client_ids.size()) + " clients)");
    }

    void setAuthenticator(
        const std::function<bool(const std::string&, const std::string&)>&
            authenticator) {
        authenticator_ = authenticator;
        log(LogLevel::INFO_LEVEL, "Custom authenticator set");
    }

    void requireAuthentication(bool require) {
        require_authentication_ = require;
        log(LogLevel::INFO_LEVEL, "Authentication requirement set to: " +
                                      std::string(require ? "true" : "false"));
    }

    void setClientMetadata(size_t client_id, const std::string& key,
                           const std::string& value) {
        std::shared_ptr<Client> client;
        {
            std::unique_lock lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (client) {
            client->setMetadata(key, value);
            log(LogLevel::DEBUG_LEVEL, "Set metadata '" + key +
                                           "' for client " +
                                           std::to_string(client_id));
        }
    }

    std::string getClientMetadata(size_t client_id, const std::string& key) {
        std::shared_ptr<Client> client;
        {
            std::unique_lock lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (client) {
            return client->getMetadata(key);
        }
        return "";
    }

    SocketHubStats getStatistics() const { return stats_; }

    void enableLogging(bool enable, LogLevel level) {
        logging_enabled_ = enable;
        log_level_ = level;
    }

    void setLogHandler(
        const std::function<void(LogLevel, const std::string&)>& handler) {
        log_handler_ = handler;
    }

    bool isRunning() const { return is_running_; }

    bool isClientConnected(size_t client_id) const {
        std::unique_lock lock(client_mutex_);
        return clients_.find(client_id) != clients_.end();
    }

    std::vector<size_t> getConnectedClients() const {
        std::vector<size_t> result;
        std::unique_lock lock(client_mutex_);
        result.reserve(clients_.size());
        for (const auto& [id, _] : clients_) {
            result.push_back(id);
        }
        return result;
    }

    std::vector<std::string> getGroups() const {
        std::vector<std::string> result;
        std::unique_lock lock(group_mutex_);
        result.reserve(groups_.size());
        for (const auto& [name, _] : groups_) {
            result.push_back(name);
        }
        return result;
    }

    std::vector<size_t> getClientsInGroup(const std::string& group_name) const {
        std::vector<size_t> result;
        std::unique_lock lock(group_mutex_);
        auto it = groups_.find(group_name);
        if (it != groups_.end()) {
            result.assign(it->second.begin(), it->second.end());
        }
        return result;
    }

private:
    void configureSSL() {
        try {
            ssl_context_.set_options(asio::ssl::context::default_workarounds |
                                     asio::ssl::context::no_sslv2 |
                                     asio::ssl::context::no_sslv3);

            // Set password callback if needed
            if (!config_.ssl_password.empty()) {
                ssl_context_.set_password_callback(
                    [this](std::size_t, asio::ssl::context::password_purpose) {
                        return config_.ssl_password;
                    });
            }

            // Load certificate chain
            if (!config_.ssl_cert_file.empty()) {
                ssl_context_.use_certificate_chain_file(config_.ssl_cert_file);
            }

            // Load private key
            if (!config_.ssl_key_file.empty()) {
                ssl_context_.use_private_key_file(config_.ssl_key_file,
                                                  asio::ssl::context::pem);
            }

            // Load DH parameters if provided
            if (!config_.ssl_dh_file.empty()) {
                ssl_context_.use_tmp_dh_file(config_.ssl_dh_file);
            }

            log(LogLevel::INFO_LEVEL, "SSL configured successfully");
        } catch (const std::exception& e) {
            log(LogLevel::ERROR_LEVEL,
                "SSL configuration error: " + std::string(e.what()));
            throw;
        }
    }

    void doAccept() {
        if (config_.use_ssl) {
            doAcceptSsl();
        } else {
            doAcceptTcp();
        }
    }

    void doAcceptTcp() {
        auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);

        acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
            if (!ec) {
                std::string remote_address = "unknown";
                try {
                    remote_address =
                        socket->remote_endpoint().address().to_string();

                    // Apply rate limiting if enabled
                    if (config_.enable_rate_limiting &&
                        !rate_limiter_.canConnect(remote_address)) {
                        log(LogLevel::WARNING_LEVEL,
                            "Rate limit exceeded for IP: " + remote_address);
                        socket->close();
                    } else {
                        handleNewTcpConnection(socket);
                    }
                } catch (const std::exception& e) {
                    handleError("Accept error: " + std::string(e.what()), 0);
                }
            } else {
                handleError("Accept error: " + ec.message(), 0);
            }

            if (is_running_) {
                doAcceptTcp();
            }
        });
    }

    void doAcceptSsl() {
        auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);

        acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
            if (!ec) {
                std::string remote_address = "unknown";
                try {
                    remote_address =
                        socket->remote_endpoint().address().to_string();

                    // Apply rate limiting if enabled
                    if (config_.enable_rate_limiting &&
                        !rate_limiter_.canConnect(remote_address)) {
                        log(LogLevel::WARNING_LEVEL,
                            "Rate limit exceeded for IP: " + remote_address);
                        socket->close();
                    } else {
                        auto ssl_socket = std::make_shared<
                            asio::ssl::stream<asio::ip::tcp::socket>>(
                            std::move(*socket), ssl_context_);

                        // Perform SSL handshake
                        ssl_socket->async_handshake(
                            asio::ssl::stream_base::server,
                            [this, ssl_socket, remote_address](
                                const std::error_code& handshake_ec) {
                                if (!handshake_ec) {
                                    handleNewSslConnection(ssl_socket);
                                } else {
                                    log(LogLevel::ERROR_LEVEL,
                                        "SSL handshake failed: " +
                                            handshake_ec.message() + " from " +
                                            remote_address);
                                    try {
                                        ssl_socket->lowest_layer().close();
                                    } catch (...) {
                                    }
                                }
                            });
                    }
                } catch (const std::exception& e) {
                    handleError("SSL accept error: " + std::string(e.what()),
                                0);
                }
            } else {
                handleError("Accept error: " + ec.message(), 0);
            }

            if (is_running_) {
                doAcceptSsl();
            }
        });
    }

    void handleNewTcpConnection(std::shared_ptr<asio::ip::tcp::socket> socket) {
        try {
            std::string remote_address =
                socket->remote_endpoint().address().to_string();
            size_t client_id = next_client_id_++;

            auto client = std::make_shared<Client>(client_id, socket);

            // Add client to the collection
            {
                std::unique_lock lock(client_mutex_);
                clients_[client_id] = client;
                stats_.total_connections++;
                stats_.active_connections++;
            }

            // Setup read handler
            client->startReading(
                [this, client_id](const Message& message) {
                    // Check rate limiting for messages
                    std::string client_ip = this->getClientIp(client_id);
                    if (config_.enable_rate_limiting &&
                        !rate_limiter_.canSendMessage(client_ip)) {
                        log(LogLevel::WARNING_LEVEL,
                            "Message rate limit exceeded for client " +
                                std::to_string(client_id) + " (" + client_ip +
                                ")");
                        return;
                    }

                    stats_.messages_received++;
                    stats_.bytes_received += message.data.size();

                    // Forward message to all registered handlers
                    this->notifyMessageHandlers(message, client_id);
                },
                [this, client_id]() {
                    // Handle disconnection
                    this->disconnectClient(client_id,
                                           "Connection closed by client");
                });

            // Set TCP keep-alive if configured
            if (config_.keep_alive) {
                socket->set_option(asio::socket_base::keep_alive(true));
            }

            // Notify connect handlers
            notifyConnect(client_id, remote_address);

            log(LogLevel::INFO_LEVEL,
                "New client connected: " + std::to_string(client_id) +
                    " from " + remote_address);

        } catch (const std::exception& e) {
            handleError(
                "Error handling new connection: " + std::string(e.what()), 0);
        }
    }

    void handleNewSslConnection(
        std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket) {
        try {
            std::string remote_address = ssl_socket->lowest_layer()
                                             .remote_endpoint()
                                             .address()
                                             .to_string();
            size_t client_id = next_client_id_++;

            auto client = std::make_shared<Client>(client_id, ssl_socket);

            // Add client to the collection
            {
                std::unique_lock lock(client_mutex_);
                clients_[client_id] = client;
                stats_.total_connections++;
                stats_.active_connections++;
            }

            // Setup read handler (similar to TCP but for SSL socket)
            client->startReading(
                [this, client_id](const Message& message) {
                    std::string client_ip = this->getClientIp(client_id);
                    if (config_.enable_rate_limiting &&
                        !rate_limiter_.canSendMessage(client_ip)) {
                        log(LogLevel::WARNING_LEVEL,
                            "Message rate limit exceeded for client " +
                                std::to_string(client_id) + " (" + client_ip +
                                ")");
                        return;
                    }

                    stats_.messages_received++;
                    stats_.bytes_received += message.data.size();
                    this->notifyMessageHandlers(message, client_id);
                },
                [this, client_id]() {
                    this->disconnectClient(client_id,
                                           "Connection closed by client");
                });

            // Set TCP keep-alive if configured
            if (config_.keep_alive) {
                ssl_socket->lowest_layer().set_option(
                    asio::socket_base::keep_alive(true));
            }

            notifyConnect(client_id, remote_address);
            log(LogLevel::INFO_LEVEL,
                "New SSL client connected: " + std::to_string(client_id) +
                    " from " + remote_address);

        } catch (const std::exception& e) {
            handleError(
                "Error handling new SSL connection: " + std::string(e.what()),
                0);
        }
    }

    void notifyMessageHandlers(const Message& message, size_t client_id) {
        // Copy the handlers to avoid holding the lock during callback execution
        std::vector<std::function<void(const Message&, size_t)>> handlers_copy;
        {
            std::unique_lock lock(handler_mutex_);
            handlers_copy = message_handlers_;
        }

        // Process message asynchronously in task queue
        for (const auto& handler : handlers_copy) {
            task_queue_.enqueue([handler, message, client_id]() {
                handler(message, client_id);
            });
        }
    }

    void notifyConnect(size_t client_id, const std::string& address) {
        std::vector<std::function<void(size_t, const std::string&)>>
            handlers_copy;
        {
            std::unique_lock lock(connect_handler_mutex_);
            handlers_copy = connect_handlers_;
        }

        for (const auto& handler : handlers_copy) {
            task_queue_.enqueue([handler, client_id, address]() {
                handler(client_id, address);
            });
        }
    }

    void notifyDisconnect(size_t client_id, const std::string& reason) {
        std::vector<std::function<void(size_t, const std::string&)>>
            handlers_copy;
        {
            std::unique_lock lock(disconnect_handler_mutex_);
            handlers_copy = disconnect_handlers_;
        }

        for (const auto& handler : handlers_copy) {
            task_queue_.enqueue(
                [handler, client_id, reason]() { handler(client_id, reason); });
        }
    }

    void handleError(const std::string& error_message, size_t client_id) {
        log(LogLevel::ERROR_LEVEL,
            error_message + " (client: " + std::to_string(client_id) + ")");

        std::vector<std::function<void(const std::string&, size_t)>>
            handlers_copy;
        {
            std::unique_lock lock(error_handler_mutex_);
            handlers_copy = error_handlers_;
        }

        for (const auto& handler : handlers_copy) {
            task_queue_.enqueue([handler, error_message, client_id]() {
                handler(error_message, client_id);
            });
        }
    }

    void disconnectAllClients(const std::string& reason) {
        std::vector<size_t> client_ids;
        {
            std::unique_lock lock(client_mutex_);
            client_ids.reserve(clients_.size());
            for (const auto& [id, _] : clients_) {
                client_ids.push_back(id);
            }
        }

        for (size_t id : client_ids) {
            disconnectClient(id, reason);
        }
    }

    std::string getClientIp(size_t client_id) {
        std::shared_ptr<Client> client;
        {
            std::unique_lock lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (client) {
            return client->getRemoteAddress();
        }
        return "unknown";
    }

    void log(LogLevel level, const std::string& message) {
        if (!logging_enabled_ || level < log_level_) {
            return;
        }

        if (log_handler_) {
            log_handler_(level, message);
        } else {
            // Default log to console
            std::string level_str;
            switch (level) {
                case LogLevel::TRACE:
                    level_str = "TRACE";
                    break;
                case LogLevel::DEBUG_LEVEL:
                    level_str = "DEBUG";
                    break;
                case LogLevel::INFO_LEVEL:
                    level_str = "INFO";
                    break;
                case LogLevel::WARNING_LEVEL:
                    level_str = "WARNING";
                    break;
                case LogLevel::ERROR_LEVEL:
                    level_str = "ERROR";
                    break;
                case LogLevel::FATAL_LEVEL:
                    level_str = "FATAL";
                    break;
            }

            std::cout << "[SocketHub][" << level_str << "] " << message
                      << std::endl;
        }
    }

    void startStatsTimer() {
        auto timer = std::make_shared<asio::steady_timer>(
            io_context_, std::chrono::seconds(60));
        timer->async_wait([this, timer](const std::error_code& ec) {
            if (!ec) {
                // Clean up inactive clients
                checkTimeouts();

                // Restart timer
                timer->expires_at(timer->expiry() + std::chrono::seconds(60));
                startStatsTimer();
            }
        });
    }

    void checkTimeouts() {
        if (!config_.connection_timeout.count()) {
            return;  // Timeout disabled
        }

        std::vector<size_t> timeout_clients;
        auto now = std::chrono::system_clock::now();

        {
            std::unique_lock lock(client_mutex_);
            for (const auto& [id, client] : clients_) {
                auto last_activity = client->getLastActivityTime();
                if (now - last_activity > config_.connection_timeout) {
                    timeout_clients.push_back(id);
                }
            }
        }

        for (size_t id : timeout_clients) {
            disconnectClient(id, "Connection timeout");
        }

        if (!timeout_clients.empty()) {
            log(LogLevel::INFO_LEVEL,
                "Disconnected " + std::to_string(timeout_clients.size()) +
                    " clients due to timeout");
        }
    }

    SocketHubConfig config_;
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    asio::ssl::context ssl_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic<bool> is_running_{false};
    std::unordered_map<size_t, std::shared_ptr<Client>> clients_;
    mutable std::shared_mutex
        client_mutex_;  // Use shared_mutex for read-heavy operations
    std::vector<std::function<void(const Message&, size_t)>> message_handlers_;
    mutable std::shared_mutex handler_mutex_;
    std::vector<std::function<void(size_t, const std::string&)>>
        connect_handlers_;
    mutable std::shared_mutex connect_handler_mutex_;
    std::vector<std::function<void(size_t, const std::string&)>>
        disconnect_handlers_;
    mutable std::shared_mutex disconnect_handler_mutex_;
    std::vector<std::function<void(const std::string&, size_t)>>
        error_handlers_;
    mutable std::shared_mutex error_handler_mutex_;
    std::atomic<size_t> next_client_id_{1};
    std::thread io_thread_;
    std::unordered_map<std::string, std::unordered_set<size_t>> groups_;
    mutable std::shared_mutex group_mutex_;
    RateLimiter rate_limiter_;
    TaskQueue task_queue_;
    std::function<bool(const std::string&, const std::string&)> authenticator_;
    std::atomic<bool> require_authentication_{false};
    std::atomic<bool> logging_enabled_{true};
    LogLevel log_level_ = LogLevel::INFO_LEVEL;
    std::function<void(LogLevel, const std::string&)> log_handler_;
    SocketHubStats stats_;  // Now uses atomic members from socket_types.hpp
};

// SocketHub implementation forwarding to Impl
SocketHub::SocketHub(const SocketHubConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

SocketHub::~SocketHub() = default;

void SocketHub::start(int port) { impl_->start(port); }

void SocketHub::stop() { impl_->stop(); }

void SocketHub::restart() { impl_->restart(); }

void SocketHub::addMessageHandler(
    const std::function<void(const Message&, size_t)>& handler) {
    impl_->addMessageHandler(handler);
}

void SocketHub::addConnectHandler(
    const std::function<void(size_t, const std::string&)>& handler) {
    impl_->addConnectHandler(handler);
}

void SocketHub::addDisconnectHandler(
    const std::function<void(size_t, const std::string&)>& handler) {
    impl_->addDisconnectHandler(handler);
}

void SocketHub::addErrorHandler(
    const std::function<void(const std::string&, size_t)>& handler) {
    impl_->addErrorHandler(handler);
}

void SocketHub::broadcastMessage(const Message& message) {
    impl_->broadcastMessage(message);
}

void SocketHub::sendMessageToClient(size_t client_id, const Message& message) {
    impl_->sendMessageToClient(client_id, message);
}

void SocketHub::disconnectClient(size_t client_id, const std::string& reason) {
    impl_->disconnectClient(client_id, reason);
}

void SocketHub::createGroup(const std::string& group_name) {
    impl_->createGroup(group_name);
}

void SocketHub::addClientToGroup(size_t client_id,
                                 const std::string& group_name) {
    impl_->addClientToGroup(client_id, group_name);
}

void SocketHub::removeClientFromGroup(size_t client_id,
                                      const std::string& group_name) {
    impl_->removeClientFromGroup(client_id, group_name);
}

void SocketHub::broadcastToGroup(const std::string& group_name,
                                 const Message& message) {
    impl_->broadcastToGroup(group_name, message);
}

void SocketHub::setAuthenticator(
    const std::function<bool(const std::string&, const std::string&)>&
        authenticator) {
    impl_->setAuthenticator(authenticator);
}

void SocketHub::requireAuthentication(bool require) {
    impl_->requireAuthentication(require);
}

void SocketHub::setClientMetadata(size_t client_id, const std::string& key,
                                  const std::string& value) {
    impl_->setClientMetadata(client_id, key, value);
}

std::string SocketHub::getClientMetadata(size_t client_id,
                                         const std::string& key) {
    return impl_->getClientMetadata(client_id, key);
}

SocketHubStats SocketHub::getStatistics() const {
    return impl_->getStatistics();
}

void SocketHub::enableLogging(bool enable, LogLevel level) {
    impl_->enableLogging(enable, level);
}

void SocketHub::setLogHandler(
    const std::function<void(LogLevel, const std::string&)>& handler) {
    impl_->setLogHandler(handler);
}

bool SocketHub::isRunning() const { return impl_->isRunning(); }

bool SocketHub::isClientConnected(size_t client_id) const {
    return impl_->isClientConnected(client_id);
}

std::vector<size_t> SocketHub::getConnectedClients() const {
    return impl_->getConnectedClients();
}

std::vector<std::string> SocketHub::getGroups() const {
    return impl_->getGroups();
}

std::vector<size_t> SocketHub::getClientsInGroup(
    const std::string& group_name) const {
    return impl_->getClientsInGroup(group_name);
}

}  // namespace atom::async::connection
