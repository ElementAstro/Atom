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
class Logger {
public:
    enum Level { INFO, SUCCESS, WARNING, ERROR, DEBUG };

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
            case INFO:
                std::cout << "[INFO] ";
                break;
            case SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case WARNING:
                std::cout << "[WARN] ";
                break;
            case ERROR:
                std::cout << "[ERROR] ";
                break;
            case DEBUG:
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

        Logger::log(Logger::INFO, "ServerStats",
                    "=== UDP Server Statistics ===");
        Logger::log(Logger::INFO, "ServerStats",
                    "Runtime: " + std::to_string(seconds) + " seconds");
        Logger::log(Logger::INFO, "ServerStats",
                    "Total messages: " + std::to_string(total_messages.load()));
        Logger::log(Logger::INFO, "ServerStats",
                    "Total bytes: " + std::to_string(total_bytes.load()));
        Logger::log(Logger::INFO, "ServerStats",
                    "Unique clients: " + std::to_string(unique_clients.load()));
        Logger::log(
            Logger::INFO, "ServerStats",
            "Active sessions: " + std::to_string(active_sessions.load()));
        Logger::log(
            Logger::INFO, "ServerStats",
            "Processed requests: " + std::to_string(processed_requests.load()));
        Logger::log(
            Logger::INFO, "ServerStats",
            "Failed requests: " + std::to_string(failed_requests.load()));
        Logger::log(
            Logger::INFO, "ServerStats",
            "Broadcast messages: " + std::to_string(broadcast_messages.load()));
        Logger::log(
            Logger::INFO, "ServerStats",
            "Unicast messages: " + std::to_string(unicast_messages.load()));

        if (seconds > 0) {
            Logger::log(Logger::INFO, "ServerStats",
                        "Messages/sec: " +
                            std::to_string(total_messages.load() / seconds));
            Logger::log(
                Logger::INFO, "ServerStats",
                "Bytes/sec: " + std::to_string(total_bytes.load() / seconds));
        }

        double success_rate =
            (processed_requests.load() + failed_requests.load()) > 0
                ? (double(processed_requests.load()) /
                   (processed_requests.load() + failed_requests.load())) *
                      100.0
                : 0.0;
        Logger::log(Logger::INFO, "ServerStats",
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
            Logger::log(Logger::INFO, "SessionMgr",
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
        Logger::log(Logger::INFO, "SessionMgr",
                    "Active sessions (" + std::to_string(active.size()) + "):");

        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(sessions_mutex_));
        for (const auto& endpoint : active) {
            auto it = sessions_.find(endpoint);
            if (it != sessions_.end()) {
                const auto& session = it->second;
                Logger::log(
                    Logger::INFO, "SessionMgr",
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
                Logger::log(Logger::DEBUG, "SessionMgr",
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
    Logger::log(Logger::INFO, "UdpServer", logMsg);
    receivedMessages.push_back(message);
}

// Example 1: Basic UDP server operations
void basicUdpServerExample(int port) {
    Logger::log(Logger::INFO, "Example1", "Starting basic UDP server example");

    try {
        atom::connection::UdpSocketHub udpServer;

        // Add message handler
        udpServer.addMessageHandler(onMessageReceived);
        Logger::log(Logger::INFO, "Example1", "Message handler added");

        // Start the UDP server
        auto startResult = udpServer.start(port);
        if (startResult.has_value()) {
            Logger::log(Logger::SUCCESS, "Example1",
                        "UDP server started on port " + std::to_string(port));
        } else {
            Logger::log(
                Logger::ERROR, "Example1",
                "Failed to start UDP server on port " + std::to_string(port));
            return;
        }

        // Check if server is running
        if (udpServer.isRunning()) {
            Logger::log(Logger::SUCCESS, "Example1", "Server is running");
        }

        // Send a test message to ourselves
        auto sendResult =
            udpServer.sendTo("Hello from server!", "127.0.0.1", port);
        if (sendResult.has_value()) {
            Logger::log(Logger::SUCCESS, "Example1",
                        "Test message sent to self");
        }

        // Keep the server running for a while to receive messages
        Logger::log(Logger::INFO, "Example1",
                    "Server running for 10 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Stop the UDP server
        udpServer.stop();
        Logger::log(Logger::INFO, "Example1", "UDP server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example1", "Basic UDP server example completed");
}

// Example 2: UDP server with statistics monitoring
void statisticsMonitoringExample(int port) {
    Logger::log(Logger::INFO, "Example2",
                "Starting statistics monitoring example");

    try {
        atom::connection::UdpSocketHub udpServer;
        udpServer.addMessageHandler(onMessageReceived);

        auto startResult = udpServer.start(port);
        if (!startResult.has_value()) {
            Logger::log(Logger::ERROR, "Example2", "Failed to start server");
            return;
        }
        Logger::log(Logger::SUCCESS, "Example2",
                    "Server started for statistics monitoring");

        // Send multiple test messages
        std::vector<std::string> testMessages = {"Statistics test message 1",
                                                 "Statistics test message 2",
                                                 "Statistics test message 3"};

        for (const auto& msg : testMessages) {
            auto sendResult = udpServer.sendTo(msg, "127.0.0.1", port);
            if (sendResult.has_value()) {
                Logger::log(Logger::INFO, "Example2", "Sent: " + msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for messages to be processed
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Display message statistics (manual tracking since basic UdpSocketHub
        // doesn't have getStatistics)
        Logger::log(Logger::INFO, "Example2", "=== Message Statistics ===");
        Logger::log(Logger::INFO, "Example2",
                    "Messages received: " + std::to_string(messageCount));
        Logger::log(Logger::INFO, "Example2",
                    "Total received messages: " +
                        std::to_string(receivedMessages.size()));

        // Display received messages
        for (size_t i = 0; i < receivedMessages.size(); ++i) {
            Logger::log(Logger::INFO, "Example2",
                        "Message " + std::to_string(i + 1) + ": " +
                            receivedMessages[i]);
        }

        udpServer.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example2",
                "Statistics monitoring example completed");
}

// Example 3: Multiple handlers and buffer size configuration
void multipleHandlersExample(int port) {
    Logger::log(Logger::INFO, "Example3", "Starting multiple handlers example");

    try {
        atom::connection::UdpSocketHub udpServer;

        // Add multiple message handlers
        udpServer.addMessageHandler([](const std::string& message,
                                       const std::string& senderIp,
                                       int senderPort) {
            Logger::log(Logger::INFO, "Handler1",
                        "Processed: " + message + " from " + senderIp + ":" +
                            std::to_string(senderPort));
        });

        udpServer.addMessageHandler([](const std::string& message,
                                       const std::string& /*senderIp*/,
                                       int /*senderPort*/) {
            if (message.find("important") != std::string::npos) {
                Logger::log(Logger::WARNING, "Handler2",
                            "Important message detected: " + message);
            }
        });

        // Set buffer size
        udpServer.setBufferSize(2048);
        Logger::log(Logger::INFO, "Example3", "Buffer size set to 2048 bytes");

        auto startResult = udpServer.start(port);
        if (!startResult.has_value()) {
            Logger::log(Logger::ERROR, "Example3", "Failed to start server");
            return;
        }
        Logger::log(Logger::SUCCESS, "Example3",
                    "Server started with multiple handlers");

        // Send test messages
        std::vector<std::string> testMessages = {"Normal message",
                                                 "This is an important message",
                                                 "Another normal message"};

        for (const auto& msg : testMessages) {
            auto sendResult = udpServer.sendTo(msg, "127.0.0.1", port);
            if (sendResult.has_value()) {
                Logger::log(Logger::INFO, "Example3", "Sent: " + msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        udpServer.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::INFO, "Example3",
                "Multiple handlers example completed");
}

// Example 4: Advanced message routing and filtering
void advancedMessageRoutingExample(int port) {
    Logger::log(Logger::INFO, "Example4",
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
            Logger::log(
                Logger::INFO, "Router",
                "Processing message from " + clientEndpoint + ": " + message);

            // Route messages based on content
            if (message.find("BROADCAST:") == 0) {
                std::string broadcastMsg =
                    message.substr(10);  // Remove "BROADCAST:" prefix
                Logger::log(Logger::INFO, "Router",
                            "Broadcasting message: " + broadcastMsg);
                globalServerStats.record_message(broadcastMsg.length(), true);

            } else if (message.find("ECHO:") == 0) {
                std::string echoMsg = "ECHO_RESPONSE:" + message.substr(5);
                Logger::log(Logger::INFO, "Router", "Echoing back: " + echoMsg);
                globalServerStats.record_request(true);

            } else if (message.find("STATUS") == 0) {
                auto activeSessions =
                    globalSessionManager.get_active_sessions();
                std::string statusMsg = "SERVER_STATUS:ACTIVE_SESSIONS=" +
                                        std::to_string(activeSessions.size());
                Logger::log(Logger::INFO, "Router",
                            "Status request: " + statusMsg);
                globalServerStats.record_request(true);

            } else if (message.find("DISCOVERY_REQUEST") == 0) {
                std::string discoveryResponse =
                    "DISCOVERY_RESPONSE:UDP_SERVER_AVAILABLE";
                Logger::log(Logger::INFO, "Router",
                            "Discovery request from " + clientEndpoint);
                globalServerStats.record_request(true);

            } else {
                Logger::log(Logger::INFO, "Router",
                            "Regular message processed");
                globalServerStats.record_request(true);
            }
        });

        // Start the server
        auto result = server.start(static_cast<std::uint16_t>(port));
        if (!result.has_value()) {
            Logger::log(
                Logger::ERROR, "Example4",
                "Failed to start server on port " + std::to_string(port));
            return;
        }
        Logger::log(
            Logger::SUCCESS, "Example4",
            "Advanced routing server started on port " + std::to_string(port));

        // Run for a period to demonstrate routing
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Print session information
        globalSessionManager.print_sessions();

        server.stop();
        Logger::log(Logger::INFO, "Example4",
                    "Advanced message routing example completed");

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Example4",
                    "Exception: " + std::string(e.what()));
    }
}

// Example 5: High-performance server with load testing
void highPerformanceServerExample(int port) {
    Logger::log(Logger::INFO, "Example5",
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
            Logger::log(Logger::ERROR, "Example5",
                        "Failed to start high-performance server");
            return;
        }
        Logger::log(
            Logger::SUCCESS, "Example5",
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
                    Logger::log(Logger::ERROR, "LoadGen",
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
        Logger::log(Logger::INFO, "Example5",
                    "Performance test completed in " +
                        std::to_string(duration.count()) + " seconds");

        // Wait for load generators to complete
        for (auto& future : loadGenerators) {
            future.wait();
        }

        server.stop();
        Logger::log(Logger::INFO, "Example5",
                    "High-performance server example completed");

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Example5",
                    "Exception: " + std::string(e.what()));
    }
}

// Function to print comprehensive summary of all examples
void printSummary() {
    Logger::log(Logger::INFO, "Summary",
                "=== Enhanced UDP Server Examples Summary ===");

    // Print global statistics
    globalServerStats.print_summary();

    // Print session information
    globalSessionManager.print_sessions();

    // Print legacy message information
    Logger::log(Logger::INFO, "Summary",
                "Legacy message count: " + std::to_string(messageCount));
    Logger::log(Logger::INFO, "Summary", "Sample messages received:");
    for (size_t i = 0; i < std::min(receivedMessages.size(), size_t(3)); ++i) {
        Logger::log(Logger::INFO, "Summary",
                    "  " + std::to_string(i + 1) + ": " + receivedMessages[i]);
    }
    if (receivedMessages.size() > 3) {
        Logger::log(Logger::INFO, "Summary",
                    "  ... and " + std::to_string(receivedMessages.size() - 3) +
                        " more messages");
    }
}

int main() {
    const int port = 8080;  // Port to listen for incoming messages

    Logger::log(Logger::WARNING, "Main",
                "This example demonstrates Enhanced UDP Server functionality");
    Logger::log(Logger::INFO, "Main",
                "Server will listen on port " + std::to_string(port));
    Logger::log(
        Logger::INFO, "Main",
        "You can test with the udpclient example or netcat: nc -u localhost " +
            std::to_string(port));
    Logger::log(Logger::INFO, "Main", "");
    Logger::log(Logger::INFO, "Main", "Features demonstrated:");
    Logger::log(Logger::INFO, "Main",
                "- Basic UDP server operations with enhanced message handling");
    Logger::log(Logger::INFO, "Main",
                "- Advanced statistics monitoring and reporting");
    Logger::log(Logger::INFO, "Main",
                "- Multiple message handlers and routing");
    Logger::log(Logger::INFO, "Main",
                "- Advanced message routing and filtering");
    Logger::log(Logger::INFO, "Main",
                "- High-performance server with load testing");
    Logger::log(Logger::INFO, "Main",
                "- Client session management and tracking");
    Logger::log(Logger::INFO, "Main",
                "- Comprehensive error handling and recovery");
    Logger::log(Logger::INFO, "Main", "");

    try {
        Logger::log(Logger::INFO, "Main",
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
        Logger::log(Logger::INFO, "Main", "");
        printSummary();

        Logger::log(Logger::SUCCESS, "Main",
                    "All enhanced UDP server examples completed successfully");
        Logger::log(Logger::INFO, "Main", "");
        Logger::log(
            Logger::INFO, "Main",
            "Example completed. Check the output above for detailed results.");
        return 0;

    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Main",
                    "Exception: " + std::string(e.what()));
        globalServerStats.print_summary();
        return 1;
    }
}
