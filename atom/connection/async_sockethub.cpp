#include "async_sockethub.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atom::async::connection {

// Client class to manage individual connections
class Client {
public:
  Client(size_t id, std::shared_ptr<asio::ip::tcp::socket> socket)
      : id_(id), socket_(std::move(socket)), is_authenticated_(false),
        connect_time_(std::chrono::system_clock::now()),
        last_activity_time_(connect_time_) {}

  // SSL version constructor
  Client(size_t id,
         std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket)
      : id_(id), ssl_socket_(std::move(ssl_socket)), is_authenticated_(false),
        connect_time_(std::chrono::system_clock::now()),
        last_activity_time_(connect_time_) {}

  auto getId() const -> size_t { return id_; }

  auto isAuthenticated() const -> bool { return is_authenticated_; }
  void setAuthenticated(bool auth) { is_authenticated_ = auth; }

  void setMetadata(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    metadata_[std::string(key)] = value;
  }

  auto getMetadata(std::string_view key) const -> std::string {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    if (auto it = metadata_.find(std::string(key)); it != metadata_.end()) {
      return it->second;
    }
    return "";
  }

  auto getRemoteAddress() const -> std::string {
    try {
      if (socket_) {
        return socket_->remote_endpoint().address().to_string();
      } else if (ssl_socket_) {
        return ssl_socket_->lowest_layer().remote_endpoint().address().to_string();
      }
    } catch (const std::exception &e) {
      spdlog::warn("Could not get remote address for client {}: {}", id_,
                   e.what());
    }
    return "unknown";
  }

  auto getConnectTime() const -> std::chrono::system_clock::time_point {
    return connect_time_;
  }

  auto getLastActivityTime() const -> std::chrono::system_clock::time_point {
    return last_activity_time_;
  }

  void updateLastActivity() {
    last_activity_time_ = std::chrono::system_clock::now();
  }

  void send(const Message &message,
            const std::function<void(bool success)> &callback = nullptr) {
    if (socket_) {
      sendViaTcp(message, callback);
    } else if (ssl_socket_) {
      sendViaSsl(message, callback);
    }
  }

  void startReading(const std::function<void(const Message &)> &message_handler,
                    const std::function<void()> &disconnect_handler) {
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
      asio::error_code ec; // Added error code for close
      if (socket_ && socket_->is_open()) {
        [[maybe_unused]] auto close_result = socket_->close(ec); // Check return value
        if (ec) {
            spdlog::error("Error closing TCP socket for client {}: {}", id_, ec.message());
        }
      } else if (ssl_socket_ && ssl_socket_->lowest_layer().is_open()) {
        [[maybe_unused]] auto close_result = ssl_socket_->lowest_layer().close(ec); // Check return value
         if (ec) {
            spdlog::error("Error closing SSL socket for client {}: {}", id_, ec.message());
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("Error during disconnect for client {}: {}", id_, e.what());
    }
  }

  // Statistics
  auto getMessagesSent() const -> size_t { return messages_sent_; }
  auto getMessagesReceived() const -> size_t { return messages_received_; }
  auto getBytesSent() const -> size_t { return bytes_sent_; }
  auto getBytesReceived() const -> size_t { return bytes_received_; }

private:
  void doReadTcp() {
    auto buffer = std::make_shared<std::vector<char>>(4096);
    socket_->async_read_some(
        asio::buffer(*buffer),
        [this, buffer](const asio::error_code &ec, std::size_t length) {
          if (!ec) {
            bytes_received_ += length;
            messages_received_++;
            updateLastActivity();

            Message msg{Message::Type::TEXT, {buffer->begin(), buffer->begin() + length},
                        id_};

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
        [this, buffer](const asio::error_code &ec, std::size_t length) {
          if (!ec) {
            bytes_received_ += length;
            messages_received_++;
            updateLastActivity();

            Message msg{Message::Type::TEXT, {buffer->begin(), buffer->begin() + length},
                        id_};

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

  void sendViaTcp(const Message &message,
                  const std::function<void(bool)> &callback) {
    bytes_sent_ += message.data.size();
    messages_sent_++;
    updateLastActivity();

    asio::async_write(*socket_, asio::buffer(message.data),
                      [this, callback](const asio::error_code &ec, std::size_t) {
                        if (callback) {
                          callback(!ec);
                        }
                      });
  }

  void sendViaSsl(const Message &message,
                  const std::function<void(bool)> &callback) {
    bytes_sent_ += message.data.size();
    messages_sent_++;
    updateLastActivity();

    asio::async_write(*ssl_socket_, asio::buffer(message.data),
                      [this, callback](const asio::error_code &ec, std::size_t) {
                        if (callback) {
                          callback(!ec);
                        }
                      });
  }

  size_t id_;
  std::shared_ptr<asio::ip::tcp::socket> socket_;
  std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket_;
  std::atomic<bool> is_authenticated_;
  std::function<void(const Message &)> message_handler_;
  std::function<void()> disconnect_handler_;
  std::chrono::system_clock::time_point connect_time_;
  std::atomic<std::chrono::system_clock::time_point> last_activity_time_;
  std::atomic<size_t> messages_sent_{0};
  std::atomic<size_t> messages_received_{0};
  std::atomic<size_t> bytes_sent_{0};
  std::atomic<size_t> bytes_received_{0};
  std::unordered_map<std::string, std::string> metadata_;
  mutable std::mutex metadata_mutex_;
};

// Rate limiter for DoS protection
class RateLimiter {
public:
  RateLimiter(int max_connections_per_ip, int max_messages_per_minute)
      : max_connections_per_ip_(max_connections_per_ip),
        max_messages_per_minute_(max_messages_per_minute) {}

  auto canConnect(std::string_view ip_address) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto &count = connection_count_[std::string(ip_address)];
    if (count >= max_connections_per_ip_) {
      return false;
    }

    count++;
    return true;
  }

  void releaseConnection(std::string_view ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (auto it = connection_count_.find(std::string(ip_address));
        it != connection_count_.end() && it->second > 0) {
      it->second--;
    }
  }

  auto canSendMessage(std::string_view ip_address) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto &message_times = message_history_[std::string(ip_address)];

    // Remove messages older than 1 minute
    auto minute_ago = now - std::chrono::minutes(1);
    [[maybe_unused]] auto erase_result = message_times.erase(
        std::remove_if(message_times.begin(), message_times.end(),
                       [&minute_ago](const auto &time) { return time < minute_ago; }),
        message_times.end());

    if (message_times.size() >= static_cast<size_t>(max_messages_per_minute_)) {
      return false;
    }

    message_times.push_back(now);
    return true;
  }

private:
  int max_connections_per_ip_;
  int max_messages_per_minute_;
  std::unordered_map<std::string, int> connection_count_;
  std::unordered_map<std::string, std::vector<std::chrono::system_clock::time_point>>
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
            condition_.wait(lock,
                          [this] { return !running_ || !tasks_.empty(); });

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

    for (auto &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  template <class F>
  void enqueue(F &&task) {
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
  std::atomic<bool> running_;
};

// Enhanced implementation of SocketHub
class SocketHub::Impl {
public:
  Impl(const SocketHubConfig &config)
      : config_(config),
        io_context_(std::make_unique<asio::io_context>()), // Use unique_ptr
        acceptor_(*io_context_), ssl_context_(asio::ssl::context::sslv23),
        work_guard_(std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(asio::make_work_guard(*io_context_))), // Use unique_ptr and make_work_guard
        is_running_(false), next_client_id_(1),
        rate_limiter_(config.max_connections_per_ip,
                      config.max_messages_per_minute),
        task_queue_(4), require_authentication_(false), stats_() {
    if (config.use_ssl) {
      configureSSL();
    }

    // Start statistics timer
    startStatsTimer();
  }

  ~Impl() { stop(); }

  void start(uint16_t port) {
    try {
      asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
      acceptor_.open(endpoint.protocol());
      acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
      acceptor_.bind(endpoint);
      acceptor_.listen(config_.backlog_size);

      is_running_ = true;
      doAccept();

      if (!io_thread_.joinable()) {
        io_thread_ = std::thread([this]() { io_context_->run(); }); // Use ->
      }

      spdlog::info("SocketHub started on port {}", port);
      stats_.start_time = std::chrono::system_clock::now();

    } catch (const std::exception &e) {
      spdlog::error("Failed to start SocketHub: {}", e.what());
      throw;
    }
  }

  void stop() {
    if (is_running_) {
      is_running_ = false;

      // Cancel the acceptor
      asio::error_code ec;
      [[maybe_unused]] auto cancel_result = acceptor_.cancel(ec);
      // Intentionally not checking return value as this is during shutdown

      // Stop the work guard to allow io_context to stop
      work_guard_.reset(); // Reset the unique_ptr

      // Disconnect all clients
      disconnectAllClients("Server shutting down");

      // Stop the io_context
      io_context_->stop(); // Use ->

      // Join the thread
      if (io_thread_.joinable()) {
        io_thread_.join();
      }

      spdlog::info("SocketHub stopped.");
    }
  }

  void restart() {
    uint16_t port = 0;
    try {
      port = acceptor_.local_endpoint().port();
    } catch (...) {
      spdlog::error("Could not determine port for restart");
      return;
    }

    stop();

    // Reset the io_context and work_guard
    io_context_ = std::make_unique<asio::io_context>(); // Re-create io_context
    work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(asio::make_work_guard(*io_context_)); // Re-create work_guard

    // Re-open and bind acceptor to the new io_context
    acceptor_.close(); // Close the old acceptor associated with the old io_context
    new (&acceptor_) asio::ip::tcp::acceptor(*io_context_); // Placement new to re-initialize acceptor

    // Start again
    start(port);
  }

  void addMessageHandler(const MessageHandler &handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    message_handlers_.push_back(handler);
  }

  void addConnectHandler(const ConnectHandler &handler) {
    std::lock_guard<std::mutex> lock(connect_handler_mutex_);
    connect_handlers_.push_back(handler);
  }

  void addDisconnectHandler(const DisconnectHandler &handler) {
    std::lock_guard<std::mutex> lock(disconnect_handler_mutex_);
    disconnect_handlers_.push_back(handler);
  }

  void addErrorHandler(const ErrorHandler &handler) {
    std::lock_guard<std::mutex> lock(error_handler_mutex_);
    error_handlers_.push_back(handler);
  }

  void broadcastMessage(const Message &message) {
    std::vector<std::shared_ptr<Client>> client_copies;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      for (const auto &[id, client] : clients_) {
        client_copies.push_back(client);
      }
    }

    for (const auto &client : client_copies) {
      client->send(message);
    }

    stats_.messages_sent += client_copies.size();
    stats_.bytes_sent += message.data.size() * client_copies.size();

    spdlog::debug("Broadcasted message of {} bytes to {} clients",
                  message.data.size(), client_copies.size());
  }

  void sendMessageToClient(size_t client_id, const Message &message) {
    std::shared_ptr<Client> client;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (auto it = clients_.find(client_id); it != clients_.end()) {
        client = it->second;
      }
    }

    if (client) {
      client->send(message, [this, client_id](bool success) {
        if (!success) {
          this->handleError("Failed to send message to client", client_id);
        }
      });

      stats_.messages_sent++;
      stats_.bytes_sent += message.data.size();

      spdlog::debug("Sent message of {} bytes to client {}", message.data.size(),
                    client_id);
    } else {
      spdlog::warn("Attempted to send message to non-existent client: {}",
                   client_id);
    }
  }

  void disconnectClient(size_t client_id, std::string_view reason) {
    std::shared_ptr<Client> client;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (auto it = clients_.find(client_id); it != clients_.end()) {
        client = it->second;
        clients_.erase(it);

        // Remove from all groups
        for (auto &[group_name, clients] : groups_) {
          [[maybe_unused]] auto erase_count = clients.erase(client_id);
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

      spdlog::info("Client {} disconnected. Reason: {}", client_id, reason);
    }
  }

  void createGroup(std::string_view group_name) {
    std::lock_guard<std::mutex> lock(group_mutex_);
    groups_[std::string(group_name)];
    spdlog::info("Created group: {}", group_name);
  }

  void addClientToGroup(size_t client_id, std::string_view group_name) {
    bool client_exists = false;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      client_exists = clients_.count(client_id) > 0;
    }

    if (!client_exists) {
      spdlog::warn("Cannot add non-existent client {} to group {}", client_id,
                   group_name);
      return;
    }

    std::lock_guard<std::mutex> lock(group_mutex_);
    if (auto it = groups_.find(std::string(group_name)); it == groups_.end()) {
      // Create the group if it doesn't exist
      groups_[std::string(group_name)] = {client_id};
      spdlog::info("Created group {} and added client {}", group_name, client_id);
    } else {
      it->second.insert(client_id);
      spdlog::info("Added client {} to group {}", client_id, group_name);
    }
  }

  void removeClientFromGroup(size_t client_id, std::string_view group_name) {
    std::lock_guard<std::mutex> lock(group_mutex_);
    if (auto it = groups_.find(std::string(group_name)); it != groups_.end()) {
      [[maybe_unused]] auto erase_count = it->second.erase(client_id);
      spdlog::info("Removed client {} from group {}", client_id, group_name);
    }
  }

  void broadcastToGroup(std::string_view group_name, const Message &message) {
    std::vector<size_t> client_ids;
    {
      std::lock_guard<std::mutex> lock(group_mutex_);
      if (auto it = groups_.find(std::string(group_name)); it != groups_.end()) {
        client_ids.assign(it->second.begin(), it->second.end());
      }
    }

    for (size_t client_id : client_ids) {
      sendMessageToClient(client_id, message);
    }

    spdlog::debug("Broadcasted message to group {} ({} clients)", group_name,
                  client_ids.size());
  }

  void setAuthenticator(const Authenticator &authenticator) {
    authenticator_ = authenticator;
    spdlog::info("Custom authenticator set");
  }

  void requireAuthentication(bool require) {
    require_authentication_ = require;
    spdlog::info("Authentication requirement set to: {}", require);
  }

  void setClientMetadata(size_t client_id, std::string_view key,
                         std::string_view value) {
    std::shared_ptr<Client> client;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (auto it = clients_.find(client_id); it != clients_.end()) {
        client = it->second;
      }
    }

    if (client) {
      client->setMetadata(key, value);
      spdlog::debug("Set metadata '{}' for client {}", key, client_id);
    }
  }

  auto getClientMetadata(size_t client_id, std::string_view key)
      -> std::string {
    std::shared_ptr<Client> client;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (auto it = clients_.find(client_id); it != clients_.end()) {
        client = it->second;
      }
    }

    if (client) {
      return client->getMetadata(key);
    }
    return "";
  }

  auto getStatistics() const -> SocketHubStats {
    SocketHubStats current_stats;
    // Explicitly load atomic values
    current_stats.total_connections = stats_.total_connections.load();
    current_stats.active_connections = stats_.active_connections.load();
    current_stats.messages_received = stats_.messages_received.load();
    current_stats.messages_sent = stats_.messages_sent.load();
    current_stats.bytes_received = stats_.bytes_received.load();
    current_stats.bytes_sent = stats_.bytes_sent.load();
    current_stats.start_time = stats_.start_time; // Not atomic, can be copied
    return current_stats;
  }

  auto isRunning() const -> bool { return is_running_; }

  auto isClientConnected(size_t client_id) const -> bool {
    std::lock_guard<std::mutex> lock(client_mutex_);
    return clients_.count(client_id) > 0;
  }

  auto getConnectedClients() const -> std::vector<size_t> {
    std::vector<size_t> result;
    std::lock_guard<std::mutex> lock(client_mutex_);
    result.reserve(clients_.size());
    for (const auto &[id, _] : clients_) {
      result.push_back(id);
    }
    return result;
  }

  auto getGroups() const -> std::vector<std::string> {
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(group_mutex_);
    result.reserve(groups_.size());
    for (const auto &[name, _] : groups_) {
      result.push_back(name);
    }
    return result;
  }

  auto getClientsInGroup(std::string_view group_name) const
      -> std::vector<size_t> {
    std::vector<size_t> result;
    std::lock_guard<std::mutex> lock(group_mutex_);
    if (auto it = groups_.find(std::string(group_name)); it != groups_.end()) {
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

      spdlog::info("SSL configured successfully");
    } catch (const std::exception &e) {
      spdlog::error("SSL configuration error: {}", e.what());
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
    auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context_); // Use ->

    acceptor_.async_accept(*socket, [this, socket](const asio::error_code &ec) {
      if (!ec) {
        std::string remote_address = "unknown";
        try {
          remote_address = socket->remote_endpoint().address().to_string();

          // Apply rate limiting if enabled
          if (config_.enable_rate_limiting &&
              !rate_limiter_.canConnect(remote_address)) {
            spdlog::warn("Rate limit exceeded for IP: {}", remote_address);
            socket->close();
          } else {
            handleNewTcpConnection(socket);
          }
        } catch (const std::exception &e) {
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
    auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context_); // Use ->

    acceptor_.async_accept(*socket, [this, socket](const asio::error_code &ec) {
      if (!ec) {
        std::string remote_address = "unknown";
        try {
          remote_address = socket->remote_endpoint().address().to_string();

          // Apply rate limiting if enabled
          if (config_.enable_rate_limiting &&
              !rate_limiter_.canConnect(remote_address)) {
            spdlog::warn("Rate limit exceeded for IP: {}", remote_address);
            socket->close();
          } else {
            auto ssl_socket =
                std::make_shared<asio::ssl::stream<asio::ip::tcp::socket>>(
                    std::move(*socket), ssl_context_);

            // Perform SSL handshake
            ssl_socket->async_handshake(
                asio::ssl::stream_base::server,
                [this, ssl_socket,
                 remote_address](const asio::error_code &handshake_ec) {
                  if (!handshake_ec) {
                    handleNewSslConnection(ssl_socket);
                  } else {
                    spdlog::error("SSL handshake failed: {} from {}",
                                  handshake_ec.message(), remote_address);
                    try {
                      asio::error_code close_ec; // Added error code for close
                      [[maybe_unused]] auto close_result = ssl_socket->lowest_layer().close(close_ec); // Check return value
                       if (close_ec) {
                            spdlog::error("Error closing socket after SSL handshake failure: {}", close_ec.message());
                       }
                    } catch (...) {
                    }
                  }
                });
          }
        } catch (const std::exception &e) {
          handleError("SSL accept error: " + std::string(e.what()), 0);
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
          [this, client_id](const Message &message) {
            // Check rate limiting for messages
            std::string client_ip = this->getClientIp(client_id);
            if (config_.enable_rate_limiting &&
                !rate_limiter_.canSendMessage(client_ip)) {
              spdlog::warn("Message rate limit exceeded for client {} ({})",
                           client_id, client_ip);
              return;
            }

            stats_.messages_received++;
            stats_.bytes_received += message.data.size();

            // Forward message to all registered handlers
            this->notifyMessageHandlers(message, client_id);
          },
          [this, client_id]() {
            // Handle disconnection
            this->disconnectClient(client_id, "Connection closed by client");
          });

      // Set TCP keep-alive if configured
      if (config_.keep_alive) {
        asio::error_code ec; // Added error code
        [[maybe_unused]] auto keep_alive_result = socket->set_option(asio::socket_base::keep_alive(true), ec); // Check return value
        if (ec) {
            spdlog::warn("Failed to set keep-alive for client {}: {}", client_id, ec.message());
        }
      }

      // Notify connect handlers
      notifyConnect(client_id, remote_address);

      spdlog::info("New client connected: {} from {}", client_id, remote_address);

    } catch (const std::exception &e) {
      handleError("Error handling new connection: " + std::string(e.what()),
                  0);
    }
  }

  void handleNewSslConnection(
      std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssl_socket) {
    try {
      std::string remote_address =
          ssl_socket->lowest_layer().remote_endpoint().address().to_string();
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
          [this, client_id](const Message &message) {
            std::string client_ip = this->getClientIp(client_id);
            if (config_.enable_rate_limiting &&
                !rate_limiter_.canSendMessage(client_ip)) {
              spdlog::warn("Message rate limit exceeded for client {} ({})",
                           client_id, client_ip);
              return;
            }

            stats_.messages_received++;
            stats_.bytes_received += message.data.size();
            this->notifyMessageHandlers(message, client_id);
          },
          [this, client_id]() {
            this->disconnectClient(client_id, "Connection closed by client");
          });

      // Set TCP keep-alive if configured
      if (config_.keep_alive) {
        asio::error_code ec; // Added error code
        [[maybe_unused]] auto keep_alive_result = ssl_socket->lowest_layer().set_option(
            asio::socket_base::keep_alive(true), ec); // Check return value
         if (ec) {
            spdlog::warn("Failed to set keep-alive for SSL client {}: {}", client_id, ec.message());
        }
      }

      notifyConnect(client_id, remote_address);
      spdlog::info("New SSL client connected: {} from {}", client_id,
                   remote_address);

    } catch (const std::exception &e) {
      handleError("Error handling new SSL connection: " + std::string(e.what()),
                  0);
    }
  }

  void notifyMessageHandlers(const Message &message, size_t client_id) {
    // Copy the handlers to avoid holding the lock during callback execution
    std::vector<MessageHandler> handlers_copy;
    {
      std::lock_guard<std::mutex> lock(handler_mutex_);
      handlers_copy = message_handlers_;
    }

    // Process message asynchronously in task queue
    for (const auto &handler : handlers_copy) {
      task_queue_.enqueue(
          [handler, message, client_id]() { handler(message, client_id); });
    }
  }

  void notifyConnect(size_t client_id, std::string_view address) {
    std::vector<ConnectHandler> handlers_copy;
    {
      std::lock_guard<std::mutex> lock(connect_handler_mutex_);
      handlers_copy = connect_handlers_;
    }

    for (const auto &handler : handlers_copy) {
      task_queue_.enqueue(
          [handler, client_id, address]() { handler(client_id, address); });
    }
  }

  void notifyDisconnect(size_t client_id, std::string_view reason) {
    std::vector<DisconnectHandler> handlers_copy;
    {
      std::lock_guard<std::mutex> lock(disconnect_handler_mutex_);
      handlers_copy = disconnect_handlers_;
    }

    for (const auto &handler : handlers_copy) {
      task_queue_.enqueue(
          [handler, client_id, reason]() { handler(client_id, reason); });
    }
  }

  void handleError(const std::string &error_message, size_t client_id) {
    spdlog::error("{} (client: {})", error_message, client_id);

    std::vector<ErrorHandler> handlers_copy;
    {
      std::lock_guard<std::mutex> lock(error_handler_mutex_);
      handlers_copy = error_handlers_;
    }

    for (const auto &handler : handlers_copy) {
      task_queue_.enqueue([handler, error_message, client_id]() {
        handler(error_message, client_id);
      });
    }
  }

  void disconnectAllClients(std::string_view reason) {
    std::vector<size_t> client_ids;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      client_ids.reserve(clients_.size());
      for (const auto &[id, _] : clients_) {
        client_ids.push_back(id);
      }
    }

    for (size_t id : client_ids) {
      disconnectClient(id, reason);
    }
  }

  auto getClientIp(size_t client_id) -> std::string {
    std::shared_ptr<Client> client;
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (auto it = clients_.find(client_id); it != clients_.end()) {
        client = it->second;
      }
    }

    if (client) {
      return client->getRemoteAddress();
    }
    return "unknown";
  }

  void startStatsTimer() {
    auto timer =
        std::make_shared<asio::steady_timer>(*io_context_, std::chrono::seconds(60)); // Use ->
    timer->async_wait([this, timer](const asio::error_code &ec) {
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
    if (config_.connection_timeout.count() == 0) {
      return; // Timeout disabled
    }

    std::vector<size_t> timeout_clients;
    auto now = std::chrono::system_clock::now();

    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      for (const auto &[id, client] : clients_) {
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
      spdlog::info("Disconnected {} clients due to timeout",
                   timeout_clients.size());
    }
  }

  SocketHubConfig config_;
  std::unique_ptr<asio::io_context> io_context_; // Use unique_ptr
  asio::ip::tcp::acceptor acceptor_; // Acceptor can be re-initialized with placement new
  asio::ssl::context ssl_context_;
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; // Use unique_ptr
  std::atomic<bool> is_running_;
  std::unordered_map<size_t, std::shared_ptr<Client>> clients_;
  mutable std::mutex client_mutex_;
  std::vector<MessageHandler> message_handlers_;
  std::mutex handler_mutex_;
  std::vector<ConnectHandler> connect_handlers_;
  std::mutex connect_handler_mutex_;
  std::vector<DisconnectHandler> disconnect_handlers_;
  std::mutex disconnect_handler_mutex_;
  std::vector<ErrorHandler> error_handlers_;
  std::mutex error_handler_mutex_;
  std::atomic<size_t> next_client_id_;
  std::thread io_thread_;
  std::unordered_map<std::string, std::unordered_set<size_t>> groups_;
  mutable std::mutex group_mutex_;
  RateLimiter rate_limiter_;
  TaskQueue task_queue_;
  Authenticator authenticator_;
  std::atomic<bool> require_authentication_;
  SocketHubStats stats_;
};

// SocketHub implementation forwarding to Impl
SocketHub::SocketHub(const SocketHubConfig &config)
    : pimpl_(std::make_unique<Impl>(config)) {}

SocketHub::~SocketHub() = default;

SocketHub::SocketHub(SocketHub &&other) noexcept = default;
auto SocketHub::operator=(SocketHub &&other) noexcept -> SocketHub & = default;

void SocketHub::start(uint16_t port) { pimpl_->start(port); }

void SocketHub::stop() { pimpl_->stop(); }

void SocketHub::restart() { pimpl_->restart(); }

void SocketHub::addMessageHandler(MessageHandler handler) {
  pimpl_->addMessageHandler(std::move(handler));
}

void SocketHub::addConnectHandler(ConnectHandler handler) {
  pimpl_->addConnectHandler(std::move(handler));
}

void SocketHub::addDisconnectHandler(DisconnectHandler handler) {
  pimpl_->addDisconnectHandler(std::move(handler));
}

void SocketHub::addErrorHandler(ErrorHandler handler) {
  pimpl_->addErrorHandler(std::move(handler));
}

void SocketHub::broadcastMessage(const Message &message) {
  pimpl_->broadcastMessage(message);
}

void SocketHub::sendMessageToClient(size_t client_id, const Message &message) {
  pimpl_->sendMessageToClient(client_id, message);
}

void SocketHub::disconnectClient(size_t client_id, std::string_view reason) {
  pimpl_->disconnectClient(client_id, reason);
}

void SocketHub::createGroup(std::string_view group_name) {
  pimpl_->createGroup(group_name);
}

void SocketHub::addClientToGroup(size_t client_id, std::string_view group_name) {
  pimpl_->addClientToGroup(client_id, group_name);
}

void SocketHub::removeClientFromGroup(size_t client_id,
                                    std::string_view group_name) {
  pimpl_->removeClientFromGroup(client_id, group_name);
}

void SocketHub::broadcastToGroup(std::string_view group_name,
                               const Message &message) {
  pimpl_->broadcastToGroup(group_name, message);
}

void SocketHub::setAuthenticator(Authenticator authenticator) {
  pimpl_->setAuthenticator(std::move(authenticator));
}

void SocketHub::requireAuthentication(bool require) {
  pimpl_->requireAuthentication(require);
}

void SocketHub::setClientMetadata(size_t client_id, std::string_view key,
                                std::string_view value) {
  pimpl_->setClientMetadata(client_id, key, value);
}

auto SocketHub::getClientMetadata(size_t client_id, std::string_view key)
    -> std::string {
  return pimpl_->getClientMetadata(client_id, key);
}

auto SocketHub::getStatistics() const -> SocketHubStats {
  return pimpl_->getStatistics();
}

auto SocketHub::isRunning() const -> bool { return pimpl_->isRunning(); }

auto SocketHub::isClientConnected(size_t client_id) const -> bool {
  return pimpl_->isClientConnected(client_id);
}

auto SocketHub::getConnectedClients() const -> std::vector<size_t> {
  return pimpl_->getConnectedClients();
}

auto SocketHub::getGroups() const -> std::vector<std::string> {
  return pimpl_->getGroups();
}

auto SocketHub::getClientsInGroup(std::string_view group_name) const
    -> std::vector<size_t> {
  return pimpl_->getClientsInGroup(group_name);
}

} // namespace atom::async::connection
