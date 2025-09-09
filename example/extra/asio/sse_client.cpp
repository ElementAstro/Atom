/*
 * sse_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: ASIO SSE Client Example (Minimal Stub Implementation)
This is a stub implementation since the atom-extra-asio library linking
is not configured in the current build system.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

using namespace std::chrono_literals;

// Minimal stub implementations since atom-extra-asio is not linked

namespace atom::extra::asio::sse {

// Stub Event class
class Event {
public:
    Event(std::string id, std::string event_type, std::string data)
        : id_(std::move(id)), event_type_(std::move(event_type)), data_(std::move(data)) {}

    const std::string& id() const noexcept { return id_; }
    const std::string& event_type() const noexcept { return event_type_; }
    const std::string& data() const noexcept { return data_; }

private:
    std::string id_;
    std::string event_type_;
    std::string data_;
};

// Stub ClientConfig struct
struct ClientConfig {
    std::string host = "localhost";
    std::string port = "8080";
    std::string path = "/events";
    bool use_ssl = false;
    bool verify_ssl = true;
    std::string ca_cert_file;
    std::string api_key;
    std::string username;
    std::string password;
    bool reconnect = true;
    int max_reconnect_attempts = 10;
    int reconnect_base_delay_ms = 1000;
    bool store_events = true;
    std::string event_store_path = "client_events";
    std::string last_event_id;
    std::vector<std::string> event_types_filter;
};

// Stub Client class
class Client {
public:
    Client(auto& io_context, const ClientConfig& config) {
        std::cout << "SSE Client created (stub implementation)" << std::endl;
        std::cout << "  Host: " << config.host << ":" << config.port << std::endl;
        std::cout << "  Path: " << config.path << std::endl;
    }

    void set_event_handler(std::function<void(const Event&)> handler) {
        event_handler_ = std::move(handler);
    }

    void set_connection_handler(std::function<void(bool, const std::string&)> handler) {
        connection_handler_ = std::move(handler);
    }

    void add_event_filter(const std::string& event_type) {
        std::cout << "Added event filter: " << event_type << std::endl;
    }

    void start() {
        std::cout << "Starting SSE client (stub)..." << std::endl;
        if (connection_handler_) {
            connection_handler_(true, "Connected to stub server");
        }

        // Simulate some events
        if (event_handler_) {
            std::thread([this]() {
                std::this_thread::sleep_for(100ms);
                event_handler_(Event("1", "message", "Hello from SSE stub"));
                std::this_thread::sleep_for(100ms);
                event_handler_(Event("2", "update", "Update from SSE stub"));
                std::this_thread::sleep_for(100ms);
                event_handler_(Event("3", "alert", "Alert from SSE stub"));
            }).detach();
        }
    }

    void stop() {
        std::cout << "Stopping SSE client (stub)..." << std::endl;
    }

    void reconnect() {
        std::cout << "Reconnecting SSE client (stub)..." << std::endl;
    }

private:
    std::function<void(const Event&)> event_handler_;
    std::function<void(bool, const std::string&)> connection_handler_;
};

} // namespace atom::extra::asio::sse

// Stub io_context
namespace asio {
class io_context {
public:
    void run() {
        std::cout << "Running io_context (stub)..." << std::endl;
        std::this_thread::sleep_for(1s);
    }

    void run_for(std::chrono::milliseconds duration) {
        std::cout << "Running io_context for " << duration.count() << "ms (stub)..." << std::endl;
        std::this_thread::sleep_for(duration);
    }

    void stop() {
        std::cout << "Stopping io_context (stub)..." << std::endl;
    }
};
} // namespace asio

using namespace atom::extra::asio::sse;

int main() {
    std::cout << "=== ASIO SSE Client Example (Stub Implementation) ===" << std::endl;
    std::cout << "Note: This is a stub implementation since atom-extra-asio library is not linked." << std::endl;

    try {
        asio::io_context io_context;

        // 1. Basic SSE client connection
        std::cout << "\n1. Basic SSE Client Connection:" << std::endl;
        {
            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/20";
            config.use_ssl = false;
            config.reconnect = true;
            config.reconnect_base_delay_ms = 2000;
            config.max_reconnect_attempts = 3;

            Client client(io_context, config);

            std::atomic<int> events_received{0};

            client.set_event_handler([&events_received](const Event& event) {
                std::cout << "Event received:" << std::endl;
                std::cout << "  ID: " << event.id() << std::endl;
                std::cout << "  Type: " << event.event_type() << std::endl;
                std::cout << "  Data: " << event.data() << std::endl;
                events_received++;
            });

            client.set_connection_handler([](bool connected, const std::string& message) {
                std::cout << "Connection status: " << (connected ? "Connected" : "Disconnected")
                          << " - " << message << std::endl;
            });

            client.start();
            io_context.run_for(2s);
            client.stop();

            std::cout << "Total events received: " << events_received.load() << std::endl;
        }

        std::cout << "\n=== SSE Client Example Complete (Stub Implementation) ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in SSE client examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
