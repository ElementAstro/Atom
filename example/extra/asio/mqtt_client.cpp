#include "atom/extra/asio/mqtt/client.hpp"
#include "atom/extra/asio/mqtt/types.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace atom::extra::asio::mqtt;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== ASIO MQTT Client Example ===" << std::endl;

        // 1. Basic MQTT connection and messaging
        std::cout << "\n1. Basic MQTT Connection and Messaging:" << std::endl;
        {
            Client client;

            // Set up connection options
            ConnectionOptions options;
            options.client_id = "atom_mqtt_example_client";
            options.username = "test_user";
            options.password = "test_password";
            options.keep_alive = 60s;
            options.clean_session = true;
            options.use_tls = false;  // Use plain TCP for this example

            std::atomic<bool> connected{false};
            std::atomic<bool> message_received{false};

            // Set connection handler
            client.set_connection_handler([&connected](ErrorCode ec) {
                if (ec == ErrorCode::SUCCESS) {
                    std::cout << "Successfully connected to MQTT broker!"
                              << std::endl;
                    connected = true;
                } else {
                    std::cerr << "Connection failed with error: "
                              << static_cast<int>(ec) << std::endl;
                }
            });

            // Set message handler
            client.set_message_handler([&message_received](const Message& msg) {
                std::cout << "Received message:" << std::endl;
                std::cout << "  Topic: " << msg.topic << std::endl;
                std::cout << "  QoS: " << static_cast<int>(msg.qos)
                          << std::endl;
                std::cout << "  Retain: " << (msg.retain ? "true" : "false")
                          << std::endl;
                std::cout << "  Payload: ";
                for (const auto& byte : msg.payload) {
                    std::cout << static_cast<char>(byte);
                }
                std::cout << std::endl;
                message_received = true;
            });

            // Connect to broker (using public test broker)
            std::cout << "Connecting to MQTT broker..." << std::endl;
            client.async_connect("test.mosquitto.org", 1883, options);

            // Wait for connection
            auto start_time = std::chrono::steady_clock::now();
            while (!connected &&
                   std::chrono::steady_clock::now() - start_time < 10s) {
                std::this_thread::sleep_for(100ms);
            }

            if (connected) {
                // Subscribe to a topic
                std::cout << "Subscribing to topic 'atom/test/example'..."
                          << std::endl;
                client.async_subscribe("atom/test/example", QoS::AT_LEAST_ONCE);

                std::this_thread::sleep_for(1s);

                // Publish a message
                std::cout << "Publishing message..." << std::endl;
                Message msg;
                msg.topic = "atom/test/example";
                msg.qos = QoS::AT_LEAST_ONCE;
                msg.retain = false;
                std::string payload = "Hello from Atom MQTT Client!";
                msg.payload.assign(payload.begin(), payload.end());

                client.async_publish(std::move(msg));

                // Wait for message to be received
                start_time = std::chrono::steady_clock::now();
                while (!message_received &&
                       std::chrono::steady_clock::now() - start_time < 5s) {
                    std::this_thread::sleep_for(100ms);
                }

                if (message_received) {
                    std::cout << "Message loop completed successfully!"
                              << std::endl;
                } else {
                    std::cout << "Did not receive message within timeout"
                              << std::endl;
                }

                // Unsubscribe
                std::cout << "Unsubscribing from topic..." << std::endl;
                client.async_unsubscribe("atom/test/example");

                std::this_thread::sleep_for(1s);
            }

            // Disconnect
            std::cout << "Disconnecting..." << std::endl;
            client.disconnect();
        }

        // 2. Multiple topic subscriptions
        std::cout << "\n2. Multiple Topic Subscriptions:" << std::endl;
        {
            Client client;

            ConnectionOptions options;
            options.client_id = "atom_mqtt_multi_topic";
            options.keep_alive = 30s;
            options.clean_session = true;

            std::atomic<int> messages_received{0};

            client.set_connection_handler([](ErrorCode ec) {
                if (ec == ErrorCode::SUCCESS) {
                    std::cout << "Connected for multi-topic test" << std::endl;
                } else {
                    std::cerr << "Multi-topic connection failed" << std::endl;
                }
            });

            client.set_message_handler(
                [&messages_received](const Message& msg) {
                    std::cout << "Multi-topic message - Topic: " << msg.topic
                              << ", Payload: ";
                    for (const auto& byte : msg.payload) {
                        std::cout << static_cast<char>(byte);
                    }
                    std::cout << std::endl;
                    messages_received++;
                });

            client.async_connect("test.mosquitto.org", 1883, options);
            std::this_thread::sleep_for(2s);

            if (client.is_connected()) {
                // Subscribe to multiple topics
                std::vector<std::string> topics = {
                    "atom/test/topic1", "atom/test/topic2", "atom/test/topic3"};

                for (const auto& topic : topics) {
                    std::cout << "Subscribing to: " << topic << std::endl;
                    client.async_subscribe(topic, QoS::AT_MOST_ONCE);
                }

                std::this_thread::sleep_for(1s);

                // Publish to each topic
                for (size_t i = 0; i < topics.size(); ++i) {
                    Message msg;
                    msg.topic = topics[i];
                    msg.qos = QoS::AT_MOST_ONCE;
                    std::string payload =
                        "Message for topic " + std::to_string(i + 1);
                    msg.payload.assign(payload.begin(), payload.end());

                    std::cout << "Publishing to: " << topics[i] << std::endl;
                    client.async_publish(std::move(msg));
                    std::this_thread::sleep_for(200ms);
                }

                // Wait for messages
                std::this_thread::sleep_for(3s);
                std::cout << "Received " << messages_received.load()
                          << " messages" << std::endl;
            }

            client.disconnect();
        }

        // 3. QoS levels demonstration
        std::cout << "\n3. QoS Levels Demonstration:" << std::endl;
        {
            Client client;

            ConnectionOptions options;
            options.client_id = "atom_mqtt_qos_test";
            options.keep_alive = 30s;

            std::atomic<int> qos0_received{0};
            std::atomic<int> qos1_received{0};
            std::atomic<int> qos2_received{0};

            client.set_message_handler([&](const Message& msg) {
                std::cout << "QoS " << static_cast<int>(msg.qos)
                          << " message on topic: " << msg.topic << std::endl;
                switch (msg.qos) {
                    case QoS::AT_MOST_ONCE:
                        qos0_received++;
                        break;
                    case QoS::AT_LEAST_ONCE:
                        qos1_received++;
                        break;
                    case QoS::EXACTLY_ONCE:
                        qos2_received++;
                        break;
                }
            });

            client.async_connect("test.mosquitto.org", 1883, options);
            std::this_thread::sleep_for(2s);

            if (client.is_connected()) {
                // Subscribe with different QoS levels
                client.async_subscribe("atom/test/qos0", QoS::AT_MOST_ONCE);
                client.async_subscribe("atom/test/qos1", QoS::AT_LEAST_ONCE);
                client.async_subscribe("atom/test/qos2", QoS::EXACTLY_ONCE);

                std::this_thread::sleep_for(1s);

                // Publish with different QoS levels
                std::vector<std::pair<std::string, QoS>> test_cases = {
                    {"atom/test/qos0", QoS::AT_MOST_ONCE},
                    {"atom/test/qos1", QoS::AT_LEAST_ONCE},
                    {"atom/test/qos2", QoS::EXACTLY_ONCE}};

                for (const auto& [topic, qos] : test_cases) {
                    Message msg;
                    msg.topic = topic;
                    msg.qos = qos;
                    std::string payload =
                        "QoS " + std::to_string(static_cast<int>(qos)) +
                        " message";
                    msg.payload.assign(payload.begin(), payload.end());

                    std::cout << "Publishing QoS " << static_cast<int>(qos)
                              << " message to " << topic << std::endl;
                    client.async_publish(std::move(msg));
                    std::this_thread::sleep_for(500ms);
                }

                // Wait for messages
                std::this_thread::sleep_for(3s);

                std::cout << "QoS 0 messages received: " << qos0_received.load()
                          << std::endl;
                std::cout << "QoS 1 messages received: " << qos1_received.load()
                          << std::endl;
                std::cout << "QoS 2 messages received: " << qos2_received.load()
                          << std::endl;
            }

            client.disconnect();
        }

        // 4. Retained messages
        std::cout << "\n4. Retained Messages:" << std::endl;
        {
            Client client;

            ConnectionOptions options;
            options.client_id = "atom_mqtt_retained_test";
            options.keep_alive = 30s;

            client.async_connect("test.mosquitto.org", 1883, options);
            std::this_thread::sleep_for(2s);

            if (client.is_connected()) {
                // Publish a retained message
                Message retained_msg;
                retained_msg.topic = "atom/test/retained";
                retained_msg.qos = QoS::AT_LEAST_ONCE;
                retained_msg.retain = true;
                std::string payload = "This is a retained message";
                retained_msg.payload.assign(payload.begin(), payload.end());

                std::cout << "Publishing retained message..." << std::endl;
                client.async_publish(std::move(retained_msg));

                std::this_thread::sleep_for(1s);

                // Now subscribe to the topic (should receive the retained
                // message)
                std::atomic<bool> retained_received{false};
                client.set_message_handler(
                    [&retained_received](const Message& msg) {
                        if (msg.retain) {
                            std::cout << "Received retained message: ";
                            for (const auto& byte : msg.payload) {
                                std::cout << static_cast<char>(byte);
                            }
                            std::cout << std::endl;
                            retained_received = true;
                        }
                    });

                std::cout << "Subscribing to retained message topic..."
                          << std::endl;
                client.async_subscribe("atom/test/retained",
                                       QoS::AT_LEAST_ONCE);

                // Wait for retained message
                auto start_time = std::chrono::steady_clock::now();
                while (!retained_received &&
                       std::chrono::steady_clock::now() - start_time < 5s) {
                    std::this_thread::sleep_for(100ms);
                }

                if (retained_received) {
                    std::cout << "Successfully received retained message!"
                              << std::endl;
                } else {
                    std::cout
                        << "Did not receive retained message within timeout"
                        << std::endl;
                }
            }

            client.disconnect();
        }

        // 5. Client statistics
        std::cout << "\n5. Client Statistics:" << std::endl;
        {
            Client client;

            ConnectionOptions options;
            options.client_id = "atom_mqtt_stats_test";

            client.async_connect("test.mosquitto.org", 1883, options);
            std::this_thread::sleep_for(2s);

            if (client.is_connected()) {
                // Get initial stats
                auto initial_stats = client.get_stats();
                std::cout << "Initial stats:" << std::endl;
                std::cout << "  Messages sent: " << initial_stats.messages_sent
                          << std::endl;
                std::cout << "  Messages received: "
                          << initial_stats.messages_received << std::endl;
                std::cout << "  Bytes sent: " << initial_stats.bytes_sent
                          << std::endl;
                std::cout << "  Bytes received: "
                          << initial_stats.bytes_received << std::endl;

                // Perform some operations
                client.async_subscribe("atom/test/stats", QoS::AT_MOST_ONCE);

                for (int i = 0; i < 5; ++i) {
                    Message msg;
                    msg.topic = "atom/test/stats";
                    msg.qos = QoS::AT_MOST_ONCE;
                    std::string payload =
                        "Stats test message " + std::to_string(i);
                    msg.payload.assign(payload.begin(), payload.end());

                    client.async_publish(std::move(msg));
                    std::this_thread::sleep_for(200ms);
                }

                std::this_thread::sleep_for(2s);

                // Get final stats
                auto final_stats = client.get_stats();
                std::cout << "Final stats:" << std::endl;
                std::cout << "  Messages sent: " << final_stats.messages_sent
                          << std::endl;
                std::cout << "  Messages received: "
                          << final_stats.messages_received << std::endl;
                std::cout << "  Bytes sent: " << final_stats.bytes_sent
                          << std::endl;
                std::cout << "  Bytes received: " << final_stats.bytes_received
                          << std::endl;
            }

            client.disconnect();
        }

        std::cout << "\n=== MQTT Client Example Completed ===" << std::endl;
        std::cout << "\nNote: This example uses test.mosquitto.org as a public "
                     "MQTT broker."
                  << std::endl;
        std::cout << "For production use, replace with your own MQTT broker."
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
