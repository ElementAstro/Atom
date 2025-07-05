#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <span>
#include <vector>


#include "protocol.hpp"
#include "types.hpp"

using namespace mqtt;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

// Mock ITransport for testing
class MockTransport : public ITransport {
public:
    MOCK_METHOD(void, async_connect,
                (const std::string&, uint16_t, std::function<void(ErrorCode)>),
                (override));
    MOCK_METHOD(void, async_write,
                (std::span<const uint8_t>,
                 std::function<void(ErrorCode, size_t)>),
                (override));
    MOCK_METHOD(void, async_read,
                (std::span<uint8_t>, std::function<void(ErrorCode, size_t)>),
                (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(bool, is_open, (), (const, override));
};

TEST(ConnectionStateTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(ConnectionState::DISCONNECTED), 0);
    EXPECT_EQ(static_cast<int>(ConnectionState::CONNECTING), 1);
    EXPECT_EQ(static_cast<int>(ConnectionState::CONNECTED), 2);
    EXPECT_EQ(static_cast<int>(ConnectionState::DISCONNECTING), 3);
}

TEST(PendingOperationTest, ConstructionAndFields) {
    Message msg;
    msg.topic = "test/topic";
    auto now = std::chrono::steady_clock::now();
    uint8_t retries = 2;
    bool callback_called = false;
    PendingOperation op{msg, now, retries,
                        [&](ErrorCode ec) { callback_called = true; }};

    EXPECT_EQ(op.message.topic, "test/topic");
    EXPECT_EQ(op.retry_count, retries);
    op.callback(ErrorCode::SUCCESS);
    EXPECT_TRUE(callback_called);
}

TEST(TCPTransportTest, ConstructionAndIsOpen) {
    asio::io_context io;
    TCPTransport transport(io);
    // Socket is not open by default
    EXPECT_FALSE(transport.is_open());
}

TEST(TCPTransportTest, AsyncConnectFailure) {
    asio::io_context io;
    TCPTransport transport(io);

    // Use an invalid host to force failure
    bool called = false;
    transport.async_connect("invalid_host", 65535, [&](ErrorCode ec) {
        EXPECT_EQ(ec, ErrorCode::SERVER_UNAVAILABLE);
        called = true;
    });

    // Run the io_context to process the async operation
    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(called);
}

TEST(TCPTransportTest, AsyncWriteAndRead) {
    asio::io_context io;
    TCPTransport transport(io);

    // Open a local acceptor to connect to
    asio::ip::tcp::acceptor acceptor(
        io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    uint16_t port = acceptor.local_endpoint().port();

    bool connect_called = false;
    transport.async_connect("127.0.0.1", port, [&](ErrorCode ec) {
        EXPECT_EQ(ec, ErrorCode::SUCCESS);
        connect_called = true;
    });

    // Accept the connection
    asio::ip::tcp::socket server_socket(io);
    acceptor.async_accept(server_socket, [](const std::error_code&) {});

    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(connect_called);

    // Write test
    std::vector<uint8_t> data = {1, 2, 3, 4};
    bool write_called = false;
    transport.async_write(data, [&](ErrorCode ec, size_t bytes) {
        EXPECT_EQ(ec, ErrorCode::SUCCESS);
        EXPECT_EQ(bytes, data.size());
        write_called = true;
    });

    // Read on server side
    std::vector<uint8_t> server_buf(4);
    server_socket.async_read_some(asio::buffer(server_buf),
                                  [](const std::error_code&, size_t) {});

    io.restart();
    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(write_called);

    // Read test
    // Write from server to client
    std::vector<uint8_t> send_buf = {5, 6, 7, 8};
    asio::async_write(server_socket, asio::buffer(send_buf),
                      [](const std::error_code&, size_t) {});

    std::array<uint8_t, 4> client_buf{};
    bool read_called = false;
    transport.async_read(client_buf, [&](ErrorCode ec, size_t bytes) {
        EXPECT_EQ(ec, ErrorCode::SUCCESS);
        EXPECT_EQ(bytes, send_buf.size());
        read_called = true;
        EXPECT_EQ(std::vector<uint8_t>(client_buf.begin(), client_buf.end()),
                  send_buf);
    });

    io.restart();
    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(read_called);

    transport.close();
    EXPECT_FALSE(transport.is_open());
}

TEST(TLSTransportTest, ConstructionAndIsOpen) {
    asio::io_context io;
    asio::ssl::context ssl_ctx(asio::ssl::context::sslv23);
    TLSTransport transport(io, ssl_ctx);
    EXPECT_FALSE(transport.is_open());
}

// Note: Full async_connect, async_write, async_read tests for TLSTransport
// would require SSL certificates and a running SSL server, which is out of
// scope for a simple unit test. Instead, we check that the methods can be
// called and invoke the callback with an error.

TEST(TLSTransportTest, AsyncConnectFailure) {
    asio::io_context io;
    asio::ssl::context ssl_ctx(asio::ssl::context::sslv23);
    TLSTransport transport(io, ssl_ctx);

    bool called = false;
    transport.async_connect("invalid_host", 65535, [&](ErrorCode ec) {
        EXPECT_EQ(ec, ErrorCode::SERVER_UNAVAILABLE);
        called = true;
    });

    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(called);
}

TEST(TLSTransportTest, AsyncWriteAndReadError) {
    asio::io_context io;
    asio::ssl::context ssl_ctx(asio::ssl::context::sslv23);
    TLSTransport transport(io, ssl_ctx);

    // Not connected, so write/read should fail
    std::vector<uint8_t> data = {1, 2, 3, 4};
    bool write_called = false;
    transport.async_write(data, [&](ErrorCode ec, size_t bytes) {
        EXPECT_EQ(ec, ErrorCode::UNSPECIFIED_ERROR);
        EXPECT_EQ(bytes, 0);
        write_called = true;
    });

    std::array<uint8_t, 4> buf{};
    bool read_called = false;
    transport.async_read(buf, [&](ErrorCode ec, size_t bytes) {
        EXPECT_EQ(ec, ErrorCode::UNSPECIFIED_ERROR);
        EXPECT_EQ(bytes, 0);
        read_called = true;
    });

    io.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(write_called);
    EXPECT_TRUE(read_called);

    transport.close();
    EXPECT_FALSE(transport.is_open());
}
