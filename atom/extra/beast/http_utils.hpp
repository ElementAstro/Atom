#ifndef ATOM_EXTRA_BEAST_HTTP_UTILS_HPP
#define ATOM_EXTRA_BEAST_HTTP_UTILS_HPP

#include "atom/extra/beast/http.hpp"

#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/http/fields.hpp>
#include <string>
#include <string_view>
#include <zlib.h>

namespace http_utils {

/**
 * @brief 创建基本身份验证的值
 * @param username 用户名
 * @param password 密码
 * @return 编码后的 Authorization 标头值
 */
inline std::string basicAuth(std::string_view username, std::string_view password) {
    std::string auth_string = std::string(username) + ":" + std::string(password);
    std::string encoded;
    
    // Beast 的 base64 编码
    encoded.resize(boost::beast::detail::base64::encoded_size(auth_string.size()));
    boost::beast::detail::base64::encode(encoded.data(), 
                                      auth_string.data(), 
                                      auth_string.size());
    
    return "Basic " + encoded;
}

/**
 * @brief 压缩数据（使用 GZIP 或 DEFLATE）
 * @param data 要压缩的数据
 * @param is_gzip 是否使用 GZIP 格式（否则使用 DEFLATE）
 * @return 压缩后的数据
 */
inline std::string compress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    
    // 初始化 zlib
    int result = deflateInit2(&stream, 
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            is_gzip ? 31 : 15,  // 31 for gzip, 15 for deflate
                            8,                  // 默认内存级别
                            Z_DEFAULT_STRATEGY);
    
    if (result != Z_OK) {
        throw std::runtime_error("Failed to initialize compression");
    }
    
    // 准备输入
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
    
    // 准备输出（预估压缩后的大小）
    std::string compressed;
    compressed.resize(deflateBound(&stream, stream.avail_in));
    
    stream.avail_out = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data());
    
    // 压缩
    result = deflate(&stream, Z_FINISH);
    
    // 清理
    deflateEnd(&stream);
    
    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to compress data");
    }
    
    // 调整输出大小
    compressed.resize(compressed.size() - stream.avail_out);
    
    return compressed;
}

/**
 * @brief 解压缩数据（GZIP 或 DEFLATE）
 * @param data 要解压缩的数据
 * @param is_gzip 是否是 GZIP 格式（否则为 DEFLATE）
 * @return 解压缩后的数据
 */
inline std::string decompress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
    
    // 初始化 zlib
    int result = inflateInit2(&stream, is_gzip ? 31 : 15);  // 31 for gzip, 15 for deflate
    
    if (result != Z_OK) {
        throw std::runtime_error("Failed to initialize decompression");
    }
    
    // 准备输入
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
    
    // 准备输出
    const size_t initial_size = data.size() * 2;  // 初始估计解压缩大小
    std::string decompressed;
    decompressed.resize(initial_size);
    
    stream.avail_out = static_cast<uInt>(decompressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(decompressed.data());
    
    // 解压缩（可能需要多次调用 inflate）
    std::size_t total_out = 0;
    do {
        result = inflate(&stream, Z_NO_FLUSH);
        
        if (result == Z_NEED_DICT || result == Z_DATA_ERROR || result == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("Failed to decompress data");
        }
        
        if (stream.avail_out == 0) {
            // 如果输出缓冲区已满，需要扩展
            total_out += decompressed.size();
            decompressed.resize(decompressed.size() * 2);
            
            stream.avail_out = static_cast<uInt>(decompressed.size() - total_out);
            stream.next_out = reinterpret_cast<Bytef*>(decompressed.data() + total_out);
        }
    } while (result != Z_STREAM_END);
    
    // 调整输出大小
    total_out += decompressed.size() - stream.avail_out;
    decompressed.resize(total_out);
    
    // 清理
    inflateEnd(&stream);
    
    return decompressed;
}

/**
 * @brief 转义 URL 参数
 * @param input 未转义的输入
 * @return URL 转义后的字符串
 */
inline std::string urlEncode(std::string_view input) {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() * 3);
    
    for (auto c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            result += '%';
            result += hex[(c & 0xF0) >> 4];
            result += hex[c & 0x0F];
        }
    }
    
    return result;
}

/**
 * @brief 解析 URL 参数
 * @param input URL 编码的字符串
 * @return 解码后的字符串
 */
inline std::string urlDecode(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    
    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        
        if (c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < input.size()) {
            int value = 0;
            for (int j = 0; j < 2; ++j) {
                char hex_char = input[i + j + 1];
                value <<= 4;
                
                if (hex_char >= '0' && hex_char <= '9') {
                    value |= hex_char - '0';
                } else if (hex_char >= 'A' && hex_char <= 'F') {
                    value |= hex_char - 'A' + 10;
                } else if (hex_char >= 'a' && hex_char <= 'f') {
                    value |= hex_char - 'a' + 10;
                } else {
                    // 无效的 hex 编码，保持原样
                    value = -1;
                    break;
                }
            }
            
            if (value >= 0) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += c;
            }
        } else {
            result += c;
        }
    }
    
    return result;
}

/**
 * @brief 构建 URL 查询字符串
 * @param params 参数映射
 * @return 编码后的查询字符串（不包含前导 '?'）
 */
inline std::string buildQueryString(const std::unordered_map<std::string, std::string>& params) {
    std::string query;
    bool first = true;
    
    for (const auto& [key, value] : params) {
        if (!first) {
            query += '&';
        }
        first = false;
        
        query += urlEncode(key) + '=' + urlEncode(value);
    }
    
    return query;
}

/**
 * @brief 解析 Cookie 字符串
 * @param cookie_header Cookie 标头值
 * @return 解析后的 Cookie 名称和值的映射
 */
inline std::unordered_map<std::string, std::string> parseCookies(std::string_view cookie_header) {
    std::unordered_map<std::string, std::string> cookies;
    
    std::size_t pos = 0;
    std::size_t end;
    
    while (pos < cookie_header.size()) {
        // 查找分号
        end = cookie_header.find(';', pos);
        if (end == std::string_view::npos) {
            end = cookie_header.size();
        }
        
        // 提取 cookie 字符串
        std::string_view cookie_str = cookie_header.substr(pos, end - pos);
        
        // 查找等号
        std::size_t eq_pos = cookie_str.find('=');
        if (eq_pos != std::string_view::npos) {
            // 提取名称和值
            std::string_view name = cookie_str.substr(0, eq_pos);
            std::string_view value = cookie_str.substr(eq_pos + 1);
            
            // 去除前导和尾随空格
            while (!name.empty() && std::isspace(name.front())) {
                name.remove_prefix(1);
            }
            
            while (!value.empty() && std::isspace(value.front())) {
                value.remove_prefix(1);
            }
            
            while (!value.empty() && std::isspace(value.back())) {
                value.remove_suffix(1);
            }
            
            cookies[std::string(name)] = std::string(value);
        }
        
        // 移动到下一个 cookie
        pos = end + 1;
        
        // 跳过前导空格
        while (pos < cookie_header.size() && std::isspace(cookie_header[pos])) {
            ++pos;
        }
    }
    
    return cookies;
}

/**
 * @brief 构建 Cookie 字符串
 * @param cookies Cookie 名称和值的映射
 * @return 格式化后的 Cookie 字符串
 */
inline std::string buildCookieString(const std::unordered_map<std::string, std::string>& cookies) {
    std::string cookie_str;
    bool first = true;
    
    for (const auto& [name, value] : cookies) {
        if (!first) {
            cookie_str += "; ";
        }
        first = false;
        
        cookie_str += name + "=" + value;
    }
    
    return cookie_str;
}

/**
 * @brief Cookie 管理器
 */
class CookieManager {
public:
    /**
     * @brief 从响应中提取 Cookie
     * @param host 主机
     * @param response HTTP 响应
     */
    void extractCookies(std::string_view host, const http::response<http::string_body>& response) {
        auto it = response.find(http::field::set_cookie);
        while (it != response.end() && beast::iequals(it->name_string(), "set-cookie")) {
            parseCookie(std::string(host), std::string(it->value()));
            ++it;
        }
    }
    
    /**
     * @brief 向请求中添加 Cookie
     * @param host 主机
     * @param request HTTP 请求
     */
    void addCookiesToRequest(std::string_view host, http::request<http::string_body>& request) {
        // 查找适用于此主机的所有 cookie
        std::unordered_map<std::string, std::string> applicable_cookies;
        
        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [cookie_host, cookie_name] = cookie_key;
            
            if (hostMatches(std::string(host), cookie_host)) {
                applicable_cookies[cookie_name] = cookie_value.value;
            }
        }
        
        // 如果有适用的 cookie，添加到请求中
        if (!applicable_cookies.empty()) {
            request.set(http::field::cookie, buildCookieString(applicable_cookies));
        }
    }
    
    /**
     * @brief 清除所有 Cookie
     */
    void clearCookies() {
        cookies_.clear();
    }
    
    /**
     * @brief 获取特定 Cookie 的值
     * @param host 主机
     * @param name Cookie 名称
     * @return Cookie 值，如果不存在则返回空字符串
     */
    std::string getCookie(std::string_view host, std::string_view name) {
        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [cookie_host, cookie_name] = cookie_key;
            
            if (hostMatches(std::string(host), cookie_host) && cookie_name == name) {
                return cookie_value.value;
            }
        }
        
        return "";
    }
    
private:
    struct CookieValue {
        std::string value;
        std::string path;
        std::chrono::system_clock::time_point expires;
        bool secure = false;
        bool http_only = false;
    };
    
    // 以 (host, name) 为键存储 cookie
    std::unordered_map<std::pair<std::string, std::string>, CookieValue, 
                      boost::hash<std::pair<std::string, std::string>>> cookies_;
    
    void parseCookie(const std::string& host, const std::string& set_cookie_str) {
        // 提取 cookie 的名称和值
        std::size_t pos = set_cookie_str.find('=');
        if (pos == std::string::npos) {
            return;  // 无效的 cookie
        }
        
        std::string name = set_cookie_str.substr(0, pos);
        
        // 查找下一个分号
        std::size_t value_end = set_cookie_str.find(';', pos + 1);
        std::string value;
        
        if (value_end == std::string::npos) {
            value = set_cookie_str.substr(pos + 1);
        } else {
            value = set_cookie_str.substr(pos + 1, value_end - (pos + 1));
        }
        
        // 创建 cookie 值
        CookieValue cookie_value;
        cookie_value.value = value;
        cookie_value.path = "/";  // 默认路径
        
        // 解析其他属性
        if (value_end != std::string::npos) {
            std::string attrs = set_cookie_str.substr(value_end + 1);
            parseAttributes(attrs, cookie_value);
        }
        
        // 存储 cookie
        cookies_[{host, name}] = std::move(cookie_value);
    }
    
    void parseAttributes(const std::string& attrs, CookieValue& cookie_value) {
        std::size_t pos = 0;
        
        while (pos < attrs.size()) {
            // 跳过前导空格
            while (pos < attrs.size() && std::isspace(attrs[pos])) {
                ++pos;
            }
            
            // 查找下一个分号
            std::size_t attr_end = attrs.find(';', pos);
            if (attr_end == std::string::npos) {
                attr_end = attrs.size();
            }
            
            // 提取属性字符串
            std::string attr = attrs.substr(pos, attr_end - pos);
            
            // 解析属性
            parseAttribute(attr, cookie_value);
            
            // 移动到下一个属性
            pos = attr_end + 1;
        }
    }
    
    void parseAttribute(const std::string& attr, CookieValue& cookie_value) {
        // 查找等号
        std::size_t eq_pos = attr.find('=');
        
        if (eq_pos == std::string::npos) {
            // 标志属性（无值）
            std::string name = attr;
            trim(name);
            
            if (iequals(name, "secure")) {
                cookie_value.secure = true;
            } else if (iequals(name, "httponly")) {
                cookie_value.http_only = true;
            }
        } else {
            // 键值属性
            std::string name = attr.substr(0, eq_pos);
            std::string value = attr.substr(eq_pos + 1);
            
            trim(name);
            trim(value);
            
            if (iequals(name, "path")) {
                cookie_value.path = value;
            } else if (iequals(name, "expires")) {
                // 解析过期时间（简化处理）
                // 实际实现应使用适当的日期解析
                // 这里只是一个占位符
            }
        }
    }
    
    static void trim(std::string& s) {
        // 去除前导空格
        auto start = std::find_if_not(s.begin(), s.end(), 
                                   [](unsigned char c) { return std::isspace(c); });
        s.erase(s.begin(), start);
        
        // 去除尾随空格
        auto end = std::find_if_not(s.rbegin(), s.rend(),
                                 [](unsigned char c) { return std::isspace(c); }).base();
        s.erase(end, s.end());
    }
    
    static bool iequals(const std::string& a, const std::string& b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                       [](char a, char b) {
                           return std::tolower(a) == std::tolower(b);
                       });
    }
    
    static bool hostMatches(const std::string& request_host, const std::string& cookie_host) {
        // 完全匹配
        if (request_host == cookie_host) {
            return true;
        }
        
        // 域匹配（.example.com 匹配 sub.example.com）
        if (cookie_host.size() > 1 && cookie_host[0] == '.') {
            std::string domain = cookie_host.substr(1);
            
            // 检查请求主机是否以 cookie 域结尾
            if (request_host.size() >= domain.size() &&
                request_host.substr(request_host.size() - domain.size()) == domain) {
                return true;
            }
        }
        
        return false;
    }
};

} // namespace http_utils

#endif // ATOM_EXTRA_BEAST_HTTP_UTILS_HPP
