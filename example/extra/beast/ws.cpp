/*
 * ws.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Beast WebSocket Example (Minimal Stub Implementation)
This is a stub implementation since the atom-extra-beast library has
complex template/concept compatibility issues.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <functional>
#include <atomic>

using namespace std::chrono_literals;

// Minimal stub implementations since atom-extra-beast has compatibility issues

namespace boost::beast {
    struct error_code {
        int value_ = 0;
        error_code() = default;
        error_code(int val) : value_(val) {}
        operator bool() const { return value_ != 0; }
        std::string message() const { return value_ ? "Error " + std::to_string(value_) : "Success"; }
    };
}

// Stub WSClient class
class WSClient {
public:
    WSClient() {
        std::cout << "WebSocket Client created (stub implementation)" << std::endl;
    }

    template<typename ConnectHandler>
    void asyncConnect(std::string_view host, std::string_view port, ConnectHandler&& handler) {
        std::cout << "Async connecting to " << host << ":" << port << " (stub)" << std::endl;
        
        // Simulate async connection
        std::thread([handler = std::forward<ConnectHandler>(handler)]() mutable {
            std::this_thread::sleep_for(100ms);
            boost::beast::error_code ec(0); // Success
            handler(ec);
        }).detach();
    }

    void connect(std::string_view host, std::string_view port) {
        std::cout << "Connecting to " << host << ":" << port << " (stub)" << std::endl;
        connected_ = true;
    }

    void send(std::string_view message) {
        if (connected_) {
            std::cout << "Sending message (stub): " << message << std::endl;
        } else {
            std::cout << "Cannot send message: not connected (stub)" << std::endl;
        }
    }

    std::string receive() {
        if (connected_) {
            std::string response = "Echo from WebSocket server (stub): Hello World!";
            std::cout << "Received message (stub): " << response << std::endl;
            return response;
        } else {
            std::cout << "Cannot receive message: not connected (stub)" << std::endl;
            return "";
        }
    }

    void close() {
        std::cout << "Closing WebSocket connection (stub)" << std::endl;
        connected_ = false;
    }

    bool isConnected() const {
        return connected_;
    }

private:
    std::atomic<bool> connected_{false};
};

int main() {
    std::cout << "=== Beast WebSocket Example (Stub Implementation) ===" << std::endl;
    std::cout << "Note: This is a stub implementation due to template/concept compatibility issues." << std::endl;

    try {
        // 1. Async WebSocket connection
        std::cout << "\n1. Async WebSocket Connection:" << std::endl;
        {
            WSClient client;
            std::atomic<bool> connection_complete{false};

            client.asyncConnect("example.com", "80", [&connection_complete](boost::beast::error_code ec) {
                if (ec) {
                    std::cerr << "Async connection failed: " << ec.message() << std::endl;
                } else {
                    std::cout << "Async connected to WebSocket server" << std::endl;
                }
                connection_complete = true;
            });

            // Wait for async connection to complete
            while (!connection_complete) {
                std::this_thread::sleep_for(10ms);
            }

            if (client.isConnected()) {
                client.send("Hello from async client!");
                std::string response = client.receive();
                client.close();
            }
        }

        // 2. Synchronous WebSocket connection
        std::cout << "\n2. Synchronous WebSocket Connection:" << std::endl;
        {
            WSClient client;
            client.connect("ws://echo.websocket.org", "80");

            if (client.isConnected()) {
                client.send("Hello from sync client!");
                std::string response = client.receive();
                client.close();
            }
        }

        // 3. Multiple message exchange
        std::cout << "\n3. Multiple Message Exchange:" << std::endl;
        {
            WSClient client;
            client.connect("ws://localhost", "8080");

            if (client.isConnected()) {
                for (int i = 0; i < 3; ++i) {
                    std::string message = "Message #" + std::to_string(i + 1);
                    client.send(message);
                    std::string response = client.receive();
                    std::this_thread::sleep_for(100ms);
                }
                client.close();
            }
        }

        std::cout << "\n=== Beast WebSocket Example Complete (Stub Implementation) ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in WebSocket examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
