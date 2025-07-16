#include "atom/connection/async_tcpclient.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <chrono>
#include <cstdio>  // For std::remove
#include <future>
#include <thread>

// Silence spdlog during tests to keep output clean
#define SPDLOG_LEVEL_OFF
#include <spdlog/spdlog.h>

using namespace atom::async::connection;
using namespace std::chrono_literals;
using asio::ip::tcp;

// Helper to find an available port
uint16_t find_free_port() {
    asio::io_context io_context;
    tcp::acceptor acceptor(io_context);
    tcp::endpoint endpoint(tcp::v4(), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
}

/**
 * @brief A simple, controllable mock TCP server for testing the client.
 *
 * It runs in its own thread and can be started and stopped by the tests.
 * It primarily functions as an echo server.
 */
class MockServer {
public:
    MockServer(bool use_ssl = false)
        : io_context_(),
          acceptor_(io_context_),
          ssl_context_(asio::ssl::context::sslv23),
          use_ssl_(use_ssl) {}

    ~MockServer() { stop(); }

    void start(uint16_t port) {
        port_ = port;
        server_thread_ = std::thread([this] {
            try {
                if (use_ssl_) {
                    configure_ssl();
                }
                tcp::endpoint endpoint(tcp::v4(), port_);
                acceptor_.open(endpoint.protocol());
                acceptor_.set_option(asio::socket_base::reuse_address(true));
                acceptor_.bind(endpoint);
                acceptor_.listen();
                do_accept();
                io_context_.run();
            } catch (const std::exception& e) {
                // This can happen during shutdown, which is fine.
                // std::cerr << "MockServer thread exception: " << e.what() <<
                // std::endl;
            }
        });
        // Wait a bit for the server thread to start listening
        std::this_thread::sleep_for(50ms);
    }

    void stop() {
        if (!server_thread_.joinable())
            return;

        asio::post(io_context_, [this]() {
            acceptor_.close();
            if (active_socket_ && active_socket_->is_open())
                active_socket_->close();
            io_context_.stop();
        });

        server_thread_.join();
    }

private:
    void configure_ssl() {
        // Use the same test certs as the client test fixture will generate
        ssl_context_.set_options(asio::ssl::context::default_workarounds |
                                 asio::ssl::context::no_sslv2 |
                                 asio::ssl::context::single_dh_use);

        ssl_context_.use_certificate_chain_file("test_server_cert.pem");
        ssl_context_.use_private_key_file("test_server_key.pem",
                                          asio::ssl::context::pem);
    }

    void do_accept() {
        acceptor_.async_accept([this](asio::error_code ec, tcp::socket socket) {
            if (!ec) {
                active_socket_ =
                    std::make_shared<tcp::socket>(std::move(socket));
                if (use_ssl_) {
                    // SSL session setup
                    auto ssl_stream =
                        std::make_shared<asio::ssl::stream<tcp::socket>>(
                            std::move(*active_socket_), ssl_context_);
                    ssl_stream->async_handshake(
                        asio::ssl::stream_base::server,
                        [this,
                         ssl_stream](const asio::error_code& handshake_ec) {
                            if (!handshake_ec) {
                                do_read_ssl(ssl_stream);
                            }
                        });
                } else {
                    // Plain TCP session setup
                    do_read_plain(active_socket_);
                }
            }
            do_accept();  // Continue accepting
        });
    }

    // Echo session for plain TCP
    void do_read_plain(std::shared_ptr<tcp::socket> socket) {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        socket->async_read_some(
            asio::buffer(*buffer),
            [this, socket, buffer](asio::error_code ec, std::size_t length) {
                if (!ec) {
                    asio::async_write(
                        *socket, asio::buffer(buffer->data(), length),
                        [this, socket](asio::error_code /*write_ec*/,
                                       std::size_t /*len*/) {
                            do_read_plain(socket);  // Continue reading
                        });
                }
            });
    }

    // Echo session for SSL
    void do_read_ssl(
        std::shared_ptr<asio::ssl::stream<tcp::socket>> ssl_stream) {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        ssl_stream->async_read_some(
            asio::buffer(*buffer),
            [this, ssl_stream, buffer](asio::error_code ec,
                                       std::size_t length) {
                if (!ec) {
                    asio::async_write(
                        *ssl_stream, asio::buffer(buffer->data(), length),
                        [this, ssl_stream](asio::error_code /*write_ec*/,
                                           std::size_t /*len*/) {
                            do_read_ssl(ssl_stream);  // Continue reading
                        });
                }
            });
    }

    asio::io_context io_context_;
    tcp::acceptor acceptor_;
    asio::ssl::context ssl_context_;
    std::shared_ptr<tcp::socket> active_socket_;
    std::thread server_thread_;
    uint16_t port_;
    bool use_ssl_;
};

// Main test fixture
class TcpClientTest : public ::testing::Test {
protected:
    std::unique_ptr<TcpClient> client_;
    std::unique_ptr<MockServer> server_;
    uint16_t port_;

    void SetUp() override {
        port_ = find_free_port();
        server_ = std::make_unique<MockServer>();
        server_->start(port_);
    }

    void TearDown() override {
        if (client_) {
            client_->disconnect();
        }
        if (server_) {
            server_->stop();
        }
        // Give OS time to release resources
        std::this_thread::sleep_for(50ms);
    }
};

TEST_F(TcpClientTest, InitialState) {
    client_ = std::make_unique<TcpClient>();
    EXPECT_EQ(client_->getConnectionState(), ConnectionState::Disconnected);
    EXPECT_FALSE(client_->isConnected());
    auto stats = client_->getStats();
    EXPECT_EQ(stats.connection_attempts, 0);
    EXPECT_EQ(stats.total_bytes_sent, 0);
}

TEST_F(TcpClientTest, ConnectAndDisconnect) {
    client_ = std::make_unique<TcpClient>();
    std::promise<void> connect_promise, disconnect_promise;

    client_->setOnConnectedCallback([&] { connect_promise.set_value(); });
    client_->setOnDisconnectedCallback([&] { disconnect_promise.set_value(); });

    ASSERT_TRUE(client_->connect("127.0.0.1", port_));
    EXPECT_EQ(connect_promise.get_future().wait_for(1s),
              std::future_status::ready);

    EXPECT_EQ(client_->getConnectionState(), ConnectionState::Connected);
    EXPECT_TRUE(client_->isConnected());
    EXPECT_EQ(client_->getRemoteAddress(), "127.0.0.1");
    EXPECT_EQ(client_->getRemotePort(), port_);

    auto stats = client_->getStats();
    EXPECT_EQ(stats.connection_attempts, 1);
    EXPECT_EQ(stats.successful_connections, 1);

    client_->disconnect();
    EXPECT_EQ(disconnect_promise.get_future().wait_for(1s),
              std::future_status::ready);
    EXPECT_EQ(client_->getConnectionState(), ConnectionState::Disconnected);
}

TEST_F(TcpClientTest, ConnectAsync) {
    client_ = std::make_unique<TcpClient>();
    auto connect_future = client_->connectAsync("127.0.0.1", port_);
    ASSERT_TRUE(connect_future.get());
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(TcpClientTest, ConnectFails) {
    client_ = std::make_unique<TcpClient>();
    std::promise<std::string> error_promise;
    client_->setOnErrorCallback(
        [&](const std::string& err) { error_promise.set_value(err); });

    // Try to connect to a port where nothing is listening
    uint16_t bad_port = find_free_port();
    ASSERT_FALSE(client_->connect("127.0.0.1", bad_port));

    EXPECT_EQ(error_promise.get_future().wait_for(1s),
              std::future_status::ready);
    EXPECT_EQ(client_->getConnectionState(), ConnectionState::Failed);
    EXPECT_FALSE(client_->isConnected());
    EXPECT_FALSE(client_->getErrorMessage().empty());

    auto stats = client_->getStats();
    EXPECT_EQ(stats.connection_attempts, 1);
    EXPECT_EQ(stats.failed_connections, 1);
}

TEST_F(TcpClientTest, SendAndReceiveData) {
    client_ = std::make_unique<TcpClient>();
    ASSERT_TRUE(client_->connect("127.0.0.1", port_));

    std::promise<std::vector<char>> received_promise;
    client_->setOnDataReceivedCallback([&](const std::vector<char>& data) {
        received_promise.set_value(data);
    });

    std::string message = "Hello, World!";
    ASSERT_TRUE(client_->sendString(message));

    auto received_future = received_promise.get_future();
    ASSERT_EQ(received_future.wait_for(1s), std::future_status::ready);

    auto received_data = received_future.get();
    std::string received_string(received_data.begin(), received_data.end());

    EXPECT_EQ(received_string, message);  // Server echoes the message back

    auto stats = client_->getStats();
    EXPECT_EQ(stats.total_bytes_sent, message.length());
    EXPECT_EQ(stats.total_bytes_received, message.length());
}

TEST_F(TcpClientTest, RequestResponse) {
    client_ = std::make_unique<TcpClient>();
    ASSERT_TRUE(client_->connect("127.0.0.1", port_));

    std::string request_str = "request";
    std::vector<char> request_data(request_str.begin(), request_str.end());

    auto response_future =
        client_->requestResponse(request_data, request_data.size());
    ASSERT_EQ(response_future.wait_for(1s), std::future_status::ready);

    auto response_data = response_future.get();
    std::string response_str(response_data.begin(), response_data.end());

    EXPECT_EQ(response_str, request_str);
}

TEST_F(TcpClientTest, StateChangeCallback) {
    client_ = std::make_unique<TcpClient>();
    std::vector<ConnectionState> states;
    client_->setOnStateChangedCallback(
        [&](ConnectionState old_state, ConnectionState new_state) {
            states.push_back(old_state);
            states.push_back(new_state);
        });

    client_->connect("127.0.0.1", port_);
    std::this_thread::sleep_for(100ms);  // allow callbacks to fire
    client_->disconnect();
    std::this_thread::sleep_for(100ms);  // allow callbacks to fire

    // Expected sequence: Disconnected -> Connecting -> Connected ->
    // Disconnected
    ASSERT_GE(states.size(), 6);
    EXPECT_EQ(states[0], ConnectionState::Disconnected);
    EXPECT_EQ(states[1], ConnectionState::Connecting);
    EXPECT_EQ(states[2], ConnectionState::Connecting);
    EXPECT_EQ(states[3], ConnectionState::Connected);
    EXPECT_EQ(states[4], ConnectionState::Connected);
    EXPECT_EQ(states[5], ConnectionState::Disconnected);
}

TEST_F(TcpClientTest, AutoReconnect) {
    ConnectionConfig config;
    config.auto_reconnect = true;
    config.reconnect_delay = 100ms;
    config.reconnect_attempts = 5;
    client_ = std::make_unique<TcpClient>(config);

    std::promise<void> first_connect_promise;
    std::promise<void> disconnect_promise;
    std::promise<void> reconnect_promise;

    client_->setOnConnectedCallback([&] {
        if (client_->getStats().successful_connections == 1) {
            first_connect_promise.set_value();
        } else {
            reconnect_promise.set_value();
        }
    });
    client_->setOnDisconnectedCallback([&] { disconnect_promise.set_value(); });

    // 1. Initial connection
    ASSERT_TRUE(client_->connect("127.0.0.1", port_));
    ASSERT_EQ(first_connect_promise.get_future().wait_for(1s),
              std::future_status::ready);

    // 2. Kill the server to force a disconnect
    server_->stop();
    ASSERT_EQ(disconnect_promise.get_future().wait_for(2s),
              std::future_status::ready);
    EXPECT_EQ(client_->getConnectionState(), ConnectionState::Reconnecting);

    // 3. Restart the server. The client should reconnect automatically.
    server_->start(port_);
    ASSERT_EQ(reconnect_promise.get_future().wait_for(2s),
              std::future_status::ready);

    EXPECT_TRUE(client_->isConnected());
    auto stats = client_->getStats();
    EXPECT_EQ(stats.successful_connections, 2);
    EXPECT_GE(stats.connection_attempts, 2);
}

TEST_F(TcpClientTest, Heartbeat) {
    ConnectionConfig config;
    config.heartbeat_interval = 200ms;
    client_ = std::make_unique<TcpClient>(config);

    std::promise<void> heartbeat_promise;
    client_->setOnHeartbeatCallback([&] { heartbeat_promise.set_value(); });

    ASSERT_TRUE(client_->connect("127.0.0.1", port_));

    // The heartbeat should fire after the interval
    ASSERT_EQ(heartbeat_promise.get_future().wait_for(500ms),
              std::future_status::ready);
}

// Fixture for SSL/TLS tests
class TcpClientSslTest : public ::testing::Test {
protected:
    std::unique_ptr<TcpClient> client_;
    std::unique_ptr<MockServer> server_;
    uint16_t port_;
    const char* cert_file = "test_server_cert.pem";
    const char* key_file = "test_server_key.pem";

    void SetUp() override {
        // Generate self-signed certs for the mock server
        int res = system(
            "openssl req -x509 -newkey rsa:2048 -nodes -keyout "
            "test_server_key.pem -out test_server_cert.pem -subj "
            "\"/CN=localhost\" -days 1 > /dev/null 2>&1");
        if (res != 0) {
            GTEST_SKIP()
                << "Skipping SSL tests: Could not generate certificates. Is "
                   "OpenSSL installed and in the PATH?";
        }

        port_ = find_free_port();
        server_ =
            std::make_unique<MockServer>(true);  // Create SSL-enabled server
        server_->start(port_);
    }

    void TearDown() override {
        if (client_)
            client_->disconnect();
        if (server_)
            server_->stop();
        std::remove(cert_file);
        std::remove(key_file);
        std::this_thread::sleep_for(50ms);
    }
};

TEST_F(TcpClientSslTest, SslConnectsSuccessfully) {
    ConnectionConfig config;
    config.use_ssl = true;
    config.verify_ssl =
        false;  // We don't verify the self-signed cert in this test
    client_ = std::make_unique<TcpClient>(config);

    std::promise<void> connect_promise;
    client_->setOnConnectedCallback([&] { connect_promise.set_value(); });

    ASSERT_TRUE(client_->connect("127.0.0.1", port_));
    ASSERT_EQ(connect_promise.get_future().wait_for(2s),
              std::future_status::ready);

    EXPECT_TRUE(client_->isConnected());

    // Test data transfer over SSL
    std::promise<std::vector<char>> received_promise;
    client_->setOnDataReceivedCallback(
        [&](const auto& data) { received_promise.set_value(data); });

    std::string message = "Secure Hello";
    client_->sendString(message);

    auto received_future = received_promise.get_future();
    ASSERT_EQ(received_future.wait_for(1s), std::future_status::ready);
    auto received_data = received_future.get();
    std::string received_string(received_data.begin(), received_data.end());

    EXPECT_EQ(received_string, message);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}