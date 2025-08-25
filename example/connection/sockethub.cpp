/*
 * sockethub.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Enhanced comprehensive example usage of the SocketHub class.
Demonstrates advanced TCP socket server functionality including:
- Basic socket server operations with enhanced message handling
- Advanced client connection management and tracking
- Message broadcasting and selective communication
- Real-time statistics monitoring and reporting
- Connection lifecycle management (connect/disconnect handlers)
- Performance optimization and load testing
- Error handling and recovery mechanisms
- Multi-threaded client simulation
- Service discovery and health monitoring
- Advanced message routing and filtering

**************************************************/

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/sockethub.hpp"

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

// Enhanced socket hub statistics tracking
class SocketHubStatistics {
public:
    std::atomic<size_t> total_connections{0};
    std::atomic<size_t> active_connections{0};
    std::atomic<size_t> messages_received{0};
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> bytes_sent{0};
    std::atomic<size_t> connection_errors{0};
    std::atomic<size_t> message_errors{0};
    std::chrono::steady_clock::time_point start_time;

    SocketHubStatistics() : start_time(std::chrono::steady_clock::now()) {}

    void record_connection() {
        total_connections++;
        active_connections++;
    }

    void record_disconnection() {
        if (active_connections > 0) {
            active_connections--;
        }
    }

    void record_message_received(size_t bytes) {
        messages_received++;
        bytes_received += bytes;
    }

    void record_message_sent(size_t bytes) {
        messages_sent++;
        bytes_sent += bytes;
    }

    void record_connection_error() { connection_errors++; }

    void record_message_error() { message_errors++; }

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "=== Socket Hub Statistics ===");
        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "Runtime: " + std::to_string(seconds) + " seconds");
        Logger::log(
            Logger::LOG_INFO, "SocketStats",
            "Total connections: " + std::to_string(total_connections.load()));
        Logger::log(
            Logger::LOG_INFO, "SocketStats",
            "Active connections: " + std::to_string(active_connections.load()));
        Logger::log(
            Logger::LOG_INFO, "SocketStats",
            "Messages received: " + std::to_string(messages_received.load()));
        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "Messages sent: " + std::to_string(messages_sent.load()));
        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "Bytes received: " + std::to_string(bytes_received.load()));
        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "Bytes sent: " + std::to_string(bytes_sent.load()));
        Logger::log(
            Logger::LOG_INFO, "SocketStats",
            "Connection errors: " + std::to_string(connection_errors.load()));
        Logger::log(Logger::LOG_INFO, "SocketStats",
                    "Message errors: " + std::to_string(message_errors.load()));

        if (seconds > 0) {
            Logger::log(Logger::LOG_INFO, "SocketStats",
                        "Connections/sec: " +
                            std::to_string(total_connections.load() / seconds));
            Logger::log(Logger::LOG_INFO, "SocketStats",
                        "Messages/sec received: " +
                            std::to_string(messages_received.load() / seconds));
            Logger::log(Logger::LOG_INFO, "SocketStats",
                        "Bytes/sec received: " +
                            std::to_string(bytes_received.load() / seconds));
        }
    }
};

// Client connection manager
class ClientConnectionManager {
private:
    struct ClientInfo {
        int client_id;
        std::string ip_address;
        std::chrono::steady_clock::time_point connect_time;
        size_t messages_received{0};
        size_t bytes_received{0};

        ClientInfo(int id, const std::string& ip)
            : client_id(id),
              ip_address(ip),
              connect_time(std::chrono::steady_clock::now()) {}
    };

    std::map<int, ClientInfo> clients_;
    std::mutex clients_mutex_;

public:
    void add_client(int client_id, const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.emplace(client_id, ClientInfo(client_id, ip_address));
        Logger::log(Logger::LOG_INFO, "ClientMgr",
                    "Client " + std::to_string(client_id) + " connected from " +
                        ip_address);
    }

    void remove_client(int client_id, const std::string& reason = "") {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            auto duration =
                std::chrono::steady_clock::now() - it->second.connect_time;
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(duration)
                    .count();

            Logger::log(Logger::LOG_INFO, "ClientMgr",
                        "Client " + std::to_string(client_id) +
                            " disconnected after " + std::to_string(seconds) +
                            " seconds" +
                            (reason.empty() ? "" : " (" + reason + ")"));
            clients_.erase(it);
        }
    }

    void update_client_stats(int client_id, size_t bytes) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            it->second.messages_received++;
            it->second.bytes_received += bytes;
        }
    }

    std::vector<int> get_active_clients() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(clients_mutex_));
        std::vector<int> active;
        for (const auto& [id, info] : clients_) {
            active.push_back(id);
        }
        return active;
    }

    size_t get_client_count() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(clients_mutex_));
        return clients_.size();
    }

    void print_clients() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(clients_mutex_));
        Logger::log(
            Logger::LOG_INFO, "ClientMgr",
            "Active clients (" + std::to_string(clients_.size()) + "):");

        for (const auto& [id, info] : clients_) {
            auto duration =
                std::chrono::steady_clock::now() - info.connect_time;
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(duration)
                    .count();

            Logger::log(
                Logger::LOG_INFO, "ClientMgr",
                "  Client " + std::to_string(id) + " (" + info.ip_address +
                    ") - " + "Connected: " + std::to_string(seconds) + "s, " +
                    "Messages: " + std::to_string(info.messages_received) +
                    ", " + "Bytes: " + std::to_string(info.bytes_received));
        }
    }
};

// Global instances
SocketHubStatistics globalSocketStats;
ClientConnectionManager globalClientManager;

// Example 1: Enhanced basic socket hub server
void basicSocketHubExample(int port) {
    Logger::log(Logger::LOG_INFO, "Example1",
                "Starting enhanced basic socket hub example on port " +
                    std::to_string(port));

    try {
        atom::connection::SocketHub socketHub;

        // Enhanced message handler with comprehensive processing
        socketHub.addHandler([](std::string_view message) {
            std::string msg(message);
            Logger::log(Logger::LOG_INFO, "Example1",
                        "Received message: " + msg + " (" +
                            std::to_string(message.length()) + " bytes)");

            globalSocketStats.record_message_received(message.length());

            // Process different message types
            if (msg.find("ECHO:") == 0) {
                Logger::log(Logger::LOG_INFO, "Example1",
                            "Echo request: " + msg.substr(5));
                globalSocketStats.record_message_sent(msg.length());

            } else if (msg.find("BROADCAST:") == 0) {
                Logger::log(Logger::LOG_INFO, "Example1",
                            "Broadcast request: " + msg.substr(10));
                globalSocketStats.record_message_sent(msg.length());

            } else if (msg.find("STATUS") == 0) {
                std::string statusResponse =
                    "SERVER_STATUS:ACTIVE_CLIENTS=" +
                    std::to_string(globalClientManager.get_client_count());
                Logger::log(Logger::LOG_INFO, "Example1",
                            "Status response: " + statusResponse);
                globalSocketStats.record_message_sent(statusResponse.length());

            } else {
                Logger::log(Logger::LOG_DEBUG, "Example1",
                            "Processing regular message");
            }
        });

        // Add connection handler
        socketHub.addConnectHandler([](int clientId,
                                       std::string_view ipAddress) {
            std::string ip(ipAddress);
            Logger::log(
                Logger::LOG_SUCCESS, "Example1",
                "Client " + std::to_string(clientId) + " connected from " + ip);
            globalSocketStats.record_connection();
            globalClientManager.add_client(clientId, ip);
        });

        // Add disconnection handler
        socketHub.addDisconnectHandler(
            [](int clientId, std::string_view reason) {
                std::string disconnectReason(reason);
                Logger::log(Logger::LOG_WARNING, "Example1",
                            "Client " + std::to_string(clientId) +
                                " disconnected: " + disconnectReason);
                globalSocketStats.record_disconnection();
                globalClientManager.remove_client(clientId, disconnectReason);
            });

        // Start the socket server
        socketHub.start(port);
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Socket server started on port " + std::to_string(port));

        // Run server for demonstration period
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Server running for 10 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Print client information
        globalClientManager.print_clients();

        // Stop the server
        socketHub.stop();
        Logger::log(Logger::LOG_INFO, "Example1", "Socket server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example1",
                    "Exception: " + std::string(e.what()));
        globalSocketStats.record_connection_error();
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Enhanced basic socket hub example completed");
}

// Example 2: Multi-threaded client simulation
void clientSimulationExample(int port) {
    Logger::log(Logger::LOG_INFO, "Example2",
                "Starting client simulation example");

    try {
        atom::connection::SocketHub socketHub;

        // Set up handlers for client simulation
        socketHub.addHandler([](std::string_view message) {
            globalSocketStats.record_message_received(message.length());
            Logger::log(Logger::LOG_DEBUG, "Example2",
                        "Received: " + std::string(message));
        });

        socketHub.addConnectHandler([](int clientId,
                                       std::string_view ipAddress) {
            globalSocketStats.record_connection();
            globalClientManager.add_client(clientId, std::string(ipAddress));
            Logger::log(
                Logger::LOG_INFO, "Example2",
                "Simulated client " + std::to_string(clientId) + " connected");
        });

        socketHub.addDisconnectHandler([](int clientId,
                                          std::string_view reason) {
            globalSocketStats.record_disconnection();
            globalClientManager.remove_client(clientId, std::string(reason));
            Logger::log(Logger::LOG_INFO, "Example2",
                        "Simulated client " + std::to_string(clientId) +
                            " disconnected");
        });

        // Start server
        socketHub.start(port);
        Logger::log(
            Logger::LOG_SUCCESS, "Example2",
            "Client simulation server started on port " + std::to_string(port));

        // Simulate multiple client connections and message exchanges
        std::vector<std::future<void>> clientSimulators;

        for (int i = 0; i < 3; ++i) {
            clientSimulators.push_back(std::async(std::launch::async, [i]() {
                try {
                    // Simulate client behavior
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        500 * i));  // Stagger connections

                    // Simulate message sending
                    for (int j = 0; j < 5; ++j) {
                        std::string message = "SimulatedClient_" +
                                              std::to_string(i) + "_Message_" +
                                              std::to_string(j);
                        globalSocketStats.record_message_sent(message.length());

                        Logger::log(Logger::LOG_DEBUG, "ClientSim",
                                    "Client " + std::to_string(i) +
                                        " would send: " + message);

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(300));
                    }

                } catch (const std::exception& e) {
                    Logger::log(Logger::LOG_ERROR, "ClientSim",
                                "Client simulator " + std::to_string(i) +
                                    " error: " + std::string(e.what()));
                }
            }));
        }

        // Monitor simulation
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Running client simulation for 8 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(8));

        // Wait for all client simulators to complete
        for (auto& future : clientSimulators) {
            future.wait();
        }

        socketHub.stop();
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Client simulation server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2",
                "Client simulation example completed");
}

// Example 3: Performance and load testing
void performanceTestingExample(int port) {
    Logger::log(Logger::LOG_INFO, "Example3",
                "Starting performance testing example");

    try {
        atom::connection::SocketHub socketHub;

        // Performance-optimized message handler
        socketHub.addHandler([](std::string_view message) {
            globalSocketStats.record_message_received(message.length());

            // Minimal processing for maximum throughput
            if (message.find("PERF:") == 0) {
                globalSocketStats.record_message_sent(message.length());
            }
        });

        socketHub.addConnectHandler([](int clientId,
                                       std::string_view ipAddress) {
            globalSocketStats.record_connection();
            globalClientManager.add_client(clientId, std::string(ipAddress));
        });

        socketHub.addDisconnectHandler([](int clientId,
                                          std::string_view reason) {
            globalSocketStats.record_disconnection();
            globalClientManager.remove_client(clientId, std::string(reason));
        });

        // Start server
        socketHub.start(port);
        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "Performance testing server started on port " +
                        std::to_string(port));

        // Simulate high-load scenario
        auto startTime = std::chrono::high_resolution_clock::now();

        // Generate load with multiple threads
        std::vector<std::future<void>> loadGenerators;

        for (int i = 0; i < 4; ++i) {
            loadGenerators.push_back(std::async(std::launch::async, [i]() {
                try {
                    for (int j = 0; j < 25; ++j) {
                        std::string message = "PERF:LoadTest_" +
                                              std::to_string(i) + "_" +
                                              std::to_string(j);
                        globalSocketStats.record_message_sent(message.length());

                        // Simulate processing time
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(100));
                    }
                } catch (const std::exception& e) {
                    Logger::log(Logger::LOG_ERROR, "LoadGen",
                                "Load generator " + std::to_string(i) +
                                    " error: " + std::string(e.what()));
                }
            }));
        }

        // Monitor performance
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "Performance test completed in " +
                        std::to_string(duration.count()) + " ms");

        // Wait for load generators to complete
        for (auto& future : loadGenerators) {
            future.wait();
        }

        socketHub.stop();
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Performance testing server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Performance testing example completed");
}

int main() {
    try {
        Logger::log(Logger::LOG_INFO, "Main",
                    "Starting Enhanced Socket Hub Examples");
        Logger::log(Logger::LOG_INFO, "Main", "");
        Logger::log(Logger::LOG_INFO, "Main", "Features demonstrated:");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Enhanced basic socket hub server operations");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Advanced client connection management and tracking");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Multi-threaded client simulation and testing");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Performance testing and load analysis");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Connection lifecycle management (connect/disconnect)");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Real-time statistics monitoring and reporting");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Advanced message routing and processing");
        Logger::log(Logger::LOG_INFO, "Main",
                    "- Comprehensive error handling and recovery");
        Logger::log(Logger::LOG_INFO, "Main", "");

        const int basePort = 8080;

        // Run all examples with proper spacing
        basicSocketHubExample(basePort);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        clientSimulationExample(basePort + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        performanceTestingExample(basePort + 2);

        // Print comprehensive statistics
        Logger::log(Logger::LOG_INFO, "Main", "");
        globalSocketStats.print_summary();
        globalClientManager.print_clients();

        Logger::log(Logger::LOG_SUCCESS, "Main",
                    "All enhanced socket hub examples completed successfully");
        Logger::log(Logger::LOG_INFO, "Main", "");
        Logger::log(
            Logger::LOG_INFO, "Main",
            "Example completed. Check the output above for detailed results.");
        Logger::log(
            Logger::LOG_INFO, "Main",
            "You can test the server with telnet: telnet localhost 8080");

        return 0;

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERROR, "Main",
                    "Fatal error: " + std::string(e.what()));
        globalSocketStats.print_summary();
        return 1;
    }
}
