#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/connection/async_fifoclient.hpp"
#include "atom/connection/async_fifoserver.hpp"
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
#include <filesystem>

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

// Integration test fixture
class AsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tcp_port_ = find_free_port();
        udp_port_ = find_free_port();

        // Generate unique FIFO path
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        fifo_path_ = (std::filesystem::temp_directory_path() /
                     ("integration_fifo_" + std::to_string(now))).string();
    }

    void TearDown() override {
        // Cleanup FIFO file
#ifndef _WIN32
        if (std::filesystem::exists(fifo_path_)) {
            std::filesystem::remove(fifo_path_);
        }
#endif
    }

    uint16_t tcp_port_;
    uint16_t udp_port_;
    std::string fifo_path_;
};

// Test TCP client-server communication through socket hub
TEST_F(AsyncIntegrationTest, TcpClientSocketHubCommunication) {
    // Set up socket hub (server)
    auto hub = std::make_unique<SocketHub>();
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    hub->setMessageHandler([&](const std::string& message, std::shared_ptr<asio::ip::tcp::socket>) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.push_back(message);
    });

    ASSERT_TRUE(hub->start(tcp_port_));

    // Set up TCP client
    auto client = std::make_unique<TcpClient>();
    ASSERT_TRUE(client->connect("127.0.0.1", tcp_port_));

    // Test bidirectional communication
    std::string test_message = "Hello from TCP client";
    client->sendString(test_message);

    std::this_thread::sleep_for(200ms);

    // Verify message received by hub
    EXPECT_EQ(received_messages.size(), 1);
    if (!received_messages.empty()) {
        EXPECT_EQ(received_messages[0], test_message);
    }

    // Test response from hub to client
    std::promise<std::vector<char>> response_promise;
    client->setOnDataReceivedCallback([&](const auto& data) {
        response_promise.set_value(data);
    });

    hub->broadcast("Response from hub");

    auto response_future = response_promise.get_future();
    ASSERT_EQ(response_future.wait_for(2s), std::future_status::ready);
    auto response_data = response_future.get();
    std::string response_string(response_data.begin(), response_data.end());
    EXPECT_EQ(response_string, "Response from hub");

    client->disconnect();
    hub->stop();
}

// Test UDP client-server communication
TEST_F(AsyncIntegrationTest, UdpClientServerCommunication) {
    // Set up UDP server
    auto server = std::make_unique<UdpSocketHub>();
    std::vector<std::string> received_messages;
    std::vector<std::string> sender_ips;
    std::mutex messages_mutex;

    server->addMessageHandler([&](const std::string& message, const std::string& ip, unsigned short port) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.push_back(message);
        sender_ips.push_back(ip);
    });

    ASSERT_TRUE(server->start(udp_port_));

    // Set up UDP client
    auto client = std::make_unique<UdpClient>();
    ASSERT_TRUE(client->bind(0));

    // Test client to server communication
    std::string test_message = "Hello from UDP client";
    ASSERT_TRUE(client->send("127.0.0.1", udp_port_, test_message));

    std::this_thread::sleep_for(200ms);

    // Verify message received by server
    EXPECT_EQ(received_messages.size(), 1);
    if (!received_messages.empty()) {
        EXPECT_EQ(received_messages[0], test_message);
        EXPECT_EQ(sender_ips[0], "127.0.0.1");
    }

    // Test server to client communication
    auto client_endpoint = client->getLocalEndpoint();
    std::atomic<bool> response_received{false};
    std::string received_response;

    client->setOnDataReceivedCallback([&](const std::vector<char>& data, const std::string&, int) {
        received_response = std::string(data.begin(), data.end());
        response_received = true;
    });

    client->startReceiving(1024);

    ASSERT_TRUE(server->sendTo("Response from server", "127.0.0.1", client_endpoint.second));

    // Wait for response
    auto start_time = std::chrono::steady_clock::now();
    while (!response_received &&
           std::chrono::steady_clock::now() - start_time < 2s) {
        std::this_thread::sleep_for(10ms);
    }

    EXPECT_TRUE(response_received);
    EXPECT_EQ(received_response, "Response from server");

    client->stopReceiving();
    server->stop();
}

// Test FIFO client-server communication
TEST_F(AsyncIntegrationTest, FifoClientServerCommunication) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping on Windows due to Unix-specific FIFO behavior";
#endif

    // Set up FIFO server
    auto server = std::make_unique<FifoServer>(fifo_path_);
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    server->setMessageHandler([&](std::string_view data) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.emplace_back(data);
    });

    server->start([&](std::string_view data) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.emplace_back(data);
    });

    std::this_thread::sleep_for(100ms); // Let server start

    // Set up FIFO client
    auto client = std::make_unique<FifoClient>();
    ASSERT_TRUE(client->open(fifo_path_));

    // Test client to server communication
    std::string test_message = "Hello from FIFO client";
    auto write_future = client->write(test_message);
    ASSERT_TRUE(write_future.get());

    std::this_thread::sleep_for(200ms);

    // Verify message received by server
    EXPECT_GT(received_messages.size(), 0);
    if (!received_messages.empty()) {
        EXPECT_EQ(received_messages[0], test_message);
    }

    client->close();
    server->stop();
}

// Test mixed protocol communication scenario
TEST_F(AsyncIntegrationTest, MixedProtocolCommunication) {
    // Set up multiple servers
    auto tcp_hub = std::make_unique<SocketHub>();
    auto udp_server = std::make_unique<UdpSocketHub>();

    std::atomic<int> tcp_message_count{0};
    std::atomic<int> udp_message_count{0};

    tcp_hub->setMessageHandler([&](const std::string&, std::shared_ptr<asio::ip::tcp::socket>) {
        tcp_message_count.fetch_add(1);
    });

    udp_server->addMessageHandler([&](const std::string&, const std::string&, unsigned short) {
        udp_message_count.fetch_add(1);
    });

    ASSERT_TRUE(tcp_hub->start(tcp_port_));
    ASSERT_TRUE(udp_server->start(udp_port_));

    // Set up multiple clients
    auto tcp_client = std::make_unique<TcpClient>();
    auto udp_client = std::make_unique<UdpClient>();

    ASSERT_TRUE(tcp_client->connect("127.0.0.1", tcp_port_));
    ASSERT_TRUE(udp_client->bind(0));

    // Send messages from both clients
    const int num_messages = 10;

    for (int i = 0; i < num_messages; ++i) {
        tcp_client->sendString("TCP message " + std::to_string(i));
        udp_client->send("127.0.0.1", udp_port_, "UDP message " + std::to_string(i));
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(500ms);

    // Verify both protocols received messages
    EXPECT_EQ(tcp_message_count.load(), num_messages);
    EXPECT_EQ(udp_message_count.load(), num_messages);

    tcp_client->disconnect();
    tcp_hub->stop();
    udp_server->stop();
}

// Test error propagation across components
TEST_F(AsyncIntegrationTest, ErrorPropagationTest) {
    std::atomic<int> tcp_errors{0};
    std::atomic<int> udp_errors{0};

    // Set up clients with error handlers
    auto tcp_client = std::make_unique<TcpClient>();
    auto udp_client = std::make_unique<UdpClient>();

    tcp_client->setOnErrorCallback([&](const std::string&) {
        tcp_errors.fetch_add(1);
    });

    udp_client->setOnErrorCallback([&](const std::string&, int) {
        udp_errors.fetch_add(1);
    });

    // Try to connect to non-existent servers
    EXPECT_FALSE(tcp_client->connect("127.0.0.1", 99999)); // Invalid port
    EXPECT_FALSE(udp_client->send("192.0.2.1", 12345, "test")); // RFC5737 test address

    std::this_thread::sleep_for(200ms);

    // Verify errors were captured
    EXPECT_GT(tcp_errors.load() + udp_errors.load(), 0);
}

// Test concurrent multi-protocol operations
TEST_F(AsyncIntegrationTest, ConcurrentMultiProtocolOperations) {
    // Set up servers
    auto tcp_hub = std::make_unique<SocketHub>();
    auto udp_server = std::make_unique<UdpSocketHub>();

    std::atomic<int> total_messages{0};

    tcp_hub->setMessageHandler([&](const std::string&, std::shared_ptr<asio::ip::tcp::socket>) {
        total_messages.fetch_add(1);
    });

    udp_server->addMessageHandler([&](const std::string&, const std::string&, unsigned short) {
        total_messages.fetch_add(1);
    });

    ASSERT_TRUE(tcp_hub->start(tcp_port_));
    ASSERT_TRUE(udp_server->start(udp_port_));

    const int num_threads = 10;
    const int messages_per_thread = 5;
    std::vector<std::thread> threads;

    // Start concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t]() {
            if (t % 2 == 0) {
                // TCP operations
                auto client = std::make_unique<TcpClient>();
                if (client->connect("127.0.0.1", tcp_port_)) {
                    for (int i = 0; i < messages_per_thread; ++i) {
                        client->sendString("TCP " + std::to_string(t) + "_" + std::to_string(i));
                        std::this_thread::sleep_for(1ms);
                    }
                    client->disconnect();
                }
            } else {
                // UDP operations
                auto client = std::make_unique<UdpClient>();
                if (client->bind(0)) {
                    for (int i = 0; i < messages_per_thread; ++i) {
                        client->send("127.0.0.1", udp_port_, "UDP " + std::to_string(t) + "_" + std::to_string(i));
                        std::this_thread::sleep_for(1ms);
                    }
                }
            }
        });
    }

    // Wait for all operations to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::this_thread::sleep_for(500ms);

    // Verify messages were received
    int expected_messages = num_threads * messages_per_thread;
    EXPECT_GT(total_messages.load(), expected_messages / 2); // At least half should succeed

    tcp_hub->stop();
    udp_server->stop();
}
