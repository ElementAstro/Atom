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
#include "atom/utils/string.hpp"

/**
 * @brief High-performance HTTP utility functions for web operations
 *
 * Provides optimized implementations for common HTTP operations including
 * authentication, compression, URL encoding, and cookie management.
 */
namespace http_utils {

namespace beast = boost::beast;
namespace http = beast::http;

/**
 * @brief Creates a Base64-encoded Basic Authentication header value
 * @param username The authentication username
 * @param password The authentication password
 * @return The complete Authorization header value with "Basic " prefix
 * @throws std::runtime_error If encoding fails
 */
inline std::string basicAuth(std::string_view username,
                             std::string_view password) {
    std::string credentials;
    credentials.reserve(username.size() + password.size() + 1);
    credentials.append(username).append(":").append(password);

    std::string encoded;
    encoded.resize(beast::detail::base64::encoded_size(credentials.size()));

    auto result = beast::detail::base64::encode(
        encoded.data(), credentials.data(), credentials.size());

    if (result == 0) {
        throw std::runtime_error("Base64 encoding failed");
    }

    return "Basic " + encoded;
}

/**
 * @brief Compresses data using optimized GZIP or DEFLATE algorithms
 * @param data The input data to compress
 * @param use_gzip If true, uses GZIP format; otherwise uses raw DEFLATE
 * @param compression_level Compression level (0-9, default
 * Z_DEFAULT_COMPRESSION)
 * @return The compressed data as a string
 * @throws std::runtime_error If compression fails
 */
inline std::string compress(std::string_view data, bool use_gzip = true,
                            int compression_level = Z_DEFAULT_COMPRESSION) {
    if (data.empty())
        return {};

    z_stream stream{};
    const int window_bits = use_gzip ? 31 : -15;  // Negative for raw deflate

    int result = deflateInit2(&stream, compression_level, Z_DEFLATED,
                              window_bits, 8, Z_DEFAULT_STRATEGY);
    if (result != Z_OK) {
        throw std::runtime_error("Compression initialization failed: " +
                                 std::to_string(result));
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in =
        const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));

    std::string compressed;
    compressed.reserve(deflateBound(&stream, stream.avail_in));

    constexpr size_t chunk_size = 16384;
    std::vector<Bytef> buffer(chunk_size);

    do {
        stream.avail_out = chunk_size;
        stream.next_out = buffer.data();

        result = deflate(&stream, Z_FINISH);
        if (result == Z_STREAM_ERROR) {
            deflateEnd(&stream);
            throw std::runtime_error("Compression stream error");
        }

        size_t compressed_bytes = chunk_size - stream.avail_out;
        compressed.append(reinterpret_cast<const char*>(buffer.data()),
                          compressed_bytes);

    } while (stream.avail_out == 0);

    deflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Compression incomplete: " +
                                 std::to_string(result));
    }

    return compressed;
}

/**
 * @brief Decompresses GZIP or DEFLATE compressed data with automatic format
 * detection
 * @param data The compressed input data
 * @param use_gzip If true, expects GZIP format; otherwise expects raw DEFLATE
 * @param max_size Maximum allowed decompressed size (default 100MB)
 * @return The decompressed data as a string
 * @throws std::runtime_error If decompression fails or exceeds size limit
 */
inline std::string decompress(std::string_view data, bool use_gzip = true,
                              size_t max_size = 100 * 1024 * 1024) {
    if (data.empty())
        return {};

    z_stream stream{};
    const int window_bits =
        use_gzip ? 47 : -15;  // Auto-detect GZIP, or raw deflate

    int result = inflateInit2(&stream, window_bits);
    if (result != Z_OK) {
        throw std::runtime_error("Decompression initialization failed: " +
                                 std::to_string(result));
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));

    std::string decompressed;
    constexpr size_t chunk_size = 16384;
    std::vector<Bytef> buffer(chunk_size);

    do {
        stream.avail_out = chunk_size;
        stream.next_out = buffer.data();

        result = inflate(&stream, Z_NO_FLUSH);

        switch (result) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                inflateEnd(&stream);
                throw std::runtime_error("Decompression error: " +
                                         std::to_string(result));
        }

        size_t decompressed_bytes = chunk_size - stream.avail_out;

        if (decompressed.size() + decompressed_bytes > max_size) {
            inflateEnd(&stream);
            throw std::runtime_error(
                "Decompressed data exceeds maximum size limit");
        }

        decompressed.append(reinterpret_cast<const char*>(buffer.data()),
                            decompressed_bytes);

    } while (stream.avail_out == 0);

    inflateEnd(&stream);
    return decompressed;
}

/**
 * @brief High-performance URL encoding according to RFC 3986
 * @param input The string to URL-encode
 * @return The percent-encoded string
 */
inline std::string urlEncode(std::string_view input) {
    static constexpr char hex_digits[] = "0123456789ABCDEF";
    static constexpr bool unreserved[256] = {
        // Initialize lookup table for unreserved characters
        ['-'] = true, ['_'] = true, ['.'] = true, ['~'] = true, ['A'] = true,
        ['B'] = true, ['C'] = true, ['D'] = true, ['E'] = true, ['F'] = true,
        ['G'] = true, ['H'] = true, ['I'] = true, ['J'] = true, ['K'] = true,
        ['L'] = true, ['M'] = true, ['N'] = true, ['O'] = true, ['P'] = true,
        ['Q'] = true, ['R'] = true, ['S'] = true, ['T'] = true, ['U'] = true,
        ['V'] = true, ['W'] = true, ['X'] = true, ['Y'] = true, ['Z'] = true,
        ['a'] = true, ['b'] = true, ['c'] = true, ['d'] = true, ['e'] = true,
        ['f'] = true, ['g'] = true, ['h'] = true, ['i'] = true, ['j'] = true,
        ['k'] = true, ['l'] = true, ['m'] = true, ['n'] = true, ['o'] = true,
        ['p'] = true, ['q'] = true, ['r'] = true, ['s'] = true, ['t'] = true,
        ['u'] = true, ['v'] = true, ['w'] = true, ['x'] = true, ['y'] = true,
        ['z'] = true, ['0'] = true, ['1'] = true, ['2'] = true, ['3'] = true,
        ['4'] = true, ['5'] = true, ['6'] = true, ['7'] = true, ['8'] = true,
        ['9'] = true};

    std::string result;
    result.reserve(input.size() * 2);  // Reasonable estimate

    for (auto byte : input) {
        auto uc = static_cast<unsigned char>(byte);
        if (unreserved[uc]) {
            result += static_cast<char>(uc);
        } else {
            result += '%';
            result += hex_digits[uc >> 4];
            result += hex_digits[uc & 0x0F];
        }
    }

    return result;
}

/**
 * @brief High-performance URL decoding with validation
 * @param input The percent-encoded string to decode
 * @return The decoded string
 * @throws std::invalid_argument If the input contains invalid encoding
 */
inline std::string urlDecode(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    constexpr auto hex_to_int = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < input.size()) {
            int high = hex_to_int(input[i + 1]);
            int low = hex_to_int(input[i + 2]);

            if (high >= 0 && low >= 0) {
                result += static_cast<char>((high << 4) | low);
                i += 2;
            } else {
                throw std::invalid_argument("Invalid percent-encoded sequence");
            }
        } else {
            result += c;
        }
    }

    return result;
}

/**
 * @brief Builds an optimized URL query string from parameters
 * @param params Map of parameter names to values
 * @return The formatted query string without leading '?'
 */
inline std::string buildQueryString(
    const std::unordered_map<std::string, std::string>& params) {
    if (params.empty())
        return {};

    std::string query;
    query.reserve(params.size() * 32);  // Reasonable estimate

    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first)
            query += '&';
        first = false;

        query += urlEncode(key);
        query += '=';
        query += urlEncode(value);
    }

    return query;
}

/**
 * @brief Fast cookie header parser with validation
 * @param cookie_header The Cookie header value
 * @return Map of cookie names to values
 */
inline std::unordered_map<std::string, std::string> parseCookies(
    std::string_view cookie_header) {
    std::unordered_map<std::string, std::string> cookies;

    size_t pos = 0;
    while (pos < cookie_header.size()) {
        // Skip whitespace
        while (pos < cookie_header.size() && std::isspace(cookie_header[pos])) {
            ++pos;
        }
        if (pos >= cookie_header.size())
            break;

        // Find next semicolon or end
        size_t end = cookie_header.find(';', pos);
        if (end == std::string_view::npos)
            end = cookie_header.size();

        // Extract name=value pair
        std::string_view pair = cookie_header.substr(pos, end - pos);
        size_t eq_pos = pair.find('=');

        if (eq_pos != std::string_view::npos) {
            std::string_view name = pair.substr(0, eq_pos);
            std::string_view value = pair.substr(eq_pos + 1);

            // Trim whitespace
            while (!name.empty() && std::isspace(name.front()))
                name.remove_prefix(1);
            while (!name.empty() && std::isspace(name.back()))
                name.remove_suffix(1);
            while (!value.empty() && std::isspace(value.front()))
                value.remove_prefix(1);
            while (!value.empty() && std::isspace(value.back()))
                value.remove_suffix(1);

            if (!name.empty()) {
                cookies.emplace(name, value);
            }
        }

        pos = end + 1;
    }

    return cookies;
}

/**
 * @brief Builds optimized Cookie header string
 * @param cookies Map of cookie names to values
 * @return The formatted Cookie header value
 */
inline std::string buildCookieString(
    const std::unordered_map<std::string, std::string>& cookies) {
    if (cookies.empty())
        return {};

    std::string result;
    result.reserve(cookies.size() * 32);

    bool first = true;
    for (const auto& [name, value] : cookies) {
        if (!first)
            result += "; ";
        first = false;
        result += name + "=" + value;
    }

    return result;
}

/**
 * @brief Advanced HTTP cookie management with RFC compliance
 *
 * Provides comprehensive cookie storage, retrieval, and validation
 * according to HTTP cookie specifications with domain and path matching.
 */
class CookieManager {
private:
    struct Cookie {
        std::string value;
        std::string path = "/";
        std::string domain;
        bool secure = false;
        bool http_only = false;

        Cookie() = default;
        Cookie(std::string v) : value(std::move(v)) {}
    };

    struct CookieKey {
        std::string domain;
        std::string path;
        std::string name;

        bool operator==(const CookieKey& other) const noexcept {
            return domain == other.domain && path == other.path &&
                   name == other.name;
        }
    };

    struct CookieKeyHash {
        size_t operator()(const CookieKey& key) const noexcept {
            size_t seed = 0;
            boost::hash_combine(seed, key.domain);
            boost::hash_combine(seed, key.path);
            boost::hash_combine(seed, key.name);
            return seed;
        }
    };

    std::unordered_map<CookieKey, Cookie, CookieKeyHash> cookies_;

    static bool caseInsensitiveEqual(std::string_view a,
                                     std::string_view b) noexcept {
        return std::equal(
            a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            });
    }

    static std::string_view trim(std::string_view sv) noexcept {
        while (!sv.empty() && std::isspace(sv.front()))
            sv.remove_prefix(1);
        while (!sv.empty() && std::isspace(sv.back()))
            sv.remove_suffix(1);
        return sv;
    }

    static bool domainMatches(std::string_view request_host,
                              std::string_view cookie_domain) noexcept {
        if (cookie_domain.empty())
            return true;
        if (caseInsensitiveEqual(request_host, cookie_domain))
            return true;

        // Handle subdomain matching
        if (request_host.size() > cookie_domain.size() + 1) {
            auto suffix_pos = request_host.size() - cookie_domain.size();
            if (request_host[suffix_pos - 1] == '.' &&
                caseInsensitiveEqual(request_host.substr(suffix_pos),
                                     cookie_domain)) {
                // Prevent IP address domain matching
                bool is_ip = std::all_of(
                    cookie_domain.begin(), cookie_domain.end(),
                    [](char c) { return std::isdigit(c) || c == '.'; });
                return !is_ip;
            }
        }

        return false;
    }

    static bool pathMatches(std::string_view request_path,
                            std::string_view cookie_path) noexcept {
        if (request_path == cookie_path)
            return true;

        if (request_path.starts_with(cookie_path)) {
            return cookie_path.back() == '/' ||
                   (request_path.size() > cookie_path.size() &&
                    request_path[cookie_path.size()] == '/');
        }

        return false;
    }

    void parseSetCookieValue(std::string_view host,
                             std::string_view set_cookie) {
        if (set_cookie.empty())
            return;

        // Find first semicolon to separate name=value from attributes
        size_t semi_pos = set_cookie.find(';');
        std::string_view name_value = set_cookie.substr(0, semi_pos);

        size_t eq_pos = name_value.find('=');
        if (eq_pos == std::string_view::npos)
            return;

        auto name = trim(name_value.substr(0, eq_pos));
        auto value = trim(name_value.substr(eq_pos + 1));

        if (name.empty())
            return;

        Cookie cookie((std::string(value)));
        cookie.domain = std::string(host);

        // Parse attributes if present
        if (semi_pos != std::string_view::npos) {
            parseAttributes(set_cookie.substr(semi_pos + 1), cookie);
        }

        // Validate domain
        if (!domainMatches(host, cookie.domain)) {
            return;  // Invalid domain, reject cookie
        }

        // Store cookie
        CookieKey key{cookie.domain, cookie.path, std::string(name)};
        cookies_[std::move(key)] = std::move(cookie);
    }

    void parseAttributes(std::string_view attributes, Cookie& cookie) {
        size_t pos = 0;

        while (pos < attributes.size()) {
            // Skip whitespace and semicolons
            while (pos < attributes.size() &&
                   (std::isspace(attributes[pos]) || attributes[pos] == ';')) {
                ++pos;
            }
            if (pos >= attributes.size())
                break;

            // Find next semicolon
            size_t end = attributes.find(';', pos);
            if (end == std::string_view::npos)
                end = attributes.size();

            auto attr = trim(attributes.substr(pos, end - pos));
            parseAttribute(attr, cookie);

            pos = end + 1;
        }
    }

    void parseAttribute(std::string_view attr, Cookie& cookie) {
        if (attr.empty())
            return;

        size_t eq_pos = attr.find('=');
        auto name = trim(attr.substr(0, eq_pos));
        auto value = (eq_pos != std::string_view::npos)
                         ? trim(attr.substr(eq_pos + 1))
                         : std::string_view{};

        if (caseInsensitiveEqual(name, "Secure")) {
            cookie.secure = true;
        } else if (caseInsensitiveEqual(name, "HttpOnly")) {
            cookie.http_only = true;
        } else if (caseInsensitiveEqual(name, "Path")) {
            cookie.path = value.empty() ? "/" : std::string(value);
        } else if (caseInsensitiveEqual(name, "Domain")) {
            std::string domain_str(value);
            // Remove leading dot if present
            if (!domain_str.empty() && domain_str[0] == '.') {
                domain_str = domain_str.substr(1);
            }
            cookie.domain = std::move(domain_str);
        }
    }

public:
    /**
     * @brief Extracts and stores cookies from HTTP response Set-Cookie headers
     * @param request_host The hostname of the original request
     * @param response The HTTP response containing Set-Cookie headers
     */
    void extractCookies(std::string_view request_host,
                        const http::response<http::string_body>& response) {
        for (const auto& field : response) {
            if (beast::iequals(field.name_string(), "set-cookie")) {
                parseSetCookieValue(request_host, field.value());
            }
        }
    }

    /**
     * @brief Adds applicable cookies to an HTTP request
     * @param request_host The target hostname
     * @param request_path The target path (default "/")
     * @param is_secure True if using HTTPS connection
     * @param request The HTTP request to modify
     */
    void addCookiesToRequest(std::string_view request_host,
                             std::string_view request_path, bool is_secure,
                             http::request<http::string_body>& request) {
        std::string path =
            request_path.empty() ? "/" : std::string(request_path);
        std::unordered_map<std::string, std::string> applicable_cookies;

        for (const auto& [key, cookie] : cookies_) {
            // Skip secure cookies on non-secure connections
            if (cookie.secure && !is_secure)
                continue;

            // Check domain match
            if (!domainMatches(request_host, key.domain))
                continue;

            // Check path match
            if (!pathMatches(path, key.path))
                continue;

            applicable_cookies[key.name] = cookie.value;
        }

        if (!applicable_cookies.empty()) {
            request.set(http::field::cookie,
                        buildCookieString(applicable_cookies));
        }
    }

    /**
     * @brief Retrieves a specific cookie value
     * @param host The hostname context
     * @param name The cookie name
     * @param path The path context (default "/")
     * @return The cookie value if found, empty string otherwise
     */
    std::string getCookie(std::string_view host, std::string_view name,
                          std::string_view path = "/") const {
        std::string path_str = path.empty() ? "/" : std::string(path);

        // Try exact match first
        CookieKey key{std::string(host), path_str, std::string(name)};
        auto it = cookies_.find(key);
        if (it != cookies_.end()) {
            return it->second.value;
        }

        // Try domain and path matching
        for (const auto& [cookie_key, cookie] : cookies_) {
            if (cookie_key.name == name &&
                domainMatches(host, cookie_key.domain) &&
                pathMatches(path_str, cookie_key.path)) {
                return cookie.value;
            }
        }

        return {};
    }

    /**
     * @brief Sets a cookie directly in the manager
     * @param host The domain for the cookie
     * @param name The cookie name
     * @param value The cookie value
     * @param path The cookie path (default "/")
     * @param secure Whether the cookie requires HTTPS
     * @param http_only Whether the cookie is HTTP-only
     */
    void setCookie(std::string_view host, std::string_view name,
                   std::string_view value, std::string_view path = "/",
                   bool secure = false, bool http_only = false) {
        Cookie cookie((std::string(value)));
        cookie.domain = std::string(host);
        cookie.path = path.empty() ? "/" : std::string(path);
        cookie.secure = secure;
        cookie.http_only = http_only;

        CookieKey key{cookie.domain, cookie.path, std::string(name)};
        cookies_[std::move(key)] = std::move(cookie);
    }

    /**
     * @brief Removes a specific cookie
     * @param host The domain of the cookie
     * @param name The cookie name
     * @param path The cookie path (default "/")
     * @return True if cookie was found and removed
     */
    bool removeCookie(std::string_view host, std::string_view name,
                      std::string_view path = "/") {
        CookieKey key{std::string(host), path.empty() ? "/" : std::string(path),
                      std::string(name)};
        return cookies_.erase(key) > 0;
    }

    /**
     * @brief Clears all stored cookies
     */
    void clearAllCookies() noexcept { cookies_.clear(); }

    /**
     * @brief Gets the total number of stored cookies
     * @return The cookie count
     */
    size_t getCookieCount() const noexcept { return cookies_.size(); }

    /**
     * @brief Checks if any cookies are stored for a domain
     * @param host The domain to check
     * @return True if cookies exist for the domain
     */
    bool hasCookiesForDomain(std::string_view host) const {
        return std::any_of(cookies_.begin(), cookies_.end(),
                           [host](const auto& pair) {
                               return domainMatches(host, pair.first.domain);
                           });
    }
};

/**
 * @brief Utility class for HTTP header manipulation and validation
 */
class HeaderUtils {
public:
    /**
     * @brief Validates an HTTP header name according to RFC standards
     * @param name The header name to validate
     * @return True if the header name is valid
     */
    static bool isValidHeaderName(std::string_view name) noexcept {
        if (name.empty())
            return false;

        return std::all_of(name.begin(), name.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '_';
        });
    }

    /**
     * @brief Validates an HTTP header value
     * @param value The header value to validate
     * @return True if the header value is valid
     */
    static bool isValidHeaderValue(std::string_view value) noexcept {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c >= 32 && c != 127;  // Printable ASCII except DEL
        });
    }

    /**
     * @brief Safely sets an HTTP header with validation
     * @param request The HTTP request to modify
     * @param name The header name
     * @param value The header value
     * @return True if header was set successfully
     */
    template <typename Body>
    static bool setHeader(http::request<Body>& request, std::string_view name,
                          std::string_view value) {
        if (!isValidHeaderName(name) || !isValidHeaderValue(value)) {
            return false;
        }

        request.set(std::string(name), std::string(value));
        return true;
    }

    /**
     * @brief Extracts content type and charset from Content-Type header
     * @param content_type_header The Content-Type header value
     * @return Pair of content type and charset (empty if not specified)
     */
    static std::pair<std::string, std::string> parseContentType(
        std::string_view content_type_header) {
        size_t semicolon_pos = content_type_header.find(';');
        std::string content_type = std::string(
            atom::utils::trim(content_type_header.substr(0, semicolon_pos)));

        std::string charset;
        if (semicolon_pos != std::string_view::npos) {
            std::string_view params =
                content_type_header.substr(semicolon_pos + 1);
            size_t charset_pos = params.find("charset=");
            if (charset_pos != std::string_view::npos) {
                std::string_view charset_part = params.substr(charset_pos + 8);
                size_t end_pos = charset_part.find_first_of(" ;");
                charset = std::string(
                    atom::utils::trim(charset_part.substr(0, end_pos)));

                // Remove quotes if present
                if (charset.size() >= 2 && charset.front() == '"' &&
                    charset.back() == '"') {
                    charset = charset.substr(1, charset.size() - 2);
                }
            }
        }

        return {std::move(content_type), std::move(charset)};
    }
};

}  // namespace http_utils

#endif
