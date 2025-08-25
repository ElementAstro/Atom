#include "atom/extra/asio/sse/event.hpp"
#include "atom/extra/asio/sse/server/server.hpp"
#include "atom/extra/asio/sse/server/server_config.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace atom::extra::asio::sse;
using namespace std::chrono_literals;

// Helper function to generate random data
std::string generate_random_data() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 1000);

    return "Random data: " + std::to_string(dis(gen)) +
           ", timestamp: " + std::to_string(std::time(nullptr));
}

int main() {
    try {
        std::cout << "=== ASIO SSE Server Example ===" << std::endl;

        // 1. Basic SSE server
        std::cout << "\n1. Basic SSE Server:" << std::endl;
        {
            net::io_context io_context;

            // Configure SSE server
            ServerConfig config;
            config.address = "127.0.0.1";
            config.port = 8080;
            config.max_connections = 100;
            config.enable_cors = true;
            config.cors_origin = "*";
            config.heartbeat_interval = 30s;
            config.persist_events = false;
            config.require_auth = false;

            SSEServer server(io_context, config);

            std::cout << "Starting SSE server on " << config.address << ":"
                      << config.port << std::endl;
            server.start();

            // Run server in a separate thread
            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast some events
            std::cout << "Broadcasting events..." << std::endl;
            for (int i = 0; i < 10; ++i) {
                Event event;
                event.id = "event_" + std::to_string(i);
                event.type = "message";
                event.data =
                    "Hello from SSE server! Event #" + std::to_string(i);

                server.broadcast_event(event);
                std::cout << "Broadcasted event " << i << std::endl;

                std::this_thread::sleep_for(2s);
            }

            std::cout << "Stopping server..." << std::endl;
            server.stop();
            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }

            // Display server metrics
            auto metrics = server.get_metrics();
            std::cout << "Server metrics:" << std::endl;
            std::cout << "  Total connections: " << metrics.total_connections
                      << std::endl;
            std::cout << "  Active connections: " << metrics.active_connections
                      << std::endl;
            std::cout << "  Events sent: " << metrics.events_sent << std::endl;
            std::cout << "  Bytes sent: " << metrics.bytes_sent << std::endl;
        }

        // 2. SSE server with channels
        std::cout << "\n2. SSE Server with Channels:" << std::endl;
        {
            net::io_context io_context;

            ServerConfig config;
            config.address = "127.0.0.1";
            config.port = 8081;
            config.max_connections = 50;
            config.enable_cors = true;
            config.heartbeat_interval = 15s;

            SSEServer server(io_context, config);

            std::cout << "Starting channel-based SSE server on port 8081"
                      << std::endl;
            server.start();

            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast to different channels
            std::vector<std::string> channels = {"news", "sports", "weather",
                                                 "tech"};

            std::cout << "Broadcasting to different channels..." << std::endl;
            for (int round = 0; round < 3; ++round) {
                for (const auto& channel : channels) {
                    Event event;
                    event.id = channel + "_" + std::to_string(round);
                    event.type = "update";
                    event.data = "Update for " + channel + " channel, round " +
                                 std::to_string(round);

                    server.broadcast_event(event, channel);
                    std::cout << "Broadcasted to channel '" << channel
                              << "': " << event.data << std::endl;

                    std::this_thread::sleep_for(1s);
                }
            }

            std::this_thread::sleep_for(2s);

            server.stop();
            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }
        }

        // 3. SSE server with authentication
        std::cout << "\n3. SSE Server with Authentication:" << std::endl;
        {
            net::io_context io_context;

            ServerConfig config;
            config.address = "127.0.0.1";
            config.port = 8082;
            config.max_connections = 30;
            config.require_auth = true;
            config.auth_file =
                "sse_auth.txt";  // Would contain user credentials
            config.enable_cors = true;

            SSEServer server(io_context, config);

            std::cout << "Starting authenticated SSE server on port 8082"
                      << std::endl;
            server.start();

            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast authenticated events
            std::cout << "Broadcasting authenticated events..." << std::endl;
            for (int i = 0; i < 5; ++i) {
                Event event;
                event.id = "auth_event_" + std::to_string(i);
                event.type = "secure_message";
                event.data = "Authenticated event #" + std::to_string(i) +
                             ": " + generate_random_data();

                server.broadcast_event(event);
                std::cout << "Broadcasted authenticated event " << i
                          << std::endl;

                std::this_thread::sleep_for(3s);
            }

            server.stop();
            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }
        }

        // 4. SSE server with event persistence
        std::cout << "\n4. SSE Server with Event Persistence:" << std::endl;
        {
            net::io_context io_context;

            ServerConfig config;
            config.address = "127.0.0.1";
            config.port = 8083;
            config.max_connections = 50;
            config.persist_events = true;
            config.event_store_path = "sse_server_events.db";
            config.max_event_history = 1000;
            config.heartbeat_interval = 20s;

            SSEServer server(io_context, config);

            std::cout << "Starting persistent SSE server on port 8083"
                      << std::endl;
            server.start();

            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast events that will be persisted
            std::cout << "Broadcasting persistent events..." << std::endl;
            for (int i = 0; i < 8; ++i) {
                Event event;
                event.id = "persistent_" + std::to_string(i);
                event.type = "data_update";
                event.data = "Persistent event #" + std::to_string(i) + ": " +
                             generate_random_data();
                event.retry = 5000;  // 5 second retry

                server.broadcast_event(event);
                std::cout << "Broadcasted persistent event " << i << std::endl;

                std::this_thread::sleep_for(2s);
            }

            std::this_thread::sleep_for(3s);

            server.stop();
            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }

            auto metrics = server.get_metrics();
            std::cout << "Persistent server metrics:" << std::endl;
            std::cout << "  Events sent: " << metrics.events_sent << std::endl;
            std::cout << "  Stored events: " << metrics.stored_events
                      << std::endl;
        }

        // 5. High-frequency SSE server
        std::cout << "\n5. High-Frequency SSE Server:" << std::endl;
        {
            net::io_context io_context;

            ServerConfig config;
            config.address = "127.0.0.1";
            config.port = 8084;
            config.max_connections = 100;
            config.heartbeat_interval = 10s;
            config.enable_cors = true;

            SSEServer server(io_context, config);

            std::cout << "Starting high-frequency SSE server on port 8084"
                      << std::endl;
            server.start();

            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast high-frequency events
            std::cout << "Broadcasting high-frequency events..." << std::endl;
            auto start_time = std::chrono::steady_clock::now();
            int event_count = 0;

            while (std::chrono::steady_clock::now() - start_time < 10s) {
                Event event;
                event.id = "hf_" + std::to_string(event_count);
                event.type = "realtime_data";
                event.data =
                    "High-frequency event #" + std::to_string(event_count) +
                    " at " +
                    std::to_string(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count());

                server.broadcast_event(event);

                if (event_count % 10 == 0) {
                    std::cout << "Broadcasted " << event_count
                              << " high-frequency events" << std::endl;
                }

                event_count++;
                std::this_thread::sleep_for(100ms);  // 10 events per second
            }

            std::cout << "Total high-frequency events: " << event_count
                      << std::endl;

            server.stop();
            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }

            auto metrics = server.get_metrics();
            std::cout << "High-frequency server metrics:" << std::endl;
            std::cout << "  Events sent: " << metrics.events_sent << std::endl;
            std::cout << "  Bytes sent: " << metrics.bytes_sent << std::endl;
            std::cout << "  Average event size: "
                      << (metrics.events_sent > 0
                              ? metrics.bytes_sent / metrics.events_sent
                              : 0)
                      << " bytes" << std::endl;
        }

        // 6. Multiple SSE servers
        std::cout << "\n6. Multiple SSE Servers:" << std::endl;
        {
            net::io_context io_context;

            std::vector<std::unique_ptr<SSEServer>> servers;
            std::vector<ServerConfig> configs(3);

            // Configure multiple servers
            for (int i = 0; i < 3; ++i) {
                configs[i].address = "127.0.0.1";
                configs[i].port = 8085 + i;
                configs[i].max_connections = 20;
                configs[i].heartbeat_interval = 15s;
                configs[i].enable_cors = true;

                auto server =
                    std::make_unique<SSEServer>(io_context, configs[i]);
                server->start();
                servers.push_back(std::move(server));

                std::cout << "Started SSE server " << i << " on port "
                          << configs[i].port << std::endl;
            }

            std::thread server_thread([&io_context]() { io_context.run(); });

            // Broadcast different events to each server
            std::cout << "Broadcasting to multiple servers..." << std::endl;
            for (int round = 0; round < 4; ++round) {
                for (size_t i = 0; i < servers.size(); ++i) {
                    Event event;
                    event.id = "multi_" + std::to_string(i) + "_" +
                               std::to_string(round);
                    event.type = "server_" + std::to_string(i) + "_event";
                    event.data = "Event from server " + std::to_string(i) +
                                 ", round " + std::to_string(round);

                    servers[i]->broadcast_event(event);
                    std::cout << "Server " << i
                              << " broadcasted: " << event.data << std::endl;
                }
                std::this_thread::sleep_for(2s);
            }

            // Stop all servers
            for (size_t i = 0; i < servers.size(); ++i) {
                servers[i]->stop();
                std::cout << "Stopped server " << i << std::endl;
            }

            io_context.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }

            // Display metrics for all servers
            for (size_t i = 0; i < servers.size(); ++i) {
                auto metrics = servers[i]->get_metrics();
                std::cout << "Server " << i << " metrics:" << std::endl;
                std::cout << "  Events sent: " << metrics.events_sent
                          << std::endl;
                std::cout << "  Active connections: "
                          << metrics.active_connections << std::endl;
            }
        }

        std::cout << "\n=== SSE Server Example Completed ===" << std::endl;
        std::cout << "\nNote: To test these servers, you can use curl or a web "
                     "browser:"
                  << std::endl;
        std::cout << "curl -N http://127.0.0.1:8080/events" << std::endl;
        std::cout << "curl -N http://127.0.0.1:8081/events/news" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
