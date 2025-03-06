#ifndef ATOM_EXTRA_CURL_COOKIE_HPP
#define ATOM_EXTRA_CURL_COOKIE_HPP

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Cookie {
public:
    Cookie(std::string name, std::string value, std::string domain = "",
           std::string path = "/", bool secure = false, bool http_only = false,
           std::optional<std::chrono::system_clock::time_point> expires =
               std::nullopt);

    std::string to_string() const;

    const std::string& name() const noexcept { return name_; }
    const std::string& value() const noexcept { return value_; }
    const std::string& domain() const noexcept { return domain_; }
    const std::string& path() const noexcept { return path_; }
    bool secure() const noexcept { return secure_; }
    bool http_only() const noexcept { return http_only_; }
    const std::optional<std::chrono::system_clock::time_point>& expires()
        const noexcept {
        return expires_;
    }

    bool is_expired() const;

private:
    std::string name_;
    std::string value_;
    std::string domain_;
    std::string path_;
    bool secure_;
    bool http_only_;
    std::optional<std::chrono::system_clock::time_point> expires_;
};

class CookieJar {
public:
    CookieJar() = default;

    void set_cookie(const Cookie& cookie);
    std::optional<Cookie> get_cookie(const std::string& name) const;
    std::vector<Cookie> get_cookies() const;
    void clear();

    // File operations
    bool load_from_file(const std::string& filename);
    bool save_to_file(const std::string& filename) const;

    // Parse cookies from HTTP headers
    void parse_cookies_from_headers(
        const std::map<std::string, std::string>& headers,
        const std::string& domain);

private:
    std::unordered_map<std::string, Cookie> cookies_;
    mutable std::mutex mutex_;

    // Parse a single Set-Cookie header
    void parse_cookie_header(const std::string& header,
                             const std::string& default_domain);
};

#endif  // ATOM_EXTRA_CURL_COOKIE_HPP