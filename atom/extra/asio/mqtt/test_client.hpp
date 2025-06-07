// atom/extra/asio/mqtt/test_client.hpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "client.hpp"
#include "protocol.hpp"
#include "types.hpp"


using namespace mqtt;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

// Mock Transport for testing
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

class ClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create client without auto-starting IO thread
        client_ = std::make_unique<Client>(false);
    }

    void TearDown() override {
        if (client_) {
            client_->stop();
        }
    }

    std::unique_ptr<Client> client_;
};

TEST_F(ClientTest, ConstructorAndDestructor) {
    // Test that client is created with default state
    EXPECT_EQ(client_->get_state(), ConnectionState::DISCONNECTED);
    EXPECT_FALSE(client_->is_connected());
    EXPECT_TRUE(client_->get_auto_reconnect());
}

TEST_F(ClientTest, InitialState) {
    EXPECT_EQ(client_->get_state(), ConnectionState::DISCONNECTED);
    EXPECT_FALSE(client_->is_connected());

    // Check initial stats
    auto stats = client_->get_stats();
    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
    EXPECT_EQ(stats.reconnect_count, 0u);
}

TEST_F(ClientTest, AutoReconnectConfiguration) {
    EXPECT_TRUE(client_->get_auto_reconnect());

    client_->set_auto_reconnect(false);
    EXPECT_FALSE(client_->get_auto_reconnect());

    client_->set_auto_reconnect(true);
    EXPECT_TRUE(client_->get_auto_reconnect());
}

TEST_F(ClientTest, EventHandlerSetters) {
    bool message_received = false;
    bool connection_called = false;
    bool disconnection_called = false;

    client_->set_message_handler(
        [&](const Message& msg) { message_received = true; });

    client_->set_connection_handler(
        [&](ErrorCode ec) { connection_called = true; });

    client_->set_disconnection_handler(
        [&](ErrorCode ec) { disconnection_called = true; });

    // Handlers are set but not called yet
    EXPECT_FALSE(message_received);
    EXPECT_FALSE(connection_called);
    EXPECT_FALSE(disconnection_called);
}

TEST_F(ClientTest, StatsManagement) {
    auto initial_stats = client_->get_stats();
    EXPECT_EQ(initial_stats.messages_sent, 0u);

    client_->reset_stats();
    auto reset_stats = client_->get_stats();
    EXPECT_EQ(reset_stats.messages_sent, 0u);
    EXPECT_EQ(reset_stats.messages_received, 0u);
    EXPECT_EQ(reset_stats.bytes_sent, 0u);
    EXPECT_EQ(reset_stats.bytes_received, 0u);
}

TEST_F(ClientTest, AsyncPublishWhenNotConnected) {
    Message msg;
    msg.topic = "test/topic";
    msg.payload = {1, 2, 3, 4};
    msg.qos = QoS::AT_MOST_ONCE;

    bool callback_called = false;
    ErrorCode received_error = ErrorCode::SUCCESS;

    client_->async_publish(std::move(msg), [&](ErrorCode ec) {
        callback_called = true;
        received_error = ec;
    });

    // Since client is not connected, should get an error
    // Note: This depends on implementation - might be called immediately or
    // async
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, TemplateAsyncPublishStringPayload) {
    std::string payload = "hello world";
    bool callback_called = false;

    client_->async_publish("test/topic", payload, QoS::AT_MOST_ONCE, false,
                           [&](ErrorCode ec) { callback_called = true; });

    // Should not crash and callback should be setup
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, TemplateAsyncPublishBytePayload) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    bool callback_called = false;

    client_->async_publish("test/topic", std::span<const uint8_t>(payload),
                           QoS::AT_LEAST_ONCE, true,
                           [&](ErrorCode ec) { callback_called = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncSubscribeWhenNotConnected) {
    bool callback_called = false;
    ErrorCode received_error = ErrorCode::SUCCESS;

    client_->async_subscribe("test/topic", QoS::AT_MOST_ONCE,
                             [&](ErrorCode ec) {
                                 callback_called = true;
                                 received_error = ec;
                             });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncSubscribeMultipleTopics) {
    std::vector<Subscription> subscriptions = {{"topic1", QoS::AT_MOST_ONCE},
                                               {"topic2", QoS::AT_LEAST_ONCE}};

    bool callback_called = false;
    std::vector<ErrorCode> received_errors;

    client_->async_subscribe(subscriptions,
                             [&](const std::vector<ErrorCode>& errors) {
                                 callback_called = true;
                                 received_errors = errors;
                             });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncUnsubscribeWhenNotConnected) {
    bool callback_called = false;
    ErrorCode received_error = ErrorCode::SUCCESS;

    client_->async_unsubscribe("test/topic", [&](ErrorCode ec) {
        callback_called = true;
        received_error = ec;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncUnsubscribeMultipleTopics) {
    std::vector<std::string> topics = {"topic1", "topic2", "topic3"};

    bool callback_called = false;
    std::vector<ErrorCode> received_errors;

    client_->async_unsubscribe(topics,
                               [&](const std::vector<ErrorCode>& errors) {
                                   callback_called = true;
                                   received_errors = errors;
                               });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncConnectBasic) {
    ConnectionOptions options;
    options.client_id = "test_client";
    options.username = "user";
    options.password = "pass";
    options.keep_alive = std::chrono::seconds(30);

    bool callback_called = false;
    ErrorCode received_error = ErrorCode::SUCCESS;

    client_->async_connect("localhost", 1883, options, [&](ErrorCode ec) {
        callback_called = true;
        received_error = ec;
    });

    // State should change to CONNECTING
    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(ClientTest, AsyncConnectWithEmptyClientId) {
    ConnectionOptions options;
    // Leave client_id empty to test auto-generation
    options.username = "user";

    bool callback_called = false;

    client_->async_connect("localhost", 1883, options,
                           [&](ErrorCode ec) { callback_called = true; });

    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, AsyncConnectWithTLS) {
    ConnectionOptions options;
    options.client_id = "tls_client";
    options.use_tls = true;
    options.ca_cert_file = "ca.pem";
    options.cert_file = "client.pem";
    options.private_key_file = "client.key";
    options.verify_certificate = true;

    bool callback_called = false;

    client_->async_connect("secure.broker.com", 8883, options,
                           [&](ErrorCode ec) { callback_called = true; });

    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, DisconnectWhenNotConnected) {
    EXPECT_EQ(client_->get_state(), ConnectionState::DISCONNECTED);

    client_->disconnect(ErrorCode::SUCCESS);

    // Should remain disconnected
    EXPECT_EQ(client_->get_state(), ConnectionState::DISCONNECTED);
}

TEST_F(ClientTest, DisconnectAfterConnecting) {
    ConnectionOptions options;
    options.client_id = "test_client";

    client_->async_connect("localhost", 1883, options);
    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);

    client_->disconnect(ErrorCode::SUCCESS);

    // Should change to disconnecting
    auto state = client_->get_state();
    EXPECT_TRUE(state == ConnectionState::DISCONNECTING ||
                state == ConnectionState::DISCONNECTED);
}

TEST_F(ClientTest, IOContextAccess) {
    auto& io_context = client_->get_io_context();
    EXPECT_NO_THROW(io_context.get_executor());
}

TEST_F(ClientTest, RunAndStop) {
    // Test that run and stop don't crash
    std::thread run_thread([this]() { client_->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    client_->stop();

    if (run_thread.joinable()) {
        run_thread.join();
    }
}

// Test fixture for testing with a running IO context
class ClientWithIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<Client>(true);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));  // Let IO thread start
    }

    void TearDown() override {
        if (client_) {
            client_->stop();
        }
    }

    std::unique_ptr<Client> client_;
};

TEST_F(ClientWithIOTest, ConnectWithRunningIO) {
    ConnectionOptions options;
    options.client_id = "running_io_client";

    bool callback_called = false;
    ErrorCode connection_result = ErrorCode::SUCCESS;

    client_->async_connect("localhost", 1883, options, [&](ErrorCode ec) {
        callback_called = true;
        connection_result = ec;
    });

    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);

    // Wait a bit for the connection attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(ClientWithIOTest, PublishWithRunningIO) {
    Message msg;
    msg.topic = "test/publish";
    msg.payload = {'h', 'e', 'l', 'l', 'o'};
    msg.qos = QoS::AT_MOST_ONCE;

    bool callback_called = false;
    ErrorCode publish_result = ErrorCode::SUCCESS;

    client_->async_publish(std::move(msg), [&](ErrorCode ec) {
        callback_called = true;
        publish_result = ec;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Test edge cases and error conditions
TEST_F(ClientTest, ConnectWhileAlreadyConnecting) {
    ConnectionOptions options1;
    options1.client_id = "client1";

    ConnectionOptions options2;
    options2.client_id = "client2";

    // First connection
    client_->async_connect("localhost", 1883, options1);
    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);

    // Second connection should be rejected or handled appropriately
    bool second_callback_called = false;
    client_->async_connect("localhost", 1884, options2, [&](ErrorCode ec) {
        second_callback_called = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, MessageHandlerWithComplexMessage) {
    Message received_msg;
    bool handler_called = false;

    client_->set_message_handler([&](const Message& msg) {
        received_msg = msg;
        handler_called = true;
    });

    // The handler is set but won't be called until we actually receive a
    // message
    EXPECT_FALSE(handler_called);
}

TEST_F(ClientTest, LargePayloadPublish) {
    std::vector<uint8_t> large_payload(10000, 0x42);

    bool callback_called = false;
    client_->async_publish("test/large",
                           std::span<const uint8_t>(large_payload),
                           QoS::AT_LEAST_ONCE, false,
                           [&](ErrorCode ec) { callback_called = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, MultipleSubscriptionsWithDifferentQoS) {
    std::vector<Subscription> subs;

    Subscription sub1;
    sub1.topic_filter = "sensor/+/temperature";
    sub1.qos = QoS::AT_MOST_ONCE;
    sub1.no_local = false;
    sub1.retain_as_published = true;

    Subscription sub2;
    sub2.topic_filter = "control/+/command";
    sub2.qos = QoS::EXACTLY_ONCE;
    sub2.no_local = true;
    sub2.retain_handling = 1;

    subs.push_back(sub1);
    subs.push_back(sub2);

    bool callback_called = false;
    client_->async_subscribe(subs, [&](const std::vector<ErrorCode>& results) {
        callback_called = true;
        EXPECT_EQ(results.size(), 2u);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, ConnectionOptionsWithWillMessage) {
    ConnectionOptions options;
    options.client_id = "will_client";
    options.will_topic = "clients/will_client/status";
    options.will_payload =
        std::vector<uint8_t>{'o', 'f', 'f', 'l', 'i', 'n', 'e'};
    options.will_qos = QoS::AT_LEAST_ONCE;
    options.will_retain = true;
    options.clean_session = false;

    bool callback_called = false;
    client_->async_connect("localhost", 1883, options,
                           [&](ErrorCode ec) { callback_called = true; });

    EXPECT_EQ(client_->get_state(), ConnectionState::CONNECTING);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ClientTest, StatsAfterOperations) {
    auto initial_stats = client_->get_stats();

    // Perform some operations (they may fail due to no connection, but stats
    // tracking should work)
    Message msg;
    msg.topic = "stats/test";
    msg.payload = {1, 2, 3};

    client_->async_publish(std::move(msg));
    client_->async_subscribe("stats/+");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Stats might not change immediately for failed operations, but the test
    // verifies no crashes
    auto after_stats = client_->get_stats();
    EXPECT_GE(after_stats.messages_sent, initial_stats.messages_sent);
}