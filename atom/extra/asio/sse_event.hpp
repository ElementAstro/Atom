#pragma once

/**
 * @file sse_event.hpp
 * @brief Server-Sent Events (SSE) event handling and management
 */

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

#ifdef USE_COMPRESSION
/**
 * @brief Compresses data using zlib
 * @param data The data to compress
 * @return Compressed data string
 * @throws std::runtime_error on compression failure
 */
inline std::string compress_data(const std::string& data) {
    z_stream zs{};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib deflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string compressed;
    compressed.reserve(data.size());
    char outbuffer[32768];

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        if (deflate(&zs, Z_FINISH) == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw std::runtime_error("Zlib compression error");
        }

        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return compressed;
}

/**
 * @brief Decompresses zlib compressed data
 * @param data The compressed data
 * @return Decompressed data string
 * @throws std::runtime_error on decompression failure
 */
inline std::string decompress_data(const std::string& data) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib inflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string decompressed;
    decompressed.reserve(data.size() * 2);
    char outbuffer[32768];

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        if (inflate(&zs, Z_NO_FLUSH) == Z_STREAM_ERROR) {
            inflateEnd(&zs);
            throw std::runtime_error("Zlib decompression error");
        }

        if (decompressed.size() < zs.total_out) {
            decompressed.append(outbuffer, zs.total_out - decompressed.size());
        }
    } while (zs.avail_out == 0);

    inflateEnd(&zs);
    return decompressed;
}
#endif

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

/**
 * @brief Represents a Server-Sent Event with metadata and payload
 */
class Event {
public:
    Event(std::string id, std::string event_type, std::string data)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(std::move(data)),
          timestamp_(std::chrono::system_clock::now().time_since_epoch().count()) {}

    Event(std::string id, std::string event_type, std::string data,
          std::unordered_map<std::string, std::string> meta)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(std::move(data)),
          metadata_(std::move(meta)),
          timestamp_(std::chrono::system_clock::now().time_since_epoch().count()) {}

    Event(std::string id, std::string event_type, nlohmann::json json_data)
        : id_(std::move(id)),
          event_type_(std::move(event_type)),
          data_(json_data.dump()),
          timestamp_(std::chrono::system_clock::now().time_since_epoch().count()),
          is_json_(true) {}

    virtual ~Event() = default;

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& event_type() const noexcept { return event_type_; }
    [[nodiscard]] const std::string& data() const noexcept { return data_; }
    [[nodiscard]] uint64_t timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] bool is_json() const noexcept { return is_json_; }
    [[nodiscard]] bool is_compressed() const noexcept { return is_compressed_; }

    [[nodiscard]] std::optional<std::string> get_metadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return it != metadata_.end() ? std::optional{it->second} : std::nullopt;
    }

    void add_metadata(std::string key, std::string value) {
        metadata_.emplace(std::move(key), std::move(value));
    }

    [[nodiscard]] nlohmann::json parse_json() const {
        if (!is_json_) {
            throw std::runtime_error("Event data is not JSON");
        }
        return nlohmann::json::parse(data_);
    }

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

    void decompress() {
#ifdef USE_COMPRESSION
        if (is_compressed_) {
            data_ = decompress_data(data_);
            is_compressed_ = false;
            metadata_.erase("compressed");
        }
#else
        SPDLOG_WARN("Decompression requested but not available");
#endif
    }

    [[nodiscard]] std::string serialize() const {
        std::string result;
        result.reserve(data_.size() + 100);

        if (!id_.empty()) {
            result += "id: " + id_ + "\n";
        }

        if (!event_type_.empty()) {
            result += "event: " + event_type_ + "\n";
        }

        for (const auto& [key, value] : metadata_) {
            result += ": " + key + "=" + value + "\n";
        }

        if (is_compressed_) {
            result += ": compressed=true\n";
        }

        if (is_json_) {
            result += ": content-type=application/json\n";
        }

        if (is_compressed_) {
            result += "data: <compressed binary data>\n";
        } else {
            for (const auto& line : std::views::split(data_, '\n')) {
                result += "data: ";
                result.append(line.begin(), line.end());
                result += "\n";
            }
        }

        result += "\n";
        return result;
    }

    static std::optional<Event> deserialize(const std::vector<std::string>& lines) {
        std::string id;
        std::string event_type = "message";
        std::string data;
        std::unordered_map<std::string, std::string> metadata;
        bool is_json = false;
        bool is_compressed = false;

        for (const auto& line : lines) {
            if (line.empty()) continue;

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
                std::string comment = line.substr(1);
                if (!comment.empty() && comment[0] == ' ') {
                    comment = comment.substr(1);
                }

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

        if (id.empty() || data.empty()) {
            return std::nullopt;
        }

        Event event(id, event_type, data, metadata);
        event.is_json_ = is_json;
        event.is_compressed_ = is_compressed;
        return event;
    }

private:
    std::string id_;
    std::string event_type_;
    mutable std::string data_;
    std::unordered_map<std::string, std::string> metadata_;
    uint64_t timestamp_;
    bool is_json_ = false;
    bool is_compressed_ = false;
};

class MessageEvent final : public Event {
public:
    MessageEvent(std::string id, std::string message)
        : Event(std::move(id), "message", std::move(message)) {}
};

class UpdateEvent final : public Event {
public:
    UpdateEvent(std::string id, std::string data)
        : Event(std::move(id), "update", std::move(data)) {}

    UpdateEvent(std::string id, const nlohmann::json& json_data)
        : Event(std::move(id), "update", json_data) {}
};

class AlertEvent final : public Event {
public:
    AlertEvent(std::string id, std::string alert, std::string severity = "info")
        : Event(std::move(id), "alert", std::move(alert)) {
        add_metadata("severity", std::move(severity));
    }
};

class HeartbeatEvent final : public Event {
public:
    HeartbeatEvent()
        : Event(
              "heartbeat-" +
              std::to_string(std::chrono::system_clock::now().time_since_epoch().count()),
              "heartbeat",
              "ping") {}
};