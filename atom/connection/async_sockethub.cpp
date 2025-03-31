#include "async_sockethub.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace atom::async::connection {

// Client class to manage individual connections
class Client {
public:
    Client(size_t id, std::shared_ptr<asio::ip::tcp::socket> socket)
        : id_(id),
          socket_(socket),
          is_authenticated_(false),
          connect_time_(std::chrono::system_clock::now()),
          last_activity_time_(connect_time_),
          messages_sent_(0),
          messages_received_(0),
          bytes_sent_(0),
          bytes_received_(0) {}

    // SSL version constructor
    Client(size_t id,
           std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket)
        : id_(id),
          ssl_socket_(ssl_socket),
          is_authenticated_(false),
          connect_time_(std::chrono::system_clock::now()),
          last_activity_time_(connect_time_),
          messages_sent_(0),
          messages_received_(0),
          bytes_sent_(0),
          bytes_received_(0) {}

    size_t getId() const { return id_; }

    bool isAuthenticated() const { return is_authenticated_; }
    void setAuthenticated(bool auth) { is_authenticated_ = auth; }

    void setMetadata(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        metadata_[key] = value;
    }

    std::string getMetadata(const std::string& key) const {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        auto it = metadata_.find(key);
        if (it != metadata_.end()) {
            return it->second;
        }
        return "";
    }

    std::string getRemoteAddress() const {
        try {
            if (socket_) {
                return socket_->remote_endpoint().address().to_string();
            } else if (ssl_socket_) {
                return ssl_socket_->lowest_layer()
                    .remote_endpoint()
                    .address()
                    .to_string();
            }
        } catch (const std::exception& e) {
            // Endpoint might be closed
        }
        return "unknown";
    }

    std::chrono::system_clock::time_point getConnectTime() const {
        return connect_time_;
    }

    std::chrono::system_clock::time_point getLastActivityTime() const {
        return last_activity_time_;
    }

    void updateLastActivity() {
        last_activity_time_ = std::chrono::system_clock::now();
    }

    void send(const Message& message,
              std::function<void(bool success)> callback = nullptr) {
        if (socket_) {
            sendViaTcp(message, callback);
        } else if (ssl_socket_) {
            sendViaSsl(message, callback);
        }
    }

    void startReading(std::function<void(const Message&)> message_handler,
                      std::function<void()> disconnect_handler) {
        message_handler_ = message_handler;
        disconnect_handler_ = disconnect_handler;

        if (socket_) {
            doReadTcp();
        } else if (ssl_socket_) {
            doReadSsl();
        }
    }

    void disconnect() {
        try {
            if (socket_) {
                socket_->close();
            } else if (ssl_socket_) {
                ssl_socket_->lowest_layer().close();
            }
        } catch (const std::exception& e) {
            // Already closed or other error
        }
    }

    // Statistics
    size_t getMessagesSent() const { return messages_sent_; }
    size_t getMessagesReceived() const { return messages_received_; }
    size_t getBytesSent() const { return bytes_sent_; }
    size_t getBytesReceived() const { return bytes_received_; }

private:
    void doReadTcp() {
        auto buffer = std::make_shared<std::vector<char>>(4096);
        socket_->async_read_some(
            asio::buffer(*buffer),
            [this, buffer](std::error_code ec, std::size_t length) {
                if (!ec) {
                    bytes_received_ += length;
                    messages_received_++;
                    updateLastActivity();

                    Message msg;
                    msg.type = Message::Type::TEXT;
                    msg.data = std::vector<char>(buffer->begin(),
                                                 buffer->begin() + length);
                    msg.sender_id = id_;

                    if (message_handler_) {
                        message_handler_(msg);
                    }

                    doReadTcp();
                } else {
                    if (disconnect_handler_) {
                        disconnect_handler_();
                    }
                }
            });
    }

    void doReadSsl() {
        auto buffer = std::make_shared<std::vector<char>>(4096);
        ssl_socket_->async_read_some(
            asio::buffer(*buffer),
            [this, buffer](std::error_code ec, std::size_t length) {
                if (!ec) {
                    bytes_received_ += length;
                    messages_received_++;
                    updateLastActivity();

                    Message msg;
                    msg.type = Message::Type::TEXT;
                    msg.data = std::vector<char>(buffer->begin(),
                                                 buffer->begin() + length);
                    msg.sender_id = id_;

                    if (message_handler_) {
                        message_handler_(msg);
                    }

                    doReadSsl();
                } else {
                    if (disconnect_handler_) {
                        disconnect_handler_();
                    }
                }
            });
    }

    void sendViaTcp(const Message& message,
                    std::function<void(bool)> callback) {
        bytes_sent_ += message.data.size();
        messages_sent_++;
        updateLastActivity();

        asio::async_write(*socket_, asio::buffer(message.data),
                          [this, callback](std::error_code ec, std::size_t) {
                              if (callback) {
                                  callback(!ec);
                              }
                          });
    }

    void sendViaSsl(const Message& message,
                    std::function<void(bool)> callback) {
        bytes_sent_ += message.data.size();
        messages_sent_++;
        updateLastActivity();

        asio::async_write(*ssl_socket_, asio::buffer(message.data),
                          [this, callback](std::error_code ec, std::size_t) {
                              if (callback) {
                                  callback(!ec);
                              }
                          });
    }

    size_t id_;
    std::shared_ptr<asio::ip::tcp::socket> socket_;
    std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket_;
    bool is_authenticated_;
    std::function<void(const Message&)> message_handler_;
    std::function<void()> disconnect_handler_;
    std::chrono::system_clock::time_point connect_time_;
    std::chrono::system_clock::time_point last_activity_time_;
    std::atomic<size_t> messages_sent_;
    std::atomic<size_t> messages_received_;
    std::atomic<size_t> bytes_sent_;
    std::atomic<size_t> bytes_received_;
    std::unordered_map<std::string, std::string> metadata_;
    mutable std::mutex metadata_mutex_;
};

// Rate limiter for DoS protection
class RateLimiter {
public:
    RateLimiter(int max_connections_per_ip, int max_messages_per_minute)
        : max_connections_per_ip_(max_connections_per_ip),
          max_messages_per_minute_(max_messages_per_minute) {}

    bool canConnect(const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& count = connection_count_[ip_address];
        if (count >= max_connections_per_ip_) {
            return false;
        }

        count++;
        return true;
    }

    void releaseConnection(const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = connection_count_.find(ip_address);
        if (it != connection_count_.end() && it->second > 0) {
            it->second--;
        }
    }

    bool canSendMessage(const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto& message_times = message_history_[ip_address];

        // Remove messages older than 1 minute
        auto minute_ago = now - std::chrono::minutes(1);
        message_times.erase(
            std::remove_if(
                message_times.begin(), message_times.end(),
                [&minute_ago](const auto& time) { return time < minute_ago; }),
            message_times.end());

        if (message_times.size() >= max_messages_per_minute_) {
            return false;
        }

        message_times.push_back(now);
        return true;
    }

private:
    int max_connections_per_ip_;
    int max_messages_per_minute_;
    std::unordered_map<std::string, int> connection_count_;
    std::unordered_map<std::string,
                       std::vector<std::chrono::system_clock::time_point>>
        message_history_;
    std::mutex mutex_;
};

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

            log(LogLevel::INFO,
                "SocketHub started on port " + std::to_string(port));
            stats_.start_time = std::chrono::system_clock::now();

        } catch (const std::exception& e) {
            log(LogLevel::ERROR,
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

            log(LogLevel::INFO, "SocketHub stopped.");
        }
    }

    void restart() {
        int port = 0;
        try {
            port = acceptor_.local_endpoint().port();
        } catch (...) {
            log(LogLevel::ERROR, "Could not determine port for restart");
            return;
        }

        stop();

        // Reset the io_context
        io_context_.restart();
        // TODO: Reset the acceptor
        // work_guard_ = asio::make_work_guard(io_context_);

        // Start again
        start(port);
    }

    void addMessageHandler(
        const std::function<void(const Message&, size_t)>& handler) {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        message_handlers_.push_back(handler);
    }

    void addConnectHandler(
        const std::function<void(size_t, const std::string&)>& handler) {
        std::lock_guard<std::mutex> lock(connect_handler_mutex_);
        connect_handlers_.push_back(handler);
    }

    void addDisconnectHandler(
        const std::function<void(size_t, const std::string&)>& handler) {
        std::lock_guard<std::mutex> lock(disconnect_handler_mutex_);
        disconnect_handlers_.push_back(handler);
    }

    void addErrorHandler(
        const std::function<void(const std::string&, size_t)>& handler) {
        std::lock_guard<std::mutex> lock(error_handler_mutex_);
        error_handlers_.push_back(handler);
    }

    void broadcastMessage(const Message& message) {
        std::vector<std::shared_ptr<Client>> client_copies;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            for (const auto& [id, client] : clients_) {
                client_copies.push_back(client);
            }
        }

        for (const auto& client : client_copies) {
            client->send(message);
        }

        stats_.messages_sent += client_copies.size();
        stats_.bytes_sent += message.data.size() * client_copies.size();

        log(LogLevel::DEBUG,
            "Broadcasted message of " + std::to_string(message.data.size()) +
                " bytes to " + std::to_string(client_copies.size()) +
                " clients");
    }

    void sendMessageToClient(size_t client_id, const Message& message) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
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

            log(LogLevel::DEBUG,
                "Sent message of " + std::to_string(message.data.size()) +
                    " bytes to client " + std::to_string(client_id));
        } else {
            log(LogLevel::WARNING,
                "Attempted to send message to non-existent client: " +
                    std::to_string(client_id));
        }
    }

    void disconnectClient(size_t client_id, const std::string& reason) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
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

            log(LogLevel::INFO, "Client " + std::to_string(client_id) +
                                    " disconnected. Reason: " + reason);
        }
    }

    void createGroup(const std::string& group_name) {
        std::lock_guard<std::mutex> lock(group_mutex_);
        groups_[group_name] = std::unordered_set<size_t>();
        log(LogLevel::INFO, "Created group: " + group_name);
    }

    void addClientToGroup(size_t client_id, const std::string& group_name) {
        bool client_exists = false;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            client_exists = clients_.find(client_id) != clients_.end();
        }

        if (!client_exists) {
            log(LogLevel::WARNING, "Cannot add non-existent client " +
                                       std::to_string(client_id) +
                                       " to group " + group_name);
            return;
        }

        std::lock_guard<std::mutex> lock(group_mutex_);
        auto it = groups_.find(group_name);
        if (it == groups_.end()) {
            // Create the group if it doesn't exist
            groups_[group_name] = std::unordered_set<size_t>{client_id};
            log(LogLevel::INFO, "Created group " + group_name +
                                    " and added client " +
                                    std::to_string(client_id));
        } else {
            it->second.insert(client_id);
            log(LogLevel::INFO, "Added client " + std::to_string(client_id) +
                                    " to group " + group_name);
        }
    }

    void removeClientFromGroup(size_t client_id,
                               const std::string& group_name) {
        std::lock_guard<std::mutex> lock(group_mutex_);
        auto it = groups_.find(group_name);
        if (it != groups_.end()) {
            it->second.erase(client_id);
            log(LogLevel::INFO, "Removed client " + std::to_string(client_id) +
                                    " from group " + group_name);
        }
    }

    void broadcastToGroup(const std::string& group_name,
                          const Message& message) {
        std::vector<size_t> client_ids;
        {
            std::lock_guard<std::mutex> lock(group_mutex_);
            auto it = groups_.find(group_name);
            if (it != groups_.end()) {
                client_ids.assign(it->second.begin(), it->second.end());
            }
        }

        for (size_t client_id : client_ids) {
            sendMessageToClient(client_id, message);
        }

        log(LogLevel::DEBUG, "Broadcasted message to group " + group_name +
                                 " (" + std::to_string(client_ids.size()) +
                                 " clients)");
    }

    void setAuthenticator(
        const std::function<bool(const std::string&, const std::string&)>&
            authenticator) {
        authenticator_ = authenticator;
        log(LogLevel::INFO, "Custom authenticator set");
    }

    void requireAuthentication(bool require) {
        require_authentication_ = require;
        log(LogLevel::INFO, "Authentication requirement set to: " +
                                std::string(require ? "true" : "false"));
    }

    void setClientMetadata(size_t client_id, const std::string& key,
                           const std::string& value) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (client) {
            client->setMetadata(key, value);
            log(LogLevel::DEBUG, "Set metadata '" + key + "' for client " +
                                     std::to_string(client_id));
        }
    }

    std::string getClientMetadata(size_t client_id, const std::string& key) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
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
        std::lock_guard<std::mutex> lock(client_mutex_);
        return clients_.find(client_id) != clients_.end();
    }

    std::vector<size_t> getConnectedClients() const {
        std::vector<size_t> result;
        std::lock_guard<std::mutex> lock(client_mutex_);
        result.reserve(clients_.size());
        for (const auto& [id, _] : clients_) {
            result.push_back(id);
        }
        return result;
    }

    std::vector<std::string> getGroups() const {
        std::vector<std::string> result;
        std::lock_guard<std::mutex> lock(group_mutex_);
        result.reserve(groups_.size());
        for (const auto& [name, _] : groups_) {
            result.push_back(name);
        }
        return result;
    }

    std::vector<size_t> getClientsInGroup(const std::string& group_name) const {
        std::vector<size_t> result;
        std::lock_guard<std::mutex> lock(group_mutex_);
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

            log(LogLevel::INFO, "SSL configured successfully");
        } catch (const std::exception& e) {
            log(LogLevel::ERROR,
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
                        log(LogLevel::WARNING,
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
                        log(LogLevel::WARNING,
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
                                    log(LogLevel::ERROR,
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
                std::lock_guard<std::mutex> lock(client_mutex_);
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
                        log(LogLevel::WARNING,
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

            log(LogLevel::INFO,
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
                std::lock_guard<std::mutex> lock(client_mutex_);
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
                        log(LogLevel::WARNING,
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
            log(LogLevel::INFO,
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
            std::lock_guard<std::mutex> lock(handler_mutex_);
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
            std::lock_guard<std::mutex> lock(connect_handler_mutex_);
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
            std::lock_guard<std::mutex> lock(disconnect_handler_mutex_);
            handlers_copy = disconnect_handlers_;
        }

        for (const auto& handler : handlers_copy) {
            task_queue_.enqueue(
                [handler, client_id, reason]() { handler(client_id, reason); });
        }
    }

    void handleError(const std::string& error_message, size_t client_id) {
        log(LogLevel::ERROR,
            error_message + " (client: " + std::to_string(client_id) + ")");

        std::vector<std::function<void(const std::string&, size_t)>>
            handlers_copy;
        {
            std::lock_guard<std::mutex> lock(error_handler_mutex_);
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
            std::lock_guard<std::mutex> lock(client_mutex_);
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
            std::lock_guard<std::mutex> lock(client_mutex_);
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
                case LogLevel::DEBUG:
                    level_str = "DEBUG";
                    break;
                case LogLevel::INFO:
                    level_str = "INFO";
                    break;
                case LogLevel::WARNING:
                    level_str = "WARNING";
                    break;
                case LogLevel::ERROR:
                    level_str = "ERROR";
                    break;
                case LogLevel::FATAL:
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
            std::lock_guard<std::mutex> lock(client_mutex_);
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
            log(LogLevel::INFO, "Disconnected " +
                                    std::to_string(timeout_clients.size()) +
                                    " clients due to timeout");
        }
    }

    SocketHubConfig config_;
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    asio::ssl::context ssl_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    bool is_running_;
    std::unordered_map<size_t, std::shared_ptr<Client>> clients_;
    mutable std::mutex client_mutex_;
    std::vector<std::function<void(const Message&, size_t)>> message_handlers_;
    std::mutex handler_mutex_;
    std::vector<std::function<void(size_t, const std::string&)>>
        connect_handlers_;
    std::mutex connect_handler_mutex_;
    std::vector<std::function<void(size_t, const std::string&)>>
        disconnect_handlers_;
    std::mutex disconnect_handler_mutex_;
    std::vector<std::function<void(const std::string&, size_t)>>
        error_handlers_;
    std::mutex error_handler_mutex_;
    size_t next_client_id_;
    std::thread io_thread_;
    std::unordered_map<std::string, std::unordered_set<size_t>> groups_;
    mutable std::mutex group_mutex_;
    RateLimiter rate_limiter_;
    TaskQueue task_queue_;
    std::function<bool(const std::string&, const std::string&)> authenticator_;
    bool require_authentication_;
    bool logging_enabled_ = true;
    LogLevel log_level_ = LogLevel::INFO;
    std::function<void(LogLevel, const std::string&)> log_handler_;
    SocketHubStats stats_;
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
