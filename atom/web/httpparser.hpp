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
#include <tuple>
#include <unordered_map>
#include <vector>

namespace atom::web {

/**
 * @brief HTTP方法枚举
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
 * @brief HTTP版本枚举
 */
enum class HttpVersion { HTTP_1_0, HTTP_1_1, HTTP_2_0, HTTP_3_0, UNKNOWN };

/**
 * @brief HTTP状态码及其对应描述的结构
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
 * @brief Cookie结构体，用于表示HTTP Cookie
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
     * @brief 解析完整的HTTP请求
     * @param rawRequest 原始HTTP请求字符串
     * @return 是否成功解析
     */
    bool parseRequest(const std::string& rawRequest);

    /**
     * @brief 解析完整的HTTP响应
     * @param rawResponse 原始HTTP响应字符串
     * @return 是否成功解析
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
     * @brief 获取某个头部字段的第一个值
     * @param key 头部字段名
     * @return 头部字段的第一个值，如果不存在则返回空
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
     * @brief 添加一个Cookie
     * @param cookie Cookie对象
     */
    void addCookie(const Cookie& cookie);

    /**
     * @brief 解析Cookie字符串
     * @param cookieStr Cookie字符串
     * @return 解析后的Cookie键值对
     */
    [[nodiscard]] std::map<std::string, std::string> parseCookies(
        const std::string& cookieStr) const;

    /**
     * @brief 获取所有Cookie
     * @return 所有Cookie的列表
     */
    [[nodiscard]] std::vector<Cookie> getAllCookies() const;

    /**
     * @brief 获取指定名称的Cookie
     * @param name Cookie名称
     * @return Cookie对象（如果存在）
     */
    [[nodiscard]] std::optional<Cookie> getCookie(
        const std::string& name) const;

    /**
     * @brief 删除指定Cookie
     * @param name Cookie名称
     */
    void removeCookie(const std::string& name);

    /**
     * @brief 解析URL查询参数
     * @param url 包含查询参数的URL
     * @return 解析后的参数键值对
     */
    [[nodiscard]] std::map<std::string, std::string> parseUrlParameters(
        const std::string& url) const;

    /**
     * @brief 设置HTTP方法
     * @param method HTTP方法
     */
    void setMethod(HttpMethod method);

    /**
     * @brief 获取HTTP方法
     * @return 当前HTTP方法
     */
    [[nodiscard]] HttpMethod getMethod() const;

    /**
     * @brief 将字符串转换为HTTP方法枚举
     * @param methodStr 方法字符串
     * @return 对应的HTTP方法枚举
     */
    [[nodiscard]] static HttpMethod stringToMethod(
        const std::string& methodStr);

    /**
     * @brief 将HTTP方法枚举转换为字符串
     * @param method HTTP方法枚举
     * @return 对应的方法字符串
     */
    [[nodiscard]] static std::string methodToString(HttpMethod method);

    /**
     * @brief 设置HTTP状态
     * @param status HTTP状态对象
     */
    void setStatus(const HttpStatus& status);

    /**
     * @brief 获取HTTP状态
     * @return 当前HTTP状态对象
     */
    [[nodiscard]] HttpStatus getStatus() const;

    /**
     * @brief 设置URL路径
     * @param path URL路径
     */
    void setPath(const std::string& path);

    /**
     * @brief 获取URL路径
     * @return 当前URL路径
     */
    [[nodiscard]] std::string getPath() const;

    /**
     * @brief 设置HTTP版本
     * @param version HTTP版本
     */
    void setVersion(HttpVersion version);

    /**
     * @brief 获取HTTP版本
     * @return 当前HTTP版本
     */
    [[nodiscard]] HttpVersion getVersion() const;

    /**
     * @brief 设置请求体
     * @param body 请求体内容
     */
    void setBody(const std::string& body);

    /**
     * @brief 获取请求体内容
     * @return 请求体内容
     */
    [[nodiscard]] std::string getBody() const;

    /**
     * @brief 构建HTTP请求字符串
     * @return 构建的HTTP请求
     */
    [[nodiscard]] std::string buildRequest() const;

    /**
     * @brief 构建HTTP响应字符串
     * @return 构建的HTTP响应
     */
    [[nodiscard]] std::string buildResponse() const;

    /**
     * @brief URL编码
     * @param str 要编码的字符串
     * @return 编码后的字符串
     */
    [[nodiscard]] static std::string urlEncode(const std::string& str);

    /**
     * @brief URL解码
     * @param str 要解码的字符串
     * @return 解码后的字符串
     */
    [[nodiscard]] static std::string urlDecode(const std::string& str);

private:
    class HttpHeaderParserImpl;
    std::shared_ptr<HttpHeaderParserImpl> impl_;  // Pointer to implementation
};
}  // namespace atom::web

#endif  // ATOM_WEB_HTTP_PARSER_HPP
