#include "context.h"

#include <format>
#include <functional>

namespace modern_log {

std::string LogContext::to_json() const {
    if (json_cache_valid_) {
        return cached_json_;
    }

    std::string result;
    to_json_fast(result);
    cached_json_ = result;
    json_cache_valid_ = true;
    return result;
}

void LogContext::to_json_fast(std::string& buffer) const {
    buffer.clear();
    buffer.reserve(256); // Pre-allocate reasonable size

    buffer += "{";
    bool first = true;

    auto add_field = [&](std::string_view key, std::string_view value) {
        if (!value.empty()) {
            if (!first) buffer += ",";
            buffer += std::format("\"{}\":\"{}\"", key, value);
            first = false;
        }
    };

    add_field("user_id", user_id_);
    add_field("session_id", session_id_);
    add_field("trace_id", trace_id_);
    add_field("request_id", request_id_);

    for (const auto& [key, value] : custom_fields_) {
        if (!first) buffer += ",";
        buffer += std::format("\"{}\":", key);

        if (value.type() == typeid(std::string)) {
            buffer += std::format("\"{}\"", std::any_cast<std::string>(value));
        } else if (value.type() == typeid(int)) {
            buffer += std::format("{}", std::any_cast<int>(value));
        } else if (value.type() == typeid(double)) {
            buffer += std::format("{}", std::any_cast<double>(value));
        } else if (value.type() == typeid(bool)) {
            buffer += std::any_cast<bool>(value) ? "true" : "false";
        } else {
            buffer += "null";
        }
        first = false;
    }

    buffer += "}";
}

std::string_view LogContext::to_json_view() const {
    if (!json_cache_valid_) {
        to_json(); // This will populate the cache
    }
    return cached_json_;
}

LogContext LogContext::merge(const LogContext& other) const {
    LogContext result = *this;
    result.merge_inplace(other);
    return result;
}

LogContext& LogContext::merge_inplace(const LogContext& other) {
    if (!other.user_id_.empty())
        user_id_ = other.user_id_;
    if (!other.session_id_.empty())
        session_id_ = other.session_id_;
    if (!other.trace_id_.empty())
        trace_id_ = other.trace_id_;
    if (!other.request_id_.empty())
        request_id_ = other.request_id_;

    for (const auto& [key, value] : other.custom_fields_) {
        custom_fields_[key] = value;
    }

    invalidate_caches();
    return *this;
}

void LogContext::clear() {
    user_id_.clear();
    session_id_.clear();
    trace_id_.clear();
    request_id_.clear();
    custom_fields_.clear();
    invalidate_caches();
}

bool LogContext::empty() const {
    return user_id_.empty() && session_id_.empty() && trace_id_.empty() &&
           request_id_.empty() && custom_fields_.empty();
}

size_t LogContext::hash() const {
    if (hash_cache_valid_) {
        return hash_cache_;
    }

    size_t h1 = std::hash<std::string>{}(user_id_);
    size_t h2 = std::hash<std::string>{}(session_id_);
    size_t h3 = std::hash<std::string>{}(trace_id_);
    size_t h4 = std::hash<std::string>{}(request_id_);

    // Combine hashes using a simple but effective method
    hash_cache_ = h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);

    // Add custom fields to hash
    for (const auto& [key, value] : custom_fields_) {
        size_t key_hash = std::hash<std::string>{}(key);
        hash_cache_ ^= key_hash << 4;
    }

    hash_cache_valid_ = true;
    return hash_cache_;
}

bool LogContext::equals_fast(const LogContext& other) const {
    // Quick hash comparison first
    if (hash() != other.hash()) {
        return false;
    }

    // Detailed comparison
    return user_id_ == other.user_id_ &&
           session_id_ == other.session_id_ &&
           trace_id_ == other.trace_id_ &&
           request_id_ == other.request_id_ &&
           custom_fields_ == other.custom_fields_;
}

}  // namespace modern_log
