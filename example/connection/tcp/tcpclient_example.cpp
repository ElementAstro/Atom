/*
 * tcpclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the TcpClient class.
Demonstrates all features including:
- Basic connection and disconnection
- Synchronous send/receive operations
- Asynchronous operations with coroutines
- Callbacks (connect, disconnect, data, error)
- Configuration options
- Connection state management

**************************************************/

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/tcp/tcpclient.hpp"

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

// Example 1: Basic TCP client usagevoid basicTcpClientExample() {
Logger::log(Logger::LOG_INFO, "Example1", "=== Basic TcpClient Usage ===");

try {
    // Create TCP client
    atom::connection::TcpClient client;

    Logger::log(Logger::LOG_INFO, "Example1",
                "Created TcpClient, isConnected: " +
                    std::string(client.isConnected() ? "yes" : "no"));

    // Connect to server (example: HTTP server)
    std::string host = "httpbin.org";
    uint16_t port = 80;

    Logger::log(Logger::LOG_INFO, "Example1",
                "Connecting to " + host + ":" + std::to_string(port));

    bool connected = client.connect(host, port);

    if (connected) {
        Logger::log(Logger::LOG_SUCCESS, "Example1", "Connected successfully");
        Logger::log(
            Logger::LOG_INFO, "Example1",
            "isConnected: " + std::string(client.isConnected() ? "yes" : "no"));

        // Disconnect
        client.disconnect();
        Logger::log(Logger::LOG_INFO, "Example1", "Disconnected");
    } else {
        Logger::log(Logger::LOG_WARNING, "Example1", "Connection failed");
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example1",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example1",
            "Basic TcpClient example completed\n");
}

// Example 2: Custom optionsvoid customOptionsExample() {
Logger::log(Logger::LOG_INFO, "Example2", "=== Custom Options ===");

try {
    // Create options
    atom::connection::TcpClient::Options options;
    options.use_ipv6 = false;
    options.keep_alive = true;
    options.no_delay = true;
    options.send_buffer_size = 65536;
    options.recv_buffer_size = 65536;
    options.connect_timeout = std::chrono::seconds(10);
    options.read_timeout = std::chrono::seconds(30);
    options.write_timeout = std::chrono::seconds(30);

    Logger::log(Logger::LOG_INFO, "Example2", "Options:");
    Logger::log(Logger::LOG_INFO, "Example2",
                "  - IPv6: " + std::string(options.use_ipv6 ? "yes" : "no"));
    Logger::log(
        Logger::LOG_INFO, "Example2",
        "  - Keep alive: " + std::string(options.keep_alive ? "yes" : "no"));
    Logger::log(Logger::LOG_INFO, "Example2",
                "  - No delay (Nagle off): " +
                    std::string(options.no_delay ? "yes" : "no"));
    Logger::log(Logger::LOG_INFO, "Example2",
                "  - Send buffer: " + std::to_string(options.send_buffer_size));
    Logger::log(Logger::LOG_INFO, "Example2",
                "  - Recv buffer: " + std::to_string(options.recv_buffer_size));

    // Create client with options
    atom::connection::TcpClient client(options);

    Logger::log(Logger::LOG_SUCCESS, "Example2",
                "Client created with custom options");

    // Connect
    if (client.connect("httpbin.org", 80)) {
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Connected with custom options");
        client.disconnect();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example2",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example2", "Custom options example completed\n");
}

// Example 3: Synchronous send/receivevoid syncSendReceiveExample() {
Logger::log(Logger::LOG_INFO, "Example3", "=== Synchronous Send/Receive ===");

try {
    atom::connection::TcpClient client;

    if (client.connect("httpbin.org", 80)) {
        Logger::log(Logger::LOG_SUCCESS, "Example3", "Connected");

        // Send HTTP GET request
        std::string request =
            "GET /get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n"
            "\r\n";

        Logger::log(Logger::LOG_INFO, "Example3",
                    "Sending HTTP request (" + std::to_string(request.size()) +
                        " bytes)");

        ssize_t sent = client.send(request);
        if (sent > 0) {
            Logger::log(Logger::LOG_SUCCESS, "Example3",
                        "Sent " + std::to_string(sent) + " bytes");

            // Receive response
            std::string response = client.receive(4096);

            if (!response.empty()) {
                Logger::log(
                    Logger::LOG_SUCCESS, "Example3",
                    "Received " + std::to_string(response.size()) + " bytes");

                // Show first few lines
                size_t pos = 0;
                int lineCount = 0;
                while (pos < response.size() && lineCount < 5) {
                    size_t end = response.find('\n', pos);
                    if (end == std::string::npos)
                        end = response.size();
                    std::string line = response.substr(pos, end - pos);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    Logger::log(Logger::LOG_DEBUG, "Example3", "  " + line);
                    pos = end + 1;
                    lineCount++;
                }
            } else {
                Logger::log(Logger::LOG_WARNING, "Example3",
                            "No response received");
            }
        } else {
            Logger::log(Logger::LOG_WARNING, "Example3", "Send failed");
        }

        client.disconnect();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example3",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example3",
            "Sync send/receive example completed\n");
}

// Example 4: Callbacksvoid callbacksExample() {
Logger::log(Logger::LOG_INFO, "Example4", "=== Callbacks ===");

try {
    atom::connection::TcpClient client;

    // Set connect callback
    client.setConnectCallback([](bool success) {
        Logger::log(success ? Logger::LOG_SUCCESS : Logger::LOG_WARNING,
                    "ConnectCB",
                    success ? "Connection established" : "Connection failed");
    });

    // Set disconnect callback
    client.setDisconnectCallback([]() {
        Logger::log(Logger::LOG_INFO, "DisconnectCB",
                    "Disconnected from server");
    });

    // Set data callback
    client.setDataCallback([](const std::string& data) {
        Logger::log(Logger::LOG_INFO, "DataCB",
                    "Received " + std::to_string(data.size()) + " bytes");
    });

    // Set error callback
    client.setErrorCallback([](const std::string& error) {
        Logger::log(Logger::LOG_ERR, "ErrorCB", "Error: " + error);
    });

    Logger::log(Logger::LOG_INFO, "Example4", "All callbacks registered");

    // Connect (triggers connect callback)
    client.connect("httpbin.org", 80);

    // Send request
    std::string request =
        "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "Connection: close\r\n"
        "\r\n";
    client.send(request);

    // Receive (triggers data callback)
    client.receive(4096);

    // Disconnect (triggers disconnect callback)
    client.disconnect();

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example4",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example4", "Callbacks example completed\n");
}

// Example 5: Async connect with coroutinesvoid asyncConnectExample() {
Logger::log(Logger::LOG_INFO, "Example5", "=== Async Connect (Coroutines) ===");

try {
    atom::connection::TcpClient client;

    Logger::log(Logger::LOG_INFO, "Example5",
                "Note: Coroutine examples require C++20 coroutine support");

    // The connectAsync method returns a Task<bool>
    // In a real application, you would co_await this in a coroutine

    // For demonstration, we'll use the synchronous version
    Logger::log(Logger::LOG_INFO, "Example5",
                "Using synchronous connect for demo");

    if (client.connect("httpbin.org", 80)) {
        Logger::log(Logger::LOG_SUCCESS, "Example5", "Connected");

        // Similarly, sendAsync and receiveAsync return Task types
        // that can be co_awaited in coroutines

        client.disconnect();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example5",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example5", "Async connect example completed\n");
}

// Example 6: Binary data transfervoid binaryDataExample() {
Logger::log(Logger::LOG_INFO, "Example6", "=== Binary Data Transfer ===");

try {
    atom::connection::TcpClient client;

    if (client.connect("httpbin.org", 80)) {
        Logger::log(Logger::LOG_SUCCESS, "Example6", "Connected");

        // Send binary data using vector
        std::vector<char> binaryRequest = {
            'G',  'E',  'T', ' ', '/', 'g',  'e',  't',  ' ',  'H',  'T', 'T',
            'P',  '/',  '1', '.', '1', '\r', '\n', 'H',  'o',  's',  't', ':',
            ' ',  'h',  't', 't', 'p', 'b',  'i',  'n',  '.',  'o',  'r', 'g',
            '\r', '\n', 'C', 'o', 'n', 'n',  'e',  'c',  't',  'i',  'o', 'n',
            ':',  ' ',  'c', 'l', 'o', 's',  'e',  '\r', '\n', '\r', '\n'};

        Logger::log(Logger::LOG_INFO, "Example6",
                    "Sending binary data (" +
                        std::to_string(binaryRequest.size()) + " bytes)");

        ssize_t sent = client.send(binaryRequest.data(), binaryRequest.size());
        if (sent > 0) {
            Logger::log(Logger::LOG_SUCCESS, "Example6",
                        "Sent " + std::to_string(sent) + " bytes");

            // Receive into buffer
            std::vector<char> buffer(4096);
            ssize_t received = client.receive(buffer.data(), buffer.size());

            if (received > 0) {
                Logger::log(Logger::LOG_SUCCESS, "Example6",
                            "Received " + std::to_string(received) + " bytes");
            }
        }

        client.disconnect();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example6",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example6", "Binary data example completed\n");
}

// Example 7: Connection state managementvoid connectionStateExample() {
Logger::log(Logger::LOG_INFO, "Example7",
            "=== Connection State Management ===");

try {
    atom::connection::TcpClient client;

    // Check initial state
    Logger::log(Logger::LOG_INFO, "Example7",
                "Initial state - isConnected: " +
                    std::string(client.isConnected() ? "yes" : "no"));

    // Connect
    client.connect("httpbin.org", 80);
    Logger::log(Logger::LOG_INFO, "Example7",
                "After connect - isConnected: " +
                    std::string(client.isConnected() ? "yes" : "no"));

    // Disconnect
    client.disconnect();
    Logger::log(Logger::LOG_INFO, "Example7",
                "After disconnect - isConnected: " +
                    std::string(client.isConnected() ? "yes" : "no"));

    // Reconnect
    client.connect("httpbin.org", 80);
    Logger::log(Logger::LOG_INFO, "Example7",
                "After reconnect - isConnected: " +
                    std::string(client.isConnected() ? "yes" : "no"));

    client.disconnect();

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example7",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example7",
            "Connection state example completed\n");
}

// Example 8: Move semanticsvoid moveSemanticsExample() {
Logger::log(Logger::LOG_INFO, "Example8", "=== Move Semantics ===");

try {
    // Create and connect client
    atom::connection::TcpClient client1;
    client1.connect("httpbin.org", 80);

    Logger::log(Logger::LOG_INFO, "Example8",
                "client1 connected: " +
                    std::string(client1.isConnected() ? "yes" : "no"));

    // Move to new client
    atom::connection::TcpClient client2 = std::move(client1);

    Logger::log(Logger::LOG_INFO, "Example8", "After move:");
    Logger::log(Logger::LOG_INFO, "Example8",
                "client2 connected: " +
                    std::string(client2.isConnected() ? "yes" : "no"));

    // Use moved client
    if (client2.isConnected()) {
        std::string request =
            "GET /get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n"
            "\r\n";
        client2.send(request);
        std::string response = client2.receive(1024);
        Logger::log(Logger::LOG_SUCCESS, "Example8",
                    "Received " + std::to_string(response.size()) +
                        " bytes from moved client");
        client2.disconnect();
    }

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example8",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example8", "Move semantics example completed\n");
}

// Example 9: Error handlingvoid errorHandlingExample() {
Logger::log(Logger::LOG_INFO, "Example9", "=== Error Handling ===");

// Test invalid host
try {
    Logger::log(Logger::LOG_INFO, "Example9", "Testing invalid host...");
    atom::connection::TcpClient client;
    bool connected =
        client.connect("invalid.host.that.does.not.exist.example", 80);
    Logger::log(Logger::LOG_INFO, "Example9",
                "Connect result: " +
                    std::string(connected ? "success" : "failed (expected)"));
} catch (const std::exception& e) {
    Logger::log(Logger::LOG_WARNING, "Example9",
                "Exception (expected): " + std::string(e.what()));
}

// Test invalid port
try {
    Logger::log(Logger::LOG_INFO, "Example9", "Testing connection refused...");
    atom::connection::TcpClient client;
    bool connected = client.connect("localhost", 59999);  // Unlikely to be open
    Logger::log(Logger::LOG_INFO, "Example9",
                "Connect result: " +
                    std::string(connected ? "success" : "failed (expected)"));
} catch (const std::exception& e) {
    Logger::log(Logger::LOG_WARNING, "Example9",
                "Exception (expected): " + std::string(e.what()));
}

// Test operations on disconnected client
try {
    Logger::log(Logger::LOG_INFO, "Example9",
                "Testing send when disconnected...");
    atom::connection::TcpClient client;
    ssize_t sent = client.send("test");
    Logger::log(Logger::LOG_INFO, "Example9",
                "Send result: " + std::to_string(sent) + " (expected <= 0)");
} catch (const std::exception& e) {
    Logger::log(Logger::LOG_WARNING, "Example9",
                "Exception (expected): " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example9", "Error handling example completed\n");
}

// Example 10: Complete HTTP client workflowvoid completeWorkflowExample() {
Logger::log(Logger::LOG_INFO, "Example10",
            "=== Complete HTTP Client Workflow ===");

try {
    // Configure client
    atom::connection::TcpClient::Options options;
    options.keep_alive = true;
    options.no_delay = true;
    options.connect_timeout = std::chrono::seconds(10);
    options.read_timeout = std::chrono::seconds(30);

    atom::connection::TcpClient client(options);

    // Set callbacks
    client.setConnectCallback([](bool success) {
        Logger::log(
            Logger::LOG_DEBUG, "Workflow",
            "Connect callback: " + std::string(success ? "success" : "failed"));
    });

    client.setErrorCallback([](const std::string& error) {
        Logger::log(Logger::LOG_ERR, "Workflow", "Error: " + error);
    });

    // Connect
    Logger::log(Logger::LOG_INFO, "Example10", "Step 1: Connecting...");
    if (!client.connect("httpbin.org", 80)) {
        Logger::log(Logger::LOG_ERR, "Example10", "Connection failed");
        return;
    }
    Logger::log(Logger::LOG_SUCCESS, "Example10", "Connected");

    // Send request
    Logger::log(Logger::LOG_INFO, "Example10", "Step 2: Sending request...");
    std::string request =
        "GET /json HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n";

    ssize_t sent = client.send(request);
    Logger::log(Logger::LOG_SUCCESS, "Example10",
                "Sent " + std::to_string(sent) + " bytes");

    // Receive response
    Logger::log(Logger::LOG_INFO, "Example10", "Step 3: Receiving response...");
    std::string fullResponse;
    std::string chunk;

    do {
        chunk = client.receive(4096);
        fullResponse += chunk;
    } while (!chunk.empty());

    Logger::log(
        Logger::LOG_SUCCESS, "Example10",
        "Received total " + std::to_string(fullResponse.size()) + " bytes");

    // Parse response
    Logger::log(Logger::LOG_INFO, "Example10", "Step 4: Parsing response...");
    size_t headerEnd = fullResponse.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        std::string headers = fullResponse.substr(0, headerEnd);
        std::string body = fullResponse.substr(headerEnd + 4);

        // Get status line
        size_t statusEnd = headers.find("\r\n");
        std::string statusLine = headers.substr(0, statusEnd);
        Logger::log(Logger::LOG_INFO, "Example10", "Status: " + statusLine);
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Body size: " + std::to_string(body.size()) + " bytes");
    }

    // Disconnect
    Logger::log(Logger::LOG_INFO, "Example10", "Step 5: Disconnecting...");
    client.disconnect();
    Logger::log(Logger::LOG_SUCCESS, "Example10",
                "Workflow completed successfully");

} catch (const std::exception& e) {
    Logger::log(Logger::LOG_ERR, "Example10",
                "Exception: " + std::string(e.what()));
}

Logger::log(Logger::LOG_INFO, "Example10",
            "Complete workflow example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main", "  TcpClient Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Examples connect to httpbin.org (requires internet)");
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicTcpClientExample();
    customOptionsExample();
    syncSendReceiveExample();
    callbacksExample();
    asyncConnectExample();
    binaryDataExample();
    connectionStateExample();
    moveSemanticsExample();
    errorHandlingExample();
    completeWorkflowExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All TcpClient examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}
