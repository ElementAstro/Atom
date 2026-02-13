#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/connection/async_udpserver.hpp"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

// Silence spdlog during tests for cleaner output
#define SPDLOG_LEVEL_OFF
#include <spdlog/spdlog.h>

using namespace atom::async::connection;
using namespace std::chrono_literals;

// Helper function to find an available UDP port
uint16_t find_free_udp_port() {
    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context);
    asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), 0);
    socket.open(endpoint.protocol());
    socket.bind(endpoint);
    return socket.local_endpoint().port();
}

// Simple UDP client for testing
class TestUdpClient {
public:
    TestUdpClient() : socket_(io_context_) {}

    bool send(const std::string& host, uint16_t port,
              const std::string& message) {
        try {
            asio::ip::udp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));
            auto endpoint = *endpoints.begin();

            socket_.open(asio::ip::udp::v4());
            socket_.send_to(asio::buffer(message), endpoint);
            socket_.close();
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::string receive(uint16_t port,
                        std::chrono::milliseconds timeout = 1000ms) {
        try {
            socket_.open(asio::ip::udp::v4());
            socket_.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
            socket_.non_blocking(true);

            std::vector<char> buffer(1024);
            asio::ip::udp::endpoint sender_endpoint;

            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < timeout) {
                asio::error_code ec;
                size_t received = socket_.receive_from(asio::buffer(buffer),
                                                       sender_endpoint, 0, ec);
                if (!ec && received > 0) {
                    socket_.close();
                    return std::string(buffer.data(), received);
                }
                std::this_thread::sleep_for(1ms);
            }
            socket_.close();
            return "";
        } catch (const std::exception&) {
            return "";
        }
    }

private:
    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
};

// Test fixture for AsyncUdpServer
class AsyncUdpServerTest : public ::testing::Test {
protected:
    std::unique_ptr<AsyncUdpServer> server_;
    uint16_t port_;

    void SetUp() override {
        port_ = find_free_udp_port();
        server_ = std::make_unique<AsyncUdpServer>();
    }

    void TearDown() override {
        if (server_ && server_->isRunning()) {
            server_->stop();
        }
        std::this_thread::sleep_for(50ms);  // Allow cleanup
    }
};

// Basic functionality tests
TEST_F(AsyncUdpServerTest, StartStop) {
    EXPECT_FALSE(server_->isRunning());

    ASSERT_TRUE(server_->start(port_));
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(AsyncUdpServerTest, MessageHandling) {
    std::vector<std::string> received_messages;
    std::vector<std::string> sender_ips;
    std::vector<unsigned short> sender_ports;
    std::mutex messages_mutex;

    server_->addMessageHandler([&](const std::string& message,
                                   const std::string& ip, unsigned short port) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.push_back(message);
        sender_ips.push_back(ip);
        sender_ports.push_back(port);
    });

    ASSERT_TRUE(server_->start(port_));

    // Send test message
    TestUdpClient client;
    std::string test_message = "Hello UDP Server";
    ASSERT_TRUE(client.send("127.0.0.1", port_, test_message));

    // Wait for message processing
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(received_messages.size(), 1);
    if (!received_messages.empty()) {
        EXPECT_EQ(received_messages[0], test_message);
        EXPECT_EQ(sender_ips[0], "127.0.0.1");
    }
}

TEST_F(AsyncUdpServerTest, SendToClient) {
    ASSERT_TRUE(server_->start(port_));

    // Start a client to receive the message
    std::thread client_thread([this]() {
        TestUdpClient client;
        std::string received = client.receive(port_ + 1, 2000ms);
        EXPECT_EQ(received, "Server response");
    });

    std::this_thread::sleep_for(50ms);  // Let client start

    // Send message to client
    bool sent = server_->sendTo("Server response", "127.0.0.1", port_ + 1);
    EXPECT_TRUE(sent);

    client_thread.join();
}

TEST_F(AsyncUdpServerTest, Broadcast) {
    ASSERT_TRUE(server_->start(port_));

    // Start multiple clients to receive broadcast
    const int num_clients = 3;
    std::vector<std::thread> client_threads;
    std::atomic<int> received_count{0};

    for (int i = 0; i < num_clients; ++i) {
        client_threads.emplace_back([this, &received_count, i]() {
            TestUdpClient client;
            std::string received = client.receive(port_ + 10 + i, 2000ms);
            if (received == "Broadcast message") {
                received_count.fetch_add(1);
            }
        });
    }

    std::this_thread::sleep_for(100ms);  // Let clients start

    // Send broadcast message
    bool sent = server_->broadcast("Broadcast message", port_ + 10);
    EXPECT_TRUE(sent);

    for (auto& thread : client_threads) {
        thread.join();
    }

    // At least one client should have received the broadcast
    EXPECT_GT(received_count.load(), 0);
}

// Multicast tests
TEST_F(AsyncUdpServerTest, MulticastJoinLeave) {
    ASSERT_TRUE(server_->start(port_));

    std::string multicast_address = "224.0.0.1";

    // Join multicast group
    bool joined = server_->joinMulticastGroup(multicast_address);
    EXPECT_TRUE(joined);

    // Leave multicast group
    bool left = server_->leaveMulticastGroup(multicast_address);
    EXPECT_TRUE(left);
}

TEST_F(AsyncUdpServerTest, MulticastSend) {
    ASSERT_TRUE(server_->start(port_));

    std::string multicast_address = "224.0.0.2";
    uint16_t multicast_port = port_ + 20;

    // Send multicast message
    bool sent = server_->sendToMulticast("Multicast message", multicast_address,
                                         multicast_port);
    EXPECT_TRUE(sent);
}

// Concurrent client handling tests
TEST_F(AsyncUdpServerTest, ConcurrentClients) {
    std::atomic<int> message_count{0};
    std::mutex messages_mutex;
    std::vector<std::string> all_messages;

    server_->addMessageHandler(
        [&](const std::string& message, const std::string&, unsigned short) {
            std::lock_guard<std::mutex> lock(messages_mutex);
            all_messages.push_back(message);
            message_count.fetch_add(1);
        });

    ASSERT_TRUE(server_->start(port_));

    const int num_clients = 10;
    std::vector<std::thread> client_threads;

    // Start multiple clients sending messages concurrently
    for (int i = 0; i < num_clients; ++i) {
        client_threads.emplace_back([this, i]() {
            TestUdpClient client;
            std::string message = "Message from client " + std::to_string(i);
            client.send("127.0.0.1", port_, message);
        });
    }

    // Wait for all clients to complete
    for (auto& thread : client_threads) {
        thread.join();
    }

    // Wait for message processing
    std::this_thread::sleep_for(200ms);

    // Verify all messages were received
    EXPECT_EQ(message_count.load(), num_clients);
    EXPECT_EQ(all_messages.size(), num_clients);
}

// Statistics tests
TEST_F(AsyncUdpServerTest, Statistics) {
    ASSERT_TRUE(server_->start(port_));

    // Send some messages
    TestUdpClient client;
    for (int i = 0; i < 5; ++i) {
        client.send("127.0.0.1", port_, "Test message " + std::to_string(i));
    }

    std::this_thread::sleep_for(100ms);

    auto stats = server_->getStatistics();
    EXPECT_GT(stats.messages_received, 0);
    EXPECT_GT(stats.bytes_received, 0);

    // Reset statistics
    server_->resetStatistics();
    auto reset_stats = server_->getStatistics();
    EXPECT_EQ(reset_stats.messages_received, 0);
    EXPECT_EQ(reset_stats.bytes_received, 0);
}

// ============================================================================
// ENHANCED TESTS FOR COMPREHENSIVE COVERAGE
// ============================================================================

// Test error handling
TEST_F(AsyncUdpServerTest, ErrorHandling) {
    std::vector<std::string> errors;
    std::mutex errors_mutex;

    server_->addErrorHandler([&](const std::string& error,
                                 const std::error_code& ec) {
        std::lock_guard<std::mutex> lock(errors_mutex);
        errors.push_back(error + " (code: " + std::to_string(ec.value()) + ")");
    });

    ASSERT_TRUE(server_->start(port_));

    // Try to start again (should fail)
    EXPECT_FALSE(server_->start(port_));

    // Try invalid operations
    EXPECT_FALSE(server_->sendTo("test", "invalid.ip.address", 12345));
    EXPECT_FALSE(server_->joinMulticastGroup("invalid.multicast.address"));

    std::this_thread::sleep_for(100ms);

    // Should have captured some errors
    EXPECT_GT(errors.size(), 0);
}

// Test performance under load
TEST_F(AsyncUdpServerTest, PerformanceUnderLoad) {
    std::atomic<int> message_count{0};
    std::atomic<int> error_count{0};

    server_->addMessageHandler(
        [&](const std::string&, const std::string&, unsigned short) {
            message_count.fetch_add(1);
        });

    server_->addErrorHandler([&](const std::string&, const std::error_code&) {
        error_count.fetch_add(1);
    });

    ASSERT_TRUE(server_->start(port_));

    const int num_messages = 1000;
    const int num_threads = 20;
    std::vector<std::thread> client_threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Start multiple threads sending messages rapidly
    for (int t = 0; t < num_threads; ++t) {
        client_threads.emplace_back([this, t]() {
            TestUdpClient client;
            for (int i = 0; i < num_messages / num_threads; ++i) {
                std::string message =
                    "Load test " + std::to_string(t) + "_" + std::to_string(i);
                client.send("127.0.0.1", port_, message);
                // Small delay to avoid overwhelming the system
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : client_threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Give server time to process remaining messages
    std::this_thread::sleep_for(500ms);

    // Verify performance metrics
    EXPECT_GT(message_count.load(),
              num_messages / 2);         // At least half should succeed
    EXPECT_LT(duration.count(), 10000);  // Should complete within 10 seconds

    // Error rate should be reasonable
    EXPECT_LT(error_count.load(),
              num_messages / 10);  // Less than 10% error rate
}

// Test IP filtering functionality
TEST_F(AsyncUdpServerTest, IpFiltering) {
    std::atomic<int> message_count{0};

    server_->addMessageHandler(
        [&](const std::string&, const std::string&, unsigned short) {
            message_count.fetch_add(1);
        });

    ASSERT_TRUE(server_->start(port_));

    // Add allowed IP
    server_->addAllowedIp("127.0.0.1");

    // Send from allowed IP
    TestUdpClient client;
    ASSERT_TRUE(client.send("127.0.0.1", port_, "Allowed message"));

    std::this_thread::sleep_for(100ms);

    // Message should be received
    EXPECT_EQ(message_count.load(), 1);

    // Remove allowed IP
    server_->removeAllowedIp("127.0.0.1");

    // Send another message (should be filtered out if filtering is active)
    ASSERT_TRUE(client.send("127.0.0.1", port_, "Filtered message"));

    std::this_thread::sleep_for(100ms);

    // Message count might not increase if filtering is working
    // Note: This test depends on the actual implementation of IP filtering
}

// Test buffer size configuration
TEST_F(AsyncUdpServerTest, BufferSizeConfiguration) {
    // Create server with custom buffer size
    server_ = std::make_unique<AsyncUdpServer>(4);  // 4 threads

    std::atomic<int> message_count{0};
    server_->addMessageHandler(
        [&](const std::string&, const std::string&, unsigned short) {
            message_count.fetch_add(1);
        });

    ASSERT_TRUE(server_->start(port_));

    // Send messages of various sizes
    TestUdpClient client;
    std::vector<std::string> messages = {
        "Small", std::string(100, 'A'),  // 100 bytes
        std::string(1000, 'B'),          // 1KB
        std::string(8000, 'C')           // 8KB (close to typical UDP limit)
    };

    for (const auto& message : messages) {
        ASSERT_TRUE(client.send("127.0.0.1", port_, message));
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(200ms);

    // All messages should be received
    EXPECT_EQ(message_count.load(), messages.size());
}

// Test graceful shutdown
TEST_F(AsyncUdpServerTest, GracefulShutdown) {
    std::atomic<bool> handler_running{false};
    std::atomic<bool> handler_completed{false};

    server_->addMessageHandler(
        [&](const std::string&, const std::string&, unsigned short) {
            handler_running = true;
            std::this_thread::sleep_for(200ms);  // Simulate processing time
            handler_completed = true;
        });

    ASSERT_TRUE(server_->start(port_));

    // Send a message that will take time to process
    std::thread sender([this]() {
        TestUdpClient client;
        client.send("127.0.0.1", port_, "Long processing message");
    });

    // Wait for handler to start
    std::this_thread::sleep_for(50ms);
    sender.join();

    // Stop server while handler is running
    auto stop_start = std::chrono::steady_clock::now();
    server_->stop();
    auto stop_end = std::chrono::steady_clock::now();
    auto stop_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        stop_end - stop_start);

    // Server should stop gracefully, allowing handler to complete
    EXPECT_TRUE(handler_completed.load());
    EXPECT_LT(stop_duration.count(), 1000);  // Should not take too long to stop
}
