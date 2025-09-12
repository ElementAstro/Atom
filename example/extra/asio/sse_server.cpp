/*
 * sse_server.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: ASIO SSE Server Example (Minimal Stub Implementation)
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
#include <map>

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

// Stub ServerConfig struct
struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string path = "/events";
    bool use_ssl = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    int heartbeat_interval_seconds = 30;
    int max_connections = 1000;
    bool store_events = false;
    std::string event_store_path = "server_events";
    std::vector<std::string> allowed_origins;
};

// Stub SSEServer class
class SSEServer {
public:
    SSEServer(auto& io_context, const ServerConfig& config) : config_(config) {
        std::cout << "SSE Server created (stub implementation)" << std::endl;
        std::cout << "  Host: " << config.host << ":" << config.port << std::endl;
        std::cout << "  Path: " << config.path << std::endl;
    }

    void broadcast_event(const Event& event) {
        std::cout << "Broadcasting event (stub): " << event.data() << std::endl;
        events_sent_++;
    }

    void broadcast_to_channel(const std::string& channel, const Event& event) {
        std::cout << "Broadcasted to channel '" << channel << "': " << event.data() << std::endl;
        events_sent_++;
    }

    auto get_metrics() {
        // Return a simple map that can be accessed like JSON
        std::map<std::string, int> metrics;
        metrics["total_events_sent"] = events_sent_;
        metrics["current_connections"] = current_connections_;
        return metrics;
    }

    void run() {
        std::cout << "Running SSE server (stub)..." << std::endl;
        current_connections_ = 5; // Simulate some connections

        // Simulate server running
        std::this_thread::sleep_for(100ms);
    }

private:
    ServerConfig config_;
    std::atomic<int> events_sent_{0};
    std::atomic<int> current_connections_{0};
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
    std::cout << "=== ASIO SSE Server Example (Stub Implementation) ===" << std::endl;
    std::cout << "Note: This is a stub implementation since atom-extra-asio library is not linked." << std::endl;

    try {
        asio::io_context io_context;

        // 1. Basic SSE server
        std::cout << "\n1. Basic SSE Server:" << std::endl;
        {
            ServerConfig config;
            config.host = "localhost";
            config.port = 8080;
            config.path = "/events";
            config.heartbeat_interval_seconds = 30;

            SSEServer server(io_context, config);

            // Simulate broadcasting events
            for (int i = 0; i < 3; ++i) {
                Event event("event_" + std::to_string(i), "message",
                           "Hello from SSE server stub #" + std::to_string(i));
                server.broadcast_event(event);
                std::this_thread::sleep_for(100ms);
            }

            server.run();

            auto metrics = server.get_metrics();
            std::cout << "Server metrics:" << std::endl;
            std::cout << "  Events sent: " << metrics["total_events_sent"] << std::endl;
            std::cout << "  Active connections: " << metrics["current_connections"] << std::endl;
        }

        // 2. Channel-based broadcasting
        std::cout << "\n2. Channel-based Broadcasting:" << std::endl;
        {
            ServerConfig config;
            config.host = "localhost";
            config.port = 8081;
            config.path = "/secure-events";

            SSEServer server(io_context, config);

            // Simulate channel-based events
            for (int i = 0; i < 2; ++i) {
                Event event("auth_event_" + std::to_string(i), "secure_message",
                           "Authenticated event #" + std::to_string(i) + " with secure data");
                server.broadcast_to_channel("authenticated_users", event);
                std::this_thread::sleep_for(100ms);
            }

            server.run();
        }

        std::cout << "\n=== SSE Server Example Complete (Stub Implementation) ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in SSE server examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
