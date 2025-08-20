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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
