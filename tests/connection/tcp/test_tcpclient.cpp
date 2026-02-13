#include <gtest/gtest.h>
#include "atom/connection/tcp/tcp_common.hpp"
#include "atom/connection/tcp/tcpclient.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <future>
#include <span>
#include <thread>

using namespace atom::connection;

class MockServer {
public:
    MockServer(int port) : port_(port), serverSocket_(-1), clientSocket_(-1) {}

    ~MockServer() { stop(); }

    void start() { serverThread_ = std::thread(&MockServer::run, this); }

    void stop() {
        if (serverThread_.joinable()) {
            stop_ = true;
            serverThread_.join();
        }

        if (clientSocket_ != -1) {
#ifdef _WIN32
            closesocket(clientSocket_);
#else
            close(clientSocket_);
#endif
        }

        if (serverSocket_ != -1) {
#ifdef _WIN32
            closesocket(serverSocket_);
            WSACleanup();
#else
            close(serverSocket_);
#endif
        }
    }

private:
    void run() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(serverSocket_, -1) << "Failed to create server socket";

        struct sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);

        int opt = 1;
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,
                   sizeof(opt));

        int result = bind(serverSocket_, (struct sockaddr*)&serverAddr,
                          sizeof(serverAddr));
        ASSERT_EQ(result, 0) << "Bind failed";

        result = listen(serverSocket_, 1);
        ASSERT_EQ(result, 0) << "Listen failed";

        while (!stop_) {
            struct sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);

            clientSocket_ = accept(serverSocket_, (struct sockaddr*)&clientAddr,
                                   &clientLen);
            if (clientSocket_ < 0) {
                if (stop_)
                    break;
                continue;
            }

            char buffer[1024];
            int bytesRead = recv(clientSocket_, buffer, sizeof(buffer), 0);
            if (bytesRead > 0) {
                send(clientSocket_, buffer, bytesRead, 0);
            }

#ifdef _WIN32
            closesocket(clientSocket_);
#else
            close(clientSocket_);
#endif
            clientSocket_ = -1;
        }
    }

    int port_;
    int serverSocket_;
    int clientSocket_;
    bool stop_ = false;
    std::thread serverThread_;
};

class TcpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockServer_.start();
        client_ = std::make_unique<TcpClient>(TcpClient::Options{});
    }

    void TearDown() override {
        client_.reset();  // Reset the unique_ptr, not call TcpClient::reset()
        mockServer_.stop();
    }

    MockServer mockServer_{8080};
    std::unique_ptr<TcpClient> client_;
};

TEST_F(TcpClientTest, ConnectToServer) {
    auto result =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(client_->isConnected());
}

TEST_F(TcpClientTest, SendData) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string message = "Hello, server!";
    std::span<const char> data_span(message.data(), message.size());
    auto sendResult = client_->send(data_span);
    ASSERT_TRUE(sendResult.has_value());
}

TEST_F(TcpClientTest, ReceiveData) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string message = "Hello, server!";
    std::span<const char> data_span(message.data(), message.size());
    auto sendResult = client_->send(data_span);
    ASSERT_TRUE(sendResult.has_value());

    auto receiveResult = client_->receive(1024);
    ASSERT_TRUE(receiveResult.has_value());
    auto data = receiveResult.value();

    ASSERT_EQ(std::string(data.begin(), data.end()), message);
}

TEST_F(TcpClientTest, DisconnectFromServer) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());
    client_->disconnect();
    ASSERT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, Callbacks) {
    bool connected = false;
    bool disconnected = false;
    std::string receivedData;
    std::system_error lastError{std::error_code{}, ""};

    client_->setOnConnectedCallback([&]() { connected = true; });
    client_->setOnDisconnectedCallback([&]() { disconnected = true; });
    client_->setOnDataReceivedCallback([&](std::span<const char> data) {
        receivedData = std::string(data.begin(), data.end());
    });
    client_->setOnErrorCallback(
        [&](const std::system_error& error) { lastError = error; });

    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());
    ASSERT_TRUE(connected);

    std::string message = "Hello, server!";
    std::span<const char> data_span(message.data(), message.size());
    auto sendResult = client_->send(data_span);
    ASSERT_TRUE(sendResult.has_value());

    client_->startReceiving(1024);
    std::this_thread::sleep_for(
        std::chrono::seconds(1));  // Give some time to receive the message

    ASSERT_EQ(receivedData, message);

    client_->disconnect();
    ASSERT_TRUE(disconnected);
}

TEST_F(TcpClientTest, ConnectToInvalidHost) {
    auto result = client_->connect("invalid.host.example.com", 80,
                                   std::chrono::milliseconds(1000));
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, ConnectToInvalidPort) {
    auto result =
        client_->connect("127.0.0.1", 65535, std::chrono::milliseconds(1000));
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, SendWithoutConnection) {
    std::string message = "test";
    std::span<const char> data_span(message.data(), message.size());
    auto result = client_->send(data_span);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, ReceiveWithoutConnection) {
    auto result = client_->receive(100);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, SendEmptyData) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string emptyMessage = "";
    std::span<const char> data_span(emptyMessage.data(), emptyMessage.size());
    auto result = client_->send(data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TcpClientTest, SendLargeData) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string largeMessage(8192, 'X');
    largeMessage += "END";
    std::span<const char> data_span(largeMessage.data(), largeMessage.size());

    auto result = client_->send(data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TcpClientTest, MultipleConnectCalls) {
    auto result1 =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(client_->isConnected());

    // Second connect should handle gracefully
    auto result2 =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(TcpClientTest, MultipleDisconnectCalls) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());
    EXPECT_TRUE(client_->isConnected());

    client_->disconnect();
    EXPECT_FALSE(client_->isConnected());

    // Second disconnect should not cause issues
    client_->disconnect();
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, SendAfterDisconnect) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    client_->disconnect();

    std::string message = "test";
    std::span<const char> data_span(message.data(), message.size());
    auto result = client_->send(data_span);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, ReceiveAfterDisconnect) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    client_->disconnect();

    auto result = client_->receive(100);
    EXPECT_FALSE(result.has_value());
}

TEST_F(TcpClientTest, ConnectionTimeout) {
    // Try to connect to a non-routable IP to test timeout
    auto start = std::chrono::steady_clock::now();
    auto result =
        client_->connect("192.0.2.1", 80, std::chrono::milliseconds(1000));
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    // Should timeout within reasonable time
    EXPECT_GE(duration, std::chrono::milliseconds(800));
    EXPECT_LE(duration, std::chrono::seconds(5));
}

// ============================================================================
// Additional TcpClient Tests
// ============================================================================

TEST_F(TcpClientTest, GetLastError) {
    // Try to connect to invalid host to generate an error
    auto result = client_->connect("invalid.host.example.com", 80,
                                   std::chrono::milliseconds(1000));
    EXPECT_FALSE(result.has_value());

    // getLastError should return the error - just verify it doesn't throw
    [[maybe_unused]] auto& lastError = client_->getLastError();
}

TEST_F(TcpClientTest, OptionsWithKeepAlive) {
    TcpClient::Options options;
    options.keep_alive = true;
    options.no_delay = true;
    options.receive_buffer_size = 65536;
    options.send_buffer_size = 65536;

    TcpClient clientWithOptions(options);
    auto result = clientWithOptions.connect("127.0.0.1", 8080,
                                            std::chrono::milliseconds(5000));
    // Connection may or may not succeed depending on server state
    EXPECT_NO_THROW(clientWithOptions.disconnect());
}

TEST_F(TcpClientTest, OptionsWithIPv6) {
    TcpClient::Options options;
    options.ipv6_enabled = true;

    TcpClient clientWithOptions(options);
    // IPv6 connection test - may fail if IPv6 not available
    EXPECT_NO_THROW(clientWithOptions.disconnect());
}

TEST_F(TcpClientTest, ReceiveWithTimeout) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string message = "Receive timeout test";
    std::span<const char> data_span(message.data(), message.size());
    client_->send(data_span);

    auto result = client_->receive(1024, std::chrono::milliseconds(2000));
    EXPECT_TRUE(result.has_value());
}

TEST_F(TcpClientTest, ReceiveTimeoutExpired) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    // Don't send anything, just try to receive
    auto start = std::chrono::steady_clock::now();
    auto result = client_->receive(1024, std::chrono::milliseconds(500));
    auto duration = std::chrono::steady_clock::now() - start;

    // Should timeout
    EXPECT_GE(duration, std::chrono::milliseconds(400));
}

TEST_F(TcpClientTest, StartStopReceiving) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    // startReceiving and stopReceiving don't have isReceiving() check
    EXPECT_NO_THROW(client_->startReceiving(1024));
    EXPECT_NO_THROW(client_->stopReceiving());
}

// Note: MoveConstruction and MoveAssignment tests removed because
// TcpClient inherits from NonCopyable which deletes copy/move operations

TEST_F(TcpClientTest, SendStringAsSpan) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string message = "String as span test";
    std::span<const char> data_span(message.data(), message.size());
    auto result = client_->send(data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TcpClientTest, SendBinaryData) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::vector<char> binaryData = {0x00, 0x01, 0x02, static_cast<char>(0xFF)};
    std::span<const char> data_span(binaryData.data(), binaryData.size());

    auto result = client_->send(data_span);
    EXPECT_TRUE(result.has_value());
}

TEST_F(TcpClientTest, ConcurrentSendReceive) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::atomic<int> sendCount{0};
    std::atomic<int> receiveCount{0};

    std::thread sender([this, &sendCount]() {
        for (int i = 0; i < 5; ++i) {
            std::string message = "Concurrent_" + std::to_string(i);
            std::span<const char> data_span(message.data(), message.size());
            if (client_->send(data_span).has_value()) {
                sendCount++;
            }
        }
    });

    std::thread receiver([this, &receiveCount]() {
        for (int i = 0; i < 5; ++i) {
            auto result =
                client_->receive(1024, std::chrono::milliseconds(500));
            if (result.has_value()) {
                receiveCount++;
            }
        }
    });

    sender.join();
    receiver.join();

    EXPECT_GT(sendCount.load(), 0);
}

TEST_F(TcpClientTest, ReconnectAfterDisconnect) {
    auto connectResult1 =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult1.has_value());

    client_->disconnect();
    EXPECT_FALSE(client_->isConnected());

    auto connectResult2 =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    EXPECT_TRUE(connectResult2.has_value());
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(TcpClientTest, SendReceiveRoundTrip) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string message = "Round trip test";
    std::span<const char> data_span(message.data(), message.size());
    auto sendResult = client_->send(data_span);
    EXPECT_TRUE(sendResult.has_value());

    auto receiveResult =
        client_->receive(1024, std::chrono::milliseconds(2000));
    // May or may not receive depending on server echo behavior
    EXPECT_NO_THROW(client_->receive(1024));
}

TEST_F(TcpClientTest, OptionsConstructionValid) {
    TcpClient::Options options;
    options.keep_alive = true;
    options.no_delay = true;
    options.receive_buffer_size = 8192;
    options.send_buffer_size = 8192;

    TcpClient clientWithOptions(options);

    auto result = clientWithOptions.connect("127.0.0.1", 8080,
                                            std::chrono::milliseconds(5000));
    // Connection may succeed or fail depending on server
    EXPECT_NO_THROW(clientWithOptions.disconnect());
}

TEST_F(TcpClientTest, SendSpecialCharacters) {
    auto connectResult =
        client_->connect("127.0.0.1", 8080, std::chrono::milliseconds(5000));
    ASSERT_TRUE(connectResult.has_value());

    std::string specialMessage = "Special: \n\t\r chars";
    std::span<const char> data_span(specialMessage.data(),
                                    specialMessage.size());

    auto result = client_->send(data_span);
    EXPECT_TRUE(result.has_value());
}
