/*
 * httpparser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-3

Description: Http Header Parser with C++20 features

**************************************************/

#include "httpparser.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "spdlog/spdlog.h"

#undef ERROR
#undef DELETE

namespace atom::web {

class HttpHeaderParser::HttpHeaderParserImpl {
public:
    std::map<std::string, std::vector<std::string>> headers;
    std::vector<Cookie> cookies;
    HttpMethod method = HttpMethod::UNKNOWN;
    HttpVersion version = HttpVersion::UNKNOWN;
    HttpStatus status = {200, "OK"};
    std::string path;
    std::string body;
};

HttpHeaderParser::HttpHeaderParser()
    : impl_(std::make_shared<HttpHeaderParser::HttpHeaderParserImpl>()) {
    spdlog::debug("HttpHeaderParser constructor called");
}

HttpHeaderParser::~HttpHeaderParser() {
    spdlog::debug("HttpHeaderParser destructor called");
}

void HttpHeaderParser::parseHeaders(const std::string& rawHeaders) {
    spdlog::debug("parseHeaders called");
    impl_->headers.clear();

    std::string_view sv(rawHeaders);
    size_t pos = 0;
    size_t lineEnd;

    // Process each line
    while ((lineEnd = sv.find('\n', pos)) != std::string_view::npos) {
        std::string_view line = sv.substr(pos, lineEnd - pos);

        // Remove trailing CR if present
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        // Skip empty lines
        if (!line.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string_view::npos) {
                std::string key(line.substr(0, colonPos));
                std::string_view valueView = line.substr(colonPos + 1);

                // Trim leading whitespace from value
                while (!valueView.empty() && std::isspace(valueView.front())) {
                    valueView.remove_prefix(1);
                }

                // Trim trailing whitespace from key
                while (!key.empty() && std::isspace(key.back())) {
                    key.pop_back();
                }

                std::string value(valueView);

                impl_->headers[key].push_back(value);
                spdlog::trace("Parsed header: {}: {}", key, value);
            }
        }

        pos = lineEnd + 1;
    }

    spdlog::debug("parseHeaders completed");
}

bool HttpHeaderParser::parseRequest(const std::string& rawRequest) {
    spdlog::debug("parseRequest called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(rawRequest);
    std::string line;

    // Parse request line
    if (!std::getline(iss, line)) {
        spdlog::error("Failed to read request line");
        return false;
    }

    // Trim carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Parse method, path, and version
    std::istringstream requestLine(line);
    std::string methodStr, versionStr;

    if (!(requestLine >> methodStr >> impl_->path >> versionStr)) {
        spdlog::error("Failed to parse request line: {}", line);
        return false;
    }

    // Set method and version
    impl_->method = stringToMethod(methodStr);

    if (versionStr == "HTTP/1.0") {
        impl_->version = HttpVersion::HTTP_1_0;
    } else if (versionStr == "HTTP/1.1") {
        impl_->version = HttpVersion::HTTP_1_1;
    } else if (versionStr == "HTTP/2.0" || versionStr == "HTTP/2") {
        impl_->version = HttpVersion::HTTP_2_0;
    } else if (versionStr == "HTTP/3.0" || versionStr == "HTTP/3") {
        impl_->version = HttpVersion::HTTP_3_0;
    } else {
        impl_->version = HttpVersion::UNKNOWN;
    }

    // Parse headers
    bool inHeaders = true;
    std::string headerBlock;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            inHeaders = false;
            break;
        }

        headerBlock += line + "\n";
    }

    if (!headerBlock.empty()) {
        parseHeaders(headerBlock);
    }

    // Parse cookies if present
    auto cookieValues = getHeaderValues("Cookie");
    if (cookieValues) {
        for (const auto& cookieStr : cookieValues.value()) {
            auto cookies = parseCookies(cookieStr);
            for (const auto& [name, value] : cookies) {
                Cookie cookie;
                cookie.name = name;
                cookie.value = value;
                impl_->cookies.push_back(cookie);
            }
        }
    }

    // Parse body if present
    if (!inHeaders) {
        std::stringstream bodyStream;
        std::string bodyLine;

        while (std::getline(iss, bodyLine)) {
            if (!bodyLine.empty() && bodyLine.back() == '\r') {
                bodyLine.pop_back();
            }
            bodyStream << bodyLine << '\n';
        }

        impl_->body = bodyStream.str();
    }

    spdlog::debug(
        "Request successfully parsed. Method: {}, Path: {}, Version: {}",
        methodToString(impl_->method), impl_->path,
        static_cast<int>(impl_->version));
    return true;
}

bool HttpHeaderParser::parseResponse(const std::string& rawResponse) {
    spdlog::debug("parseResponse called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(rawResponse);
    std::string line;

    // Parse status line
    if (!std::getline(iss, line)) {
        spdlog::error("Failed to read status line");
        return false;
    }

    // Trim carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Parse version, status code and description using faster string operations
    size_t firstSpace = line.find(' ');
    if (firstSpace == std::string::npos) {
        spdlog::error("Invalid status line: {}", line);
        return false;
    }

    std::string_view versionPart(line.c_str(), firstSpace);

    size_t secondSpace = line.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos) {
        spdlog::error("Invalid status line: {}", line);
        return false;
    }

    std::string_view statusCodePart(line.c_str() + firstSpace + 1,
                                    secondSpace - firstSpace - 1);

    std::string_view statusDescPart(line.c_str() + secondSpace + 1);

    // Extract HTTP version
    if (versionPart.substr(0, 5) != "HTTP/") {
        spdlog::error("Invalid HTTP version: {}", versionPart);
        return false;
    }

    std::string_view versionStr = versionPart.substr(5);

    if (versionStr == "1.0") {
        impl_->version = HttpVersion::HTTP_1_0;
    } else if (versionStr == "1.1") {
        impl_->version = HttpVersion::HTTP_1_1;
    } else if (versionStr == "2.0" || versionStr == "2") {
        impl_->version = HttpVersion::HTTP_2_0;
    } else if (versionStr == "3.0" || versionStr == "3") {
        impl_->version = HttpVersion::HTTP_3_0;
    } else {
        impl_->version = HttpVersion::UNKNOWN;
    }

    // Set status - convert status code to int
    int statusCode;
    try {
        statusCode = std::stoi(std::string(statusCodePart));
    } catch (const std::exception& e) {
        spdlog::error("Invalid status code: {}", statusCodePart);
        return false;
    }

    impl_->status = {statusCode, std::string(statusDescPart)};

    // Parse headers and rest of response
    bool inHeaders = true;
    std::string headerBlock;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            inHeaders = false;
            break;
        }

        headerBlock += line + "\n";
    }

    if (!headerBlock.empty()) {
        parseHeaders(headerBlock);
    }

    // Parse cookies if present
    auto setCookieValues = getHeaderValues("Set-Cookie");
    if (setCookieValues) {
        for (const auto& cookieStr : setCookieValues.value()) {
            Cookie cookie;

            // Parse name=value part
            size_t pos = cookieStr.find('=');
            if (pos != std::string::npos) {
                cookie.name = cookieStr.substr(0, pos);

                size_t semicolonPos = cookieStr.find(';', pos + 1);
                if (semicolonPos != std::string::npos) {
                    cookie.value =
                        cookieStr.substr(pos + 1, semicolonPos - pos - 1);
                } else {
                    cookie.value = cookieStr.substr(pos + 1);
                }

                // Parse cookie attributes
                if (semicolonPos != std::string::npos) {
                    std::string_view remaining(cookieStr.c_str() +
                                               semicolonPos);
                    size_t attrStart = 0;
                    size_t attrEnd;

                    while ((attrEnd = remaining.find(';', attrStart)) !=
                           std::string_view::npos) {
                        std::string_view attr = remaining.substr(
                            attrStart + 1, attrEnd - attrStart - 1);

                        // Skip leading whitespace
                        while (!attr.empty() && std::isspace(attr.front())) {
                            attr.remove_prefix(1);
                        }

                        size_t eqPos = attr.find('=');
                        std::string attrName;
                        std::string attrValue;

                        if (eqPos != std::string_view::npos) {
                            attrName = std::string(attr.substr(0, eqPos));
                            attrValue = std::string(attr.substr(eqPos + 1));
                        } else {
                            attrName = std::string(attr);
                        }

                        // Convert attribute name to lowercase
                        std::transform(
                            attrName.begin(), attrName.end(), attrName.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        if (attrName == "expires") {
                            // Parse date format
                            std::tm tm = {};
                            std::istringstream ss(attrValue);
                            ss >>
                                std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

                            if (!ss.fail()) {
                                std::time_t tt = std::mktime(&tm);
                                cookie.expires =
                                    std::chrono::system_clock::from_time_t(tt);
                            }
                        } else if (attrName == "max-age") {
                            try {
                                cookie.maxAge = std::stoi(attrValue);
                            } catch (...) {
                                spdlog::warn("Invalid max-age value: {}",
                                             attrValue);
                            }
                        } else if (attrName == "domain") {
                            cookie.domain = attrValue;
                        } else if (attrName == "path") {
                            cookie.path = attrValue;
                        } else if (attrName == "secure") {
                            cookie.secure = true;
                        } else if (attrName == "httponly") {
                            cookie.httpOnly = true;
                        } else if (attrName == "samesite") {
                            cookie.sameSite = attrValue;
                        }

                        attrStart = attrEnd + 1;
                    }

                    // Process the last attribute
                    if (attrStart < remaining.size()) {
                        std::string_view attr = remaining.substr(attrStart + 1);
                        while (!attr.empty() && std::isspace(attr.front())) {
                            attr.remove_prefix(1);
                        }

                        size_t eqPos = attr.find('=');
                        std::string attrName;
                        std::string attrValue;

                        if (eqPos != std::string_view::npos) {
                            attrName = std::string(attr.substr(0, eqPos));
                            attrValue = std::string(attr.substr(eqPos + 1));
                        } else {
                            attrName = std::string(attr);
                        }

                        std::transform(
                            attrName.begin(), attrName.end(), attrName.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        if (attrName == "expires") {
                            std::tm tm = {};
                            std::istringstream ss(attrValue);
                            ss >>
                                std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

                            if (!ss.fail()) {
                                std::time_t tt = std::mktime(&tm);
                                cookie.expires =
                                    std::chrono::system_clock::from_time_t(tt);
                            }
                        } else if (attrName == "max-age") {
                            try {
                                cookie.maxAge = std::stoi(attrValue);
                            } catch (...) {
                                spdlog::warn("Invalid max-age value: {}",
                                             attrValue);
                            }
                        } else if (attrName == "domain") {
                            cookie.domain = attrValue;
                        } else if (attrName == "path") {
                            cookie.path = attrValue;
                        } else if (attrName == "secure") {
                            cookie.secure = true;
                        } else if (attrName == "httponly") {
                            cookie.httpOnly = true;
                        } else if (attrName == "samesite") {
                            cookie.sameSite = attrValue;
                        }
                    }
                }

                impl_->cookies.push_back(cookie);
            }
        }
    }

    // Parse body if present
    if (!inHeaders) {
        std::stringstream bodyStream;
        std::string bodyLine;

        while (std::getline(iss, bodyLine)) {
            if (!bodyLine.empty() && bodyLine.back() == '\r') {
                bodyLine.pop_back();
            }
            bodyStream << bodyLine << '\n';
        }

        impl_->body = bodyStream.str();
    }

    spdlog::debug("Response successfully parsed. Status: {} {}, Version: {}",
                  impl_->status.code, impl_->status.description,
                  static_cast<int>(impl_->version));
    return true;
}

void HttpHeaderParser::setHeaderValue(const std::string& key,
                                      const std::string& value) {
    spdlog::trace("setHeaderValue called: {}={}", key, value);
    impl_->headers[key] = {value};
}

void HttpHeaderParser::setHeaders(
    const std::map<std::string, std::vector<std::string>>& headers) {
    spdlog::debug("setHeaders called");
    impl_->headers = headers;
}

void HttpHeaderParser::addHeaderValue(const std::string& key,
                                      const std::string& value) {
    spdlog::trace("addHeaderValue called: {}={}", key, value);
    impl_->headers[key].push_back(value);
}

auto HttpHeaderParser::getHeaderValues(const std::string& key) const
    -> std::optional<std::vector<std::string>> {
    spdlog::trace("getHeaderValues called: {}", key);
    if (auto it = impl_->headers.find(key); it != impl_->headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto HttpHeaderParser::getHeaderValue(const std::string& key) const
    -> std::optional<std::string> {
    spdlog::trace("getHeaderValue called: {}", key);

    auto values = getHeaderValues(key);
    if (values && !values->empty()) {
        return values->front();
    }

    return std::nullopt;
}

void HttpHeaderParser::removeHeader(const std::string& key) {
    spdlog::trace("removeHeader called: {}", key);
    impl_->headers.erase(key);
}

auto HttpHeaderParser::getAllHeaders() const
    -> std::map<std::string, std::vector<std::string>> {
    spdlog::trace("getAllHeaders called");
    return impl_->headers;
}

auto HttpHeaderParser::hasHeader(const std::string& key) const -> bool {
    spdlog::trace("hasHeader called: {}", key);
    return impl_->headers.contains(key);  // C++20 contains method
}

void HttpHeaderParser::clearHeaders() {
    spdlog::trace("clearHeaders called");
    impl_->headers.clear();
}

void HttpHeaderParser::addCookie(const Cookie& cookie) {
    spdlog::debug("addCookie called: {}", cookie.name);

    // Check if cookie with the same name already exists
    auto it = std::find_if(
        impl_->cookies.begin(), impl_->cookies.end(),
        [&cookie](const Cookie& c) { return c.name == cookie.name; });

    if (it != impl_->cookies.end()) {
        *it = cookie;
        spdlog::debug("Updated existing cookie: {}", cookie.name);
    } else {
        // Add new cookie
        impl_->cookies.push_back(cookie);
    }

    // Add to Set-Cookie header
    std::string cookieStr = cookie.name + "=" + cookie.value;

    if (cookie.expires) {
        std::time_t tt = std::chrono::system_clock::to_time_t(*cookie.expires);
        std::tm tm = *std::gmtime(&tt);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer),
                      "; Expires=%a, %d %b %Y %H:%M:%S GMT", &tm);
        cookieStr += buffer;
    }

    if (cookie.maxAge) {
        cookieStr += "; Max-Age=" + std::to_string(*cookie.maxAge);
    }

    if (cookie.domain) {
        cookieStr += "; Domain=" + *cookie.domain;
    }

    if (cookie.path) {
        cookieStr += "; Path=" + *cookie.path;
    }

    if (cookie.secure) {
        cookieStr += "; Secure";
    }

    if (cookie.httpOnly) {
        cookieStr += "; HttpOnly";
    }

    if (cookie.sameSite) {
        cookieStr += "; SameSite=" + *cookie.sameSite;
    }

    addHeaderValue("Set-Cookie", cookieStr);
}

std::map<std::string, std::string> HttpHeaderParser::parseCookies(
    const std::string& cookieStr) const {
    spdlog::trace("parseCookies called");

    std::map<std::string, std::string> cookies;
    std::string_view sv(cookieStr);
    size_t pos = 0;
    size_t nextSemicolon;

    // Process each cookie pair
    while ((nextSemicolon = sv.find(';', pos)) != std::string_view::npos) {
        std::string_view pair = sv.substr(pos, nextSemicolon - pos);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos) {
            std::string_view keyView = pair.substr(0, eqPos);
            std::string_view valueView = pair.substr(eqPos + 1);

            // Trim whitespace from key
            while (!keyView.empty() && std::isspace(keyView.front())) {
                keyView.remove_prefix(1);
            }
            while (!keyView.empty() && std::isspace(keyView.back())) {
                keyView.remove_suffix(1);
            }

            // Trim whitespace from value
            while (!valueView.empty() && std::isspace(valueView.front())) {
                valueView.remove_prefix(1);
            }
            while (!valueView.empty() && std::isspace(valueView.back())) {
                valueView.remove_suffix(1);
            }

            cookies[std::string(keyView)] = std::string(valueView);
        }

        pos = nextSemicolon + 1;
    }

    // Process the last pair
    if (pos < sv.size()) {
        std::string_view pair = sv.substr(pos);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos) {
            std::string_view keyView = pair.substr(0, eqPos);
            std::string_view valueView = pair.substr(eqPos + 1);

            // Trim whitespace from key and value
            while (!keyView.empty() && std::isspace(keyView.front())) {
                keyView.remove_prefix(1);
            }
            while (!keyView.empty() && std::isspace(keyView.back())) {
                keyView.remove_suffix(1);
            }

            while (!valueView.empty() && std::isspace(valueView.front())) {
                valueView.remove_prefix(1);
            }
            while (!valueView.empty() && std::isspace(valueView.back())) {
                valueView.remove_suffix(1);
            }

            cookies[std::string(keyView)] = std::string(valueView);
        }
    }

    return cookies;
}

std::vector<Cookie> HttpHeaderParser::getAllCookies() const {
    spdlog::trace("getAllCookies called");
    return impl_->cookies;
}

std::optional<Cookie> HttpHeaderParser::getCookie(
    const std::string& name) const {
    spdlog::trace("getCookie called: {}", name);

    auto it = std::find_if(impl_->cookies.begin(), impl_->cookies.end(),
                           [&name](const Cookie& c) { return c.name == name; });

    if (it != impl_->cookies.end()) {
        return *it;
    }

    return std::nullopt;
}

void HttpHeaderParser::removeCookie(const std::string& name) {
    spdlog::debug("removeCookie called: {}", name);

    impl_->cookies.erase(
        std::remove_if(impl_->cookies.begin(), impl_->cookies.end(),
                       [&name](const Cookie& c) { return c.name == name; }),
        impl_->cookies.end());
}

std::map<std::string, std::string> HttpHeaderParser::parseUrlParameters(
    const std::string& url) const {
    spdlog::debug("parseUrlParameters called");

    std::map<std::string, std::string> parameters;
    size_t queryStart = url.find('?');

    if (queryStart == std::string::npos) {
        return parameters;
    }

    std::string_view queryString(url.c_str() + queryStart + 1);
    size_t pos = 0;
    size_t nextAmp;

    // Process each parameter pair
    while ((nextAmp = queryString.find('&', pos)) != std::string_view::npos) {
        std::string_view pair = queryString.substr(pos, nextAmp - pos);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos) {
            std::string key = urlDecode(std::string(pair.substr(0, eqPos)));
            std::string value = urlDecode(std::string(pair.substr(eqPos + 1)));
            parameters[key] = value;
        } else {
            // Parameter without value
            std::string key = urlDecode(std::string(pair));
            parameters[key] = "";
        }

        pos = nextAmp + 1;
    }

    // Process the last pair
    if (pos < queryString.size()) {
        std::string_view pair = queryString.substr(pos);

        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos) {
            std::string key = urlDecode(std::string(pair.substr(0, eqPos)));
            std::string value = urlDecode(std::string(pair.substr(eqPos + 1)));
            parameters[key] = value;
        } else {
            // Parameter without value
            std::string key = urlDecode(std::string(pair));
            parameters[key] = "";
        }
    }

    return parameters;
}

void HttpHeaderParser::setMethod(HttpMethod method) {
    spdlog::trace("setMethod called: {}", static_cast<int>(method));
    impl_->method = method;
}

HttpMethod HttpHeaderParser::getMethod() const {
    spdlog::trace("getMethod called");
    return impl_->method;
}

HttpMethod HttpHeaderParser::stringToMethod(const std::string& methodStr) {
    std::string_view sv(methodStr);

    // Convert to uppercase for comparison
    std::string upperMethod(sv);
    std::transform(upperMethod.begin(), upperMethod.end(), upperMethod.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    // Use string_view for efficient comparison
    std::string_view upperView(upperMethod);

    if (upperView == "GET")
        return HttpMethod::GET;
    if (upperView == "POST")
        return HttpMethod::POST;
    if (upperView == "PUT")
        return HttpMethod::PUT;
    if (upperView == "DELETE")
        return HttpMethod::DELETE;
    if (upperView == "HEAD")
        return HttpMethod::HEAD;
    if (upperView == "OPTIONS")
        return HttpMethod::OPTIONS;
    if (upperView == "PATCH")
        return HttpMethod::PATCH;
    if (upperView == "TRACE")
        return HttpMethod::TRACE;
    if (upperView == "CONNECT")
        return HttpMethod::CONNECT;

    return HttpMethod::UNKNOWN;
}

std::string HttpHeaderParser::methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE:
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

void HttpHeaderParser::setStatus(const HttpStatus& status) {
    spdlog::trace("setStatus called: {} {}", status.code, status.description);
    impl_->status = status;
}

HttpStatus HttpHeaderParser::getStatus() const {
    spdlog::trace("getStatus called");
    return impl_->status;
}

void HttpHeaderParser::setPath(const std::string& path) {
    spdlog::trace("setPath called: {}", path);
    impl_->path = path;
}

std::string HttpHeaderParser::getPath() const {
    spdlog::trace("getPath called");
    return impl_->path;
}

void HttpHeaderParser::setVersion(HttpVersion version) {
    spdlog::trace("setVersion called: {}", static_cast<int>(version));
    impl_->version = version;
}

HttpVersion HttpHeaderParser::getVersion() const {
    spdlog::trace("getVersion called");
    return impl_->version;
}

void HttpHeaderParser::setBody(const std::string& body) {
    spdlog::debug("setBody called: {} bytes", body.length());
    impl_->body = body;
}

std::string HttpHeaderParser::getBody() const {
    spdlog::trace("getBody called");
    return impl_->body;
}

std::string HttpHeaderParser::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Handle percent encoding
            int value = 0;
            for (int j = 1; j <= 2; ++j) {
                char c = str[i + j];
                value <<= 4;

                if (c >= '0' && c <= '9') {
                    value += c - '0';
                } else if (c >= 'A' && c <= 'F') {
                    value += c - 'A' + 10;
                } else if (c >= 'a' && c <= 'f') {
                    value += c - 'a' + 10;
                } else {
                    // Invalid hex character, keep original
                    value = -1;
                    break;
                }
            }

            if (value >= 0) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                // Not a valid percent encoding, keep original
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

std::string HttpHeaderParser::urlEncode(const std::string& str) {
    static const char hexChars[] = "0123456789ABCDEF";
    static const bool shouldEscape[256] = {/* 0x00-0x0F */ true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           /* 0x10-0x1F */ true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           /* ' '-'/' */ true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           false,
                                           false,
                                           true,
                                           /* '0'-'9' */ false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           /* '@'-'O' */ true,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           /* 'P'-'_' */ false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           true,
                                           true,
                                           true,
                                           true,
                                           false,
                                           /* '`'-'o' */ true,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           /* 'p'-DEL */ false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           false,
                                           true,
                                           true,
                                           true,
                                           false,
                                           true,
                                           /* 0x80-0xFF */ true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true,
                                           true};

    std::string result;
    result.reserve(str.size() * 3);  // Reserve enough space

    for (unsigned char c : str) {
        if (shouldEscape[c]) {
            if (c == ' ') {
                // Convert space to plus
                result += '+';
            } else {
                // Convert other characters to %HH
                result += '%';
                result += hexChars[c >> 4];
                result += hexChars[c & 15];
            }
        } else {
            result += c;
        }
    }

    return result;
}

std::string HttpHeaderParser::buildRequest() const {
    spdlog::debug("buildRequest called");

    std::stringstream request;

    // Build request line
    request << methodToString(impl_->method) << " " << impl_->path << " ";

    // Add HTTP version
    switch (impl_->version) {
        case HttpVersion::HTTP_1_0:
            request << "HTTP/1.0";
            break;
        case HttpVersion::HTTP_1_1:
            request << "HTTP/1.1";
            break;
        case HttpVersion::HTTP_2_0:
            request << "HTTP/2.0";
            break;
        case HttpVersion::HTTP_3_0:
            request << "HTTP/3.0";
            break;
        default:
            request << "HTTP/1.1";  // Default to HTTP/1.1
            break;
    }
    request << "\r\n";

    // Add request headers
    for (const auto& [key, values] : impl_->headers) {
        for (const auto& value : values) {
            request << key << ": " << value << "\r\n";
        }
    }

    // Add blank line to indicate end of headers
    request << "\r\n";

    // Add request body if present
    if (!impl_->body.empty()) {
        request << impl_->body;
    }

    return request.str();
}

std::string HttpHeaderParser::buildResponse() const {
    spdlog::debug("buildResponse called");

    std::stringstream response;

    // Build status line
    switch (impl_->version) {
        case HttpVersion::HTTP_1_0:
            response << "HTTP/1.0";
            break;
        case HttpVersion::HTTP_1_1:
            response << "HTTP/1.1";
            break;
        case HttpVersion::HTTP_2_0:
            response << "HTTP/2.0";
            break;
        case HttpVersion::HTTP_3_0:
            response << "HTTP/3.0";
            break;
        default:
            response << "HTTP/1.1";  // Default to HTTP/1.1
            break;
    }
    response << " " << impl_->status.code << " " << impl_->status.description
             << "\r\n";

    // Add response headers
    for (const auto& [key, values] : impl_->headers) {
        for (const auto& value : values) {
            response << key << ": " << value << "\r\n";
        }
    }

    // Add blank line to indicate end of headers
    response << "\r\n";

    // Add response body if present
    if (!impl_->body.empty()) {
        response << impl_->body;
    }

    return response.str();
}

}  // namespace atom::web
