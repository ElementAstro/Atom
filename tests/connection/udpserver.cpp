#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "atom/connection/udpserver.hpp"

// Include platform-specific networking headers
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib
using SocketType = SOCKET;
constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>  // For memset
using SocketType = int;
constexpr SocketType INVALID_SOCKET_VALUE = -1;
#endif

namespace atom::connection {

// Mock class for message handler
class MockMessageHandler {
public:
    MOCK_METHOD(void, handle,
                (const std::string& msg, const std::string& ip, int port), ());
};

// Test fixture for UdpSocketHub
class UdpSocketHubTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize networking on Windows
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        ASSERT_EQ(result, 0) << "WSAStartup failed";
#endif
    }

    void TearDown() override {
        // Cleanup networking on Windows
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Helper to send a UDP message using raw sockets
    void sendUdpMessage(const std::string& ip, uint16_t port,
                        const std::string& message) {
        SocketType sender_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        ASSERT_NE(sender_socket, INVALID_SOCKET_VALUE)
            << "Failed to create sender socket";

        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        int pton_result = inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);
        ASSERT_EQ(pton_result, 1) << "Failed to convert IP address: " << ip;

        sendto(sender_socket, message.data(), message.size(), 0,
               reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));

        // Close sender socket
#ifdef _WIN32
        closesocket(sender_socket);
#else
        close(sender_socket);
#endif
    }
};

// Test case: Start and Stop cycle
TEST_F(UdpSocketHubTest, StartStopCycle) {
    UdpSocketHub hub;
    ASSERT_FALSE(hub.isRunning());

    // Start on a valid port
    auto start_res = hub.start(54321);
    ASSERT_TRUE(start_res.has_value())
        << "Start failed with error: "
        << static_cast<int>(start_res.error().error());
    ASSERT_TRUE(hub.isRunning());

    // Stop the hub
    hub.stop();
    ASSERT_FALSE(hub.isRunning());

    // Stopping again should be safe
    hub.stop();
    ASSERT_FALSE(hub.isRunning());
}

// Test case: Start on an invalid port
TEST_F(UdpSocketHubTest, StartInvalidPort) {
    UdpSocketHub hub;
    ASSERT_FALSE(hub.isRunning());

    // Port below minimum
    auto start_res_low = hub.start(1000);
    ASSERT_FALSE(start_res_low.has_value());
    ASSERT_EQ(start_res_low.error(), UdpError::InvalidPort);
    ASSERT_FALSE(hub.isRunning());

    // Port above maximum
    auto start_res_high = hub.start(70000);
    ASSERT_FALSE(start_res_high.has_value());
    ASSERT_EQ(start_res_high.error(), UdpError::InvalidPort);
    ASSERT_FALSE(hub.isRunning());
}

// Test case: isRunning state check
TEST_F(UdpSocketHubTest, IsRunningState) {
    UdpSocketHub hub;
    EXPECT_FALSE(hub.isRunning());

    auto start_res = hub.start(54322);
    ASSERT_TRUE(start_res.has_value());
    EXPECT_TRUE(hub.isRunning());

    hub.stop();
    EXPECT_FALSE(hub.isRunning());
}

// Test case: Add and remove message handlers
TEST_F(UdpSocketHubTest, AddRemoveHandlers) {
    UdpSocketHub hub;
    MockMessageHandler handler1, handler2;

    // Create std::function wrappers
    auto handler_func1 =
        std::bind(&MockMessageHandler::handle, &handler1, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    auto handler_func2 =
        std::bind(&MockMessageHandler::handle, &handler2, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);

    // Add handlers
    hub.addMessageHandler(handler_func1);
    hub.addMessageHandler(handler_func2);

    // Remove handler1
    hub.removeMessageHandler(handler_func1);

    // Note: Due to the implementation using target_type(), removing
    // handler_func1 might remove handler_func2 as well if their underlying
    // types are the same. Using distinct mock objects and std::bind should
    // ideally result in distinct target_types, but this is
    // implementation-dependent. A robust test would involve sending messages
    // and checking which handlers are called. This is covered in the
    // ReceiveMessage tests below.

    // Removing a handler that wasn't added should be safe
    MockMessageHandler handler3;
    auto handler_func3 =
        std::bind(&MockMessageHandler::handle, &handler3, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    hub.removeMessageHandler(handler_func3);  // Should not crash
}

// Test case: Send message when not running
TEST_F(UdpSocketHubTest, SendWhenNotRunning) {
    UdpSocketHub hub;
    ASSERT_FALSE(hub.isRunning());

    auto send_res = hub.sendTo("test message", "127.0.0.1", 54323);
    ASSERT_FALSE(send_res.has_value());
    ASSERT_EQ(send_res.error(), UdpError::NotRunning);
}

// Test case: Send message to invalid address
TEST_F(UdpSocketHubTest, SendInvalidAddress) {
    UdpSocketHub hub;
    auto start_res = hub.start(54323);
    ASSERT_TRUE(start_res.has_value());
    ASSERT_TRUE(hub.isRunning());

    auto send_res = hub.sendTo("test message", "invalid-ip", 54324);
    ASSERT_FALSE(send_res.has_value());
    ASSERT_EQ(send_res.error(), UdpError::InvalidAddress);

    hub.stop();
}

// Test case: Send message to invalid port
TEST_F(UdpSocketHubTest, SendInvalidPort) {
    UdpSocketHub hub;
    auto start_res = hub.start(54324);
    ASSERT_TRUE(start_res.has_value());
    ASSERT_TRUE(hub.isRunning());

    auto send_res_low = hub.sendTo("test message", "127.0.0.1", 1000);
    ASSERT_FALSE(send_res_low.has_value());
    ASSERT_EQ(send_res_low.error(), UdpError::InvalidPort);

    auto send_res_high = hub.sendTo("test message", "127.0.0.1", 70000);
    ASSERT_FALSE(send_res_high.has_value());
    ASSERT_EQ(send_res_high.error(), UdpError::InvalidPort);

    hub.stop();
}

// Test case: Set buffer size
TEST_F(UdpSocketHubTest, SetBufferSize) {
    UdpSocketHub hub;
    // Cannot directly verify the internal buffer size without exposing Impl
    // details. This test primarily checks that the call doesn't crash and
    // handles invalid input.

    hub.setBufferSize(8192);  // Valid size
    // Verification would require sending a message larger than the default but
    // smaller than 8192 and checking if it's received fully, which is complex.

    hub.setBufferSize(0);  // Invalid size - should be ignored
    // Verification would require checking the size didn't change, which is not
    // possible here.
}

// Test case: Receive message and call handler
TEST_F(UdpSocketHubTest, ReceiveMessageAndCallHandler) {
    UdpSocketHub hub;
    MockMessageHandler handler;

    // Use a fixed high port unlikely to be in use for testing
    const uint16_t test_port = 54325;
    const std::string test_ip = "127.0.0.1";
    const std::string test_message = "Hello from test sender!";

    auto start_res = hub.start(test_port);
    ASSERT_TRUE(start_res.has_value())
        << "Failed to start hub: "
        << static_cast<int>(start_res.error().error());
    ASSERT_TRUE(hub.isRunning());

    // Add the mock handler
    auto handler_func =
        std::bind(&MockMessageHandler::handle, &handler, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    hub.addMessageHandler(handler_func);

    // Set expectation on the mock handler
    // The sender IP will be 127.0.0.1, the port will be ephemeral (use An<int>
    // matcher)
    EXPECT_CALL(handler, handle(test_message, test_ip, testing::An<int>()))
        .Times(1);

    // Send a message to the hub's address and port
    sendUdpMessage(test_ip, test_port, test_message);

    // Give the receiver thread time to process the message and call the handler
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Expectations are checked automatically when the mock object goes out of
    // scope or at the end of the test case.

    hub.stop();
    ASSERT_FALSE(hub.isRunning());
}

// Test case: Remove handler prevents it from being called
TEST_F(UdpSocketHubTest, RemoveHandlerPreventsCall) {
    UdpSocketHub hub;
    MockMessageHandler handler1, handler2;

    const uint16_t test_port = 54326;
    const std::string test_ip = "127.0.0.1";
    const std::string test_message = "Message for handler2 only!";

    auto start_res = hub.start(test_port);
    ASSERT_TRUE(start_res.has_value())
        << "Failed to start hub: "
        << static_cast<int>(start_res.error().error());
    ASSERT_TRUE(hub.isRunning());

    // Add both handlers
    auto handler_func1 =
        std::bind(&MockMessageHandler::handle, &handler1, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    auto handler_func2 =
        std::bind(&MockMessageHandler::handle, &handler2, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    hub.addMessageHandler(handler_func1);
    hub.addMessageHandler(handler_func2);

    // Remove handler1
    hub.removeMessageHandler(handler_func1);

    // Set expectations: handler1 should NOT be called, handler2 SHOULD be
    // called
    EXPECT_CALL(handler1, handle(testing::_, testing::_, testing::_))
        .Times(0);  // handler1 should not be called
    EXPECT_CALL(handler2, handle(test_message, test_ip, testing::An<int>()))
        .Times(1);  // handler2 should be called

    // Send a message
    sendUdpMessage(test_ip, test_port, test_message);

    // Give the receiver thread time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    hub.stop();
    ASSERT_FALSE(hub.isRunning());
}

// Test case: Multiple handlers are called
TEST_F(UdpSocketHubTest, MultipleHandlersCalled) {
    UdpSocketHub hub;
    MockMessageHandler handler1, handler2;

    const uint16_t test_port = 54327;
    const std::string test_ip = "127.0.0.1";
    const std::string test_message = "Message for everyone!";

    auto start_res = hub.start(test_port);
    ASSERT_TRUE(start_res.has_value())
        << "Failed to start hub: "
        << static_cast<int>(start_res.error().error());
    ASSERT_TRUE(hub.isRunning());

    // Add both handlers
    auto handler_func1 =
        std::bind(&MockMessageHandler::handle, &handler1, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    auto handler_func2 =
        std::bind(&MockMessageHandler::handle, &handler2, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    hub.addMessageHandler(handler_func1);
    hub.addMessageHandler(handler_func2);

    // Set expectations: both handlers should be called
    EXPECT_CALL(handler1, handle(test_message, test_ip, testing::An<int>()))
        .Times(1);
    EXPECT_CALL(handler2, handle(test_message, test_ip, testing::An<int>()))
        .Times(1);

    // Send a message
    sendUdpMessage(test_ip, test_port, test_message);

    // Give the receiver thread time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    hub.stop();
    ASSERT_FALSE(hub.isRunning());
}

}  // namespace atom::connection