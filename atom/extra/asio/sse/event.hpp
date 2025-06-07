#ifndef ATOM_EXTRA_ASIO_SSE_EVENT_HPP
#define ATOM_EXTRA_ASIO_SSE_EVENT_HPP

/**
 * @file event.hpp
 * @brief Server-Sent Events (SSE) event handling and management
 */

#include <concepts>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json_fwd.hpp"

namespace atom::extra::asio::sse {

#ifdef USE_COMPRESSION
/**
 * @brief Compresses data using zlib.
 * @param data The data to compress.
 * @return Compressed data string.
 * @throws std::runtime_error on compression failure.
 */
std::string compress_data(const std::string& data);

/**
 * @brief Decompresses zlib compressed data.
 * @param data The compressed data.
 * @return Decompressed data string.
 * @throws std::runtime_error on decompression failure.
 */
std::string decompress_data(const std::string& data);
#endif

/**
 * @brief Concept for types that can be serialized to a string.
 * @tparam T The type to check.
 */
template <typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::string>;
};

/**
 * @brief Concept for event types that provide required event interface.
 * @tparam T The type to check.
 */
template <typename T>
concept EventType = Serializable<T> && requires(T t) {
    { t.id() } -> std::convertible_to<std::string>;
    { t.event_type() } -> std::convertible_to<std::string>;
    { t.data() } -> std::convertible_to<std::string>;
    { t.timestamp() } -> std::convertible_to<uint64_t>;
};

/**
 * @class Event
 * @brief Represents a Server-Sent Event with metadata and payload.
 *
 * This class encapsulates all information for a single SSE event, including
 * its unique identifier, type, data payload, metadata, timestamp, and flags
 * indicating whether the data is JSON or compressed.
 */
class Event {
public:
    /**
     * @brief Constructs an Event with string data.
     * @param id The unique event identifier.
     * @param event_type The type of the event.
     * @param data The event payload as a string.
     */
    Event(std::string id, std::string event_type, std::string data);

    /**
     * @brief Constructs an Event with string data and metadata.
     * @param id The unique event identifier.
     * @param event_type The type of the event.
     * @param data The event payload as a string.
     * @param meta Metadata key-value pairs.
     */
    Event(std::string id, std::string event_type, std::string data,
          std::unordered_map<std::string, std::string> meta);

    /**
     * @brief Constructs an Event with JSON data.
     * @param id The unique event identifier.
     * @param event_type The type of the event.
     * @param json_data The event payload as JSON.
     */
    Event(std::string id, std::string event_type, nlohmann::json json_data);

    virtual ~Event() = default;

    /**
     * @brief Gets the event's unique identifier.
     * @return The event ID.
     */
    [[nodiscard]] const std::string& id() const noexcept;

    /**
     * @brief Gets the event type.
     * @return The event type string.
     */
    [[nodiscard]] const std::string& event_type() const noexcept;

    /**
     * @brief Gets the event data as a string.
     * @return The event data.
     */
    [[nodiscard]] const std::string& data() const noexcept;

    /**
     * @brief Gets the event's timestamp (nanoseconds since epoch).
     * @return The event timestamp.
     */
    [[nodiscard]] uint64_t timestamp() const noexcept;

    /**
     * @brief Checks if the event data is JSON.
     * @return True if the data is JSON, false otherwise.
     */
    [[nodiscard]] bool is_json() const noexcept;

    /**
     * @brief Checks if the event data is compressed.
     * @return True if the data is compressed, false otherwise.
     */
    [[nodiscard]] bool is_compressed() const noexcept;

    /**
     * @brief Retrieves a metadata value by key.
     * @param key The metadata key.
     * @return The value if present, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::string> get_metadata(
        const std::string& key) const;

    /**
     * @brief Adds or updates a metadata key-value pair.
     * @param key The metadata key.
     * @param value The metadata value.
     */
    void add_metadata(std::string key, std::string value);

    /**
     * @brief Parses the event data as JSON.
     * @return The parsed JSON object.
     * @throws std::runtime_error if the data is not valid JSON.
     */
    [[nodiscard]] nlohmann::json parse_json() const;

    /**
     * @brief Compresses the event data (if supported).
     */
    void compress();

    /**
     * @brief Decompresses the event data (if supported).
     */
    void decompress();

    /**
     * @brief Serializes the event to a string in SSE format.
     * @return The serialized event string.
     */
    [[nodiscard]] std::string serialize() const;

    /**
     * @brief Deserializes an Event from a sequence of SSE lines.
     * @param lines The lines representing the event.
     * @return The deserialized Event if successful, std::nullopt otherwise.
     */
    static std::optional<Event> deserialize(
        const std::vector<std::string>& lines);

private:
    std::string id_;            ///< Unique event identifier.
    std::string event_type_;    ///< Event type string.
    mutable std::string data_;  ///< Event data payload.
    std::unordered_map<std::string, std::string> metadata_;  ///< Metadata map.
    uint64_t timestamp_;    ///< Event timestamp (nanoseconds since epoch).
    bool is_json_ = false;  ///< True if data is JSON.
    bool is_compressed_ = false;  ///< True if data is compressed.
};

/**
 * @class MessageEvent
 * @brief Specialized event type for plain messages.
 */
class MessageEvent final : public Event {
public:
    /**
     * @brief Constructs a MessageEvent.
     * @param id The event ID.
     * @param message The message payload.
     */
    MessageEvent(std::string id, std::string message);
};

/**
 * @class UpdateEvent
 * @brief Specialized event type for update messages.
 */
class UpdateEvent final : public Event {
public:
    /**
     * @brief Constructs an UpdateEvent with string data.
     * @param id The event ID.
     * @param data The update data.
     */
    UpdateEvent(std::string id, std::string data);

    /**
     * @brief Constructs an UpdateEvent with JSON data.
     * @param id The event ID.
     * @param json_data The update data as JSON.
     */
    UpdateEvent(std::string id, const nlohmann::json& json_data);
};

/**
 * @class AlertEvent
 * @brief Specialized event type for alerts.
 */
class AlertEvent final : public Event {
public:
    /**
     * @brief Constructs an AlertEvent.
     * @param id The event ID.
     * @param alert The alert message.
     * @param severity The alert severity (default: "info").
     */
    AlertEvent(std::string id, std::string alert,
               std::string severity = "info");
};

/**
 * @class HeartbeatEvent
 * @brief Specialized event type for heartbeat/ping events.
 */
class HeartbeatEvent final : public Event {
public:
    /**
     * @brief Constructs a HeartbeatEvent.
     */
    HeartbeatEvent();
};

}  // namespace atom::extra::asio::sse

#endif  // ATOM_EXTRA_ASIO_SSE_EVENT_HPP