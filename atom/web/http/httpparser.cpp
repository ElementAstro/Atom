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
#include <cstdint>
#include <iomanip>
#include <span>
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

auto HttpHeaderParser::parseHeaders(std::string_view rawHeaders) -> bool {
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
    return true;
}

auto HttpHeaderParser::parseRequest(std::string_view rawRequest) -> bool {
    spdlog::debug("parseRequest called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(std::string(rawRequest));
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

auto HttpHeaderParser::parseResponse(std::string_view rawResponse) -> bool {
    spdlog::debug("parseResponse called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(std::string(rawResponse));
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
    } catch (const std::exception&) {
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

auto HttpHeaderParser::setHeaderValue(
    std::string_view key, std::string_view value) -> HttpHeaderParser& {
    spdlog::trace("setHeaderValue called: {}={}", key, value);
    impl_->headers[std::string(key)] = {std::string(value)};
    return *this;
}

auto HttpHeaderParser::setHeaders(
    const std::map<std::string, std::vector<std::string>>& headers)
    -> HttpHeaderParser& {
    spdlog::debug("setHeaders called");
    impl_->headers = headers;
    return *this;
}

auto HttpHeaderParser::addHeaderValue(
    std::string_view key, std::string_view value) -> HttpHeaderParser& {
    spdlog::trace("addHeaderValue called: {}={}", key, value);
    impl_->headers[std::string(key)].push_back(std::string(value));
    return *this;
}

auto HttpHeaderParser::getHeaderValues(std::string_view key) const
    -> std::optional<std::vector<std::string>> {
    spdlog::trace("getHeaderValues called: {}", key);
    if (auto it = impl_->headers.find(std::string(key));
        it != impl_->headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto HttpHeaderParser::getHeaderValue(std::string_view key) const
    -> std::optional<std::string> {
    spdlog::trace("getHeaderValue called: {}", key);

    auto values = getHeaderValues(key);
    if (values && !values->empty()) {
        return values->front();
    }

    return std::nullopt;
}

auto HttpHeaderParser::removeHeader(std::string_view key) -> HttpHeaderParser& {
    spdlog::trace("removeHeader called: {}", key);
    impl_->headers.erase(std::string(key));
    return *this;
}

auto HttpHeaderParser::getAllHeaders() const
    -> std::map<std::string, std::vector<std::string>> {
    spdlog::trace("getAllHeaders called");
    return impl_->headers;
}

auto HttpHeaderParser::hasHeader(std::string_view key) const noexcept -> bool {
    spdlog::trace("hasHeader called: {}", key);
    return impl_->headers.contains(std::string(key));  // C++20 contains method
}

auto HttpHeaderParser::clearHeaders() -> HttpHeaderParser& {
    spdlog::trace("clearHeaders called");
    impl_->headers.clear();
    return *this;
}

auto HttpHeaderParser::addCookie(const Cookie& cookie) -> HttpHeaderParser& {
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
    return *this;
}

auto HttpHeaderParser::parseCookies(std::string_view cookieStr) const
    -> std::map<std::string, std::string> {
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

auto HttpHeaderParser::getAllCookies() const -> std::vector<Cookie> {
    spdlog::trace("getAllCookies called");
    return impl_->cookies;
}

auto HttpHeaderParser::getCookie(std::string_view name) const
    -> std::optional<Cookie> {
    spdlog::trace("getCookie called: {}", name);

    auto it = std::find_if(impl_->cookies.begin(), impl_->cookies.end(),
                           [name](const Cookie& c) { return c.name == name; });

    if (it != impl_->cookies.end()) {
        return *it;
    }

    return std::nullopt;
}

auto HttpHeaderParser::removeCookie(std::string_view name)
    -> HttpHeaderParser& {
    spdlog::debug("removeCookie called: {}", name);

    impl_->cookies.erase(
        std::remove_if(impl_->cookies.begin(), impl_->cookies.end(),
                       [name](const Cookie& c) { return c.name == name; }),
        impl_->cookies.end());
    return *this;
}

auto HttpHeaderParser::parseUrlParameters(std::string_view url) const
    -> std::map<std::string, std::string> {
    spdlog::debug("parseUrlParameters called");

    std::map<std::string, std::string> parameters;
    size_t queryStart = url.find('?');
    std::string urlStr(url);

    if (queryStart == std::string::npos) {
        return parameters;
    }

    std::string_view queryString(urlStr.c_str() + queryStart + 1);
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

auto HttpHeaderParser::setMethod(HttpMethod method) -> HttpHeaderParser& {
    spdlog::trace("setMethod called: {}", static_cast<int>(method));
    impl_->method = method;
    return *this;
}

auto HttpHeaderParser::getMethod() const noexcept -> HttpMethod {
    spdlog::trace("getMethod called");
    return impl_->method;
}

auto HttpHeaderParser::stringToMethod(std::string_view methodStr) noexcept
    -> HttpMethod {
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
        return HttpMethod::DELETE_;
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

auto HttpHeaderParser::methodToString(HttpMethod method) noexcept
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

auto HttpHeaderParser::setStatus(const HttpStatus& status)
    -> HttpHeaderParser& {
    spdlog::trace("setStatus called: {} {}", status.code, status.description);
    impl_->status = status;
    return *this;
}

auto HttpHeaderParser::getStatus() const noexcept -> HttpStatus {
    spdlog::trace("getStatus called");
    return impl_->status;
}

auto HttpHeaderParser::setPath(std::string_view path) -> HttpHeaderParser& {
    spdlog::trace("setPath called: {}", path);
    impl_->path = std::string(path);
    return *this;
}

auto HttpHeaderParser::getPath() const -> std::string_view {
    spdlog::trace("getPath called");
    return impl_->path;
}

auto HttpHeaderParser::setVersion(HttpVersion version) -> HttpHeaderParser& {
    spdlog::trace("setVersion called: {}", static_cast<int>(version));
    impl_->version = version;
    return *this;
}

auto HttpHeaderParser::getVersion() const noexcept -> HttpVersion {
    spdlog::trace("getVersion called");
    return impl_->version;
}

auto HttpHeaderParser::setBody(std::string_view body) -> HttpHeaderParser& {
    spdlog::debug("setBody called: {} bytes", body.length());
    impl_->body = std::string(body);
    return *this;
}

auto HttpHeaderParser::getBody() const -> std::string_view {
    spdlog::trace("getBody called");
    return impl_->body;
}

auto HttpHeaderParser::urlDecode(std::string_view str) -> std::string {
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

auto HttpHeaderParser::urlEncode(std::string_view str) -> std::string {
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

auto HttpHeaderParser::buildRequest() const -> std::string {
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

auto HttpHeaderParser::buildResponse() const -> std::string {
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

auto HttpHeaderParser::headerCount() const noexcept -> size_t {
    return impl_->headers.size();
}

auto HttpHeaderParser::cookieCount() const noexcept -> size_t {
    return impl_->cookies.size();
}

auto HttpHeaderParser::setStatus(int code) -> HttpHeaderParser& {
    impl_->status.code = code;
    // Set default description based on common status codes
    switch (code) {
        case 200:
            impl_->status.description = "OK";
            break;
        case 201:
            impl_->status.description = "Created";
            break;
        case 204:
            impl_->status.description = "No Content";
            break;
        case 301:
            impl_->status.description = "Moved Permanently";
            break;
        case 302:
            impl_->status.description = "Found";
            break;
        case 304:
            impl_->status.description = "Not Modified";
            break;
        case 400:
            impl_->status.description = "Bad Request";
            break;
        case 401:
            impl_->status.description = "Unauthorized";
            break;
        case 403:
            impl_->status.description = "Forbidden";
            break;
        case 404:
            impl_->status.description = "Not Found";
            break;
        case 405:
            impl_->status.description = "Method Not Allowed";
            break;
        case 500:
            impl_->status.description = "Internal Server Error";
            break;
        case 502:
            impl_->status.description = "Bad Gateway";
            break;
        case 503:
            impl_->status.description = "Service Unavailable";
            break;
        default:
            impl_->status.description = "Unknown";
            break;
    }
    return *this;
}

auto HttpHeaderParser::setBody(std::span<const std::byte> data)
    -> HttpHeaderParser& {
    impl_->body.assign(reinterpret_cast<const char*>(data.data()), data.size());
    return *this;
}

auto HttpHeaderParser::getBodySize() const noexcept -> size_t {
    return impl_->body.size();
}

auto HttpHeaderParser::hasBody() const noexcept -> bool {
    return !impl_->body.empty();
}

auto HttpHeaderParser::base64Encode(std::span<const std::byte> data)
    -> std::string {
    static constexpr char base64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a =
            i < data.size() ? static_cast<uint8_t>(data[i++]) : 0;
        uint32_t octet_b =
            i < data.size() ? static_cast<uint8_t>(data[i++]) : 0;
        uint32_t octet_c =
            i < data.size() ? static_cast<uint8_t>(data[i++]) : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        result += base64Chars[(triple >> 18) & 0x3F];
        result += base64Chars[(triple >> 12) & 0x3F];
        result +=
            (i > data.size() + 1) ? '=' : base64Chars[(triple >> 6) & 0x3F];
        result += (i > data.size()) ? '=' : base64Chars[triple & 0x3F];
    }

    return result;
}

auto HttpHeaderParser::base64Decode(std::string_view str)
    -> std::vector<std::byte> {
    static constexpr uint8_t decodeTable[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64};

    std::vector<std::byte> result;
    result.reserve((str.size() / 4) * 3);

    uint32_t temp = 0;
    int bits = 0;

    for (char c : str) {
        if (c == '=' || c == '\n' || c == '\r')
            continue;
        uint8_t val = decodeTable[static_cast<uint8_t>(c)];
        if (val == 64)
            continue;  // Invalid character

        temp = (temp << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<std::byte>((temp >> bits) & 0xFF));
        }
    }

    return result;
}

auto HttpHeaderParser::parseContentType(std::string_view contentType)
    -> std::pair<std::string, std::map<std::string, std::string>> {
    std::string mimeType;
    std::map<std::string, std::string> params;

    size_t semicolonPos = contentType.find(';');
    if (semicolonPos == std::string_view::npos) {
        mimeType = std::string(contentType);
        // Trim whitespace
        while (!mimeType.empty() && std::isspace(mimeType.back())) {
            mimeType.pop_back();
        }
        return {mimeType, params};
    }

    mimeType = std::string(contentType.substr(0, semicolonPos));
    // Trim whitespace from mime type
    while (!mimeType.empty() && std::isspace(mimeType.back())) {
        mimeType.pop_back();
    }

    // Parse parameters
    std::string_view paramStr = contentType.substr(semicolonPos + 1);
    size_t pos = 0;

    while (pos < paramStr.size()) {
        // Skip whitespace
        while (pos < paramStr.size() && std::isspace(paramStr[pos])) {
            ++pos;
        }

        size_t eqPos = paramStr.find('=', pos);
        if (eqPos == std::string_view::npos)
            break;

        std::string key(paramStr.substr(pos, eqPos - pos));
        // Trim key
        while (!key.empty() && std::isspace(key.back())) {
            key.pop_back();
        }

        pos = eqPos + 1;

        // Handle quoted values
        std::string value;
        if (pos < paramStr.size() && paramStr[pos] == '"') {
            ++pos;
            size_t endQuote = paramStr.find('"', pos);
            if (endQuote != std::string_view::npos) {
                value = std::string(paramStr.substr(pos, endQuote - pos));
                pos = endQuote + 1;
            }
        } else {
            size_t nextSemi = paramStr.find(';', pos);
            if (nextSemi == std::string_view::npos) {
                value = std::string(paramStr.substr(pos));
                pos = paramStr.size();
            } else {
                value = std::string(paramStr.substr(pos, nextSemi - pos));
                pos = nextSemi + 1;
            }
            // Trim value
            while (!value.empty() && std::isspace(value.back())) {
                value.pop_back();
            }
        }

        params[key] = value;
    }

    return {mimeType, params};
}

auto HttpHeaderParser::isJsonContent() const -> bool {
    auto ct = getContentType();
    if (!ct)
        return false;
    return ct->find("application/json") != std::string::npos;
}

auto HttpHeaderParser::isFormContent() const -> bool {
    auto ct = getContentType();
    if (!ct)
        return false;
    return ct->find("application/x-www-form-urlencoded") != std::string::npos;
}

auto HttpHeaderParser::isMultipartContent() const -> bool {
    auto ct = getContentType();
    if (!ct)
        return false;
    return ct->find("multipart/") != std::string::npos;
}

auto HttpHeaderParser::getContentType() const -> std::optional<std::string> {
    return getHeaderValue("Content-Type");
}

auto HttpHeaderParser::getContentLength() const -> std::optional<size_t> {
    auto cl = getHeaderValue("Content-Length");
    if (!cl)
        return std::nullopt;
    try {
        return std::stoull(*cl);
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace atom::web
