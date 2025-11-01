#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include "atom/connection/async_udpserver.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using namespace atom::async::connection;
using namespace std::chrono_literals;

class AsyncUdpServerTest : public ::testing::Test {
protected:
    void SetUp() override { server_ = std::make_unique<UdpSocketHub>(); }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
        server_.reset();
    }

    std::unique_ptr<UdpSocketHub> server_;
};

TEST_F(AsyncUdpServerTest, BasicStartStop) {
    EXPECT_FALSE(server_->isRunning());

    ASSERT_TRUE(server_->start(12400));
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(AsyncUdpServerTest, StartWithIPv6) {
    EXPECT_FALSE(server_->isRunning());

    // IPv6 may not be available in all test environments
    bool started = server_->start(12401, true);
    if (started) {
        EXPECT_TRUE(server_->isRunning());
        server_->stop();
        EXPECT_FALSE(server_->isRunning());
    }
}

TEST_F(AsyncUdpServerTest, MultipleStartCalls) {
    ASSERT_TRUE(server_->start(12402));
    EXPECT_TRUE(server_->isRunning());

    // Second start should not cause issues
    EXPECT_TRUE(server_->start(12402));
    EXPECT_TRUE(server_->isRunning());

    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(AsyncUdpServerTest, MessageHandler) {
    std::promise<std::string> messagePromise;
    std::promise<std::string> hostPromise;
    std::promise<unsigned short> portPromise;

    auto messageFuture = messagePromise.get_future();
    auto hostFuture = hostPromise.get_future();
    auto portFuture = portPromise.get_future();

    bool handlerCalled = false;

    server_->addMessageHandler([&](const std::string& message,
                                   const std::string& host,
                                   unsigned short port) {
        if (!handlerCalled) {
            handlerCalled = true;
            messagePromise.set_value(message);
            hostPromise.set_value(host);
            portPromise.set_value(port);
        }
    });

    ASSERT_TRUE(server_->start(12403));

    // Send a message to the server
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12403);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string testMessage = "Hello UDP Server!";
            sendto(sock, testMessage.c_str(), testMessage.length(), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr));

#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
        }
    });

    // Wait for message handler to be called
    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    ASSERT_EQ(hostFuture.wait_for(100ms), std::future_status::ready);
    ASSERT_EQ(portFuture.wait_for(100ms), std::future_status::ready);

    auto receivedMessage = messageFuture.get();
    auto receivedHost = hostFuture.get();
    auto receivedPort = portFuture.get();

    sender.join();

    EXPECT_EQ(receivedMessage, "Hello UDP Server!");
    EXPECT_EQ(receivedHost, "127.0.0.1");
    EXPECT_GT(receivedPort, 0);
}

TEST_F(AsyncUdpServerTest, MultipleMessageHandlers) {
    std::atomic<int> handler1Count{0};
    std::atomic<int> handler2Count{0};

    server_->addMessageHandler([&](const std::string&, const std::string&,
                                   unsigned short) { handler1Count++; });

    server_->addMessageHandler([&](const std::string&, const std::string&,
                                   unsigned short) { handler2Count++; });

    ASSERT_TRUE(server_->start(12404));

    // Send multiple messages
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);

        for (int i = 0; i < 3; ++i) {
#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                sockaddr_in serverAddr{};
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(12404);
                inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

                std::string testMessage = "Message " + std::to_string(i);
                sendto(sock, testMessage.c_str(), testMessage.length(), 0,
                       (sockaddr*)&serverAddr, sizeof(serverAddr));

#ifdef _WIN32
                closesocket(sock);
                WSACleanup();
#else
                close(sock);
#endif
            }
            std::this_thread::sleep_for(50ms);
        }
    });

    sender.join();
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(handler1Count.load(), 3);
    EXPECT_EQ(handler2Count.load(), 3);
}

TEST_F(AsyncUdpServerTest, ErrorHandler) {
    std::promise<std::string> errorPromise;
    auto errorFuture = errorPromise.get_future();

    bool errorHandlerCalled = false;

    server_->addErrorHandler([&](const std::string& error,
                                 const std::error_code& code) {
        if (!errorHandlerCalled) {
            errorHandlerCalled = true;
            errorPromise.set_value(error +
                                   " Code: " + std::to_string(code.value()));
        }
    });

    // Try to start on an invalid port to trigger error
    server_->start(0);  // Port 0 might cause issues

    // Error handling is implementation dependent
    if (errorFuture.wait_for(2s) == std::future_status::ready) {
        auto error = errorFuture.get();
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(AsyncUdpServerTest, SendToSpecificEndpoint) {
    ASSERT_TRUE(server_->start(12405));

    // Create a client to receive the message
    std::promise<std::string> messagePromise;
    auto messageFuture = messagePromise.get_future();

    std::thread receiver([&messagePromise]() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in clientAddr{};
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = htons(12406);
            clientAddr.sin_addr.s_addr = INADDR_ANY;

            if (bind(sock, (sockaddr*)&clientAddr, sizeof(clientAddr)) == 0) {
                char buffer[1024];
                sockaddr_in senderAddr{};
                socklen_t senderLen = sizeof(senderAddr);

                int received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                        (sockaddr*)&senderAddr, &senderLen);
                if (received > 0) {
                    messagePromise.set_value(std::string(buffer, received));
                }
            }

#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
        }
    });

    std::this_thread::sleep_for(100ms);

    std::string testMessage = "Direct send test";
    EXPECT_TRUE(server_->sendTo(testMessage, "127.0.0.1", 12406));

    ASSERT_EQ(messageFuture.wait_for(3s), std::future_status::ready);
    auto receivedMessage = messageFuture.get();

    receiver.join();

    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncUdpServerTest, BroadcastMessage) {
    ASSERT_TRUE(server_->start(12407));

    std::string broadcastMessage = "Broadcast test message";
    EXPECT_TRUE(server_->broadcast(broadcastMessage, 12408));

    // Broadcast testing requires special network setup, so we just verify the
    // call succeeds
}

TEST_F(AsyncUdpServerTest, MulticastOperations) {
    ASSERT_TRUE(server_->start(12409));

    std::string multicastAddress = "224.0.0.1";

    // Join multicast group
    bool joined = server_->joinMulticastGroup(multicastAddress);
    // Multicast may not be available in all test environments

    if (joined) {
        EXPECT_TRUE(server_->leaveMulticastGroup(multicastAddress));
    }
}

TEST_F(AsyncUdpServerTest, Statistics) {
    ASSERT_TRUE(server_->start(12410));

    auto initialStats = server_->getStatistics();
    EXPECT_EQ(initialStats.messagesReceived, 0);
    EXPECT_EQ(initialStats.messagesSent, 0);
    EXPECT_EQ(initialStats.bytesReceived, 0);
    EXPECT_EQ(initialStats.bytesSent, 0);

    // Send a message to update statistics
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12410);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string testMessage = "Statistics test";
            sendto(sock, testMessage.c_str(), testMessage.length(), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr));

#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
        }
    });

    sender.join();
    std::this_thread::sleep_for(200ms);

    auto updatedStats = server_->getStatistics();
    EXPECT_GT(updatedStats.messagesReceived, 0);
    EXPECT_GT(updatedStats.bytesReceived, 0);
}

TEST_F(AsyncUdpServerTest, ResetStatistics) {
    ASSERT_TRUE(server_->start(12411));

    // Send a message first
    std::thread sender([this]() {
        std::this_thread::sleep_for(100ms);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12411);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string testMessage = "Reset stats test";
            sendto(sock, testMessage.c_str(), testMessage.length(), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr));

#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
        }
    });

    sender.join();
    std::this_thread::sleep_for(200ms);

    auto stats = server_->getStatistics();
    EXPECT_GT(stats.messagesReceived, 0);

    server_->resetStatistics();
    auto resetStats = server_->getStatistics();
    EXPECT_EQ(resetStats.messagesReceived, 0);
    EXPECT_EQ(resetStats.bytesReceived, 0);
}

TEST_F(AsyncUdpServerTest, ConcurrentClients) {
    std::atomic<int> messagesReceived{0};

    server_->addMessageHandler([&](const std::string&, const std::string&,
                                   unsigned short) { messagesReceived++; });

    ASSERT_TRUE(server_->start(12412));

    const int numClients = 5;
    const int messagesPerClient = 3;
    std::vector<std::thread> clients;

    for (int i = 0; i < numClients; ++i) {
        clients.emplace_back([this, i, messagesPerClient]() {
            std::this_thread::sleep_for(100ms);

            for (int j = 0; j < messagesPerClient; ++j) {
#ifdef _WIN32
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

                int sock = socket(AF_INET, SOCK_DGRAM, 0);
                if (sock >= 0) {
                    sockaddr_in serverAddr{};
                    serverAddr.sin_family = AF_INET;
                    serverAddr.sin_port = htons(12412);
                    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

                    std::string message = "Client" + std::to_string(i) +
                                          "_Msg" + std::to_string(j);
                    sendto(sock, message.c_str(), message.length(), 0,
                           (sockaddr*)&serverAddr, sizeof(serverAddr));

#ifdef _WIN32
                    closesocket(sock);
                    WSACleanup();
#else
                    close(sock);
#endif
                }
                std::this_thread::sleep_for(10ms);
            }
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(messagesReceived.load(), numClients * messagesPerClient);
}
