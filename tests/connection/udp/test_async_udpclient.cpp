#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>
#include "atom/connection/udp/async_udpclient.hpp"

using namespace atom::async::connection;
using namespace std::chrono_literals;

class AsyncUdpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<UdpClient>();
        receiverClient_ = std::make_unique<UdpClient>();
    }

    void TearDown() override {
        if (client_) {
            client_->stopReceiving();
        }
        if (receiverClient_) {
            receiverClient_->stopReceiving();
        }
        client_.reset();
        receiverClient_.reset();
    }

    std::unique_ptr<UdpClient> client_;
    std::unique_ptr<UdpClient> receiverClient_;
};

TEST_F(AsyncUdpClientTest, BasicBinding) { EXPECT_TRUE(client_->bind(12346)); }

TEST_F(AsyncUdpClientTest, BindWithAddress) {
    EXPECT_TRUE(client_->bind(12347, "127.0.0.1"));
}

TEST_F(AsyncUdpClientTest, SendBasicData) {
    ASSERT_TRUE(receiverClient_->bind(12348));

    std::vector<char> testData = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_TRUE(client_->send("127.0.0.1", 12348, testData));
}

TEST_F(AsyncUdpClientTest, SendStringData) {
    ASSERT_TRUE(receiverClient_->bind(12349));

    std::string testMessage = "Hello, Async UDP!";
    EXPECT_TRUE(client_->send("127.0.0.1", 12349, testMessage));
}

TEST_F(AsyncUdpClientTest, SendWithTimeout) {
    ASSERT_TRUE(receiverClient_->bind(12350));

    std::vector<char> testData = {'T', 'e', 's', 't'};
    EXPECT_TRUE(client_->sendWithTimeout("127.0.0.1", 12350, testData, 1000ms));
}

TEST_F(AsyncUdpClientTest, BatchSend) {
    ASSERT_TRUE(receiverClient_->bind(12351));

    std::vector<std::pair<std::string, int>> destinations = {
        {"127.0.0.1", 12351}, {"127.0.0.1", 12351}, {"127.0.0.1", 12351}};

    std::vector<char> testData = {'B', 'a', 't', 'c', 'h'};
    int sent = client_->batchSend(destinations, testData);
    EXPECT_EQ(sent, 3);
}

TEST_F(AsyncUdpClientTest, ReceiveData) {
    ASSERT_TRUE(client_->bind(12352));

    // Send data from another client
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);
        std::string message = "Test receive";
        receiverClient_->send("127.0.0.1", 12352, message);
    });

    std::string remoteHost;
    int remotePort;
    auto received = client_->receive(1024, remoteHost, remotePort, 2000ms);

    sender.join();

    EXPECT_FALSE(received.empty());
    std::string receivedMessage(received.begin(), received.end());
    EXPECT_EQ(receivedMessage, "Test receive");
    EXPECT_EQ(remoteHost, "127.0.0.1");
}

TEST_F(AsyncUdpClientTest, AsyncReceiveCallback) {
    ASSERT_TRUE(client_->bind(12353));

    std::promise<std::vector<char>> dataPromise;
    auto dataFuture = dataPromise.get_future();

    std::promise<std::string> hostPromise;
    auto hostFuture = hostPromise.get_future();

    std::promise<int> portPromise;
    auto portFuture = portPromise.get_future();

    bool callbackCalled = false;

    client_->setOnDataReceivedCallback(
        [&](const std::vector<char>& data, const std::string& host, int port) {
            if (!callbackCalled) {
                callbackCalled = true;
                dataPromise.set_value(data);
                hostPromise.set_value(host);
                portPromise.set_value(port);
            }
        });

    // startReceiving returns void in async client
    client_->startReceiving(1024);

    // Send data from another client
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);
        std::string message = "Async callback test";
        receiverClient_->send("127.0.0.1", 12353, message);
    });

    // Wait for callback
    ASSERT_EQ(dataFuture.wait_for(3s), std::future_status::ready);
    ASSERT_EQ(hostFuture.wait_for(100ms), std::future_status::ready);
    ASSERT_EQ(portFuture.wait_for(100ms), std::future_status::ready);

    auto receivedData = dataFuture.get();
    auto receivedHost = hostFuture.get();
    auto receivedPort = portFuture.get();

    sender.join();

    std::string message(receivedData.begin(), receivedData.end());
    EXPECT_EQ(message, "Async callback test");
    EXPECT_EQ(receivedHost, "127.0.0.1");
    EXPECT_GT(receivedPort, 0);
}

TEST_F(AsyncUdpClientTest, ErrorCallback) {
    std::promise<std::string> errorPromise;
    auto errorFuture = errorPromise.get_future();

    std::promise<int> codePromise;
    auto codeFuture = codePromise.get_future();

    bool errorCallbackCalled = false;

    client_->setOnErrorCallback([&](const std::string& error, int errorCode) {
        if (!errorCallbackCalled) {
            errorCallbackCalled = true;
            errorPromise.set_value(error);
            codePromise.set_value(errorCode);
        }
    });

    // Try to send to invalid address to trigger error
    std::vector<char> data = {'E', 'r', 'r', 'o', 'r'};
    client_->send("999.999.999.999", 12354, data);

    // Wait for error callback (may not always trigger depending on system)
    if (errorFuture.wait_for(2s) == std::future_status::ready) {
        auto error = errorFuture.get();
        auto code = codeFuture.get();
        EXPECT_FALSE(error.empty());
        EXPECT_NE(code, 0);
    }
}

TEST_F(AsyncUdpClientTest, StatusCallback) {
    std::promise<std::string> statusPromise;
    auto statusFuture = statusPromise.get_future();

    bool statusCallbackCalled = false;

    client_->setOnStatusCallback([&](const std::string& status) {
        if (!statusCallbackCalled) {
            statusCallbackCalled = true;
            statusPromise.set_value(status);
        }
    });

    ASSERT_TRUE(client_->bind(12355));

    // Status callback should be triggered
    if (statusFuture.wait_for(1s) == std::future_status::ready) {
        auto status = statusFuture.get();
        EXPECT_FALSE(status.empty());
    }
}

TEST_F(AsyncUdpClientTest, StartStopReceiving) {
    ASSERT_TRUE(client_->bind(12356));

    // startReceiving returns void and there's no isReceiving() in async client
    client_->startReceiving(1024);

    // Give it a brief moment to start and then stop
    std::this_thread::sleep_for(50ms);
    client_->stopReceiving();
    SUCCEED();
}

TEST_F(AsyncUdpClientTest, MulticastJoinLeave) {
    ASSERT_TRUE(client_->bind(12357));

    // Test multicast operations (may not work in all environments)
    EXPECT_NO_THROW(client_->joinMulticastGroup("224.0.0.1"));
    EXPECT_NO_THROW(client_->leaveMulticastGroup("224.0.0.1"));
}

TEST_F(AsyncUdpClientTest, BroadcastSend) {
    // The async UdpClient does not support sendBroadcast; skip this test
    GTEST_SKIP()
        << "sendBroadcast is not supported by async::connection::UdpClient";
}

TEST_F(AsyncUdpClientTest, GetStatistics) {
    ASSERT_TRUE(client_->bind(12360));

    auto stats = client_->getStatistics();
    EXPECT_EQ(stats.packetsSent, 0);
    EXPECT_EQ(stats.packetsReceived, 0);
    EXPECT_EQ(stats.bytesSent, 0);
    EXPECT_EQ(stats.bytesReceived, 0);

    // Send some data to update statistics
    std::vector<char> testData = {'S', 't', 'a', 't', 's'};
    client_->send("127.0.0.1", 12361, testData);

    auto updatedStats = client_->getStatistics();
    EXPECT_GT(updatedStats.packetsSent, 0);
    EXPECT_GT(updatedStats.bytesSent, 0);
}

TEST_F(AsyncUdpClientTest, ResetStatistics) {
    ASSERT_TRUE(client_->bind(12362));

    // Send some data first
    std::vector<char> testData = {'R', 'e', 's', 'e', 't'};
    client_->send("127.0.0.1", 12363, testData);

    auto stats = client_->getStatistics();
    EXPECT_GT(stats.packetsSent, 0);

    client_->resetStatistics();
    auto resetStats = client_->getStatistics();
    EXPECT_EQ(resetStats.packetsSent, 0);
    EXPECT_EQ(resetStats.bytesSent, 0);
}

TEST_F(AsyncUdpClientTest, SocketOptions) {
    ASSERT_TRUE(client_->bind(12364));

    // Test socket option setting (implementation dependent)
    EXPECT_NO_THROW(client_->setSocketOption(SocketOption::Broadcast, 1));
    EXPECT_NO_THROW(client_->setSocketOption(SocketOption::ReuseAddress, 1));
    EXPECT_NO_THROW(
        client_->setSocketOption(SocketOption::ReceiveBufferSize, 8192));
    EXPECT_NO_THROW(
        client_->setSocketOption(SocketOption::SendBufferSize, 8192));
}

TEST_F(AsyncUdpClientTest, IPv6Support) {
    auto ipv6Client = std::make_unique<UdpClient>(true);  // IPv6

    // IPv6 binding (may not work in all environments)
    bool bound = ipv6Client->bind(12365, "::1");
    if (bound) {
        // Test IPv6 send
        std::vector<char> testData = {'I', 'P', 'v', '6'};
        EXPECT_NO_THROW(ipv6Client->send("::1", 12366, testData));
    }

    ipv6Client->stopReceiving();
}

TEST_F(AsyncUdpClientTest, LargeDataTransfer) {
    ASSERT_TRUE(client_->bind(12367));
    ASSERT_TRUE(receiverClient_->bind(12368));

    // Create large data packet (but within UDP limits)
    std::vector<char> largeData(1400, 'X');  // 1400 bytes

    EXPECT_TRUE(client_->send("127.0.0.1", 12368, largeData));

    std::string remoteHost;
    int remotePort;
    auto received =
        receiverClient_->receive(2000, remoteHost, remotePort, 2000ms);

    EXPECT_EQ(received.size(), largeData.size());
    EXPECT_EQ(received, largeData);
}

TEST_F(AsyncUdpClientTest, ConcurrentOperations) {
    ASSERT_TRUE(client_->bind(12369));

    const int numThreads = 5;
    const int messagesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, messagesPerThread, &successCount]() {
            for (int j = 0; j < messagesPerThread; ++j) {
                std::string message =
                    "Thread" + std::to_string(i) + "_Msg" + std::to_string(j);
                if (client_->send("127.0.0.1", 12370, message)) {
                    successCount++;
                }
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}
