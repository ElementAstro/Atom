/*
 * fifoserver_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the FIFOServer class.
Demonstrates all features including:
- Basic server operations
- Custom configuration
- Message priorities
- Async message sending
- Batch message sending
- Message and status callbacks
- Statistics tracking
- Queue management
- Log level configuration

**************************************************/

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/fifo/fifoserver.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Utility class for formatted logging
class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
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

// Helper function to create platform-specific pipe path
std::string createPipePath(const std::string& name) {
#ifdef _WIN32
    return "\\\\.\\pipe\\" + name;
#else
    return "/tmp/" + name;
#endif
}

// Helper to ensure FIFO exists on POSIX systems
void ensureFifoExists(const std::string& path) {
#ifndef _WIN32
    unlink(path.c_str());
    if (mkfifo(path.c_str(), 0666) != 0) {
        Logger::log(Logger::LOG_WARNING, "Setup",
                    "Could not create FIFO: " + path);
    }
#endif
}

std::string priorityToString(atom::connection::MessagePriority priority) {
    switch (priority) {
        case atom::connection::MessagePriority::Low:
            return "Low";
        case atom::connection::MessagePriority::Normal:
            return "Normal";
        case atom::connection::MessagePriority::High:
            return "High";
        case atom::connection::MessagePriority::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

std::string logLevelToString(atom::connection::LogLevel level) {
    switch (level) {
        case atom::connection::LogLevel::Debug:
            return "Debug";
        case atom::connection::LogLevel::Info:
            return "Info";
        case atom::connection::LogLevel::Warning:
            return "Warning";
        case atom::connection::LogLevel::Error:
            return "Error";
        case atom::connection::LogLevel::None:
            return "None";
        default:
            return "Unknown";
    }
}

}  // namespace

// Example 1: Basic FIFOServer usage
void basicServerExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic FIFOServer Usage ===");

    const std::string fifoPath = createPipePath("basic_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        // Create server with default configuration
        atom::connection::FIFOServer server(fifoPath);

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Created FIFOServer for path: " + server.getFifoPath());

        // Start the server
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Server started, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

        // Send some messages
        for (int i = 1; i <= 5; ++i) {
            std::string message = "Basic message " + std::to_string(i);
            if (server.sendMessage(message)) {
                Logger::log(Logger::LOG_SUCCESS, "Example1",
                            "Sent: " + message);
            } else {
                Logger::log(Logger::LOG_WARNING, "Example1",
                            "Failed to send: " + message);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop the server
        server.stop();
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Server stopped, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Basic server example completed\n");
}

// Example 2: Custom server configuration
void customConfigExample() {
    Logger::log(Logger::LOG_INFO, "Example2",
                "=== Custom Server Configuration ===");

    const std::string fifoPath = createPipePath("config_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        // Create custom configuration
        atom::connection::ServerConfig config;
        config.max_queue_size = 500;
        config.max_message_size = 512 * 1024;  // 512KB
        config.enable_compression = false;
        config.enable_encryption = false;
        config.auto_reconnect = true;
        config.max_reconnect_attempts = 3;
        config.reconnect_delay = std::chrono::milliseconds(1000);
        config.log_level = atom::connection::LogLevel::Debug;
        config.flush_on_stop = true;
        config.message_ttl = std::chrono::milliseconds(30000);

        Logger::log(Logger::LOG_INFO, "Example2", "Configuration:");
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Max queue size: " + std::to_string(config.max_queue_size));
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Max message size: " + std::to_string(config.max_message_size));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Auto reconnect: " +
                        std::string(config.auto_reconnect ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Log level: " + logLevelToString(config.log_level));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Flush on stop: " +
                        std::string(config.flush_on_stop ? "yes" : "no"));

        // Create server with custom config
        atom::connection::FIFOServer server(fifoPath, config);

        // Get current config
        auto currentConfig = server.getConfig();
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Server created with custom configuration");

        // Update configuration at runtime
        atom::connection::ServerConfig newConfig = currentConfig;
        newConfig.log_level = atom::connection::LogLevel::Info;

        if (server.updateConfig(newConfig)) {
            Logger::log(Logger::LOG_SUCCESS, "Example2",
                        "Configuration updated at runtime");
        }

        server.start();

        // Send a test message
        server.sendMessage("Test message with custom config");

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2",
                "Custom configuration example completed\n");
}

// Example 3: Message priorities
void messagePriorityExample() {
    Logger::log(Logger::LOG_INFO, "Example3", "=== Message Priorities ===");

    const std::string fifoPath = createPipePath("priority_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);
        server.start();

        // Send messages with different priorities
        std::vector<std::pair<std::string, atom::connection::MessagePriority>>
            messages = {
                {"Low priority task", atom::connection::MessagePriority::Low},
                {"Normal priority task",
                 atom::connection::MessagePriority::Normal},
                {"High priority task", atom::connection::MessagePriority::High},
                {"Critical alert!",
                 atom::connection::MessagePriority::Critical}};

        for (const auto& [msg, priority] : messages) {
            if (server.sendMessage(msg, priority)) {
                Logger::log(
                    Logger::LOG_SUCCESS, "Example3",
                    "Sent [" + priorityToString(priority) + "]: " + msg);
            } else {
                Logger::log(Logger::LOG_WARNING, "Example3",
                            "Failed to send [" + priorityToString(priority) +
                                "]: " + msg);
            }
        }

        // Display queue size
        Logger::log(
            Logger::LOG_INFO, "Example3",
            "Current queue size: " + std::to_string(server.getQueueSize()));

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Message priority example completed\n");
}

// Example 4: Async message sending
void asyncMessageExample() {
    Logger::log(Logger::LOG_INFO, "Example4", "=== Async Message Sending ===");

    const std::string fifoPath = createPipePath("async_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);
        server.start();

        std::vector<std::future<bool>> futures;

        // Send messages asynchronously
        for (int i = 1; i <= 5; ++i) {
            std::string message = "Async message " + std::to_string(i);
            futures.push_back(server.sendMessageAsync(message));
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Queued async message: " + message);
        }

        // Send async messages with priority
        futures.push_back(server.sendMessageAsync(
            "High priority async", atom::connection::MessagePriority::High));
        futures.push_back(server.sendMessageAsync(
            "Critical async", atom::connection::MessagePriority::Critical));

        // Wait for all futures
        int successCount = 0;
        for (size_t i = 0; i < futures.size(); ++i) {
            if (futures[i].wait_for(std::chrono::seconds(5)) ==
                std::future_status::ready) {
                if (futures[i].get()) {
                    successCount++;
                }
            }
        }

        Logger::log(Logger::LOG_SUCCESS, "Example4",
                    "Async messages sent: " + std::to_string(successCount) +
                        "/" + std::to_string(futures.size()));

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "Async message example completed\n");
}

// Example 5: Batch message sending with ranges
void batchMessageExample() {
    Logger::log(Logger::LOG_INFO, "Example5", "=== Batch Message Sending ===");

    const std::string fifoPath = createPipePath("batch_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);
        server.start();

        // Prepare batch of messages
        std::vector<std::string> messages = {
            "Batch message 1", "Batch message 2", "Batch message 3",
            "Batch message 4", "Batch message 5"};

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Sending batch of " + std::to_string(messages.size()) +
                        " messages");

        // Send all messages at once
        size_t sentCount = server.sendMessages(messages);
        Logger::log(Logger::LOG_SUCCESS, "Example5",
                    "Sent " + std::to_string(sentCount) + " messages");

        // Send batch with priority
        std::vector<std::string> urgentMessages = {
            "Urgent batch 1", "Urgent batch 2", "Urgent batch 3"};

        size_t urgentSent = server.sendMessages(
            urgentMessages, atom::connection::MessagePriority::High);
        Logger::log(
            Logger::LOG_SUCCESS, "Example5",
            "Sent " + std::to_string(urgentSent) + " high-priority messages");

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5",
                "Batch message example completed\n");
}

// Example 6: Messageable concept with different types
void messageableTypesExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Messageable Types ===");

    const std::string fifoPath = createPipePath("types_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);
        server.start();

        // Send string
        server.sendMessage(std::string("String message"));
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent std::string");

        // Send string literal (const char*)
        server.sendMessage("String literal message");
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent string literal");

        // Send numeric types (converted via std::to_string)
        server.sendMessage(42);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent int: 42");

        server.sendMessage(3.14159);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent double: 3.14159");

        server.sendMessage(100L);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent long: 100");

        // Send with priority
        server.sendMessage(999, atom::connection::MessagePriority::Critical);
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Sent critical int: 999");

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6",
                "Messageable types example completed\n");
}

// Example 7: Message and status callbacks
void callbacksExample() {
    Logger::log(Logger::LOG_INFO, "Example7",
                "=== Message and Status Callbacks ===");

    const std::string fifoPath = createPipePath("callback_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);

        // Register message callback
        int msgCallbackId = server.registerMessageCallback(
            [](const std::string& message, bool success) {
                if (success) {
                    Logger::log(
                        Logger::LOG_SUCCESS, "MsgCallback",
                        "Message delivered: " + message.substr(0, 30) + "...");
                } else {
                    Logger::log(
                        Logger::LOG_WARNING, "MsgCallback",
                        "Message failed: " + message.substr(0, 30) + "...");
                }
            });

        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Registered message callback ID: " + std::to_string(msgCallbackId));

        // Register status callback
        int statusCallbackId = server.registerStatusCallback([](bool running) {
            Logger::log(Logger::LOG_INFO, "StatusCallback",
                        "Server status changed: " +
                            std::string(running ? "RUNNING" : "STOPPED"));
        });

        Logger::log(Logger::LOG_INFO, "Example7",
                    "Registered status callback ID: " +
                        std::to_string(statusCallbackId));

        // Start server (triggers status callback)
        server.start();

        // Send messages (triggers message callbacks)
        for (int i = 1; i <= 3; ++i) {
            server.sendMessage("Callback test message " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Unregister callbacks
        if (server.unregisterMessageCallback(msgCallbackId)) {
            Logger::log(Logger::LOG_INFO, "Example7",
                        "Message callback unregistered");
        }

        if (server.unregisterStatusCallback(statusCallbackId)) {
            Logger::log(Logger::LOG_INFO, "Example7",
                        "Status callback unregistered");
        }

        // Stop server
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7", "Callbacks example completed\n");
}

// Example 8: Statistics tracking
void statisticsExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Statistics Tracking ===");

    const std::string fifoPath = createPipePath("stats_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);
        server.start();

        // Send various messages
        for (int i = 0; i < 10; ++i) {
            std::string msg(100 + i * 10, 'X');  // Variable size messages
            server.sendMessage(msg);
        }

        // Get statistics
        auto stats = server.getStatistics();

        Logger::log(Logger::LOG_INFO, "Example8", "=== Server Statistics ===");
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Messages sent: " + std::to_string(stats.messages_sent));
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "Messages failed: " + std::to_string(stats.messages_failed));
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Bytes sent: " + std::to_string(stats.bytes_sent));
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Avg message size: " +
                        std::to_string(stats.avg_message_size) + " bytes");
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "Avg latency: " + std::to_string(stats.avg_latency_ms) + " ms");
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Queue high watermark: " +
                        std::to_string(stats.queue_high_watermark));
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "Current queue size: " + std::to_string(stats.current_queue_size));

        // Reset statistics
        server.resetStatistics();
        Logger::log(Logger::LOG_INFO, "Example8", "Statistics reset");

        // Verify reset
        auto resetStats = server.getStatistics();
        Logger::log(Logger::LOG_INFO, "Example8",
                    "After reset - Messages sent: " +
                        std::to_string(resetStats.messages_sent));

        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Statistics example completed\n");
}

// Example 9: Queue management
void queueManagementExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Queue Management ===");

    const std::string fifoPath = createPipePath("queue_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::ServerConfig config;
        config.max_queue_size = 100;
        config.flush_on_stop = false;  // Don't flush on stop for this demo

        atom::connection::FIFOServer server(fifoPath, config);
        server.start();

        // Fill the queue
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Filling queue with messages...");
        for (int i = 0; i < 20; ++i) {
            server.sendMessage("Queue message " + std::to_string(i));
        }

        Logger::log(Logger::LOG_INFO, "Example9",
                    "Queue size after filling: " +
                        std::to_string(server.getQueueSize()));

        // Clear the queue
        size_t cleared = server.clearQueue();
        Logger::log(
            Logger::LOG_SUCCESS, "Example9",
            "Cleared " + std::to_string(cleared) + " messages from queue");

        Logger::log(Logger::LOG_INFO, "Example9",
                    "Queue size after clearing: " +
                        std::to_string(server.getQueueSize()));

        // Stop without flushing (config.flush_on_stop = false)
        server.stop(false);
        Logger::log(Logger::LOG_INFO, "Example9",
                    "Server stopped without flushing");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9",
                "Queue management example completed\n");
}

// Example 10: Log level configuration
void logLevelExample() {
    Logger::log(Logger::LOG_INFO, "Example10",
                "=== Log Level Configuration ===");

    const std::string fifoPath = createPipePath("log_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FIFOServer server(fifoPath);

        // Demonstrate different log levels
        std::vector<atom::connection::LogLevel> levels = {
            atom::connection::LogLevel::Debug, atom::connection::LogLevel::Info,
            atom::connection::LogLevel::Warning,
            atom::connection::LogLevel::Error,
            atom::connection::LogLevel::None};

        for (auto level : levels) {
            server.setLogLevel(level);
            Logger::log(Logger::LOG_INFO, "Example10",
                        "Set log level to: " + logLevelToString(level));
        }

        // Set back to Info for normal operation
        server.setLogLevel(atom::connection::LogLevel::Info);

        server.start();
        server.sendMessage("Test message with Info log level");
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example10",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example10", "Log level example completed\n");
}

// Example 11: Move semantics
void moveSemanticsExample() {
    Logger::log(Logger::LOG_INFO, "Example11", "=== Move Semantics ===");

    const std::string fifoPath = createPipePath("move_server_fifo");
    ensureFifoExists(fifoPath);

    try {
        // Create original server
        atom::connection::FIFOServer server1(fifoPath);
        server1.start();
        server1.sendMessage("Message from server1");

        Logger::log(Logger::LOG_INFO, "Example11",
                    "server1 running: " +
                        std::string(server1.isRunning() ? "yes" : "no"));

        // Move to new server
        atom::connection::FIFOServer server2 = std::move(server1);

        Logger::log(Logger::LOG_INFO, "Example11",
                    "After move - server2 running: " +
                        std::string(server2.isRunning() ? "yes" : "no"));

        // Continue using moved server
        server2.sendMessage("Message from server2 (moved)");

        server2.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example11",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example11",
                "Move semantics example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main",
                "  FIFOServer Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================\n");

    // Run all examples
    basicServerExample();
    customConfigExample();
    messagePriorityExample();
    asyncMessageExample();
    batchMessageExample();
    messageableTypesExample();
    callbacksExample();
    statisticsExample();
    queueManagementExample();
    logLevelExample();
    moveSemanticsExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All FIFOServer examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
