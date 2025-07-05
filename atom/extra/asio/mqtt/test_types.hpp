#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "types.hpp"


using namespace mqtt;

TEST(ProtocolVersionTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(ProtocolVersion::V3_1_1), 4);
    EXPECT_EQ(static_cast<uint8_t>(ProtocolVersion::V5_0), 5);
}

TEST(QoSTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(QoS::AT_MOST_ONCE), 0);
    EXPECT_EQ(static_cast<uint8_t>(QoS::AT_LEAST_ONCE), 1);
    EXPECT_EQ(static_cast<uint8_t>(QoS::EXACTLY_ONCE), 2);
}

TEST(ErrorCodeTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::SUCCESS), 0);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::CONNECTION_REFUSED_PROTOCOL), 1);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::CONNECTION_REFUSED_IDENTIFIER),
              2);
    EXPECT_EQ(
        static_cast<uint8_t>(ErrorCode::CONNECTION_REFUSED_SERVER_UNAVAILABLE),
        3);
    EXPECT_EQ(
        static_cast<uint8_t>(ErrorCode::CONNECTION_REFUSED_BAD_CREDENTIALS), 4);
    EXPECT_EQ(
        static_cast<uint8_t>(ErrorCode::CONNECTION_REFUSED_NOT_AUTHORIZED), 5);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::UNSPECIFIED_ERROR), 128);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::PAYLOAD_FORMAT_INVALID), 153);
}

TEST(ConnectionOptionsTest, DefaultValues) {
    ConnectionOptions opts;
    EXPECT_EQ(opts.client_id, "");
    EXPECT_EQ(opts.username, "");
    EXPECT_EQ(opts.password, "");
    EXPECT_EQ(opts.keep_alive, std::chrono::seconds(60));
    EXPECT_TRUE(opts.clean_session);
    EXPECT_FALSE(opts.will_topic.has_value());
    EXPECT_FALSE(opts.will_payload.has_value());
    EXPECT_EQ(opts.will_qos, QoS::AT_MOST_ONCE);
    EXPECT_FALSE(opts.will_retain);
    EXPECT_EQ(opts.version, ProtocolVersion::V5_0);
    EXPECT_FALSE(opts.use_tls);
    EXPECT_EQ(opts.ca_cert_file, "");
    EXPECT_EQ(opts.cert_file, "");
    EXPECT_EQ(opts.private_key_file, "");
    EXPECT_TRUE(opts.verify_certificate);
}

TEST(ConnectionOptionsTest, CustomValues) {
    ConnectionOptions opts;
    opts.client_id = "cid";
    opts.username = "user";
    opts.password = "pw";
    opts.keep_alive = std::chrono::seconds(10);
    opts.clean_session = false;
    opts.will_topic = "will";
    opts.will_payload = std::vector<uint8_t>{1, 2, 3};
    opts.will_qos = QoS::EXACTLY_ONCE;
    opts.will_retain = true;
    opts.version = ProtocolVersion::V3_1_1;
    opts.use_tls = true;
    opts.ca_cert_file = "ca.pem";
    opts.cert_file = "cert.pem";
    opts.private_key_file = "key.pem";
    opts.verify_certificate = false;

    EXPECT_EQ(opts.client_id, "cid");
    EXPECT_EQ(opts.username, "user");
    EXPECT_EQ(opts.password, "pw");
    EXPECT_EQ(opts.keep_alive, std::chrono::seconds(10));
    EXPECT_FALSE(opts.clean_session);
    ASSERT_TRUE(opts.will_topic.has_value());
    EXPECT_EQ(opts.will_topic.value(), "will");
    ASSERT_TRUE(opts.will_payload.has_value());
    EXPECT_EQ(opts.will_payload.value(), std::vector<uint8_t>({1, 2, 3}));
    EXPECT_EQ(opts.will_qos, QoS::EXACTLY_ONCE);
    EXPECT_TRUE(opts.will_retain);
    EXPECT_EQ(opts.version, ProtocolVersion::V3_1_1);
    EXPECT_TRUE(opts.use_tls);
    EXPECT_EQ(opts.ca_cert_file, "ca.pem");
    EXPECT_EQ(opts.cert_file, "cert.pem");
    EXPECT_EQ(opts.private_key_file, "key.pem");
    EXPECT_FALSE(opts.verify_certificate);
}

TEST(MessageTest, DefaultValues) {
    Message msg;
    EXPECT_EQ(msg.topic, "");
    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.qos, QoS::AT_MOST_ONCE);
    EXPECT_FALSE(msg.retain);
    EXPECT_EQ(msg.packet_id, 0);
    EXPECT_FALSE(msg.message_expiry_interval.has_value());
    EXPECT_FALSE(msg.response_topic.has_value());
    EXPECT_FALSE(msg.correlation_data.has_value());
    EXPECT_FALSE(msg.content_type.has_value());
}

TEST(MessageTest, CustomValues) {
    Message msg;
    msg.topic = "topic";
    msg.payload = {1, 2, 3, 4};
    msg.qos = QoS::EXACTLY_ONCE;
    msg.retain = true;
    msg.packet_id = 42;
    msg.message_expiry_interval = 1234;
    msg.response_topic = "resp";
    msg.correlation_data = std::vector<uint8_t>{9, 8, 7};
    msg.content_type = "ct";

    EXPECT_EQ(msg.topic, "topic");
    EXPECT_EQ(msg.payload, std::vector<uint8_t>({1, 2, 3, 4}));
    EXPECT_EQ(msg.qos, QoS::EXACTLY_ONCE);
    EXPECT_TRUE(msg.retain);
    EXPECT_EQ(msg.packet_id, 42);
    ASSERT_TRUE(msg.message_expiry_interval.has_value());
    EXPECT_EQ(msg.message_expiry_interval.value(), 1234u);
    ASSERT_TRUE(msg.response_topic.has_value());
    EXPECT_EQ(msg.response_topic.value(), "resp");
    ASSERT_TRUE(msg.correlation_data.has_value());
    EXPECT_EQ(msg.correlation_data.value(), std::vector<uint8_t>({9, 8, 7}));
    ASSERT_TRUE(msg.content_type.has_value());
    EXPECT_EQ(msg.content_type.value(), "ct");
}

TEST(SubscriptionTest, DefaultValues) {
    Subscription sub;
    EXPECT_EQ(sub.topic_filter, "");
    EXPECT_EQ(sub.qos, QoS::AT_MOST_ONCE);
    EXPECT_FALSE(sub.no_local);
    EXPECT_FALSE(sub.retain_as_published);
    EXPECT_EQ(sub.retain_handling, 0);
}

TEST(SubscriptionTest, CustomValues) {
    Subscription sub;
    sub.topic_filter = "foo/#";
    sub.qos = QoS::AT_LEAST_ONCE;
    sub.no_local = true;
    sub.retain_as_published = true;
    sub.retain_handling = 2;

    EXPECT_EQ(sub.topic_filter, "foo/#");
    EXPECT_EQ(sub.qos, QoS::AT_LEAST_ONCE);
    EXPECT_TRUE(sub.no_local);
    EXPECT_TRUE(sub.retain_as_published);
    EXPECT_EQ(sub.retain_handling, 2);
}

TEST(ClientStatsTest, DefaultValues) {
    ClientStats stats;
    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
    EXPECT_EQ(stats.reconnect_count, 0u);
    // connected_since is default constructed, can't check value
}

TEST(ClientStatsTest, CustomValues) {
    ClientStats stats;
    stats.messages_sent = 10;
    stats.messages_received = 20;
    stats.bytes_sent = 100;
    stats.bytes_received = 200;
    stats.reconnect_count = 3;
    auto now = std::chrono::steady_clock::now();
    stats.connected_since = now;

    EXPECT_EQ(stats.messages_sent, 10u);
    EXPECT_EQ(stats.messages_received, 20u);
    EXPECT_EQ(stats.bytes_sent, 100u);
    EXPECT_EQ(stats.bytes_received, 200u);
    EXPECT_EQ(stats.reconnect_count, 3u);
    EXPECT_EQ(stats.connected_since, now);
}

TEST(ResultTest, SuccessAndError) {
    Result<int> ok = 42;
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok.value(), 42);

    Result<int> err = std::unexpected(ErrorCode::PROTOCOL_ERROR);
    ASSERT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), ErrorCode::PROTOCOL_ERROR);
}

TEST(CallbackTypesTest, MessageHandler) {
    Message msg;
    msg.topic = "abc";
    bool called = false;
    MessageHandler handler = [&](const Message& m) {
        called = true;
        EXPECT_EQ(m.topic, "abc");
    };
    handler(msg);
    EXPECT_TRUE(called);
}

TEST(CallbackTypesTest, ConnectionHandler) {
    bool called = false;
    ConnectionHandler handler = [&](ErrorCode ec) {
        called = true;
        EXPECT_EQ(ec, ErrorCode::SUCCESS);
    };
    handler(ErrorCode::SUCCESS);
    EXPECT_TRUE(called);
}

TEST(CallbackTypesTest, DisconnectionHandler) {
    bool called = false;
    DisconnectionHandler handler = [&](ErrorCode ec) {
        called = true;
        EXPECT_EQ(ec, ErrorCode::SERVER_UNAVAILABLE);
    };
    handler(ErrorCode::SERVER_UNAVAILABLE);
    EXPECT_TRUE(called);
}
