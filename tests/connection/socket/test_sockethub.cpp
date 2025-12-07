#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>

#include "atom/connection/shared/sockethub.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

using namespace atom::connection;

class SocketHubTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start the SocketHub on a separate thread
        socketHub_ = std::make_unique<SocketHub>();
        socketHub_->addHandler([this](std::string_view message) {
            std::scoped_lock lock(mutex_);
            messages_.emplace_back(message);
        });
        socketHub_->start(port_);
        std::this_thread::sleep_for(
            std::chrono::seconds(1));  // Give some time for server to start
    }

    void TearDown() override {
        socketHub_->stop();
        socketHub_.reset();
    }

    std::unique_ptr<SocketHub> socketHub_;
    int port_ = 8080;
    std::vector<std::string> messages_;
    std::mutex mutex_;
};

TEST_F(SocketHubTest, StartAndStop) {
    ASSERT_TRUE(socketHub_->isRunning());
    socketHub_->stop();
    ASSERT_FALSE(socketHub_->isRunning());
}

TEST_F(SocketHubTest, AcceptConnection) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    ::close(clientSocket);
}

TEST_F(SocketHubTest, SendAndReceiveMessage) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::string message = "Hello, server!";
    result = ::send(clientSocket, message.c_str(), message.size(), 0);
    ASSERT_NE(result, -1);

    std::this_thread::sleep_for(
        std::chrono::seconds(1));  // Give some time for message to be handled

    {
        std::scoped_lock lock(mutex_);
        ASSERT_EQ(messages_.size(), 1);
        ASSERT_EQ(messages_[0], message);
    }

    ::close(clientSocket);
}

TEST_F(SocketHubTest, HandleMultipleClients) {
    const int clientCount = 5;
    std::vector<int> clientSockets(clientCount);

    for (int i = 0; i < clientCount; ++i) {
        clientSockets[i] = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(clientSockets[i], -1);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port_);
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

        int result = ::connect(clientSockets[i], (sockaddr *)&serverAddress,
                               sizeof(serverAddress));
        ASSERT_EQ(result, 0);
    }

    std::string message = "Hello, server!";
    for (int i = 0; i < clientCount; ++i) {
        int result =
            ::send(clientSockets[i], message.c_str(), message.size(), 0);
        ASSERT_NE(result, -1);
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(1));  // Give some time for messages to be handled

    {
        std::scoped_lock lock(mutex_);
        ASSERT_EQ(messages_.size(), clientCount);
        for (const auto &msg : messages_) {
            ASSERT_EQ(msg, message);
        }
    }

    for (int i = 0; i < clientCount; ++i) {
        ::close(clientSockets[i]);
    }
}

TEST_F(SocketHubTest, ClientTimeout) {
    // Set a short timeout
    socketHub_->setClientTimeout(std::chrono::seconds(1));

    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    // Don't send any data and wait for timeout
    std::this_thread::sleep_for(std::chrono::seconds(2));

    ::close(clientSocket);
}

TEST_F(SocketHubTest, GetPort) { EXPECT_EQ(socketHub_->getPort(), port_); }

TEST_F(SocketHubTest, SendLargeMessage) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    // Send a large message
    std::string largeMessage(8192, 'X');
    largeMessage += "END_MARKER";

    result = ::send(clientSocket, largeMessage.c_str(), largeMessage.size(), 0);
    ASSERT_NE(result, -1);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
        std::scoped_lock lock(mutex_);
        ASSERT_EQ(messages_.size(), 1);
        EXPECT_EQ(messages_[0], largeMessage);
    }

    ::close(clientSocket);
}

TEST_F(SocketHubTest, EmptyMessage) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    // Send empty message
    std::string emptyMessage = "";
    result = ::send(clientSocket, emptyMessage.c_str(), emptyMessage.size(), 0);
    ASSERT_NE(result, -1);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Empty message handling depends on implementation
    // Should not crash the server

    ::close(clientSocket);
}

TEST_F(SocketHubTest, RapidConnectDisconnect) {
    const int numConnections = 10;

    for (int i = 0; i < numConnections; ++i) {
        int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(clientSocket, -1);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port_);
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

        int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                               sizeof(serverAddress));
        ASSERT_EQ(result, 0);

        // Immediately disconnect
        ::close(clientSocket);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(SocketHubTest, ServerRestart) {
    ASSERT_TRUE(socketHub_->isRunning());

    socketHub_->stop();
    ASSERT_FALSE(socketHub_->isRunning());

    socketHub_->start(port_);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(socketHub_->isRunning());

    // Test functionality after restart
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::string message = "Restart test message";
    result = ::send(clientSocket, message.c_str(), message.size(), 0);
    ASSERT_NE(result, -1);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
        std::scoped_lock lock(mutex_);
        ASSERT_EQ(messages_.size(), 1);
        EXPECT_EQ(messages_[0], message);
    }

    ::close(clientSocket);
}

// ============================================================================
// Additional SocketHub Tests
// ============================================================================

TEST_F(SocketHubTest, GetClientCount) {
    EXPECT_EQ(socketHub_->getClientCount(), 0);

    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_GE(socketHub_->getClientCount(), 1);

    ::close(clientSocket);
}

TEST_F(SocketHubTest, BroadcastMessage) {
    const int clientCount = 3;
    std::vector<int> clientSockets(clientCount);

    for (int i = 0; i < clientCount; ++i) {
        clientSockets[i] = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(clientSockets[i], -1);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port_);
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

        int result = ::connect(clientSockets[i], (sockaddr *)&serverAddress,
                               sizeof(serverAddress));
        ASSERT_EQ(result, 0);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string broadcastMsg = "Broadcast message";
    socketHub_->broadcast(broadcastMsg);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int i = 0; i < clientCount; ++i) {
        ::close(clientSockets[i]);
    }
}

TEST_F(SocketHubTest, GetStatistics) {
    auto stats = socketHub_->getStatistics();
    EXPECT_EQ(stats.totalConnections, 0);
    EXPECT_EQ(stats.totalMessagesReceived, 0);
}

TEST_F(SocketHubTest, ResetStatistics) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::string statsMsg = "Stats test";
    ::send(clientSocket, statsMsg.c_str(), statsMsg.size(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    socketHub_->resetStatistics();

    auto stats = socketHub_->getStatistics();
    EXPECT_EQ(stats.totalMessagesReceived, 0);

    ::close(clientSocket);
}

TEST_F(SocketHubTest, SetMaxClients) {
    socketHub_->setMaxClients(2);

    std::vector<int> clientSockets(3);

    for (int i = 0; i < 3; ++i) {
        clientSockets[i] = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(clientSockets[i], -1);

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port_);
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

        ::connect(clientSockets[i], (sockaddr *)&serverAddress,
                  sizeof(serverAddress));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_LE(socketHub_->getClientCount(), 2);

    for (int i = 0; i < 3; ++i) {
        ::close(clientSockets[i]);
    }
}

TEST_F(SocketHubTest, SetOnClientConnectedCallback) {
    std::atomic<int> connectCount{0};

    socketHub_->setOnClientConnectedCallback(
        [&connectCount](int) { connectCount++; });

    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(connectCount.load(), 1);

    ::close(clientSocket);
}

TEST_F(SocketHubTest, SetOnClientDisconnectedCallback) {
    std::atomic<int> disconnectCount{0};

    socketHub_->setOnClientDisconnectedCallback(
        [&disconnectCount](int) { disconnectCount++; });

    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ::close(clientSocket);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(disconnectCount.load(), 1);
}

TEST_F(SocketHubTest, SendBinaryData) {
    int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(clientSocket, -1);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                           sizeof(serverAddress));
    ASSERT_EQ(result, 0);

    char binaryData[] = {0x00, 0x01, 0x02, static_cast<char>(0xFF)};
    result = ::send(clientSocket, binaryData, sizeof(binaryData), 0);
    ASSERT_NE(result, -1);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ::close(clientSocket);
}

TEST_F(SocketHubTest, ConcurrentConnections) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            int clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (clientSocket == -1)
                return;

            sockaddr_in serverAddress{};
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_port = htons(port_);
            inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

            int result = ::connect(clientSocket, (sockaddr *)&serverAddress,
                                   sizeof(serverAddress));
            if (result == 0) {
                std::string concurrentMsg = "Thread_" + std::to_string(i);
                ::send(clientSocket, concurrentMsg.c_str(),
                       concurrentMsg.size(), 0);
                successCount++;
            }

            ::close(clientSocket);
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}
