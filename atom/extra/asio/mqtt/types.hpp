#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mqtt {

/**
 * @enum ProtocolVersion
 * @brief Supported MQTT protocol versions.
 *
 * Enumerates the MQTT protocol versions supported by the library.
 */
enum class ProtocolVersion : uint8_t {
    V3_1_1 = 4,  ///< MQTT version 3.1.1
    V5_0 = 5     ///< MQTT version 5.0
};

/**
 * @enum QoS
 * @brief Quality of Service levels for MQTT message delivery.
 *
 * Specifies the delivery guarantees for MQTT messages.
 */
enum class QoS : uint8_t {
    AT_MOST_ONCE = 0,   ///< Fire and forget (QoS 0)
    AT_LEAST_ONCE = 1,  ///< Acknowledged delivery (QoS 1)
    EXACTLY_ONCE = 2    ///< Assured delivery (QoS 2)
};

/**
 * @enum ErrorCode
 * @brief MQTT error and return codes.
 *
 * Represents standard MQTT error codes and MQTT 5.0 specific codes.
 */
enum class ErrorCode : uint8_t {
    SUCCESS = 0,
    CONNECTION_REFUSED_PROTOCOL = 1,
    CONNECTION_REFUSED_IDENTIFIER = 2,
    CONNECTION_REFUSED_SERVER_UNAVAILABLE = 3,
    CONNECTION_REFUSED_BAD_CREDENTIALS = 4,
    CONNECTION_REFUSED_NOT_AUTHORIZED = 5,
    UNSPECIFIED_ERROR = 128,
    MALFORMED_PACKET = 129,
    PROTOCOL_ERROR = 130,
    IMPLEMENTATION_SPECIFIC = 131,
    UNSUPPORTED_PROTOCOL_VERSION = 132,
    CLIENT_IDENTIFIER_NOT_VALID = 133,
    BAD_USER_NAME_OR_PASSWORD = 134,
    NOT_AUTHORIZED = 135,
    SERVER_UNAVAILABLE = 136,
    SERVER_BUSY = 137,
    BANNED = 138,
    BAD_AUTHENTICATION_METHOD = 140,
    TOPIC_FILTER_INVALID = 143,
    TOPIC_NAME_INVALID = 144,
    PACKET_IDENTIFIER_IN_USE = 145,
    PACKET_IDENTIFIER_NOT_FOUND = 146,
    RECEIVE_MAXIMUM_EXCEEDED = 147,
    TOPIC_ALIAS_INVALID = 148,
    PACKET_TOO_LARGE = 149,
    MESSAGE_RATE_TOO_HIGH = 150,
    QUOTA_EXCEEDED = 151,
    ADMINISTRATIVE_ACTION = 152,
    PAYLOAD_FORMAT_INVALID = 153
};

/**
 * @struct ConnectionOptions
 * @brief Configuration options for establishing an MQTT connection.
 *
 * Contains all parameters required to connect to an MQTT broker, including
 * authentication, session, will message, protocol version, and TLS settings.
 */
struct ConnectionOptions {
    std::string client_id;                ///< Unique client identifier.
    std::string username;                 ///< Username for authentication.
    std::string password;                 ///< Password for authentication.
    std::chrono::seconds keep_alive{60};  ///< Keep-alive interval in seconds.
    bool clean_session{true};             ///< Whether to start a clean session.
    std::optional<std::string> will_topic;  ///< Topic for the will message.
    std::optional<std::vector<uint8_t>>
        will_payload;                 ///< Payload for the will message.
    QoS will_qos{QoS::AT_MOST_ONCE};  ///< QoS for the will message.
    bool will_retain{false};          ///< Retain flag for the will message.
    ProtocolVersion version{ProtocolVersion::V5_0};  ///< MQTT protocol version.

    // TLS Options
    bool use_tls{false};            ///< Enable TLS/SSL.
    std::string ca_cert_file;       ///< Path to CA certificate file.
    std::string cert_file;          ///< Path to client certificate file.
    std::string private_key_file;   ///< Path to private key file.
    bool verify_certificate{true};  ///< Whether to verify server certificate.
};

/**
 * @struct Message
 * @brief Represents an MQTT message.
 *
 * Contains topic, payload, QoS, retain flag, packet ID, and MQTT 5.0
 * properties.
 */
struct Message {
    std::string topic;             ///< Topic name.
    std::vector<uint8_t> payload;  ///< Message payload.
    QoS qos{QoS::AT_MOST_ONCE};    ///< Quality of Service level.
    bool retain{false};            ///< Retain flag.
    uint16_t packet_id{0};         ///< Packet identifier.

    // MQTT 5.0 properties
    std::optional<uint32_t>
        message_expiry_interval;                ///< Expiry interval in seconds.
    std::optional<std::string> response_topic;  ///< Response topic.
    std::optional<std::vector<uint8_t>>
        correlation_data;                     ///< Correlation data.
    std::optional<std::string> content_type;  ///< Content type.
};

/**
 * @struct Subscription
 * @brief Represents a subscription to an MQTT topic filter.
 *
 * Contains topic filter, QoS, and MQTT 5.0 subscription options.
 */
struct Subscription {
    std::string topic_filter;    ///< Topic filter to subscribe to.
    QoS qos{QoS::AT_MOST_ONCE};  ///< Requested QoS level.
    bool no_local{false};  ///< Do not receive own publications (MQTT 5.0).
    bool retain_as_published{false};  ///< Retain as published flag (MQTT 5.0).
    uint8_t retain_handling{0};       ///< Retain handling option (MQTT 5.0).
};

/**
 * @struct ClientStats
 * @brief Statistics for an MQTT client session.
 *
 * Tracks message and byte counts, connection time, and reconnect attempts.
 */
struct ClientStats {
    uint64_t messages_sent{0};      ///< Number of messages sent.
    uint64_t messages_received{0};  ///< Number of messages received.
    uint64_t bytes_sent{0};         ///< Number of bytes sent.
    uint64_t bytes_received{0};     ///< Number of bytes received.
    std::chrono::steady_clock::time_point
        connected_since;          ///< Time point when connected.
    uint32_t reconnect_count{0};  ///< Number of reconnect attempts.
};

/**
 * @brief Result type alias for operations that may fail with an ErrorCode.
 * @tparam T The value type on success.
 */
template <typename T>
using Result = std::expected<T, ErrorCode>;

/**
 * @brief Callback type for handling received messages.
 */
using MessageHandler = std::function<void(const Message&)>;

/**
 * @brief Callback type for connection events.
 */
using ConnectionHandler = std::function<void(ErrorCode)>;

/**
 * @brief Callback type for disconnection events.
 */
using DisconnectionHandler = std::function<void(ErrorCode)>;

}  // namespace mqtt
