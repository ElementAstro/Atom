#include "atom/connection/udpclient.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <span>

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

    // Should handle invalid port gracefully
    EXPECT_NO_THROW(client_->send(endpoint, data_span));
}

TEST_F(UdpClientTest, SendEmptyMessage) {
    std::string emptyMessage = "";
    std::span<const char> data_span(emptyMessage.data(), emptyMessage.size());
    RemoteEndpoint endpoint{"127.0.0.1", 12346};

    auto result = client_->send(endpoint, data_span);
    EXPECT_TRUE(result.has_value());
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
    // Port 0 should be invalid
    auto result = client_->bind(0);
    EXPECT_FALSE(result.has_value());
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
