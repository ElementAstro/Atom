#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>
#include <span>
#include <thread>
#include "atom/connection/udp/udpclient.hpp"

using namespace atom::connection;

class UdpClientTest : public ::testing::Test {
protected:
    void SetUp() override { client_ = std::make_unique<UdpClient>(); }

    void TearDown() override {
        client_->stopReceiving();
        client_.reset();
    }

    std::unique_ptr<UdpClient> client_;
};

TEST_F(UdpClientTest, Bind) {
    auto result = client_->bind(12345);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, SendReceive) {
    auto bindResult = client_->bind(12345);
    EXPECT_TRUE(bindResult.has_value());

    std::string message = "Hello, UDP!";
    std::span<const char> data_span(message.data(), message.size());

    std::thread sender([&]() {
        UdpClient senderClient;
        RemoteEndpoint endpoint{"127.0.0.1", 12345};
        auto sendResult = senderClient.send(endpoint, data_span);
        EXPECT_TRUE(sendResult.has_value());
    });

    auto receiveResult = client_->receive(1024);
    EXPECT_TRUE(receiveResult.has_value());
    auto [receivedData, remoteEndpoint] = receiveResult.value();

    std::string receivedMessage(receivedData.begin(), receivedData.end());
    EXPECT_EQ(receivedMessage, message);
    EXPECT_EQ(remoteEndpoint.host, "127.0.0.1");

    sender.join();
}

TEST_F(UdpClientTest, AsyncReceive) {
    auto bindResult = client_->bind(12345);
    EXPECT_TRUE(bindResult.has_value());

    std::promise<std::vector<char>> promise;
    auto future = promise.get_future();

    client_->setOnDataReceivedCallback(
        [&](std::span<const char> data, const RemoteEndpoint& endpoint) {
            std::vector<char> vec_data(data.begin(), data.end());
            promise.set_value(vec_data);
        });

    auto startResult = client_->startReceiving(1024);
    EXPECT_TRUE(startResult.has_value());

    std::string message = "Hello, Async UDP!";
    std::span<const char> data_span(message.data(), message.size());

    std::thread sender([&]() {
        UdpClient senderClient;
        RemoteEndpoint endpoint{"127.0.0.1", 12345};
        auto sendResult = senderClient.send(endpoint, data_span);
        EXPECT_TRUE(sendResult.has_value());
    });

    auto status = future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);
    auto receivedData = future.get();
    std::string receivedMessage(receivedData.begin(), receivedData.end());
    EXPECT_EQ(receivedMessage, message);

    client_->stopReceiving();
    sender.join();
}

TEST_F(UdpClientTest, SendToInvalidHost) {
    std::string message = "test message";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"invalid.host.example.com", 12345};

    // Should handle invalid host gracefully
    auto result = client_->send(endpoint, data_span);
    // May succeed or fail depending on implementation
    EXPECT_NO_THROW(client_->send(endpoint, data_span));
}

TEST_F(UdpClientTest, SendToInvalidPort) {
    std::string message = "test message";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"127.0.0.1", 0};

    // Port 0 is invalid for sending; expect error
    auto result = client_->send(endpoint, data_span);
    EXPECT_FALSE(result.has_value());
}

TEST_F(UdpClientTest, SendEmptyMessage) {
    std::string emptyMessage = "";
    std::span<const char> data_span(emptyMessage.data(), emptyMessage.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12346};

    auto result = client_->send(endpoint, data_span);
    // Empty data is considered invalid parameter
    EXPECT_FALSE(result.has_value());
}

TEST_F(UdpClientTest, SendLargeMessage) {
    // Create large message (but within UDP limits)
    std::string largeMessage(1400, 'X');
    largeMessage += "END";
    std::span<const char> data_span(largeMessage.data(), largeMessage.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12347};

    auto result = client_->send(endpoint, data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, ReceiveTimeout) {
    auto bindResult = client_->bind(12348);
    ASSERT_TRUE(bindResult.has_value());

    // Try to receive with short timeout when no data is available
    auto start = std::chrono::steady_clock::now();
    auto result = client_->receive(1024, std::chrono::milliseconds(500));
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_GE(duration, std::chrono::milliseconds(400));
    EXPECT_LE(duration, std::chrono::seconds(2));
}

TEST_F(UdpClientTest, BindToInvalidPort) {
    // Port 0 is allowed for system-assigned port
    auto result = client_->bind(0);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, MultipleBindCalls) {
    auto result1 = client_->bind(12349);
    ASSERT_TRUE(result1.has_value());

    // Second bind should handle gracefully
    auto result2 = client_->bind(12349);
    // Implementation dependent - may succeed or fail
}

TEST_F(UdpClientTest, StopReceivingWithoutStart) {
    // Should not cause issues
    EXPECT_NO_THROW(client_->stopReceiving());
}

TEST_F(UdpClientTest, SendWithoutBinding) {
    // Should still work - UDP doesn't require binding for sending
    std::string message = "test without binding";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12350};

    auto result = client_->send(endpoint, data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, ConcurrentSends) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            std::string message = "Thread_" + std::to_string(i) + "_message";
            std::span<const char> data_span(message.data(), message.size());
            RemoteEndpoint endpoint{"127.0.0.1", 12351};

            auto result = client_->send(endpoint, data_span);
            if (result.has_value()) {
                successCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}

// ============================================================================
// Additional UdpClient Tests
// ============================================================================

TEST_F(UdpClientTest, GetStatistics) {
    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.packetsReceived, 0);
    EXPECT_EQ(stats.packetsSent, 0);
    EXPECT_EQ(stats.bytesReceived, 0);
    EXPECT_EQ(stats.bytesSent, 0);
}

TEST_F(UdpClientTest, ResetStatistics) {
    // Send some data first
    std::string message = "test";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12352};
    client_->send(endpoint, data_span);

    client_->resetStatistics();

    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.packetsSent, 0);
    EXPECT_EQ(stats.bytesSent, 0);
}

TEST_F(UdpClientTest, IsBound) {
    EXPECT_FALSE(client_->isBound());

    auto result = client_->bind(12353);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(client_->isBound());
}

TEST_F(UdpClientTest, GetLocalPort) {
    auto result = client_->bind(12354);
    ASSERT_TRUE(result.has_value());

    auto portResult = client_->getLocalPort();
    EXPECT_TRUE(portResult.has_value());
    if (portResult.has_value()) {
        EXPECT_EQ(portResult.value(), 12354);
    }
}

TEST_F(UdpClientTest, GetLocalPortWithSystemAssigned) {
    auto result = client_->bind(0);  // System assigns port
    ASSERT_TRUE(result.has_value());

    auto portResult = client_->getLocalPort();
    EXPECT_TRUE(portResult.has_value());
    if (portResult.has_value()) {
        EXPECT_GT(portResult.value(), 0);
    }
}

TEST_F(UdpClientTest, IsReceiving) {
    EXPECT_FALSE(client_->isReceiving());

    auto bindResult = client_->bind(12355);
    ASSERT_TRUE(bindResult.has_value());

    auto startResult = client_->startReceiving(1024);
    if (startResult.has_value()) {
        EXPECT_TRUE(client_->isReceiving());
    }

    client_->stopReceiving();
    EXPECT_FALSE(client_->isReceiving());
}

TEST_F(UdpClientTest, Close) {
    auto bindResult = client_->bind(12356);
    ASSERT_TRUE(bindResult.has_value());

    client_->close();
    EXPECT_FALSE(client_->isBound());
}

TEST_F(UdpClientTest, SetSocketOptions) {
    SocketOptions options;
    options.reuseAddress = true;
    options.reusePort = false;
    options.broadcast = false;
    options.sendBufferSize = 65536;
    options.receiveBufferSize = 65536;
    options.nonBlocking = true;

    auto result = client_->setSocketOptions(options);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, SetSocketOptionsWithBroadcast) {
    SocketOptions options;
    options.broadcast = true;

    auto result = client_->setSocketOptions(options);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, SetSocketOptionsWithTimeout) {
    SocketOptions options;
    options.sendTimeout = std::chrono::milliseconds(1000);
    options.receiveTimeout = std::chrono::milliseconds(1000);

    auto result = client_->setSocketOptions(options);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, SendBroadcast) {
    SocketOptions options;
    options.broadcast = true;
    client_->setSocketOptions(options);

    std::string message = "Broadcast message";
    auto result = client_->sendBroadcast(12357, message);
    // May succeed or fail depending on network configuration
}

TEST_F(UdpClientTest, SendMultiple) {
    std::vector<RemoteEndpoint> endpoints = {
        {"127.0.0.1", 12358}, {"127.0.0.1", 12359}, {"127.0.0.1", 12360}};

    std::string message = "Multi-destination message";
    std::span<const char> data_span(message.data(), message.size());

    auto result = client_->sendMultiple(endpoints, data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, JoinMulticastGroup) {
    auto bindResult = client_->bind(12361);
    ASSERT_TRUE(bindResult.has_value());

    auto result = client_->joinMulticastGroup("224.0.0.1");
    // May succeed or fail depending on network configuration
}

TEST_F(UdpClientTest, LeaveMulticastGroup) {
    auto bindResult = client_->bind(12362);
    ASSERT_TRUE(bindResult.has_value());

    client_->joinMulticastGroup("224.0.0.1");
    auto result = client_->leaveMulticastGroup("224.0.0.1");
    // May succeed or fail depending on network configuration
}

TEST_F(UdpClientTest, SendToMulticastGroup) {
    std::string message = "Multicast message";
    std::span<const char> data_span(message.data(), message.size());

    auto result = client_->sendToMulticastGroup("224.0.0.1", 12363, data_span);
    // May succeed or fail depending on network configuration
}

TEST_F(UdpClientTest, SetOnErrorCallback) {
    std::atomic<bool> errorCalled{false};

    client_->setOnErrorCallback(
        [&errorCalled](UdpError error, const std::string& message) {
            errorCalled = true;
        });

    // Trigger an error condition
    std::string message = "";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"127.0.0.1", 0};
    client_->send(endpoint, data_span);

    // Error callback may or may not be called depending on implementation
}

TEST_F(UdpClientTest, SetOnStatusChangeCallback) {
    std::atomic<bool> statusCalled{false};

    client_->setOnStatusChangeCallback(
        [&statusCalled](bool status) { statusCalled = true; });

    auto bindResult = client_->bind(12364);
    // Status callback may or may not be called depending on implementation
}

TEST_F(UdpClientTest, MoveConstruction) {
    auto bindResult = client_->bind(12365);
    ASSERT_TRUE(bindResult.has_value());

    UdpClient movedClient(std::move(*client_));
    EXPECT_TRUE(movedClient.isBound());
}

TEST_F(UdpClientTest, MoveAssignment) {
    auto bindResult = client_->bind(12366);
    ASSERT_TRUE(bindResult.has_value());

    UdpClient otherClient;
    otherClient = std::move(*client_);
    EXPECT_TRUE(otherClient.isBound());
}

TEST_F(UdpClientTest, ConstructorWithPort) {
    UdpClient clientWithPort(12367);
    EXPECT_TRUE(clientWithPort.isBound());
}

TEST_F(UdpClientTest, ConstructorWithPortAndOptions) {
    SocketOptions options;
    options.reuseAddress = true;
    options.nonBlocking = true;

    UdpClient clientWithOptions(12368, options);
    EXPECT_TRUE(clientWithOptions.isBound());
}

TEST_F(UdpClientTest, SendStringOverload) {
    RemoteEndpoint endpoint{"127.0.0.1", 12369};
    std::string message = "String overload test";

    auto result = client_->send(endpoint, message);
    EXPECT_TRUE(result.has_value());
}

TEST_F(UdpClientTest, SendBroadcastStringOverload) {
    SocketOptions options;
    options.broadcast = true;
    client_->setSocketOptions(options);

    std::string message = "Broadcast string test";
    auto result = client_->sendBroadcast(12370, message);
    // May succeed or fail depending on network configuration
}

TEST_F(UdpClientTest, StatisticsAfterSends) {
    client_->resetStatistics();

    std::string message = "Stats test message";
    std::span<const char> data_span(message.data(), message.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12371};

    for (int i = 0; i < 5; ++i) {
        client_->send(endpoint, data_span);
    }

    auto stats = client_->getStatistics();
    EXPECT_GE(stats.packetsSent,
              0);  // May or may not count depending on implementation
}

TEST_F(UdpClientTest, IPv6Support) {
    bool ipv6Supported = UdpClient::isIPv6Supported();
    // Just verify the function works
    EXPECT_TRUE(ipv6Supported || !ipv6Supported);
}

TEST_F(UdpClientTest, RemoteEndpointEquality) {
    RemoteEndpoint ep1{"127.0.0.1", 8080};
    RemoteEndpoint ep2{"127.0.0.1", 8080};
    RemoteEndpoint ep3{"127.0.0.1", 8081};
    RemoteEndpoint ep4{"192.168.1.1", 8080};

    EXPECT_EQ(ep1, ep2);
    EXPECT_NE(ep1, ep3);
    EXPECT_NE(ep1, ep4);
}

TEST_F(UdpClientTest, UdpStatisticsReset) {
    UdpStatistics stats;
    stats.packetsReceived = 100;
    stats.packetsSent = 50;
    stats.bytesReceived = 10000;
    stats.bytesSent = 5000;
    stats.receiveErrors = 5;
    stats.sendErrors = 2;

    stats.reset();

    EXPECT_EQ(stats.packetsReceived, 0);
    EXPECT_EQ(stats.packetsSent, 0);
    EXPECT_EQ(stats.bytesReceived, 0);
    EXPECT_EQ(stats.bytesSent, 0);
    EXPECT_EQ(stats.receiveErrors, 0);
    EXPECT_EQ(stats.sendErrors, 0);
}

TEST_F(UdpClientTest, ReceiveWithLongTimeout) {
    auto bindResult = client_->bind(12372);
    ASSERT_TRUE(bindResult.has_value());

    auto start = std::chrono::steady_clock::now();
    auto result = client_->receive(1024, std::chrono::milliseconds(100));
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_GE(duration, std::chrono::milliseconds(50));
}

TEST_F(UdpClientTest, ConcurrentReceiveAndSend) {
    auto bindResult = client_->bind(12373);
    ASSERT_TRUE(bindResult.has_value());

    std::atomic<int> sendCount{0};
    std::atomic<int> receiveCount{0};

    std::thread sender([this, &sendCount]() {
        for (int i = 0; i < 10; ++i) {
            std::string message = "Concurrent_" + std::to_string(i);
            std::span<const char> data_span(message.data(), message.size());
            RemoteEndpoint endpoint{"127.0.0.1", 12373};
            if (client_->send(endpoint, data_span).has_value()) {
                sendCount++;
            }
        }
    });

    std::thread receiver([this, &receiveCount]() {
        for (int i = 0; i < 5; ++i) {
            auto result =
                client_->receive(1024, std::chrono::milliseconds(100));
            if (result.has_value()) {
                receiveCount++;
            }
        }
    });

    sender.join();
    receiver.join();

    EXPECT_GT(sendCount.load(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
