// atom/extra/asio/mqtt/test_packet.hpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "packet.hpp"
#include "types.hpp"


using namespace mqtt;

TEST(PacketTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(PacketType::CONNECT), 1);
    EXPECT_EQ(static_cast<uint8_t>(PacketType::CONNACK), 2);
    EXPECT_EQ(static_cast<uint8_t>(PacketType::PUBLISH), 3);
    EXPECT_EQ(static_cast<uint8_t>(PacketType::SUBSCRIBE), 8);
    EXPECT_EQ(static_cast<uint8_t>(PacketType::DISCONNECT), 14);
}

TEST(PacketHeaderTest, FlagManipulation) {
    PacketHeader header;
    header.flags = 0;

    header.set_duplicate(true);
    EXPECT_TRUE(header.is_duplicate());
    header.set_duplicate(false);
    EXPECT_FALSE(header.is_duplicate());

    header.set_qos(QoS::AT_MOST_ONCE);
    EXPECT_EQ(header.get_qos(), QoS::AT_MOST_ONCE);
    header.set_qos(QoS::AT_LEAST_ONCE);
    EXPECT_EQ(header.get_qos(), QoS::AT_LEAST_ONCE);
    header.set_qos(QoS::EXACTLY_ONCE);
    EXPECT_EQ(header.get_qos(), QoS::EXACTLY_ONCE);

    header.set_retain(true);
    EXPECT_TRUE(header.is_retain());
    header.set_retain(false);
    EXPECT_FALSE(header.is_retain());
}

TEST(BinaryBufferTest, WriteAndReadIntegral) {
    BinaryBuffer buf;
    buf.write<uint8_t>(0x12);
    buf.write<uint16_t>(0x3456);
    buf.write<uint32_t>(0x789ABCDE);

    buf.reset_position();
    auto v8 = buf.read<uint8_t>();
    ASSERT_TRUE(v8.has_value());
    EXPECT_EQ(*v8, 0x12);

    auto v16 = buf.read<uint16_t>();
    ASSERT_TRUE(v16.has_value());
    EXPECT_EQ(*v16, 0x3456);

    auto v32 = buf.read<uint32_t>();
    ASSERT_TRUE(v32.has_value());
    EXPECT_EQ(*v32, 0x789ABCDE);
}

TEST(BinaryBufferTest, WriteAndReadString) {
    BinaryBuffer buf;
    std::string test = "hello";
    buf.write_string(test);

    buf.reset_position();
    auto result = buf.read_string();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, test);
}

TEST(BinaryBufferTest, WriteAndReadBytes) {
    BinaryBuffer buf;
    std::vector<uint8_t> bytes = {1, 2, 3, 4, 5};
    buf.write_bytes(bytes);

    EXPECT_EQ(buf.size(), bytes.size());
    EXPECT_EQ(std::vector<uint8_t>(buf.data().begin(), buf.data().end()),
              bytes);
}

TEST(BinaryBufferTest, WriteAndReadVariableInt) {
    BinaryBuffer buf;
    buf.write_variable_int(127);
    buf.write_variable_int(128);
    buf.write_variable_int(16383);
    buf.write_variable_int(0xFFFFFFF);

    buf.reset_position();
    auto v1 = buf.read_variable_int();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 127u);

    auto v2 = buf.read_variable_int();
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 128u);

    auto v3 = buf.read_variable_int();
    ASSERT_TRUE(v3.has_value());
    EXPECT_EQ(*v3, 16383u);

    auto v4 = buf.read_variable_int();
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(*v4, 0xFFFFFFFu);
}

TEST(BinaryBufferTest, AppendFromOtherBuffer) {
    BinaryBuffer buf1;
    buf1.write<uint8_t>(1);
    buf1.write<uint8_t>(2);

    BinaryBuffer buf2;
    buf2.write<uint8_t>(3);
    buf2.write<uint8_t>(4);

    buf1.append_from(buf2);
    EXPECT_EQ(buf1.size(), 4u);
    EXPECT_EQ(buf1.data()[2], 3);
    EXPECT_EQ(buf1.data()[3], 4);
}

TEST(PacketCodecTest, SerializeAndParseConnect) {
    ConnectionOptions opts;
    opts.client_id = "cid";
    opts.username = "user";
    opts.password = "pw";
    opts.keep_alive = std::chrono::seconds(10);
    opts.clean_session = true;
    opts.version = ProtocolVersion::V5_0;

    BinaryBuffer buf = PacketCodec::serialize_connect(opts);
    // The first byte should be CONNECT packet type
    EXPECT_EQ(buf.data()[0] >> 4, static_cast<uint8_t>(PacketType::CONNECT));
}

TEST(PacketCodecTest, SerializeAndParsePublish) {
    Message msg;
    msg.topic = "topic";
    msg.payload = {1, 2, 3};
    msg.qos = QoS::AT_LEAST_ONCE;
    msg.retain = true;
    msg.packet_id = 42;
    msg.message_expiry_interval = 123;
    msg.response_topic = "resp";
    msg.correlation_data = std::vector<uint8_t>{9, 8};
    msg.content_type = "ct";

    BinaryBuffer buf = PacketCodec::serialize_publish(msg, msg.packet_id);
    // The first byte should be PUBLISH packet type
    EXPECT_EQ(buf.data()[0] >> 4, static_cast<uint8_t>(PacketType::PUBLISH));
}

TEST(PacketCodecTest, SerializeAndParseSubscribe) {
    Subscription sub;
    sub.topic_filter = "foo/#";
    sub.qos = QoS::AT_LEAST_ONCE;
    std::vector<Subscription> subs = {sub};
    BinaryBuffer buf = PacketCodec::serialize_subscribe(subs, 123);
    EXPECT_EQ(buf.data()[0] >> 4, static_cast<uint8_t>(PacketType::SUBSCRIBE));
}

TEST(PacketCodecTest, SerializeAndParseUnsubscribe) {
    std::vector<std::string> topics = {"foo/#", "bar"};
    BinaryBuffer buf = PacketCodec::serialize_unsubscribe(topics, 321);
    EXPECT_EQ(buf.data()[0] >> 4,
              static_cast<uint8_t>(PacketType::UNSUBSCRIBE));
}

TEST(PacketCodecTest, SerializePingReq) {
    BinaryBuffer buf = PacketCodec::serialize_pingreq();
    EXPECT_EQ(buf.data()[0] >> 4, static_cast<uint8_t>(PacketType::PINGREQ));
    EXPECT_EQ(buf.data()[1], 0);
}

TEST(PacketCodecTest, SerializeDisconnect) {
    BinaryBuffer buf = PacketCodec::serialize_disconnect(ProtocolVersion::V5_0,
                                                         ErrorCode::SUCCESS);
    EXPECT_EQ(buf.data()[0] >> 4, static_cast<uint8_t>(PacketType::DISCONNECT));
}

TEST(PacketCodecTest, ParseHeader) {
    // Compose a simple header: PUBLISH, flags=0, remaining_length=1
    std::vector<uint8_t> data = {static_cast<uint8_t>(PacketType::PUBLISH) << 4,
                                 1};
    auto result = PacketCodec::parse_header(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, PacketType::PUBLISH);
    EXPECT_EQ(result->flags, 0);
    EXPECT_EQ(result->remaining_length, 1u);
}

TEST(PacketCodecTest, ParseConnack) {
    std::vector<uint8_t> data = {0x01, 0x00};  // session present, return code 0
    auto result = PacketCodec::parse_connack(data, ProtocolVersion::V3_1_1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ErrorCode::SUCCESS);
}

TEST(PacketCodecTest, ParsePublish) {
    // Compose a publish packet: topic="a", payload={1,2,3}
    BinaryBuffer buf;
    buf.write_string("a");
    buf.write_variable_int(0);  // properties
    buf.write_bytes(std::vector<uint8_t>{1, 2, 3});
    PacketHeader header;
    header.type = PacketType::PUBLISH;
    header.flags = 0;
    auto result = PacketCodec::parse_publish(header, buf.data());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->topic, "a");
    EXPECT_EQ(result->payload, std::vector<uint8_t>({1, 2, 3}));
}

TEST(PacketCodecTest, ParseSuback) {
    // Compose a SUBACK packet: packet_id=1, properties=0, return codes={0,1}
    BinaryBuffer buf;
    buf.write<uint16_t>(1);
    buf.write_variable_int(0);
    buf.write<uint8_t>(0);
    buf.write<uint8_t>(1);
    auto result = PacketCodec::parse_suback(buf.data());
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0], static_cast<ErrorCode>(0));
    EXPECT_EQ((*result)[1], static_cast<ErrorCode>(1));
}

TEST(PacketCodecTest, ParseUnsuback) {
    // Compose an UNSUBACK packet: packet_id=1, properties=0, return codes={0}
    BinaryBuffer buf;
    buf.write<uint16_t>(1);
    buf.write_variable_int(0);
    buf.write<uint8_t>(0);
    auto result = PacketCodec::parse_unsuback(buf.data());
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0], static_cast<ErrorCode>(0));
}

TEST(BinaryBufferTest, ReadMalformedPacket) {
    BinaryBuffer buf;
    // Not enough data for uint16_t
    buf.write<uint8_t>(0x01);
    buf.reset_position();
    auto result = buf.read<uint16_t>();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ErrorCode::MALFORMED_PACKET);
}