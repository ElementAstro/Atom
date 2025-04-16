#ifndef ATOM_EXTRA_BEAST_HTTP_UTILS_HPP
#define ATOM_EXTRA_BEAST_HTTP_UTILS_HPP

#include <zlib.h>
#include <algorithm>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/http.hpp>       // Include the main Beast HTTP header
#include <boost/functional/hash.hpp>  // Required for std::pair hashing
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

// Introduce namespaces for convenience within this scope
namespace beast = boost::beast;
namespace http = beast::http;

/**
 * @brief Creates a Basic Authentication Authorization header value.
 * @param username The username.
 * @param password The password.
 * @return The encoded Authorization header value (e.g., "Basic dXNlcjpwYXNz").
 */
inline std::string basicAuth(std::string_view username,
                             std::string_view password) {
    std::string auth_string =
        std::string(username) + ":" + std::string(password);
    std::string encoded;

    encoded.resize(beast::detail::base64::encoded_size(auth_string.size()));
    beast::detail::base64::encode(encoded.data(), auth_string.data(),
                                  auth_string.size());

    return "Basic " + encoded;
}

/**
 * @brief Compresses data using GZIP or DEFLATE algorithm.
 * @param data The data to compress.
 * @param is_gzip If true, uses GZIP format; otherwise, uses DEFLATE. Defaults
 * to true.
 * @return The compressed data as a string.
 * @throws std::runtime_error If compression initialization or execution fails.
 */
inline std::string compress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int windowBits =
        is_gzip ? 15 + 16 : 15;  // 15 for DEFLATE, +16 for GZIP header
    int result =
        deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits,
                     8,  // Default memory level
                     Z_DEFAULT_STRATEGY);

    if (result != Z_OK) {
        throw std::runtime_error(
            "Failed to initialize compression: zlib error " +
            std::to_string(result));
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in =
        const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));

    std::string compressed;
    compressed.resize(deflateBound(
        &stream, stream.avail_in));  // Estimate max compressed size

    stream.avail_out = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data());

    result = deflate(&stream, Z_FINISH);  // Perform compression in one go

    deflateEnd(&stream);  // Clean up

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to compress data: zlib error " +
                                 std::to_string(result));
    }

    compressed.resize(stream.total_out);  // Resize to actual compressed size

    return compressed;
}

/**
 * @brief Decompresses data compressed with GZIP or DEFLATE.
 * @param data The compressed data.
 * @param is_gzip If true, assumes GZIP format; otherwise, assumes DEFLATE.
 * Defaults to true.
 * @return The decompressed data as a string.
 * @throws std::runtime_error If decompression initialization or execution
 * fails.
 */
inline std::string decompress(std::string_view data, bool is_gzip = true) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    int windowBits =
        is_gzip ? 15 + 32 : 15;  // 15 for DEFLATE, +32 to auto-detect GZIP/zlib
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
    std::vector<char> buffer(data.size() * 2);  // Initial buffer size guess
    std::size_t total_out = 0;

    do {
        if (total_out >= buffer.size()) {
            buffer.resize(buffer.size() * 2);  // Double buffer size if needed
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

    inflateEnd(&stream);  // Clean up

    decompressed.assign(buffer.data(),
                        total_out);  // Assign the valid decompressed part

    return decompressed;
}

/**
 * @brief URL-encodes a string according to RFC 3986.
 *        Unreserved characters (A-Z, a-z, 0-9, '-', '_', '.', '~') are not
 * encoded. Spaces are encoded as '%20' (not '+').
 * @param input The string to encode.
 * @return The URL-encoded string.
 */
inline std::string urlEncode(std::string_view input) {
    static const char hex_chars[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size());  // Reserve at least the original size

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
 *        Decodes percent-encoded sequences (e.g., %20 becomes space).
 *        Handles '+' as a space for compatibility with form data, though
 * urlEncode uses %20.
 * @param input The URL-encoded string.
 * @return The decoded string. Invalid percent-encoded sequences are passed
 * through unchanged.
 */
inline std::string urlDecode(std::string_view input) {
    std::string result;
    result.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (c == '+') {
            result += ' ';
        } else if (c == '%' && i + 2 < input.size()) {
            char h1 = input[i + 1];
            char h2 = input[i + 2];
            int v1 = 0, v2 = 0;

            if (h1 >= '0' && h1 <= '9')
                v1 = h1 - '0';
            else if (h1 >= 'A' && h1 <= 'F')
                v1 = h1 - 'A' + 10;
            else if (h1 >= 'a' && h1 <= 'f')
                v1 = h1 - 'a' + 10;
            else {  // Invalid hex char 1
                result += c;
                continue;
            }

            if (h2 >= '0' && h2 <= '9')
                v2 = h2 - '0';
            else if (h2 >= 'A' && h2 <= 'F')
                v2 = h2 - 'A' + 10;
            else if (h2 >= 'a' && h2 <= 'f')
                v2 = h2 - 'a' + 10;
            else {  // Invalid hex char 2
                result += c;
                continue;
            }

            result += static_cast<char>((v1 << 4) | v2);
            i += 2;  // Skip the two hex chars
        } else {
            result += c;
        }
    }

    return result;
}

/**
 * @brief Builds a URL query string from a map of parameters.
 *        Keys and values are URL-encoded.
 * @param params A map where keys are parameter names and values are parameter
 * values.
 * @return The formatted query string (e.g., "key1=value1&key2=value2"). Does
 * not include the leading '?'.
 */
inline std::string buildQueryString(
    const std::unordered_map<std::string, std::string>& params) {
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
 * @brief Parses a Cookie header string into a map of cookie names and values.
 * @param cookie_header The value of the Cookie HTTP header (e.g.,
 * "name1=value1; name2=value2").
 * @return An unordered map containing the parsed cookie names and values.
 * Leading/trailing whitespace is trimmed.
 */
inline std::unordered_map<std::string, std::string> parseCookies(
    std::string_view cookie_header) {
    std::unordered_map<std::string, std::string> cookies;
    std::size_t pos = 0;
    std::size_t end;

    while (pos < cookie_header.size()) {
        // Find the next semicolon
        end = cookie_header.find(';', pos);
        if (end == std::string_view::npos) {
            end = cookie_header.size();
        }

        // Extract the cookie pair string (e.g., " name1 = value1 ")
        std::string_view cookie_pair = cookie_header.substr(pos, end - pos);

        // Find the equals sign
        std::size_t eq_pos = cookie_pair.find('=');
        if (eq_pos != std::string_view::npos) {
            // Extract name and value
            std::string_view name = cookie_pair.substr(0, eq_pos);
            std::string_view value = cookie_pair.substr(eq_pos + 1);

            // Trim leading/trailing whitespace from name and value
            auto trim_ws = [](std::string_view& sv) {
                while (!sv.empty() &&
                       std::isspace(static_cast<unsigned char>(sv.front()))) {
                    sv.remove_prefix(1);
                }
                while (!sv.empty() &&
                       std::isspace(static_cast<unsigned char>(sv.back()))) {
                    sv.remove_suffix(1);
                }
            };

            trim_ws(name);
            trim_ws(value);

            if (!name.empty()) {  // Only add if name is not empty after
                                  // trimming
                cookies[std::string(name)] = std::string(value);
            }
        }

        // Move to the next cookie part
        pos = end + 1;
    }

    return cookies;
}

/**
 * @brief Builds a Cookie header string from a map of cookie names and values.
 * @param cookies A map containing cookie names and values.
 * @return The formatted Cookie header string (e.g., "name1=value1;
 * name2=value2").
 */
inline std::string buildCookieString(
    const std::unordered_map<std::string, std::string>& cookies) {
    std::string cookie_str;
    bool first = true;

    for (const auto& [name, value] : cookies) {
        if (!first) {
            cookie_str += "; ";
        }
        first = false;

        // Note: Cookie values should ideally not contain ';' or '=', but this
        // function doesn't encode them.
        cookie_str += name + "=" + value;
    }

    return cookie_str;
}

/**
 * @brief Manages HTTP cookies, parsing Set-Cookie headers and adding Cookie
 * headers. Provides basic cookie storage and retrieval based on host matching.
 *        Note: This is a simplified implementation and does not handle all
 * aspects of cookie management (e.g., complex Path/Domain matching, Expires
 * parsing, Max-Age, SameSite).
 */
class CookieManager {
private:
    /**
     * @brief Represents the value and attributes of a stored cookie.
     */
    struct CookieValue {
        std::string value;
        std::string path = "/";  // Default path
        // std::chrono::system_clock::time_point expires; // TODO: Implement
        // expiration handling
        bool secure = false;
        bool http_only = false;  // Note: This manager doesn't prevent
                                 // client-side script access
        std::string domain;      // Domain attribute
    };

    // Key for the cookie map: pair of (domain, path, name)
    using CookieKey = std::tuple<std::string, std::string, std::string>;

    // Hash function for the tuple key
    struct CookieKeyHash {
        std::size_t operator()(const CookieKey& k) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, std::get<0>(k));
            boost::hash_combine(seed, std::get<1>(k));
            boost::hash_combine(seed, std::get<2>(k));
            return seed;
        }
    };

    // Storage for cookies
    std::unordered_map<CookieKey, CookieValue, CookieKeyHash> cookies_;

    /**
     * @brief Trims leading and trailing whitespace from a string in-place.
     * @param s The string to trim.
     */
    static void trim(std::string& s) {
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(),
                s.end());
    }

    /**
     * @brief Performs case-insensitive string comparison.
     * @param a First string.
     * @param b Second string.
     * @return True if strings are equal ignoring case, false otherwise.
     */
    static bool iequals(const std::string& a, const std::string& b) {
        return std::equal(
            a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            });
    }

    /**
     * @brief Parses a single attribute from a Set-Cookie string.
     * @param attr The attribute string (e.g., "Path=/", "Secure", "HttpOnly").
     * @param cookie_value The CookieValue object to update.
     */
    void parseAttribute(const std::string& attr, CookieValue& cookie_value) {
        std::size_t eq_pos = attr.find('=');
        std::string name;
        std::string value;

        if (eq_pos == std::string::npos) {
            // Flag attribute (no value)
            name = attr;
            trim(name);
        } else {
            // Key-value attribute
            name = attr.substr(0, eq_pos);
            value = attr.substr(eq_pos + 1);
            trim(name);
            trim(value);
        }

        if (iequals(name, "Secure")) {
            cookie_value.secure = true;
        } else if (iequals(name, "HttpOnly")) {
            cookie_value.http_only = true;
        } else if (iequals(name, "Path")) {
            cookie_value.path = value.empty() ? "/" : value;
        } else if (iequals(name, "Domain")) {
            // Store domain, removing leading '.' if present for normalization
            cookie_value.domain =
                (!value.empty() && value[0] == '.') ? value.substr(1) : value;
        } else if (iequals(name, "Expires")) {
            // TODO: Implement proper date parsing for Expires
            // Example: cookie_value.expires = parseHttpDate(value);
        } else if (iequals(name, "Max-Age")) {
            // TODO: Implement Max-Age calculation to set expires
            // Example: try { int max_age_seconds = std::stoi(value); ... }
            // catch(...) {}
        }
        // Other attributes like SameSite are ignored in this simplified version
    }

    /**
     * @brief Parses a Set-Cookie header string and stores the cookie.
     * @param request_host The host from the original request, used as default
     * domain.
     * @param set_cookie_str The value of the Set-Cookie header.
     */
    void parseSetCookieString(const std::string& request_host,
                              const std::string& set_cookie_str) {
        std::size_t first_semi = set_cookie_str.find(';');
        std::string name_value_pair = set_cookie_str.substr(0, first_semi);
        std::size_t eq_pos = name_value_pair.find('=');

        if (eq_pos == std::string::npos) {
            return;  // Invalid format: name=value is required
        }

        std::string name = name_value_pair.substr(0, eq_pos);
        std::string value = name_value_pair.substr(eq_pos + 1);
        trim(name);
        trim(value);  // Trim value as well

        if (name.empty()) {
            return;  // Cookie name cannot be empty
        }

        CookieValue cookie_value;
        cookie_value.value = value;
        cookie_value.domain =
            request_host;  // Default domain is the request host

        // Parse attributes if they exist
        if (first_semi != std::string::npos) {
            std::string attrs_part = set_cookie_str.substr(first_semi + 1);
            std::size_t pos = 0;
            while (pos < attrs_part.size()) {
                // Skip leading whitespace/semicolons
                while (
                    pos < attrs_part.size() &&
                    (std::isspace(attrs_part[pos]) || attrs_part[pos] == ';')) {
                    ++pos;
                }
                if (pos >= attrs_part.size())
                    break;

                std::size_t next_semi = attrs_part.find(';', pos);
                std::string current_attr = attrs_part.substr(
                    pos, next_semi == std::string::npos ? std::string::npos
                                                        : next_semi - pos);
                // Pass only the attribute string and the cookie value object
                parseAttribute(current_attr, cookie_value);

                if (next_semi == std::string::npos) {
                    break;  // Last attribute
                }
                pos = next_semi + 1;
            }
        }

        // TODO: Add check for expiration before storing

        // Normalize domain for storage (remove leading dot if user added it)
        if (!cookie_value.domain.empty() && cookie_value.domain[0] == '.') {
            cookie_value.domain = cookie_value.domain.substr(1);
        }
        // Basic validation: Host must domain-match the Domain attribute if
        // specified
        if (!domainMatches(request_host, cookie_value.domain)) {
            // If Domain is set, it must match the request host.
            // If Domain is not set, it defaults to request_host, which always
            // matches. This check prevents setting cookies for unrelated
            // domains.
            if (!cookie_value.domain.empty() &&
                cookie_value.domain != request_host) {
                // Only proceed if the specified domain is valid for the request
                // host
                return;
            }
            // If domain attribute was empty, it defaulted to request_host,
            // which is fine.
        }

        CookieKey key = {cookie_value.domain, cookie_value.path, name};
        cookies_[key] = std::move(cookie_value);
    }

    /**
     * @brief Checks if a request host matches a cookie's domain attribute
     * according to RFC 6265 rules.
     * @param request_host The host the request is being sent to.
     * @param cookie_domain The Domain attribute of the cookie (normalized, no
     * leading dot).
     * @return True if the request host matches the cookie domain, false
     * otherwise.
     */
    static bool domainMatches(const std::string& request_host,
                              const std::string& cookie_domain) {
        if (cookie_domain.empty()) {
            // Should not happen if default is set correctly, but handle
            // defensively. If no domain attribute, it defaults to the request
            // host.
            return true;  // Or compare request_host == default_domain used in
                          // parseSetCookieString
        }

        // Exact match
        if (iequals(request_host, cookie_domain)) {
            return true;
        }

        // Subdomain match: request_host must end with ".cookie_domain"
        // e.g., request_host="sub.example.com", cookie_domain="example.com"
        std::string domain_suffix = "." + cookie_domain;
        if (request_host.size() > domain_suffix.size() &&
            iequals(
                request_host.substr(request_host.size() - domain_suffix.size()),
                domain_suffix)) {
            // Ensure it's not an IP address match attempt
            // Basic check: if cookie_domain contains non-digit/non-dot, it's
            // likely not an IP
            bool domain_is_ip = true;
            for (char c : cookie_domain) {
                if (!std::isdigit(c) && c != '.') {
                    domain_is_ip = false;
                    break;
                }
            }
            // Allow subdomain matching only if it's not an IP address
            return !domain_is_ip;
        }

        return false;
    }

    /**
     * @brief Checks if a request path matches a cookie's path attribute.
     * @param request_path The path part of the request URL.
     * @param cookie_path The Path attribute of the cookie.
     * @return True if the request path matches the cookie path, false
     * otherwise.
     */
    static bool pathMatches(const std::string& request_path,
                            const std::string& cookie_path) {
        if (request_path == cookie_path) {
            return true;  // Exact match
        }
        // Path prefix match: request_path must start with cookie_path,
        // and the next character in request_path must be '/' or end of string.
        if (request_path.rfind(cookie_path, 0) ==
            0) {  // Starts with cookie_path
            if (cookie_path.back() == '/') {
                return true;  // e.g., cookie="/foo/", request="/foo/bar"
            }
            if (request_path.size() > cookie_path.size() &&
                request_path[cookie_path.size()] == '/') {
                return true;  // e.g., cookie="/foo", request="/foo/bar"
            }
        }
        return false;
    }

public:
    /**
     * @brief Extracts cookies from the Set-Cookie headers of an HTTP response.
     * @param request_host The original host the request was sent to (used for
     * default domain).
     * @param response The HTTP response object.
     */
    void extractCookies(std::string_view request_host,
                        const http::response<http::string_body>& response) {
        // Iterate over all headers with the name "Set-Cookie"
        for (auto const& field : response) {
            // Use beast::iequals for case-insensitive comparison
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
     * @param is_secure True if the connection is secure (HTTPS), false
     * otherwise.
     * @param request The HTTP request object to modify.
     */
    void addCookiesToRequest(std::string_view request_host_sv,
                             std::string_view request_path_sv, bool is_secure,
                             http::request<http::string_body>& request) {
        std::string request_host = std::string(request_host_sv);
        std::string request_path = std::string(request_path_sv);
        if (request_path.empty())
            request_path = "/";  // Ensure path is at least "/"

        std::unordered_map<std::string, std::string> applicable_cookies;

        // TODO: Add check for cookie expiration

        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [domain, path, name] = cookie_key;

            // Check Secure flag
            if (cookie_value.secure && !is_secure) {
                continue;
            }

            // Check Domain match
            if (!domainMatches(request_host, domain)) {
                continue;
            }

            // Check Path match
            if (!pathMatches(request_path, path)) {
                continue;
            }

            // Cookie is applicable, add it (or replace if a more specific one
            // was already added) Simple approach: last one wins if names
            // collide (real browsers have more complex rules)
            applicable_cookies[name] = cookie_value.value;
        }

        // If any applicable cookies were found, build the Cookie header
        if (!applicable_cookies.empty()) {
            // Use http::field enum
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
     *        Note: This is a simplified retrieval and might not reflect the
     * exact cookie that would be sent in a request due to path/domain matching
     * rules. Prefer using addCookiesToRequest for accurate cookie selection.
     * @param host The host context.
     * @param name The name of the cookie.
     * @return The cookie value if found matching the host and name (considering
     * domain matching), otherwise an empty string. Returns the first match
     * found.
     */
    std::string getCookie(std::string_view host_sv, std::string_view name_sv) {
        std::string host = std::string(host_sv);
        std::string name = std::string(name_sv);
        // Iterate through cookies to find a match based on domain and name
        // This simplified version doesn't consider path or expiration.
        for (const auto& [cookie_key, cookie_value] : cookies_) {
            const auto& [domain, path, cookie_name] = cookie_key;
            if (cookie_name == name && domainMatches(host, domain)) {
                // TODO: Check expiration here as well
                return cookie_value.value;  // Return first match
            }
        }
        return "";  // Not found
    }
};

}  // namespace http_utils

#endif  // ATOM_EXTRA_BEAST_HTTP_UTILS_HPP
