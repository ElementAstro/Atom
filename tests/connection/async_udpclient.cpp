#include "atom/connection/async_udpclient.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <asio.hpp>
#include <future>
#include <thread>

// Silence spdlog during tests for cleaner output
#define SPDLOG_LEVEL_OFF
#include <spdlog/spdlog.h>

using namespace atom::async::connection;
using namespace std::chrono_literals;

/**
 * @brief A simple UDP echo server for testing purposes.
 * It listens on a port and echoes back any received datagrams.
 */
class MockUdpEchoServer {
public:
    MockUdpEchoServer() : socket_(io_context_) {}

    ~MockUdpEchoServer() { stop(); }

    // Starts the server on a given port
    void start(uint16_t port) {
        port_ = port;
        server_thread_ = std::thread([this, port] {
            try {
                asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);
                socket_.open(endpoint.protocol());
                socket_.bind(endpoint);
                do_receive();
                io_context_.run();
            } catch (const std::exception& e) {
                // Can happen during shutdown, which is expected.
            }
        });
        // Give the thread a moment to start listening
        std::this_thread::sleep_for(50ms);
    }

    void stop() {
        if (!server_thread_.joinable())
            return;
        asio::post(io_context_, [this]() {
            socket_.close();
            io_context_.stop();
        });
        server_thread_.join();
    }

    uint16_t getPort() const { return port_; }

private:
    void do_receive() {
        socket_.async_receive_from(
            asio::buffer(data_, max_length), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    // Echo the data back to the sender
                    socket_.async_send_to(
                        asio::buffer(data_, bytes_recvd), remote_endpoint_,
                        [this](std::error_code /*ec*/,
                               std::size_t /*bytes_sent*/) {
                            do_receive();  // Continue listening
                        });
                } else {
                    // Stop on error (likely caused by closing the socket)
                }
            });
    }

    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
    std::thread server_thread_;
    uint16_t port_{0};
    enum { max_length = 1024 };
    char data_[max_length];
};

// Main test fixture
class UdpClientTest : public ::testing::Test {
protected:
    std::unique_ptr<UdpClient> client_;
    std::unique_ptr<MockUdpEchoServer> server_;
    uint16_t server_port_;

    void SetUp() override {
        server_ = std::make_unique<MockUdpEchoServer>();
        // Find a free port for the server to listen on
        asio::io_context ctx;
        asio::ip::udp::socket sock(
            ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        server_port_ = sock.local_endpoint().port();
        sock.close();

        server_->start(server_port_);
        client_ = std::make_unique<UdpClient>();
    }

    void TearDown() override {
        if (client_) {
            client_->close();
        }
        if (server_) {
            server_->stop();
        }
        // Give the OS a moment to release resources
        std::this_thread::sleep_for(50ms);
    }
};

TEST_F(UdpClientTest, InitialState) {
    EXPECT_FALSE(client_->isOpen());
    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.packets_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
}

TEST_F(UdpClientTest, Bind) {
    ASSERT_TRUE(client_->bind(0));  // Bind to any available port
    EXPECT_TRUE(client_->isOpen());
    auto endpoint = client_->getLocalEndpoint();
    EXPECT_NE(endpoint.second, 0);  // Port should be non-zero
}

TEST_F(UdpClientTest, SendAndReceiveSync) {
    ASSERT_TRUE(client_->bind(0));

    std::string message = "Hello, UDP!";
    ASSERT_TRUE(client_->send("127.0.0.1", server_port_, message));

    std::string remote_host;
    int remote_port;
    auto received_data = client_->receive(1024, remote_host, remote_port);

    ASSERT_FALSE(received_data.empty());
    std::string received_string(received_data.begin(), received_data.end());

    EXPECT_EQ(received_string, message);
    EXPECT_EQ(remote_host, "127.0.0.1");
    EXPECT_EQ(remote_port, server_port_);

    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.packets_sent, 1);
    EXPECT_EQ(stats.bytes_sent, message.length());
    EXPECT_EQ(stats.packets_received, 1);
    EXPECT_EQ(stats.bytes_received, message.length());
}

TEST_F(UdpClientTest, ReceiveWithTimeout) {
    ASSERT_TRUE(client_->bind(0));

    // This should time out and return an empty vector
    std::string remote_host;
    int remote_port;
    auto received_data =
        client_->receive(1024, remote_host, remote_port, 100ms);
    EXPECT_TRUE(received_data.empty());
}

TEST_F(UdpClientTest, AsynchronousReceive) {
    ASSERT_TRUE(client_->bind(0));
    auto client_port = client_->getLocalEndpoint().second;

    std::promise<std::string> received_promise;
    client_->setOnDataReceivedCallback(
        [&](const std::vector<char>& data, const std::string& host, int port) {
            EXPECT_EQ(host, "127.0.0.1");
            EXPECT_EQ(port, server_port_);
            received_promise.set_value(std::string(data.begin(), data.end()));
        });

    client_->startReceiving();

    std::string message = "Async Hello!";
    // Use a different client to send the message to avoid any conflicts
    UdpClient sender;
    ASSERT_TRUE(sender.send("127.0.0.1", client_port, message));

    auto future = received_promise.get_future();
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get(), message);

    client_->stopReceiving();
}

TEST_F(UdpClientTest, ResetStatistics) {
    ASSERT_TRUE(client_->bind(0));
    client_->send("127.0.0.1", server_port_, "data");

    std::string host;
    int port;
    client_->receive(1024, host, port, 500ms);

    auto stats = client_->getStatistics();
    ASSERT_GT(stats.packets_sent, 0);
    ASSERT_GT(stats.packets_received, 0);

    client_->resetStatistics();
    stats = client_->getStatistics();
    EXPECT_EQ(stats.packets_sent, 0);
    EXPECT_EQ(stats.packets_received, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
}

TEST_F(UdpClientTest, Broadcast) {
    // Note: Broadcast tests can be flaky depending on network
    // configuration/permissions.
    UdpClient receiver;
    ASSERT_TRUE(receiver.bind(
        server_port_));  // Bind to the same port as our dummy server
    ASSERT_TRUE(
        receiver.setSocketOption(UdpClient::SocketOption::ReuseAddress, 1));

    std::promise<std::string> received_promise;
    receiver.setOnDataReceivedCallback([&](const auto& data, auto, auto) {
        received_promise.set_value({data.begin(), data.end()});
    });
    receiver.startReceiving();

    // The sender must enable broadcast
    ASSERT_TRUE(
        client_->setSocketOption(UdpClient::SocketOption::Broadcast, 1));

    std::string broadcast_msg = "Broadcast test!";
    // Send to the broadcast address
    ASSERT_TRUE(client_->send("255.255.255.255", server_port_, broadcast_msg));

    auto future = received_promise.get_future();
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get(), broadcast_msg);
}

TEST_F(UdpClientTest, Multicast) {
    const std::string multicast_address = "239.255.0.1";
    const int multicast_port = server_port_;

    UdpClient receiver1, receiver2;
    // Both receivers must bind to the same port and enable reuse_address
    ASSERT_TRUE(
        receiver1.setSocketOption(UdpClient::SocketOption::ReuseAddress, 1));
    ASSERT_TRUE(
        receiver2.setSocketOption(UdpClient::SocketOption::ReuseAddress, 1));
    ASSERT_TRUE(receiver1.bind(multicast_port));
    ASSERT_TRUE(receiver2.bind(multicast_port));

    // Both join the multicast group
    ASSERT_TRUE(receiver1.joinMulticastGroup(multicast_address));
    ASSERT_TRUE(receiver2.joinMulticastGroup(multicast_address));

    std::promise<void> r1_promise, r2_promise;
    receiver1.setOnDataReceivedCallback(
        [&](auto, auto, auto) { r1_promise.set_value(); });
    receiver2.setOnDataReceivedCallback(
        [&](auto, auto, auto) { r2_promise.set_value(); });
    receiver1.startReceiving();
    receiver2.startReceiving();

    // Sender sends a packet to the multicast group
    std::string msg = "Multicast!";
    ASSERT_TRUE(client_->send(multicast_address, multicast_port, msg));

    // Both should receive it
    EXPECT_EQ(r1_promise.get_future().wait_for(1s), std::future_status::ready);
    EXPECT_EQ(r2_promise.get_future().wait_for(1s), std::future_status::ready);

    // Now, receiver2 leaves the group
    ASSERT_TRUE(receiver2.leaveMulticastGroup(multicast_address));
    receiver2.stopReceiving();

    // Reset promise for receiver1
    r1_promise = std::promise<void>();

    // Send another message
    ASSERT_TRUE(client_->send(multicast_address, multicast_port, msg));

    // Only receiver1 should get it now
    EXPECT_EQ(r1_promise.get_future().wait_for(1s), std::future_status::ready);
    // We expect receiver2 to NOT get a message, so we don't check its promise.
}

TEST_F(UdpClientTest, ErrorCallback) {
    std::promise<std::string> error_promise;
    client_->setOnErrorCallback(
        [&](const std::string& msg, [[maybe_unused]] int code) {
            error_promise.set_value(msg);
        });

    // Try to set an option on a closed socket
    client_->setSocketOption(UdpClient::SocketOption::Broadcast, 1);

    auto future = error_promise.get_future();
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_NE(future.get().find("Socket not open"), std::string::npos);
}

// ============================================================================
// ENHANCED TESTS FOR COMPREHENSIVE COVERAGE
// ============================================================================

// Test send with timeout functionality
TEST_F(UdpClientTest, SendWithTimeout) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0)); // Bind to any available port

    std::vector<char> test_data = {'H', 'e', 'l', 'l', 'o'};

    auto start_time = std::chrono::steady_clock::now();
    bool result = client_->sendWithTimeout("127.0.0.1", server_->getPort(), test_data, std::chrono::milliseconds(1000));
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_TRUE(result);
    EXPECT_LT(duration.count(), 1200); // Should complete within timeout + tolerance
}

// Test batch send functionality
TEST_F(UdpClientTest, BatchSend) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    std::vector<std::pair<std::string, int>> destinations = {
        {"127.0.0.1", server_->getPort()},
        {"127.0.0.1", server_->getPort()},
        {"127.0.0.1", server_->getPort()}
    };

    std::vector<char> test_data = {'B', 'a', 't', 'c', 'h'};

    int success_count = client_->batchSend(destinations, test_data);
    EXPECT_EQ(success_count, 3); // All sends should succeed
}

// Test multicast functionality
TEST_F(UdpClientTest, MulticastOperations) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    std::string multicast_address = "224.0.0.1";
    std::string interface_address = "127.0.0.1";

    // Join multicast group
    bool joined = client_->joinMulticastGroup(multicast_address, interface_address);
    EXPECT_TRUE(joined);

    // Leave multicast group
    bool left = client_->leaveMulticastGroup(multicast_address, interface_address);
    EXPECT_TRUE(left);
}

// Test TTL setting
TEST_F(UdpClientTest, TTLSetting) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    // Set TTL
    bool ttl_set = client_->setTTL(64);
    EXPECT_TRUE(ttl_set);

    // Try invalid TTL
    bool invalid_ttl = client_->setTTL(-1);
    EXPECT_FALSE(invalid_ttl);
}

// Test socket options
TEST_F(UdpClientTest, SocketOptions) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    // Test broadcast option
    bool broadcast_set = client_->setSocketOption(UdpClient::SocketOption::Broadcast, 1);
    EXPECT_TRUE(broadcast_set);

    // Test reuse address option
    bool reuse_set = client_->setSocketOption(UdpClient::SocketOption::ReuseAddress, 1);
    EXPECT_TRUE(reuse_set);

    // Test buffer size options
    bool recv_buffer_set = client_->setSocketOption(UdpClient::SocketOption::ReceiveBufferSize, 8192);
    EXPECT_TRUE(recv_buffer_set);

    bool send_buffer_set = client_->setSocketOption(UdpClient::SocketOption::SendBufferSize, 8192);
    EXPECT_TRUE(send_buffer_set);
}

// Test asynchronous receiving
TEST_F(UdpClientTest, AsynchronousReceiving) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    std::atomic<int> received_count{0};
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    // Set up data received callback
    client_->setOnDataReceivedCallback([&](const std::vector<char>& data, const std::string& /*ip*/, int /*port*/) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.emplace_back(data.begin(), data.end());
        received_count.fetch_add(1);
    });

    // Start receiving
    client_->startReceiving(1024);

    // Send some test data to ourselves
    auto local_endpoint = client_->getLocalEndpoint();
    std::string test_message = "Async test message";

    for (int i = 0; i < 3; ++i) {
        bool sent = client_->send("127.0.0.1", local_endpoint.second, test_message + std::to_string(i));
        EXPECT_TRUE(sent);
        std::this_thread::sleep_for(10ms);
    }

    // Wait for messages to be received
    std::this_thread::sleep_for(200ms);

    // Stop receiving
    client_->stopReceiving();

    // Verify messages were received
    EXPECT_GT(received_count.load(), 0);
    EXPECT_GT(received_messages.size(), 0);
}

// Test error handling callbacks
TEST_F(UdpClientTest, ErrorHandlingCallbacks) {
    client_ = std::make_unique<UdpClient>();

    std::atomic<int> error_count{0};
    std::vector<std::string> error_messages;
    std::mutex errors_mutex;

    // Set up error callback
    client_->setOnErrorCallback([&](const std::string& error, int /*code*/) {
        std::lock_guard<std::mutex> lock(errors_mutex);
        error_messages.push_back(error);
        error_count.fetch_add(1);
    });

    // Try operations that should cause errors
    // Send without binding
    [[maybe_unused]] bool sent = client_->send("127.0.0.1", 12345, "Error test");
    // This might succeed or fail depending on implementation

    // Try to bind to an invalid port
    bool bound = client_->bind(99999); // Invalid port
    EXPECT_FALSE(bound);

    std::this_thread::sleep_for(100ms);

    // Should have captured some errors
    EXPECT_GT(error_count.load(), 0);
}

// Test statistics functionality
TEST_F(UdpClientTest, Statistics) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    // Get initial statistics
    auto initial_stats = client_->getStatistics();

    // Send some data
    std::string test_data = "Statistics test";
    for (int i = 0; i < 5; ++i) {
        client_->send("127.0.0.1", server_->getPort(), test_data);
    }

    // Get updated statistics
    auto updated_stats = client_->getStatistics();
    EXPECT_GT(updated_stats.packets_sent, initial_stats.packets_sent);
    EXPECT_GT(updated_stats.bytes_sent, initial_stats.bytes_sent);

    // Reset statistics
    client_->resetStatistics();
    auto reset_stats = client_->getStatistics();
    EXPECT_EQ(reset_stats.packets_sent, 0);
    EXPECT_EQ(reset_stats.bytes_sent, 0);
}

// Test concurrent operations
TEST_F(UdpClientTest, ConcurrentOperations) {
    client_ = std::make_unique<UdpClient>();
    ASSERT_TRUE(client_->bind(0));

    const int num_threads = 10;
    const int messages_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Start multiple threads sending data concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &success_count]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                std::string message = "Thread" + std::to_string(t) + "_Msg" + std::to_string(i);
                if (client_->send("127.0.0.1", server_->getPort(), message)) {
                    success_count.fetch_add(1);
                }
                std::this_thread::sleep_for(1ms); // Small delay
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Most sends should succeed
    EXPECT_GT(success_count.load(), (num_threads * messages_per_thread) / 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
