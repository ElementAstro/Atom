#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "atom/extra/asio/mqtt/client.hpp"
#include "atom/extra/asio/mqtt/types.hpp"
#include "atom/extra/asio/asio_compatibility.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <future>
#include <atomic>
#include <condition_variable>

using namespace testing;
using namespace mqtt;

namespace atom::extra::asio::test {

// Mock MQTT broker for testing
class MockMqttBroker {
public:
    MockMqttBroker(uint16_t port = 1883) : port_(port), acceptor_(ioc_) {}

    void start() {
        running_ = true;

        // Setup acceptor
        ::asio::ip::tcp::endpoint endpoint(::asio::ip::tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        server_thread_ = std::thread([this]() {
            try {
                while (running_) {
                    auto socket = std::make_shared<::asio::ip::tcp::socket>(ioc_);
                    acceptor_.async_accept(*socket,
                        [this, socket](std::error_code ec) {
                            if (!ec && running_) {
                                handleClient(socket);
                            }
                        });

                    ioc_.run_one();
                    if (!running_) break;
                }
            } catch (const std::exception& e) {
                // Ignore exceptions during shutdown
            }
        });
    }

    void stop() {
        running_ = false;
        acceptor_.close();
        ioc_.stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    uint16_t port() const { return port_; }

    void setConnectResponse(bool accept) { accept_connections_ = accept; }
    void setPublishResponse(bool accept) { accept_publishes_ = accept; }

private:
    void handleClient(std::shared_ptr<::asio::ip::tcp::socket> socket) {
        // Simple mock broker behavior
        if (accept_connections_) {
            // Send CONNACK
            std::vector<uint8_t> connack = {0x20, 0x02, 0x00, 0x00}; // CONNACK with success
            ::asio::async_write(*socket, ::asio::buffer(connack),
                [](std::error_code, std::size_t) {});
        }
    }

    uint16_t port_;
    ::asio::io_context ioc_;
    ::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};
    std::atomic<bool> accept_connections_{true};
    std::atomic<bool> accept_publishes_{true};
    std::thread server_thread_;
};

class MqttClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        broker_ = std::make_unique<MockMqttBroker>(test_port_);
        broker_->start();

        // Give broker time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (broker_) {
            broker_->stop();
        }
    }

    ConnectionOptions createTestOptions() {
        ConnectionOptions options;
        options.client_id = "test_client_" + std::to_string(std::time(nullptr));
        options.keep_alive = std::chrono::seconds(30);
        options.clean_session = true;
        return options;
    }

    static constexpr uint16_t test_port_ = 11883;
    std::unique_ptr<MockMqttBroker> broker_;
};

// Test MQTT client construction and destruction
TEST_F(MqttClientTest, Construction) {
    EXPECT_NO_THROW({
        Client client(false); // Don't auto-start IO
    });
}

TEST_F(MqttClientTest, ConstructionWithAutoStart) {
    EXPECT_NO_THROW({
        Client client(true); // Auto-start IO
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
}

// Test basic connection functionality
TEST_F(MqttClientTest, ConnectionBasic) {
    Client client(true);
    auto options = createTestOptions();

    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto status = connect_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    if (status == std::future_status::ready) {
        auto result = connect_future.get();
        EXPECT_EQ(result, ErrorCode::SUCCESS);
        EXPECT_TRUE(client.is_connected());
    }
}

// Test connection state management
TEST_F(MqttClientTest, ConnectionState) {
    Client client(true);

    // Initially disconnected
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.get_state(), ConnectionState::DISCONNECTED);

    auto options = createTestOptions();
    client.async_connect("127.0.0.1", test_port_, options);

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should be connected or connecting
    auto state = client.get_state();
    EXPECT_TRUE(state == ConnectionState::CONNECTED ||
                state == ConnectionState::CONNECTING);
}

// Test connection failure handling
TEST_F(MqttClientTest, ConnectionFailure) {
    Client client(true);
    auto options = createTestOptions();

    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    // Try to connect to non-existent broker
    client.async_connect("127.0.0.1", 9999, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto status = connect_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    if (status == std::future_status::ready) {
        auto result = connect_future.get();
        EXPECT_NE(result, ErrorCode::SUCCESS);
        EXPECT_FALSE(client.is_connected());
    }
}

// Test message publishing
TEST_F(MqttClientTest, PublishMessage) {
    Client client(true);
    auto options = createTestOptions();

    // Connect first
    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(connect_status, std::future_status::ready);
    ASSERT_EQ(connect_future.get(), ErrorCode::SUCCESS);

    // Now publish a message
    std::promise<ErrorCode> publish_promise;
    auto publish_future = publish_promise.get_future();

    std::string test_message = "Hello, MQTT!";
    client.async_publish("test/topic", test_message, QoS::AT_MOST_ONCE, false,
        [&publish_promise](ErrorCode ec) {
            publish_promise.set_value(ec);
        });

    auto publish_status = publish_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(publish_status, std::future_status::ready);

    if (publish_status == std::future_status::ready) {
        auto result = publish_future.get();
        EXPECT_EQ(result, ErrorCode::SUCCESS);
    }
}

// Test topic subscription
TEST_F(MqttClientTest, SubscribeToTopic) {
    Client client(true);
    auto options = createTestOptions();

    // Connect first
    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(connect_status, std::future_status::ready);
    ASSERT_EQ(connect_future.get(), ErrorCode::SUCCESS);

    // Subscribe to topic
    std::promise<ErrorCode> subscribe_promise;
    auto subscribe_future = subscribe_promise.get_future();

    client.async_subscribe("test/topic", QoS::AT_MOST_ONCE,
        [&subscribe_promise](ErrorCode ec) {
            subscribe_promise.set_value(ec);
        });

    auto subscribe_status = subscribe_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(subscribe_status, std::future_status::ready);

    if (subscribe_status == std::future_status::ready) {
        auto result = subscribe_future.get();
        EXPECT_EQ(result, ErrorCode::SUCCESS);
    }
}

// Test message handling
TEST_F(MqttClientTest, MessageHandling) {
    Client client(true);
    auto options = createTestOptions();

    std::atomic<bool> message_received{false};
    std::string received_topic;
    std::vector<uint8_t> received_payload;

    // Set message handler
    client.set_message_handler([&](const mqtt::Message& msg) {
        received_topic = msg.topic;
        received_payload = msg.payload;
        message_received = true;
    });

    // Connect and subscribe
    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(connect_status, std::future_status::ready);
    ASSERT_EQ(connect_future.get(), ErrorCode::SUCCESS);

    // Test that message handler is set
    EXPECT_TRUE(message_received == false); // No messages yet
}

// Test disconnection
TEST_F(MqttClientTest, Disconnection) {
    Client client(true);
    auto options = createTestOptions();

    // Connect first
    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(connect_status, std::future_status::ready);
    ASSERT_EQ(connect_future.get(), ErrorCode::SUCCESS);

    // Disconnect
    client.disconnect();

    // Wait for disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(client.get_state(), ConnectionState::DISCONNECTED);
}

// Test QoS levels
TEST_F(MqttClientTest, QoSLevels) {
    Client client(true);
    auto options = createTestOptions();

    // Connect first
    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(connect_status, std::future_status::ready);
    ASSERT_EQ(connect_future.get(), ErrorCode::SUCCESS);

    // Test different QoS levels
    std::string test_message = "QoS test message";

    // QoS 0
    std::promise<ErrorCode> qos0_promise;
    auto qos0_future = qos0_promise.get_future();

    client.async_publish("test/qos0", test_message, QoS::AT_MOST_ONCE, false,
        [&qos0_promise](ErrorCode ec) {
            qos0_promise.set_value(ec);
        });

    auto qos0_status = qos0_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(qos0_status, std::future_status::ready);

    // QoS 1
    std::promise<ErrorCode> qos1_promise;
    auto qos1_future = qos1_promise.get_future();

    client.async_publish("test/qos1", test_message, QoS::AT_LEAST_ONCE, false,
        [&qos1_promise](ErrorCode ec) {
            qos1_promise.set_value(ec);
        });

    auto qos1_status = qos1_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(qos1_status, std::future_status::ready);
}

// Test client statistics
TEST_F(MqttClientTest, ClientStatistics) {
    Client client(true);

    // Get initial stats
    auto stats = client.get_stats();
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
}

// Test will message functionality
TEST_F(MqttClientTest, WillMessage) {
    Client client(true);
    auto options = createTestOptions();

    // Set will message
    options.will_topic = "test/will";
    options.will_payload = std::vector<uint8_t>{'w', 'i', 'l', 'l'};
    options.will_qos = QoS::AT_MOST_ONCE;
    options.will_retain = false;

    std::promise<ErrorCode> connect_promise;
    auto connect_future = connect_promise.get_future();

    client.async_connect("127.0.0.1", test_port_, options,
        [&connect_promise](ErrorCode ec) {
            connect_promise.set_value(ec);
        });

    auto connect_status = connect_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(connect_status, std::future_status::ready);

    if (connect_status == std::future_status::ready) {
        auto result = connect_future.get();
        EXPECT_EQ(result, ErrorCode::SUCCESS);
    }
}

} // namespace atom::extra::asio::test
