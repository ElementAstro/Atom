#ifndef ATOM_EXTRA_BEAST_HTTP_UTILS_HPP
#define ATOM_EXTRA_BEAST_HTTP_UTILS_HPP

#include <zlib.h>
#include <algorithm>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/http.hpp>
#include <boost/functional/hash.hpp>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * @brief Provides utility functions for HTTP operations.
 */
namespace http_utils {

namespace beast = boost::beast;
namespace http = beast::http;

/**
 * @brief Creates a Basic Authentication Authorization header value.
 * @param username The username.
 * @param password The password.
 * @return The encoded Authorization header value.
 */
inline std::string basicAuth(std::string_view username,
                             std::string_view password) {
    std::string auth_string;
    auth_string.reserve(username.size() + password.size() + 1);
    auth_string.append(username).append(":").append(password);

    std::string encoded;
    encoded.resize(beast::detail::base64::encoded_size(auth_string.size()));
    beast::detail::base64::encode(encoded.data(), auth_string.data(),
                                  auth_string.size());

    encoded.insert(0, "Basic ");
    return encoded;
}

/**
 * @brief Compresses data using GZIP or DEFLATE algorithm.
 * @param data The data to compress.
 * @param is_gzip If true, uses GZIP format; otherwise, uses DEFLATE.
 * @return The compressed data as a string.
 * @throws std::runtime_error If compression fails.
 */
inline std::string compress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    const int windowBits = is_gzip ? 31 : 15;
    int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                              windowBits, 8, Z_DEFAULT_STRATEGY);

    if (result != Z_OK) {
        throw std::runtime_error(
            "Failed to initialize compression: zlib error " +
            std::to_string(result));
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in =
        const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));

    std::string compressed;
    compressed.resize(deflateBound(&stream, stream.avail_in));

    stream.avail_out = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data());

    result = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to compress data: zlib error " +
                                 std::to_string(result));
    }

    compressed.resize(stream.total_out);
    return compressed;
}

/**
 * @brief Decompresses data compressed with GZIP or DEFLATE.
 * @param data The compressed data.
 * @param is_gzip If true, assumes GZIP format; otherwise, assumes DEFLATE.
 * @return The decompressed data as a string.
 * @throws std::runtime_error If decompression fails.
 */
inline std::string decompress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    const int windowBits = is_gzip ? 47 : 15;
    int result = inflateInit2(&stream, windowBits);

    if (result != Z_OK) {
        throw std::runtime_error(
            "Failed to initialize decompression: zlib error " +
            std::to_string(result));
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in =
        const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));

    std::string decompressed;
    std::vector<char> buffer(
        std::max(data.size() * 2, static_cast<size_t>(1024)));
    std::size_t total_out = 0;

    do {
        if (total_out >= buffer.size()) {
            buffer.resize(buffer.size() * 2);
        }

        stream.avail_out = static_cast<uInt>(buffer.size() - total_out);
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data() + total_out);

        result = inflate(&stream, Z_NO_FLUSH);

        if (result == Z_NEED_DICT || result == Z_DATA_ERROR ||
            result == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("Failed to decompress data: zlib error " +
                                     std::to_string(result));
        }

        total_out = stream.total_out;
    } while (result != Z_STREAM_END);

    inflateEnd(&stream);
    decompressed.assign(buffer.data(), total_out);
    return decompressed;
}

/**
 * @brief URL-encodes a string according to RFC 3986.
 * @param input The string to encode.
 * @return The URL-encoded string.
 */
inline std::string urlEncode(std::string_view input) {
    static constexpr char hex_chars[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() * 3);

    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            result += '%';
            result += hex_chars[(c >> 4) & 0xF];
            result += hex_chars[c & 0xF];
        }
    }

    return result;
}

/**
 * @brief URL-decodes a string.
 * @param input The URL-encoded string.
 * @return The decoded string.
 */
inline std::string urlDecode(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < input.size()) {
            const char h1 = input[i + 1];
            const char h2 = input[i + 2];

            const auto hex_to_int = [](char h) -> int {
                if (h >= '0' && h <= '9')
                    return h - '0';
                if (h >= 'A' && h <= 'F')
                    return h - 'A' + 10;
                if (h >= 'a' && h <= 'f')
                    return h - 'a' + 10;
                return -1;
            };

            const int v1 = hex_to_int(h1);
            const int v2 = hex_to_int(h2);

            if (v1 >= 0 && v2 >= 0) {
                result += static_cast<char>((v1 << 4) | v2);
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
 * @brief Builds a URL query string from a map of parameters.
 * @param params A map of parameter names and values.
 * @return The formatted query string without leading '?'.
 */
inline std::string buildQueryString(
    const std::unordered_map<std::string, std::string>& params) {
    std::string query;
    query.reserve(params.size() * 32);

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
 * @brief Parses a Cookie header string into a map of cookie names and values.
 * @param cookie_header The value of the Cookie HTTP header.
 * @return An unordered map containing the parsed cookie names and values.
 */
inline std::unordered_map<std::string, std::string> parseCookies(
    std::string_view cookie_header) {
    std::unordered_map<std::string, std::string> cookies;
    std::size_t pos = 0;

    const auto trim_ws = [](std::string_view& sv) {
        while (!sv.empty() &&
               std::isspace(static_cast<unsigned char>(sv.front()))) {
            sv.remove_prefix(1);
        }
        while (!sv.empty() &&
               std::isspace(static_cast<unsigned char>(sv.back()))) {
            sv.remove_suffix(1);
        }
    };

    while (pos < cookie_header.size()) {
        const std::size_t end = cookie_header.find(';', pos);
        const std::size_t actual_end =
            (end == std::string_view::npos) ? cookie_header.size() : end;

        std::string_view cookie_pair =
            cookie_header.substr(pos, actual_end - pos);
        const std::size_t eq_pos = cookie_pair.find('=');

        if (eq_pos != std::string_view::npos) {
            std::string_view name = cookie_pair.substr(0, eq_pos);
            std::string_view value = cookie_pair.substr(eq_pos + 1);

            trim_ws(name);
            trim_ws(value);

            if (!name.empty()) {
                cookies.emplace(std::string(name), std::string(value));
            }
        }

        pos = actual_end + 1;
    }

    return cookies;
}

/**
 * @brief Builds a Cookie header string from a map of cookie names and values.
 * @param cookies A map containing cookie names and values.
 * @return The formatted Cookie header string.
 */
inline std::string buildCookieString(
    const std::unordered_map<std::string, std::string>& cookies) {
    std::string cookie_str;
    cookie_str.reserve(cookies.size() * 32);

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
 * @brief Manages HTTP cookies with basic storage and retrieval functionality.
 */
class CookieManager {
private:
    struct CookieValue {
        std::string value;
        std::string path = "/";
        std::string domain;
        bool secure = false;
        bool http_only = false;
    };

    using CookieKey = std::tuple<std::string, std::string, std::string>;

    struct CookieKeyHash {
        std::size_t operator()(const CookieKey& k) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, std::get<0>(k));
            boost::hash_combine(seed, std::get<1>(k));
            boost::hash_combine(seed, std::get<2>(k));
            return seed;
        }
    };

    std::unordered_map<CookieKey, CookieValue, CookieKeyHash> cookies_;

    static void trim(std::string& s) {
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(),
                s.end());
    }

    static bool iequals(const std::string& a, const std::string& b) {
        return std::equal(
            a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            });
    }

    void parseAttribute(const std::string& attr, CookieValue& cookie_value) {
        const std::size_t eq_pos = attr.find('=');
        std::string name =
            (eq_pos == std::string::npos) ? attr : attr.substr(0, eq_pos);
        std::string value =
            (eq_pos == std::string::npos) ? "" : attr.substr(eq_pos + 1);

        trim(name);
        trim(value);

        if (iequals(name, "Secure")) {
            cookie_value.secure = true;
        } else if (iequals(name, "HttpOnly")) {
            cookie_value.http_only = true;
        } else if (iequals(name, "Path")) {
            cookie_value.path = value.empty() ? "/" : value;
        } else if (iequals(name, "Domain")) {
            cookie_value.domain =
                (!value.empty() && value[0] == '.') ? value.substr(1) : value;
        }
    }

    void parseSetCookieString(const std::string& request_host,
                              const std::string& set_cookie_str) {
        const std::size_t first_semi = set_cookie_str.find(';');
        const std::string name_value_pair =
            set_cookie_str.substr(0, first_semi);
        const std::size_t eq_pos = name_value_pair.find('=');

        if (eq_pos == std::string::npos) {
            return;
        }

        std::string name = name_value_pair.substr(0, eq_pos);
        std::string value = name_value_pair.substr(eq_pos + 1);
        trim(name);
        trim(value);

        if (name.empty()) {
            return;
        }

        CookieValue cookie_value;
        cookie_value.value = std::move(value);
        cookie_value.domain = request_host;

        if (first_semi != std::string::npos) {
            const std::string attrs_part =
                set_cookie_str.substr(first_semi + 1);
            std::size_t pos = 0;

            while (pos < attrs_part.size()) {
                while (
                    pos < attrs_part.size() &&
                    (std::isspace(attrs_part[pos]) || attrs_part[pos] == ';')) {
                    ++pos;
                }
                if (pos >= attrs_part.size())
                    break;

                const std::size_t next_semi = attrs_part.find(';', pos);
                const std::string current_attr = attrs_part.substr(
                    pos, next_semi == std::string::npos ? std::string::npos
                                                        : next_semi - pos);

                parseAttribute(current_attr, cookie_value);

                if (next_semi == std::string::npos)
                    break;
                pos = next_semi + 1;
            }
        }

        if (!cookie_value.domain.empty() && cookie_value.domain[0] == '.') {
            cookie_value.domain = cookie_value.domain.substr(1);
        }

        if (!domainMatches(request_host, cookie_value.domain)) {
            if (!cookie_value.domain.empty() &&
                cookie_value.domain != request_host) {
                return;
            }
        }

        const CookieKey key = {cookie_value.domain, cookie_value.path, name};
        cookies_[key] = std::move(cookie_value);
    }

    static bool domainMatches(const std::string& request_host,
                              const std::string& cookie_domain) {
        if (cookie_domain.empty()) {
            return true;
        }

        if (iequals(request_host, cookie_domain)) {
            return true;
        }

        const std::string domain_suffix = "." + cookie_domain;
        if (request_host.size() > domain_suffix.size() &&
            iequals(
                request_host.substr(request_host.size() - domain_suffix.size()),
                domain_suffix)) {
            const bool domain_is_ip =
                std::all_of(cookie_domain.begin(), cookie_domain.end(),
                            [](char c) { return std::isdigit(c) || c == '.'; });
            return !domain_is_ip;
        }

        return false;
    }

    static bool pathMatches(const std::string& request_path,
                            const std::string& cookie_path) {
        if (request_path == cookie_path) {
            return true;
        }

        if (request_path.rfind(cookie_path, 0) == 0) {
            if (cookie_path.back() == '/') {
                return true;
            }
            if (request_path.size() > cookie_path.size() &&
                request_path[cookie_path.size()] == '/') {
                return true;
            }
        }

        return false;
    }

public:
    /**
     * @brief Extracts cookies from the Set-Cookie headers of an HTTP response.
     * @param request_host The original host the request was sent to.
     * @param response The HTTP response object.
     */
    void extractCookies(std::string_view request_host,
                        const http::response<http::string_body>& response) {
        for (const auto& field : response) {
            if (beast::iequals(field.name_string(), "set-cookie")) {
                parseSetCookieString(std::string(request_host),
                                     std::string(field.value()));
            }
        }
    }

    /**
     * @brief Adds applicable stored cookies to the Cookie header of an HTTP
     * request.
     * @param request_host The host the request is being sent to.
     * @param request_path The path the request is being sent to.
     * @param is_secure True if the connection is secure (HTTPS).
     * @param request The HTTP request object to modify.
     */
    void addCookiesToRequest(std::string_view request_host_sv,
                             std::string_view request_path_sv, bool is_secure,
                             http::request<http::string_body>& request) {
        const std::string request_host = std::string(request_host_sv);
        const std::string request_path =
            request_path_sv.empty() ? "/" : std::string(request_path_sv);

        std::unordered_map<std::string, std::string> applicable_cookies;

        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [domain, path, name] = cookie_key;

            if (cookie_value.secure && !is_secure) {
                continue;
            }

            if (!domainMatches(request_host, domain)) {
                continue;
            }

            if (!pathMatches(request_path, path)) {
                continue;
            }

            applicable_cookies[name] = cookie_value.value;
        }

        if (!applicable_cookies.empty()) {
            request.set(http::field::cookie,
                        buildCookieString(applicable_cookies));
        }
    }

    /**
     * @brief Clears all cookies stored in the manager.
     */
    void clearCookies() { cookies_.clear(); }

    /**
     * @brief Gets the value of a specific cookie for a given host and name.
     * @param host The host context.
     * @param name The name of the cookie.
     * @return The cookie value if found, otherwise an empty string.
     */
    std::string getCookie(std::string_view host_sv, std::string_view name_sv) {
        const std::string host = std::string(host_sv);
        const std::string name = std::string(name_sv);

        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [domain, path, cookie_name] = cookie_key;
            if (cookie_name == name && domainMatches(host, domain)) {
                return cookie_value.value;
            }
        }
        return "";
    }
};

}  // namespace http_utils

#endif
