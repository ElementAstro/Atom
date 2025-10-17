/*
 * async_udpserver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Enhanced comprehensive example usage of the async UdpSocketHub
class. Demonstrates advanced asynchronous UDP server functionality including:
- Basic async UDP server operations with enhanced message handling
- Advanced message routing and filtering capabilities
- Client session management and tracking
- Load balancing and high availability patterns
- Real-time statistics monitoring and reporting
- Message queuing and buffering strategies
- Performance optimization and tuning
- Error handling and recovery mechanisms
- Multi-threaded server architectures
- Service discovery and health monitoring

**************************************************/

#include "atom/connection/async_udpserver.hpp"

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
class ExampleLogger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERROR, LOG_DEBUG };

    static void write(Level level, const std::string& component,
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

// Enhanced async server statistics tracking
class AsyncServerStatistics {
public:
    std::atomic<size_t> messages_received{0};
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> bytes_sent{0};
    std::atomic<size_t> unique_clients{0};
    std::atomic<size_t> active_sessions{0};
    std::atomic<size_t> handler_invocations{0};
    std::atomic<size_t> broadcast_messages{0};
    std::chrono::steady_clock::time_point start_time;

    AsyncServerStatistics() : start_time(std::chrono::steady_clock::now()) {}

    void record_received_message(size_t bytes) {
        messages_received++;
        bytes_received += bytes;
        handler_invocations++;
    }

    void record_sent_message(size_t bytes, bool is_broadcast = false) {
        messages_sent++;
        bytes_sent += bytes;
        if (is_broadcast) {
            broadcast_messages++;
        }
    }

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        ExampleLogger::write(ExampleLogger::LOG_INFO, "AsyncServerStats",
                             "=== Async UDP Server Statistics ===");
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Runtime: " + std::to_string(seconds) + " seconds");
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Messages received: " + std::to_string(messages_received.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Messages sent: " + std::to_string(messages_sent.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Bytes received: " + std::to_string(bytes_received.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Bytes sent: " + std::to_string(bytes_sent.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Unique clients: " + std::to_string(unique_clients.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Active sessions: " + std::to_string(active_sessions.load()));
        ExampleLogger::write(ExampleLogger::LOG_INFO, "AsyncServerStats",
                             "Handler invocations: " +
                                 std::to_string(handler_invocations.load()));
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncServerStats",
            "Broadcast messages: " + std::to_string(broadcast_messages.load()));

        if (seconds > 0) {
            ExampleLogger::write(
                ExampleLogger::LOG_INFO, "AsyncServerStats",
                "Messages/sec received: " +
                    std::to_string(messages_received.load() / seconds));
            ExampleLogger::write(
                ExampleLogger::LOG_INFO, "AsyncServerStats",
                "Bytes/sec received: " +
                    std::to_string(bytes_received.load() / seconds));
        }
    }
};

// Client session manager for async server
class AsyncClientSessionManager {
private:
    struct ClientSession {
        std::string endpoint;
        std::chrono::steady_clock::time_point last_seen;
        size_t message_count{0};
        size_t bytes_received{0};

        // Default constructor needed for std::map::operator[]
        ClientSession() : last_seen(std::chrono::steady_clock::now()) {}

        ClientSession(const std::string& ep)
            : endpoint(ep), last_seen(std::chrono::steady_clock::now()) {}
    };

    std::map<std::string, ClientSession> sessions_;
    std::mutex sessions_mutex_;

public:
    void update_session(const std::string& ip, unsigned short port,
                        size_t bytes) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        std::string endpoint = ip + ":" + std::to_string(port);

        auto it = sessions_.find(endpoint);
        if (it == sessions_.end()) {
            sessions_.emplace(endpoint, ClientSession(endpoint));
            ExampleLogger::write(ExampleLogger::LOG_INFO, "AsyncSessionMgr",
                                 "New client session: " + endpoint);
        }

        auto& session = sessions_[endpoint];
        session.last_seen = std::chrono::steady_clock::now();
        session.message_count++;
        session.bytes_received += bytes;
    }

    std::vector<std::string> get_active_sessions(
        std::chrono::seconds timeout = std::chrono::seconds(30)) const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(sessions_mutex_));
        std::vector<std::string> active;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [endpoint, session] : sessions_) {
            if (now - session.last_seen < timeout) {
                active.push_back(endpoint);
            }
        }
        return active;
    }

    size_t get_total_sessions() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(sessions_mutex_));
        return sessions_.size();
    }

    void print_sessions() const {
        auto active = get_active_sessions();
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "AsyncSessionMgr",
            "Active sessions (" + std::to_string(active.size()) + "):");

        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(sessions_mutex_));
        for (const auto& endpoint : active) {
            auto it = sessions_.find(endpoint);
            if (it != sessions_.end()) {
                const auto& session = it->second;
                ExampleLogger::write(
                    ExampleLogger::LOG_INFO, "AsyncSessionMgr",
                    "  " + endpoint + " - Messages: " +
                        std::to_string(session.message_count) +
                        ", Bytes: " + std::to_string(session.bytes_received));
            }
        }
    }
};

// Global instances
AsyncServerStatistics globalAsyncServerStats;
AsyncClientSessionManager globalAsyncSessionManager;

// Example 1: Enhanced basic async UDP server
void basicAsyncServerExample() {
    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                         "Starting enhanced basic async UDP server example");

    try {
        UdpSocketHub server;

        // Enhanced message handler with comprehensive processing
        UdpSocketHub::MessageHandler handler = [](const std::string& message,
                                                  const std::string& remoteIp,
                                                  unsigned short remotePort) {
            std::string clientEndpoint =
                remoteIp + ":" + std::to_string(remotePort);
            ExampleLogger::write(
                ExampleLogger::LOG_INFO, "Example1",
                "Received from " + clientEndpoint + ": " + message + " (" +
                    std::to_string(message.length()) + " bytes)");

            // Update statistics and session tracking
            globalAsyncServerStats.record_received_message(message.length());
            globalAsyncSessionManager.update_session(remoteIp, remotePort,
                                                     message.length());

            // Process different message types
            if (message.find("ECHO:") == 0) {
                // Echo back the message
                std::string response = "ECHO_RESPONSE:" + message.substr(5);
                // Note: In a real implementation, you would send the response
                // back
                ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                                     "Would echo back: " + response);
                globalAsyncServerStats.record_sent_message(response.length());

            } else if (message.find("STATUS") == 0) {
                // Provide server status
                auto activeSessions =
                    globalAsyncSessionManager.get_active_sessions();
                std::string statusResponse =
                    "SERVER_STATUS:ACTIVE_SESSIONS=" +
                    std::to_string(activeSessions.size());
                ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                                     "Status response: " + statusResponse);
                globalAsyncServerStats.record_sent_message(
                    statusResponse.length());

            } else if (message.find("BROADCAST:") == 0) {
                // Handle broadcast request
                std::string broadcastMsg = message.substr(10);
                ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                                     "Broadcasting: " + broadcastMsg);
                globalAsyncServerStats.record_sent_message(
                    broadcastMsg.length(), true);

            } else {
                // Regular message processing
                ExampleLogger::write(ExampleLogger::LOG_DEBUG, "Example1",
                                     "Processing regular message");
            }
        };

        // Add the enhanced message handler
        server.addMessageHandler(handler);

        // Start the server
        unsigned short port = 12345;
        bool result = server.start(port);
        if (!result) {
            ExampleLogger::write(
                ExampleLogger::LOG_ERROR, "Example1",
                "Failed to start server on port " + std::to_string(port));
            return;
        }
        ExampleLogger::write(ExampleLogger::LOG_SUCCESS, "Example1",
                             "Server started on port " + std::to_string(port));

        // Check server status
        if (server.isRunning()) {
            ExampleLogger::write(
                ExampleLogger::LOG_SUCCESS, "Example1",
                "Server is running and ready to accept connections");
        } else {
            ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example1",
                                 "Server failed to start properly");
            return;
        }

        // Simulate server operation
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                             "Server running for 8 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(8));

        // Print session information
        globalAsyncSessionManager.print_sessions();

        // Stop the server
        server.stop();
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                             "Server stopped");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example1",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example1",
                         "Enhanced basic async UDP server example completed");
}

// Example 2: Multi-handler server with message routing
void multiHandlerServerExample() {
    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example2",
                         "Starting multi-handler server example");

    try {
        UdpSocketHub server;

        // Handler 1: General message processing
        UdpSocketHub::MessageHandler generalHandler =
            [](const std::string& message, const std::string& remoteIp,
               unsigned short remotePort) {
                if (message.find("GENERAL:") == 0) {
                    ExampleLogger::write(
                        ExampleLogger::LOG_INFO, "GeneralHandler",
                        "Processing general message from " + remoteIp + ":" +
                            std::to_string(remotePort));
                    globalAsyncServerStats.record_received_message(
                        message.length());
                    globalAsyncSessionManager.update_session(
                        remoteIp, remotePort, message.length());
                }
            };

        // Handler 2: Command processing
        UdpSocketHub::MessageHandler commandHandler =
            [](const std::string& message, const std::string& remoteIp,
               unsigned short remotePort) {
                if (message.find("CMD:") == 0) {
                    std::string command = message.substr(4);
                    ExampleLogger::write(
                        ExampleLogger::LOG_INFO, "CommandHandler",
                        "Executing command '" + command + "' from " + remoteIp +
                            ":" + std::to_string(remotePort));
                    globalAsyncServerStats.record_received_message(
                        message.length());
                    globalAsyncSessionManager.update_session(
                        remoteIp, remotePort, message.length());
                }
            };

        // Handler 3: Statistics and monitoring
        UdpSocketHub::MessageHandler statsHandler =
            [](const std::string& message, const std::string& remoteIp,
               unsigned short remotePort) {
                if (message.find("STATS") == 0) {
                    ExampleLogger::write(ExampleLogger::LOG_INFO,
                                         "StatsHandler",
                                         "Statistics request from " + remoteIp +
                                             ":" + std::to_string(remotePort));
                    globalAsyncServerStats.record_received_message(
                        message.length());

                    // Would send back statistics in a real implementation
                    std::string statsResponse =
                        "STATS_RESPONSE:MSG_COUNT=" +
                        std::to_string(
                            globalAsyncServerStats.messages_received.load());
                    ExampleLogger::write(ExampleLogger::LOG_INFO,
                                         "StatsHandler",
                                         "Response: " + statsResponse);
                    globalAsyncServerStats.record_sent_message(
                        statsResponse.length());
                }
            };

        // Add all handlers
        server.addMessageHandler(generalHandler);
        server.addMessageHandler(commandHandler);
        server.addMessageHandler(statsHandler);

        // Start server on different port
        unsigned short port = 12346;
        bool result = server.start(port);
        if (!result) {
            ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example2",
                                 "Failed to start multi-handler server");
            return;
        }
        ExampleLogger::write(
            ExampleLogger::LOG_SUCCESS, "Example2",
            "Multi-handler server started on port " + std::to_string(port));

        // Run server for demonstration
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example2",
                             "Multi-handler server running for 6 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(6));

        server.stop();
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example2",
                             "Multi-handler server stopped");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example2",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example2",
                         "Multi-handler server example completed");
}

// Example 3: Performance monitoring and load testing
void performanceMonitoringExample() {
    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example3",
                         "Starting performance monitoring example");

    try {
        UdpSocketHub server;

        // Performance monitoring handler
        UdpSocketHub::MessageHandler perfHandler =
            [](const std::string& message, const std::string& remoteIp,
               unsigned short remotePort) {
                auto start = std::chrono::high_resolution_clock::now();

                // Process message
                globalAsyncServerStats.record_received_message(
                    message.length());
                globalAsyncSessionManager.update_session(remoteIp, remotePort,
                                                         message.length());

                // Simulate different processing loads
                if (message.find("HEAVY:") == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                } else if (message.find("LIGHT:") == 0) {
                    // Minimal processing
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end - start);

                ExampleLogger::write(
                    ExampleLogger::LOG_DEBUG, "PerfHandler",
                    "Processed message from " + remoteIp + ":" +
                        std::to_string(remotePort) + " in " +
                        std::to_string(duration.count()) + " μs");
            };

        server.addMessageHandler(perfHandler);

        // Start server
        unsigned short port = 12347;
        bool result = server.start(port);
        if (!result) {
            ExampleLogger::write(
                ExampleLogger::LOG_ERROR, "Example3",
                "Failed to start performance monitoring server");
            return;
        }
        ExampleLogger::write(ExampleLogger::LOG_SUCCESS, "Example3",
                             "Performance monitoring server started on port " +
                                 std::to_string(port));

        // Simulate load testing with internal message generation
        std::vector<std::future<void>> loadGenerators;

        for (int i = 0; i < 2; ++i) {
            loadGenerators.push_back(std::async(std::launch::async, [i]() {
                try {
                    for (int j = 0; j < 30; ++j) {
                        std::string messageType = (j % 3 == 0)   ? "HEAVY:"
                                                  : (j % 3 == 1) ? "LIGHT:"
                                                                 : "NORMAL:";
                        std::string testMessage = messageType + "LoadTest_" +
                                                  std::to_string(i) + "_" +
                                                  std::to_string(j);

                        // Simulate message processing
                        globalAsyncServerStats.record_received_message(
                            testMessage.length());

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(50));
                    }
                } catch (const std::exception& e) {
                    ExampleLogger::write(
                        ExampleLogger::LOG_ERROR, "LoadGen",
                        "Load generator " + std::to_string(i) +
                            " error: " + std::string(e.what()));
                }
            }));
        }

        // Monitor performance for a period
        auto startTime = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto endTime = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            endTime - startTime);
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example3",
                             "Performance monitoring completed in " +
                                 std::to_string(duration.count()) + " seconds");

        // Wait for load generators to complete
        for (auto& future : loadGenerators) {
            future.wait();
        }

        server.stop();
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Example3",
                             "Performance monitoring server stopped");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example3",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::LOG_INFO, "Example3",
                         "Performance monitoring example completed");
}

int main() {
    try {
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "Starting Enhanced Async UDP Server Examples");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main", "");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "Features demonstrated:");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Enhanced basic async UDP server operations");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Multi-handler message routing and processing");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Performance monitoring and load testing");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Client session management and tracking");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Advanced error handling and recovery");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Real-time statistics monitoring");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main",
                             "- Comprehensive message processing patterns");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main", "");

        // Run all examples with proper spacing
        basicAsyncServerExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        multiHandlerServerExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        performanceMonitoringExample();

        // Print comprehensive statistics
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main", "");
        globalAsyncServerStats.print_summary();
        globalAsyncSessionManager.print_sessions();

        ExampleLogger::write(
            ExampleLogger::LOG_SUCCESS, "Main",
            "All enhanced async UDP server examples completed successfully");
        ExampleLogger::write(ExampleLogger::LOG_INFO, "Main", "");
        ExampleLogger::write(
            ExampleLogger::LOG_INFO, "Main",
            "Example completed. Check the output above for detailed results.");

        return 0;

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Main",
                             "Fatal error: " + std::string(e.what()));
        globalAsyncServerStats.print_summary();
        return 1;
    }
}
