/*
 * httpparser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-3

Description: Http Header Parser with C++20 features

**************************************************/

#ifndef ATOM_WEB_HTTP_HTTPPARSER_HPP
#define ATOM_WEB_HTTP_HTTPPARSER_HPP

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Windows compatibility: Undefine problematic macros
#ifdef _WIN32
#ifdef DELETE
#undef DELETE
#endif
#endif

namespace atom::web {

/**
 * @brief HTTP method enumeration
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE_,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT,
    UNKNOWN
};

/**
 * @brief Convert HttpMethod to string.
 * @param method The HTTP method.
 * @return String representation.
 */
[[nodiscard]] constexpr auto httpMethodToString(HttpMethod method) noexcept
    -> std::string_view {
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE_:
            return "DELETE";
        case HttpMethod::HEAD:
            return "HEAD";
        case HttpMethod::OPTIONS:
            return "OPTIONS";
        case HttpMethod::PATCH:
            return "PATCH";
        case HttpMethod::TRACE:
            return "TRACE";
        case HttpMethod::CONNECT:
            return "CONNECT";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Parse string to HttpMethod.
 * @param str The string to parse.
 * @return The HTTP method.
 */
[[nodiscard]] auto stringToHttpMethod(std::string_view str) noexcept
    -> HttpMethod;

/**
 * @brief HTTP version enumeration
 */
enum class HttpVersion { HTTP_1_0, HTTP_1_1, HTTP_2_0, HTTP_3_0, UNKNOWN };

/**
 * @brief Convert HttpVersion to string.
 * @param version The HTTP version.
 * @return String representation.
 */
[[nodiscard]] constexpr auto httpVersionToString(HttpVersion version) noexcept
    -> std::string_view {
    switch (version) {
        case HttpVersion::HTTP_1_0:
            return "HTTP/1.0";
        case HttpVersion::HTTP_1_1:
            return "HTTP/1.1";
        case HttpVersion::HTTP_2_0:
            return "HTTP/2.0";
        case HttpVersion::HTTP_3_0:
            return "HTTP/3.0";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Parse string to HttpVersion.
 * @param str The string to parse.
 * @return The HTTP version.
 */
[[nodiscard]] auto stringToHttpVersion(std::string_view str) noexcept
    -> HttpVersion;

/**
 * @brief HTTP status code and description structure
 */
struct HttpStatus {
    int code{0};
    std::string description;

    [[nodiscard]] auto isInformational() const noexcept -> bool {
        return code >= 100 && code < 200;
    }

    [[nodiscard]] auto isSuccess() const noexcept -> bool {
        return code >= 200 && code < 300;
    }

    [[nodiscard]] auto isRedirection() const noexcept -> bool {
        return code >= 300 && code < 400;
    }

    [[nodiscard]] auto isClientError() const noexcept -> bool {
        return code >= 400 && code < 500;
    }

    [[nodiscard]] auto isServerError() const noexcept -> bool {
        return code >= 500;
    }

    [[nodiscard]] auto isError() const noexcept -> bool { return code >= 400; }

    /**
     * @brief Get standard description for HTTP status code.
     * @param statusCode The HTTP status code.
     * @return Standard description.
     */
    [[nodiscard]] static auto getStandardDescription(int statusCode)
        -> std::string;
};

/**
 * @brief Cookie structure representing an HTTP Cookie
 */
struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    std::string expires;
    std::optional<std::chrono::seconds> maxAge;
    bool secure{false};
    bool httpOnly{false};
    std::string sameSite;
    std::optional<std::chrono::system_clock::time_point> expiresTime;

    /**
     * @brief Check if the cookie has expired.
     * @return True if expired.
     */
    [[nodiscard]] auto isExpired() const -> bool;

    /**
     * @brief Check if the cookie is session-only.
     * @return True if session cookie.
     */
    [[nodiscard]] auto isSession() const noexcept -> bool {
        return !maxAge.has_value() && expires.empty();
    }

    /**
     * @brief Convert cookie to Set-Cookie header value.
     * @return The header value string.
     */
    [[nodiscard]] auto toSetCookieString() const -> std::string;

    /**
     * @brief Parse a Set-Cookie header value.
     * @param setCookieValue The header value.
     * @return Parsed cookie.
     */
    [[nodiscard]] static auto parse(std::string_view setCookieValue) -> Cookie;
};

/**
 * @brief The HttpHeaderParser class is responsible for parsing and manipulating
 * HTTP headers.
 */
class HttpHeaderParser {
public:
    /**
     * @brief Constructs a new HttpHeaderParser object.
     */
    HttpHeaderParser();

    /**
     * @brief Destructor.
     */
    ~HttpHeaderParser();

    HttpHeaderParser(const HttpHeaderParser& other);
    HttpHeaderParser& operator=(const HttpHeaderParser& other);
    HttpHeaderParser(HttpHeaderParser&& other) noexcept;
    HttpHeaderParser& operator=(HttpHeaderParser&& other) noexcept;

    /**
     * @brief Parses the raw HTTP headers and stores them internally.
     * @param rawHeaders The raw HTTP headers as a string.
     * @return True if parsing succeeded.
     */
    auto parseHeaders(std::string_view rawHeaders) -> bool;

    /**
     * @brief Parse a complete HTTP request
     * @param rawRequest The raw HTTP request string
     * @return Whether parsing was successful
     */
    [[nodiscard]] auto parseRequest(std::string_view rawRequest) -> bool;

    /**
     * @brief Parse a complete HTTP response
     * @param rawResponse The raw HTTP response string
     * @return Whether parsing was successful
     */
    [[nodiscard]] auto parseResponse(std::string_view rawResponse) -> bool;

    /**
     * @brief Sets the value of a specific header field.
     * @param key The key of the header field.
     * @param value The value to set.
     * @return Reference to this for chaining.
     */
    auto setHeaderValue(std::string_view key,
                        std::string_view value) -> HttpHeaderParser&;

    /**
     * @brief Sets multiple header fields at once.
     * @param headers A map containing header fields and their values.
     * @return Reference to this for chaining.
     */
    auto setHeaders(const std::map<std::string, std::vector<std::string>>&
                        headers) -> HttpHeaderParser&;

    /**
     * @brief Adds a new value to an existing header field.
     * @param key The key of the header field.
     * @param value The value to add.
     * @return Reference to this for chaining.
     */
    auto addHeaderValue(std::string_view key,
                        std::string_view value) -> HttpHeaderParser&;

    /**
     * @brief Retrieves the values of a specific header field.
     * @param key The key of the header field.
     * @return A vector containing the values of the header field.
     */
    [[nodiscard]] auto getHeaderValues(std::string_view key) const
        -> std::optional<std::vector<std::string>>;

    /**
     * @brief Gets the first value of a header field
     * @param key The header field name
     * @return The first header field value, or nullopt if it doesn't exist
     */
    [[nodiscard]] auto getHeaderValue(std::string_view key) const
        -> std::optional<std::string>;

    /**
     * @brief Removes a specific header field.
     * @param key The key of the header field to remove.
     * @return Reference to this for chaining.
     */
    auto removeHeader(std::string_view key) -> HttpHeaderParser&;

    /**
     * @brief Retrieves all the parsed headers.
     * @return A map containing all the parsed headers.
     */
    [[nodiscard]] auto getAllHeaders() const
        -> std::map<std::string, std::vector<std::string>>;

    /**
     * @brief Checks if a specific header field exists.
     * @param key The key of the header field to check.
     * @return True if the header field exists, false otherwise.
     */
    [[nodiscard]] auto hasHeader(std::string_view key) const noexcept -> bool;

    /**
     * @brief Get the number of headers.
     * @return Number of header fields.
     */
    [[nodiscard]] auto headerCount() const noexcept -> size_t;

    /**
     * @brief Clears all the parsed headers.
     * @return Reference to this for chaining.
     */
    auto clearHeaders() -> HttpHeaderParser&;

    /**
     * @brief Add a Cookie
     * @param cookie The Cookie object to add
     * @return Reference to this for chaining.
     */
    auto addCookie(const Cookie& cookie) -> HttpHeaderParser&;

    /**
     * @brief Parse a Cookie string
     * @param cookieStr The Cookie string
     * @return A map of cookie name-value pairs
     */
    [[nodiscard]] auto parseCookies(std::string_view cookieStr) const
        -> std::map<std::string, std::string>;

    /**
     * @brief Get all Cookies
     * @return A list of all cookies
     */
    [[nodiscard]] auto getAllCookies() const -> std::vector<Cookie>;

    /**
     * @brief Get a cookie with the specified name
     * @param name The cookie name
     * @return The Cookie object if it exists
     */
    [[nodiscard]] auto getCookie(std::string_view name) const
        -> std::optional<Cookie>;

    /**
     * @brief Remove a specific cookie
     * @param name The name of the cookie to remove
     * @return Reference to this for chaining.
     */
    auto removeCookie(std::string_view name) -> HttpHeaderParser&;

    /**
     * @brief Get the number of cookies.
     * @return Number of cookies.
     */
    [[nodiscard]] auto cookieCount() const noexcept -> size_t;

    /**
     * @brief Parse URL query parameters
     * @param url The URL containing query parameters
     * @return A map of parsed parameter name-value pairs
     */
    [[nodiscard]] auto parseUrlParameters(std::string_view url) const
        -> std::map<std::string, std::string>;

    /**
     * @brief Set the HTTP method
     * @param method The HTTP method
     * @return Reference to this for chaining.
     */
    auto setMethod(HttpMethod method) -> HttpHeaderParser&;

    /**
     * @brief Get the HTTP method
     * @return The current HTTP method
     */
    [[nodiscard]] auto getMethod() const noexcept -> HttpMethod;

    /**
     * @brief Convert a string to HTTP method enum
     * @param methodStr The method string
     * @return The corresponding HTTP method enum
     */
    [[nodiscard]] static auto stringToMethod(
        std::string_view methodStr) noexcept -> HttpMethod;

    /**
     * @brief Convert an HTTP method enum to string
     * @param method The HTTP method enum
     * @return The corresponding method string
     */
    [[nodiscard]] static auto methodToString(HttpMethod method) noexcept
        -> std::string_view;

    /**
     * @brief Set the HTTP status
     * @param status The HTTP status object
     * @return Reference to this for chaining.
     */
    auto setStatus(const HttpStatus& status) -> HttpHeaderParser&;

    /**
     * @brief Set the HTTP status by code
     * @param code The HTTP status code
     * @return Reference to this for chaining.
     */
    auto setStatus(int code) -> HttpHeaderParser&;

    /**
     * @brief Get the HTTP status
     * @return The current HTTP status object
     */
    [[nodiscard]] auto getStatus() const noexcept -> HttpStatus;

    /**
     * @brief Set the URL path
     * @param path The URL path
     * @return Reference to this for chaining.
     */
    auto setPath(std::string_view path) -> HttpHeaderParser&;

    /**
     * @brief Get the URL path
     * @return The current URL path
     */
    [[nodiscard]] auto getPath() const -> std::string_view;

    /**
     * @brief Set the HTTP version
     * @param version The HTTP version
     * @return Reference to this for chaining.
     */
    auto setVersion(HttpVersion version) -> HttpHeaderParser&;

    /**
     * @brief Get the HTTP version
     * @return The current HTTP version
     */
    [[nodiscard]] auto getVersion() const noexcept -> HttpVersion;

    /**
     * @brief Set the request/response body
     * @param body The body content
     * @return Reference to this for chaining.
     */
    auto setBody(std::string_view body) -> HttpHeaderParser&;

    /**
     * @brief Set the request/response body from binary data
     * @param data The binary body content
     * @return Reference to this for chaining.
     */
    auto setBody(std::span<const std::byte> data) -> HttpHeaderParser&;

    /**
     * @brief Get the request/response body content
     * @return The body content
     */
    [[nodiscard]] auto getBody() const -> std::string_view;

    /**
     * @brief Get the body size
     * @return Size of the body in bytes
     */
    [[nodiscard]] auto getBodySize() const noexcept -> size_t;

    /**
     * @brief Check if body is empty
     * @return True if body is empty
     */
    [[nodiscard]] auto hasBody() const noexcept -> bool;

    /**
     * @brief Build an HTTP request string
     * @return The constructed HTTP request
     */
    [[nodiscard]] auto buildRequest() const -> std::string;

    /**
     * @brief Build an HTTP response string
     * @return The constructed HTTP response
     */
    [[nodiscard]] auto buildResponse() const -> std::string;

    /**
     * @brief URL encode a string
     * @param str The string to encode
     * @return The encoded string
     */
    [[nodiscard]] static auto urlEncode(std::string_view str) -> std::string;

    /**
     * @brief URL decode a string
     * @param str The string to decode
     * @return The decoded string
     */
    [[nodiscard]] static auto urlDecode(std::string_view str) -> std::string;

    /**
     * @brief Base64 encode a string
     * @param data The data to encode
     * @return The encoded string
     */
    [[nodiscard]] static auto base64Encode(std::span<const std::byte> data)
        -> std::string;

    /**
     * @brief Base64 decode a string
     * @param str The string to decode
     * @return The decoded data
     */
    [[nodiscard]] static auto base64Decode(std::string_view str)
        -> std::vector<std::byte>;

    /**
     * @brief Parse Content-Type header
     * @param contentType The Content-Type header value
     * @return Pair of (mime-type, parameters map)
     */
    [[nodiscard]] static auto parseContentType(std::string_view contentType)
        -> std::pair<std::string, std::map<std::string, std::string>>;

    /**
     * @brief Check if content type is JSON
     * @return True if Content-Type is application/json
     */
    [[nodiscard]] auto isJsonContent() const -> bool;

    /**
     * @brief Check if content type is form data
     * @return True if Content-Type is application/x-www-form-urlencoded
     */
    [[nodiscard]] auto isFormContent() const -> bool;

    /**
     * @brief Check if content type is multipart
     * @return True if Content-Type is multipart/form-data or similar
     */
    [[nodiscard]] auto isMultipartContent() const -> bool;

    /**
     * @brief Get Content-Type MIME type
     * @return The MIME type or empty string
     */
    [[nodiscard]] auto getContentType() const -> std::optional<std::string>;

    /**
     * @brief Get Content-Length
     * @return The content length or nullopt
     */
    [[nodiscard]] auto getContentLength() const -> std::optional<size_t>;

private:
    class HttpHeaderParserImpl;
    std::shared_ptr<HttpHeaderParserImpl> impl_;
};
}  // namespace atom::web

#endif  // ATOM_WEB_HTTP_PARSER_HPP
