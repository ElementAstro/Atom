#ifndef ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP
#define ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#undef ERROR

namespace atom::async::connection {

// Forward declarations
class Client;
struct Message;

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, FATAL };

// Configuration structure for the SocketHub
struct SocketHubConfig {
    bool use_ssl = false;
    int backlog_size = 10;
    std::chrono::seconds connection_timeout{30};
    bool keep_alive = true;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_dh_file;
    std::string ssl_password;
    bool enable_rate_limiting = false;
    int max_connections_per_ip = 10;
    int max_messages_per_minute = 100;
    LogLevel log_level = LogLevel::INFO;
};

// Message structure for more structured data exchange
struct Message {
    enum class Type { TEXT, BINARY, PING, PONG, CLOSE };

    Type type = Type::TEXT;
    std::vector<char> data;
    size_t sender_id = 0;

    static Message createText(std::string text, size_t sender = 0) {
        Message msg;
        msg.type = Type::TEXT;
        msg.data = std::vector<char>(text.begin(), text.end());
        msg.sender_id = sender;
        return msg;
    }

    static Message createBinary(const std::vector<char>& data,
                                size_t sender = 0) {
        Message msg;
        msg.type = Type::BINARY;
        msg.data = data;
        msg.sender_id = sender;
        return msg;
    }

    std::string asString() const {
        return std::string(data.begin(), data.end());
    }
};

// Statistics for monitoring
struct SocketHubStats {
    size_t total_connections = 0;
    size_t active_connections = 0;
    size_t messages_received = 0;
    size_t messages_sent = 0;
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
    std::chrono::system_clock::time_point start_time =
        std::chrono::system_clock::now();
};

// Enhanced SocketHub class
class SocketHub {
public:
    explicit SocketHub(const SocketHubConfig& config = SocketHubConfig{});
    ~SocketHub();

    // Server control
    void start(int port);
    void stop();
    void restart();

    // Handler registration
    void addMessageHandler(
        const std::function<void(const Message&, size_t)>& handler);
    void addConnectHandler(
        const std::function<void(size_t, const std::string&)>& handler);
    void addDisconnectHandler(
        const std::function<void(size_t, const std::string&)>& handler);
    void addErrorHandler(
        const std::function<void(const std::string&, size_t)>& handler);

    // Client interaction
    void broadcastMessage(const Message& message);
    void sendMessageToClient(size_t client_id, const Message& message);
    void disconnectClient(size_t client_id, const std::string& reason = "");

    // Group management
    void createGroup(const std::string& group_name);
    void addClientToGroup(size_t client_id, const std::string& group_name);
    void removeClientFromGroup(size_t client_id, const std::string& group_name);
    void broadcastToGroup(const std::string& group_name,
                          const Message& message);

    // Authentication
    void setAuthenticator(
        const std::function<bool(const std::string&, const std::string&)>&
            authenticator);
    void requireAuthentication(bool require);

    // Client metadata
    void setClientMetadata(size_t client_id, const std::string& key,
                           const std::string& value);
    std::string getClientMetadata(size_t client_id, const std::string& key);

    // Statistics and monitoring
    SocketHubStats getStatistics() const;
    void enableLogging(bool enable, LogLevel level = LogLevel::INFO);
    void setLogHandler(
        const std::function<void(LogLevel, const std::string&)>& handler);

    // Status checks
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isClientConnected(size_t client_id) const;
    [[nodiscard]] std::vector<size_t> getConnectedClients() const;
    [[nodiscard]] std::vector<std::string> getGroups() const;
    [[nodiscard]] std::vector<size_t> getClientsInGroup(
        const std::string& group_name) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::async::connection

#endif  // ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP