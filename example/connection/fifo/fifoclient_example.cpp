/*
 * fifoclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the FifoClient class.
Demonstrates all features including:
- Basic read/write operations
- Configuration options
- Timeout handling
- Async operations with callbacks and futures
- Message priorities
- Statistics tracking
- Connection callbacks
- Multiple message writing

**************************************************/

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/fifo/fifoclient.hpp"

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

// Helper to create FIFO on POSIX systems
void ensureFifoExists(const std::string& path) {
#ifndef _WIN32
    // Remove existing FIFO if present
    unlink(path.c_str());
    // Create new FIFO
    if (mkfifo(path.c_str(), 0666) != 0) {
        Logger::log(Logger::LOG_WARNING, "Setup",
                    "Could not create FIFO (may already exist): " + path);
    }
#endif
}

}  // namespace

// Example 1: Basic FifoClient usage with default configuration
void basicUsageExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic FifoClient Usage ===");

    const std::string fifoPath = createPipePath("basic_fifo");
    ensureFifoExists(fifoPath);

    try {
        // Create client with default configuration
        atom::connection::FifoClient client(fifoPath);

        Logger::log(
            Logger::LOG_INFO, "Example1",
            "Created FifoClient for path: " + std::string(client.getPath()));
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Is open: " + std::string(client.isOpen() ? "yes" : "no"));

        // Write string data
        std::string message = "Hello from FifoClient!";
        auto writeResult =
            client.write(message, std::chrono::milliseconds(1000));

        if (writeResult.has_value()) {
            Logger::log(
                Logger::LOG_SUCCESS, "Example1",
                "Wrote " + std::to_string(writeResult.value()) + " bytes");
        } else {
            Logger::log(
                Logger::LOG_ERR, "Example1",
                "Write failed: " + writeResult.error().error().message());
        }

        // Read data
        auto readResult = client.read(0, std::chrono::milliseconds(1000));
        if (readResult.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example1",
                        "Read: " + readResult.value());
        } else {
            Logger::log(Logger::LOG_WARNING, "Example1",
                        "Read timeout or error (expected in this demo)");
        }

        client.close();
        Logger::log(Logger::LOG_INFO, "Example1", "Client closed");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Basic usage example completed\n");
}

// Example 2: Custom configuration
void customConfigExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Custom Configuration ===");

    const std::string fifoPath = createPipePath("config_fifo");
    ensureFifoExists(fifoPath);

    try {
        // Create custom configuration
        atom::connection::ClientConfig config;
        config.read_buffer_size = 8192;
        config.max_message_size = 2 * 1024 * 1024;  // 2MB
        config.auto_reconnect = true;
        config.max_reconnect_attempts = 10;
        config.reconnect_delay = std::chrono::milliseconds(250);
        config.default_timeout = std::chrono::milliseconds(3000);
        config.enable_compression = false;
        config.compression_threshold = 2048;
        config.enable_encryption = false;

        Logger::log(Logger::LOG_INFO, "Example2", "Configuration:");
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Read buffer size: " + std::to_string(config.read_buffer_size));
        Logger::log(
            Logger::LOG_INFO, "Example2",
            "  - Max message size: " + std::to_string(config.max_message_size));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Auto reconnect: " +
                        std::string(config.auto_reconnect ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Max reconnect attempts: " +
                        std::to_string(config.max_reconnect_attempts));

        // Create client with custom config
        atom::connection::FifoClient client(fifoPath, config);

        // Get and display current config
        auto currentConfig = client.getConfig();
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Client created with custom configuration");

        // Update configuration at runtime
        atom::connection::ClientConfig newConfig = currentConfig;
        newConfig.default_timeout = std::chrono::milliseconds(5000);

        if (client.updateConfig(newConfig)) {
            Logger::log(Logger::LOG_SUCCESS, "Example2",
                        "Configuration updated successfully");
        }

        client.close();

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

    const std::string fifoPath = createPipePath("priority_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        // Send messages with different priorities
        std::vector<std::pair<std::string, atom::connection::MessagePriority>>
            messages = {{"Low priority message",
                         atom::connection::MessagePriority::Low},
                        {"Normal priority message",
                         atom::connection::MessagePriority::Normal},
                        {"High priority message",
                         atom::connection::MessagePriority::High},
                        {"Critical priority message",
                         atom::connection::MessagePriority::Critical}};

        for (const auto& [msg, priority] : messages) {
            std::string priorityStr;
            switch (priority) {
                case atom::connection::MessagePriority::Low:
                    priorityStr = "Low";
                    break;
                case atom::connection::MessagePriority::Normal:
                    priorityStr = "Normal";
                    break;
                case atom::connection::MessagePriority::High:
                    priorityStr = "High";
                    break;
                case atom::connection::MessagePriority::Critical:
                    priorityStr = "Critical";
                    break;
            }

            auto result =
                client.write(msg, priority, std::chrono::milliseconds(1000));
            if (result.has_value()) {
                Logger::log(Logger::LOG_SUCCESS, "Example3",
                            "Sent [" + priorityStr + "]: " + msg + " (" +
                                std::to_string(result.value()) + " bytes)");
            } else {
                Logger::log(Logger::LOG_WARNING, "Example3",
                            "Failed to send [" + priorityStr + "] message");
            }
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Message priority example completed\n");
}

// Example 4: Async operations with callbacks
void asyncCallbackExample() {
    Logger::log(Logger::LOG_INFO, "Example4",
                "=== Async Operations with Callbacks ===");

    const std::string fifoPath = createPipePath("async_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        std::atomic<bool> writeCompleted{false};
        std::atomic<bool> readCompleted{false};

        // Async write with callback
        std::string message = "Async message with callback";
        int writeOpId = client.writeAsync(
            message,
            [&writeCompleted](bool success, std::error_code ec, size_t bytes) {
                if (success) {
                    Logger::log(Logger::LOG_SUCCESS, "Example4",
                                "Async write completed: " +
                                    std::to_string(bytes) + " bytes");
                } else {
                    Logger::log(Logger::LOG_WARNING, "Example4",
                                "Async write failed: " + ec.message());
                }
                writeCompleted = true;
            },
            std::chrono::milliseconds(2000));

        Logger::log(
            Logger::LOG_INFO, "Example4",
            "Started async write operation ID: " + std::to_string(writeOpId));

        // Async read with callback
        int readOpId = client.readAsync(
            [&readCompleted](bool success, std::error_code ec, size_t bytes) {
                if (success) {
                    Logger::log(Logger::LOG_SUCCESS, "Example4",
                                "Async read completed: " +
                                    std::to_string(bytes) + " bytes");
                } else {
                    Logger::log(Logger::LOG_WARNING, "Example4",
                                "Async read completed (timeout expected): " +
                                    ec.message());
                }
                readCompleted = true;
            },
            4096, std::chrono::milliseconds(1000));

        Logger::log(
            Logger::LOG_INFO, "Example4",
            "Started async read operation ID: " + std::to_string(readOpId));

        // Wait for operations to complete
        auto startWait = std::chrono::steady_clock::now();
        while ((!writeCompleted || !readCompleted) &&
               std::chrono::steady_clock::now() - startWait <
                   std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cancel any pending operations
        if (!writeCompleted) {
            client.cancelOperation(writeOpId);
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Cancelled write operation");
        }
        if (!readCompleted) {
            client.cancelOperation(readOpId);
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Cancelled read operation");
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "Async callback example completed\n");
}

// Example 5: Async operations with futures
void asyncFutureExample() {
    Logger::log(Logger::LOG_INFO, "Example5",
                "=== Async Operations with Futures ===");

    const std::string fifoPath = createPipePath("future_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        // Async write with future
        std::string message = "Async message with future";
        auto writeFuture = client.writeAsyncWithFuture(
            message, std::chrono::milliseconds(2000));

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Started async write with future");

        // Wait for write result with timeout
        if (writeFuture.wait_for(std::chrono::seconds(3)) ==
            std::future_status::ready) {
            auto result = writeFuture.get();
            if (result.has_value()) {
                Logger::log(Logger::LOG_SUCCESS, "Example5",
                            "Future write completed: " +
                                std::to_string(result.value()) + " bytes");
            } else {
                Logger::log(
                    Logger::LOG_WARNING, "Example5",
                    "Future write failed: " + result.error().error().message());
            }
        } else {
            Logger::log(Logger::LOG_WARNING, "Example5",
                        "Future write timed out");
        }

        // Async read with future
        auto readFuture =
            client.readAsyncWithFuture(4096, std::chrono::milliseconds(1000));

        Logger::log(Logger::LOG_INFO, "Example5",
                    "Started async read with future");

        if (readFuture.wait_for(std::chrono::seconds(2)) ==
            std::future_status::ready) {
            auto result = readFuture.get();
            if (result.has_value()) {
                Logger::log(Logger::LOG_SUCCESS, "Example5",
                            "Future read completed: " + result.value());
            } else {
                Logger::log(Logger::LOG_WARNING, "Example5",
                            "Future read failed (expected): " +
                                result.error().error().message());
            }
        } else {
            Logger::log(Logger::LOG_WARNING, "Example5",
                        "Future read timed out");
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5",
                "Async future example completed\n");
}

// Example 6: Multiple message writing
void multipleMessagesExample() {
    Logger::log(Logger::LOG_INFO, "Example6",
                "=== Multiple Messages Writing ===");

    const std::string fifoPath = createPipePath("multi_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        // Prepare multiple messages
        std::vector<std::string> messages = {
            "Message 1: Hello", "Message 2: World", "Message 3: From",
            "Message 4: FifoClient", "Message 5: Example"};

        Logger::log(Logger::LOG_INFO, "Example6",
                    "Sending " + std::to_string(messages.size()) + " messages");

        // Write multiple messages at once
        auto result =
            client.writeMultiple(messages, std::chrono::milliseconds(5000));

        if (result.has_value()) {
            Logger::log(
                Logger::LOG_SUCCESS, "Example6",
                "Wrote " + std::to_string(result.value()) + " total bytes");
        } else {
            Logger::log(
                Logger::LOG_WARNING, "Example6",
                "Write multiple failed: " + result.error().error().message());
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6",
                "Multiple messages example completed\n");
}

// Example 7: Connection callbacks and statistics
void connectionAndStatsExample() {
    Logger::log(Logger::LOG_INFO, "Example7",
                "=== Connection Callbacks & Statistics ===");

    const std::string fifoPath = createPipePath("stats_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        // Register connection callback
        int callbackId = client.registerConnectionCallback(
            [](bool connected, std::error_code ec) {
                if (connected) {
                    Logger::log(Logger::LOG_SUCCESS, "Callback",
                                "Connection established");
                } else {
                    Logger::log(Logger::LOG_WARNING, "Callback",
                                "Connection lost: " + ec.message());
                }
            });

        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Registered connection callback ID: " + std::to_string(callbackId));

        // Perform some operations
        for (int i = 0; i < 5; ++i) {
            std::string msg = "Test message " + std::to_string(i + 1);
            client.write(msg, std::chrono::milliseconds(500));
        }

        // Get statistics
        auto stats = client.getStatistics();

        Logger::log(Logger::LOG_INFO, "Example7", "=== Client Statistics ===");
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Messages sent: " + std::to_string(stats.messages_sent));
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Messages failed: " + std::to_string(stats.messages_failed));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Bytes sent: " + std::to_string(stats.bytes_sent));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Bytes received: " + std::to_string(stats.bytes_received));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Avg write latency: " +
                        std::to_string(stats.avg_write_latency_ms) + " ms");
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Avg read latency: " +
                        std::to_string(stats.avg_read_latency_ms) + " ms");
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Reconnect attempts: " + std::to_string(stats.reconnect_attempts));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Successful reconnects: " +
                        std::to_string(stats.successful_reconnects));

        // Reset statistics
        client.resetStatistics();
        Logger::log(Logger::LOG_INFO, "Example7", "Statistics reset");

        // Unregister callback
        if (client.unregisterConnectionCallback(callbackId)) {
            Logger::log(Logger::LOG_INFO, "Example7",
                        "Connection callback unregistered");
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7",
                "Connection and stats example completed\n");
}

// Example 8: Open/Close lifecycle management
void lifecycleExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Lifecycle Management ===");

    const std::string fifoPath = createPipePath("lifecycle_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::ClientConfig config;
        config.default_timeout = std::chrono::milliseconds(2000);

        atom::connection::FifoClient client(fifoPath, config);

        Logger::log(Logger::LOG_INFO, "Example8",
                    "Initial state - Is open: " +
                        std::string(client.isOpen() ? "yes" : "no"));

        // Close the client
        client.close();
        Logger::log(Logger::LOG_INFO, "Example8",
                    "After close - Is open: " +
                        std::string(client.isOpen() ? "yes" : "no"));

        // Reopen the client
        auto openResult = client.open(std::chrono::milliseconds(1000));
        if (openResult.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example8",
                        "Client reopened successfully");
        } else {
            Logger::log(
                Logger::LOG_WARNING, "Example8",
                "Reopen failed: " + openResult.error().error().message());
        }

        Logger::log(Logger::LOG_INFO, "Example8",
                    "After reopen - Is open: " +
                        std::string(client.isOpen() ? "yes" : "no"));

        // Write after reopen
        if (client.isOpen()) {
            auto result = client.write("Message after reopen");
            if (result.has_value()) {
                Logger::log(Logger::LOG_SUCCESS, "Example8",
                            "Write after reopen: " +
                                std::to_string(result.value()) + " bytes");
            }
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Lifecycle example completed\n");
}

// Example 9: Template write with custom data types
void templateWriteExample() {
    Logger::log(Logger::LOG_INFO, "Example9",
                "=== Template Write with Custom Types ===");

    const std::string fifoPath = createPipePath("template_fifo");
    ensureFifoExists(fifoPath);

    try {
        atom::connection::FifoClient client(fifoPath);

        // Write using std::string
        std::string strData = "Hello";
        auto result1 = client.write(strData, std::chrono::milliseconds(1000));
        if (result1.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example9",
                        "Wrote std::string: " +
                            std::to_string(result1.value()) + " bytes");
        }

        // Write using string literal
        auto result2 =
            client.write("Hello from literal", std::chrono::milliseconds(1000));
        if (result2.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example9",
                        "Wrote string literal: " +
                            std::to_string(result2.value()) + " bytes");
        }

        // Write using string_view
        std::string_view strView = "Hello from string_view";
        auto result3 = client.write(strView, std::chrono::milliseconds(1000));
        if (result3.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example9",
                        "Wrote string_view: " +
                            std::to_string(result3.value()) + " bytes");
        }

        client.close();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9",
                "Template write example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main",
                "  FifoClient Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================\n");

    // Run all examples
    basicUsageExample();
    customConfigExample();
    messagePriorityExample();
    asyncCallbackExample();
    asyncFutureExample();
    multipleMessagesExample();
    connectionAndStatsExample();
    lifecycleExample();
    templateWriteExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All FifoClient examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
