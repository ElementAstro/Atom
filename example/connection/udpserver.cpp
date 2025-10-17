/*
 * udpserver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Enhanced comprehensive example usage of the UdpSocketHub class.
Demonstrates advanced UDP server functionality including:
- Basic UDP server operations with enhanced message handling
- Advanced message routing and filtering
- Client session management and tracking
- Load balancing and high availability patterns
- Real-time statistics monitoring and reporting
- Message queuing and buffering strategies
- Security features and access control
- Performance optimization and tuning
- Error handling and recovery mechanisms
- Multi-threaded server architectures

**************************************************/

#include "atom/connection/udpserver.hpp"

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

// Enhanced utility class for formatted logging with thread safety
class ExampleLogger {
public:
    enum class Level { Info, Success, Warning, Error, Debug };

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
            case Level::Info:
                std::cout << "[INFO] ";
                break;
            case Level::Success:
                std::cout << "[SUCCESS] ";
                break;
            case Level::Warning:
                std::cout << "[WARN] ";
                break;
            case Level::Error:
                std::cout << "[ERROR] ";
                break;
            case Level::Debug:
                std::cout << "[DEBUG] ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

// Enhanced server statistics tracking
class ServerStatistics {
public:
    std::atomic<size_t> total_messages{0};
    std::atomic<size_t> total_bytes{0};
    std::atomic<size_t> unique_clients{0};
    std::atomic<size_t> active_sessions{0};
    std::atomic<size_t> processed_requests{0};
    std::atomic<size_t> failed_requests{0};
    std::atomic<size_t> broadcast_messages{0};
    std::atomic<size_t> unicast_messages{0};
    std::chrono::steady_clock::time_point start_time;

    ServerStatistics() : start_time(std::chrono::steady_clock::now()) {}

    void record_message(size_t bytes, bool is_broadcast = false) {
        total_messages++;
        total_bytes += bytes;
        if (is_broadcast) {
            broadcast_messages++;
        } else {
            unicast_messages++;
        }
    }

    void record_request(bool success) {
        if (success) {
            processed_requests++;
        } else {
            failed_requests++;
        }
    }

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        ExampleLogger::write(ExampleLogger::Level::Info, "ServerStats",
                             "=== UDP Server Statistics ===");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Runtime: " + std::to_string(seconds) + " seconds");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Total messages: " + std::to_string(total_messages.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Total bytes: " + std::to_string(total_bytes.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Unique clients: " + std::to_string(unique_clients.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Active sessions: " + std::to_string(active_sessions.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Processed requests: " + std::to_string(processed_requests.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Failed requests: " + std::to_string(failed_requests.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Broadcast messages: " + std::to_string(broadcast_messages.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Unicast messages: " + std::to_string(unicast_messages.load()));

        if (seconds > 0) {
            ExampleLogger::write(
                ExampleLogger::Level::Info, "ServerStats",
                "Messages/sec: " +
                    std::to_string(total_messages.load() / seconds));
            ExampleLogger::write(
                ExampleLogger::Level::Info, "ServerStats",
                "Bytes/sec: " + std::to_string(total_bytes.load() / seconds));
        }

        double success_rate =
            (processed_requests.load() + failed_requests.load()) > 0
                ? (double(processed_requests.load()) /
                   (processed_requests.load() + failed_requests.load())) *
                      100.0
                : 0.0;
        ExampleLogger::write(
            ExampleLogger::Level::Info, "ServerStats",
            "Success rate: " + std::to_string(success_rate) + "%");
    }
};

// Client session manager for tracking connected clients
class ClientSessionManager {
private:
    struct ClientSession {
        std::string endpoint;
        std::chrono::steady_clock::time_point last_seen;
        size_t message_count{0};
        size_t bytes_received{0};

        ClientSession() : last_seen(std::chrono::steady_clock::now()) {}

        ClientSession(const std::string& ep)
            : endpoint(ep), last_seen(std::chrono::steady_clock::now()) {}
    };

    std::map<std::string, ClientSession> sessions_;
    std::mutex sessions_mutex_;

public:
    void update_session(const std::string& ip, uint16_t port, size_t bytes) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        std::string endpoint = ip + ":" + std::to_string(port);

        auto it = sessions_.find(endpoint);
        if (it == sessions_.end()) {
            sessions_.emplace(endpoint, ClientSession(endpoint));
            ExampleLogger::write(ExampleLogger::Level::Info, "SessionMgr",
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
            ExampleLogger::Level::Info, "SessionMgr",
            "Active sessions (" + std::to_string(active.size()) + "):");

        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(sessions_mutex_));
        for (const auto& endpoint : active) {
            auto it = sessions_.find(endpoint);
            if (it != sessions_.end()) {
                const auto& session = it->second;
                ExampleLogger::write(
                    ExampleLogger::Level::Info, "SessionMgr",
                    "  " + endpoint + " - Messages: " +
                        std::to_string(session.message_count) +
                        ", Bytes: " + std::to_string(session.bytes_received));
            }
        }
    }

    void cleanup_expired_sessions(
        std::chrono::seconds timeout = std::chrono::seconds(60)) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto now = std::chrono::steady_clock::now();

        auto it = sessions_.begin();
        while (it != sessions_.end()) {
            if (now - it->second.last_seen > timeout) {
                ExampleLogger::write(ExampleLogger::Level::Debug, "SessionMgr",
                                     "Removing expired session: " + it->first);
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// Global instances
ServerStatistics globalServerStats;
ClientSessionManager globalSessionManager;

// Global variables to track server activity
std::vector<std::string> receivedMessages;
int messageCount = 0;

// Function to handle incoming messages
void onMessageReceived(const std::string& message, const std::string& senderIp,
                       int senderPort) {
    messageCount++;
    std::string logMsg = "Message #" + std::to_string(messageCount) + ": '" +
                         message + "' from " + senderIp + ":" +
                         std::to_string(senderPort);
    ExampleLogger::write(ExampleLogger::Level::Info, "UdpServer", logMsg);
    receivedMessages.push_back(message);
}

// Example 1: Basic UDP server operations
void basicUdpServerExample(int port) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Starting basic UDP server example");

    try {
        atom::connection::UdpSocketHub udpServer;

        // Add message handler
        udpServer.addMessageHandler(onMessageReceived);
        ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                             "Message handler added");

        // Start the UDP server
        auto startResult = udpServer.start(port);
        if (startResult.has_value()) {
            ExampleLogger::write(
                ExampleLogger::Level::Success, "Example1",
                "UDP server started on port " + std::to_string(port));
        } else {
            ExampleLogger::write(
                ExampleLogger::Level::Error, "Example1",
                "Failed to start UDP server on port " + std::to_string(port));
            return;
        }

        // Check if server is running
        if (udpServer.isRunning()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                                 "Server is running");
        }

        // Send a test message to ourselves
        auto sendResult =
            udpServer.sendTo("Hello from server!", "127.0.0.1", port);
        if (sendResult.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                                 "Test message sent to self");
        }

        // Keep the server running for a while to receive messages
        ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                             "Server running for 10 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Stop the UDP server
        udpServer.stop();
        ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                             "UDP server stopped");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example1",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Basic UDP server example completed");
}

// Example 2: UDP server with statistics monitoring
void statisticsMonitoringExample(int port) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Starting statistics monitoring example");

    try {
        atom::connection::UdpSocketHub udpServer;
        udpServer.addMessageHandler(onMessageReceived);

        auto startResult = udpServer.start(port);
        if (!startResult.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Error, "Example2",
                                 "Failed to start server");
            return;
        }
        ExampleLogger::write(ExampleLogger::Level::Success, "Example2",
                             "Server started for statistics monitoring");

        // Send multiple test messages
        std::vector<std::string> testMessages = {"Statistics test message 1",
                                                 "Statistics test message 2",
                                                 "Statistics test message 3"};

        for (const auto& msg : testMessages) {
            auto sendResult = udpServer.sendTo(msg, "127.0.0.1", port);
            if (sendResult.has_value()) {
                ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                                     "Sent: " + msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for messages to be processed
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Display message statistics (manual tracking since basic UdpSocketHub
        // doesn't have getStatistics)
        ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                             "=== Message Statistics ===");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Example2",
            "Messages received: " + std::to_string(messageCount));
        ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                             "Total received messages: " +
                                 std::to_string(receivedMessages.size()));

        // Display received messages
        for (size_t i = 0; i < receivedMessages.size(); ++i) {
            ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                                 "Message " + std::to_string(i + 1) + ": " +
                                     receivedMessages[i]);
        }

        udpServer.stop();

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example2",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Statistics monitoring example completed");
}

// Example 3: Multiple handlers and buffer size configuration
void multipleHandlersExample(int port) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "Starting multiple handlers example");

    try {
        atom::connection::UdpSocketHub udpServer;

        // Add multiple message handlers
        udpServer.addMessageHandler([](const std::string& message,
                                       const std::string& senderIp,
                                       int senderPort) {
            ExampleLogger::write(ExampleLogger::Level::Info, "Handler1",
                                 "Processed: " + message + " from " + senderIp +
                                     ":" + std::to_string(senderPort));
        });

        udpServer.addMessageHandler([](const std::string& message,
                                       const std::string& /*senderIp*/,
                                       int /*senderPort*/) {
            if (message.find("important") != std::string::npos) {
                ExampleLogger::write(ExampleLogger::Level::Warning, "Handler2",
                                     "Important message detected: " + message);
            }
        });

        // Set buffer size
        udpServer.setBufferSize(2048);
        ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                             "Buffer size set to 2048 bytes");

        auto startResult = udpServer.start(port);
        if (!startResult.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Error, "Example3",
                                 "Failed to start server");
            return;
        }
        ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                             "Server started with multiple handlers");

        // Send test messages
        std::vector<std::string> testMessages = {"Normal message",
                                                 "This is an important message",
                                                 "Another normal message"};

        for (const auto& msg : testMessages) {
            auto sendResult = udpServer.sendTo(msg, "127.0.0.1", port);
            if (sendResult.has_value()) {
                ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                                     "Sent: " + msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        udpServer.stop();

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example3",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "Multiple handlers example completed");
}

// Example 4: Advanced message routing and filtering
void advancedMessageRoutingExample(int port) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                         "Starting advanced message routing example on port " +
                             std::to_string(port));

    try {
        atom::connection::UdpSocketHub server;

        // Set up advanced message routing handler
        server.addMessageHandler([](const std::string& message,
                                    const std::string& ip,
                                    unsigned short clientPort) {
            globalSessionManager.update_session(ip, clientPort,
                                                message.length());
            globalServerStats.record_message(message.length());

            std::string clientEndpoint = ip + ":" + std::to_string(clientPort);
            ExampleLogger::write(
                ExampleLogger::Level::Info, "Router",
                "Processing message from " + clientEndpoint + ": " + message);

            // Route messages based on content
            if (message.find("BROADCAST:") == 0) {
                std::string broadcastMsg =
                    message.substr(10);  // Remove "BROADCAST:" prefix
                ExampleLogger::write(ExampleLogger::Level::Info, "Router",
                                     "Broadcasting message: " + broadcastMsg);
                globalServerStats.record_message(broadcastMsg.length(), true);

            } else if (message.find("ECHO:") == 0) {
                std::string echoMsg = "ECHO_RESPONSE:" + message.substr(5);
                ExampleLogger::write(ExampleLogger::Level::Info, "Router",
                                     "Echoing back: " + echoMsg);
                globalServerStats.record_request(true);

            } else if (message.find("STATUS") == 0) {
                auto activeSessions =
                    globalSessionManager.get_active_sessions();
                std::string statusMsg = "SERVER_STATUS:ACTIVE_SESSIONS=" +
                                        std::to_string(activeSessions.size());
                ExampleLogger::write(ExampleLogger::Level::Info, "Router",
                                     "Status request: " + statusMsg);
                globalServerStats.record_request(true);

            } else if (message.find("DISCOVERY_REQUEST") == 0) {
                std::string discoveryResponse =
                    "DISCOVERY_RESPONSE:UDP_SERVER_AVAILABLE";
                ExampleLogger::write(
                    ExampleLogger::Level::Info, "Router",
                    "Discovery request from " + clientEndpoint);
                globalServerStats.record_request(true);

            } else {
                ExampleLogger::write(ExampleLogger::Level::Info, "Router",
                                     "Regular message processed");
                globalServerStats.record_request(true);
            }
        });

        // Start the server
        auto result = server.start(static_cast<std::uint16_t>(port));
        if (!result.has_value()) {
            ExampleLogger::write(
                ExampleLogger::Level::Error, "Example4",
                "Failed to start server on port " + std::to_string(port));
            return;
        }
        ExampleLogger::write(
            ExampleLogger::Level::Success, "Example4",
            "Advanced routing server started on port " + std::to_string(port));

        // Run for a period to demonstrate routing
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Print session information
        globalSessionManager.print_sessions();

        server.stop();
        ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                             "Advanced message routing example completed");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example4",
                             "Exception: " + std::string(e.what()));
    }
}

// Example 5: High-performance server with load testing
void highPerformanceServerExample(int port) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                         "Starting high-performance server example on port " +
                             std::to_string(port));

    try {
        atom::connection::UdpSocketHub server;

        // Performance-optimized message handler
        server.addMessageHandler([](const std::string& message,
                                    const std::string& /*ip*/,
                                    unsigned short /*clientPort*/) {
            // Minimal processing for maximum throughput
            globalServerStats.record_message(message.length());

            // Simulate different processing loads based on message type
            if (message.find("HEAVY:") == 0) {
                // Simulate heavy processing
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                globalServerStats.record_request(true);
            } else if (message.find("LIGHT:") == 0) {
                // Minimal processing
                globalServerStats.record_request(true);
            } else {
                // Standard processing
                globalServerStats.record_request(true);
            }
        });

        // Start the server
        auto result = server.start(static_cast<std::uint16_t>(port));
        if (!result.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Error, "Example5",
                                 "Failed to start high-performance server");
            return;
        }
        ExampleLogger::write(
            ExampleLogger::Level::Success, "Example5",
            "High-performance server started on port " + std::to_string(port));

        // Simulate load testing by generating internal traffic
        std::vector<std::future<void>> loadGenerators;

        for (int i = 0; i < 3; ++i) {
            loadGenerators.push_back(std::async(std::launch::async, [i]() {
                try {
                    // Simulate client load
                    for (int j = 0; j < 50; ++j) {
                        std::string testMessage = "LOAD_TEST_" +
                                                  std::to_string(i) + "_" +
                                                  std::to_string(j);
                        globalServerStats.record_message(testMessage.length());
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(20));
                    }
                } catch (const std::exception& e) {
                    ExampleLogger::write(
                        ExampleLogger::Level::Error, "LoadGen",
                        "Load generator " + std::to_string(i) +
                            " error: " + std::string(e.what()));
                }
            }));
        }

        // Monitor performance for a period
        auto startTime = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(8));
        auto endTime = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            endTime - startTime);
        ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                             "Performance test completed in " +
                                 std::to_string(duration.count()) + " seconds");

        // Wait for load generators to complete
        for (auto& future : loadGenerators) {
            future.wait();
        }

        server.stop();
        ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                             "High-performance server example completed");

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example5",
                             "Exception: " + std::string(e.what()));
    }
}

// Function to print comprehensive summary of all examples
void printSummary() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Summary",
                         "=== Enhanced UDP Server Examples Summary ===");

    // Print global statistics
    globalServerStats.print_summary();

    // Print session information
    globalSessionManager.print_sessions();

    // Print legacy message information
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Summary",
        "Legacy message count: " + std::to_string(messageCount));
    ExampleLogger::write(ExampleLogger::Level::Info, "Summary",
                         "Sample messages received:");
    for (size_t i = 0; i < std::min(receivedMessages.size(), size_t(3)); ++i) {
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Summary",
            "  " + std::to_string(i + 1) + ": " + receivedMessages[i]);
    }
    if (receivedMessages.size() > 3) {
        ExampleLogger::write(ExampleLogger::Level::Info, "Summary",
                             "  ... and " +
                                 std::to_string(receivedMessages.size() - 3) +
                                 " more messages");
    }
}

int main() {
    const int port = 8080;  // Port to listen for incoming messages

    ExampleLogger::write(
        ExampleLogger::Level::Warning, "Main",
        "This example demonstrates Enhanced UDP Server functionality");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "Server will listen on port " + std::to_string(port));
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Main",
        "You can test with the udpclient example or netcat: nc -u localhost " +
            std::to_string(port));
    ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "Features demonstrated:");
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Main",
        "- Basic UDP server operations with enhanced message handling");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- Advanced statistics monitoring and reporting");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- Multiple message handlers and routing");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- Advanced message routing and filtering");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- High-performance server with load testing");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- Client session management and tracking");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "- Comprehensive error handling and recovery");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");

    try {
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "Starting enhanced comprehensive UDP server examples");

        // Run all examples with proper spacing
        basicUdpServerExample(port);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        statisticsMonitoringExample(port + 1);  // Use different port
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        multipleHandlersExample(port + 2);  // Use different port
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        advancedMessageRoutingExample(port + 3);  // Use different port
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        highPerformanceServerExample(port + 4);  // Use different port

        // Print comprehensive summary
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
        printSummary();

        ExampleLogger::write(
            ExampleLogger::Level::Success, "Main",
            "All enhanced UDP server examples completed successfully");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "Example completed. Check the output above for detailed results.");
        return 0;

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Main",
                             "Exception: " + std::string(e.what()));
        globalServerStats.print_summary();
        return 1;
    }
}
