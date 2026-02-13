// filepath: /home/max/Atom/atom/connection/test_udpclient.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "udpclient.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_ERROR_CODE WSAGetLastError()
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR_CODE errno
#define CLOSE_SOCKET(s) ::close(s)
#endif

using namespace atom::connection;

namespace {

// Helper function to get a free port and bind a socket to it
int bind_socket_to_free_port(sockaddr_in& addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -1;  // Socket creation failed
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
                   sizeof(reuse)) < 0) {
        CLOSE_SOCKET(sock);
        return -1;
    }
#ifndef _WIN32
    // Allow port reuse (useful for multiple listeners on the same port)
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse,
                   sizeof(reuse)) < 0) {
        // This might fail on some systems, not critical for basic test
    }
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // Use loopback for testing
    addr.sin_port = 0;                              // Request a free port

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        CLOSE_SOCKET(sock);
        return -1;  // Bind failed
    }

    // Get the assigned port
    socklen_t addr_len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr*)&addr, &addr_len) < 0) {
        CLOSE_SOCKET(sock);
        return -1;  // getsockname failed
    }

    return sock;
}

}  // namespace

class UdpClientReceivingLoopTest : public ::testing::Test {
protected:
    std::unique_ptr<UdpClient> client_;
    int sender_socket_ = INVALID_SOCKET;
    sockaddr_in client_addr_ = {};  // Address the UdpClient is bound to

    // Callbacks and synchronization
    std::promise<std::pair<std::vector<char>, RemoteEndpoint>> data_promise_;
    std::future<std::pair<std::vector<char>, RemoteEndpoint>> data_future_;

    std::promise<std::pair<UdpError, std::string>> error_promise_;
    std::future<std::pair<UdpError, std::string>> error_future_;

    std::promise<bool> status_promise_;
    std::future<bool> status_future_;
    std::mutex status_mutex_;
    std::condition_variable status_cv_;
    bool current_status_ = false;

    void SetUp() override {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        // Create and bind the client socket (which UdpClient will use)
        int client_sock = bind_socket_to_free_port(client_addr_);
        ASSERT_NE(client_sock, -1) << "Failed to bind client socket";

        // Create a UdpClient instance. We need to replace its internal socket
        // with the one we just created for testing the receiving loop.
        // This requires some test-specific access or modification to Impl.
        // A simpler approach is to test the public startReceiving method
        // which uses the internal socket created by the UdpClient constructor.
        // Let's switch to testing startReceiving/stopReceiving.

        // Revised Setup: Create UdpClient and let it bind
        client_ = std::make_unique<UdpClient>();
        auto bind_result = client_->bind(0);  // Bind to any free port
        ASSERT_TRUE(bind_result)
            << "Failed to bind UdpClient: "
            << static_cast<int>(bind_result.error().error());

        auto port_result = client_->getLocalPort();
        ASSERT_TRUE(port_result)
            << "Failed to get local port: "
            << static_cast<int>(port_result.error().error());
        uint16_t bound_port = port_result.value();

        // Create a sender socket
        sender_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        ASSERT_NE(sender_socket_, INVALID_SOCKET)
            << "Failed to create sender socket";

        // Set up futures
        data_future_ = data_promise_.get_future();
        error_future_ = error_promise_.get_future();
        status_future_ = status_promise_.get_future();

        // Set callbacks
        client_->setOnDataReceivedCallback(
            [this](std::span<const char> data, const RemoteEndpoint& endpoint) {
                std::vector<char> received_data(data.begin(), data.end());
                data_promise_.set_value({received_data, endpoint});
            });

        client_->setOnErrorCallback(
            [this](UdpError error, const std::string& msg) {
                error_promise_.set_value({error, msg});
            });

        client_->setOnStatusChangeCallback([this](bool status) {
            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                current_status_ = status;
            }
            status_promise_.set_value(
                status);  // Only signals the first status change
            status_cv_.notify_one();
        });
    }

    void TearDown() override {
        if (client_) {
            client_->stopReceiving();
            client_->close();
        }
        if (sender_socket_ != INVALID_SOCKET) {
            CLOSE_SOCKET(sender_socket_);
            sender_socket_ = INVALID_SOCKET;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Helper to wait for a specific status change
    bool waitForStatus(bool expected_status,
                       std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(status_mutex_);
        return status_cv_.wait_for(lock, timeout, [this, expected_status] {
            return current_status_ == expected_status;
        });
    }

    // Helper to send data from the sender socket
    ssize_t sendData(const std::string& host, uint16_t port,
                     std::span<const char> data) {
        struct sockaddr_in dest_addr {};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &dest_addr.sin_addr) <= 0) {
            return -1;  // Invalid address
        }

        return sendto(sender_socket_, data.data(), data.size(), 0,
                      (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    }
};

TEST_F(UdpClientReceivingLoopTest, StartAndStopReceiving) {
    // Start receiving
    auto start_result = client_->startReceiving(1024);
    ASSERT_TRUE(start_result) << "startReceiving failed: "
                              << static_cast<int>(start_result.error().error());

    // Wait for status change to true (receiving started)
    EXPECT_TRUE(waitForStatus(true, std::chrono::seconds(1)))
        << "Client did not report starting receiving";
    EXPECT_TRUE(client_->isReceiving());

    // Stop receiving
    client_->stopReceiving();

    // Wait for status change to false (receiving stopped)
    // Need a new promise/future for the second status change
    std::promise<bool> stop_status_promise;
    std::future<bool> stop_status_future = stop_status_promise.get_future();
    client_->setOnStatusChangeCallback([&](bool status) {
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            current_status_ = status;
        }
        stop_status_promise.set_value(status);
        status_cv_.notify_one();
    });

    // Trigger stop again to ensure the callback is set before the thread
    // potentially exits
    client_->stopReceiving();  // Safe to call multiple times

    EXPECT_TRUE(waitForStatus(false, std::chrono::seconds(1)))
        << "Client did not report stopping receiving";
    EXPECT_FALSE(client_->isReceiving());
}

TEST_F(UdpClientReceivingLoopTest, ReceiveSinglePacket) {
    // Start receiving
    auto start_result = client_->startReceiving(1024);
    ASSERT_TRUE(start_result) << "startReceiving failed: "
                              << static_cast<int>(start_result.error().error());
    ASSERT_TRUE(waitForStatus(true, std::chrono::seconds(1)))
        << "Client did not report starting receiving";

    // Get the client's bound port
    auto port_result = client_->getLocalPort();
    ASSERT_TRUE(port_result) << "Failed to get local port: "
                             << static_cast<int>(port_result.error().error());
    uint16_t bound_port = port_result.value();

    // Data to send
    std::string test_data_str = "Hello UDP!";
    std::vector<char> test_data(test_data_str.begin(), test_data_str.end());

    // Send data from sender socket to the client's address and port
    ssize_t bytes_sent = sendData("127.0.0.1", bound_port, test_data);
    ASSERT_EQ(bytes_sent, test_data.size()) << "Failed to send data";

    // Wait for the data received callback
    auto future_status = data_future_.wait_for(std::chrono::seconds(1));
    ASSERT_EQ(future_status, std::future_status::ready)
        << "Timeout waiting for data callback";

    // Get the received data and endpoint
    auto received_pair = data_future_.get();
    std::vector<char> received_data = received_pair.first;
    RemoteEndpoint remote_endpoint = received_pair.second;

    // Verify received data
    ASSERT_EQ(received_data.size(), test_data.size());
    EXPECT_EQ(std::string(received_data.begin(), received_data.end()),
              test_data_str);

    // Verify remote endpoint (sender's address and a dynamic port)
    EXPECT_EQ(remote_endpoint.host, "127.0.0.1");
    EXPECT_GT(remote_endpoint.port, 0);  // Sender's port will be dynamic

    // Verify statistics
    UdpStatistics stats = client_->getStatistics();
    EXPECT_EQ(stats.packetsReceived, 1);
    EXPECT_EQ(stats.bytesReceived, test_data.size());
    EXPECT_EQ(stats.receiveErrors, 0);
    // Note: Send statistics are not updated by the receiving loop

    // Stop receiving
    client_->stopReceiving();
}

TEST_F(UdpClientReceivingLoopTest, ReceiveMultiplePackets) {
    // Start receiving
    auto start_result = client_->startReceiving(1024);
    ASSERT_TRUE(start_result) << "startReceiving failed: "
                              << static_cast<int>(start_result.error().error());
    ASSERT_TRUE(waitForStatus(true, std::chrono::seconds(1)))
        << "Client did not report starting receiving";

    // Get the client's bound port
    auto port_result = client_->getLocalPort();
    ASSERT_TRUE(port_result) << "Failed to get local port: "
                             << static_cast<int>(port_result.error().error());
    uint16_t bound_port = port_result.value();

    // Data to send
    std::vector<std::string> test_packets = {"Packet 1", "Packet 2",
                                             "Packet 3"};
    std::vector<std::vector<char>> test_data;
    for (const auto& s : test_packets) {
        test_data.emplace_back(s.begin(), s.end());
    }

    // Use promises/futures for each packet
    std::vector<std::promise<std::pair<std::vector<char>, RemoteEndpoint>>>
        packet_promises(test_packets.size());
    std::vector<std::future<std::pair<std::vector<char>, RemoteEndpoint>>>
        packet_futures;
    for (size_t i = 0; i < test_packets.size(); ++i) {
        packet_futures.push_back(packet_promises[i].get_future());
    }

    // Override the data callback to handle multiple packets
    std::atomic<int> packets_received_count = 0;
    client_->setOnDataReceivedCallback(
        [&](std::span<const char> data, const RemoteEndpoint& endpoint) {
            int index = packets_received_count.fetch_add(1);
            if (index < packet_promises.size()) {
                std::vector<char> received_data(data.begin(), data.end());
                packet_promises[index].set_value({received_data, endpoint});
            }
        });

    // Send data from sender socket
    for (const auto& data : test_data) {
        ssize_t bytes_sent = sendData("127.0.0.1", bound_port, data);
        ASSERT_EQ(bytes_sent, data.size()) << "Failed to send data";
        // Add a small delay to help ensure packets are processed sequentially
        // by the test logic
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for all packets to be received
    for (size_t i = 0; i < test_packets.size(); ++i) {
        auto future_status =
            packet_futures[i].wait_for(std::chrono::seconds(1));
        ASSERT_EQ(future_status, std::future_status::ready)
            << "Timeout waiting for packet " << i;
        auto received_pair = packet_futures[i].get();
        std::vector<char> received_data = received_pair.first;
        RemoteEndpoint remote_endpoint = received_pair.second;

        // Verify received data
        ASSERT_EQ(received_data.size(), test_data[i].size());
        EXPECT_EQ(std::string(received_data.begin(), received_data.end()),
                  test_packets[i]);
        EXPECT_EQ(remote_endpoint.host, "127.0.0.1");
        EXPECT_GT(remote_endpoint.port, 0);
    }

    // Verify total statistics
    UdpStatistics stats = client_->getStatistics();
    EXPECT_EQ(stats.packetsReceived, test_packets.size());
    size_t total_bytes = 0;
    for (const auto& data : test_data)
        total_bytes += data.size();
    EXPECT_EQ(stats.bytesReceived, total_bytes);
    EXPECT_EQ(stats.receiveErrors, 0);

    // Stop receiving
    client_->stopReceiving();
}

TEST_F(UdpClientReceivingLoopTest, StartReceivingInvalidBufferSize) {
    // Test buffer size 0
    auto start_result_zero = client_->startReceiving(0);
    EXPECT_FALSE(start_result_zero);
    EXPECT_EQ(start_result_zero.error(), UdpError::InvalidParameter);
    EXPECT_FALSE(client_->isReceiving());

    // Test buffer size > MAX_BUFFER_SIZE (65536)
    auto start_result_large = client_->startReceiving(65537);
    EXPECT_FALSE(start_result_large);
    EXPECT_EQ(start_result_large.error(), UdpError::InvalidParameter);
    EXPECT_FALSE(client_->isReceiving());
}

TEST_F(UdpClientReceivingLoopTest, StartReceivingWithoutDataCallback) {
    // Unset the data callback
    client_->setOnDataReceivedCallback(nullptr);

    // Start receiving - should fail
    auto start_result = client_->startReceiving(1024);
    EXPECT_FALSE(start_result);
    EXPECT_EQ(start_result.error(), UdpError::InvalidParameter);
    EXPECT_FALSE(client_->isReceiving());
}

TEST_F(UdpClientReceivingLoopTest, StopReceivingWhenNotReceiving) {
    // Ensure client is not receiving initially
    EXPECT_FALSE(client_->isReceiving());

    // Call stopReceiving - should be safe and do nothing
    client_->stopReceiving();
    EXPECT_FALSE(client_->isReceiving());

    // Call stopReceiving again
    client_->stopReceiving();
    EXPECT_FALSE(client_->isReceiving());
}

TEST_F(UdpClientReceivingLoopTest, StartReceivingWhenAlreadyReceiving) {
    // Start receiving the first time
    auto start_result1 = client_->startReceiving(1024);
    ASSERT_TRUE(start_result1) << "First startReceiving failed";
    ASSERT_TRUE(waitForStatus(true, std::chrono::seconds(1)))
        << "Client did not report starting receiving (1st time)";
    EXPECT_TRUE(client_->isReceiving());

    // Reset status promise for the second start
}
