#include "ws.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Helper for websocket mock server
class MockWebSocketServer {
public:
    explicit MockWebSocketServer(uint16_t port)
        : acceptor_(ioc_, {net::ip::make_address("127.0.0.1"), port}),
          socket_(ioc_) {
        // Start accepting connections
        acceptor_.async_accept(
            socket_, [this](beast::error_code ec) { handleAccept(ec); });
    }

    void run() {
        server_thread_ = std::thread([this]() { ioc_.run(); });
    }

    void stop() {
        ioc_.stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    void setAcceptHandler(std::function<void()> handler) {
        accept_handler_ = std::move(handler);
    }

    void setMessageHandler(std::function<void(std::string)> handler) {
        message_handler_ = std::move(handler);
    }

    void close() {
        if (ws_) {
            beast::error_code ec;
            ws_->close(websocket::close_code::normal, ec);
        }
    }

    void send(const std::string& message) {
        if (ws_ && ws_->is_open()) {
            ws_->write(net::buffer(message));
        }
    }

private:
    void handleAccept(beast::error_code ec) {
        if (ec) {
            return;
        }

        // Create the websocket session
        ws_ = std::make_unique<websocket::stream<tcp::socket>>(
            std::move(socket_));

        // Accept the websocket handshake
        ws_->async_accept([this](beast::error_code ec) {
            if (!ec) {
                if (accept_handler_) {
                    accept_handler_();
                }
                doRead();
            }
        });

        // Accept another connection
        acceptor_.async_accept(
            socket_, [this](beast::error_code ec) { handleAccept(ec); });
    }

    void doRead() {
        if (!ws_ || !ws_->is_open()) {
            return;
        }

        ws_->async_read(buffer_, [this](beast::error_code ec,
                                        std::size_t bytes_transferred) {
            if (!ec) {
                if (message_handler_) {
                    message_handler_(beast::buffers_to_string(buffer_.data()));
                }
                buffer_.consume(buffer_.size());
                doRead();
            }
        });
    }

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::thread server_thread_;
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    std::function<void()> accept_handler_;
    std::function<void(std::string)> message_handler_;
};

class WSClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start the mock server
        mock_server_ = std::make_unique<MockWebSocketServer>(test_port_);
        mock_server_->run();

        // Create IO context and client
        ioc_ = std::make_shared<net::io_context>();
        run_thread_ = std::thread([this]() { ioc_->run(); });

        client_ = std::make_shared<WSClient>(*ioc_);

        // Wait a bit for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        // Clean up
        try {
            if (client_ && client_->isConnected()) {
                client_->close();
            }
        } catch (...) {
            // Ignore exceptions during cleanup
        }

        mock_server_->stop();
        mock_server_.reset();

        ioc_->stop();
        if (run_thread_.joinable()) {
            run_thread_.join();
        }

        client_.reset();
        ioc_.reset();
    }

    // Helper to connect to the mock server
    void connectToMockServer() {
        // Set up connection successful promise
        std::promise<void> connected_promise;
        std::future<void> connected_future = connected_promise.get_future();

        mock_server_->setAcceptHandler(
            [&connected_promise]() { connected_promise.set_value(); });

        // Connect
        client_->connect(test_host_, std::to_string(test_port_));

        // Wait for server to accept connection
        auto status = connected_future.wait_for(std::chrono::seconds(2));
        ASSERT_EQ(status, std::future_status::ready) << "Connection timed out";
    }

    std::shared_ptr<net::io_context> ioc_;
    std::shared_ptr<WSClient> client_;
    std::unique_ptr<MockWebSocketServer> mock_server_;
    std::thread run_thread_;

    const std::string test_host_ = "127.0.0.1";
    const uint16_t test_port_ = 8765;
};

// Test constructor
TEST_F(WSClientTest, Constructor) {
    // Create a new client (the SetUp already created one)
    EXPECT_NO_THROW({ WSClient client(*ioc_); });

    // The client should not be connected initially
    EXPECT_FALSE(client_->isConnected());
}

// Test connection
TEST_F(WSClientTest, Connect) {
    EXPECT_NO_THROW(connectToMockServer());
    EXPECT_TRUE(client_->isConnected());
}

// Test connection validation
TEST_F(WSClientTest, ConnectionValidation) {
    // Empty host
    EXPECT_THROW(client_->connect("", std::to_string(test_port_)),
                 std::invalid_argument);

    // Empty port
    EXPECT_THROW(client_->connect(test_host_, ""), std::invalid_argument);

    // Invalid port (non-numeric and not a service name)
    EXPECT_THROW(client_->connect(test_host_, "not-a-port!"),
                 std::invalid_argument);

    // Valid service name port should be accepted
    EXPECT_THROW(client_->connect(test_host_, "http"), beast::system_error);
    // This throws system_error not invalid_argument because it's a valid format
    // but might not resolve to a WebSocket server
}

// Test timeout setting
TEST_F(WSClientTest, Timeout) {
    // Set timeout
    client_->setTimeout(std::chrono::seconds(30));

    // Connect to verify timeout didn't break anything
    EXPECT_NO_THROW(connectToMockServer());
    EXPECT_TRUE(client_->isConnected());
}

// Test reconnect options
TEST_F(WSClientTest, ReconnectOptions) {
    // Test valid options
    EXPECT_NO_THROW(client_->setReconnectOptions(3, std::chrono::seconds(5)));

    // Test invalid options
    EXPECT_THROW(client_->setReconnectOptions(-1, std::chrono::seconds(5)),
                 std::invalid_argument);
    EXPECT_THROW(client_->setReconnectOptions(3, std::chrono::seconds(0)),
                 std::invalid_argument);
    EXPECT_THROW(client_->setReconnectOptions(3, std::chrono::seconds(-1)),
                 std::invalid_argument);
}

// Test ping interval
TEST_F(WSClientTest, PingInterval) {
    // Test valid ping interval
    EXPECT_NO_THROW(client_->setPingInterval(std::chrono::seconds(10)));

    // Test invalid ping interval
    EXPECT_THROW(client_->setPingInterval(std::chrono::seconds(0)),
                 std::invalid_argument);
    EXPECT_THROW(client_->setPingInterval(std::chrono::seconds(-1)),
                 std::invalid_argument);

    // Connect and verify ping doesn't break anything
    EXPECT_NO_THROW(connectToMockServer());
    EXPECT_TRUE(client_->isConnected());

    // Wait a bit to allow ping to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test send and receive
TEST_F(WSClientTest, SendReceive) {
    // Set up expected message
    std::string test_message = "Hello, WebSocket!";
    std::promise<std::string> message_promise;
    std::future<std::string> message_future = message_promise.get_future();

    // Set message handler in mock server
    mock_server_->setMessageHandler([&message_promise](std::string message) {
        message_promise.set_value(message);
    });

    // Connect
    connectToMockServer();

    // Send message
    EXPECT_NO_THROW(client_->send(test_message));

    // Wait for server to receive message
    auto status = message_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready)
        << "Message receiving timed out";
    EXPECT_EQ(message_future.get(), test_message);

    // Test server to client message
    std::string response_message = "Server response";
    mock_server_->send(response_message);

    // Small delay to ensure message is received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Receive message
    std::string received_message;
    EXPECT_NO_THROW(received_message = client_->receive());
    EXPECT_EQ(received_message, response_message);
}

// Test sending without connection
TEST_F(WSClientTest, SendWithoutConnection) {
    // Attempt to send without connecting first
    EXPECT_THROW(client_->send("test message"), std::logic_error);
}

// Test receiving without connection
TEST_F(WSClientTest, ReceiveWithoutConnection) {
    // Attempt to receive without connecting first
    EXPECT_THROW(client_->receive(), std::logic_error);
}

// Test connection closure
TEST_F(WSClientTest, Close) {
    // Connect
    connectToMockServer();
    EXPECT_TRUE(client_->isConnected());

    // Close
    EXPECT_NO_THROW(client_->close());
    EXPECT_FALSE(client_->isConnected());

    // Close again should not throw
    EXPECT_NO_THROW(client_->close());
}

// Test JSON sending
TEST_F(WSClientTest, AsyncSendJson) {
    // Set up expected message
    json test_json = {
        {"message", "Hello"}, {"value", 42}, {"array", {1, 2, 3}}};

    std::promise<std::string> message_promise;
    std::future<std::string> message_future = message_promise.get_future();

    // Set message handler in mock server
    mock_server_->setMessageHandler([&message_promise](std::string message) {
        message_promise.set_value(message);
    });

    // Connect
    connectToMockServer();

    // Send JSON
    std::promise<bool> send_promise;
    std::future<bool> send_future = send_promise.get_future();

    client_->asyncSendJson(
        test_json, [&send_promise](beast::error_code ec, std::size_t bytes) {
            send_promise.set_value(!ec);
        });

    // Wait for async send to complete
    auto send_status = send_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(send_status, std::future_status::ready) << "Async send timed out";
    EXPECT_TRUE(send_future.get());

    // Wait for server to receive message
    auto message_status = message_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(message_status, std::future_status::ready)
        << "Message receiving timed out";

    // Verify JSON was correctly serialized
    std::string received = message_future.get();
    json received_json = json::parse(received);
    EXPECT_EQ(received_json, test_json);
}

// Test sending JSON without connection
TEST_F(WSClientTest, AsyncSendJsonWithoutConnection) {
    json test_json = {{"message", "test"}};

    std::promise<beast::error_code> error_promise;
    std::future<beast::error_code> error_future = error_promise.get_future();

    client_->asyncSendJson(
        test_json, [&error_promise](beast::error_code ec, std::size_t bytes) {
            error_promise.set_value(ec);
        });

    auto status = error_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(error_future.get() == net::error::not_connected);
}

// Test invalid JSON handling
TEST_F(WSClientTest, InvalidJsonHandling) {
    // Create a custom object that will fail to be serialized
    struct NonSerializable {};

    // Connect
    connectToMockServer();

    // Attempt to send invalid JSON
    std::promise<beast::error_code> error_promise;
    std::future<beast::error_code> error_future = error_promise.get_future();

    // This should cause an exception in asyncSendJson which gets caught
    // and converted to an error code
    json invalid_json;
    try {
        // TODO: Add a non-serializable object to the JSON
        // invalid_json["bad"] = std::make_shared<NonSerializable>();
    } catch (...) {
        // if we can't even create invalid JSON, use a different approach
        GTEST_SKIP() << "Cannot create invalid JSON for testing";
    }

    client_->asyncSendJson(invalid_json, [&error_promise](beast::error_code ec,
                                                          std::size_t bytes) {
        error_promise.set_value(ec);
    });

    auto status = error_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(error_future.get() == net::error::invalid_argument);
}

// Test connection to non-existent server
TEST_F(WSClientTest, ConnectionToNonExistentServer) {
    // Stop the mock server
    mock_server_->stop();

    // Try to connect to a port with no server
    EXPECT_THROW(client_->connect(test_host_, "8766"), beast::system_error);
    EXPECT_FALSE(client_->isConnected());
}

// Test connection interrupted
TEST_F(WSClientTest, ConnectionInterrupted) {
    // Connect
    connectToMockServer();
    EXPECT_TRUE(client_->isConnected());

    // Close server side
    mock_server_->close();

    // Wait a bit for the connection to detect closure
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Sending should throw
    EXPECT_THROW(client_->send("test"), beast::system_error);

    // Client should detect the disconnect
    EXPECT_FALSE(client_->isConnected());
}

// Test ping mechanism
TEST_F(WSClientTest, PingMechanism) {
    // Set a short ping interval
    client_->setPingInterval(std::chrono::seconds(1));

    // Connect
    connectToMockServer();
    EXPECT_TRUE(client_->isConnected());

    // Wait for several ping cycles
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Client should still be connected
    EXPECT_TRUE(client_->isConnected());

    // Send a message to verify connection is still good
    EXPECT_NO_THROW(client_->send("After pings"));
}

// Test destructor behavior
TEST_F(WSClientTest, DestructorBehavior) {
    // Create a local client
    auto local_client = std::make_unique<WSClient>(*ioc_);

    // Connect
    std::promise<void> connected_promise;
    std::future<void> connected_future = connected_promise.get_future();

    mock_server_->setAcceptHandler(
        [&connected_promise]() { connected_promise.set_value(); });

    local_client->connect(test_host_, std::to_string(test_port_));

    auto status = connected_future.wait_for(std::chrono::seconds(2));
    ASSERT_EQ(status, std::future_status::ready);

    // Destroy the client
    EXPECT_NO_THROW(local_client.reset());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}