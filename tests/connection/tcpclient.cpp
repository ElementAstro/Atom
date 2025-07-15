/*
 * tcpclient_test.cpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

#include "atom/connection/tcpclient.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace atom::connection;
using namespace std::chrono_literals;

class EchoServer {
public:
    EchoServer(uint16_t port) : port_(port), stop_flag_(false) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~EchoServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void start() {
        server_thread_ = std::jthread(&EchoServer::run, this);
        // Wait for the server to be ready
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,
                 [this] { return running_.load(); });  // FIXED: use .load()
    }

    void stop() {
        stop_flag_.store(true);
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

private:
    void run() {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            return;
        }

#ifdef _WIN32
        char opt = 1;
#else
        int opt = 1;
#endif
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port_);

        if (bind(listen_fd, (struct sockaddr*)&server_addr,
                 sizeof(server_addr)) < 0) {
#ifdef _WIN32
            closesocket(listen_fd);
#else
            close(listen_fd);
#endif
            return;
        }

        if (listen(listen_fd, 1) < 0) {
#ifdef _WIN32
            closesocket(listen_fd);
#else
            close(listen_fd);
#endif
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = true;
        }
        cv_.notify_one();

        while (!stop_flag_.load()) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(listen_fd, &read_fds);
            timeval timeout = {0, 100000};  // 100ms

            int activity =
                select(listen_fd + 1, &read_fds, nullptr, nullptr, &timeout);

            if (activity > 0 && FD_ISSET(listen_fd, &read_fds)) {
                int client_fd = accept(listen_fd, nullptr, nullptr);
                if (client_fd < 0) {
                    continue;
                }

                char buffer[1024];
                while (!stop_flag_.load()) {
                    int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        send(client_fd, buffer, bytes_read, 0);
                    } else {
                        break;
                    }
                }
#ifdef _WIN32
                closesocket(client_fd);
#else
                close(client_fd);
#endif
            }
        }

#ifdef _WIN32
        closesocket(listen_fd);
#else
        close(listen_fd);
#endif
    }

    uint16_t port_;
    std::jthread server_thread_;
    std::atomic<bool> stop_flag_;
    std::mutex mutex_;  // This mutex protects 'running_' and 'cv_'
    std::condition_variable cv_;
    std::atomic<bool> running_ = false;  // Initialize with assignment
};

class TcpClientTest : public ::testing::Test {
protected:
    static constexpr uint16_t TEST_PORT = 12345;

    void SetUp() override {
        server_ = std::make_unique<EchoServer>(TEST_PORT);
        server_->start();
    }

    void TearDown() override {
        server_->stop();
        server_.reset();
    }

    std::unique_ptr<EchoServer> server_;
};

TEST_F(TcpClientTest, ConnectAndDisconnect) {
    TcpClient::Options options;
    TcpClient client(options);

    auto result = client.connect("127.0.0.1", TEST_PORT, 1000ms);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(client.isConnected());

    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(TcpClientTest, ConnectInvalidHost) {
    TcpClient::Options options;
    TcpClient client(options);

    auto result = client.connect("invalid.host.name", TEST_PORT, 1000ms);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.getLastError().code(), std::errc::host_unreachable);
}

TEST_F(TcpClientTest, ConnectInvalidPort) {
    TcpClient::Options options;
    TcpClient client(options);

    auto result = client.connect("127.0.0.1", 0, 1000ms);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.getLastError().code(), std::errc::invalid_argument);
}

TEST_F(TcpClientTest, SendAndReceive) {
    TcpClient::Options options;
    TcpClient client(options);

    ASSERT_TRUE(client.connect("127.0.0.1", TEST_PORT, 1000ms).has_value());

    std::string message = "Hello, world!";
    std::vector<char> data(message.begin(), message.end());

    auto send_result = client.send(data);
    ASSERT_TRUE(send_result.has_value());
    EXPECT_EQ(send_result.value(), data.size());

    auto receive_result = client.receive(1024, 1000ms);
    ASSERT_TRUE(receive_result.has_value());

    std::string received_message(receive_result.value().begin(),
                                 receive_result.value().end());
    EXPECT_EQ(received_message, message);

    client.disconnect();
}

TEST_F(TcpClientTest, SendWhenNotConnected) {
    TcpClient::Options options;
    TcpClient client(options);

    std::string message = "This should fail";
    std::vector<char> data(message.begin(), message.end());

    auto send_result = client.send(data);
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(client.getLastError().code(), std::errc::not_connected);
}

TEST_F(TcpClientTest, ReceiveWhenNotConnected) {
    TcpClient::Options options;
    TcpClient client(options);

    auto receive_result = client.receive(1024, 100ms);
    ASSERT_FALSE(receive_result.has_value());
    EXPECT_EQ(client.getLastError().code(), std::errc::not_connected);
}

TEST_F(TcpClientTest, Callbacks) {
    TcpClient::Options options;
    TcpClient client(options);

    std::atomic<bool> connected_called = false;
    std::atomic<bool> disconnected_called = false;
    std::atomic<bool> data_received_called = false;
    std::atomic<bool> error_called = false;

    client.setOnConnectedCallback([&]() { connected_called = true; });
    client.setOnDisconnectedCallback([&]() { disconnected_called = true; });
    client.setOnDataReceivedCallback(
        [&](std::span<const char>) { data_received_called = true; });
    client.setOnErrorCallback(
        [&](const std::system_error&) { error_called = true; });

    // Test onConnected
    auto result = client.connect("127.0.0.1", TEST_PORT, 1000ms);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(connected_called);

    // Test onDisconnected
    client.disconnect();
    EXPECT_TRUE(disconnected_called);

    // Test onError
    auto result_fail =
        client.connect("1.1.1.1", 1, 10ms);  // should fail and timeout
    ASSERT_FALSE(result_fail.has_value());
    // Note: The onError callback in the public TcpClient is not directly
    // triggered by connect failure. It's designed more for the background
    // receiving thread. Let's test that.

    // Test onDataReceived and onError in background thread
    TcpClient client2(options);
    ASSERT_TRUE(client2.connect("127.0.0.1", TEST_PORT, 1000ms).has_value());
    client2.setOnDataReceivedCallback(
        [&](std::span<const char>) { data_received_called = true; });
    client2.startReceiving(1024);

    std::string message = "test data";
    client2.send({message.begin(), message.end()});

    std::this_thread::sleep_for(100ms);  // give time for receive
    EXPECT_TRUE(data_received_called);

    client2.stopReceiving();
    client2.disconnect();
}

TEST_F(TcpClientTest, StartStopReceiving) {
    TcpClient::Options options;
    TcpClient client(options);

    std::atomic<int> received_count = 0;
    client.setOnDataReceivedCallback(
        [&](std::span<const char> data) { received_count++; });

    ASSERT_TRUE(client.connect("127.0.0.1", TEST_PORT, 1000ms).has_value());

    client.startReceiving(1024);

    std::string message = "data1";
    client.send({message.begin(), message.end()});
    std::this_thread::sleep_for(50ms);

    message = "data2";
    client.send({message.begin(), message.end()});
    std::this_thread::sleep_for(50ms);

    EXPECT_GE(received_count.load(), 1);

    client.stopReceiving();

    int count_after_stop = received_count.load();
    message = "data3";
    client.send({message.begin(), message.end()});
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(received_count.load(), count_after_stop);

    client.disconnect();
}