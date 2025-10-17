/*
 * udpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Enhanced comprehensive example usage of the UdpClient class.
Demonstrates advanced UDP communication patterns including:
- Basic UDP communication with enhanced error handling
- Multicast communication with group management
- Broadcast messaging with network discovery
- Socket options configuration and optimization
- Statistics tracking and performance monitoring
- Asynchronous operations and callback handling
- Network topology discovery
- Quality of Service (QoS) patterns
- Load balancing and failover scenarios

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

#include "atom/connection/udpclient.hpp"

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

// Enhanced UDP statistics tracking
class UdpStatistics {
public:
    std::atomic<size_t> packets_sent{0};
    std::atomic<size_t> packets_received{0};
    std::atomic<size_t> bytes_sent{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> multicast_packets{0};
    std::atomic<size_t> broadcast_packets{0};
    std::atomic<size_t> unicast_packets{0};
    std::atomic<size_t> failed_operations{0};
    std::atomic<size_t> successful_operations{0};
    std::chrono::steady_clock::time_point start_time;

    UdpStatistics() : start_time(std::chrono::steady_clock::now()) {}

    void record_send(size_t bytes, bool success, bool is_multicast = false,
                     bool is_broadcast = false) {
        if (success) {
            packets_sent++;
            bytes_sent += bytes;
            successful_operations++;

            if (is_multicast)
                multicast_packets++;
            else if (is_broadcast)
                broadcast_packets++;
            else
                unicast_packets++;
        } else {
            failed_operations++;
        }
    }

    void record_receive(size_t bytes) {
        packets_received++;
        bytes_received += bytes;
    }

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        ExampleLogger::write(ExampleLogger::Level::Info, "UdpStats",
                             "=== UDP Communication Statistics ===");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Runtime: " + std::to_string(seconds) + " seconds");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Packets sent: " + std::to_string(packets_sent.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Packets received: " + std::to_string(packets_received.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Bytes sent: " + std::to_string(bytes_sent.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Bytes received: " + std::to_string(bytes_received.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Unicast packets: " + std::to_string(unicast_packets.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Multicast packets: " + std::to_string(multicast_packets.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Broadcast packets: " + std::to_string(broadcast_packets.load()));
        ExampleLogger::write(ExampleLogger::Level::Info, "UdpStats",
                             "Successful operations: " +
                                 std::to_string(successful_operations.load()));
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Failed operations: " + std::to_string(failed_operations.load()));

        if (seconds > 0) {
            ExampleLogger::write(
                ExampleLogger::Level::Info, "UdpStats",
                "Packets/sec sent: " +
                    std::to_string(packets_sent.load() / seconds));
            ExampleLogger::write(
                ExampleLogger::Level::Info, "UdpStats",
                "Bytes/sec sent: " +
                    std::to_string(bytes_sent.load() / seconds));
        }

        double success_rate =
            (successful_operations.load() + failed_operations.load()) > 0
                ? (double(successful_operations.load()) /
                   (successful_operations.load() + failed_operations.load())) *
                      100.0
                : 0.0;
        ExampleLogger::write(
            ExampleLogger::Level::Info, "UdpStats",
            "Success rate: " + std::to_string(success_rate) + "%");
    }
};

// Network endpoint manager for tracking discovered endpoints
class EndpointManager {
private:
    std::map<std::string, std::chrono::steady_clock::time_point>
        discovered_endpoints_;
    std::mutex endpoints_mutex_;

public:
    void add_endpoint(const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(endpoints_mutex_);
        discovered_endpoints_[endpoint] = std::chrono::steady_clock::now();
    }

    std::vector<std::string> get_active_endpoints(
        std::chrono::seconds timeout = std::chrono::seconds(30)) const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(endpoints_mutex_));
        std::vector<std::string> active;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [endpoint, last_seen] : discovered_endpoints_) {
            if (now - last_seen < timeout) {
                active.push_back(endpoint);
            }
        }
        return active;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(endpoints_mutex_));
        return discovered_endpoints_.size();
    }

    void print_endpoints() const {
        auto active = get_active_endpoints();
        ExampleLogger::write(
            ExampleLogger::Level::Info, "EndpointMgr",
            "Active endpoints (" + std::to_string(active.size()) + "):");
        for (const auto& endpoint : active) {
            ExampleLogger::write(ExampleLogger::Level::Info, "EndpointMgr",
                                 "  - " + endpoint);
        }
    }
};

// Global instances
UdpStatistics globalUdpStats;
EndpointManager globalEndpointManager;

// Enhanced function to handle incoming data with statistics
void onDataReceived(std::span<const char> data,
                    const atom::connection::RemoteEndpoint& endpoint) {
    std::string receivedData(data.begin(), data.end());
    std::string endpointStr =
        endpoint.host + ":" + std::to_string(endpoint.port);

    ExampleLogger::write(ExampleLogger::Level::Info, "UdpClient",
                         "Received: '" + receivedData + "' from " +
                             endpointStr + " (" + std::to_string(data.size()) +
                             " bytes)");

    // Update statistics
    globalUdpStats.record_receive(data.size());

    // Track discovered endpoints
    globalEndpointManager.add_endpoint(endpointStr);
}

// Enhanced function to handle errors with detailed logging
void onError(atom::connection::UdpError error, const std::string& message) {
    ExampleLogger::write(ExampleLogger::Level::Error, "UdpClient",
                         "Error: " + message + " (Code: " +
                             std::to_string(static_cast<int>(error)) + ")");
    globalUdpStats.failed_operations++;
}

// Utility function to generate test data
std::string generateUdpTestData(size_t size,
                                const std::string& prefix = "UDP_DATA") {
    std::string data = prefix + "_";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis('A', 'Z');

    for (size_t i = 0; i < size - prefix.length() - 1; ++i) {
        data += static_cast<char>(dis(gen));
    }
    return data;
}

// Utility function to create optimized socket options
atom::connection::SocketOptions createOptimizedSocketOptions() {
    atom::connection::SocketOptions options;
    options.reuseAddress = true;
    options.reusePort = false;
    options.broadcast = true;
    options.sendBufferSize = 65536;     // 64KB send buffer
    options.receiveBufferSize = 65536;  // 64KB receive buffer
    options.ttl = 64;                   // Time to live
    options.nonBlocking = true;
    options.sendTimeout = std::chrono::milliseconds(1000);
    options.receiveTimeout = std::chrono::milliseconds(2000);
    return options;
}

// Utility function to measure operation latency
template <typename Func>
auto measureUdpLatency(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return std::make_pair(result, duration);
}

// Example 1: Enhanced basic UDP client operations with comprehensive features
void basicUdpClientExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Starting enhanced basic UDP client example");

    // Create UDP client with optimized socket options
    auto options = createOptimizedSocketOptions();
    atom::connection::UdpClient udpClient(8081, options);

    // Set up enhanced callbacks
    udpClient.setOnDataReceivedCallback(onDataReceived);
    udpClient.setOnErrorCallback(onError);

    ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                         "UDP client created with optimized socket options");

    // Start receiving data with enhanced buffer size
    auto receiveResult = udpClient.startReceiving(2048);
    if (!receiveResult.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example1",
                             "Failed to start receiving");
        return;
    }
    ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                         "Started receiving data with 2KB buffer");

    // Send multiple test messages with different sizes and patterns
    std::vector<std::pair<std::string, std::string>> testMessages = {
        {"Basic", "Hello, UDP Server!"},
        {"JSON", R"({"type":"test","message":"UDP JSON test","timestamp":)" +
                     std::to_string(std::time(nullptr)) + "}"},
        {"Small", generateUdpTestData(50, "SMALL")},
        {"Medium", generateUdpTestData(500, "MEDIUM")},
        {"Large", generateUdpTestData(1400, "LARGE")}  // Close to MTU limit
    };

    atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 8080};

    for (const auto& [type, message] : testMessages) {
        auto [sendResult, latency] = measureUdpLatency(
            [&]() { return udpClient.send(endpoint, message); });

        if (sendResult.has_value()) {
            ExampleLogger::write(
                ExampleLogger::Level::Success, "Example1",
                "Sent " + type +
                    " message: " + std::to_string(sendResult.value()) +
                    " bytes (latency: " + std::to_string(latency.count()) +
                    " μs)");
            globalUdpStats.record_send(sendResult.value(), true);
        } else {
            ExampleLogger::write(ExampleLogger::Level::Error, "Example1",
                                 "Failed to send " + type + " message");
            globalUdpStats.record_send(0, false);
        }

        // Small delay between messages
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Test rapid-fire messaging
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Testing rapid-fire UDP messaging (20 packets)");
    auto rapidStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 20; ++i) {
        std::string rapidMsg = "Rapid_" + std::to_string(i);
        auto sendResult = udpClient.send(endpoint, rapidMsg);
        if (sendResult.has_value()) {
            globalUdpStats.record_send(sendResult.value(), true);
        } else {
            globalUdpStats.record_send(0, false);
        }
    }

    auto rapidEnd = std::chrono::high_resolution_clock::now();
    auto rapidDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        rapidEnd - rapidStart);
    ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                         "Rapid-fire completed in " +
                             std::to_string(rapidDuration.count()) + " μs");

    // Wait for responses
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Waiting for server responses...");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Get and display statistics
    auto stats = udpClient.getStatistics();
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Client statistics:");
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Example1",
        "  Packets sent: " + std::to_string(stats.packetsSent));
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Example1",
        "  Packets received: " + std::to_string(stats.packetsReceived));
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "  Bytes sent: " + std::to_string(stats.bytesSent));
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Example1",
        "  Bytes received: " + std::to_string(stats.bytesReceived));
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "  Send errors: " + std::to_string(stats.sendErrors));
    ExampleLogger::write(
        ExampleLogger::Level::Info, "Example1",
        "  Receive errors: " + std::to_string(stats.receiveErrors));

    // Stop receiving data
    udpClient.stopReceiving();
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Enhanced basic UDP client example completed");
}

// Example 2: Broadcast messaging
void broadcastExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Starting broadcast example");

    atom::connection::UdpClient udpClient;

    // Enable broadcast
    atom::connection::SocketOptions options;
    options.broadcast = true;
    atom::connection::UdpClient broadcastClient(8082, options);

    // Send broadcast message
    std::string broadcastMsg = "Broadcast message to all!";
    auto result = broadcastClient.sendBroadcast(8080, broadcastMsg);
    if (result.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Success, "Example2",
                             "Broadcast sent: " + broadcastMsg + " (" +
                                 std::to_string(result.value()) + " bytes)");
    } else {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example2",
                             "Failed to send broadcast");
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Broadcast example completed");
}

// Example 3: Multicast communication
void multicastExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "Starting multicast example");

    atom::connection::UdpClient udpClient;

    // Bind to a port
    auto bindResult = udpClient.bind(8083);
    if (!bindResult.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example3",
                             "Failed to bind for multicast");
        return;
    }

    // Join multicast group
    std::string multicastGroup = "224.0.0.1";
    auto joinResult = udpClient.joinMulticastGroup(multicastGroup);
    if (joinResult.has_value() && joinResult.value()) {
        ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                             "Joined multicast group: " + multicastGroup);

        // Send multicast message
        atom::connection::RemoteEndpoint multicastEndpoint{multicastGroup,
                                                           8080};
        std::string multicastMsg = "Multicast message!";
        auto sendResult = udpClient.send(multicastEndpoint, multicastMsg);
        if (sendResult.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                 "Multicast sent: " + multicastMsg);
        }

        // Leave multicast group
        auto leaveResult = udpClient.leaveMulticastGroup(multicastGroup);
        if (leaveResult.has_value() && leaveResult.value()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                 "Left multicast group");
        }
    } else {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example3",
                             "Failed to join multicast group");
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "Multicast example completed");
}

// Example 4: Multiple endpoint messaging
void multipleEndpointExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                         "Starting multiple endpoint example");

    atom::connection::UdpClient udpClient;

    // Create multiple endpoints
    std::vector<atom::connection::RemoteEndpoint> endpoints = {
        {"127.0.0.1", 8080}, {"127.0.0.1", 8081}, {"127.0.0.1", 8082}};

    std::string message = "Message to multiple endpoints";
    auto result = udpClient.sendMultiple(endpoints, message);
    if (result.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Success, "Example4",
                             "Sent to multiple endpoints (" +
                                 std::to_string(result.value()) +
                                 " bytes total)");
    } else {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example4",
                             "Failed to send to multiple endpoints");
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                         "Multiple endpoint example completed");
}

// Example 5: Synchronous receive with timeout
void synchronousReceiveExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                         "Starting synchronous receive example");

    atom::connection::UdpClient udpClient;

    auto bindResult = udpClient.bind(8084);
    if (!bindResult.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example5",
                             "Failed to bind for sync receive");
        return;
    }

    // Send a message to ourselves for testing
    atom::connection::RemoteEndpoint selfEndpoint{"127.0.0.1", 8084};
    std::string selfMessage = "Self message";
    auto sendResult = udpClient.send(selfEndpoint, selfMessage);
    if (sendResult.has_value()) {
        ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                             "Sent self message for testing");
    }

    // Receive with timeout
    auto receiveResult =
        udpClient.receive(1024, std::chrono::milliseconds(2000));
    if (receiveResult.has_value()) {
        auto [data, endpoint] = receiveResult.value();
        std::string receivedMsg(data.begin(), data.end());
        ExampleLogger::write(ExampleLogger::Level::Success, "Example5",
                             "Received: '" + receivedMsg + "' from " +
                                 endpoint.host + ":" +
                                 std::to_string(endpoint.port));
    } else {
        ExampleLogger::write(ExampleLogger::Level::Warning, "Example5",
                             "No data received within timeout");
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example5",
                         "Synchronous receive example completed");
}

// Example 6: Advanced socket options and performance tuning
void advancedSocketOptionsExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example6",
                         "Starting advanced socket options example");

    try {
        // Create multiple clients with different socket configurations
        std::vector<std::pair<std::string, atom::connection::SocketOptions>>
            configurations = {
                {"Default", atom::connection::SocketOptions{}},
                {"HighPerformance", createOptimizedSocketOptions()},
                {"LowLatency",
                 []() {
                     auto opts = createOptimizedSocketOptions();
                     opts.sendTimeout = std::chrono::milliseconds(100);
                     opts.receiveTimeout = std::chrono::milliseconds(100);
                     return opts;
                 }()},
                {"HighThroughput", []() {
                     auto opts = createOptimizedSocketOptions();
                     opts.sendBufferSize = 131072;     // 128KB
                     opts.receiveBufferSize = 131072;  // 128KB
                     return opts;
                 }()}};

        for (const auto& [name, options] : configurations) {
            ExampleLogger::write(ExampleLogger::Level::Info, "Example6",
                                 "Testing " + name + " configuration");

            atom::connection::UdpClient client(
                8090 + (&configurations[0] - &configurations[0]), options);

            // Test performance with this configuration
            auto start = std::chrono::high_resolution_clock::now();

            atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 8080};
            std::string testData = generateUdpTestData(1000, name);

            for (int i = 0; i < 10; ++i) {
                auto sendResult = client.send(endpoint, testData);
                if (sendResult.has_value()) {
                    globalUdpStats.record_send(sendResult.value(), true);
                } else {
                    globalUdpStats.record_send(0, false);
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            ExampleLogger::write(ExampleLogger::Level::Success, "Example6",
                                 name + " configuration: 10 packets in " +
                                     std::to_string(duration.count()) + " μs");

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example6",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example6",
                         "Advanced socket options example completed");
}

// Example 7: Network discovery and topology mapping
void networkDiscoveryExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example7",
                         "Starting network discovery example");

    try {
        auto options = createOptimizedSocketOptions();
        atom::connection::UdpClient discoveryClient(8095, options);

        // Set up discovery callback
        discoveryClient.setOnDataReceivedCallback(
            [](std::span<const char> data,
               const atom::connection::RemoteEndpoint& endpoint) {
                std::string response(data.begin(), data.end());
                std::string endpointStr =
                    endpoint.host + ":" + std::to_string(endpoint.port);

                if (response.find("DISCOVERY_RESPONSE") != std::string::npos) {
                    ExampleLogger::write(ExampleLogger::Level::Success,
                                         "Discovery",
                                         "Found service at " + endpointStr);
                    globalEndpointManager.add_endpoint(endpointStr);
                }

                globalUdpStats.record_receive(data.size());
            });

        auto startResult = discoveryClient.startReceiving(1024);
        if (!startResult.has_value()) {
            ExampleLogger::write(ExampleLogger::Level::Error, "Example7",
                                 "Failed to start receiving for discovery");
            return;
        }

        // Send discovery broadcasts to common service ports
        std::vector<uint16_t> commonPorts = {8080, 8081, 8082, 8083,
                                             8084, 9000, 9001};
        std::string discoveryMessage = "DISCOVERY_REQUEST:UDP_CLIENT_SCAN";

        ExampleLogger::write(ExampleLogger::Level::Info, "Example7",
                             "Broadcasting discovery requests...");

        for (uint16_t port : commonPorts) {
            auto broadcastResult =
                discoveryClient.sendBroadcast(port, discoveryMessage);
            if (broadcastResult.has_value()) {
                ExampleLogger::write(
                    ExampleLogger::Level::Debug, "Example7",
                    "Sent discovery to port " + std::to_string(port));
                globalUdpStats.record_send(broadcastResult.value(), true, false,
                                           true);
            } else {
                globalUdpStats.record_send(0, false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Wait for responses
        ExampleLogger::write(ExampleLogger::Level::Info, "Example7",
                             "Waiting for discovery responses...");
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Print discovered endpoints
        globalEndpointManager.print_endpoints();

        discoveryClient.stopReceiving();

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example7",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example7",
                         "Network discovery example completed");
}

// Example 8: Load balancing and failover patterns
void loadBalancingExample() {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example8",
                         "Starting load balancing example");

    try {
        atom::connection::UdpClient client;

        // Define multiple server endpoints for load balancing
        std::vector<atom::connection::RemoteEndpoint> servers = {
            {"127.0.0.1", 8080}, {"127.0.0.1", 8081}, {"127.0.0.1", 8082}};

        // Track server response times and availability
        std::map<std::string, std::chrono::microseconds> serverLatencies;
        std::map<std::string, bool> serverAvailability;

        // Initialize server states
        for (const auto& server : servers) {
            std::string serverKey =
                server.host + ":" + std::to_string(server.port);
            serverLatencies[serverKey] = std::chrono::microseconds::max();
            serverAvailability[serverKey] = true;
        }

        // Test server responsiveness
        ExampleLogger::write(ExampleLogger::Level::Info, "Example8",
                             "Testing server responsiveness...");

        for (const auto& server : servers) {
            std::string serverKey =
                server.host + ":" + std::to_string(server.port);
            std::string testMessage = "HEALTH_CHECK:" + serverKey;

            auto [sendResult, latency] = measureUdpLatency(
                [&]() { return client.send(server, testMessage); });

            if (sendResult.has_value()) {
                serverLatencies[serverKey] = latency;
                ExampleLogger::write(ExampleLogger::Level::Success, "Example8",
                                     "Server " + serverKey + " responded in " +
                                         std::to_string(latency.count()) +
                                         " μs");
                globalUdpStats.record_send(sendResult.value(), true);
            } else {
                serverAvailability[serverKey] = false;
                ExampleLogger::write(ExampleLogger::Level::Warning, "Example8",
                                     "Server " + serverKey + " is unavailable");
                globalUdpStats.record_send(0, false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Implement round-robin load balancing
        ExampleLogger::write(ExampleLogger::Level::Info, "Example8",
                             "Implementing round-robin load balancing...");

        size_t serverIndex = 0;
        for (int i = 0; i < 15; ++i) {
            // Find next available server
            size_t attempts = 0;
            while (attempts < servers.size()) {
                const auto& server = servers[serverIndex];
                std::string serverKey =
                    server.host + ":" + std::to_string(server.port);

                if (serverAvailability[serverKey]) {
                    std::string message =
                        "LoadBalanced_Message_" + std::to_string(i + 1);
                    auto sendResult = client.send(server, message);

                    if (sendResult.has_value()) {
                        ExampleLogger::write(
                            ExampleLogger::Level::Success, "Example8",
                            "Sent message " + std::to_string(i + 1) +
                                " to server " + serverKey);
                        globalUdpStats.record_send(sendResult.value(), true);
                        break;
                    } else {
                        ExampleLogger::write(ExampleLogger::Level::Warning,
                                             "Example8",
                                             "Failed to send to " + serverKey +
                                                 ", trying next server");
                        serverAvailability[serverKey] = false;
                    }
                }

                serverIndex = (serverIndex + 1) % servers.size();
                attempts++;
            }

            if (attempts >= servers.size()) {
                ExampleLogger::write(ExampleLogger::Level::Error, "Example8",
                                     "All servers unavailable for message " +
                                         std::to_string(i + 1));
            }

            serverIndex = (serverIndex + 1) % servers.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Example8",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example8",
                         "Load balancing example completed");
}

int main() {
    try {
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "Starting Enhanced Comprehensive UDP Client Examples");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "Features demonstrated:");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "- Enhanced basic UDP communication with performance monitoring");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "- Advanced broadcast messaging with network discovery");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Comprehensive multicast group management");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Multiple endpoint communication patterns");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Synchronous receive operations with timeouts");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "- Advanced socket options and performance tuning");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Network discovery and topology mapping");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Load balancing and failover patterns");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Comprehensive statistics tracking");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "- Error handling and recovery mechanisms");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");

        // Run all examples with proper spacing
        basicUdpClientExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        broadcastExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        multicastExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        multipleEndpointExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        synchronousReceiveExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        advancedSocketOptionsExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        networkDiscoveryExample();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        loadBalancingExample();

        // Print comprehensive statistics
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
        globalUdpStats.print_summary();
        globalEndpointManager.print_endpoints();

        ExampleLogger::write(
            ExampleLogger::Level::Success, "Main",
            "All enhanced UDP client examples completed successfully");
        ExampleLogger::write(ExampleLogger::Level::Info, "Main", "");
        ExampleLogger::write(
            ExampleLogger::Level::Info, "Main",
            "Example completed. Check the output above for detailed results.");
        return 0;

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::Level::Error, "Main",
                             "Exception: " + std::string(e.what()));
        globalUdpStats.print_summary();
        return 1;
    }
}
