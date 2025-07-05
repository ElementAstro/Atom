#include "packet.hpp"

namespace mqtt {

BinaryBuffer PacketCodec::serialize_connect(const ConnectionOptions& options) {
    BinaryBuffer buffer;

    // Fixed header
    buffer.write<uint8_t>(static_cast<uint8_t>(PacketType::CONNECT) << 4);

    BinaryBuffer payload;

    // Protocol name and version
    if (options.version == ProtocolVersion::V5_0) {
        payload.write_string("MQTT");
        payload.write<uint8_t>(5);
    } else {
        payload.write_string("MQTT");
        payload.write<uint8_t>(4);
    }

    // Connect flags
    uint8_t connect_flags = 0;
    if (!options.username.empty())
        connect_flags |= 0x80;
    if (!options.password.empty())
        connect_flags |= 0x40;
    if (options.will_topic.has_value()) {
        connect_flags |= 0x04;
        if (options.will_retain)
            connect_flags |= 0x20;
        connect_flags |= (static_cast<uint8_t>(options.will_qos) << 3);
    }
    if (options.clean_session)
        connect_flags |= 0x02;

    payload.write<uint8_t>(connect_flags);

    // Keep alive
    payload.write<uint16_t>(static_cast<uint16_t>(options.keep_alive.count()));

    // Properties (MQTT 5.0 only)
    if (options.version == ProtocolVersion::V5_0) {
        payload.write_variable_int(0);  // No properties for now
    }

    // Client identifier
    payload.write_string(options.client_id);

    // Will properties and payload (MQTT 5.0)
    if (options.will_topic.has_value()) {
        if (options.version == ProtocolVersion::V5_0) {
            payload.write_variable_int(0);  // Will properties
        }
        payload.write_string(*options.will_topic);
        if (options.will_payload.has_value()) {
            payload.write<uint16_t>(
                static_cast<uint16_t>(options.will_payload->size()));
            payload.write_bytes(*options.will_payload);
        } else {
            payload.write<uint16_t>(0);
        }
    }

    // Username and password
    if (!options.username.empty()) {
        payload.write_string(options.username);
    }
    if (!options.password.empty()) {
        payload.write_string(options.password);
    }

    // Write remaining length and payload
    buffer.write_variable_int(static_cast<uint32_t>(payload.size()));
    buffer.append_from(payload);

    return buffer;
}

BinaryBuffer PacketCodec::serialize_publish(const Message& message,
                                            uint16_t packet_id) {
    BinaryBuffer buffer;

    // Fixed header
    uint8_t header_byte = static_cast<uint8_t>(PacketType::PUBLISH) << 4;
    if (message.qos != QoS::AT_MOST_ONCE) {
        header_byte |= (static_cast<uint8_t>(message.qos) << 1);
    }
    if (message.retain) {
        header_byte |= 0x01;
    }
    buffer.write<uint8_t>(header_byte);

    BinaryBuffer payload;

    // Topic name
    payload.write_string(message.topic);

    // Packet identifier (for QoS > 0)
    if (message.qos != QoS::AT_MOST_ONCE) {
        payload.write<uint16_t>(packet_id);
    }

    // Properties (MQTT 5.0)
    BinaryBuffer properties;
    if (message.message_expiry_interval.has_value()) {
        properties.write<uint8_t>(0x02);  // Message Expiry Interval
        properties.write<uint32_t>(*message.message_expiry_interval);
    }
    if (message.response_topic.has_value()) {
        properties.write<uint8_t>(0x08);  // Response Topic
        properties.write_string(*message.response_topic);
    }
    if (message.correlation_data.has_value()) {
        properties.write<uint8_t>(0x09);  // Correlation Data
        properties.write<uint16_t>(
            static_cast<uint16_t>(message.correlation_data->size()));
        properties.write_bytes(*message.correlation_data);
    }
    if (message.content_type.has_value()) {
        properties.write<uint8_t>(0x03);  // Content Type
        properties.write_string(*message.content_type);
    }

    payload.write_variable_int(static_cast<uint32_t>(properties.size()));
    payload.append_from(properties);

    // Message payload
    payload.write_bytes(message.payload);

    // Write remaining length and payload
    buffer.write_variable_int(static_cast<uint32_t>(payload.size()));
    buffer.append_from(payload);

    return buffer;
}

BinaryBuffer PacketCodec::serialize_subscribe(
    const std::vector<Subscription>& subscriptions, uint16_t packet_id) {
    BinaryBuffer buffer;

    // Fixed header
    buffer.write<uint8_t>((static_cast<uint8_t>(PacketType::SUBSCRIBE) << 4) |
                          0x02);

    BinaryBuffer payload;

    // Packet identifier
    payload.write<uint16_t>(packet_id);

    // Properties (MQTT 5.0)
    payload.write_variable_int(0);  // No properties for now

    // Topic filters
    for (const auto& sub : subscriptions) {
        payload.write_string(sub.topic_filter);

        uint8_t options = static_cast<uint8_t>(sub.qos);
        if (sub.no_local)
            options |= 0x04;
        if (sub.retain_as_published)
            options |= 0x08;
        options |= (sub.retain_handling << 4);

        payload.write<uint8_t>(options);
    }

    // Write remaining length and payload
    buffer.write_variable_int(static_cast<uint32_t>(payload.size()));
    buffer.append_from(payload);

    return buffer;
}

BinaryBuffer PacketCodec::serialize_unsubscribe(
    const std::vector<std::string>& topics, uint16_t packet_id) {
    BinaryBuffer buffer;

    // Fixed header
    buffer.write<uint8_t>((static_cast<uint8_t>(PacketType::UNSUBSCRIBE) << 4) |
                          0x02);

    BinaryBuffer payload;

    // Packet identifier
    payload.write<uint16_t>(packet_id);

    // Properties (MQTT 5.0)
    payload.write_variable_int(0);  // No properties for now

    // Topic filters
    for (const auto& topic : topics) {
        payload.write_string(topic);
    }

    // Write remaining length and payload
    buffer.write_variable_int(static_cast<uint32_t>(payload.size()));
    buffer.append_from(payload);

    return buffer;
}

BinaryBuffer PacketCodec::serialize_pingreq() {
    BinaryBuffer buffer;
    buffer.write<uint8_t>(static_cast<uint8_t>(PacketType::PINGREQ) << 4);
    buffer.write<uint8_t>(0);  // Remaining length
    return buffer;
}

BinaryBuffer PacketCodec::serialize_disconnect(ProtocolVersion version,
                                               ErrorCode reason) {
    BinaryBuffer buffer;
    buffer.write<uint8_t>(static_cast<uint8_t>(PacketType::DISCONNECT) << 4);

    if (version == ProtocolVersion::V5_0) {
        BinaryBuffer payload;
        payload.write<uint8_t>(static_cast<uint8_t>(reason));
        payload.write_variable_int(0);  // No properties

        buffer.write_variable_int(static_cast<uint32_t>(payload.size()));
        buffer.append_from(payload);
    } else {
        buffer.write<uint8_t>(0);  // Remaining length
    }

    return buffer;
}

Result<PacketHeader> PacketCodec::parse_header(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        return std::unexpected(ErrorCode::MALFORMED_PACKET);
    }

    PacketHeader header;
    header.type = static_cast<PacketType>((data[0] >> 4) & 0x0F);
    header.flags = data[0] & 0x0F;

    // Parse remaining length
    size_t pos = 1;
    uint32_t remaining_length = 0;
    uint8_t shift = 0;

    do {
        if (pos >= data.size() || shift >= 28) {
            return std::unexpected(ErrorCode::MALFORMED_PACKET);
        }

        uint8_t byte = data[pos++];
        remaining_length |= (byte & 0x7F) << shift;

        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    } while (true);

    header.remaining_length = remaining_length;
    return header;
}

Result<ErrorCode> PacketCodec::parse_connack(std::span<const uint8_t> data,
                                             ProtocolVersion version) {
    if (data.size() < 2) {
        return std::unexpected(ErrorCode::MALFORMED_PACKET);
    }

    size_t pos = 0;

    // Connect acknowledge flags
    uint8_t flags = data[pos++];
    bool session_present = (flags & 0x01) != 0;

    // Return code
    uint8_t return_code = data[pos++];

    if (version == ProtocolVersion::V5_0) {
        // Skip properties for now
        if (pos < data.size()) {
            BinaryBuffer buffer;
            buffer.write_bytes(data.subspan(pos));
            auto properties_length = buffer.read_variable_int();
            if (!properties_length) {
                return std::unexpected(properties_length.error());
            }
        }
    }

    return static_cast<ErrorCode>(return_code);
}

Result<Message> PacketCodec::parse_publish(const PacketHeader& header,
                                           std::span<const uint8_t> data) {
    BinaryBuffer buffer;
    buffer.write_bytes(data);

    Message message;
    message.qos = header.get_qos();
    message.retain = header.is_retain();

    // Topic name
    auto topic_result = buffer.read_string();
    if (!topic_result)
        return std::unexpected(topic_result.error());
    message.topic = *topic_result;

    // Packet identifier (for QoS > 0)
    if (message.qos != QoS::AT_MOST_ONCE) {
        auto packet_id_result = buffer.read<uint16_t>();
        if (!packet_id_result)
            return std::unexpected(packet_id_result.error());
        message.packet_id = *packet_id_result;
    }

    // Properties (MQTT 5.0)
    auto properties_length_result = buffer.read_variable_int();
    if (!properties_length_result)
        return std::unexpected(properties_length_result.error());

    uint32_t properties_length = *properties_length_result;
    size_t properties_start = buffer.position();

    // Parse properties
    while (buffer.position() < properties_start + properties_length) {
        auto property_id_result = buffer.read<uint8_t>();
        if (!property_id_result)
            break;

        uint8_t property_id = *property_id_result;

        switch (property_id) {
            case 0x02: {  // Message Expiry Interval
                auto value_result = buffer.read<uint32_t>();
                if (value_result)
                    message.message_expiry_interval = *value_result;
                break;
            }
            case 0x08: {  // Response Topic
                auto value_result = buffer.read_string();
                if (value_result)
                    message.response_topic = *value_result;
                break;
            }
            case 0x09: {  // Correlation Data
                auto length_result = buffer.read<uint16_t>();
                if (length_result &&
                    buffer.position() + *length_result <= buffer.size()) {
                    std::vector<uint8_t> data(
                        buffer.data().begin() + buffer.position(),
                        buffer.data().begin() + buffer.position() +
                            *length_result);
                    message.correlation_data = std::move(data);
                }
                break;
            }
            case 0x03: {  // Content Type
                auto value_result = buffer.read_string();
                if (value_result)
                    message.content_type = *value_result;
                break;
            }
            default:
                // Skip unknown properties
                break;
        }
    }

    // Message payload
    size_t payload_start = properties_start + properties_length;
    if (payload_start <= buffer.size()) {
        auto payload_span = buffer.data().subspan(payload_start);
        message.payload.assign(payload_span.begin(), payload_span.end());
    }

    return message;
}

Result<std::vector<ErrorCode>> PacketCodec::parse_suback(
    std::span<const uint8_t> data) {
    if (data.size() < 4) {
        return std::unexpected(ErrorCode::MALFORMED_PACKET);
    }

    BinaryBuffer buffer;
    buffer.write_bytes(data);

    // Skip packet identifier
    auto packet_id_result = buffer.read<uint16_t>();
    if (!packet_id_result)
        return std::unexpected(packet_id_result.error());

    // Skip properties (MQTT 5.0)
    auto properties_length_result = buffer.read_variable_int();
    if (!properties_length_result)
        return std::unexpected(properties_length_result.error());

    uint32_t properties_length = *properties_length_result;
    for (uint32_t i = 0;
         i < properties_length && buffer.position() < buffer.size(); ++i) {
        auto result = buffer.read<uint8_t>();
        if (!result)
            break;
    }

    // Return codes
    std::vector<ErrorCode> return_codes;
    while (buffer.position() < buffer.size()) {
        auto code_result = buffer.read<uint8_t>();
        if (!code_result)
            break;
        return_codes.push_back(static_cast<ErrorCode>(*code_result));
    }

    return return_codes;
}

Result<std::vector<ErrorCode>> PacketCodec::parse_unsuback(
    std::span<const uint8_t> data) {
    return parse_suback(data);  // Same format as SUBACK
}

}  // namespace mqtt
