/*
 * async_tcpclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the async TcpClient class.
Demonstrates all features including:
- Basic async connection
- SSL/TLS configuration
- Proxy support
- Reconnection handling
- Heartbeat mechanism
- Request-response pattern
- Statistics tracking
- Various callbacks

**************************************************/

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/tcp/async_tcpclient.hpp"

namespace {

class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

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

std::string stateToString(atom::async::connection::ConnectionState state) {
    switch (state) {
        case atom::async::connection::ConnectionState::Disconnected:
            return "Disconnected";
        case atom::async::connection::ConnectionState::Connecting:
            return "Connecting";
        case atom::async::connection::ConnectionState::Connected:
            return "Connected";
        case atom::async::connection::ConnectionState::Reconnecting:
            return "Reconnecting";
        case atom::async::connection::ConnectionState::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

}  // namespace

// Example 1: Basic async TCP client
void basicAsyncTcpClientExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic Async TcpClient ===");

    try {
        // Create async TCP client
        atom::async::connection::TcpClient client;

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Created async TcpClient, state: " +
                    stateToString(client.getState()));

        // Connect
        client.connect("httpbin.org", 80);

        // Wait for connection
        std::this_thread::sleep_for(std::chrono::seconds(2));

        Logger::log(Logger::LOG_INFO, "Example1",
                    "After connect, state: " + stateToString(client.getState()));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example1", "Connected successfully");
            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1", "Basic async TcpClient example completed\n");
}

// Example 2: Custom configuration
void customConfigExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Custom Configuration ===");

    try {
        // Create custom configuration
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 80;
        config.use_ssl = false;
        config.connect_timeout = std::chrono::seconds(10);
        config.read_timeout = std::chrono::seconds(30);
        config.write_timeout = std::chrono::seconds(30);
        config.keep_alive = true;
        config.no_delay = true;
        config.auto_reconnect = true;
        config.max_reconnect_attempts = 5;
        config.reconnect_delay = std::chrono::seconds(2);
        config.enable_heartbeat = false;

        Logger::log(Logger::LOG_INFO, "Example2", "Configuration:");
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Host: " + config.host);
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Port: " + std::to_string(config.port));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - SSL: " + std::string(config.use_ssl ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Keep alive: " + std::string(config.keep_alive ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example2",
                    "  - Auto reconnect: " + std::string(config.auto_reconnect ? "yes" : "no"));

        // Create client with config
        atom::async::connection::TcpClient client(config);

        Logger::log(Logger::LOG_SUCCESS, "Example2", "Client created with custom config");

        // Connect using config
        client.connect();

        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example2", "Connected");
            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2", "Custom configuration example completed\n");
}

// Example 3: SSL/TLS connection
void sslConnectionExample() {
    Logger::log(Logger::LOG_INFO, "Example3", "=== SSL/TLS Connection ===");

    try {
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 443;
        config.use_ssl = true;
        config.verify_ssl = true;
        // config.ssl_cert_file = "/path/to/cert.pem";
        // config.ssl_key_file = "/path/to/key.pem";
        // config.ssl_ca_file = "/path/to/ca.pem";

        Logger::log(Logger::LOG_INFO, "Example3", "SSL Configuration:");
        Logger::log(Logger::LOG_INFO, "Example3",
                    "  - SSL enabled: " + std::string(config.use_ssl ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example3",
                    "  - Verify SSL: " + std::string(config.verify_ssl ? "yes" : "no"));

        atom::async::connection::TcpClient client(config);

        client.connect();
        std::this_thread::sleep_for(std::chrono::seconds(3));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example3", "SSL connection established");

            // Send HTTPS request
            std::string request =
                "GET /get HTTP/1.1\r\n"
                "Host: httpbin.org\r\n"
                "Connection: close\r\n"
                "\r\n";

            client.send(request);
            std::this_thread::sleep_for(std::chrono::seconds(1));

            client.disconnect();
        } else {
            Logger::log(Logger::LOG_WARNING, "Example3", "SSL connection failed");
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3", "SSL connection example completed\n");
}

// Example 4: Proxy support
void proxyExample() {
    Logger::log(Logger::LOG_INFO, "Example4", "=== Proxy Support ===");

    try {
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 80;

        // Configure proxy
        atom::async::connection::ProxyConfig proxyConfig;
        proxyConfig.enabled = false;  // Set to true if you have a proxy
        proxyConfig.host = "proxy.example.com";
        proxyConfig.port = 8080;
        proxyConfig.type = atom::async::connection::ProxyType::HTTP;
        // proxyConfig.username = "user";
        // proxyConfig.password = "pass";

        config.proxy = proxyConfig;

        Logger::log(Logger::LOG_INFO, "Example4", "Proxy Configuration:");
        Logger::log(Logger::LOG_INFO, "Example4",
                    "  - Enabled: " + std::string(proxyConfig.enabled ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example4",
                    "  - Host: " + proxyConfig.host);
        Logger::log(Logger::LOG_INFO, "Example4",
                    "  - Port: " + std::to_string(proxyConfig.port));

        atom::async::connection::TcpClient client(config);

        if (!proxyConfig.enabled) {
            Logger::log(Logger::LOG_INFO, "Example4",
                        "Proxy disabled - connecting directly");
        }

        client.connect();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example4", "Connected");
            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4", "Proxy example completed\n");
}

// Example 5: Callbacks
void callbacksExample() {
    Logger::log(Logger::LOG_INFO, "Example5", "=== Callbacks ===");

    try {
        atom::async::connection::TcpClient client;

        // Set state change callback
        client.setStateCallback([](atom::async::connection::ConnectionState state) {
            Logger::log(Logger::LOG_INFO, "StateCB",
                        "State changed to: " + stateToString(state));
        });

        // Set connect callback
        client.setConnectCallback([](bool success, const std::string& message) {
            Logger::log(success ? Logger::LOG_SUCCESS : Logger::LOG_WARNING,
                        "ConnectCB",
                        message);
        });

        // Set disconnect callback
        client.setDisconnectCallback([](const std::string& reason) {
            Logger::log(Logger::LOG_INFO, "DisconnectCB",
                        "Disconnected: " + reason);
        });

        // Set data callback
        client.setDataCallback([](const std::string& data) {
            Logger::log(Logger::LOG_INFO, "DataCB",
                        "Received " + std::to_string(data.size()) + " bytes");
        });

        // Set error callback
        client.setErrorCallback([](const std::string& error) {
            Logger::log(Logger::LOG_ERR, "ErrorCB", error);
        });

        Logger::log(Logger::LOG_INFO, "Example5", "All callbacks registered");

        // Connect and observe callbacks
        client.connect("httpbin.org", 80);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            // Send request to trigger data callback
            std::string request =
                "GET /get HTTP/1.1\r\n"
                "Host: httpbin.org\r\n"
                "Connection: close\r\n"
                "\r\n";
            client.send(request);

            std::this_thread::sleep_for(std::chrono::seconds(2));
            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5", "Callbacks example completed\n");
}

// Example 6: Auto reconnection
void autoReconnectExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Auto Reconnection ===");

    try {
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 80;
        config.auto_reconnect = true;
        config.max_reconnect_attempts = 3;
        config.reconnect_delay = std::chrono::seconds(1);

        atom::async::connection::TcpClient client(config);

        std::atomic<int> reconnectCount{0};

        client.setStateCallback([&reconnectCount](atom::async::connection::ConnectionState state) {
            if (state == atom::async::connection::ConnectionState::Reconnecting) {
                reconnectCount++;
                Logger::log(Logger::LOG_INFO, "Reconnect",
                            "Reconnection attempt #" + std::to_string(reconnectCount.load()));
            }
        });

        client.connect();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example6", "Connected");

            // Simulate disconnect (in real scenario, this would be network issue)
            Logger::log(Logger::LOG_INFO, "Example6",
                        "Auto-reconnect is configured with " +
                        std::to_string(config.max_reconnect_attempts) + " max attempts");

            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6", "Auto reconnection example completed\n");
}

// Example 7: Heartbeat mechanism
void heartbeatExample() {
    Logger::log(Logger::LOG_INFO, "Example7", "=== Heartbeat Mechanism ===");

    try {
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 80;
        config.enable_heartbeat = true;
        config.heartbeat_interval = std::chrono::seconds(5);
        config.heartbeat_timeout = std::chrono::seconds(15);

        Logger::log(Logger::LOG_INFO, "Example7", "Heartbeat Configuration:");
        Logger::log(Logger::LOG_INFO, "Example7",
                    "  - Enabled: " + std::string(config.enable_heartbeat ? "yes" : "no"));
        Logger::log(Logger::LOG_INFO, "Example7",
                    "  - Interval: 5 seconds");
        Logger::log(Logger::LOG_INFO, "Example7",
                    "  - Timeout: 15 seconds");

        atom::async::connection::TcpClient client(config);

        // Set heartbeat callback
        client.setHeartbeatCallback([](bool success) {
            Logger::log(success ? Logger::LOG_DEBUG : Logger::LOG_WARNING,
                        "Heartbeat",
                        success ? "Heartbeat OK" : "Heartbeat failed");
        });

        client.connect();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example7", "Connected with heartbeat enabled");

            // Let heartbeat run for a while
            Logger::log(Logger::LOG_INFO, "Example7", "Running for 10 seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(10));

            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7", "Heartbeat example completed\n");
}

// Example 8: Request-response pattern
void requestResponseExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Request-Response Pattern ===");

    try {
        atom::async::connection::TcpClient client;

        client.connect("httpbin.org", 80);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example8", "Connected");

            // Send request and wait for response
            std::string request =
                "GET /json HTTP/1.1\r\n"
                "Host: httpbin.org\r\n"
                "Connection: close\r\n"
                "\r\n";

            Logger::log(Logger::LOG_INFO, "Example8", "Sending request...");

            // Use sendAndReceive for request-response pattern
            auto response = client.sendAndReceive(request, std::chrono::seconds(10));

            if (response.has_value()) {
                Logger::log(Logger::LOG_SUCCESS, "Example8",
                            "Received response: " + std::to_string(response->size()) + " bytes");

                // Show first line
                size_t lineEnd = response->find('\n');
                if (lineEnd != std::string::npos) {
                    Logger::log(Logger::LOG_INFO, "Example8",
                                "First line: " + response->substr(0, lineEnd));
                }
            } else {
                Logger::log(Logger::LOG_WARNING, "Example8", "No response received");
            }

            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Request-response example completed\n");
}

// Example 9: Statistics tracking
void statisticsExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Statistics Tracking ===");

    try {
        atom::async::connection::TcpClient client;

        client.connect("httpbin.org", 80);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (client.isConnected()) {
            Logger::log(Logger::LOG_SUCCESS, "Example9", "Connected");

            // Send some requests
            for (int i = 0; i < 3; ++i) {
                std::string request =
                    "GET /get HTTP/1.1\r\n"
                    "Host: httpbin.org\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n";
                client.send(request);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            // Get statistics
            auto stats = client.getStatistics();

            Logger::log(Logger::LOG_INFO, "Example9", "=== Connection Statistics ===");
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Bytes sent: " + std::to_string(stats.bytes_sent));
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Bytes received: " + std::to_string(stats.bytes_received));
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Messages sent: " + std::to_string(stats.messages_sent));
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Messages received: " + std::to_string(stats.messages_received));
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Reconnect count: " + std::to_string(stats.reconnect_count));

            auto connectedDuration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - stats.connected_since);
            Logger::log(Logger::LOG_INFO, "Example9",
                        "Connected for: " + std::to_string(connectedDuration.count()) + " seconds");

            // Reset statistics
            client.resetStatistics();
            Logger::log(Logger::LOG_INFO, "Example9", "Statistics reset");

            client.disconnect();
        }

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9", "Statistics example completed\n");
}

// Example 10: Complete async workflow
void completeWorkflowExample() {
    Logger::log(Logger::LOG_INFO, "Example10", "=== Complete Async Workflow ===");

    try {
        // Configure client
        atom::async::connection::ClientConfig config;
        config.host = "httpbin.org";
        config.port = 80;
        config.connect_timeout = std::chrono::seconds(10);
        config.read_timeout = std::chrono::seconds(30);
        config.keep_alive = true;
        config.auto_reconnect = true;
        config.max_reconnect_attempts = 3;

        atom::async::connection::TcpClient client(config);

        // Setup callbacks
        std::atomic<bool> dataReceived{false};

        client.setStateCallback([](atom::async::connection::ConnectionState state) {
            Logger::log(Logger::LOG_DEBUG, "Workflow",
                        "State: " + stateToString(state));
        });

        client.setDataCallback([&dataReceived](const std::string& data) {
            Logger::log(Logger::LOG_INFO, "Workflow",
                        "Data received: " + std::to_string(data.size()) + " bytes");
            dataReceived = true;
        });

        client.setErrorCallback([](const std::string& error) {
            Logger::log(Logger::LOG_ERR, "Workflow", "Error: " + error);
        });

        // Connect
        Logger::log(Logger::LOG_INFO, "Example10", "Step 1: Connecting...");
        client.connect();

        // Wait for connection
        int waitCount = 0;
        while (!client.isConnected() && waitCount < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waitCount++;
        }

        if (!client.isConnected()) {
            Logger::log(Logger::LOG_ERR, "Example10", "Connection timeout");
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

        client.send(request);
        Logger::log(Logger::LOG_SUCCESS, "Example10", "Request sent");

        // Wait for response
        Logger::log(Logger::LOG_INFO, "Example10", "Step 3: Waiting for response...");
        waitCount = 0;
        while (!dataReceived && waitCount < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waitCount++;
        }

        if (dataReceived) {
            Logger::log(Logger::LOG_SUCCESS, "Example10", "Response received");
        }

        // Get final statistics
        Logger::log(Logger::LOG_INFO, "Example10", "Step 4: Final statistics...");
        auto stats = client.getStatistics();
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Total bytes sent: " + std::to_string(stats.bytes_sent));
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Total bytes received: " + std::to_string(stats.bytes_received));

        // Disconnect
        Logger::log(Logger::LOG_INFO, "Example10", "Step 5: Disconnecting...");
        client.disconnect();
        Logger::log(Logger::LOG_SUCCESS, "Example10", "Workflow completed");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example10",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example10", "Complete workflow example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main", "============================================");
    Logger::log(Logger::LOG_INFO, "Main", "  Async TcpClient Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main", "============================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Examples connect to httpbin.org (requires internet)");
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicAsyncTcpClientExample();
    customConfigExample();
    sslConnectionExample();
    proxyExample();
    callbacksExample();
    autoReconnectExample();
    heartbeatExample();
    requestResponseExample();
    statisticsExample();
    completeWorkflowExample();

    Logger::log(Logger::LOG_SUCCESS, "Main", "============================================");
    Logger::log(Logger::LOG_SUCCESS, "Main", "  All async TcpClient examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main", "============================================");

    return 0;
}
