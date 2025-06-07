#include "http_request.hpp"

namespace atom::extra::asio::sse {

bool HttpRequest::want_events() const {
    auto it = headers.find("Accept");
    return it != headers.end() &&
           it->second.find("text/event-stream") != std::string::npos;
}

bool HttpRequest::has_auth() const {
    return headers.find("Authorization") != headers.end();
}

std::string HttpRequest::get_api_key() const {
    auto it = headers.find("X-API-Key");
    if (it != headers.end()) {
        return it->second;
    }

    it = headers.find("Authorization");
    if (it != headers.end() && it->second.starts_with("Bearer ")) {
        return it->second.substr(7);
    }

    return "";
}

std::pair<std::string, std::string> HttpRequest::get_basic_auth() const {
    auto it = headers.find("Authorization");
    if (it != headers.end() && it->second.starts_with("Basic ")) {
        std::string encoded = it->second.substr(6);
        // Note: In a real implementation, you'd decode base64 here
        std::string decoded = "user:pass";  // Placeholder

        size_t colon_pos = decoded.find(':');
        if (colon_pos != std::string::npos) {
            return {decoded.substr(0, colon_pos),
                    decoded.substr(colon_pos + 1)};
        }
    }

    return {"", ""};
}

std::optional<std::string> HttpRequest::get_last_event_id() const {
    auto it = headers.find("Last-Event-ID");
    if (it != headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

}  // namespace atom::extra::asio::sse