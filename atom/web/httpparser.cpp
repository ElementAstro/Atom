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
#include <regex>
#include <sstream>

#include "atom/log/loguru.hpp"
#include "atom/utils/to_string.hpp"

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
    LOG_F(INFO, "HttpHeaderParser constructor called");
}

HttpHeaderParser::~HttpHeaderParser() {
    LOG_F(INFO, "HttpHeaderParser destructor called");
}

void HttpHeaderParser::parseHeaders(const std::string& rawHeaders) {
    LOG_F(INFO, "parseHeaders called with rawHeaders: {}", rawHeaders);
    impl_->headers.clear();
    std::istringstream iss(rawHeaders);

    std::string line;
    while (std::getline(iss, line)) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            key.erase(key.find_last_not_of(' ') + 1);
            value.erase(0, value.find_first_not_of(' '));

            impl_->headers[key].push_back(value);
            LOG_F(INFO, "Parsed header: {}: {}", key, value);
        }
    }
    LOG_F(INFO, "parseHeaders completed");
}

bool HttpHeaderParser::parseRequest(const std::string& rawRequest) {
    LOG_F(INFO, "parseRequest called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(rawRequest);
    std::string line;

    // Parse request line
    if (!std::getline(iss, line)) {
        LOG_F(ERROR, "Failed to read request line");
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
        LOG_F(ERROR, "Failed to parse request line: {}", line);
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
        std::string bodyStr;
        std::string bodyLine;

        while (std::getline(iss, bodyLine)) {
            if (!bodyLine.empty() && bodyLine.back() == '\r') {
                bodyLine.pop_back();
            }
            bodyStr += bodyLine + "\n";
        }

        if (!bodyStr.empty()) {
            impl_->body = bodyStr;
        }
    }

    LOG_F(INFO,
          "Request successfully parsed. Method: {}, Path: {}, Version: {}",
          methodToString(impl_->method), impl_->path,
          static_cast<int>(impl_->version));
    return true;
}

bool HttpHeaderParser::parseResponse(const std::string& rawResponse) {
    LOG_F(INFO, "parseResponse called");

    // Clear existing data
    clearHeaders();
    impl_->cookies.clear();
    impl_->body.clear();

    std::istringstream iss(rawResponse);
    std::string line;

    // Parse status line
    if (!std::getline(iss, line)) {
        LOG_F(ERROR, "Failed to read status line");
        return false;
    }

    // Trim carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Parse version, status code and description
    std::regex statusLineRegex(R"(HTTP/(\d+\.\d+) (\d+) (.*))");
    std::smatch matches;

    if (!std::regex_match(line, matches, statusLineRegex)) {
        LOG_F(ERROR, "Failed to parse status line: {}", line);
        return false;
    }

    // Set version
    std::string versionStr = matches[1].str();
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

    // Set status
    int statusCode = std::stoi(matches[2].str());
    std::string statusDesc = matches[3].str();
    impl_->status = {statusCode, statusDesc};

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

                // Parse attributes
                std::string remaining =
                    cookieStr.substr(pos + cookie.value.length() + 1);
                std::regex attributeRegex(R"(;\s*([^=;]+)(?:=([^;]*))?)",
                                          std::regex::ECMAScript);

                std::sregex_iterator it(remaining.begin(), remaining.end(),
                                        attributeRegex);
                std::sregex_iterator end;

                for (; it != end; ++it) {
                    std::smatch match = *it;
                    std::string name = match[1].str();
                    std::string value = match[2].matched ? match[2].str() : "";

                    std::transform(
                        name.begin(), name.end(), name.begin(),
                        [](unsigned char c) { return std::tolower(c); });

                    if (name == "expires") {
                        // Basic support for common date format
                        std::tm tm = {};
                        std::istringstream ss(value);
                        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

                        if (!ss.fail()) {
                            std::time_t tt = std::mktime(&tm);
                            cookie.expires =
                                std::chrono::system_clock::from_time_t(tt);
                        }
                    } else if (name == "max-age") {
                        try {
                            cookie.maxAge = std::stoi(value);
                        } catch (...) {
                            LOG_F(WARNING, "Invalid max-age value: {}", value);
                        }
                    } else if (name == "domain") {
                        cookie.domain = value;
                    } else if (name == "path") {
                        cookie.path = value;
                    } else if (name == "secure") {
                        cookie.secure = true;
                    } else if (name == "httponly") {
                        cookie.httpOnly = true;
                    } else if (name == "samesite") {
                        cookie.sameSite = value;
                    }
                }

                impl_->cookies.push_back(cookie);
            }
        }
    }

    // Parse body if present
    if (!inHeaders) {
        std::string bodyStr;
        std::string bodyLine;

        while (std::getline(iss, bodyLine)) {
            if (!bodyLine.empty() && bodyLine.back() == '\r') {
                bodyLine.pop_back();
            }
            bodyStr += bodyLine + "\n";
        }

        if (!bodyStr.empty()) {
            impl_->body = bodyStr;
        }
    }

    LOG_F(INFO, "Response successfully parsed. Status: {} {}, Version: {}",
          impl_->status.code, impl_->status.description,
          static_cast<int>(impl_->version));
    return true;
}

void HttpHeaderParser::setHeaderValue(const std::string& key,
                                      const std::string& value) {
    LOG_F(INFO, "setHeaderValue called with key: {}, value: {}", key, value);
    impl_->headers[key] = {value};
    LOG_F(INFO, "Header set: {}: {}", key, value);
}

void HttpHeaderParser::setHeaders(
    const std::map<std::string, std::vector<std::string>>& headers) {
    LOG_F(INFO, "setHeaders called with headers: {}",
          atom::utils::toString(headers));
    impl_->headers = headers;
    LOG_F(INFO, "Headers set successfully");
}

void HttpHeaderParser::addHeaderValue(const std::string& key,
                                      const std::string& value) {
    LOG_F(INFO, "addHeaderValue called with key: {}, value: {}", key, value);
    impl_->headers[key].push_back(value);
    LOG_F(INFO, "Header value added: {}: {}", key, value);
}

auto HttpHeaderParser::getHeaderValues(const std::string& key) const
    -> std::optional<std::vector<std::string>> {
    LOG_F(INFO, "getHeaderValues called with key: {}", key);
    if (auto it = impl_->headers.find(key); it != impl_->headers.end()) {
        LOG_F(INFO, "Header values found for key {}: {}", key,
              atom::utils::toString(it->second));
        return it->second;
    }
    LOG_F(WARNING, "Header values not found for key: {}", key);
    return std::nullopt;  // Use optional to represent missing values
}

auto HttpHeaderParser::getHeaderValue(const std::string& key) const
    -> std::optional<std::string> {
    LOG_F(INFO, "getHeaderValue called with key: {}", key);

    auto values = getHeaderValues(key);
    if (values && !values->empty()) {
        LOG_F(INFO, "Header value found for key {}: {}", key, values->front());
        return values->front();
    }

    LOG_F(WARNING, "Header value not found for key: {}", key);
    return std::nullopt;
}

void HttpHeaderParser::removeHeader(const std::string& key) {
    LOG_F(INFO, "removeHeader called with key: {}", key);
    impl_->headers.erase(key);
    LOG_F(INFO, "Header removed: {}", key);
}

auto HttpHeaderParser::getAllHeaders() const
    -> std::map<std::string, std::vector<std::string>> {
    LOG_F(INFO, "getAllHeaders called");
    return impl_->headers;
}

auto HttpHeaderParser::hasHeader(const std::string& key) const -> bool {
    LOG_F(INFO, "hasHeader called with key: {}", key);
    bool result = impl_->headers.contains(key);  // Use C++20 contains method
    LOG_F(INFO, "hasHeader result for key {}: {}", key, result);
    return result;
}

void HttpHeaderParser::clearHeaders() {
    LOG_F(INFO, "clearHeaders called");
    impl_->headers.clear();
    LOG_F(INFO, "All headers cleared");
}

void HttpHeaderParser::addCookie(const Cookie& cookie) {
    LOG_F(INFO, "addCookie called for cookie: {}", cookie.name);

    // Check if cookie with the same name already exists
    for (auto it = impl_->cookies.begin(); it != impl_->cookies.end(); ++it) {
        if (it->name == cookie.name) {
            *it = cookie;
            LOG_F(INFO, "Updated existing cookie: {}", cookie.name);
            return;
        }
    }

    // Add new cookie
    impl_->cookies.push_back(cookie);

    // Add to Set-Cookie header
    std::string cookieStr = cookie.name + "=" + cookie.value;

    if (cookie.expires) {
        std::time_t tt = std::chrono::system_clock::to_time_t(*cookie.expires);
        std::tm* tm = std::gmtime(&tt);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer),
                      "; Expires=%a, %d %b %Y %H:%M:%S GMT", tm);
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
    LOG_F(INFO, "Cookie added: {}", cookie.name);
}

std::map<std::string, std::string> HttpHeaderParser::parseCookies(
    const std::string& cookieStr) const {
    LOG_F(INFO, "parseCookies called with cookieStr: {}", cookieStr);

    std::map<std::string, std::string> cookies;
    std::istringstream iss(cookieStr);
    std::string pair;

    while (std::getline(iss, pair, ';')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(' '));
            key.erase(key.find_last_not_of(' ') + 1);
            value.erase(0, value.find_first_not_of(' '));
            value.erase(value.find_last_not_of(' ') + 1);

            cookies[key] = value;
            LOG_F(INFO, "Parsed cookie: {}={}", key, value);
        }
    }

    return cookies;
}

std::vector<Cookie> HttpHeaderParser::getAllCookies() const {
    LOG_F(INFO, "getAllCookies called");
    return impl_->cookies;
}

std::optional<Cookie> HttpHeaderParser::getCookie(
    const std::string& name) const {
    LOG_F(INFO, "getCookie called for name: {}", name);

    for (const auto& cookie : impl_->cookies) {
        if (cookie.name == name) {
            LOG_F(INFO, "Cookie found: {}", name);
            return cookie;
        }
    }

    LOG_F(WARNING, "Cookie not found: {}", name);
    return std::nullopt;
}

void HttpHeaderParser::removeCookie(const std::string& name) {
    LOG_F(INFO, "removeCookie called for name: {}", name);

    impl_->cookies.erase(
        std::remove_if(impl_->cookies.begin(), impl_->cookies.end(),
                       [&name](const Cookie& c) { return c.name == name; }),
        impl_->cookies.end());

    LOG_F(INFO, "Cookie removed: {}", name);
}

std::map<std::string, std::string> HttpHeaderParser::parseUrlParameters(
    const std::string& url) const {
    LOG_F(INFO, "parseUrlParameters called with url: {}", url);

    std::map<std::string, std::string> parameters;
    size_t queryStart = url.find('?');

    if (queryStart == std::string::npos) {
        LOG_F(INFO, "No query parameters found in URL");
        return parameters;
    }

    std::string queryString = url.substr(queryStart + 1);
    std::istringstream iss(queryString);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);

            // URL decode
            key = urlDecode(key);
            value = urlDecode(value);

            parameters[key] = value;
            LOG_F(INFO, "Parsed URL parameter: {}={}", key, value);
        } else {
            // Parameter without value
            parameters[urlDecode(pair)] = "";
            LOG_F(INFO, "Parsed URL parameter without value: {}", pair);
        }
    }

    return parameters;
}

void HttpHeaderParser::setMethod(HttpMethod method) {
    LOG_F(INFO, "setMethod called with method: {}", static_cast<int>(method));
    impl_->method = method;
}

HttpMethod HttpHeaderParser::getMethod() const {
    LOG_F(INFO, "getMethod called");
    return impl_->method;
}

void HttpHeaderParser::setStatus(const HttpStatus& status) {
    LOG_F(INFO, "setStatus called with status: {} {}", status.code,
          status.description);
    impl_->status = status;
    LOG_F(INFO, "Status set to: {} {}", impl_->status.code,
          impl_->status.description);
}

HttpStatus HttpHeaderParser::getStatus() const {
    LOG_F(INFO, "getStatus called");
    return impl_->status;
}

void HttpHeaderParser::setPath(const std::string& path) {
    LOG_F(INFO, "setPath called with path: {}", path);
    impl_->path = path;
    LOG_F(INFO, "Path set to: {}", impl_->path);
}

std::string HttpHeaderParser::getPath() const {
    LOG_F(INFO, "getPath called");
    return impl_->path;
}

void HttpHeaderParser::setVersion(HttpVersion version) {
    LOG_F(INFO, "setVersion called with version: {}",
          static_cast<int>(version));
    impl_->version = version;
    LOG_F(INFO, "Version set to: {}", static_cast<int>(impl_->version));
}

HttpVersion HttpHeaderParser::getVersion() const {
    LOG_F(INFO, "getVersion called");
    return impl_->version;
}

void HttpHeaderParser::setBody(const std::string& body) {
    LOG_F(INFO, "setBody called with body of length: {}", body.length());
    impl_->body = body;
    LOG_F(INFO, "Body set with length: {}", impl_->body.length());
}

std::string HttpHeaderParser::getBody() const {
    LOG_F(INFO, "getBody called");
    return impl_->body;
}

HttpMethod HttpHeaderParser::stringToMethod(const std::string& methodStr) {
    std::string upperMethod = methodStr;
    std::transform(upperMethod.begin(), upperMethod.end(), upperMethod.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upperMethod == "GET")
        return HttpMethod::GET;
    if (upperMethod == "POST")
        return HttpMethod::POST;
    if (upperMethod == "PUT")
        return HttpMethod::PUT;
    if (upperMethod == "DELETE")
        return HttpMethod::DELETE;
    if (upperMethod == "HEAD")
        return HttpMethod::HEAD;
    if (upperMethod == "OPTIONS")
        return HttpMethod::OPTIONS;
    if (upperMethod == "PATCH")
        return HttpMethod::PATCH;
    if (upperMethod == "TRACE")
        return HttpMethod::TRACE;
    if (upperMethod == "CONNECT")
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

std::string HttpHeaderParser::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            std::string hex = str.substr(i + 1, 2);
            char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
            result += decodedChar;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

std::string HttpHeaderParser::urlEncode(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 3);  // 预留足够空间

    const char hexChars[] = "0123456789ABCDEF";

    for (unsigned char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // 保持这些字符不变
            result += c;
        } else if (c == ' ') {
            // 空格转换为加号或%20
            result += '+';
        } else {
            // 其他字符转换为 %HH
            result += '%';
            result += hexChars[c >> 4];
            result += hexChars[c & 15];
        }
    }

    return result;
}

std::string HttpHeaderParser::buildRequest() const {
    LOG_F(INFO, "buildRequest called");

    std::stringstream request;

    // 构建请求行
    request << methodToString(impl_->method) << " " << impl_->path << " ";

    // 添加HTTP版本
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
            request << "HTTP/1.1";  // 默认使用 HTTP/1.1
            break;
    }
    request << "\r\n";

    // 添加请求头
    for (const auto& [key, values] : impl_->headers) {
        for (const auto& value : values) {
            request << key << ": " << value << "\r\n";
        }
    }

    // 添加空行，表示头部结束
    request << "\r\n";

    // 添加请求体（如果有）
    if (!impl_->body.empty()) {
        request << impl_->body;
    }

    LOG_F(INFO, "buildRequest completed");
    return request.str();
}

std::string HttpHeaderParser::buildResponse() const {
    LOG_F(INFO, "buildResponse called");

    std::stringstream response;

    // 构建状态行
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
            response << "HTTP/1.1";  // 默认使用 HTTP/1.1
            break;
    }
    response << " " << impl_->status.code << " " << impl_->status.description
             << "\r\n";

    // 添加响应头
    for (const auto& [key, values] : impl_->headers) {
        for (const auto& value : values) {
            response << key << ": " << value << "\r\n";
        }
    }

    // 添加空行，表示头部结束
    response << "\r\n";

    // 添加响应体（如果有）
    if (!impl_->body.empty()) {
        response << impl_->body;
    }

    LOG_F(INFO, "buildResponse completed");
    return response.str();
}

}  // namespace atom::web
