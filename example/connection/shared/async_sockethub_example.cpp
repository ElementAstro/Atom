/*
 * async_sockethub_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the async SocketHub class.
Demonstrates all features including:
- Basic async server operations
- SSL/TLS configuration
- Message and event handlers
- Group management
- Authentication
- Client metadata
- Statistics and logging
- Rate limiting

**************************************************/

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/shared/async_sockethub.hpp"

namespace {

class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ";

        switch (level) {
            case LOG_INFO:
                std::cout << "[INFO]    ";
                break;
            case LOG_SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case LOG_WARNING:
                std::cout << "[WARN]    ";
                break;
            case LOG_ERR:
                std::cout << "[ERROR]   ";
                break;
            case LOG_DEBUG:
                std::cout << "[DEBUG]   ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

std::string logLevelToString(atom::async::connection::LogLevel level) {
    switch (level) {
        case atom::async::connection::LogLevel::TRACE:
            return "TRACE";
        case atom::async::connection::LogLevel::DEBUG_LEVEL:
            return "DEBUG";
        case atom::async::connection::LogLevel::INFO_LEVEL:
            return "INFO";
        case atom::async::connection::LogLevel::WARNING_LEVEL:
            return "WARNING";
        case atom::async::connection::LogLevel::ERROR_LEVEL:
            return "ERROR";
        case atom::async::connection::LogLevel::FATAL_LEVEL:
            return "FATAL";
    }
    return "UNKNOWN";
}

}  // namespace

// Example 1: Basic async SocketHub usage
void basicAsyncSocketHubExample() {
    Logger::log(Logger::LOG_INFO, "Example1",
                "=== Basic Async SocketHub Usage ===");

    try {
        // Create with default config
        atom::async::connection::SocketHub hub;

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Created async SocketHub, isRunning: " +
                        std::string(hub.isRunning() ? "yes" : "no"));

        // Start the hub
        hub.start(9080);
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Async SocketHub started on port 9080");

        // Check status
        Logger::log(
            Logger::LOG_INFO, "Example1",
            "isRunning: " + std::string(hub.isRunning() ? "yes" : "no"));

        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Stop
        hub.stop();
        Logger::log(Logger::LOG_INFO, "Example1", "Async SocketHub stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Basic async SocketHub example completed\n");
}

// Example 2: Custom configuration
void customConfigExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Custom Configuration ===");

    try {
        // Create custom configuration
        atom::async::connection::SocketHubConfig config;
        config.use_ssl = false;
        config.backlog_size = 20;
        config.connection_timeout = std::chrono::seconds(60);
        config.keep_alive = true;
        config.enable_rate_limiting = true;
        config.max_connections_per_ip = 5;
        config.max_messages_per_minute = 200;
        config.log_level = atom::async::connection::LogLevel::DEBUG_LEVEL;

        Logger::log(Logger::LOG_INFO, "Example2", "Configuration:");
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - SSL: " + std::string(config.use_ssl ? "enabled" : "disabled"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Backlog size: " + std::to_string(config.backlog_size));
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Keep alive: " + std::string(config.keep_alive ? "yes" : "no"));
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Rate limiting: " +
                std::string(config.enable_rate_limiting ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Max connections/IP: " +
                        std::to_string(config.max_connections_per_ip));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Log level: " + logLevelToString(config.log_level));

        // Create hub with custom config
        atom::async::connection::SocketHub hub(config);

        hub.start(9081);
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "SocketHub started with custom configuration");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2",
                "Custom configuration example completed\n");
}

// Example 3: Message and event handlers
void handlersExample() {
    Logger::log(Logger::LOG_INFO, "Example3",
                "=== Message and Event Handlers ===");

    try {
        atom::async::connection::SocketHub hub;

        // Add message handler
        hub.addMessageHandler(
            [](const atom::async::connection::Message& msg, size_t clientId) {
                Logger::log(Logger::LOG_INFO, "MsgHandler",
                            "Message from client " + std::to_string(clientId) +
                                ": " + msg.asString());
            });

        // Add connect handler
        hub.addConnectHandler([](size_t clientId, const std::string& addr) {
            Logger::log(Logger::LOG_SUCCESS, "Connect",
                        "Client " + std::to_string(clientId) +
                            " connected from " + addr);
        });

        // Add disconnect handler
        hub.addDisconnectHandler(
            [](size_t clientId, const std::string& reason) {
                Logger::log(Logger::LOG_WARNING, "Disconnect",
                            "Client " + std::to_string(clientId) +
                                " disconnected: " + reason);
            });

        // Add error handler
        hub.addErrorHandler([](const std::string& error, size_t clientId) {
            Logger::log(
                Logger::LOG_ERR, "Error",
                "Error for client " + std::to_string(clientId) + ": " + error);
        });

        hub.start(9082);
        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "SocketHub started with handlers");

        std::this_thread::sleep_for(std::chrono::seconds(2));
        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3", "Handlers example completed\n");
}

// Example 4: Group management
void groupManagementExample() {
    Logger::log(Logger::LOG_INFO, "Example4", "=== Group Management ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.start(9083);
        Logger::log(Logger::LOG_SUCCESS, "Example4", "SocketHub started");

        // Create groups
        hub.createGroup("admins");
        hub.createGroup("users");
        hub.createGroup("guests");
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Created groups: admins, users, guests");

        // List groups
        auto groups = hub.getGroups();
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Total groups: " + std::to_string(groups.size()));
        for (const auto& group : groups) {
            Logger::log(Logger::LOG_INFO, "Example4", "  - " + group);
        }

        // Simulate adding clients to groups (would need actual clients)
        // hub.addClientToGroup(clientId, "admins");
        // hub.removeClientFromGroup(clientId, "admins");

        // Broadcast to group
        auto msg =
            atom::async::connection::Message::createText("Admin announcement!");
        hub.broadcastToGroup("admins", msg);
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Broadcasted to admins group");

        // Get clients in group
        auto admins = hub.getClientsInGroup("admins");
        Logger::log(
            Logger::LOG_INFO, "Example4",
            "Clients in admins group: " + std::to_string(admins.size()));

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "Group management example completed\n");
}

// Example 5: Authentication
void authenticationExample() {
    Logger::log(Logger::LOG_INFO, "Example5", "=== Authentication ===");

    try {
        atom::async::connection::SocketHub hub;

        // Set authenticator
        hub.setAuthenticator([](const std::string& username,
                                const std::string& password) -> bool {
            Logger::log(Logger::LOG_INFO, "Auth",
                        "Authenticating user: " + username);

            // Simple authentication logic
            if (username == "admin" && password == "admin123") {
                Logger::log(Logger::LOG_SUCCESS, "Auth", "Admin authenticated");
                return true;
            }
            if (username == "user" && password == "user123") {
                Logger::log(Logger::LOG_SUCCESS, "Auth", "User authenticated");
                return true;
            }

            Logger::log(Logger::LOG_WARNING, "Auth", "Authentication failed");
            return false;
        });

        // Require authentication
        hub.requireAuthentication(true);
        Logger::log(Logger::LOG_INFO, "Example5", "Authentication required");

        hub.start(9084);
        Logger::log(Logger::LOG_SUCCESS, "Example5",
                    "SocketHub started with authentication");

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Disable authentication
        hub.requireAuthentication(false);
        Logger::log(Logger::LOG_INFO, "Example5", "Authentication disabled");

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5",
                "Authentication example completed\n");
}

// Example 6: Client metadata
void clientMetadataExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Client Metadata ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.addConnectHandler([&hub](size_t clientId, const std::string& addr) {
            // Set metadata for new client
            hub.setClientMetadata(clientId, "role", "guest");
            hub.setClientMetadata(
                clientId, "connected_at",
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch()
                                   .count()));
            hub.setClientMetadata(clientId, "ip_address", addr);

            Logger::log(Logger::LOG_INFO, "Metadata",
                        "Set metadata for client " + std::to_string(clientId));
        });

        hub.start(9085);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "SocketHub started");

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Get connected clients and their metadata
        auto clients = hub.getConnectedClients();
        for (size_t clientId : clients) {
            std::string role = hub.getClientMetadata(clientId, "role");
            std::string ip = hub.getClientMetadata(clientId, "ip_address");

            Logger::log(Logger::LOG_INFO, "Example6",
                        "Client " + std::to_string(clientId) +
                            " - Role: " + role + ", IP: " + ip);
        }

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6",
                "Client metadata example completed\n");
}

// Example 7: Statistics and monitoring
void statisticsExample() {
    Logger::log(Logger::LOG_INFO, "Example7",
                "=== Statistics and Monitoring ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.start(9086);
        Logger::log(Logger::LOG_SUCCESS, "Example7", "SocketHub started");

        // Simulate some activity
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get statistics
        auto stats = hub.getStatistics();

        Logger::log(Logger::LOG_INFO, "Example7", "=== Server Statistics ===");
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Total connections: " + std::to_string(stats.total_connections));
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Active connections: " + std::to_string(stats.active_connections));
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Messages received: " + std::to_string(stats.messages_received));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Messages sent: " + std::to_string(stats.messages_sent));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Bytes received: " + std::to_string(stats.bytes_received));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Bytes sent: " + std::to_string(stats.bytes_sent));

        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - stats.start_time);
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Uptime: " + std::to_string(uptime.count()) + " seconds");

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7", "Statistics example completed\n");
}

// Example 8: Logging configuration
void loggingExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Logging Configuration ===");

    try {
        atom::async::connection::SocketHub hub;

        // Enable logging with custom level
        hub.enableLogging(true, atom::async::connection::LogLevel::DEBUG_LEVEL);
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Logging enabled at DEBUG level");

        // Set custom log handler
        hub.setLogHandler([](atom::async::connection::LogLevel level,
                             const std::string& message) {
            Logger::log(Logger::LOG_DEBUG, "HubLog",
                        "[" + logLevelToString(level) + "] " + message);
        });

        hub.start(9087);
        Logger::log(Logger::LOG_SUCCESS, "Example8",
                    "SocketHub started with logging");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Change log level
        hub.enableLogging(true,
                          atom::async::connection::LogLevel::WARNING_LEVEL);
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Changed log level to WARNING");

        // Disable logging
        hub.enableLogging(false);
        Logger::log(Logger::LOG_INFO, "Example8", "Logging disabled");

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Logging example completed\n");
}

// Example 9: Message types
void messageTypesExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Message Types ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.addMessageHandler(
            [](const atom::async::connection::Message& msg, size_t clientId) {
                std::string typeStr;
                switch (msg.type) {
                    case atom::async::connection::Message::Type::TEXT:
                        typeStr = "TEXT";
                        break;
                    case atom::async::connection::Message::Type::BINARY:
                        typeStr = "BINARY";
                        break;
                    case atom::async::connection::Message::Type::PING:
                        typeStr = "PING";
                        break;
                    case atom::async::connection::Message::Type::PONG:
                        typeStr = "PONG";
                        break;
                    case atom::async::connection::Message::Type::CLOSE:
                        typeStr = "CLOSE";
                        break;
                }
                Logger::log(Logger::LOG_INFO, "Message",
                            "Type: " + typeStr +
                                ", Size: " + std::to_string(msg.data.size()));
            });

        hub.start(9088);
        Logger::log(Logger::LOG_SUCCESS, "Example9", "SocketHub started");

        // Create different message types
        auto textMsg =
            atom::async::connection::Message::createText("Hello World!");
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Created text message: " + textMsg.asString());

        std::vector<char> binaryData = {0x01, 0x02, 0x03, 0x04};
        auto binaryMsg =
            atom::async::connection::Message::createBinary(binaryData);
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Created binary message, size: " +
                        std::to_string(binaryMsg.data.size()));

        // Broadcast messages
        hub.broadcastMessage(textMsg);
        hub.broadcastMessage(binaryMsg);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9",
                "Message types example completed\n");
}

// Example 10: Client management
void clientManagementExample() {
    Logger::log(Logger::LOG_INFO, "Example10", "=== Client Management ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.addConnectHandler([](size_t clientId, const std::string& addr) {
            Logger::log(Logger::LOG_SUCCESS, "ClientMgmt",
                        "Client " + std::to_string(clientId) + " connected");
        });

        hub.start(9089);
        Logger::log(Logger::LOG_SUCCESS, "Example10", "SocketHub started");

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Get connected clients
        auto clients = hub.getConnectedClients();
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Connected clients: " + std::to_string(clients.size()));

        // Check if specific client is connected
        for (size_t clientId : clients) {
            bool connected = hub.isClientConnected(clientId);
            Logger::log(Logger::LOG_INFO, "Example10",
                        "Client " + std::to_string(clientId) +
                            " connected: " + (connected ? "yes" : "no"));

            // Send message to client
            auto msg = atom::async::connection::Message::createText("Welcome!");
            hub.sendMessageToClient(clientId, msg);

            // Disconnect client with reason
            // hub.disconnectClient(clientId, "Server maintenance");
        }

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example10",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example10",
                "Client management example completed\n");
}

// Example 11: Server restart
void serverRestartExample() {
    Logger::log(Logger::LOG_INFO, "Example11", "=== Server Restart ===");

    try {
        atom::async::connection::SocketHub hub;

        hub.start(9090);
        Logger::log(Logger::LOG_SUCCESS, "Example11",
                    "Server started, isRunning: " +
                        std::string(hub.isRunning() ? "yes" : "no"));

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Restart server
        hub.restart();
        Logger::log(Logger::LOG_INFO, "Example11",
                    "Server restarted, isRunning: " +
                        std::string(hub.isRunning() ? "yes" : "no"));

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        hub.stop();
        Logger::log(Logger::LOG_INFO, "Example11", "Server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example11",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example11",
                "Server restart example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "============================================");
    Logger::log(Logger::LOG_INFO, "Main",
                "  Async SocketHub Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "============================================\n");

    // Run all examples
    basicAsyncSocketHubExample();
    customConfigExample();
    handlersExample();
    groupManagementExample();
    authenticationExample();
    clientMetadataExample();
    statisticsExample();
    loggingExample();
    messageTypesExample();
    clientManagementExample();
    serverRestartExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "============================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All async SocketHub examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "============================================");

    return 0;
}
