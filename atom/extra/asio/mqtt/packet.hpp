#pragma once

#include <concepts>
#include <span>
#include "types.hpp"

/**
 * @file packet.hpp
 * @brief MQTT packet types, header, binary buffer, and codec for
 * serialization/deserialization.
 */

namespace mqtt {

/**
 * @enum PacketType
 * @brief Enumerates all MQTT packet types as per the MQTT specification.
 */
enum class PacketType : uint8_t {
    CONNECT = 1,       ///< Client request to connect to Server.
    CONNACK = 2,       ///< Connect acknowledgment.
    PUBLISH = 3,       ///< Publish message.
    PUBACK = 4,        ///< Publish acknowledgment.
    PUBREC = 5,        ///< Publish received (assured delivery part 1).
    PUBREL = 6,        ///< Publish release (assured delivery part 2).
    PUBCOMP = 7,       ///< Publish complete (assured delivery part 3).
    SUBSCRIBE = 8,     ///< Client subscribe request.
    SUBACK = 9,        ///< Subscribe acknowledgment.
    UNSUBSCRIBE = 10,  ///< Unsubscribe request.
    UNSUBACK = 11,     ///< Unsubscribe acknowledgment.
    PINGREQ = 12,      ///< PING request.
    PINGRESP = 13,     ///< PING response.
    DISCONNECT = 14,   ///< Client is disconnecting.
    AUTH = 15          ///< Authentication exchange (MQTT 5.0 only).
};

/**
 * @struct PacketHeader
 * @brief Represents the fixed header of an MQTT packet.
 *
 * Contains the packet type, flags, and remaining length.
 * Provides utility methods for manipulating and querying header flags.
 */
struct PacketHeader {
    PacketType type;               ///< The MQTT packet type.
    uint8_t flags{0};              ///< Flags specific to the packet type.
    uint32_t remaining_length{0};  ///< Remaining length of the packet.

    /**
     * @brief Check if the DUP (duplicate delivery) flag is set.
     * @return True if duplicate flag is set.
     */
    constexpr bool is_duplicate() const noexcept { return (flags & 0x08) != 0; }

    /**
     * @brief Get the QoS (Quality of Service) level from the flags.
     * @return QoS level.
     */
    constexpr QoS get_qos() const noexcept {
        return static_cast<QoS>((flags >> 1) & 0x03);
    }

    /**
     * @brief Check if the RETAIN flag is set.
     * @return True if retain flag is set.
     */
    constexpr bool is_retain() const noexcept { return (flags & 0x01) != 0; }

    /**
     * @brief Set or clear the DUP flag.
     * @param dup True to set, false to clear.
     */
    constexpr void set_duplicate(bool dup) noexcept {
        flags = dup ? (flags | 0x08) : (flags & ~0x08);
    }

    /**
     * @brief Set the QoS level in the flags.
     * @param qos QoS level to set.
     */
    constexpr void set_qos(QoS qos) noexcept {
        flags = (flags & ~0x06) | (static_cast<uint8_t>(qos) << 1);
    }

    /**
     * @brief Set or clear the RETAIN flag.
     * @param retain True to set, false to clear.
     */
    constexpr void set_retain(bool retain) noexcept {
        flags = retain ? (flags | 0x01) : (flags & ~0x01);
    }
};

/**
 * @class BinaryBuffer
 * @brief Efficient binary buffer for MQTT packet construction and parsing.
 *
 * Provides methods for writing and reading various types, strings, and
 * variable-length integers, as well as utility functions for buffer management
 * and range-based operations.
 */
class BinaryBuffer {
private:
    std::vector<uint8_t> data_;  ///< Underlying byte storage.
    size_t position_{0};         ///< Current read/write position.

public:
    /**
     * @brief Default constructor.
     */
    BinaryBuffer() = default;

    /**
     * @brief Construct a buffer with reserved size.
     * @param reserve_size Number of bytes to reserve.
     */
    explicit BinaryBuffer(size_t reserve_size) { data_.reserve(reserve_size); }

    /**
     * @brief Write an integral value to the buffer in big-endian order.
     * @tparam T Integral type.
     * @param value Value to write.
     */
    template <std::integral T>
    void write(T value) {
        if constexpr (sizeof(T) == 1) {
            data_.push_back(static_cast<uint8_t>(value));
        } else {
            // Big-endian encoding
            for (int i = sizeof(T) - 1; i >= 0; --i) {
                data_.push_back(
                    static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }
    }

    /**
     * @brief Write a string to the buffer with a 2-byte length prefix.
     * @param str String view to write.
     */
    void write_string(std::string_view str) {
        write<uint16_t>(static_cast<uint16_t>(str.length()));
        data_.insert(data_.end(), str.begin(), str.end());
    }

    /**
     * @brief Write a sequence of bytes to the buffer.
     * @param bytes Span of bytes to write.
     */
    void write_bytes(std::span<const uint8_t> bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
    }

    /**
     * @brief Write a variable-length integer as per MQTT encoding.
     * @param value Value to encode and write.
     */
    void write_variable_int(uint32_t value) {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value > 0) {
                byte |= 0x80;
            }
            data_.push_back(byte);
        } while (value > 0);
    }

    /**
     * @brief Read an integral value from the buffer in big-endian order.
     * @tparam T Integral type.
     * @return Result containing the value or an error.
     */
    template <std::integral T>
    [[nodiscard]] Result<T> read() {
        if (position_ + sizeof(T) > data_.size()) {
            return std::unexpected(ErrorCode::MALFORMED_PACKET);
        }

        T value{0};
        if constexpr (sizeof(T) == 1) {
            value = static_cast<T>(data_[position_++]);
        } else {
            // Big-endian decoding
            for (size_t i = 0; i < sizeof(T); ++i) {
                value = (value << 8) | data_[position_++];
            }
        }
        return value;
    }

    /**
     * @brief Read a string with a 2-byte length prefix from the buffer.
     * @return Result containing the string or an error.
     */
    [[nodiscard]] Result<std::string> read_string() {
        auto length_result = read<uint16_t>();
        if (!length_result)
            return std::unexpected(length_result.error());

        uint16_t length = *length_result;
        if (position_ + length > data_.size()) {
            return std::unexpected(ErrorCode::MALFORMED_PACKET);
        }

        std::string str(data_.begin() + position_,
                        data_.begin() + position_ + length);
        position_ += length;
        return str;
    }

    /**
     * @brief Read a variable-length integer as per MQTT encoding.
     * @return Result containing the value or an error.
     */
    [[nodiscard]] Result<uint32_t> read_variable_int() {
        uint32_t value = 0;
        uint8_t shift = 0;

        do {
            if (position_ >= data_.size() || shift >= 28) {
                return std::unexpected(ErrorCode::MALFORMED_PACKET);
            }

            uint8_t byte = data_[position_++];
            value |= (byte & 0x7F) << shift;

            if ((byte & 0x80) == 0)
                break;
            shift += 7;
        } while (true);

        return value;
    }

    /**
     * @brief Get a read-only span of the buffer's data.
     * @return Span of bytes.
     */
    [[nodiscard]] std::span<const uint8_t> data() const noexcept {
        return data_;
    }

    /**
     * @brief Get the size of the buffer in bytes.
     * @return Number of bytes in the buffer.
     */
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }

    /**
     * @brief Check if the buffer is empty.
     * @return True if empty.
     */
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    /**
     * @brief Clear the buffer and reset position.
     */
    void clear() noexcept {
        data_.clear();
        position_ = 0;
    }

    /**
     * @brief Reset the read/write position to the beginning.
     */
    void reset_position() noexcept { position_ = 0; }

    /**
     * @brief Get the current read/write position.
     * @return Position index.
     */
    [[nodiscard]] size_t position() const noexcept { return position_; }

    /**
     * @brief Append data from another BinaryBuffer.
     * @param other Buffer to append from.
     */
    void append_from(const BinaryBuffer& other) {
        data_.insert(data_.end(), other.data_.begin(), other.data_.end());
    }
};

/**
 * @class PacketCodec
 * @brief Provides static methods for serializing and deserializing MQTT
 * packets.
 *
 * Handles encoding and decoding of all MQTT packet types, including CONNECT,
 * PUBLISH, SUBSCRIBE, UNSUBSCRIBE, PINGREQ, DISCONNECT, and their corresponding
 * responses.
 */
class PacketCodec {
public:
    // Serialization methods

    /**
     * @brief Serialize a CONNECT packet.
     * @param options Connection options.
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_connect(const ConnectionOptions& options);

    /**
     * @brief Serialize a PUBLISH packet.
     * @param message Message to publish.
     * @param packet_id Optional packet identifier.
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_publish(const Message& message,
                                          uint16_t packet_id = 0);

    /**
     * @brief Serialize a SUBSCRIBE packet.
     * @param subscriptions List of subscriptions.
     * @param packet_id Packet identifier.
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_subscribe(
        const std::vector<Subscription>& subscriptions, uint16_t packet_id);

    /**
     * @brief Serialize an UNSUBSCRIBE packet.
     * @param topics List of topic strings.
     * @param packet_id Packet identifier.
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_unsubscribe(
        const std::vector<std::string>& topics, uint16_t packet_id);

    /**
     * @brief Serialize a PINGREQ packet.
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_pingreq();

    /**
     * @brief Serialize a DISCONNECT packet.
     * @param version MQTT protocol version.
     * @param reason Error code for disconnect (default: SUCCESS).
     * @return BinaryBuffer containing the encoded packet.
     */
    static BinaryBuffer serialize_disconnect(
        ProtocolVersion version, ErrorCode reason = ErrorCode::SUCCESS);

    // Deserialization methods

    /**
     * @brief Parse the fixed header of an MQTT packet.
     * @param data Span of bytes containing the packet.
     * @return Result containing the parsed PacketHeader or an error.
     */
    static Result<PacketHeader> parse_header(std::span<const uint8_t> data);

    /**
     * @brief Parse a CONNACK packet.
     * @param data Span of bytes containing the packet.
     * @param version MQTT protocol version.
     * @return Result containing the error code or an error.
     */
    static Result<ErrorCode> parse_connack(std::span<const uint8_t> data,
                                           ProtocolVersion version);

    /**
     * @brief Parse a PUBLISH packet.
     * @param header Parsed packet header.
     * @param data Span of bytes containing the packet.
     * @return Result containing the parsed Message or an error.
     */
    static Result<Message> parse_publish(const PacketHeader& header,
                                         std::span<const uint8_t> data);

    /**
     * @brief Parse a SUBACK packet.
     * @param data Span of bytes containing the packet.
     * @return Result containing a vector of error codes or an error.
     */
    static Result<std::vector<ErrorCode>> parse_suback(
        std::span<const uint8_t> data);

    /**
     * @brief Parse an UNSUBACK packet.
     * @param data Span of bytes containing the packet.
     * @return Result containing a vector of error codes or an error.
     */
    static Result<std::vector<ErrorCode>> parse_unsuback(
        std::span<const uint8_t> data);

private:
    /**
     * @brief Write MQTT properties for a message to the buffer.
     * @param buffer BinaryBuffer to write to.
     * @param message Message whose properties to write.
     * @param version MQTT protocol version.
     */
    static void write_properties(BinaryBuffer& buffer, const Message& message,
                                 ProtocolVersion version);

    /**
     * @brief Read MQTT properties for a message from the buffer.
     * @param buffer BinaryBuffer to read from.
     * @param message Message to populate.
     * @param version MQTT protocol version.
     * @return Result indicating success or error.
     */
    static Result<void> read_properties(BinaryBuffer& buffer, Message& message,
                                        ProtocolVersion version);
};

}  // namespace mqtt
