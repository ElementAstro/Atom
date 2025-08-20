/*
 * httpparser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-3

Description: Http Header Parser with C++20 features

**************************************************/

#ifndef ATOM_WEB_HTTP_PARSER_HPP
#define ATOM_WEB_HTTP_PARSER_HPP

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace atom::web {

/**
 * @brief HTTP method enumeration
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    TRACE,
    CONNECT,
    UNKNOWN
};

/**
 * @brief HTTP version enumeration
 */
enum class HttpVersion { HTTP_1_0, HTTP_1_1, HTTP_2_0, HTTP_3_0, UNKNOWN };

/**
 * @brief HTTP status code and description structure
 */
struct HttpStatus {
    int code;
    std::string description;

    static HttpStatus OK() { return {200, "OK"}; }
    static HttpStatus Created() { return {201, "Created"}; }
    static HttpStatus Accepted() { return {202, "Accepted"}; }
    static HttpStatus NoContent() { return {204, "No Content"}; }
    static HttpStatus MovedPermanently() { return {301, "Moved Permanently"}; }
    static HttpStatus Found() { return {302, "Found"}; }
    static HttpStatus BadRequest() { return {400, "Bad Request"}; }
    static HttpStatus Unauthorized() { return {401, "Unauthorized"}; }
    static HttpStatus Forbidden() { return {403, "Forbidden"}; }
    static HttpStatus NotFound() { return {404, "Not Found"}; }
    static HttpStatus MethodNotAllowed() { return {405, "Method Not Allowed"}; }
    static HttpStatus InternalServerError() {
        return {500, "Internal Server Error"};
    }
    static HttpStatus NotImplemented() { return {501, "Not Implemented"}; }
    static HttpStatus BadGateway() { return {502, "Bad Gateway"}; }
    static HttpStatus ServiceUnavailable() {
        return {503, "Service Unavailable"};
    }
};

/**
 * @brief Cookie structure representing an HTTP Cookie
 */
struct Cookie {
    std::string name;
    std::string value;
    std::optional<std::chrono::system_clock::time_point> expires;
    std::optional<int> maxAge;
    std::optional<std::string> domain;
    std::optional<std::string> path;
    bool secure = false;
    bool httpOnly = false;
    std::optional<std::string> sameSite;
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

    /**
     * @brief Parses the raw HTTP headers and stores them internally.
     * @param rawHeaders The raw HTTP headers as a string.
     */
    void parseHeaders(const std::string& rawHeaders);

    /**
     * @brief Parse a complete HTTP request
     * @param rawRequest The raw HTTP request string
     * @return Whether parsing was successful
     */
    bool parseRequest(const std::string& rawRequest);

    /**
     * @brief Parse a complete HTTP response
     * @param rawResponse The raw HTTP response string
     * @return Whether parsing was successful
     */
    bool parseResponse(const std::string& rawResponse);

    /**
     * @brief Sets the value of a specific header field.
     * @param key The key of the header field.
     * @param value The value to set.
     */
    void setHeaderValue(const std::string& key, const std::string& value);

    /**
     * @brief Sets multiple header fields at once.
     * @param headers A map containing header fields and their values.
     */
    void setHeaders(
        const std::map<std::string, std::vector<std::string>>& headers);

    /**
     * @brief Adds a new value to an existing header field.
     * @param key The key of the header field.
     * @param value The value to add.
     */
    void addHeaderValue(const std::string& key, const std::string& value);

    /**
     * @brief Retrieves the values of a specific header field.
     * @param key The key of the header field.
     * @return A vector containing the values of the header field.
     */
    [[nodiscard]] auto getHeaderValues(const std::string& key) const
        -> std::optional<std::vector<std::string>>;

    /**
     * @brief Gets the first value of a header field
     * @param key The header field name
     * @return The first header field value, or nullopt if it doesn't exist
     */
    [[nodiscard]] auto getHeaderValue(const std::string& key) const
        -> std::optional<std::string>;

    /**
     * @brief Removes a specific header field.
     * @param key The key of the header field to remove.
     */
    void removeHeader(const std::string& key);

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
    [[nodiscard]] auto hasHeader(const std::string& key) const -> bool;

    /**
     * @brief Clears all the parsed headers.
     */
    void clearHeaders();

    /**
     * @brief Add a Cookie
     * @param cookie The Cookie object to add
     */
    void addCookie(const Cookie& cookie);

    /**
     * @brief Parse a Cookie string
     * @param cookieStr The Cookie string
     * @return A map of cookie name-value pairs
     */
    [[nodiscard]] std::map<std::string, std::string> parseCookies(
        const std::string& cookieStr) const;

    /**
     * @brief Get all Cookies
     * @return A list of all cookies
     */
    [[nodiscard]] std::vector<Cookie> getAllCookies() const;

    /**
     * @brief Get a cookie with the specified name
     * @param name The cookie name
     * @return The Cookie object if it exists
     */
    [[nodiscard]] std::optional<Cookie> getCookie(
        const std::string& name) const;

    /**
     * @brief Remove a specific cookie
     * @param name The name of the cookie to remove
     */
    void removeCookie(const std::string& name);

    /**
     * @brief Parse URL query parameters
     * @param url The URL containing query parameters
     * @return A map of parsed parameter name-value pairs
     */
    [[nodiscard]] std::map<std::string, std::string> parseUrlParameters(
        const std::string& url) const;

    /**
     * @brief Set the HTTP method
     * @param method The HTTP method
     */
    void setMethod(HttpMethod method);

    /**
     * @brief Get the HTTP method
     * @return The current HTTP method
     */
    [[nodiscard]] HttpMethod getMethod() const;

    /**
     * @brief Convert a string to HTTP method enum
     * @param methodStr The method string
     * @return The corresponding HTTP method enum
     */
    [[nodiscard]] static HttpMethod stringToMethod(
        const std::string& methodStr);

    /**
     * @brief Convert an HTTP method enum to string
     * @param method The HTTP method enum
     * @return The corresponding method string
     */
    [[nodiscard]] static std::string methodToString(HttpMethod method);

    /**
     * @brief Set the HTTP status
     * @param status The HTTP status object
     */
    void setStatus(const HttpStatus& status);

    /**
     * @brief Get the HTTP status
     * @return The current HTTP status object
     */
    [[nodiscard]] HttpStatus getStatus() const;

    /**
     * @brief Set the URL path
     * @param path The URL path
     */
    void setPath(const std::string& path);

    /**
     * @brief Get the URL path
     * @return The current URL path
     */
    [[nodiscard]] std::string getPath() const;

    /**
     * @brief Set the HTTP version
     * @param version The HTTP version
     */
    void setVersion(HttpVersion version);

    /**
     * @brief Get the HTTP version
     * @return The current HTTP version
     */
    [[nodiscard]] HttpVersion getVersion() const;

    /**
     * @brief Set the request/response body
     * @param body The body content
     */
    void setBody(const std::string& body);

    /**
     * @brief Get the request/response body content
     * @return The body content
     */
    [[nodiscard]] std::string getBody() const;

    /**
     * @brief Build an HTTP request string
     * @return The constructed HTTP request
     */
    [[nodiscard]] std::string buildRequest() const;

    /**
     * @brief Build an HTTP response string
     * @return The constructed HTTP response
     */
    [[nodiscard]] std::string buildResponse() const;

    /**
     * @brief URL encode a string
     * @param str The string to encode
     * @return The encoded string
     */
    [[nodiscard]] static std::string urlEncode(const std::string& str);

    /**
     * @brief URL decode a string
     * @param str The string to decode
     * @return The decoded string
     */
    [[nodiscard]] static std::string urlDecode(const std::string& str);

private:
    class HttpHeaderParserImpl;
    std::shared_ptr<HttpHeaderParserImpl> impl_;  // Pointer to implementation
};
}  // namespace atom::web

#endif  // ATOM_WEB_HTTP_PARSER_HPP
