/*
 * udpserver_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the UdpSocketHub class.
Demonstrates all features including:
- Basic UDP server operations
- Message handlers
- Sending to specific clients
- Broadcasting
- Buffer size configuration
- Error handling

**************************************************/

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/udp/udpserver.hpp"

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

// Helper function to convert UdpError to stringstd::string
// udpErrorToString(atom::connection::UdpError error) {
switch (error) {
    case atom::connection::UdpError::SocketCreationFailed:
        return "SocketCreationFailed";
    case atom::connection::UdpError::BindFailed:
        return "BindFailed";
    case atom::connection::UdpError::NetworkInitFailed:
        return "NetworkInitFailed";
    case atom::connection::UdpError::SendFailed:
        return "SendFailed";
    case atom::connection::UdpError::NotRunning:
        return "NotRunning";
    case atom::connection::UdpError::InvalidAddress:
        return "InvalidAddress";
    case atom::connection::UdpError::InvalidPort:
        return "InvalidPort";
    default:
        return "UnknownError";
}
}

}  // namespace

// Example 1: Basic UDP servervoid basicUdpServerExample() {
Logger::log(Logger::LOG_INFO, "Example1", "=== Basic UdpSocketHub Usage ===");

try {
    // Create UDP server
    atom::connection::UdpSocketHub server;

    Logger::log(Logger::LOG_INFO, "Example1",
                "Created UdpSocketHub, isRunning: " +
                    std::string(server.isRunning() ? "yes" : "no"));

    // Start server on port
    uint16_t port = 12345;
    auto result = server.start(port);

    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Server started on port " + std::to_string(port));
        Logger::log(
            Logger::LOG_INFO, "Example1",
            "isRunning: " + std::string(server.isRunning() ? "yes" : "no"));

        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop server
        server.stop();
        Logger::log(Logger::LOG_INFO, "Example1", "Server stopped");
    } else {
        Logger::log(Logger::LOG_WARNING, "Example1",
                    "Failed to start server: " +
                        udpErrorToString(result.error().error()));
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example1",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example1", "Basic server example completed\n");
}

// Example 2: Message handlersvoid messageHandlerExample() {
Logger::log(Logger::LOG_INFO, "Example2", "=== Message Handlers ===");

try {
    atom::connection::UdpSocketHub server;

    std::atomic<int> messageCount{0};

    // Add message handler
    server.addMessageHandler([&messageCount](const std::string& message,
                                             const std::string& senderIp,
                                             int senderPort) {
        messageCount++;
        Logger::log(Logger::LOG_INFO, "Handler",
                    "Message #" + std::to_string(messageCount.load()) +
                        " from " + senderIp + ":" + std::to_string(senderPort) +
                        ": " + message);
    });

    // Add another handler for logging
    server.addMessageHandler([](const std::string& message,
                                const std::string& senderIp, int senderPort) {
        Logger::log(Logger::LOG_DEBUG, "LogHandler",
                    "Received " + std::to_string(message.size()) + " bytes");
    });

    auto result = server.start(12346);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Server started with message handlers");

        // Simulate running
        std::this_thread::sleep_for(std::chrono::seconds(3));

        Logger::log(
            Logger::LOG_INFO, "Example2",
            "Total messages received: " + std::to_string(messageCount.load()));

        server.stop();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example2",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example2",
            "Message handler example completed\n");
}

// Example 3: Sending to specific clientsvoid sendToClientExample() {
Logger::log(Logger::LOG_INFO, "Example3",
            "=== Sending to Specific Clients ===");

try {
    atom::connection::UdpSocketHub server;

    // Track clients
    std::vector<std::pair<std::string, uint16_t>> clients;
    std::mutex clientsMutex;

    server.addMessageHandler([&](const std::string& message,
                                 const std::string& senderIp, int senderPort) {
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            // Add client if not already tracked
            auto it = std::find_if(
                clients.begin(), clients.end(), [&](const auto& c) {
                    return c.first == senderIp && c.second == senderPort;
                });
            if (it == clients.end()) {
                clients.emplace_back(senderIp, senderPort);
                Logger::log(Logger::LOG_INFO, "Example3",
                            "New client: " + senderIp + ":" +
                                std::to_string(senderPort));
            }
        }
    });

    auto result = server.start(12347);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example3", "Server started");

        // Wait for potential clients
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Send to tracked clients
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (const auto& [ip, port] : clients) {
                auto sendResult = server.sendTo("Hello from server!", ip, port);
                if (sendResult.has_value()) {
                    Logger::log(Logger::LOG_SUCCESS, "Example3",
                                "Sent to " + ip + ":" + std::to_string(port));
                }
            }
        }

        // Send to specific address
        auto sendResult = server.sendTo("Direct message", "127.0.0.1", 54321);
        Logger::log(
            Logger::LOG_INFO, "Example3",
            "Send to 127.0.0.1:54321: " +
                std::string(sendResult.has_value() ? "success" : "failed"));

        server.stop();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example3",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example3", "Send to client example completed\n");
}

// Example 4: Buffer size configurationvoid bufferSizeExample() {
Logger::log(Logger::LOG_INFO, "Example4", "=== Buffer Size Configuration ===");

try {
    atom::connection::UdpSocketHub server;

    // Set buffer size before starting
    size_t bufferSize = 8192;
    server.setBufferSize(bufferSize);
    Logger::log(Logger::LOG_INFO, "Example4",
                "Set buffer size to " + std::to_string(bufferSize) + " bytes");

    auto result = server.start(12348);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example4",
                    "Server started with custom buffer size");

        // Change buffer size while running
        server.setBufferSize(16384);
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Changed buffer size to 16384 bytes");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example4",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example4", "Buffer size example completed\n");
}

// Example 5: Multiple handlers demovoid multipleHandlersExample() {
Logger::log(Logger::LOG_INFO, "Example5", "=== Multiple Handlers ===");

try {
    atom::connection::UdpSocketHub server;

    // Add first handler
    server.addMessageHandler([](const std::string& message,
                                [[maybe_unused]] const std::string& senderIp,
                                [[maybe_unused]] int senderPort) {
        Logger::log(Logger::LOG_INFO, "Handler1", "Received: " + message);
    });

    // Add second handler
    server.addMessageHandler([](const std::string& message,
                                [[maybe_unused]] const std::string& senderIp,
                                [[maybe_unused]] int senderPort) {
        Logger::log(Logger::LOG_DEBUG, "Handler2",
                    "Message size: " + std::to_string(message.size()));
    });

    Logger::log(Logger::LOG_INFO, "Example5", "Added multiple handlers");

    auto result = server.start(12349);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example5", "Server started");

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        server.stop();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example5",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example5",
            "Multiple handlers example completed\n");
}

// Example 6: Server lifecyclevoid serverLifecycleExample() {
Logger::log(Logger::LOG_INFO, "Example6", "=== Server Lifecycle ===");

try {
    atom::connection::UdpSocketHub server;

    // Multiple start/stop cycles
    for (int cycle = 1; cycle <= 3; ++cycle) {
        Logger::log(Logger::LOG_INFO, "Example6",
                    "=== Cycle " + std::to_string(cycle) + " ===");

        auto result = server.start(12350);
        if (result.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example6",
                        "Started - isRunning: " +
                            std::string(server.isRunning() ? "yes" : "no"));

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            server.stop();
            Logger::log(Logger::LOG_INFO, "Example6",
                        "Stopped - isRunning: " +
                            std::string(server.isRunning() ? "yes" : "no"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example6",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example6",
            "Server lifecycle example completed\n");
}

// Example 7: Echo servervoid echoServerExample() {
Logger::log(Logger::LOG_INFO, "Example7", "=== Echo Server ===");

try {
    atom::connection::UdpSocketHub server;

    // Echo handler - sends back what it receives
    server.addMessageHandler([&server](const std::string& message,
                                       const std::string& senderIp,
                                       int senderPort) {
        Logger::log(
            Logger::LOG_INFO, "Echo",
            "Echoing to " + senderIp + ":" + std::to_string(senderPort));
        server.sendTo("ECHO: " + message, senderIp, senderPort);
    });

    auto result = server.start(12351);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example7",
                    "Echo server started on port 12351");
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Send UDP packets to test echo functionality");

        // Run for a while
        std::this_thread::sleep_for(std::chrono::seconds(3));

        server.stop();
        Logger::log(Logger::LOG_INFO, "Example7", "Echo server stopped");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example7",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example7", "Echo server example completed\n");
}

// Example 8: Move semanticsvoid moveSemanticsExample() {
Logger::log(Logger::LOG_INFO, "Example8", "=== Move Semantics ===");

try {
    // Create and start server
    atom::connection::UdpSocketHub server1;
    server1.start(12352);

    Logger::log(
        Logger::LOG_INFO, "Example8",
        "server1 running: " + std::string(server1.isRunning() ? "yes" : "no"));

    // Move to new server
    atom::connection::UdpSocketHub server2 = std::move(server1);

    Logger::log(Logger::LOG_INFO, "Example8", "After move:");
    Logger::log(
        Logger::LOG_INFO, "Example8",
        "server2 running: " + std::string(server2.isRunning() ? "yes" : "no"));

    // Use moved server
    server2.sendTo("Message from moved server", "127.0.0.1", 54321);

    server2.stop();

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example8",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example8", "Move semantics example completed\n");
}

// Example 9: Complete server workflowvoid completeWorkflowExample() {
Logger::log(Logger::LOG_INFO, "Example9", "=== Complete Server Workflow ===");

try {
    atom::connection::UdpSocketHub server;

    // Statistics
    std::atomic<size_t> totalMessages{0};
    std::atomic<size_t> totalBytes{0};

    // Setup handler
    server.addMessageHandler([&](const std::string& message,
                                 const std::string& senderIp, int senderPort) {
        totalMessages++;
        totalBytes += message.size();

        Logger::log(
            Logger::LOG_DEBUG, "Workflow",
            "Message from " + senderIp + ":" + std::to_string(senderPort));

        // Send acknowledgment
        server.sendTo("ACK", senderIp, senderPort);
    });

    // Configure
    server.setBufferSize(4096);

    // Start
    uint16_t port = 12353;
    auto result = server.start(port);
    if (!result.has_value()) {
        Logger::log(
            Logger::LOG_ERR, "Example9",
            "Failed to start: " + udpErrorToString(result.error().error()));
        return;
    }

    Logger::log(Logger::LOG_SUCCESS, "Example9",
                "Server started on port " + std::to_string(port));

    // Run for a while with periodic status
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        Logger::log(Logger::LOG_INFO, "Example9",
                    "Status: " + std::to_string(totalMessages.load()) +
                        " messages, " + std::to_string(totalBytes.load()) +
                        " bytes");
    }

    // Final stats
    Logger::log(Logger::LOG_INFO, "Example9", "=== Final Statistics ===");
    Logger::log(Logger::LOG_INFO, "Example9",
                "Total messages: " + std::to_string(totalMessages.load()));
    Logger::log(Logger::LOG_INFO, "Example9",
                "Total bytes: " + std::to_string(totalBytes.load()));

    server.stop();
    Logger::log(Logger::LOG_SUCCESS, "Example9", "Server stopped gracefully");

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example9",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example9",
            "Complete workflow example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main",
                "  UdpSocketHub Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Some examples require UDP clients to send data");
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicUdpServerExample();
    messageHandlerExample();
    sendToClientExample();
    bufferSizeExample();
    multipleHandlersExample();
    serverLifecycleExample();
    echoServerExample();
    moveSemanticsExample();
    completeWorkflowExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All UdpSocketHub examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
