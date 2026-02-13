#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/connection/async_tcpclient.hpp"
#include "atom/connection/async_sockethub.hpp"
#include "atom/connection/async_udpclient.hpp"
#include "atom/connection/async_udpserver.hpp"

#include <asio.hpp>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <numeric>

using namespace atom::async::connection;
using namespace std::chrono_literals;

// Helper function to find an available port
uint16_t find_free_port() {
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
}

// Performance test fixture
class AsyncPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        tcp_port_ = find_free_port();
        udp_port_ = find_free_port();
    }

    void TearDown() override {
        // Allow cleanup time
        std::this_thread::sleep_for(100ms);
    }

    uint16_t tcp_port_;
    uint16_t udp_port_;
};

// TCP throughput test
TEST_F(AsyncPerformanceTest, TcpThroughputTest) {
    auto hub = std::make_unique<SocketHub>();
    std::atomic<size_t> bytes_received{0};
    std::atomic<int> messages_received{0};

    hub->setMessageHandler([&](const std::string& message, std::shared_ptr<asio::ip::tcp::socket>) {
        bytes_received.fetch_add(message.size());
        messages_received.fetch_add(1);
    });

    ASSERT_TRUE(hub->start(tcp_port_));

    auto client = std::make_unique<TcpClient>();
    ASSERT_TRUE(client->connect("127.0.0.1", tcp_port_));

    // Test parameters
    const size_t message_size = 1024; // 1KB messages
    const int num_messages = 1000;
    std::string test_message(message_size, 'A');

    auto start_time = std::chrono::high_resolution_clock::now();

    // Send messages as fast as possible
    for (int i = 0; i < num_messages; ++i) {
        client->sendString(test_message);
    }

    // Wait for all messages to be received
    auto timeout = std::chrono::steady_clock::now() + 30s;
    while (messages_received.load() < num_messages &&
           std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(10ms);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Calculate throughput
    double throughput_mbps = (bytes_received.load() * 8.0) / (duration.count() * 1000.0); // Mbps
    double message_rate = messages_received.load() / (duration.count() / 1000.0); // messages/sec

    std::cout << "TCP Throughput Test Results:\n";
    std::cout << "  Messages sent: " << num_messages << "\n";
    std::cout << "  Messages received: " << messages_received.load() << "\n";
    std::cout << "  Bytes received: " << bytes_received.load() << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << throughput_mbps << " Mbps\n";
    std::cout << "  Message rate: " << message_rate << " msg/sec\n";

    // Performance expectations (adjust based on system capabilities)
    EXPECT_GT(throughput_mbps, 1.0); // At least 1 Mbps
    EXPECT_GT(message_rate, 100.0); // At least 100 messages/sec
    EXPECT_EQ(messages_received.load(), num_messages); // All messages should be received

    client->disconnect();
    hub->stop();
}

// UDP throughput test
TEST_F(AsyncPerformanceTest, UdpThroughputTest) {
    auto server = std::make_unique<UdpSocketHub>();
    std::atomic<size_t> bytes_received{0};
    std::atomic<int> messages_received{0};

    server->addMessageHandler([&](const std::string& message, const std::string&, unsigned short) {
        bytes_received.fetch_add(message.size());
        messages_received.fetch_add(1);
    });

    ASSERT_TRUE(server->start(udp_port_));

    auto client = std::make_unique<UdpClient>();
    ASSERT_TRUE(client->bind(0));

    // Test parameters
    const size_t message_size = 512; // 512B messages (smaller for UDP)
    const int num_messages = 1000;
    std::string test_message(message_size, 'B');

    auto start_time = std::chrono::high_resolution_clock::now();

    // Send messages with small delays to avoid overwhelming UDP buffers
    for (int i = 0; i < num_messages; ++i) {
        client->send("127.0.0.1", udp_port_, test_message);
        if (i % 100 == 0) {
            std::this_thread::sleep_for(1ms); // Small delay every 100 messages
        }
    }

    // Wait for messages to be received
    std::this_thread::sleep_for(2s);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Calculate throughput
    double throughput_mbps = (bytes_received.load() * 8.0) / (duration.count() * 1000.0); // Mbps
    double message_rate = messages_received.load() / (duration.count() / 1000.0); // messages/sec
    double packet_loss = (1.0 - (double)messages_received.load() / num_messages) * 100.0;

    std::cout << "UDP Throughput Test Results:\n";
    std::cout << "  Messages sent: " << num_messages << "\n";
    std::cout << "  Messages received: " << messages_received.load() << "\n";
    std::cout << "  Packet loss: " << packet_loss << "%\n";
    std::cout << "  Bytes received: " << bytes_received.load() << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << throughput_mbps << " Mbps\n";
    std::cout << "  Message rate: " << message_rate << " msg/sec\n";

    // Performance expectations (UDP may have some packet loss)
    EXPECT_GT(throughput_mbps, 0.5); // At least 0.5 Mbps
    EXPECT_GT(message_rate, 50.0); // At least 50 messages/sec
    EXPECT_LT(packet_loss, 10.0); // Less than 10% packet loss
    EXPECT_GT(messages_received.load(), num_messages * 0.8); // At least 80% received

    server->stop();
}

// Connection latency test
TEST_F(AsyncPerformanceTest, ConnectionLatencyTest) {
    auto hub = std::make_unique<SocketHub>();
    std::vector<std::chrono::microseconds> connection_times;
    std::mutex times_mutex;

    hub->setClientHandler([&](std::shared_ptr<asio::ip::tcp::socket>, bool connected) {
        if (connected) {
            // Connection established
        }
    });

    ASSERT_TRUE(hub->start(tcp_port_));

    const int num_connections = 100;
    std::vector<std::thread> connection_threads;

    // Test multiple concurrent connections
    for (int i = 0; i < num_connections; ++i) {
        connection_threads.emplace_back([this, &connection_times, &times_mutex]() {
            auto client = std::make_unique<TcpClient>();

            auto start_time = std::chrono::high_resolution_clock::now();
            bool connected = client->connect("127.0.0.1", tcp_port_);
            auto end_time = std::chrono::high_resolution_clock::now();

            if (connected) {
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                std::lock_guard<std::mutex> lock(times_mutex);
                connection_times.push_back(duration);
                client->disconnect();
            }
        });
    }

    // Wait for all connections to complete
    for (auto& thread : connection_threads) {
        thread.join();
    }

    // Calculate statistics
    if (!connection_times.empty()) {
        auto min_time = *std::min_element(connection_times.begin(), connection_times.end());
        auto max_time = *std::max_element(connection_times.begin(), connection_times.end());
        auto avg_time = std::accumulate(connection_times.begin(), connection_times.end(),
                                       std::chrono::microseconds(0)) / connection_times.size();

        std::cout << "Connection Latency Test Results:\n";
        std::cout << "  Successful connections: " << connection_times.size() << "/" << num_connections << "\n";
        std::cout << "  Min latency: " << min_time.count() << " μs\n";
        std::cout << "  Max latency: " << max_time.count() << " μs\n";
        std::cout << "  Avg latency: " << avg_time.count() << " μs\n";

        // Performance expectations
        EXPECT_LT(avg_time.count(), 10000); // Average < 10ms
        EXPECT_LT(max_time.count(), 50000); // Max < 50ms
        EXPECT_GT(connection_times.size(), num_connections * 0.9); // At least 90% success rate
    }

    hub->stop();
}

// Memory usage stress test
TEST_F(AsyncPerformanceTest, MemoryStressTest) {
    auto hub = std::make_unique<SocketHub>();
    std::atomic<int> active_connections{0};
    std::atomic<int> total_messages{0};

    hub->setClientHandler([&](std::shared_ptr<asio::ip::tcp::socket>, bool connected) {
        if (connected) {
            active_connections.fetch_add(1);
        } else {
            active_connections.fetch_sub(1);
        }
    });

    hub->setMessageHandler([&](const std::string&, std::shared_ptr<asio::ip::tcp::socket>) {
        total_messages.fetch_add(1);
    });

    ASSERT_TRUE(hub->start(tcp_port_));

    const int max_connections = 500;
    const int messages_per_connection = 10;
    std::vector<std::thread> client_threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Create many concurrent connections
    for (int i = 0; i < max_connections; ++i) {
        client_threads.emplace_back([this, i]() {
            auto client = std::make_unique<TcpClient>();
            if (client->connect("127.0.0.1", tcp_port_)) {
                // Send some messages
                for (int j = 0; j < messages_per_connection; ++j) {
                    std::string message = "Stress test " + std::to_string(i) + "_" + std::to_string(j);
                    client->sendString(message);
                    std::this_thread::sleep_for(1ms);
                }
                std::this_thread::sleep_for(100ms); // Keep connection alive briefly
                client->disconnect();
            }
        });

        // Stagger connection attempts to avoid overwhelming the system
        if (i % 50 == 0) {
            std::this_thread::sleep_for(100ms);
        }
    }

    // Monitor peak connections
    int peak_connections = 0;
    std::thread monitor_thread([&]() {
        while (client_threads.size() > 0) {
            peak_connections = std::max(peak_connections, active_connections.load());
            std::this_thread::sleep_for(50ms);
        }
    });

    // Wait for all clients to complete
    for (auto& thread : client_threads) {
        thread.join();
    }

    monitor_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

    std::cout << "Memory Stress Test Results:\n";
    std::cout << "  Target connections: " << max_connections << "\n";
    std::cout << "  Peak concurrent connections: " << peak_connections << "\n";
    std::cout << "  Total messages processed: " << total_messages.load() << "\n";
    std::cout << "  Test duration: " << duration.count() << " seconds\n";
    std::cout << "  Final active connections: " << active_connections.load() << "\n";

    // Performance expectations
    EXPECT_GT(peak_connections, max_connections / 4); // At least 25% of connections succeeded
    EXPECT_GT(total_messages.load(), 0); // Some messages were processed
    EXPECT_EQ(active_connections.load(), 0); // All connections should be cleaned up
    EXPECT_LT(duration.count(), 60); // Should complete within 60 seconds

    hub->stop();
}

// High-frequency message test
TEST_F(AsyncPerformanceTest, HighFrequencyMessageTest) {
    auto hub = std::make_unique<SocketHub>();
    std::atomic<int> message_count{0};
    std::vector<std::chrono::microseconds> message_intervals;
    std::mutex intervals_mutex;
    std::chrono::high_resolution_clock::time_point last_message_time;
    bool first_message = true;

    hub->setMessageHandler([&](const std::string&, std::shared_ptr<asio::ip::tcp::socket>) {
        auto now = std::chrono::high_resolution_clock::now();
        if (!first_message) {
            auto interval = std::chrono::duration_cast<std::chrono::microseconds>(now - last_message_time);
            std::lock_guard<std::mutex> lock(intervals_mutex);
            message_intervals.push_back(interval);
        } else {
            first_message = false;
        }
        last_message_time = now;
        message_count.fetch_add(1);
    });

    ASSERT_TRUE(hub->start(tcp_port_));

    auto client = std::make_unique<TcpClient>();
    ASSERT_TRUE(client->connect("127.0.0.1", tcp_port_));

    const int num_messages = 1000;
    const auto target_interval = std::chrono::microseconds(1000); // 1ms between messages

    auto start_time = std::chrono::high_resolution_clock::now();

    // Send messages at high frequency
    for (int i = 0; i < num_messages; ++i) {
        client->sendString("High freq message " + std::to_string(i));
        std::this_thread::sleep_for(target_interval);
    }

    // Wait for all messages to be processed
    auto timeout = std::chrono::steady_clock::now() + 10s;
    while (message_count.load() < num_messages &&
           std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(10ms);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Calculate statistics
    if (!message_intervals.empty()) {
        auto avg_interval = std::accumulate(message_intervals.begin(), message_intervals.end(),
                                          std::chrono::microseconds(0)) / message_intervals.size();
        auto min_interval = *std::min_element(message_intervals.begin(), message_intervals.end());
        auto max_interval = *std::max_element(message_intervals.begin(), message_intervals.end());

        std::cout << "High Frequency Message Test Results:\n";
        std::cout << "  Messages sent: " << num_messages << "\n";
        std::cout << "  Messages received: " << message_count.load() << "\n";
        std::cout << "  Total duration: " << total_duration.count() << " ms\n";
        std::cout << "  Average interval: " << avg_interval.count() << " μs\n";
        std::cout << "  Min interval: " << min_interval.count() << " μs\n";
        std::cout << "  Max interval: " << max_interval.count() << " μs\n";

        // Performance expectations
        EXPECT_EQ(message_count.load(), num_messages); // All messages should be received
        EXPECT_LT(avg_interval.count(), 10000); // Average interval < 10ms
        EXPECT_LT(max_interval.count(), 100000); // Max interval < 100ms
    }

    client->disconnect();
    hub->stop();
}
