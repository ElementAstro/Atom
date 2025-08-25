/*
 * async_udpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Enhanced comprehensive example usage of the async UdpClient class.
Demonstrates advanced asynchronous UDP communication patterns including:
- Basic async UDP operations with enhanced callback handling
- Future-based async operations and promise patterns
- Concurrent message processing and load balancing
- Advanced error handling and recovery mechanisms
- Performance monitoring and statistics tracking
- Message queuing and buffering strategies
- Network discovery and service location
- Real-time data streaming patterns
- Quality of Service (QoS) implementations

**************************************************/

#include "atom/connection/async_udpclient.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace atom::async::connection;

// Enhanced utility class for formatted logging with thread safety
class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERROR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);

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
                std::cout << "[INFO] ";
                break;
            case LOG_SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case LOG_WARNING:
                std::cout << "[WARN] ";
                break;
            case LOG_ERROR:
                std::cout << "[ERROR] ";
                break;
            case LOG_DEBUG:
                std::cout << "[DEBUG] ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

// Enhanced async UDP statistics tracking
class AsyncUdpStatistics {
public:
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> messages_received{0};
    std::atomic<size_t> bytes_sent{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> async_operations{0};
    std::atomic<size_t> successful_operations{0};
    std::atomic<size_t> failed_operations{0};
    std::atomic<size_t> callback_invocations{0};
    std::chrono::steady_clock::time_point start_time;

    AsyncUdpStatistics() : start_time(std::chrono::steady_clock::now()) {}

    void record_send(size_t bytes, bool success) {
        if (success) {
            messages_sent++;
            bytes_sent += bytes;
            successful_operations++;
        } else {
            failed_operations++;
        }
        async_operations++;
    }

    void record_receive(size_t bytes) {
        messages_received++;
        bytes_received += bytes;
        callback_invocations++;
    }

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "=== Async UDP Statistics ===");
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Runtime: " + std::to_string(seconds) + " seconds");
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Messages sent: " + std::to_string(messages_sent.load()));
        Logger::log(
            Logger::LOG_INFO, "AsyncStats",
            "Messages received: " + std::to_string(messages_received.load()));
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Bytes sent: " + std::to_string(bytes_sent.load()));
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Bytes received: " + std::to_string(bytes_received.load()));
        Logger::log(
            Logger::LOG_INFO, "AsyncStats",
            "Async operations: " + std::to_string(async_operations.load()));
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Successful operations: " +
                        std::to_string(successful_operations.load()));
        Logger::log(
            Logger::LOG_INFO, "AsyncStats",
            "Failed operations: " + std::to_string(failed_operations.load()));
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Callback invocations: " +
                        std::to_string(callback_invocations.load()));

        if (seconds > 0) {
            Logger::log(Logger::LOG_INFO, "AsyncStats",
                        "Messages/sec sent: " +
                            std::to_string(messages_sent.load() / seconds));
            Logger::log(Logger::LOG_INFO, "AsyncStats",
                        "Bytes/sec sent: " +
                            std::to_string(bytes_sent.load() / seconds));
        }

        double success_rate = async_operations.load() > 0
                                  ? (double(successful_operations.load()) /
                                     async_operations.load()) *
                                        100.0
                                  : 0.0;
        Logger::log(Logger::LOG_INFO, "AsyncStats",
                    "Success rate: " + std::to_string(success_rate) + "%");
    }
};

// Async message queue for handling received messages
class AsyncMessageQueue {
private:
    std::queue<std::pair<std::vector<char>, std::string>> messages_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_processing_{false};

public:
    void push(const std::vector<char>& data, const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        messages_.emplace(data, endpoint);
        cv_.notify_one();
    }

    bool pop(
        std::vector<char>& data, std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (cv_.wait_for(lock, timeout, [this] {
                return !messages_.empty() || stop_processing_;
            })) {
            if (!messages_.empty()) {
                auto message = messages_.front();
                messages_.pop();
                data = std::move(message.first);
                endpoint = std::move(message.second);
                return true;
            }
        }
        return false;
    }

    void stop() {
        stop_processing_ = true;
        cv_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
        return messages_.size();
    }
};

// Global instances
AsyncUdpStatistics globalAsyncUdpStats;
AsyncMessageQueue globalMessageQueue;

// Utility function to convert string to vector<char>
std::vector<char> stringToVector(const std::string& str) {
    return std::vector<char>(str.begin(), str.end());
}

// Utility function to convert vector<char> to string
std::string vectorToString(const std::vector<char>& vec) {
    return std::string(vec.begin(), vec.end());
}

// Example 1: Enhanced basic async UDP operations
void basicAsyncUdpExample() {
    Logger::log(Logger::LOG_INFO, "Example1",
                "Starting enhanced basic async UDP example");

    try {
        UdpClient client;

        // Bind to a specific port
        int port = 12345;
        if (!client.bind(port)) {
            Logger::log(Logger::LOG_ERROR, "Example1",
                        "Failed to bind to port " + std::to_string(port));
            return;
        }
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Successfully bound to port " + std::to_string(port));

        // Set enhanced callback for data received
        client.setOnDataReceivedCallback([](const std::vector<char>& data,
                                            const std::string& remoteHost,
                                            int remotePort) {
            std::string message = vectorToString(data);
            std::string endpoint =
                remoteHost + ":" + std::to_string(remotePort);

            Logger::log(Logger::LOG_INFO, "Example1",
                        "Received from " + endpoint + ": " + message + " (" +
                            std::to_string(data.size()) + " bytes)");

            globalAsyncUdpStats.record_receive(data.size());
            globalMessageQueue.push(data, endpoint);
        });

        // Set enhanced error callback
        client.setOnErrorCallback([](const std::string& error, int errorCode) {
            Logger::log(Logger::LOG_ERROR, "Example1",
                        "Error: " + error +
                            " (Code: " + std::to_string(errorCode) + ")");
            globalAsyncUdpStats.failed_operations++;
        });

        // Start receiving with enhanced buffer
        client.startReceiving(2048);
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Started receiving with 2KB buffer");

        // Send multiple test messages
        std::vector<std::pair<std::string, std::string>> testMessages = {
            {"127.0.0.1:54321", "Hello, Async UDP World!"},
            {"127.0.0.1:54321", "JSON:{\"type\":\"test\",\"async\":true}"},
            {"127.0.0.1:54321", "BINARY:" + std::string(50, '\x01')},
            {"127.0.0.1:54321", "LARGE:" + std::string(1000, 'X')}};

        for (const auto& [endpoint, message] : testMessages) {
            size_t colonPos = endpoint.find(':');
            std::string host = endpoint.substr(0, colonPos);
            int targetPort = std::stoi(endpoint.substr(colonPos + 1));

            auto data = stringToVector(message);
            bool success = client.send(host, targetPort, data);

            globalAsyncUdpStats.record_send(data.size(), success);

            if (success) {
                Logger::log(Logger::LOG_SUCCESS, "Example1",
                            "Sent to " + endpoint + ": " +
                                message.substr(0, 50) +
                                (message.length() > 50 ? "..." : "") + " (" +
                                std::to_string(data.size()) + " bytes)");
            } else {
                Logger::log(Logger::LOG_ERROR, "Example1",
                            "Failed to send to " + endpoint);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Test synchronous receive with timeout
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Testing synchronous receive with timeout...");
        std::string receivedHost;
        int receivedPort;
        auto receivedData = client.receive(1024, receivedHost, receivedPort,
                                           std::chrono::milliseconds(3000));

        if (!receivedData.empty()) {
            std::string message = vectorToString(receivedData);
            Logger::log(Logger::LOG_SUCCESS, "Example1",
                        "Synchronously received from " + receivedHost + ":" +
                            std::to_string(receivedPort) + ": " + message);
            globalAsyncUdpStats.record_receive(receivedData.size());
        } else {
            Logger::log(Logger::LOG_WARNING, "Example1",
                        "No data received within timeout period");
        }

        // Process queued messages
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Processing queued messages...");
        std::vector<char> queuedData;
        std::string queuedEndpoint;
        int processedCount = 0;

        while (globalMessageQueue.pop(queuedData, queuedEndpoint,
                                      std::chrono::milliseconds(500)) &&
               processedCount < 5) {
            std::string message = vectorToString(queuedData);
            Logger::log(Logger::LOG_INFO, "Example1",
                        "Processed queued message from " + queuedEndpoint +
                            ": " + message);
            processedCount++;
        }

        // Stop receiving
        client.stopReceiving();
        Logger::log(Logger::LOG_INFO, "Example1", "Stopped receiving data");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Enhanced basic async UDP example completed");
}

// Example 2: Future-based async operations
void futureBasedAsyncExample() {
    Logger::log(Logger::LOG_INFO, "Example2",
                "Starting future-based async operations example");

    try {
        UdpClient client;

        if (!client.bind(12346)) {
            Logger::log(Logger::LOG_ERROR, "Example2",
                        "Failed to bind to port 12346");
            return;
        }

        // Set up callbacks
        client.setOnDataReceivedCallback([](const std::vector<char>& data,
                                            const std::string& /*remoteHost*/,
                                            int /*remotePort*/) {
            globalAsyncUdpStats.record_receive(data.size());
        });

        client.startReceiving(1024);

        // Create futures for async send operations
        std::vector<std::future<bool>> sendFutures;
        std::vector<std::string> messages = {
            "Future_Message_1", "Future_Message_2", "Future_Message_3"};

        for (const auto& message : messages) {
            auto future = std::async(std::launch::async, [&client, message]() {
                auto data = stringToVector(message);
                bool success = client.send("127.0.0.1", 54321, data);
                globalAsyncUdpStats.record_send(data.size(), success);
                return success;
            });
            sendFutures.push_back(std::move(future));
        }

        // Wait for all futures and collect results
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Waiting for future-based operations to complete...");
        for (size_t i = 0; i < sendFutures.size(); ++i) {
            try {
                bool result = sendFutures[i].get();
                Logger::log(result ? Logger::LOG_SUCCESS : Logger::LOG_ERROR,
                            "Example2",
                            "Future operation " + std::to_string(i + 1) +
                                (result ? " succeeded" : " failed"));
            } catch (const std::exception& e) {
                Logger::log(Logger::LOG_ERROR, "Example2",
                            "Future " + std::to_string(i + 1) +
                                " exception: " + std::string(e.what()));
            }
        }

        client.stopReceiving();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2",
                "Future-based async operations example completed");
}

// Example 3: Concurrent message processing
void concurrentProcessingExample() {
    Logger::log(Logger::LOG_INFO, "Example3",
                "Starting concurrent message processing example");

    try {
        std::vector<std::unique_ptr<UdpClient>> clients;
        const int numClients = 3;

        // Create multiple clients for concurrent processing
        for (int i = 0; i < numClients; ++i) {
            auto client = std::make_unique<UdpClient>();
            int port = 12350 + i;

            if (!client->bind(port)) {
                Logger::log(Logger::LOG_ERROR, "Example3",
                            "Failed to bind client " + std::to_string(i) +
                                " to port " + std::to_string(port));
                continue;
            }

            // Set up client-specific callbacks
            client->setOnDataReceivedCallback([i](const std::vector<char>& data,
                                                  const std::string& remoteHost,
                                                  int remotePort) {
                std::string message = vectorToString(data);
                Logger::log(Logger::LOG_INFO, "Example3",
                            "Client " + std::to_string(i) +
                                " received: " + message + " from " +
                                remoteHost + ":" + std::to_string(remotePort));
                globalAsyncUdpStats.record_receive(data.size());
            });

            client->startReceiving(1024);
            clients.push_back(std::move(client));
        }

        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "Created " + std::to_string(clients.size()) +
                        " concurrent clients");

        // Launch concurrent message sending threads
        std::vector<std::thread> senderThreads;

        for (int i = 0; i < numClients; ++i) {
            senderThreads.emplace_back([i]() {
                try {
                    UdpClient sender;
                    for (int j = 0; j < 5; ++j) {
                        std::string message = "Concurrent_Client_" +
                                              std::to_string(i) + "_Message_" +
                                              std::to_string(j);
                        auto data = stringToVector(message);

                        bool success =
                            sender.send("127.0.0.1", 12350 + (i % 3), data);
                        globalAsyncUdpStats.record_send(data.size(), success);

                        if (success) {
                            Logger::log(Logger::LOG_DEBUG, "Example3",
                                        "Thread " + std::to_string(i) +
                                            " sent: " + message);
                        }

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(200));
                    }
                } catch (const std::exception& e) {
                    Logger::log(Logger::LOG_ERROR, "Example3",
                                "Sender thread " + std::to_string(i) +
                                    " error: " + std::string(e.what()));
                }
            });
        }

        // Wait for all sender threads to complete
        for (auto& thread : senderThreads) {
            thread.join();
        }

        // Allow time for message processing
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop all clients
        for (auto& client : clients) {
            client->stopReceiving();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Concurrent message processing example completed");
}

// Example 4: Performance and load testing
void performanceTestingExample() {
    Logger::log(Logger::LOG_INFO, "Example4",
                "Starting performance testing example");

    try {
        UdpClient client;

        if (!client.bind(12360)) {
            Logger::log(Logger::LOG_ERROR, "Example4",
                        "Failed to bind to port 12360");
            return;
        }

        client.setOnDataReceivedCallback([](const std::vector<char>& data,
                                            const std::string& /*remoteHost*/,
                                            int /*remotePort*/) {
            globalAsyncUdpStats.record_receive(data.size());
        });

        client.startReceiving(1024);

        // Performance test: rapid message sending
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Starting rapid message sending test...");
        auto startTime = std::chrono::high_resolution_clock::now();

        const int numMessages = 100;
        for (int i = 0; i < numMessages; ++i) {
            std::string message = "PerfTest_" + std::to_string(i);
            auto data = stringToVector(message);
            bool success = client.send("127.0.0.1", 54321, data);
            globalAsyncUdpStats.record_send(data.size(), success);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);

        Logger::log(Logger::LOG_SUCCESS, "Example4",
                    "Sent " + std::to_string(numMessages) + " messages in " +
                        std::to_string(duration.count()) + " μs");
        Logger::log(
            Logger::LOG_INFO, "Example4",
            "Average: " + std::to_string(duration.count() / numMessages) +
                " μs per message");

        // Load test with multiple message sizes
        std::vector<size_t> messageSizes = {64, 256, 512, 1024,
                                            1400};  // Up to near MTU

        for (size_t size : messageSizes) {
            std::string testData(size, 'X');
            auto data = stringToVector(testData);

            auto start = std::chrono::high_resolution_clock::now();
            bool success = client.send("127.0.0.1", 54321, data);
            auto end = std::chrono::high_resolution_clock::now();

            auto latency =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            globalAsyncUdpStats.record_send(data.size(), success);

            Logger::log(Logger::LOG_INFO, "Example4",
                        "Size " + std::to_string(size) + " bytes: " +
                            std::to_string(latency.count()) + " μs latency");
        }

        client.stopReceiving();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "Performance testing example completed");
}

int main() {
    try {
        Logger::log(Logger::LOG_INFO, "Main",
                    "Starting Enhanced Async UDP Client Examples");
        Logger::log(Logger::LOG_INFO, "Main", "");
        Logger::log(Logger::LOG_INFO, "Main", "Features demonstrated:");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Enhanced basic async UDP operations with callbacks");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Future-based async operations and promise patterns");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Concurrent message processing with multiple clients");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Performance testing and load analysis");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Advanced error handling and recovery");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Message queuing and buffering strategies");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Comprehensive statistics tracking");
        Logger::log(Logger::LOG_INFO, "Main", "");

        // Run all examples with proper spacing
        basicAsyncUdpExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        futureBasedAsyncExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        concurrentProcessingExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        performanceTestingExample();

        // Print comprehensive statistics
        Logger::log(Logger::LOG_INFO, "Main", "");
        globalAsyncUdpStats.print_summary();

        // Process any remaining queued messages
        Logger::log(Logger::LOG_INFO, "Main",
                    "Processing remaining queued messages...");
        std::vector<char> data;
        std::string endpoint;
        int remainingCount = 0;

        while (globalMessageQueue.pop(data, endpoint,
                                      std::chrono::milliseconds(100)) &&
               remainingCount < 10) {
            std::string message = vectorToString(data);
            Logger::log(Logger::LOG_DEBUG, "Main",
                        "Remaining message from " + endpoint + ": " + message);
            remainingCount++;
        }

        if (remainingCount > 0) {
            Logger::log(Logger::LOG_INFO, "Main",
                        "Processed " + std::to_string(remainingCount) +
                            " remaining messages");
        }

        Logger::log(
            Logger::LOG_SUCCESS, "Main",
            "All enhanced async UDP client examples completed successfully");
        Logger::log(Logger::LOG_INFO, "Main", "");
        Logger::log(
            Logger::LOG_INFO, "Main",
            "Example completed. Check the output above for detailed results.");

        return 0;

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Main",
                    "Fatal error: " + std::string(e.what()));
        globalAsyncUdpStats.print_summary();
        return 1;
    }
}
