#pragma once

#include <spdlog/spdlog.h>
#include <zlib.h>
#include <chrono>
#include <concepts>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

// Common utilities
#ifdef USE_COMPRESSION
std::string compress_data(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib deflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    int ret;
    char outbuffer[32768];
    std::string compressed;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Error during zlib compression");
    }

    return compressed;
}

std::string decompress_data(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib inflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    int ret;
    char outbuffer[32768];
    std::string decompressed;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, Z_NO_FLUSH);

        if (decompressed.size() < zs.total_out) {
            decompressed.append(outbuffer, zs.total_out - decompressed.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Error during zlib decompression");
    }

    return decompressed;
}
#endif

// C++20 concept for serializable event types
template <typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::string>;
};

template <typename T>
concept EventType = Serializable<T> && requires(T t) {
    { t.id() } -> std::convertible_to<std::string>;
    { t.event_type() } -> std::convertible_to<std::string>;
    { t.data() } -> std::convertible_to<std::string>;
    { t.timestamp() } -> std::convertible_to<uint64_t>;
};

// Enhanced Event class with additional metadata
class Event {
public:
    Event(std::string id, std::string event_type, std::string data)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(std::move(data)),
          timestamp_(
              std::chrono::system_clock::now().time_since_epoch().count()) {}

    Event(std::string id, std::string event_type, std::string data,
          std::unordered_map<std::string, std::string> meta)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(std::move(data)),
          metadata_(std::move(meta)),
          timestamp_(
              std::chrono::system_clock::now().time_since_epoch().count()) {}

    Event(std::string id, std::string event_type, nlohmann::json json_data)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(json_data.dump()),
          timestamp_(
              std::chrono::system_clock::now().time_since_epoch().count()),
          is_json_(true) {}

    virtual ~Event() = default;

    // Accessors
    [[nodiscard]] const std::string& id() const { return id_; }
    [[nodiscard]] const std::string& event_type() const { return event_type_; }
    [[nodiscard]] const std::string& data() const { return data_; }
    [[nodiscard]] uint64_t timestamp() const { return timestamp_; }
    [[nodiscard]] bool is_json() const { return is_json_; }
    [[nodiscard]] bool is_compressed() const { return is_compressed_; }

    // Get metadata value
    [[nodiscard]] std::optional<std::string> get_metadata(
        const std::string& key) const {
        auto it = metadata_.find(key);
        if (it != metadata_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Add metadata
    void add_metadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }

    // Parse JSON data (if applicable)
    [[nodiscard]] nlohmann::json parse_json() const {
        if (!is_json_) {
            throw std::runtime_error("Event data is not JSON");
        }
        return nlohmann::json::parse(data_);
    }

    // Compress the event data
    void compress() {
#ifdef USE_COMPRESSION
        if (!is_compressed_) {
            data_ = compress_data(data_);
            is_compressed_ = true;
            add_metadata("compressed", "true");
        }
#else
        SPDLOG_WARN("Compression requested but not available");
#endif
    }

    // Decompress the event data
    void decompress() {
#ifdef USE_COMPRESSION
        if (is_compressed_) {
            data_ = decompress_data(data_);
            is_compressed_ = false;

            auto it = metadata_.find("compressed");
            if (it != metadata_.end()) {
                metadata_.erase(it);
            }
        }
#else
        SPDLOG_WARN("Decompression requested but not available");
#endif
    }

    // Serialize the event for SSE transmission
    [[nodiscard]] std::string serialize() const {
        std::string result;

        // Add event ID
        if (!id_.empty()) {
            result += "id: " + id_ + "\n";
        }

        // Add event type
        if (!event_type_.empty()) {
            result += "event: " + event_type_ + "\n";
        }

        // Add metadata as commented fields
        for (const auto& [key, value] : metadata_) {
            result += ": " + key + "=" + value + "\n";
        }

        // Add compressed flag if needed
        if (is_compressed_) {
            result += ": compressed=true\n";
        }

        // Add JSON flag if needed
        if (is_json_) {
            result += ": content-type=application/json\n";
        }

        // Add data, handling multiline data properly
        // Use C++20 ranges to process multiline data
        if (is_compressed_) {
            // Encode compressed data in base64
            result += "data: <compressed binary data>\n";
        } else {
            for (const auto& line : std::views::split(data_, '\n')) {
                std::string line_str(line.begin(), line.end());
                result += "data: " + line_str + "\n";
            }
        }

        // Empty line to finish the event
        result += "\n";

        return result;
    }

    // Create an event from serialized SSE data
    static std::optional<Event> deserialize(
        const std::vector<std::string>& lines) {
        std::string id;
        std::string event_type = "message";  // Default type
        std::string data;
        std::unordered_map<std::string, std::string> metadata;
        bool is_json = false;
        bool is_compressed = false;

        for (const auto& line : lines) {
            if (line.empty()) {
                continue;
            }

            if (line.starts_with("id:")) {
                id = line.substr(3);
                if (!id.empty() && id[0] == ' ') {
                    id = id.substr(1);
                }
            } else if (line.starts_with("event:")) {
                event_type = line.substr(6);
                if (!event_type.empty() && event_type[0] == ' ') {
                    event_type = event_type.substr(1);
                }
            } else if (line.starts_with("data:")) {
                std::string line_data = line.substr(5);
                if (!line_data.empty() && line_data[0] == ' ') {
                    line_data = line_data.substr(1);
                }

                if (!data.empty()) {
                    data += "\n";
                }
                data += line_data;
            } else if (line.starts_with(":")) {
                // Parse metadata from comment
                std::string comment = line.substr(1);
                if (!comment.empty() && comment[0] == ' ') {
                    comment = comment.substr(1);
                }

                // Check for key=value format
                size_t equals_pos = comment.find('=');
                if (equals_pos != std::string::npos) {
                    std::string key = comment.substr(0, equals_pos);
                    std::string value = comment.substr(equals_pos + 1);

                    metadata[key] = value;

                    if (key == "content-type" && value == "application/json") {
                        is_json = true;
                    } else if (key == "compressed" && value == "true") {
                        is_compressed = true;
                    }
                }
            }
        }

        // Return nullopt if we don't have enough data
        if (id.empty() || data.empty()) {
            return std::nullopt;
        }

        // Create event
        Event event(id, event_type, data, metadata);

        // Set JSON flag if applicable
        if (is_json) {
            event.is_json_ = true;
        }

        // Set compressed flag if applicable
        if (is_compressed) {
            event.is_compressed_ = true;
        }

        return event;
    }

private:
    std::string id_;
    std::string event_type_;
    mutable std::string data_;  // mutable to allow compression/decompression
    std::unordered_map<std::string, std::string> metadata_;
    uint64_t timestamp_;
    bool is_json_ = false;
    bool is_compressed_ = false;
};

// Specialized event types
class MessageEvent : public Event {
public:
    MessageEvent(const std::string& id, const std::string& message)
        : Event(id, "message", message) {}
};

class UpdateEvent : public Event {
public:
    UpdateEvent(const std::string& id, const std::string& data)
        : Event(id, "update", data) {}

    UpdateEvent(const std::string& id, const nlohmann::json& json_data)
        : Event(id, "update", json_data) {}
};

class AlertEvent : public Event {
public:
    AlertEvent(const std::string& id, const std::string& alert,
               const std::string& severity = "info")
        : Event(id, "alert", alert) {
        add_metadata("severity", severity);
    }
};

class HeartbeatEvent : public Event {
public:
    HeartbeatEvent()
        : Event("heartbeat-" + std::to_string(std::chrono::system_clock::now()
                                                  .time_since_epoch()
                                                  .count()),
                "heartbeat",
                std::string("ping"))  // 显式指定使用 std::string 构造函数
    {}
};