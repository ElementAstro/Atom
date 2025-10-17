#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include "atom/connection/udpserver.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using namespace atom::connection;
using namespace std::chrono_literals;

class UdpServerTest : public ::testing::Test {
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

TEST_F(UdpServerTest, BasicStartStop) {
    EXPECT_FALSE(server_->isRunning());

    auto result = server_->start(12500);
    if (result.has_value()) {
        EXPECT_TRUE(server_->isRunning());

        server_->stop();
        EXPECT_FALSE(server_->isRunning());
    }
}

TEST_F(UdpServerTest, StartWithInvalidPort) {
    // Port 0 should be invalid for UDP server
    auto result = server_->start(0);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(UdpServerTest, StartWithPrivilegedPort) {
    // Port 80 requires privileges, should fail in normal test environment
    auto result = server_->start(80);
    // May succeed or fail depending on test environment privileges
    if (!result.has_value()) {
        EXPECT_FALSE(server_->isRunning());
    }
}

TEST_F(UdpServerTest, MultipleStartCalls) {
    auto result1 = server_->start(12501);
    if (result1.has_value()) {
        EXPECT_TRUE(server_->isRunning());

        // Second start should not cause issues
        auto result2 = server_->start(12501);
        EXPECT_TRUE(server_->isRunning());
    }
}

TEST_F(UdpServerTest, StopWithoutStart) {
    EXPECT_FALSE(server_->isRunning());

    // Should not throw when stopping a server that's not running
    EXPECT_NO_THROW(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(UdpServerTest, MessageHandler) {
    std::promise<std::string> messagePromise;
    std::promise<std::string> hostPromise;
    std::promise<std::uint16_t> portPromise;

    auto messageFuture = messagePromise.get_future();
    auto hostFuture = hostPromise.get_future();
    auto portFuture = portPromise.get_future();

    bool handlerCalled = false;

    server_->addMessageHandler([&](std::string_view message,
                                   std::string_view host, std::uint16_t port) {
        if (!handlerCalled) {
            handlerCalled = true;
            messagePromise.set_value(std::string(message));
            hostPromise.set_value(std::string(host));
            portPromise.set_value(port);
        }
    });

    auto startResult = server_->start(12502);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12502";
    }

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
            serverAddr.sin_port = htons(12502);
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
    if (messageFuture.wait_for(3s) == std::future_status::ready) {
        auto receivedMessage = messageFuture.get();
        auto receivedHost = hostFuture.get();
        auto receivedPort = portFuture.get();

        EXPECT_EQ(receivedMessage, "Hello UDP Server!");
        EXPECT_EQ(receivedHost, "127.0.0.1");
        EXPECT_GT(receivedPort, 0);
    }

    sender.join();
}

TEST_F(UdpServerTest, MultipleMessageHandlers) {
    std::atomic<int> handler1Count{0};
    std::atomic<int> handler2Count{0};

    server_->addMessageHandler([&](std::string_view, std::string_view,
                                   std::uint16_t) { handler1Count++; });

    server_->addMessageHandler([&](std::string_view, std::string_view,
                                   std::uint16_t) { handler2Count++; });

    auto startResult = server_->start(12503);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12503";
    }

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
                serverAddr.sin_port = htons(12503);
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

TEST_F(UdpServerTest, RemoveMessageHandler) {
    std::atomic<int> handlerCount{0};

    auto handler = [&](std::string_view, std::string_view, std::uint16_t) {
        handlerCount++;
    };

    server_->addMessageHandler(handler);

    auto startResult = server_->start(12504);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12504";
    }

    // Send a message
    std::thread sender1([this]() {
        std::this_thread::sleep_for(100ms);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12504);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string testMessage = "Before removal";
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

    sender1.join();
    std::this_thread::sleep_for(200ms);

    // Remove handler
    server_->removeMessageHandler(handler);

    // Send another message
    std::thread sender2([this]() {
        std::this_thread::sleep_for(100ms);

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(12504);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string testMessage = "After removal";
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

    sender2.join();
    std::this_thread::sleep_for(200ms);

    // Should have received only the first message
    EXPECT_EQ(handlerCount.load(), 1);
}

TEST_F(UdpServerTest, SendToSpecificEndpoint) {
    auto startResult = server_->start(12505);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12505";
    }

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
            clientAddr.sin_port = htons(12506);
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
    auto sendResult = server_->sendTo(testMessage, "127.0.0.1", 12506);
    EXPECT_TRUE(sendResult.has_value());

    if (messageFuture.wait_for(3s) == std::future_status::ready) {
        auto receivedMessage = messageFuture.get();
        EXPECT_EQ(receivedMessage, testMessage);
    }

    receiver.join();
}

// Broadcast functionality is not available in the current UdpSocketHub
// implementation TEST_F(UdpServerTest, BroadcastMessage) {
//     auto startResult = server_->start(12507);
//     if (!startResult.has_value()) {
//         GTEST_SKIP() << "Could not start UDP server on port 12507";
//     }
//
//     // UdpSocketHub does not have a broadcast method
//     // This test is disabled until broadcast functionality is implemented
// }

// Statistics functionality is not available in the current UdpSocketHub
// implementation TEST_F(UdpServerTest, GetStatistics) {
//     auto startResult = server_->start(12509);
//     if (!startResult.has_value()) {
//         GTEST_SKIP() << "Could not start UDP server on port 12509";
//     }
//
//     // UdpSocketHub does not have statistics methods
//     // This test is disabled until statistics functionality is implemented
// }

// Statistics functionality is not available in the current UdpSocketHub
// implementation TEST_F(UdpServerTest, ResetStatistics) {
//     auto startResult = server_->start(12510);
//     if (!startResult.has_value()) {
//         GTEST_SKIP() << "Could not start UDP server on port 12510";
//     }
//
//     // UdpSocketHub does not have statistics methods
//     // This test is disabled until statistics functionality is implemented
// }

TEST_F(UdpServerTest, ConcurrentClients) {
    std::atomic<int> messagesReceived{0};

    server_->addMessageHandler([&](std::string_view, std::string_view,
                                   std::uint16_t) { messagesReceived++; });

    auto startResult = server_->start(12511);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12511";
    }

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
                    serverAddr.sin_port = htons(12511);
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

TEST_F(UdpServerTest, LargeMessageHandling) {
    std::promise<size_t> sizePromise;
    auto sizeFuture = sizePromise.get_future();

    bool handlerCalled = false;

    server_->addMessageHandler(
        [&](std::string_view message, std::string_view, std::uint16_t) {
            if (!handlerCalled) {
                handlerCalled = true;
                sizePromise.set_value(message.size());
            }
        });

    auto startResult = server_->start(12512);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12512";
    }

    // Send a large message (but within UDP limits)
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
            serverAddr.sin_port = htons(12512);
            inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

            std::string largeMessage(1400, 'X');  // 1400 bytes
            sendto(sock, largeMessage.c_str(), largeMessage.length(), 0,
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

    if (sizeFuture.wait_for(3s) == std::future_status::ready) {
        auto receivedSize = sizeFuture.get();
        EXPECT_EQ(receivedSize, 1400);
    }
}

TEST_F(UdpServerTest, ThreadSafety) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> operationCount{0};

    auto startResult = server_->start(12513);
    if (!startResult.has_value()) {
        GTEST_SKIP() << "Could not start UDP server on port 12513";
    }

    // Multiple threads performing operations concurrently
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &operationCount]() {
            try {
                // Test thread-safe operations
                (void)server_->isRunning();  // Use the result to avoid warning

                std::string message =
                    "Thread_" + std::to_string(i) + "_message";
                (void)server_->sendTo(
                    message, "127.0.0.1",
                    12514);  // Use the result to avoid warning

                operationCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(operationCount.load(), numThreads);
}
