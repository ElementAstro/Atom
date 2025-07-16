#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include "atom/macro.hpp"

#include "atom/connection/sockethub.hpp"

namespace atom::connection::test {

// Helper to find an available port
int find_available_port() {
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(),
                                     0);  // Port 0 means OS assigns a free port
    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    return acceptor.local_endpoint().port();
}

// Test fixture for SocketHub
class SocketHubTest : public ::testing::Test {
protected:
    SocketHub hub_;
    int port_;

    void SetUp() override { port_ = find_available_port(); }

    void TearDown() override {
        hub_.stop();
        // Give some time for threads to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper to create a client connection and send/receive data
    std::string create_client_and_send_recv(
        const std::string& message_to_send, std::string& received_message,
        int client_port) {  // Removed unused 'timeout' parameter
        asio::io_context io_context;
        asio::ip::tcp::socket socket(io_context);
        asio::error_code ec;

        ATOM_UNUSED_RESULT(socket.connect(  // Explicitly cast to void to ignore
                                            // nodiscard warning
            asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"),
                                    client_port),
            ec));
        if (ec) {
            return "Connect error: " + ec.message();
        }

        // Send message
        asio::write(socket, asio::buffer(message_to_send), ec);
        if (ec) {
            return "Write error: " + ec.message();
        }

        // Read response (if any)
        std::vector<char> buffer(1024);
        size_t bytes_read = socket.read_some(asio::buffer(buffer), ec);
        if (!ec || ec == asio::error::eof) {
            received_message = std::string(buffer.data(), bytes_read);
        } else {
            return "Read error: " + ec.message();
        }

        socket.close();
        return "";  // Success
    }

    // Helper to create a client connection and keep it open
    std::unique_ptr<asio::ip::tcp::socket> create_persistent_client(
        int client_port) {
        asio::io_context io_context;  // Each client needs its own io_context
                                      // for async operations
        auto socket = std::make_unique<asio::ip::tcp::socket>(io_context);
        asio::error_code ec;
        ATOM_UNUSED_RESULT(socket->connect(  // Explicitly cast to void to
                                             // ignore nodiscard warning
            asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"),
                                    client_port),
            ec));
        if (ec) {
            return nullptr;
        }
        return socket;
    }
};

// Mock classes for handlers
class MockMessageHandler {
public:
    MOCK_METHOD(void, handle, (std::string_view msg), ());
};

class MockConnectHandler {
public:
    MOCK_METHOD(void, handle, (int clientId, std::string_view clientAddr), ());
};

class MockDisconnectHandler {
public:
    MOCK_METHOD(void, handle, (int clientId, std::string_view clientAddr), ());
};

// Test Cases

// Constructor and Destructor
TEST_F(SocketHubTest, ConstructorDestructor) {
    // Test fixture already creates and destroys a SocketHub
    EXPECT_FALSE(hub_.isRunning());
    EXPECT_EQ(hub_.getClientCount(), 0);
}

// Start/Stop
TEST_F(SocketHubTest, StartAndStop) {
    EXPECT_FALSE(hub_.isRunning());
    hub_.start(port_);
    EXPECT_TRUE(hub_.isRunning());
    EXPECT_EQ(hub_.getPort(), port_);

    hub_.stop();
    EXPECT_FALSE(hub_.isRunning());
    EXPECT_EQ(hub_.getPort(), 0);  // Port should be reset after stop
}

TEST_F(SocketHubTest, StartInvalidPortThrows) {
    EXPECT_THROW(hub_.start(0), std::invalid_argument);
    EXPECT_THROW(hub_.start(65536), std::invalid_argument);
}

TEST_F(SocketHubTest, StartAlreadyRunningWarns) {
    hub_.start(port_);
    EXPECT_TRUE(hub_.isRunning());
    // This should not throw, but spdlog will log a warning.
    // We can't easily test spdlog output here, so just check no throw.
    EXPECT_NO_THROW(hub_.start(port_));
    EXPECT_TRUE(hub_.isRunning());
}

TEST_F(SocketHubTest, StopWhenNotRunning) {
    EXPECT_FALSE(hub_.isRunning());
    EXPECT_NO_THROW(hub_.stop());  // Should not throw
}

// Handlers
TEST_F(SocketHubTest, AddMessageHandler) {
    MockMessageHandler mock_handler;
    hub_.addHandler(std::bind(&MockMessageHandler::handle, &mock_handler,
                              std::placeholders::_1));
    hub_.start(port_);

    std::string sent_msg = "Hello from client!";
    std::string received_msg_from_server;

    EXPECT_CALL(mock_handler, handle(std::string_view(sent_msg))).Times(1);

    std::string client_error =
        create_client_and_send_recv(sent_msg, received_msg_from_server, port_);
    EXPECT_TRUE(client_error.empty()) << client_error;

    // Give some time for the handler to be called
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(SocketHubTest, AddConnectHandler) {
    MockConnectHandler mock_handler;
    hub_.addConnectHandler(std::bind(&MockConnectHandler::handle, &mock_handler,
                                     std::placeholders::_1,
                                     std::placeholders::_2));
    hub_.start(port_);

    EXPECT_CALL(mock_handler,
                handle(testing::An<int>(), testing::HasSubstr("127.0.0.1")))
        .Times(1);

    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    asio::error_code ec;
    ATOM_UNUSED_RESULT(
        socket.connect(asio::ip::tcp::endpoint(  // Explicitly cast to void
                           asio::ip::address::from_string("127.0.0.1"), port_),
                       ec));
    EXPECT_FALSE(ec) << ec.message();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time for handler
    socket.close();
}

TEST_F(SocketHubTest, AddDisconnectHandler) {
    MockDisconnectHandler mock_handler;
    hub_.addDisconnectHandler(std::bind(&MockDisconnectHandler::handle,
                                        &mock_handler, std::placeholders::_1,
                                        std::placeholders::_2));
    hub_.start(port_);

    EXPECT_CALL(mock_handler,
                handle(testing::An<int>(), testing::HasSubstr("127.0.0.1")))
        .Times(1);

    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    asio::error_code ec;
    ATOM_UNUSED_RESULT(
        socket.connect(asio::ip::tcp::endpoint(  // Explicitly cast to void
                           asio::ip::address::from_string("127.0.0.1"), port_),
                       ec));
    EXPECT_FALSE(ec) << ec.message();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Give time for connect
    socket.close();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time for disconnect
}

// Client Management
TEST_F(SocketHubTest, GetClientCount) {
    hub_.start(port_);
    EXPECT_EQ(hub_.getClientCount(), 0);

    asio::io_context io_context1;
    asio::ip::tcp::socket socket1(io_context1);
    socket1.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hub_.getClientCount(), 1);

    asio::io_context io_context2;
    asio::ip::tcp::socket socket2(io_context2);
    socket2.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(hub_.getClientCount(), 2);

    socket1.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(hub_.getClientCount(), 1);

    socket2.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(hub_.getClientCount(), 0);
}

TEST_F(SocketHubTest, GetConnectedClients) {
    hub_.start(port_);
    EXPECT_TRUE(hub_.getConnectedClients().empty());

    asio::io_context io_context1;
    asio::ip::tcp::socket socket1(io_context1);
    socket1.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<ClientInfo> clients = hub_.getConnectedClients();
    EXPECT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0].id, 1);  // First client gets ID 1
    EXPECT_THAT(clients[0].address, testing::HasSubstr("127.0.0.1"));
    EXPECT_GT(clients[0].connectedTime.time_since_epoch().count(), 0);
    EXPECT_EQ(clients[0].bytesReceived, 0);
    EXPECT_EQ(clients[0].bytesSent, 0);

    socket1.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(hub_.getConnectedClients().empty());
}

// Messaging
TEST_F(SocketHubTest, BroadcastMessage) {
    hub_.start(port_);

    // Connect two clients
    asio::io_context io_context1;
    asio::ip::tcp::socket socket1(io_context1);
    socket1.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));

    asio::io_context io_context2;
    asio::ip::tcp::socket socket2(io_context2);
    socket2.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow connections to establish

    std::string broadcast_msg = "Broadcast Test";
    size_t sent_count = hub_.broadcast(broadcast_msg);
    EXPECT_EQ(sent_count, 2);

    // Read from clients
    std::vector<char> buffer1(1024), buffer2(1024);
    asio::error_code ec1, ec2;

    size_t bytes_read1 = socket1.read_some(asio::buffer(buffer1), ec1);
    size_t bytes_read2 = socket2.read_some(asio::buffer(buffer2), ec2);

    EXPECT_FALSE(ec1);
    EXPECT_FALSE(ec2);
    EXPECT_EQ(std::string(buffer1.data(), bytes_read1), broadcast_msg);
    EXPECT_EQ(std::string(buffer2.data(), bytes_read2), broadcast_msg);

    socket1.close();
    socket2.close();
}

TEST_F(SocketHubTest, BroadcastEmptyMessageReturnsZero) {
    hub_.start(port_);
    size_t sent_count = hub_.broadcast("");
    EXPECT_EQ(sent_count, 0);
}

TEST_F(SocketHubTest, BroadcastWhenNotRunningReturnsZero) {
    size_t sent_count = hub_.broadcast("Test");
    EXPECT_EQ(sent_count, 0);
}

TEST_F(SocketHubTest, SendToSpecificClient) {
    hub_.start(port_);

    // Connect two clients
    asio::io_context io_context1;
    asio::ip::tcp::socket socket1(io_context1);
    socket1.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));

    asio::io_context io_context2;
    asio::ip::tcp::socket socket2(io_context2);
    socket2.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow connections to establish

    std::vector<ClientInfo> clients = hub_.getConnectedClients();
    ASSERT_EQ(clients.size(), 2);

    int client1_id = clients[0].id;
    int client2_id = clients[1].id;

    std::string msg_to_client1 = "Message for Client 1";
    bool sent_to_1 = hub_.sendTo(client1_id, msg_to_client1);
    EXPECT_TRUE(sent_to_1);

    std::string msg_to_client2 = "Message for Client 2";
    bool sent_to_2 = hub_.sendTo(client2_id, msg_to_client2);
    EXPECT_TRUE(sent_to_2);

    // Read from clients
    std::vector<char> buffer1(1024), buffer2(1024);
    asio::error_code ec1, ec2;

    size_t bytes_read1 = socket1.read_some(asio::buffer(buffer1), ec1);
    size_t bytes_read2 = socket2.read_some(asio::buffer(buffer2), ec2);

    EXPECT_FALSE(ec1);
    EXPECT_FALSE(ec2);
    EXPECT_EQ(std::string(buffer1.data(), bytes_read1), msg_to_client1);
    EXPECT_EQ(std::string(buffer2.data(), bytes_read2), msg_to_client2);

    socket1.close();
    socket2.close();
}

TEST_F(SocketHubTest, SendToNonExistentClientReturnsFalse) {
    hub_.start(port_);
    bool sent = hub_.sendTo(999, "Test");
    EXPECT_FALSE(sent);
}

TEST_F(SocketHubTest, SendToDisconnectedClientReturnsFalse) {
    hub_.start(port_);
    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    socket.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<ClientInfo> clients = hub_.getConnectedClients();
    ASSERT_EQ(clients.size(), 1);
    int client_id = clients[0].id;

    socket.close();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow disconnect to process

    bool sent = hub_.sendTo(client_id, "Test");
    EXPECT_FALSE(sent);
}

TEST_F(SocketHubTest, SendToEmptyMessageReturnsFalse) {
    hub_.start(port_);
    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    socket.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<ClientInfo> clients = hub_.getConnectedClients();
    ASSERT_EQ(clients.size(), 1);
    int client_id = clients[0].id;

    bool sent = hub_.sendTo(client_id, "");
    EXPECT_FALSE(sent);
    socket.close();
}

TEST_F(SocketHubTest, SendToWhenNotRunningReturnsFalse) {
    bool sent = hub_.sendTo(1, "Test");
    EXPECT_FALSE(sent);
}

// Timeout
TEST_F(SocketHubTest, ClientTimeout) {
    hub_.setClientTimeout(std::chrono::seconds(1));  // 1 second timeout
    hub_.start(port_);

    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    socket.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Allow connection

    EXPECT_EQ(hub_.getClientCount(), 1);

    // Wait for timeout to occur
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_EQ(hub_.getClientCount(), 0);
    socket.close();  // Ensure client socket is closed
}

TEST_F(SocketHubTest, ClientTimeoutDisabled) {
    hub_.setClientTimeout(std::chrono::seconds(0));  // Disable timeout
    hub_.start(port_);

    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    socket.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Allow connection

    EXPECT_EQ(hub_.getClientCount(), 1);

    // Wait longer than a typical timeout
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_EQ(hub_.getClientCount(), 1);  // Client should still be connected
    socket.close();
}

TEST_F(SocketHubTest, ClientActivityResetsTimeout) {
    hub_.setClientTimeout(std::chrono::seconds(1));  // 1 second timeout
    hub_.start(port_);

    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    socket.connect(asio::ip::tcp::endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Allow connection

    EXPECT_EQ(hub_.getClientCount(), 1);

    // Send message every 500ms, should keep connection alive
    for (int i = 0; i < 3; ++i) {
        asio::error_code ec;
        asio::write(socket, asio::buffer("ping"), ec);
        EXPECT_FALSE(ec);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    EXPECT_EQ(hub_.getClientCount(), 1);  // Client should still be connected
    socket.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(hub_.getClientCount(), 0);
}

// Move operations
TEST_F(SocketHubTest, MoveConstructor) {
    hub_.start(port_);
    size_t broadcast_count = hub_.broadcast("Test");  // Assign to variable
    (void)
        broadcast_count;  // Suppress unused variable warning if not used later

    SocketHub moved_hub = std::move(hub_);  // Move construct

    // Original hub_ is now in a valid but unspecified state.
    // We can't make strong assertions about hub_ after move.
    // But moved_hub should have the state.
    EXPECT_TRUE(moved_hub.isRunning());
    EXPECT_EQ(moved_hub.getPort(), port_);
    EXPECT_EQ(moved_hub.getClientCount(), 0);  // No clients connected yet
}

TEST_F(SocketHubTest, MoveAssignment) {
    hub_.start(port_);
    size_t broadcast_count_orig = hub_.broadcast("Test");  // Assign to variable
    (void)broadcast_count_orig;  // Suppress unused variable warning

    int other_port = find_available_port();
    SocketHub other_hub;
    other_hub.start(other_port);
    size_t broadcast_count_other =
        other_hub.broadcast("Other Test");  // Assign to variable
    (void)broadcast_count_other;            // Suppress unused variable warning

    other_hub = std::move(hub_);  // Move assign

    // other_hub should now have the state of the original hub_
    EXPECT_TRUE(other_hub.isRunning());
    EXPECT_EQ(other_hub.getPort(), port_);
    EXPECT_EQ(other_hub.getClientCount(), 0);

    // The hub_ (moved-from) should be stopped and in a default state
    // (implicitly handled by unique_ptr reset and destructor)
}

}  // namespace atom::connection::test
