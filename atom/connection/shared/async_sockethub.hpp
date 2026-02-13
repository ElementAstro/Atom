#ifndef ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP
#define ATOM_CONNECTION_ASYNC_SOCKETHUB_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "socket_types.hpp"

#undef ERROR

namespace atom::async::connection {

// Re-export common types for backward compatibility
using atom::connection::LogLevel;
using atom::connection::Message;
using atom::connection::SocketHubConfig;
using atom::connection::SocketHubStats;

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
    void enableLogging(bool enable, LogLevel level = LogLevel::INFO_LEVEL);
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
