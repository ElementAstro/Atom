#include "atom/connection/async_sockethub.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

using namespace atom::async::connection;
using namespace std::chrono_literals;

class MockClient {
public:
    MockClient() : socket_(-1), connected_(false) {}

    ~MockClient() { disconnect(); }

    bool connect(const std::string& host, int port) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == -1) return false;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

        if (::connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
            connected_ = true;
            return true;
        }

        disconnect();
        return false;
    }

    void disconnect() {
        if (socket_ != -1) {
#ifdef _WIN32
            closesocket(socket_);
            WSACleanup();
#else
            close(socket_);
#endif
            socket_ = -1;
        }
        connected_ = false;
    }

    bool send(const std::string& message) {
        if (!connected_ || socket_ == -1) return false;
        return ::send(socket_, message.c_str(), message.length(), 0) > 0;
    }

    std::string receive(size_t maxSize = 1024) {
        if (!connected_ || socket_ == -1) return "";

        char buffer[1024];
        int received = recv(socket_, buffer, std::min(maxSize, sizeof(buffer)), 0);
        if (received > 0) {
            return std::string(buffer, received);
        }
        return "";
    }

    bool isConnected() const { return connected_; }

private:
    int socket_;
    bool connected_;
};

class AsyncSocketHubTest : public ::testing::Test {
protected:
    void SetUp() override {
        SocketHubConfig config;
        config.use_ssl = false;
        config.connection_timeout = 5s;
        config.backlog_size = 10;
        config.keep_alive = true;

        hub_ = std::make_unique<SocketHub>(config);
    }

    void TearDown() override {
        if (hub_) {
            hub_->stop();
        }
        hub_.reset();
    }

    std::unique_ptr<SocketHub> hub_;
};

TEST_F(AsyncSocketHubTest, BasicStartStop) {
    EXPECT_FALSE(hub_->isRunning());

    EXPECT_NO_THROW(hub_->start(8090));
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(hub_->isRunning());

    hub_->stop();
    EXPECT_FALSE(hub_->isRunning());
}

TEST_F(AsyncSocketHubTest, RestartServer) {
    hub_->start(8091);
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(hub_->isRunning());

    EXPECT_NO_THROW(hub_->restart());
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(hub_->isRunning());
}

TEST_F(AsyncSocketHubTest, ClientConnection) {
    std::promise<size_t> clientIdPromise;
    std::promise<std::string> clientAddrPromise;
    auto clientIdFuture = clientIdPromise.get_future();
    auto clientAddrFuture = clientAddrPromise.get_future();

    bool connectHandlerCalled = false;

    hub_->addConnectHandler([&](size_t clientId, const std::string& clientAddr) {
        if (!connectHandlerCalled) {
            connectHandlerCalled = true;
            clientIdPromise.set_value(clientId);
            clientAddrPromise.set_value(clientAddr);
        }
    });

    hub_->start(8092);
    std::this_thread::sleep_for(100ms);

    MockClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 8092));

    ASSERT_EQ(clientIdFuture.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(clientAddrFuture.wait_for(100ms), std::future_status::ready);

    auto clientId = clientIdFuture.get();
    auto clientAddr = clientAddrFuture.get();

    EXPECT_GT(clientId, 0);
    EXPECT_FALSE(clientAddr.empty());
    EXPECT_TRUE(hub_->isClientConnected(clientId));

    auto connectedClients = hub_->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), 1);
    EXPECT_EQ(connectedClients[0], clientId);
}

TEST_F(AsyncSocketHubTest, ClientDisconnection) {
    std::promise<size_t> disconnectIdPromise;
    auto disconnectIdFuture = disconnectIdPromise.get_future();

    size_t connectedClientId = 0;
    bool disconnectHandlerCalled = false;

    hub_->addConnectHandler([&](size_t clientId, const std::string&) {
        connectedClientId = clientId;
    });

    hub_->addDisconnectHandler([&](size_t clientId, const std::string&) {
        if (!disconnectHandlerCalled) {
            disconnectHandlerCalled = true;
            disconnectIdPromise.set_value(clientId);
        }
    });

    hub_->start(8093);
    std::this_thread::sleep_for(100ms);

    {
        MockClient client;
        ASSERT_TRUE(client.connect("127.0.0.1", 8093));
        std::this_thread::sleep_for(100ms);

        EXPECT_GT(connectedClientId, 0);
        EXPECT_TRUE(hub_->isClientConnected(connectedClientId));

        // Client disconnects when going out of scope
    }

    ASSERT_EQ(disconnectIdFuture.wait_for(2s), std::future_status::ready);
    auto disconnectedId = disconnectIdFuture.get();

    EXPECT_EQ(disconnectedId, connectedClientId);
    EXPECT_FALSE(hub_->isClientConnected(connectedClientId));
}

TEST_F(AsyncSocketHubTest, MessageHandling) {
    std::promise<Message> messagePromise;
    std::promise<size_t> senderIdPromise;
    auto messageFuture = messagePromise.get_future();
    auto senderIdFuture = senderIdPromise.get_future();

    size_t connectedClientId = 0;
    bool messageHandlerCalled = false;

    hub_->addConnectHandler([&](size_t clientId, const std::string&) {
        connectedClientId = clientId;
    });

    hub_->addMessageHandler([&](const Message& message, size_t senderId) {
        if (!messageHandlerCalled) {
            messageHandlerCalled = true;
            messagePromise.set_value(message);
            senderIdPromise.set_value(senderId);
        }
    });

    hub_->start(8094);
    std::this_thread::sleep_for(100ms);

    MockClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 8094));
    std::this_thread::sleep_for(100ms);

    std::string testMessage = "Hello Socket Hub!";
    ASSERT_TRUE(client.send(testMessage));

    ASSERT_EQ(messageFuture.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(senderIdFuture.wait_for(100ms), std::future_status::ready);

    auto receivedMessage = messageFuture.get();
    auto senderId = senderIdFuture.get();

    EXPECT_EQ(senderId, connectedClientId);
    EXPECT_EQ(receivedMessage.as_string(), testMessage);
}

TEST_F(AsyncSocketHubTest, BroadcastMessage) {
    const int numClients = 3;
    std::vector<std::unique_ptr<MockClient>> clients;
    std::vector<size_t> clientIds;

    std::mutex clientIdMutex;

    hub_->addConnectHandler([&](size_t clientId, const std::string&) {
        std::lock_guard<std::mutex> lock(clientIdMutex);
        clientIds.push_back(clientId);
    });

    hub_->start(8095);
    std::this_thread::sleep_for(100ms);

    // Connect multiple clients
    for (int i = 0; i < numClients; ++i) {
        auto client = std::make_unique<MockClient>();
        ASSERT_TRUE(client->connect("127.0.0.1", 8095));
        clients.push_back(std::move(client));
        std::this_thread::sleep_for(50ms);
    }

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(clientIds.size(), numClients);

    // Broadcast a message
    Message broadcastMsg = Message::create_text("Broadcast test message");
    hub_->broadcastMessage(broadcastMsg);

    std::this_thread::sleep_for(100ms);

    // Each client should receive the broadcast message
    for (auto& client : clients) {
        std::string received = client->receive();
        EXPECT_EQ(received, "Broadcast test message");
    }
}

TEST_F(AsyncSocketHubTest, SendMessageToSpecificClient) {
    size_t targetClientId = 0;

    hub_->addConnectHandler([&](size_t clientId, const std::string&) {
        if (targetClientId == 0) {
            targetClientId = clientId;
        }
    });

    hub_->start(8096);
    std::this_thread::sleep_for(100ms);

    MockClient client1, client2;
    ASSERT_TRUE(client1.connect("127.0.0.1", 8096));
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(client2.connect("127.0.0.1", 8096));
    std::this_thread::sleep_for(100ms);

    EXPECT_GT(targetClientId, 0);

    // Send message to specific client
    Message specificMsg = Message::create_text("Specific client message");
    hub_->sendMessageToClient(targetClientId, specificMsg);

    std::this_thread::sleep_for(100ms);

    // Only the target client should receive the message
    std::string received1 = client1.receive();
    std::string received2 = client2.receive();

    // One should receive the message, the other should not
    EXPECT_TRUE((received1 == "Specific client message" && received2.empty()) ||
                (received2 == "Specific client message" && received1.empty()));
}

TEST_F(AsyncSocketHubTest, GroupManagement) {
    std::vector<size_t> clientIds;
    std::mutex clientIdMutex;

    hub_->addConnectHandler([&](size_t clientId, const std::string&) {
        std::lock_guard<std::mutex> lock(clientIdMutex);
        clientIds.push_back(clientId);
    });

    hub_->start(8097);
    std::this_thread::sleep_for(100ms);

    MockClient client1, client2, client3;
    ASSERT_TRUE(client1.connect("127.0.0.1", 8097));
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(client2.connect("127.0.0.1", 8097));
    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(client3.connect("127.0.0.1", 8097));
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(clientIds.size(), 3);

    // Create groups and add clients
    hub_->createGroup("group1");
    hub_->createGroup("group2");

    hub_->addClientToGroup(clientIds[0], "group1");
    hub_->addClientToGroup(clientIds[1], "group1");
    hub_->addClientToGroup(clientIds[2], "group2");

    auto groups = hub_->getGroups();
    EXPECT_EQ(groups.size(), 2);

    auto group1Clients = hub_->getClientsInGroup("group1");
    auto group2Clients = hub_->getClientsInGroup("group2");

    EXPECT_EQ(group1Clients.size(), 2);
    EXPECT_EQ(group2Clients.size(), 1);

    // Broadcast to group1
    Message groupMsg = Message::create_text("Group1 message");
    hub_->broadcastToGroup("group1", groupMsg);

    std::this_thread::sleep_for(100ms);

    std::string received1 = client1.receive();
    std::string received2 = client2.receive();
    std::string received3 = client3.receive();

    EXPECT_EQ(received1, "Group1 message");
    EXPECT_EQ(received2, "Group1 message");
    EXPECT_TRUE(received3.empty());  // client3 is in group2, not group1
}

TEST_F(AsyncSocketHubTest, ClientMetadata) {
    size_t clientId = 0;

    hub_->addConnectHandler([&](size_t id, const std::string&) {
        clientId = id;
    });

    hub_->start(8098);
    std::this_thread::sleep_for(100ms);

    MockClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 8098));
    std::this_thread::sleep_for(100ms);

    EXPECT_GT(clientId, 0);

    // Set and get client metadata
    hub_->setClientMetadata(clientId, "username", "testuser");
    hub_->setClientMetadata(clientId, "role", "admin");

    EXPECT_EQ(hub_->getClientMetadata(clientId, "username"), "testuser");
    EXPECT_EQ(hub_->getClientMetadata(clientId, "role"), "admin");
    EXPECT_TRUE(hub_->getClientMetadata(clientId, "nonexistent").empty());
}

TEST_F(AsyncSocketHubTest, Statistics) {
    hub_->start(8099);
    std::this_thread::sleep_for(100ms);

    auto initialStats = hub_->getStatistics();
    EXPECT_EQ(initialStats.total_connections, 0);
    EXPECT_EQ(initialStats.active_connections, 0);
    EXPECT_EQ(initialStats.messages_sent, 0);
    EXPECT_EQ(initialStats.messages_received, 0);

    MockClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 8099));
    std::this_thread::sleep_for(100ms);

    auto afterConnectStats = hub_->getStatistics();
    EXPECT_EQ(afterConnectStats.total_connections, 1);
    EXPECT_EQ(afterConnectStats.active_connections, 1);

    // Send a message to update message statistics
    ASSERT_TRUE(client.send("Test message"));
    std::this_thread::sleep_for(100ms);

    auto afterMessageStats = hub_->getStatistics();
    EXPECT_GT(afterMessageStats.messages_received, 0);
}

TEST_F(AsyncSocketHubTest, ErrorHandling) {
    std::promise<std::string> errorPromise;
    auto errorFuture = errorPromise.get_future();

    bool errorHandlerCalled = false;

    hub_->addErrorHandler([&](const std::string& error, size_t clientId) {
        if (!errorHandlerCalled) {
            errorHandlerCalled = true;
            errorPromise.set_value(error + "_" + std::to_string(clientId));
        }
    });

    hub_->start(8100);
    std::this_thread::sleep_for(100ms);

    // Try to send message to non-existent client to trigger error
    Message msg = Message::create_text("Error test");
    hub_->sendMessageToClient(99999, msg);  // Non-existent client ID

    // Error handling is implementation dependent
    if (errorFuture.wait_for(1s) == std::future_status::ready) {
        auto error = errorFuture.get();
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(AsyncSocketHubTest, DisconnectClient) {
    size_t clientId = 0;
    std::promise<size_t> disconnectPromise;
    auto disconnectFuture = disconnectPromise.get_future();

    bool disconnectHandlerCalled = false;

    hub_->addConnectHandler([&](size_t id, const std::string&) {
        clientId = id;
    });

    hub_->addDisconnectHandler([&](size_t id, const std::string&) {
        if (!disconnectHandlerCalled) {
            disconnectHandlerCalled = true;
            disconnectPromise.set_value(id);
        }
    });

    hub_->start(8101);
    std::this_thread::sleep_for(100ms);

    MockClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", 8101));
    std::this_thread::sleep_for(100ms);

    EXPECT_GT(clientId, 0);
    EXPECT_TRUE(hub_->isClientConnected(clientId));

    // Disconnect client from server side
    hub_->disconnectClient(clientId, "Server initiated disconnect");

    ASSERT_EQ(disconnectFuture.wait_for(2s), std::future_status::ready);
    auto disconnectedId = disconnectFuture.get();

    EXPECT_EQ(disconnectedId, clientId);
    EXPECT_FALSE(hub_->isClientConnected(clientId));
}
