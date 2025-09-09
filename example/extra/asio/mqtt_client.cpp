/*
 * mqtt_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: ASIO MQTT Client Example (Minimal Stub Implementation)
This is a stub implementation since the atom-extra-asio library linking
is not configured in the current build system.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

using namespace std::chrono_literals;

// Minimal stub implementations since atom-extra-asio is not linked

namespace atom::extra::asio::mqtt {

// Stub ErrorCode enum
enum class ErrorCode {
    SUCCESS = 0,
    CONNECTION_FAILED,
    TIMEOUT,
    INVALID_PACKET
};

// Stub QoS enum
enum class QoS : uint8_t {
    AT_MOST_ONCE = 0,
    AT_LEAST_ONCE = 1,
    EXACTLY_ONCE = 2
};

// Stub Message struct
struct Message {
    std::string topic;
    std::string payload;
    QoS qos = QoS::AT_MOST_ONCE;
    bool retain = false;
};

// Stub ConnectionOptions struct
struct ConnectionOptions {
    std::string client_id = "atom_mqtt_client";
    std::string username;
    std::string password;
    uint16_t keep_alive = 60;
    bool clean_session = true;
};

// Stub Client class
class Client {
public:
    Client() {
        std::cout << "MQTT Client created (stub implementation)" << std::endl;
    }

    void set_connection_handler(std::function<void(ErrorCode)> handler) {
        connection_handler_ = std::move(handler);
    }

    void set_message_handler(std::function<void(const Message&)> handler) {
        message_handler_ = std::move(handler);
    }

    void async_connect(const std::string& host, uint16_t port, const ConnectionOptions& options) {
        std::cout << "Connecting to " << host << ":" << port << " (stub)" << std::endl;
        std::cout << "  Client ID: " << options.client_id << std::endl;

        // Simulate connection
        std::thread([this]() {
            std::this_thread::sleep_for(100ms);
            if (connection_handler_) {
                connection_handler_(ErrorCode::SUCCESS);
            }
        }).detach();
    }

    void async_subscribe(const std::string& topic, QoS qos) {
        std::cout << "Subscribing to topic: " << topic << " with QoS " << static_cast<int>(qos) << " (stub)" << std::endl;

        // Simulate receiving messages
        std::thread([this, topic]() {
            std::this_thread::sleep_for(200ms);
            if (message_handler_) {
                Message msg;
                msg.topic = topic;
                msg.payload = "Hello from MQTT stub for topic: " + topic;
                msg.qos = QoS::AT_LEAST_ONCE;
                message_handler_(msg);
            }
        }).detach();
    }

    void async_publish(const Message& message) {
        std::cout << "Publishing to topic: " << message.topic << " (stub)" << std::endl;
        std::cout << "  Payload: " << message.payload.substr(0, 50) << "..." << std::endl;
    }

    void disconnect() {
        std::cout << "Disconnecting MQTT client (stub)..." << std::endl;
    }

private:
    std::function<void(ErrorCode)> connection_handler_;
    std::function<void(const Message&)> message_handler_;
};

} // namespace atom::extra::asio::mqtt

using namespace atom::extra::asio::mqtt;

int main() {
    std::cout << "=== ASIO MQTT Client Example (Stub Implementation) ===" << std::endl;
    std::cout << "Note: This is a stub implementation since atom-extra-asio library is not linked." << std::endl;

    try {
        // 1. Basic MQTT client connection
        std::cout << "\n1. Basic MQTT Client Connection:" << std::endl;
        {
            Client client;

            ConnectionOptions options;
            options.client_id = "atom_mqtt_example_client";
            options.keep_alive = 60;
            options.clean_session = true;

            std::atomic<bool> connected{false};
            std::atomic<int> message_received{0};

            client.set_connection_handler([&connected](ErrorCode ec) {
                if (ec == ErrorCode::SUCCESS) {
                    std::cout << "Connected to MQTT broker (stub)" << std::endl;
                    connected = true;
                } else {
                    std::cout << "Connection failed (stub)" << std::endl;
                }
            });

            client.set_message_handler([&message_received](const Message& msg) {
                std::cout << "Message received (stub):" << std::endl;
                std::cout << "  Topic: " << msg.topic << std::endl;
                std::cout << "  Payload: " << msg.payload << std::endl;
                std::cout << "  QoS: " << static_cast<int>(msg.qos) << std::endl;
                message_received++;
            });

            client.async_connect("test.mosquitto.org", 1883, options);

            // Wait for connection
            std::this_thread::sleep_for(500ms);

            if (connected) {
                client.async_subscribe("atom/test/example", QoS::AT_LEAST_ONCE);

                // Publish a message
                Message msg;
                msg.topic = "atom/test/example";
                msg.qos = QoS::AT_LEAST_ONCE;
                msg.payload = "Hello from Atom MQTT client (stub)!";
                client.async_publish(msg);

                // Wait for messages
                std::this_thread::sleep_for(1s);
            }

            client.disconnect();
            std::cout << "Total messages received: " << message_received.load() << std::endl;
        }

        std::cout << "\n=== MQTT Client Example Complete (Stub Implementation) ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in MQTT client examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
