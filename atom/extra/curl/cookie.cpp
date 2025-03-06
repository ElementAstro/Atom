#include "cookie.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

// Cookie implementation
Cookie::Cookie(std::string name, std::string value, std::string domain,
               std::string path, bool secure, bool http_only,
               std::optional<std::chrono::system_clock::time_point> expires)
    : name_(std::move(name)),
      value_(std::move(value)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      secure_(secure),
      http_only_(http_only),
      expires_(expires) {}

std::string Cookie::to_string() const {
    std::string result = name_ + "=" + value_;

    if (!domain_.empty()) {
        result += "; Domain=" + domain_;
    }

    if (!path_.empty()) {
        result += "; Path=" + path_;
    }

    if (secure_) {
        result += "; Secure";
    }

    if (http_only_) {
        result += "; HttpOnly";
    }

    if (expires_) {
        std::time_t time = std::chrono::system_clock::to_time_t(*expires_);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S GMT",
                      std::gmtime(&time));
        result += "; Expires=" + std::string(time_str);
    }

    return result;
}

bool Cookie::is_expired() const {
    if (!expires_) {
        return false;
    }

    return std::chrono::system_clock::now() > *expires_;
}

// CookieJar implementation
void CookieJar::set_cookie(const Cookie& cookie) {
    std::lock_guard lock(mutex_);

    // Delete expired cookies
    if (cookie.is_expired()) {
        cookies_.erase(cookie.name());
        return;
    }

    cookies_[cookie.name()] = cookie;
}

std::optional<Cookie> CookieJar::get_cookie(const std::string& name) const {
    std::lock_guard lock(mutex_);

    auto it = cookies_.find(name);
    if (it != cookies_.end() && !it->second.is_expired()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<Cookie> CookieJar::get_cookies() const {
    std::lock_guard lock(mutex_);

    std::vector<Cookie> result;
    for (const auto& [_, cookie] : cookies_) {
        if (!cookie.is_expired()) {
            result.push_back(cookie);
        }
    }

    return result;
}

void CookieJar::clear() {
    std::lock_guard lock(mutex_);
    cookies_.clear();
}

bool CookieJar::load_from_file(const std::string& filename) {
    std::lock_guard lock(mutex_);

    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    cookies_.clear();

    std::string line;
    while (std::getline(file, line)) {
        // Parse Netscape cookie file format
        if (!line.empty() && line[0] != '#') {
            std::vector<std::string> fields;
            std::istringstream iss(line);
            std::string field;

            while (std::getline(iss, field, '\t')) {
                fields.push_back(field);
            }

            if (fields.size() >= 7) {
                std::string domain = fields[0];
                bool http_only = fields[1] == "TRUE";
                std::string path = fields[2];
                bool secure = fields[3] == "TRUE";

                long long expires_time = std::stoll(fields[4]);
                std::optional<std::chrono::system_clock::time_point> expires;
                if (expires_time > 0) {
                    expires =
                        std::chrono::system_clock::from_time_t(expires_time);
                }

                std::string name = fields[5];
                std::string value = fields[6];

                Cookie cookie(name, value, domain, path, secure, http_only,
                              expires);
                cookies_[name] = cookie;
            }
        }
    }

    return true;
}

bool CookieJar::save_to_file(const std::string& filename) const {
    std::lock_guard lock(mutex_);

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "# Netscape HTTP Cookie File\n";
    file << "# https://curl.se/docs/http-cookies.html\n";

    for (const auto& [_, cookie] : cookies_) {
        if (!cookie.is_expired()) {
            file << cookie.domain() << "\t";
            file << (cookie.http_only() ? "TRUE" : "FALSE") << "\t";
            file << cookie.path() << "\t";
            file << (cookie.secure() ? "TRUE" : "FALSE") << "\t";

            if (cookie.expires()) {
                file << std::chrono::system_clock::to_time_t(*cookie.expires());
            } else {
                file << "0";
            }

            file << "\t" << cookie.name() << "\t" << cookie.value() << "\n";
        }
    }

    return true;
}

void CookieJar::parse_cookies_from_headers(
    const std::map<std::string, std::string>& headers,
    const std::string& domain) {
    // Find all Set-Cookie headers
    for (const auto& [name, value] : headers) {
        if (name == "Set-Cookie") {
            parse_cookie_header(value, domain);
        }
    }
}

void CookieJar::parse_cookie_header(const std::string& header,
                                    const std::string& default_domain) {
    std::regex cookie_regex(R"(([^=]+)=([^;]*)(?:;|$))");
    std::smatch match;

    std::string cookie_str = header;

    std::string name;
    std::string value;
    std::string domain = default_domain;
    std::string path = "/";
    bool secure = false;
    bool http_only = false;
    std::optional<std::chrono::system_clock::time_point> expires;

    // Extract name and value
    if (std::regex_search(cookie_str, match, cookie_regex)) {
        name = match[1];
        value = match[2];
        cookie_str = match.suffix();

        // Remove leading spaces
        name = std::regex_replace(name, std::regex("^\\s+"), "");
        value = std::regex_replace(value, std::regex("^\\s+"), "");
    } else {
        return;  // Invalid cookie
    }

    // Extract attributes
    std::regex attr_regex(R"(;\s*([^=]+)(?:=([^;]*))?(?:;|$))");
    while (std::regex_search(cookie_str, match, attr_regex)) {
        std::string attr_name = match[1];
        std::string attr_value = match[2].matched ? match[2].str() : "";

        // Remove leading and trailing spaces
        attr_name =
            std::regex_replace(attr_name, std::regex("^\\s+|\\s+$"), "");
        attr_value =
            std::regex_replace(attr_value, std::regex("^\\s+|\\s+$"), "");

        // Convert to lowercase for case-insensitive comparison
        std::string attr_name_lower = attr_name;
        std::transform(attr_name_lower.begin(), attr_name_lower.end(),
                       attr_name_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (attr_name_lower == "domain") {
            domain = attr_value;
        } else if (attr_name_lower == "path") {
            path = attr_value;
        } else if (attr_name_lower == "secure") {
            secure = true;
        } else if (attr_name_lower == "httponly") {
            http_only = true;
        } else if (attr_name_lower == "expires") {
            // Parse HTTP date format
            std::tm tm = {};
            std::istringstream ss(attr_value);
            ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            if (!ss.fail()) {
                std::time_t time = std::mktime(&tm);
                expires = std::chrono::system_clock::from_time_t(time);
            }
        } else if (attr_name_lower == "max-age") {
            try {
                int seconds = std::stoi(attr_value);
                auto now = std::chrono::system_clock::now();
                expires = now + std::chrono::seconds(seconds);
            } catch (const std::exception&) {
                // Ignore invalid max-age
            }
        }

        cookie_str = match.suffix();
    }

    Cookie cookie(name, value, domain, path, secure, http_only, expires);
    set_cookie(cookie);
}