/*
 * udpclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the UdpClient class.
Demonstrates all features including:
- Basic UDP send/receive
- Binding to local port
- Sending to specific endpoints
- Broadcasting
- Multicasting
- Socket options
- Statistics tracking
- Callbacks

**************************************************/

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/udp/udpclient.hpp"

namespace {

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

}  // namespace

std::string udpErrorToString(atom::connection::UdpError error) {
    switch (error) {
        case atom::connection::UdpError::None:
            return "None";
        case atom::connection::UdpError::SocketCreationFailed:
            return "SocketCreationFailed";
        case atom::connection::UdpError::BindFailed:
            return "BindFailed";
        case atom::connection::UdpError::SendFailed:
            return "SendFailed";
        case atom::connection::UdpError::ReceiveFailed:
            return "ReceiveFailed";
        case atom::connection::UdpError::HostNotFound:
            return "HostNotFound";
        case atom::connection::UdpError::Timeout:
            return "Timeout";
        case atom::connection::UdpError::InvalidParameter:
            return "InvalidParameter";
        case atom::connection::UdpError::InternalError:
            return "InternalError";
        case atom::connection::UdpError::MulticastError:
            return "MulticastError";
        case atom::connection::UdpError::BroadcastError:
            return "BroadcastError";
        case atom::connection::UdpError::NotInitialized:
            return "NotInitialized";
        case atom::connection::UdpError::NotSupported:
            return "NotSupported";
        default:
            return "Unknown";
    }
}

// Example 1: Basic UDP client usagevoid basicUdpClientExample() {
Logger::log(Logger::LOG_INFO, "Example1", "=== Basic UdpClient Usage ===");

try {
    // Create UDP client
    atom::connection::UdpClient client;

    Logger::log(Logger::LOG_INFO, "Example1", "Created UdpClient");

    // Send data to a server using RemoteEndpoint
    atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 12345};
    std::string message = "Hello UDP Server!";

    Logger::log(
        Logger::LOG_INFO, "Example1",
        "Sending to " + endpoint.host + ":" + std::to_string(endpoint.port));

    auto result = client.send(endpoint, message);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Sent " + std::to_string(result.value()) + " bytes");
    } else {
        Logger::log(Logger::LOG_WARNING, "Example1", "Send failed");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example1",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example1",
            "Basic UdpClient example completed\n");
}

// Example 2: Binding to local portvoid bindingExample() {
Logger::log(Logger::LOG_INFO, "Example2", "=== Binding to Local Port ===");

try {
    atom::connection::UdpClient client;

    // Bind to specific port
    uint16_t localPort = 54321;
    auto bindResult = client.bind(localPort);

    if (bindResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Bound to port " + std::to_string(localPort));

        // Check if bound
        Logger::log(Logger::LOG_INFO, "Example2",
                    "isBound: " + std::string(client.isBound() ? "yes" : "no"));

        // Get local port
        auto portResult = client.getLocalPort();
        if (portResult.has_value()) {
            Logger::log(Logger::LOG_INFO, "Example2",
                        "Local port: " + std::to_string(portResult.value()));
        }

        // Now we can receive on this port
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Waiting for data (1 second timeout)...");

        auto recvResult = client.receive(1024, std::chrono::milliseconds(1000));
        if (recvResult.has_value()) {
            auto& [data, endpoint] = recvResult.value();
            std::string dataStr(data.begin(), data.end());
            Logger::log(Logger::LOG_SUCCESS, "Example2",
                        "Received from " + endpoint.host + ":" +
                            std::to_string(endpoint.port) + ": " + dataStr);
        } else {
            Logger::log(Logger::LOG_INFO, "Example2",
                        "No data received (timeout expected)");
        }
    } else {
        Logger::log(Logger::LOG_WARNING, "Example2", "Bind failed");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example2",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example2", "Binding example completed\n");
}

// Example 3: Sending to multiple endpointsvoid multipleEndpointsExample() {
Logger::log(Logger::LOG_INFO, "Example3", "=== Multiple Endpoints ===");

try {
    atom::connection::UdpClient client;

    // Define multiple endpoints
    std::vector<atom::connection::RemoteEndpoint> endpoints = {
        {"127.0.0.1", 12345}, {"127.0.0.1", 12346}, {"127.0.0.1", 12347}};

    std::string message = "Broadcast to multiple endpoints";

    // Send to each endpoint individually
    for (const auto& endpoint : endpoints) {
        auto result = client.send(endpoint, message);
        if (result.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example3",
                        "Sent to " + endpoint.host + ":" +
                            std::to_string(endpoint.port) + " (" +
                            std::to_string(result.value()) + " bytes)");
        } else {
            Logger::log(Logger::LOG_WARNING, "Example3",
                        "Failed to send to " + endpoint.host + ":" +
                            std::to_string(endpoint.port));
        }
    }

    // Use sendMultiple for batch sending
    Logger::log(Logger::LOG_INFO, "Example3", "Using sendMultiple...");
    auto batchResult = client.sendMultiple(
        endpoints, std::span<const char>(message.data(), message.size()));
    if (batchResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "Batch sent to " + std::to_string(batchResult.value()) +
                        " endpoints");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example3",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example3",
            "Multiple endpoints example completed\n");
}

// Example 4: Broadcastingvoid broadcastExample() {
Logger::log(Logger::LOG_INFO, "Example4", "=== Broadcasting ===");

try {
    // Create client with broadcast enabled via socket options
    atom::connection::SocketOptions options;
    options.broadcast = true;

    atom::connection::UdpClient client;
    auto optResult = client.setSocketOptions(options);

    if (optResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example4",
                    "Broadcast enabled via socket options");

        // Send broadcast message
        uint16_t port = 12345;
        std::string message = "UDP Broadcast Message!";

        auto sendResult = client.sendBroadcast(port, message);
        if (sendResult.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example4",
                        "Broadcast sent to port " + std::to_string(port) +
                            " (" + std::to_string(sendResult.value()) +
                            " bytes)");
        } else {
            Logger::log(Logger::LOG_WARNING, "Example4", "Broadcast failed");
        }
    } else {
        Logger::log(Logger::LOG_WARNING, "Example4",
                    "Failed to set socket options");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example4",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example4", "Broadcast example completed\n");
}

// Example 5: Multicastingvoid multicastExample() {
Logger::log(Logger::LOG_INFO, "Example5", "=== Multicasting ===");

try {
    atom::connection::UdpClient client;

    // Multicast group address (224.0.0.0 - 239.255.255.255)
    std::string multicastGroup = "239.255.0.1";
    uint16_t port = 12345;

    // Join multicast group
    auto joinResult = client.joinMulticastGroup(multicastGroup);
    if (joinResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example5",
                    "Joined multicast group: " + multicastGroup);

        // Send to multicast group
        std::string message = "Multicast message!";
        auto sendResult = client.sendToMulticastGroup(
            multicastGroup, port,
            std::span<const char>(message.data(), message.size()));
        if (sendResult.has_value()) {
            Logger::log(Logger::LOG_SUCCESS, "Example5",
                        "Sent to multicast group (" +
                            std::to_string(sendResult.value()) + " bytes)");
        } else {
            Logger::log(Logger::LOG_WARNING, "Example5",
                        "Send to multicast failed");
        }

        // Leave multicast group
        auto leaveResult = client.leaveMulticastGroup(multicastGroup);
        if (leaveResult.has_value()) {
            Logger::log(Logger::LOG_INFO, "Example5",
                        "Left multicast group: " + multicastGroup);
        }
    } else {
        Logger::log(Logger::LOG_WARNING, "Example5",
                    "Failed to join multicast group");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example5",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example5", "Multicast example completed\n");
}

// Example 6: Socket optionsvoid socketOptionsExample() {
Logger::log(Logger::LOG_INFO, "Example6", "=== Socket Options ===");

try {
    atom::connection::UdpClient client;

    // Create socket options
    atom::connection::SocketOptions options;
    options.reuseAddress = true;
    options.reusePort = false;
    options.broadcast = false;
    options.sendBufferSize = 65536;
    options.receiveBufferSize = 65536;
    options.ttl = 64;
    options.nonBlocking = true;
    options.sendTimeout = std::chrono::milliseconds(5000);
    options.receiveTimeout = std::chrono::milliseconds(5000);

    Logger::log(Logger::LOG_INFO, "Example6", "Socket Options:");
    Logger::log(Logger::LOG_INFO, "Example6",
                "  - Reuse address: " +
                    std::string(options.reuseAddress ? "yes" : "no"));
    Logger::log(
        Logger::LOG_INFO, "Example6",
        "  - Reuse port: " + std::string(options.reusePort ? "yes" : "no"));
    Logger::log(
        Logger::LOG_INFO, "Example6",
        "  - Broadcast: " + std::string(options.broadcast ? "yes" : "no"));
    Logger::log(Logger::LOG_INFO, "Example6",
                "  - Send buffer: " + std::to_string(options.sendBufferSize));
    Logger::log(
        Logger::LOG_INFO, "Example6",
        "  - Recv buffer: " + std::to_string(options.receiveBufferSize));
    Logger::log(Logger::LOG_INFO, "Example6",
                "  - TTL: " + std::to_string(options.ttl));
    Logger::log(
        Logger::LOG_INFO, "Example6",
        "  - Non-blocking: " + std::string(options.nonBlocking ? "yes" : "no"));

    // Apply options
    auto result = client.setSocketOptions(options);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example6",
                    "Socket options applied successfully");
    } else {
        Logger::log(Logger::LOG_WARNING, "Example6",
                    "Failed to apply socket options");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example6",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example6", "Socket options example completed\n");
}

// Example 7: Statistics trackingvoid statisticsExample() {
Logger::log(Logger::LOG_INFO, "Example7", "=== Statistics Tracking ===");

try {
    atom::connection::UdpClient client;

    // Send some messages
    atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 12345};
    for (int i = 0; i < 5; ++i) {
        std::string msg = "Test message " + std::to_string(i + 1);
        (void)client.send(endpoint, msg);
    }

    // Get statistics
    auto stats = client.getStatistics();

    Logger::log(Logger::LOG_INFO, "Example7", "=== UDP Client Statistics ===");
    Logger::log(Logger::LOG_INFO, "Example7",
                "Packets sent: " + std::to_string(stats.packetsSent));
    Logger::log(Logger::LOG_INFO, "Example7",
                "Packets received: " + std::to_string(stats.packetsReceived));
    Logger::log(Logger::LOG_INFO, "Example7",
                "Bytes sent: " + std::to_string(stats.bytesSent));
    Logger::log(Logger::LOG_INFO, "Example7",
                "Bytes received: " + std::to_string(stats.bytesReceived));
    Logger::log(Logger::LOG_INFO, "Example7",
                "Send errors: " + std::to_string(stats.sendErrors));
    Logger::log(Logger::LOG_INFO, "Example7",
                "Receive errors: " + std::to_string(stats.receiveErrors));

    // Reset statistics
    client.resetStatistics();
    Logger::log(Logger::LOG_INFO, "Example7", "Statistics reset");

    // Verify reset
    auto resetStats = client.getStatistics();
    Logger::log(Logger::LOG_INFO, "Example7",
                "After reset - Packets sent: " +
                    std::to_string(resetStats.packetsSent));

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example7",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example7", "Statistics example completed\n");
}

// Example 8: Callbacksvoid callbacksExample() {
Logger::log(Logger::LOG_INFO, "Example8", "=== Callbacks ===");

try {
    atom::connection::UdpClient client;

    // Set data received callback
    client.setOnDataReceivedCallback(
        [](std::span<const char> data,
           const atom::connection::RemoteEndpoint& endpoint) {
            Logger::log(Logger::LOG_INFO, "DataCB",
                        "Received " + std::to_string(data.size()) +
                            " bytes from " + endpoint.host + ":" +
                            std::to_string(endpoint.port));
        });

    // Set error callback
    client.setOnErrorCallback(
        [](atom::connection::UdpError error, const std::string& message) {
            Logger::log(Logger::LOG_ERR, "ErrorCB",
                        udpErrorToString(error) + ": " + message);
        });

    // Set status change callback
    client.setOnStatusChangeCallback([](bool status) {
        Logger::log(
            Logger::LOG_INFO, "StatusCB",
            "Status changed: " + std::string(status ? "active" : "inactive"));
    });

    Logger::log(Logger::LOG_INFO, "Example8", "Callbacks registered");

    // Start receiving to trigger callbacks
    auto startResult = client.startReceiving(4096);
    if (startResult.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example8", "Started receiving");
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "isReceiving: " + std::string(client.isReceiving() ? "yes" : "no"));

        // Wait briefly
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Stop receiving
        client.stopReceiving();
        Logger::log(Logger::LOG_INFO, "Example8", "Stopped receiving");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example8",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example8", "Callbacks example completed\n");
}

// Example 9: Binary data with spanvoid binaryDataExample() {
Logger::log(Logger::LOG_INFO, "Example9", "=== Binary Data ===");

try {
    atom::connection::UdpClient client;
    atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 12345};

    // Send binary data using span
    std::vector<char> binaryData = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                                    0x0C, 0x0D, 0x0E, 0x0F};

    Logger::log(Logger::LOG_INFO, "Example9",
                "Sending " + std::to_string(binaryData.size()) +
                    " bytes of binary data");

    auto result = client.send(endpoint, std::span<const char>(binaryData));
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example9",
                    "Sent " + std::to_string(result.value()) + " bytes");
    }

    // Send using array
    std::array<char, 8> arrayData = {'U', 'D', 'P', ' ', 'D', 'A', 'T', 'A'};
    result = client.send(
        endpoint, std::span<const char>(arrayData.data(), arrayData.size()));
    if (result.has_value()) {
        Logger::log(
            Logger::LOG_SUCCESS, "Example9",
            "Sent array data: " + std::to_string(result.value()) + " bytes");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example9",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example9", "Binary data example completed\n");
}

// Example 10: Constructor with port and optionsvoid constructorOptionsExample()
// {
Logger::log(Logger::LOG_INFO, "Example10",
            "=== Constructor with Port and Options ===");

try {
    // Create with just port
    atom::connection::UdpClient client1(54323);
    Logger::log(Logger::LOG_SUCCESS, "Example10",
                "Created client1 bound to port 54323");
    Logger::log(
        Logger::LOG_INFO, "Example10",
        "client1 isBound: " + std::string(client1.isBound() ? "yes" : "no"));

    // Create with port and options
    atom::connection::SocketOptions options;
    options.reuseAddress = true;
    options.receiveBufferSize = 32768;

    atom::connection::UdpClient client2(54324, options);
    Logger::log(Logger::LOG_SUCCESS, "Example10",
                "Created client2 bound to port 54324 with options");

    // Check IPv6 support
    bool ipv6Supported = atom::connection::UdpClient::isIPv6Supported();
    Logger::log(Logger::LOG_INFO, "Example10",
                "IPv6 supported: " + std::string(ipv6Supported ? "yes" : "no"));

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example10",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example10",
            "Constructor options example completed\n");
}

// Example 11: Move semanticsvoid moveSemanticsExample() {
Logger::log(Logger::LOG_INFO, "Example11", "=== Move Semantics ===");

try {
    // Create and configure client
    atom::connection::UdpClient client1;
    (void)client1.bind(54325);

    Logger::log(Logger::LOG_INFO, "Example11",
                "client1 created and bound, isBound: " +
                    std::string(client1.isBound() ? "yes" : "no"));

    // Move to new client
    atom::connection::UdpClient client2 = std::move(client1);

    Logger::log(Logger::LOG_INFO, "Example11", "Moved to client2");
    Logger::log(
        Logger::LOG_INFO, "Example11",
        "client2 isBound: " + std::string(client2.isBound() ? "yes" : "no"));

    // Use moved client
    atom::connection::RemoteEndpoint endpoint{"127.0.0.1", 12345};
    std::string msg = "Message from moved client";
    auto result = client2.send(endpoint, msg);
    if (result.has_value()) {
        Logger::log(Logger::LOG_SUCCESS, "Example11",
                    "Sent from moved client: " +
                        std::to_string(result.value()) + " bytes");
    }

    // Close
    client2.close();
    Logger::log(Logger::LOG_INFO, "Example11", "Client closed");

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
    Logger::log(Logger::LOG_INFO, "Main", "  UdpClient Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Some examples require a UDP server running");
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicUdpClientExample();
    bindingExample();
    multipleEndpointsExample();
    broadcastExample();
    multicastExample();
    socketOptionsExample();
    statisticsExample();
    callbacksExample();
    binaryDataExample();
    constructorOptionsExample();
    moveSemanticsExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All UdpClient examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
