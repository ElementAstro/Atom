#include "atom/extra/asio/sse/client/client.hpp"
#include "atom/extra/asio/sse/client/client_config.hpp"
#include "atom/extra/asio/sse/event.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace atom::extra::asio::sse;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== ASIO SSE Client Example ===" << std::endl;

        // 1. Basic SSE client connection
        std::cout << "\n1. Basic SSE Client Connection:" << std::endl;
        {
            net::io_context io_context;

            // Configure SSE client
            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/20";  // Stream 20 events
            config.use_ssl = false;
            config.reconnect_enabled = true;
            config.reconnect_delay = 2s;
            config.max_reconnect_attempts = 3;

            Client client(io_context, config);

            std::atomic<int> events_received{0};
            std::atomic<bool> connected{false};

            // Set event handler
            client.set_event_handler([&events_received](const Event& event) {
                std::cout << "Received SSE event:" << std::endl;
                std::cout << "  ID: " << event.id << std::endl;
                std::cout << "  Type: " << event.type << std::endl;
                std::cout << "  Data: " << event.data << std::endl;
                std::cout << "  Retry: " << event.retry << std::endl;
                std::cout << "---" << std::endl;
                events_received++;
            });

            // Set connection handler
            client.set_connection_handler(
                [&connected](bool is_connected, const std::string& error) {
                    if (is_connected) {
                        std::cout << "SSE client connected successfully!"
                                  << std::endl;
                        connected = true;
                    } else {
                        std::cerr << "SSE client connection failed: " << error
                                  << std::endl;
                    }
                });

            // Start the client
            std::cout << "Starting SSE client..." << std::endl;
            client.start();

            // Run IO context in a separate thread
            std::thread io_thread([&io_context]() { io_context.run(); });

            // Wait for some events
            auto start_time = std::chrono::steady_clock::now();
            while (events_received < 5 &&
                   std::chrono::steady_clock::now() - start_time < 30s) {
                std::this_thread::sleep_for(1s);
                std::cout << "Events received so far: "
                          << events_received.load() << std::endl;
            }

            std::cout << "Stopping SSE client..." << std::endl;
            client.stop();

            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            std::cout << "Total events received: " << events_received.load()
                      << std::endl;
        }

        // 2. SSE client with event filtering
        std::cout << "\n2. SSE Client with Event Filtering:" << std::endl;
        {
            net::io_context io_context;

            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/10";
            config.use_ssl = false;
            config.enable_event_filtering = true;

            Client client(io_context, config);

            std::atomic<int> filtered_events{0};

            // Add event filters
            client.add_event_filter("message");
            client.add_event_filter("data");
            client.add_event_filter("update");

            client.set_event_handler([&filtered_events](const Event& event) {
                std::cout << "Filtered event received:" << std::endl;
                std::cout << "  Type: " << event.type << std::endl;
                std::cout << "  Data: " << event.data.substr(0, 100) << "..."
                          << std::endl;
                filtered_events++;
            });

            client.set_connection_handler([](bool is_connected,
                                             const std::string& error) {
                if (is_connected) {
                    std::cout << "Filtered SSE client connected" << std::endl;
                } else {
                    std::cerr << "Filtered SSE client failed: " << error
                              << std::endl;
                }
            });

            std::cout << "Starting filtered SSE client..." << std::endl;
            client.start();

            std::thread io_thread([&io_context]() { io_context.run(); });

            // Wait for filtered events
            std::this_thread::sleep_for(15s);

            client.stop();
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            std::cout << "Filtered events received: " << filtered_events.load()
                      << std::endl;
        }

        // 3. SSE client with authentication
        std::cout << "\n3. SSE Client with Authentication:" << std::endl;
        {
            net::io_context io_context;

            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/5";
            config.use_ssl = false;
            config.username = "test_user";
            config.password = "test_password";
            config.api_key = "test_api_key_12345";

            Client client(io_context, config);

            std::atomic<int> auth_events{0};

            client.set_event_handler([&auth_events](const Event& event) {
                std::cout << "Authenticated event: " << event.data.substr(0, 50)
                          << "..." << std::endl;
                auth_events++;
            });

            client.set_connection_handler([](bool is_connected,
                                             const std::string& error) {
                if (is_connected) {
                    std::cout << "Authenticated SSE client connected"
                              << std::endl;
                } else {
                    std::cout << "Auth SSE client connection result: " << error
                              << std::endl;
                }
            });

            std::cout << "Starting authenticated SSE client..." << std::endl;
            client.start();

            std::thread io_thread([&io_context]() { io_context.run(); });

            std::this_thread::sleep_for(10s);

            client.stop();
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            std::cout << "Authenticated events received: " << auth_events.load()
                      << std::endl;
        }

        // 4. SSE client with reconnection
        std::cout << "\n4. SSE Client with Reconnection:" << std::endl;
        {
            net::io_context io_context;

            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/3";
            config.use_ssl = false;
            config.reconnect_enabled = true;
            config.reconnect_delay = 1s;
            config.max_reconnect_attempts = 5;

            Client client(io_context, config);

            std::atomic<int> reconnect_events{0};
            std::atomic<int> connection_attempts{0};

            client.set_event_handler([&reconnect_events](const Event& event) {
                std::cout << "Reconnection test event: "
                          << event.data.substr(0, 30) << "..." << std::endl;
                reconnect_events++;
            });

            client.set_connection_handler([&connection_attempts](
                                              bool is_connected,
                                              const std::string& error) {
                connection_attempts++;
                if (is_connected) {
                    std::cout << "Reconnection test: Connected (attempt "
                              << connection_attempts.load() << ")" << std::endl;
                } else {
                    std::cout << "Reconnection test: Failed (attempt "
                              << connection_attempts.load() << "): " << error
                              << std::endl;
                }
            });

            std::cout << "Starting reconnection test SSE client..."
                      << std::endl;
            client.start();

            std::thread io_thread([&io_context]() { io_context.run(); });

            // Let it run for a bit
            std::this_thread::sleep_for(5s);

            // Simulate reconnection by stopping and starting
            std::cout << "Simulating reconnection..." << std::endl;
            client.reconnect();

            std::this_thread::sleep_for(5s);

            client.stop();
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            std::cout << "Reconnection events received: "
                      << reconnect_events.load() << std::endl;
            std::cout << "Connection attempts: " << connection_attempts.load()
                      << std::endl;
        }

        // 5. SSE client with event persistence
        std::cout << "\n5. SSE Client with Event Persistence:" << std::endl;
        {
            net::io_context io_context;

            ClientConfig config;
            config.host = "httpbin.org";
            config.port = "80";
            config.path = "/stream/8";
            config.use_ssl = false;
            config.persist_events = true;
            config.event_store_path = "sse_events.db";
            config.max_stored_events = 100;

            Client client(io_context, config);

            std::atomic<int> persistent_events{0};

            client.set_event_handler([&persistent_events](const Event& event) {
                std::cout << "Persistent event " << persistent_events.load() + 1
                          << ": " << event.data.substr(0, 40) << "..."
                          << std::endl;
                persistent_events++;
            });

            client.set_connection_handler([](bool is_connected,
                                             const std::string& error) {
                if (is_connected) {
                    std::cout << "Persistent SSE client connected" << std::endl;
                } else {
                    std::cout << "Persistent SSE client error: " << error
                              << std::endl;
                }
            });

            std::cout << "Starting persistent SSE client..." << std::endl;
            client.start();

            std::thread io_thread([&io_context]() { io_context.run(); });

            std::this_thread::sleep_for(12s);

            client.stop();
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            std::cout << "Persistent events received: "
                      << persistent_events.load() << std::endl;
        }

        // 6. Multiple SSE clients
        std::cout << "\n6. Multiple SSE Clients:" << std::endl;
        {
            net::io_context io_context;

            std::vector<std::unique_ptr<Client>> clients;
            std::vector<std::atomic<int>> event_counts(3);

            for (int i = 0; i < 3; ++i) {
                ClientConfig config;
                config.host = "httpbin.org";
                config.port = "80";
                config.path =
                    "/stream/" +
                    std::to_string(3 + i);  // Different stream lengths
                config.use_ssl = false;

                auto client = std::make_unique<Client>(io_context, config);

                client->set_event_handler(
                    [&event_counts, i](const Event& event) {
                        std::cout << "Client " << i
                                  << " event: " << event.data.substr(0, 20)
                                  << "..." << std::endl;
                        event_counts[i]++;
                    });

                client->set_connection_handler(
                    [i](bool is_connected, const std::string& error) {
                        if (is_connected) {
                            std::cout << "Multi-client " << i << ": Connected"
                                      << std::endl;
                        } else {
                            std::cout << "Multi-client " << i << ": Error - "
                                      << error << std::endl;
                        }
                    });

                client->start();
                clients.push_back(std::move(client));
            }

            std::thread io_thread([&io_context]() { io_context.run(); });

            std::cout << "Running multiple SSE clients..." << std::endl;
            std::this_thread::sleep_for(10s);

            for (auto& client : clients) {
                client->stop();
            }

            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }

            for (size_t i = 0; i < event_counts.size(); ++i) {
                std::cout << "Client " << i << " received "
                          << event_counts[i].load() << " events" << std::endl;
            }
        }

        std::cout << "\n=== SSE Client Example Completed ===" << std::endl;
        std::cout << "\nNote: This example uses httpbin.org/stream for testing."
                  << std::endl;
        std::cout
            << "For production use, replace with your own SSE server endpoint."
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
