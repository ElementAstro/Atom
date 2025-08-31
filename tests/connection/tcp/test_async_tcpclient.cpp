#include "atom/connection/async_tcpclient.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <future>
#include <thread>
#include <chrono>

using namespace atom::async::connection;
using namespace std::chrono_literals;

class MockAsyncServer {
public:
    MockAsyncServer(int port) : port_(port), serverSocket_(-1), stop_(false) {}

    ~MockAsyncServer() { stop(); }

    void start() { 
        serverThread_ = std::thread(&MockAsyncServer::run, this); 
        // Give server time to start
        std::this_thread::sleep_for(100ms);
    }

    void stop() {
        if (serverThread_.joinable()) {
            stop_ = true;
            if (serverSocket_ != -1) {
#ifdef _WIN32
                closesocket(serverSocket_);
                WSACleanup();
#else
                close(serverSocket_);
#endif
            }
            serverThread_.join();
        }
    }

    void setEchoMode(bool echo) { echoMode_ = echo; }
    void setDelayedResponse(std::chrono::milliseconds delay) { responseDelay_ = delay; }

private:
    void run() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ == -1) return;

        struct sockaddr_in serverAddr {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);

        int opt = 1;
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            return;
        }

        if (listen(serverSocket_, 5) < 0) {
            return;
        }

        while (!stop_) {
            struct sockaddr_in clientAddr {};
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0 || stop_) break;

            std::thread([this, clientSocket]() {
                handleClient(clientSocket);
            }).detach();
        }
    }

    void handleClient(int clientSocket) {
        char buffer[1024];
        while (!stop_) {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) break;

            if (responseDelay_.count() > 0) {
                std::this_thread::sleep_for(responseDelay_);
            }

            if (echoMode_) {
                send(clientSocket, buffer, bytesRead, 0);
            } else {
                std::string response = "Server response";
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }

    int port_;
    int serverSocket_;
    bool stop_;
    bool echoMode_ = true;
    std::chrono::milliseconds responseDelay_{0};
    std::thread serverThread_;
};

class AsyncTcpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockServer_ = std::make_unique<MockAsyncServer>(8081);
        mockServer_->start();
        
        ConnectionConfig config;
        config.use_ssl = false;
        config.connection_timeout = 5000ms;
        config.read_timeout = 3000ms;
        config.write_timeout = 3000ms;
        client_ = std::make_unique<TcpClient>(config);
    }

    void TearDown() override {
        if (client_) {
            client_->disconnect();
        }
        mockServer_->stop();
    }

    std::unique_ptr<MockAsyncServer> mockServer_;
    std::unique_ptr<TcpClient> client_;
};

TEST_F(AsyncTcpClientTest, BasicConnection) {
    bool connected = false;
    client_->setOnConnectedCallback([&connected]() {
        connected = true;
    });

    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    
    // Wait for connection callback
    auto start = std::chrono::steady_clock::now();
    while (!connected && std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_TRUE(connected);
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(AsyncTcpClientTest, AsyncConnection) {
    auto future = client_->connectAsync("127.0.0.1", 8081);
    
    ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
    EXPECT_TRUE(future.get());
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(AsyncTcpClientTest, ConnectionCallbacks) {
    bool connecting = false;
    bool connected = false;
    bool disconnected = false;
    ConnectionState lastState = ConnectionState::Disconnected;

    client_->setOnConnectingCallback([&connecting]() { connecting = true; });
    client_->setOnConnectedCallback([&connected]() { connected = true; });
    client_->setOnDisconnectedCallback([&disconnected]() { disconnected = true; });
    client_->setOnStateChangedCallback([&lastState](ConnectionState oldState, ConnectionState newState) {
        lastState = newState;
    });

    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    
    // Wait for callbacks
    std::this_thread::sleep_for(500ms);
    
    EXPECT_TRUE(connecting);
    EXPECT_TRUE(connected);
    EXPECT_EQ(lastState, ConnectionState::Connected);

    client_->disconnect();
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(disconnected);
}

TEST_F(AsyncTcpClientTest, SendAndReceiveData) {
    mockServer_->setEchoMode(true);
    
    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);

    std::string testMessage = "Hello, Async TCP!";
    std::vector<char> data(testMessage.begin(), testMessage.end());
    
    ASSERT_TRUE(client_->send(data));
    
    auto future = client_->receive(data.size());
    ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
    
    auto received = future.get();
    std::string receivedMessage(received.begin(), received.end());
    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncTcpClientTest, SendString) {
    mockServer_->setEchoMode(true);
    
    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);

    std::string testMessage = "String message test";
    ASSERT_TRUE(client_->sendString(testMessage));
    
    auto future = client_->receive(testMessage.length());
    ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
    
    auto received = future.get();
    std::string receivedMessage(received.begin(), received.end());
    EXPECT_EQ(receivedMessage, testMessage);
}

TEST_F(AsyncTcpClientTest, SendWithTimeout) {
    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);

    std::vector<char> data = {'T', 'e', 's', 't'};
    EXPECT_TRUE(client_->sendWithTimeout(data, 1000ms));
}

TEST_F(AsyncTcpClientTest, ReceiveTimeout) {
    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);

    auto future = client_->receive(100, 500ms);
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    
    // Should timeout and return empty vector
    auto received = future.get();
    EXPECT_TRUE(received.empty());
}

TEST_F(AsyncTcpClientTest, DataReceivedCallback) {
    mockServer_->setEchoMode(true);
    
    std::vector<char> receivedData;
    bool dataReceived = false;
    
    client_->setOnDataReceivedCallback([&](const std::vector<char>& data) {
        receivedData = data;
        dataReceived = true;
    });

    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);

    std::string testMessage = "Callback test";
    std::vector<char> data(testMessage.begin(), testMessage.end());
    
    ASSERT_TRUE(client_->send(data));
    
    // Wait for callback
    auto start = std::chrono::steady_clock::now();
    while (!dataReceived && std::chrono::steady_clock::now() - start < 3s) {
        std::this_thread::sleep_for(10ms);
    }
    
    EXPECT_TRUE(dataReceived);
    std::string received(receivedData.begin(), receivedData.end());
    EXPECT_EQ(received, testMessage);
}

TEST_F(AsyncTcpClientTest, ErrorCallback) {
    std::string lastError;
    bool errorOccurred = false;
    
    client_->setOnErrorCallback([&](const std::string& error) {
        lastError = error;
        errorOccurred = true;
    });

    // Try to connect to non-existent server
    EXPECT_FALSE(client_->connect("127.0.0.1", 9999, 1000ms));
    
    // Wait for error callback
    std::this_thread::sleep_for(1500ms);
    
    EXPECT_TRUE(errorOccurred);
    EXPECT_FALSE(lastError.empty());
}

TEST_F(AsyncTcpClientTest, ConnectionFailure) {
    // Try to connect to non-existent port
    EXPECT_FALSE(client_->connect("127.0.0.1", 9999, 1000ms));
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(AsyncTcpClientTest, Disconnect) {
    ASSERT_TRUE(client_->connect("127.0.0.1", 8081));
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(client_->isConnected());
    
    client_->disconnect();
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(AsyncTcpClientTest, ReconnectionConfiguration) {
    client_->configureReconnection(3, 100ms);
    
    // This test verifies the configuration is accepted
    // Actual reconnection testing would require more complex setup
    EXPECT_NO_THROW(client_->configureReconnection(5, 200ms));
}

TEST_F(AsyncTcpClientTest, HeartbeatConfiguration) {
    std::vector<char> heartbeatData = {'H', 'B'};
    
    EXPECT_NO_THROW(client_->setHeartbeatInterval(1000ms, heartbeatData));
    EXPECT_NO_THROW(client_->setHeartbeatInterval(500ms));
}
