#pragma once

/**
 * @file http_request.hpp
 * @brief HTTP request parsing utilities
 */

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace atom::extra::asio::sse {

/**
 * @brief HTTP request representation.
 *
 * This struct encapsulates the components of an HTTP request, including
 * the method, path, HTTP version, headers, and body. It also provides
 * utility methods for extracting information relevant to SSE and
 * authentication.
 */
struct HttpRequest {
    /**
     * @brief HTTP method (e.g., "GET", "POST").
     */
    std::string method;

    /**
     * @brief Request path (e.g., "/events").
     */
    std::string path;

    /**
     * @brief HTTP version (e.g., "HTTP/1.1").
     */
    std::string version;

    /**
     * @brief Map of HTTP headers (header name to value).
     */
    std::unordered_map<std::string, std::string> headers;

    /**
     * @brief Request body content.
     */
    std::string body;

    /**
     * @brief Check if the request is for server-sent events.
     * @return True if the request wants events (e.g., Accept:
     * text/event-stream), false otherwise.
     */
    bool want_events() const;

    /**
     * @brief Check if the request contains authentication information.
     * @return True if authentication headers are present, false otherwise.
     */
    bool has_auth() const;

    /**
     * @brief Extract API key from the request headers.
     * @return The API key as a string, or an empty string if not present.
     */
    std::string get_api_key() const;

    /**
     * @brief Extract HTTP Basic authentication credentials.
     * @return A pair of username and password. Both are empty if not present.
     */
    std::pair<std::string, std::string> get_basic_auth() const;

    /**
     * @brief Retrieve the Last-Event-ID header value, if present.
     * @return Optional string containing the Last-Event-ID, or std::nullopt if
     * not present.
     */
    std::optional<std::string> get_last_event_id() const;
};

}  // namespace atom::extra::asio::sse