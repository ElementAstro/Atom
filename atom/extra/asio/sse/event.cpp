#include "event.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <string_view>

#ifdef ATOM_USE_COMPRESSION
#include <zlib.h>
#endif

#include "atom/type/json.hpp"

namespace atom::extra::asio::sse {

#ifdef ATOM_USE_COMPRESSION
std::string compress_data(const std::string& data) {
    z_stream zs{};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib deflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string compressed;
    compressed.reserve(data.size() / 2);
    constexpr size_t BUFFER_SIZE = 32768;
    char outbuffer[BUFFER_SIZE];

    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = BUFFER_SIZE;

        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw std::runtime_error("Zlib compression error");
        }

        const size_t have = BUFFER_SIZE - zs.avail_out;
        compressed.append(outbuffer, have);
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return compressed;
}

std::string decompress_data(const std::string& data) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib inflate");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string decompressed;
    decompressed.reserve(data.size() * 4);
    constexpr size_t BUFFER_SIZE = 32768;
    char outbuffer[BUFFER_SIZE];

    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = BUFFER_SIZE;

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            inflateEnd(&zs);
            throw std::runtime_error("Zlib decompression error");
        }

        const size_t have = BUFFER_SIZE - zs.avail_out;
        decompressed.append(outbuffer, have);
    } while (zs.avail_out == 0);

    inflateEnd(&zs);
    return decompressed;
}
#endif

Event::Event(std::string id, std::string event_type, std::string data)
    : id_(std::move(id)),
      event_type_(std::move(event_type)),
      data_(std::move(data)),
      timestamp_(std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()) {}

Event::Event(std::string id, std::string event_type, std::string data,
             std::unordered_map<std::string, std::string> meta)
    : id_(std::move(id)),
      event_type_(std::move(event_type)),
      data_(std::move(data)),
      metadata_(std::move(meta)),
      timestamp_(std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()) {}

Event::Event(std::string id, std::string event_type, nlohmann::json json_data)
    : id_(std::move(id)),
      event_type_(std::move(event_type)),
      data_(json_data.dump()),
      timestamp_(std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()),
      is_json_(true) {}

const std::string& Event::id() const noexcept { return id_; }

const std::string& Event::event_type() const noexcept { return event_type_; }

const std::string& Event::data() const noexcept { return data_; }

uint64_t Event::timestamp() const noexcept { return timestamp_; }

bool Event::is_json() const noexcept { return is_json_; }

bool Event::is_compressed() const noexcept { return is_compressed_; }

std::optional<std::string> Event::get_metadata(const std::string& key) const {
    if (auto it = metadata_.find(key); it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Event::add_metadata(std::string key, std::string value) {
    metadata_.emplace(std::move(key), std::move(value));
}

nlohmann::json Event::parse_json() const {
    if (!is_json_) {
        throw std::runtime_error("Event data is not JSON");
    }
    return nlohmann::json::parse(data_);
}

void Event::compress() {
#ifdef ATOM_USE_COMPRESSION
    if (!is_compressed_) {
        data_ = compress_data(data_);
        is_compressed_ = true;
        add_metadata("compressed", "true");
    }
#else
    spdlog::warn("Compression requested but not available");
#endif
}

void Event::decompress() {
#ifdef ATOM_USE_COMPRESSION
    if (is_compressed_) {
        data_ = decompress_data(data_);
        is_compressed_ = false;
        metadata_.erase("compressed");
    }
#else
    spdlog::warn("Decompression requested but not available");
#endif
}

std::string Event::serialize() const {
    std::string result;
    result.reserve(data_.size() + metadata_.size() * 50 + 200);

    if (!id_.empty()) {
        result += "id: ";
        result += id_;
        result += '\n';
    }

    if (!event_type_.empty()) {
        result += "event: ";
        result += event_type_;
        result += '\n';
    }

    for (const auto& [key, value] : metadata_) {
        result += ": ";
        result += key;
        result += '=';
        result += value;
        result += '\n';
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
        std::string_view data_view(data_);
        size_t start = 0;
        size_t pos = 0;

        while ((pos = data_view.find('\n', start)) != std::string_view::npos) {
            result += "data: ";
            result += data_view.substr(start, pos - start);
            result += '\n';
            start = pos + 1;
        }

        if (start < data_view.size()) {
            result += "data: ";
            result += data_view.substr(start);
            result += '\n';
        }
    }

    result += '\n';
    return result;
}

std::optional<Event> Event::deserialize(const std::vector<std::string>& lines) {
    std::string id;
    std::string event_type = "message";
    std::string data;
    std::unordered_map<std::string, std::string> metadata;
    bool is_json = false;
    bool is_compressed = false;

    data.reserve(1024);

    for (const auto& line : lines) {
        if (line.empty())
            continue;

        std::string_view line_view(line);

        if (line_view.starts_with("id:")) {
            id = line_view.substr(3);
            if (!id.empty() && id.front() == ' ') {
                id.erase(0, 1);
            }
        } else if (line_view.starts_with("event:")) {
            event_type = line_view.substr(6);
            if (!event_type.empty() && event_type.front() == ' ') {
                event_type.erase(0, 1);
            }
        } else if (line_view.starts_with("data:")) {
            std::string line_data = std::string(line_view.substr(5));
            if (!line_data.empty() && line_data.front() == ' ') {
                line_data.erase(0, 1);
            }
            if (!data.empty()) {
                data += '\n';
            }
            data += line_data;
        } else if (line_view.starts_with(":")) {
            std::string comment = std::string(line_view.substr(1));
            if (!comment.empty() && comment.front() == ' ') {
                comment.erase(0, 1);
            }

            if (const size_t equals_pos = comment.find('=');
                equals_pos != std::string::npos) {
                std::string key = comment.substr(0, equals_pos);
                std::string value = comment.substr(equals_pos + 1);

                if (key == "content-type" && value == "application/json") {
                    is_json = true;
                } else if (key == "compressed" && value == "true") {
                    is_compressed = true;
                }

                metadata.emplace(std::move(key), std::move(value));
            }
        }
    }

    if (id.empty() || data.empty()) {
        return std::nullopt;
    }

    Event event(std::move(id), std::move(event_type), std::move(data),
                std::move(metadata));
    event.is_json_ = is_json;
    event.is_compressed_ = is_compressed;
    return event;
}

MessageEvent::MessageEvent(std::string id, std::string message)
    : Event(std::move(id), "message", std::move(message)) {}

UpdateEvent::UpdateEvent(std::string id, std::string data)
    : Event(std::move(id), "update", std::move(data)) {}

UpdateEvent::UpdateEvent(std::string id, const nlohmann::json& json_data)
    : Event(std::move(id), "update", json_data) {}

AlertEvent::AlertEvent(std::string id, std::string alert, std::string severity)
    : Event(std::move(id), "alert", std::move(alert)) {
    add_metadata("severity", std::move(severity));
}

HeartbeatEvent::HeartbeatEvent()
    : Event("heartbeat-" +
                std::to_string(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count()),
            std::string("heartbeat"), std::string("ping")) {}

}  // namespace atom::extra::asio::sse