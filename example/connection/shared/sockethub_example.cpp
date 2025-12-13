/*
 * sockethub_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the SocketHub class.
Demonstrates all features including:
- Basic server operations
- Message handlers
- Client connect/disconnect handlers
- Broadcasting messages
- Sending to specific clients
- Client information tracking
- Client timeout configuration
- Statistics and monitoring

**************************************************/

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/shared/sockethub.hpp"

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

}  // namespace

// Example 1: Basic SocketHub usage
void basicSocketHubExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic SocketHub Usage ===");

    try {
        // Create socket hub
        atom::connection::SocketHub hub;

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Created SocketHub, isRunning: " +
                    std::string(hub.isRunning() ? "yes" : "no"));

        // Start the hub on a port
        int port = 8080;
        hub.start(port);

        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "SocketHub started on port " + std::to_string(hub.getPort()));
        Logger::log(Logger::LOG_INFO, "Example1",
                    "isRunning: " + std::string(hub.isRunning() ? "yes" : "no"));

        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get client count
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Connected clients: " + std::to_string(hub.getClientCount()));

        // Stop the hub
        hub.stop();
        Logger::log(Logger::LOG_INFO, "Example1",
                    "SocketHub stopped, isRunning: " +
                    std::string(hub.isRunning() ? "yes" : "no"));

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1", "Basic SocketHub example completed\n");
}

// Example 2: Message handlers
void messageHandlerExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Message Handlers ===");

    try {
        atom::connection::SocketHub hub;

        std::atomic<int> messageCount{0};

        // Add message handler
        hub.addHandler([&messageCount](std::string_view message) {
            messageCount++;
            Logger::log(Logger::LOG_INFO, "Handler",
                        "Received message: " + std::string(message));
        });

        // Add another handler
        hub.addHandler([](std::string_view message) {
            Logger::log(Logger::LOG_DEBUG, "Handler2",
                        "Message length: " + std::to_string(message.length()));
        });

        hub.start(8081);
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "SocketHub started with message handlers");

        // Simulate running
        std::this_thread::sleep_for(std::chrono::seconds(2));

        Logger::log(Logger::LOG_INFO, "Example2",
                    "Total messages received: " + std::to_string(messageCount.load()));

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2", "Message handler example completed\n");
}

// Example 3: Client connect/disconnect handlers
void clientEventHandlersExample() {
    Logger::log(Logger::LOG_INFO, "Example3", "=== Client Event Handlers ===");

    try {
        atom::connection::SocketHub hub;

        std::atomic<int> connectCount{0};
        std::atomic<int> disconnectCount{0};

        // Add connect handler
        hub.addConnectHandler([&connectCount](int clientId, std::string_view clientAddr) {
            connectCount++;
            Logger::log(Logger::LOG_SUCCESS, "Connect",
                        "Client " + std::to_string(clientId) +
                        " connected from " + std::string(clientAddr));
        });

        // Add disconnect handler
        hub.addDisconnectHandler([&disconnectCount](int clientId, std::string_view clientAddr) {
            disconnectCount++;
            Logger::log(Logger::LOG_WARNING, "Disconnect",
                        "Client " + std::to_string(clientId) +
                        " disconnected (" + std::string(clientAddr) + ")");
        });

        hub.start(8082);
        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "SocketHub started with event handlers");

        // Simulate running
        std::this_thread::sleep_for(std::chrono::seconds(2));

        Logger::log(Logger::LOG_INFO, "Example3",
                    "Total connects: " + std::to_string(connectCount.load()) +
                    ", disconnects: " + std::to_string(disconnectCount.load()));

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3", "Client event handlers example completed\n");
}

// Example 4: Broadcasting messages
void broadcastExample() {
    Logger::log(Logger::LOG_INFO, "Example4", "=== Broadcasting Messages ===");

    try {
        atom::connection::SocketHub hub;

        hub.addConnectHandler([](int clientId, std::string_view) {
            Logger::log(Logger::LOG_INFO, "Broadcast",
                        "Client " + std::to_string(clientId) + " ready for broadcast");
        });

        hub.start(8083);
        Logger::log(Logger::LOG_SUCCESS, "Example4", "SocketHub started");

        // Simulate some clients connecting
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Broadcast messages
        std::vector<std::string> messages = {
            "Welcome to the server!",
            "Server announcement: Maintenance in 5 minutes",
            "Broadcast test message"
        };

        for (const auto& msg : messages) {
            size_t clientsReached = hub.broadcast(msg);
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Broadcast '" + msg + "' to " +
                        std::to_string(clientsReached) + " clients");
        }

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4", "Broadcast example completed\n");
}

// Example 5: Sending to specific clients
void sendToClientExample() {
    Logger::log(Logger::LOG_INFO, "Example5", "=== Sending to Specific Clients ===");

    try {
        atom::connection::SocketHub hub;

        std::vector<int> connectedClients;
        std::mutex clientsMutex;

        hub.addConnectHandler([&](int clientId, std::string_view) {
            std::lock_guard<std::mutex> lock(clientsMutex);
            connectedClients.push_back(clientId);
            Logger::log(Logger::LOG_INFO, "Example5",
                        "Client " + std::to_string(clientId) + " added to list");
        });

        hub.start(8084);
        Logger::log(Logger::LOG_SUCCESS, "Example5", "SocketHub started");

        // Wait for potential connections
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Send to specific clients
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (int clientId : connectedClients) {
                std::string personalMsg = "Hello client " + std::to_string(clientId) + "!";
                bool sent = hub.sendTo(clientId, personalMsg);
                Logger::log(sent ? Logger::LOG_SUCCESS : Logger::LOG_WARNING, "Example5",
                            "Send to client " + std::to_string(clientId) + ": " +
                            (sent ? "success" : "failed"));
            }
        }

        // Try sending to non-existent client
        bool sent = hub.sendTo(99999, "This should fail");
        Logger::log(Logger::LOG_INFO, "Example5",
                    "Send to non-existent client: " + std::string(sent ? "success" : "failed (expected)"));

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5", "Send to client example completed\n");
}

// Example 6: Client information tracking
void clientInfoExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Client Information Tracking ===");

    try {
        atom::connection::SocketHub hub;

        hub.start(8085);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "SocketHub started");

        // Wait for potential connections
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Get connected clients info
        auto clients = hub.getConnectedClients();

        Logger::log(Logger::LOG_INFO, "Example6",
                    "Total connected clients: " + std::to_string(clients.size()));

        for (const auto& client : clients) {
            auto connectedDuration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - client.connectedTime);

            Logger::log(Logger::LOG_INFO, "Example6",
                        "Client ID: " + std::to_string(client.id));
            Logger::log(Logger::LOG_INFO, "Example6",
                        "  Address: " + client.address);
            Logger::log(Logger::LOG_INFO, "Example6",
                        "  Connected for: " + std::to_string(connectedDuration.count()) + "s");
            Logger::log(Logger::LOG_INFO, "Example6",
                        "  Bytes received: " + std::to_string(client.bytesReceived));
            Logger::log(Logger::LOG_INFO, "Example6",
                        "  Bytes sent: " + std::to_string(client.bytesSent));
        }

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6", "Client info example completed\n");
}

// Example 7: Client timeout configuration
void clientTimeoutExample() {
    Logger::log(Logger::LOG_INFO, "Example7", "=== Client Timeout Configuration ===");

    try {
        atom::connection::SocketHub hub;

        // Set client timeout
        hub.setClientTimeout(std::chrono::seconds(30));
        Logger::log(Logger::LOG_INFO, "Example7", "Set client timeout to 30 seconds");

        hub.addDisconnectHandler([](int clientId, std::string_view) {
            Logger::log(Logger::LOG_WARNING, "Timeout",
                        "Client " + std::to_string(clientId) + " timed out");
        });

        hub.start(8086);
        Logger::log(Logger::LOG_SUCCESS, "Example7", "SocketHub started with timeout");

        // Simulate running
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Change timeout dynamically
        hub.setClientTimeout(std::chrono::seconds(60));
        Logger::log(Logger::LOG_INFO, "Example7", "Updated client timeout to 60 seconds");

        hub.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7", "Client timeout example completed\n");
}

// Example 8: Move semantics
void moveSemanticsExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Move Semantics ===");

    try {
        // Create and start hub
        atom::connection::SocketHub hub1;
        hub1.start(8087);

        Logger::log(Logger::LOG_INFO, "Example8",
                    "hub1 running: " + std::string(hub1.isRunning() ? "yes" : "no") +
                    ", port: " + std::to_string(hub1.getPort()));

        // Move to new hub
        atom::connection::SocketHub hub2 = std::move(hub1);

        Logger::log(Logger::LOG_INFO, "Example8", "After move:");
        Logger::log(Logger::LOG_INFO, "Example8",
                    "hub2 running: " + std::string(hub2.isRunning() ? "yes" : "no") +
                    ", port: " + std::to_string(hub2.getPort()));

        // Continue using moved hub
        hub2.broadcast("Message from moved hub");

        hub2.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Move semantics example completed\n");
}

// Example 9: Complete server workflow
void completeWorkflowExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Complete Server Workflow ===");

    try {
        atom::connection::SocketHub hub;

        // Statistics tracking
        std::atomic<size_t> totalMessages{0};
        std::atomic<size_t> totalConnections{0};

        // Setup handlers
        hub.addHandler([&totalMessages](std::string_view msg) {
            totalMessages++;
            // Echo back
            Logger::log(Logger::LOG_DEBUG, "Echo", "Received: " + std::string(msg));
        });

        hub.addConnectHandler([&totalConnections](int clientId, std::string_view addr) {
            totalConnections++;
            Logger::log(Logger::LOG_SUCCESS, "Workflow",
                        "New connection #" + std::to_string(totalConnections.load()) +
                        " from " + std::string(addr));
        });

        hub.addDisconnectHandler([](int clientId, std::string_view) {
            Logger::log(Logger::LOG_INFO, "Workflow",
                        "Client " + std::to_string(clientId) + " left");
        });

        // Configure timeout
        hub.setClientTimeout(std::chrono::seconds(120));

        // Start server
        int port = 8088;
        hub.start(port);
        Logger::log(Logger::LOG_SUCCESS, "Example9",
                    "Server started on port " + std::to_string(port));

        // Simulate server running
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Periodic status
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Status: " + std::to_string(hub.getClientCount()) + " clients, " +
                        std::to_string(totalMessages.load()) + " messages");

            // Periodic broadcast
            hub.broadcast("Server heartbeat " + std::to_string(i + 1));
        }

        // Final stats
        Logger::log(Logger::LOG_INFO, "Example9", "=== Final Statistics ===");
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Total connections: " + std::to_string(totalConnections.load()));
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Total messages: " + std::to_string(totalMessages.load()));
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Current clients: " + std::to_string(hub.getClientCount()));

        hub.stop();
        Logger::log(Logger::LOG_SUCCESS, "Example9", "Server stopped gracefully");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9", "Complete workflow example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main", "========================================");
    Logger::log(Logger::LOG_INFO, "Main", "  SocketHub Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main", "========================================\n");

    // Run all examples
    basicSocketHubExample();
    messageHandlerExample();
    clientEventHandlersExample();
    broadcastExample();
    sendToClientExample();
    clientInfoExample();
    clientTimeoutExample();
    moveSemanticsExample();
    completeWorkflowExample();

    Logger::log(Logger::LOG_SUCCESS, "Main", "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main", "  All SocketHub examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main", "========================================");

    return 0;
}
