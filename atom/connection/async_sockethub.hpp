#ifndef ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP
#define ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace atom::async::connection {

/**
 * @brief Configuration for the SocketHub.
 */
struct SocketHubConfig {
  bool use_ssl = false;
  int backlog_size = 128;
  std::chrono::seconds connection_timeout{30};
  bool keep_alive = true;
  std::string ssl_cert_file;
  std::string ssl_key_file;
  std::string ssl_dh_file;
  std::string ssl_password;
  bool enable_rate_limiting = false;
  int max_connections_per_ip = 10;
  int max_messages_per_minute = 100;
};

/**
 * @brief Represents a message for data exchange.
 */
struct Message {
  enum class Type { TEXT, BINARY, PING, PONG, CLOSE };

  Type type = Type::TEXT;
  std::vector<char> data;
  size_t sender_id = 0;

  /**
   * @brief Creates a text message.
   * @param text The text content.
   * @param sender The ID of the sender.
   * @return A new Message object.
   */
  static auto createText(std::string_view text, size_t sender = 0) -> Message {
    return {Type::TEXT, {text.begin(), text.end()}, sender};
  }

  /**
   * @brief Creates a binary message.
   * @param binary_data The binary data.
   * @param sender The ID of the sender.
   * @return A new Message object.
   */
  static auto createBinary(const std::vector<char> &binary_data,
                           size_t sender = 0) -> Message {
    return {Type::BINARY, binary_data, sender};
  }

  /**
   * @brief Returns the message data as a string.
   * @return The string representation of the data.
   */
  [[nodiscard]] auto asString() const -> std::string {
    return {data.begin(), data.end()};
  }
};

/**
 * @brief Statistics for monitoring the SocketHub.
 */
struct SocketHubStats {
  std::atomic<size_t> total_connections = 0;
  std::atomic<size_t> active_connections = 0;
  std::atomic<size_t> messages_received = 0;
  std::atomic<size_t> messages_sent = 0;
  std::atomic<size_t> bytes_received = 0;
  std::atomic<size_t> bytes_sent = 0;
  std::chrono::system_clock::time_point start_time;

  // Default constructor
  SocketHubStats() : start_time(std::chrono::system_clock::now()) {}

  // Explicitly define copy constructor to handle atomic members
  SocketHubStats(const SocketHubStats& other)
      : total_connections(other.total_connections.load()),
        active_connections(other.active_connections.load()),
        messages_received(other.messages_received.load()),
        messages_sent(other.messages_sent.load()),
        bytes_received(other.bytes_received.load()),
        bytes_sent(other.bytes_sent.load()),
        start_time(other.start_time) {}

  // Explicitly define copy assignment operator to handle atomic members
  SocketHubStats& operator=(const SocketHubStats& other) {
      if (this != &other) {
          total_connections.store(other.total_connections.load());
          active_connections.store(other.active_connections.load());
          messages_received.store(other.messages_received.load());
          messages_sent.store(other.messages_sent.load());
          bytes_received.store(other.bytes_received.load());
          bytes_sent.store(other.bytes_sent.load());
          start_time = other.start_time;
      }
      return *this;
  }
};

/**
 * @brief A high-performance, scalable, and thread-safe hub for managing TCP/SSL
 * socket connections.
 *
 * SocketHub provides a robust framework for building networked applications,
 * featuring asynchronous I/O, SSL/TLS encryption, client management, message
 * broadcasting, and more, all built on modern C++ and Asio.
 */
class SocketHub {
public:
  /**
   * @brief Constructs a SocketHub with the given configuration.
   * @param config The configuration settings for the hub.
   */
  explicit SocketHub(const SocketHubConfig &config = {});
  ~SocketHub();

  SocketHub(const SocketHub &) = delete;
  auto operator=(const SocketHub &) -> SocketHub & = delete;
  SocketHub(SocketHub &&) noexcept;
  auto operator=(SocketHub &&) noexcept -> SocketHub &;

  /**
   * @brief Starts the server and begins listening on the specified port.
   * @param port The port number to listen on.
   * @throws std::runtime_error on failure to start.
   */
  void start(uint16_t port);

  /**
   * @brief Stops the server and disconnects all clients.
   */
  void stop();

  /**
   * @brief Restarts the server.
   */
  void restart();

  // Handler registration
  using MessageHandler = std::function<void(const Message &, size_t)>;
  using ConnectHandler = std::function<void(size_t, std::string_view)>;
  using DisconnectHandler = std::function<void(size_t, std::string_view)>;
  using ErrorHandler = std::function<void(const std::string &, size_t)>;

  /**
   * @brief Registers a handler for incoming messages.
   * @param handler The function to call when a message is received.
   */
  void addMessageHandler(MessageHandler handler);

  /**
   * @brief Registers a handler for new client connections.
   * @param handler The function to call when a client connects.
   */
  void addConnectHandler(ConnectHandler handler);

  /**
   * @brief Registers a handler for client disconnections.
   * @param handler The function to call when a client disconnects.
   */
  void addDisconnectHandler(DisconnectHandler handler);

  /**
   * @brief Registers a handler for errors.
   * @param handler The function to call when an error occurs.
   */
  void addErrorHandler(ErrorHandler handler);

  // Client interaction
  /**
   * @brief Broadcasts a message to all connected clients.
   * @param message The message to send.
   */
  void broadcastMessage(const Message &message);

  /**
   * @brief Sends a message to a specific client.
   * @param client_id The ID of the target client.
   * @param message The message to send.
   */
  void sendMessageToClient(size_t client_id, const Message &message);

  /**
   * @brief Disconnects a specific client.
   * @param client_id The ID of the client to disconnect.
   * @param reason An optional reason for the disconnection.
   */
  void disconnectClient(size_t client_id, std::string_view reason = "");

  // Group management
  /**
   * @brief Creates a new client group.
   * @param group_name The name of the group to create.
   */
  void createGroup(std::string_view group_name);

  /**
   * @brief Adds a client to a group.
   * @param client_id The ID of the client.
   * @param group_name The name of the group.
   */
  void addClientToGroup(size_t client_id, std::string_view group_name);

  /**
   * @brief Removes a client from a group.
   * @param client_id The ID of the client.
   * @param group_name The name of the group.
   */
  void removeClientFromGroup(size_t client_id, std::string_view group_name);

  /**
   * @brief Broadcasts a message to all clients in a specific group.
   * @param group_name The name of the target group.
   * @param message The message to send.
   */
  void broadcastToGroup(std::string_view group_name, const Message &message);

  // Authentication
  using Authenticator =
      std::function<bool(std::string_view, std::string_view)>;

  /**
   * @brief Sets a custom authenticator function.
   * @param authenticator The function to use for authentication.
   */
  void setAuthenticator(Authenticator authenticator);

  /**
   * @brief Sets whether authentication is required for clients.
   * @param require True to require authentication, false otherwise.
   */
  void requireAuthentication(bool require);

  // Client metadata
  /**
   * @brief Sets a metadata key-value pair for a client.
   * @param client_id The ID of the client.
   * @param key The metadata key.
   * @param value The metadata value.
   */
  void setClientMetadata(size_t client_id, std::string_view key,
                         std::string_view value);

  /**
   * @brief Gets a metadata value for a client.
   * @param client_id The ID of the client.
   * @param key The metadata key.
   * @return The metadata value, or an empty string if not found.
   */
  auto getClientMetadata(size_t client_id, std::string_view key) -> std::string;

  // Statistics and monitoring
  /**
   * @brief Retrieves the current hub statistics.
   * @return A SocketHubStats object.
   */
  [[nodiscard]] auto getStatistics() const -> SocketHubStats;

  // Status checks
  /**
   * @brief Checks if the server is running.
   * @return True if the server is running, false otherwise.
   */
  [[nodiscard]] auto isRunning() const -> bool;

  /**
   * @brief Checks if a client is connected.
   * @param client_id The ID of the client.
   * @return True if the client is connected, false otherwise.
   */
  [[nodiscard]] auto isClientConnected(size_t client_id) const -> bool;

  /**
   * @brief Gets a list of all connected client IDs.
   * @return A vector of client IDs.
   */
  [[nodiscard]] auto getConnectedClients() const -> std::vector<size_t>;

  /**
   * @brief Gets a list of all group names.
   * @return A vector of group names.
   */
  [[nodiscard]] auto getGroups() const -> std::vector<std::string>;

  /**
   * @brief Gets a list of client IDs in a specific group.
   * @param group_name The name of the group.
   * @return A vector of client IDs.
   */
  [[nodiscard]] auto getClientsInGroup(std::string_view group_name) const
      -> std::vector<size_t>;

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

} // namespace atom::async::connection

#endif // ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP
