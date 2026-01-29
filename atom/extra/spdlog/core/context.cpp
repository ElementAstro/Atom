#include "context.h"

#include <sstream>

namespace modern_log {

std::string LogContext::to_json() const {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    auto add_field = [&](const std::string& key, const std::string& value) {
        if (!value.empty()) {
            if (!first)
                oss << ",";
            oss << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
    };

    add_field("user_id", user_id_);
    add_field("session_id", session_id_);
    add_field("trace_id", trace_id_);
    add_field("request_id", request_id_);

    for (const auto& [key, value] : custom_fields_) {
        if (!first)
            oss << ",";
        oss << "\"" << key << "\":";

        if (value.type() == typeid(std::string)) {
            oss << "\"" << std::any_cast<std::string>(value) << "\"";
        } else if (value.type() == typeid(int)) {
            oss << std::any_cast<int>(value);
        } else if (value.type() == typeid(double)) {
            oss << std::any_cast<double>(value);
        } else if (value.type() == typeid(bool)) {
            oss << (std::any_cast<bool>(value) ? "true" : "false");
        } else {
            oss << "null";
        }
        first = false;
    }

    oss << "}";
    return oss.str();
}

LogContext LogContext::merge(const LogContext& other) const {
    LogContext result = *this;

    if (!other.user_id_.empty())
        result.user_id_ = other.user_id_;
    if (!other.session_id_.empty())
        result.session_id_ = other.session_id_;
    if (!other.trace_id_.empty())
        result.trace_id_ = other.trace_id_;
    if (!other.request_id_.empty())
        result.request_id_ = other.request_id_;

    for (const auto& [key, value] : other.custom_fields_) {
        result.custom_fields_[key] = value;
    }

    return result;
}

void LogContext::clear() {
    user_id_.clear();
    session_id_.clear();
    trace_id_.clear();
    request_id_.clear();
    custom_fields_.clear();
}

bool LogContext::empty() const {
    return user_id_.empty() && session_id_.empty() && trace_id_.empty() &&
           request_id_.empty() && custom_fields_.empty();
}

}  // namespace modern_log
