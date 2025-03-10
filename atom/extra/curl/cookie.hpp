#ifndef ATOM_EXTRA_CURL_COOKIE_HPP
#define ATOM_EXTRA_CURL_COOKIE_HPP

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom::extra::curl {
/**
 * @brief Represents an HTTP cookie.
 *
 * This class encapsulates the data associated with an HTTP cookie,
 * including its name, value, domain, path, security settings, and
 * expiration time.
 */
class Cookie {
public:
    /**
     * @brief Constructor for the Cookie class.
     *
     * @param name The name of the cookie.
     * @param value The value of the cookie.
     * @param domain The domain the cookie is valid for (default: "").
     * @param path The path the cookie is valid for (default: "/").
     * @param secure True if the cookie should only be transmitted over secure
     * connections (HTTPS) (default: false).
     * @param http_only True if the cookie should only be accessible through
     * HTTP(S) and not through client-side scripts (default: false).
     * @param expires An optional expiration time for the cookie. If not
     * provided, the cookie will be a session cookie (default: std::nullopt).
     */
    Cookie(std::string name, std::string value, std::string domain = "",
           std::string path = "/", bool secure = false, bool http_only = false,
           std::optional<std::chrono::system_clock::time_point> expires =
               std::nullopt);

    /**
     * @brief Converts the cookie to a string representation suitable for use in
     * an HTTP header.
     *
     * @return A string representation of the cookie.
     */
    std::string to_string() const;

    /**
     * @brief Gets the name of the cookie.
     *
     * @return The name of the cookie.
     */
    const std::string& name() const noexcept { return name_; }
    /**
     * @brief Gets the value of the cookie.
     *
     * @return The value of the cookie.
     */
    const std::string& value() const noexcept { return value_; }
    /**
     * @brief Gets the domain the cookie is valid for.
     *
     * @return The domain the cookie is valid for.
     */
    const std::string& domain() const noexcept { return domain_; }
    /**
     * @brief Gets the path the cookie is valid for.
     *
     * @return The path the cookie is valid for.
     */
    const std::string& path() const noexcept { return path_; }
    /**
     * @brief Checks if the cookie should only be transmitted over secure
     * connections (HTTPS).
     *
     * @return True if the cookie should only be transmitted over secure
     * connections, false otherwise.
     */
    bool secure() const noexcept { return secure_; }
    /**
     * @brief Checks if the cookie should only be accessible through HTTP(S) and
     * not through client-side scripts.
     *
     * @return True if the cookie should only be accessible through HTTP(S),
     * false otherwise.
     */
    bool http_only() const noexcept { return http_only_; }
    /**
     * @brief Gets the expiration time of the cookie.
     *
     * @return An optional expiration time for the cookie. If not provided, the
     * cookie will be a session cookie.
     */
    const std::optional<std::chrono::system_clock::time_point>& expires()
        const noexcept {
        return expires_;
    }

    /**
     * @brief Checks if the cookie has expired.
     *
     * @return True if the cookie has expired, false otherwise.
     */
    bool is_expired() const;

private:
    /** @brief The name of the cookie. */
    std::string name_;
    /** @brief The value of the cookie. */
    std::string value_;
    /** @brief The domain the cookie is valid for. */
    std::string domain_;
    /** @brief The path the cookie is valid for. */
    std::string path_;
    /** @brief True if the cookie should only be transmitted over secure
     * connections (HTTPS). */
    bool secure_;
    /** @brief True if the cookie should only be accessible through HTTP(S) and
     * not through client-side scripts. */
    bool http_only_;
    /** @brief An optional expiration time for the cookie. */
    std::optional<std::chrono::system_clock::time_point> expires_;
};

/**
 * @brief Manages a collection of HTTP cookies.
 *
 * This class provides methods for storing, retrieving, and managing HTTP
 * cookies. It also supports loading and saving cookies to a file in the
 * Netscape cookie file format.
 */
class CookieJar {
public:
    /**
     * @brief Default constructor for the CookieJar class.
     */
    CookieJar() = default;

    /**
     * @brief Sets a cookie in the cookie jar.
     *
     * If a cookie with the same name already exists, it will be overwritten.
     *
     * @param cookie The cookie to set.
     */
    void set_cookie(const Cookie& cookie);
    /**
     * @brief Gets a cookie from the cookie jar by name.
     *
     * @param name The name of the cookie to get.
     * @return An optional Cookie object if a cookie with the given name exists
     * and has not expired, std::nullopt otherwise.
     */
    std::optional<Cookie> get_cookie(const std::string& name) const;
    /**
     * @brief Gets all cookies from the cookie jar that have not expired.
     *
     * @return A vector of Cookie objects.
     */
    std::vector<Cookie> get_cookies() const;
    /**
     * @brief Clears all cookies from the cookie jar.
     */
    void clear();

    /**
     * @brief Loads cookies from a file in the Netscape cookie file format.
     *
     * @param filename The name of the file to load cookies from.
     * @return True if the cookies were successfully loaded, false otherwise.
     */
    bool load_from_file(const std::string& filename);
    /**
     * @brief Saves cookies to a file in the Netscape cookie file format.
     *
     * @param filename The name of the file to save cookies to.
     * @return True if the cookies were successfully saved, false otherwise.
     */
    bool save_to_file(const std::string& filename) const;

    /**
     * @brief Parses cookies from HTTP headers and adds them to the cookie jar.
     *
     * @param headers A map of HTTP header names to header values.
     * @param domain The domain to use for cookies that do not specify a domain.
     */
    void parse_cookies_from_headers(
        const std::map<std::string, std::string>& headers,
        const std::string& domain);

private:
    /** @brief The collection of cookies stored in the cookie jar. */
    std::unordered_map<std::string, Cookie> cookies_;
    /** @brief Mutex to protect the cookie jar from concurrent access. */
    mutable std::mutex mutex_;

    /**
     * @brief Parses a single Set-Cookie header and adds the cookie to the
     * cookie jar.
     *
     * @param header The Set-Cookie header to parse.
     * @param default_domain The domain to use if the header doesn't specify
     * one.
     */
    void parse_cookie_header(const std::string& header,
                             const std::string& default_domain);
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_COOKIE_HPP