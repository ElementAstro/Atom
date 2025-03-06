#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace curl {

// 前向声明
class Session;
class Request;
class Response;
class Error;
class MultipartForm;
class Cookie;
class CookieJar;
class MultiSession;
class WebSocket;
class Cache;
class RateLimiter;
class ConnectionPool;
class SessionPool;
class Interceptor;

// 版本信息
struct Version {
    static std::string libcurl() {
        curl_version_info_data* version_info =
            curl_version_info(CURLVERSION_NOW);
        return version_info->version;
    }

    static std::string wrapper() { return "2.0.0"; }

    static std::string ssl() {
        curl_version_info_data* version_info =
            curl_version_info(CURLVERSION_NOW);
        return version_info->ssl_version ? version_info->ssl_version
                                         : "unknown";
    }

    static bool has_feature(long feature) {
        curl_version_info_data* version_info =
            curl_version_info(CURLVERSION_NOW);
        return (version_info->features & feature) != 0;
    }

    static bool supports_http2() { return has_feature(CURL_VERSION_HTTP2); }

    static bool supports_http3() { return has_feature(CURL_VERSION_HTTP3); }
};

// 错误类
class Error : public std::runtime_error {
public:
    explicit Error(CURLcode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    explicit Error(CURLMcode code, std::string message)
        : std::runtime_error(std::move(message)),
          code_(static_cast<CURLcode>(code)),
          multi_code_(code) {}

    CURLcode code() const noexcept { return code_; }
    std::optional<CURLMcode> multi_code() const noexcept { return multi_code_; }

private:
    CURLcode code_;
    std::optional<CURLMcode> multi_code_;
};

// Cookie 类
class Cookie {
public:
    Cookie(std::string name, std::string value, std::string domain = "",
           std::string path = "/", bool secure = false, bool http_only = false,
           std::optional<std::chrono::system_clock::time_point> expires =
               std::nullopt)
        : name_(std::move(name)),
          value_(std::move(value)),
          domain_(std::move(domain)),
          path_(std::move(path)),
          secure_(secure),
          http_only_(http_only),
          expires_(expires) {}

    std::string to_string() const {
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
            std::strftime(time_str, sizeof(time_str),
                          "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&time));
            result += "; Expires=" + std::string(time_str);
        }

        return result;
    }

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

    bool is_expired() const {
        if (!expires_) {
            return false;
        }

        return std::chrono::system_clock::now() > *expires_;
    }

private:
    std::string name_;
    std::string value_;
    std::string domain_;
    std::string path_;
    bool secure_;
    bool http_only_;
    std::optional<std::chrono::system_clock::time_point> expires_;
};

// Cookie Jar 类
class CookieJar {
public:
    CookieJar() = default;

    void set_cookie(const Cookie& cookie) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 删除已过期的cookie
        if (cookie.is_expired()) {
            cookies_.erase(cookie.name());
            return;
        }

        cookies_[cookie.name()] = cookie;
    }

    std::optional<Cookie> get_cookie(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cookies_.find(name);
        if (it != cookies_.end() && !it->second.is_expired()) {
            return it->second;
        }

        return std::nullopt;
    }

    std::vector<Cookie> get_cookies() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Cookie> result;
        for (const auto& [_, cookie] : cookies_) {
            if (!cookie.is_expired()) {
                result.push_back(cookie);
            }
        }

        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cookies_.clear();
    }

    // 从文件加载 cookies
    bool load_from_file(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        cookies_.clear();

        std::string line;
        while (std::getline(file, line)) {
            // 解析 Netscape cookie 文件格式
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
                    std::optional<std::chrono::system_clock::time_point>
                        expires;
                    if (expires_time > 0) {
                        expires = std::chrono::system_clock::from_time_t(
                            expires_time);
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

    // 保存 cookies 到文件
    bool save_to_file(const std::string& filename) const {
        std::lock_guard<std::mutex> lock(mutex_);

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
                    file << std::chrono::system_clock::to_time_t(
                        *cookie.expires());
                } else {
                    file << "0";
                }

                file << "\t" << cookie.name() << "\t" << cookie.value() << "\n";
            }
        }

        return true;
    }

    // 从 HTTP 响应头中提取 cookies
    void parse_cookies_from_headers(
        const std::map<std::string, std::string>& headers,
        const std::string& domain) {
        // 查找所有 Set-Cookie 头
        for (const auto& [name, value] : headers) {
            if (name == "Set-Cookie") {
                parse_cookie_header(value, domain);
            }
        }
    }

private:
    std::unordered_map<std::string, Cookie> cookies_;
    mutable std::mutex mutex_;

    // 解析单个 Set-Cookie 头
    void parse_cookie_header(const std::string& header,
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

        // 提取名称和值
        if (std::regex_search(cookie_str, match, cookie_regex)) {
            name = match[1];
            value = match[2];
            cookie_str = match.suffix();

            // 移除前导空格
            name = std::regex_replace(name, std::regex("^\\s+"), "");
            value = std::regex_replace(value, std::regex("^\\s+"), "");
        } else {
            return;  // 无效的 cookie
        }

        // 提取属性
        std::regex attr_regex(R"(;\s*([^=]+)(?:=([^;]*))?(?:;|$))");
        while (std::regex_search(cookie_str, match, attr_regex)) {
            std::string attr_name = match[1];
            std::string attr_value = match[2].matched ? match[2].str() : "";

            // 移除前导和尾随空格
            attr_name =
                std::regex_replace(attr_name, std::regex("^\\s+|\\s+$"), "");
            attr_value =
                std::regex_replace(attr_value, std::regex("^\\s+|\\s+$"), "");

            // 转换为小写进行不区分大小写的比较
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
                // 解析 HTTP 日期格式
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
                    // 忽略无效的 max-age
                }
            }

            cookie_str = match.suffix();
        }

        Cookie cookie(name, value, domain, path, secure, http_only, expires);
        set_cookie(cookie);
    }
};

// HTTP响应类
class Response {
public:
    Response() : status_code_(0) {}  // Add default constructor

    Response(int status_code, std::vector<char> body,
             std::map<std::string, std::string> headers)
        : status_code_(status_code),
          body_(std::move(body)),
          headers_(std::move(headers)) {}

    int status_code() const noexcept { return status_code_; }

    const std::vector<char>& body() const noexcept { return body_; }

    std::string body_string() const { return {body_.data(), body_.size()}; }

    // 将响应体解析为JSON（需要包含JSON库，这里使用简单字符串替代）
    std::string json() const { return body_string(); }

    const std::map<std::string, std::string>& headers() const noexcept {
        return headers_;
    }

    bool ok() const noexcept {
        return status_code_ >= 200 && status_code_ < 300;
    }

    bool redirect() const noexcept {
        return status_code_ >= 300 && status_code_ < 400;
    }

    bool client_error() const noexcept {
        return status_code_ >= 400 && status_code_ < 500;
    }

    bool server_error() const noexcept {
        return status_code_ >= 500 && status_code_ < 600;
    }

    bool has_header(const std::string& name) const {
        return headers_.find(name) != headers_.end();
    }

    std::string get_header(const std::string& name) const {
        auto it = headers_.find(name);
        return it != headers_.end() ? it->second : "";
    }

    std::optional<std::string> content_type() const {
        auto it = headers_.find("Content-Type");
        return it != headers_.end() ? std::optional<std::string>(it->second)
                                    : std::nullopt;
    }

    std::optional<size_t> content_length() const {
        auto it = headers_.find("Content-Length");
        if (it != headers_.end()) {
            try {
                return std::stoul(it->second);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

private:
    int status_code_;
    std::vector<char> body_;
    std::map<std::string, std::string> headers_;
};

// 拦截器抽象类
class Interceptor {
public:
    virtual ~Interceptor() = default;

    // 请求前拦截
    virtual void before_request(CURL* handle, const Request& request) = 0;

    // 响应后拦截
    virtual void after_response(CURL* handle, const Request& request,
                                const Response& response) = 0;
};

// HTTP请求类
class Request {
public:
    enum class Method { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS };

    Request() = default;

    Request& method(Method m) {
        method_ = m;
        return *this;
    }

    Request& url(std::string_view url) {
        url_ = url;
        return *this;
    }

    Request& header(std::string_view name, std::string_view value) {
        headers_[std::string(name)] = std::string(value);
        return *this;
    }

    Request& headers(std::map<std::string, std::string> headers) {
        headers_ = std::move(headers);
        return *this;
    }

    Request& body(std::vector<char> body) {
        body_ = std::move(body);
        return *this;
    }

    Request& body(std::string_view body) {
        body_.assign(body.begin(), body.end());
        return *this;
    }

    Request& timeout(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
        return *this;
    }

    Request& connection_timeout(std::chrono::milliseconds timeout) {
        connection_timeout_ = timeout;
        return *this;
    }

    Request& follow_redirects(bool follow) {
        follow_redirects_ = follow;
        return *this;
    }

    Request& max_redirects(long max) {
        max_redirects_ = max;
        return *this;
    }

    Request& verify_ssl(bool verify) {
        verify_ssl_ = verify;
        return *this;
    }

    Request& ca_path(std::string_view path) {
        ca_path_ = path;
        return *this;
    }

    Request& ca_info(std::string_view info) {
        ca_info_ = info;
        return *this;
    }

    Request& client_cert(std::string_view cert, std::string_view key) {
        client_cert_ = cert;
        client_key_ = key;
        return *this;
    }

    Request& proxy(std::string_view proxy) {
        proxy_ = proxy;
        return *this;
    }

    Request& proxy_type(curl_proxytype type) {
        proxy_type_ = type;
        return *this;
    }

    Request& proxy_auth(std::string_view username, std::string_view password) {
        proxy_username_ = username;
        proxy_password_ = password;
        return *this;
    }

    Request& basic_auth(std::string_view username, std::string_view password) {
        username_ = username;
        password_ = password;
        return *this;
    }

    Request& bearer_auth(std::string_view token) {
        header("Authorization", "Bearer " + std::string(token));
        return *this;
    }

    Request& multipart_form(MultipartForm& form);

    Request& cookie(const Cookie& cookie) {
        cookies_.push_back(cookie);
        return *this;
    }

    Request& cookie_jar(CookieJar* jar) {
        cookie_jar_ = jar;
        return *this;
    }

    Request& user_agent(std::string_view agent) {
        user_agent_ = agent;
        return *this;
    }

    Request& accept_encoding(std::string_view encoding) {
        accept_encoding_ = encoding;
        return *this;
    }

    Request& low_speed_limit(long limit) {
        low_speed_limit_ = limit;
        return *this;
    }

    Request& low_speed_time(long time) {
        low_speed_time_ = time;
        return *this;
    }

    Request& resume_from(curl_off_t offset) {
        resume_from_ = offset;
        return *this;
    }

    Request& http_version(long version) {
        http_version_ = version;
        return *this;
    }

    Request& http2(bool enabled = true) {
        http_version_ = enabled ? CURL_HTTP_VERSION_2 : CURL_HTTP_VERSION_1_1;
        return *this;
    }

    Request& http3(bool enabled = true) {
        http_version_ = enabled ? CURL_HTTP_VERSION_3 : CURL_HTTP_VERSION_1_1;
        return *this;
    }

    Request& retries(int count) {
        retries_ = count;
        return *this;
    }

    Request& retry_delay(std::chrono::milliseconds delay) {
        retry_delay_ = delay;
        return *this;
    }

    Request& retry_on_error(bool retry) {
        retry_on_error_ = retry;
        return *this;
    }

    Request& add_interceptor(std::shared_ptr<Interceptor> interceptor) {
        interceptors_.push_back(std::move(interceptor));
        return *this;
    }

    const std::string& url() const noexcept { return url_; }
    Method method() const noexcept { return method_; }
    const std::map<std::string, std::string>& headers() const noexcept {
        return headers_;
    }
    const std::vector<char>& body() const noexcept { return body_; }
    std::optional<std::chrono::milliseconds> timeout() const noexcept {
        return timeout_;
    }
    std::optional<std::chrono::milliseconds> connection_timeout()
        const noexcept {
        return connection_timeout_;
    }
    bool follow_redirects() const noexcept { return follow_redirects_; }
    std::optional<long> max_redirects() const noexcept {
        return max_redirects_;
    }
    bool verify_ssl() const noexcept { return verify_ssl_; }
    const std::optional<std::string>& ca_path() const noexcept {
        return ca_path_;
    }
    const std::optional<std::string>& ca_info() const noexcept {
        return ca_info_;
    }
    const std::optional<std::string>& client_cert() const noexcept {
        return client_cert_;
    }
    const std::optional<std::string>& client_key() const noexcept {
        return client_key_;
    }
    const std::optional<std::string>& proxy() const noexcept { return proxy_; }
    std::optional<curl_proxytype> proxy_type() const noexcept {
        return proxy_type_;
    }
    const std::optional<std::string>& proxy_username() const noexcept {
        return proxy_username_;
    }
    const std::optional<std::string>& proxy_password() const noexcept {
        return proxy_password_;
    }
    const std::optional<std::string>& username() const noexcept {
        return username_;
    }
    const std::optional<std::string>& password() const noexcept {
        return password_;
    }
    curl_mime* form() const noexcept { return form_; }
    const std::vector<Cookie>& cookies() const noexcept { return cookies_; }
    CookieJar* cookie_jar() const noexcept { return cookie_jar_; }
    const std::optional<std::string>& user_agent() const noexcept {
        return user_agent_;
    }
    const std::optional<std::string>& accept_encoding() const noexcept {
        return accept_encoding_;
    }
    std::optional<long> low_speed_limit() const noexcept {
        return low_speed_limit_;
    }
    std::optional<long> low_speed_time() const noexcept {
        return low_speed_time_;
    }
    std::optional<curl_off_t> resume_from() const noexcept {
        return resume_from_;
    }
    std::optional<long> http_version() const noexcept { return http_version_; }
    int retries() const noexcept { return retries_; }
    std::chrono::milliseconds retry_delay() const noexcept {
        return retry_delay_;
    }
    bool retry_on_error() const noexcept { return retry_on_error_; }
    const std::vector<std::shared_ptr<Interceptor>>& interceptors()
        const noexcept {
        return interceptors_;
    }

private:
    std::string url_;
    Method method_ = Method::GET;
    std::map<std::string, std::string> headers_;
    std::vector<char> body_;
    std::optional<std::chrono::milliseconds> timeout_;
    std::optional<std::chrono::milliseconds> connection_timeout_;
    bool follow_redirects_ = true;
    std::optional<long> max_redirects_;
    bool verify_ssl_ = true;
    std::optional<std::string> ca_path_;
    std::optional<std::string> ca_info_;
    std::optional<std::string> client_cert_;
    std::optional<std::string> client_key_;
    std::optional<std::string> proxy_;
    std::optional<curl_proxytype> proxy_type_;
    std::optional<std::string> proxy_username_;
    std::optional<std::string> proxy_password_;
    std::optional<std::string> username_;
    std::optional<std::string> password_;
    curl_mime* form_ = nullptr;
    std::vector<Cookie> cookies_;
    CookieJar* cookie_jar_ = nullptr;
    std::optional<std::string> user_agent_;
    std::optional<std::string> accept_encoding_;
    std::optional<long> low_speed_limit_;
    std::optional<long> low_speed_time_;
    std::optional<curl_off_t> resume_from_;
    std::optional<long> http_version_;
    int retries_ = 0;
    std::chrono::milliseconds retry_delay_ = std::chrono::seconds(1);
    bool retry_on_error_ = false;
    std::vector<std::shared_ptr<Interceptor>> interceptors_;
};

// 用于文件上传的辅助类
class MultipartForm {
public:
    MultipartForm() : form_(nullptr) {}

    ~MultipartForm() {
        if (form_) {
            curl_mime_free(form_);
        }
    }

    // 禁止拷贝
    MultipartForm(const MultipartForm&) = delete;
    MultipartForm& operator=(const MultipartForm&) = delete;

    // 允许移动
    MultipartForm(MultipartForm&& other) noexcept : form_(other.form_) {
        other.form_ = nullptr;
    }

    MultipartForm& operator=(MultipartForm&& other) noexcept {
        if (this != &other) {
            if (form_) {
                curl_mime_free(form_);
            }
            form_ = other.form_;
            other.form_ = nullptr;
        }
        return *this;
    }

    // 添加文件
    void add_file(std::string_view name, std::string_view filepath,
                  std::string_view content_type = "") {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_filedata(part, filepath.data());
        if (!content_type.empty()) {
            curl_mime_type(part, content_type.data());
        }
    }

    // 添加内存数据作为文件
    void add_buffer(std::string_view name, const void* data, size_t size,
                    std::string_view filename,
                    std::string_view content_type = "") {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_data(part, static_cast<const char*>(data), size);
        curl_mime_filename(part, filename.data());
        if (!content_type.empty()) {
            curl_mime_type(part, content_type.data());
        }
    }

    // 添加表单字段
    void add_field(std::string_view name, std::string_view content) {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_data(part, content.data(), content.size());
    }

    // 添加表单字段，指定内容类型
    void add_field_with_type(std::string_view name, std::string_view content,
                             std::string_view content_type) {
        if (!form_) {
            initialize();
        }

        curl_mimepart* part = curl_mime_addpart(form_);
        curl_mime_name(part, name.data());
        curl_mime_data(part, content.data(), content.size());
        curl_mime_type(part, content_type.data());
    }

    curl_mime* handle() const { return form_; }

private:
    curl_mime* form_;

    void initialize() {
        CURL* curl = curl_easy_init();
        form_ = curl_mime_init(curl);
        curl_easy_cleanup(curl);
    }

    friend class Session;
};

inline Request& Request::multipart_form(MultipartForm& form) {
    form_ = form.handle();
    return *this;
}

// 缓存系统
class Cache {
public:
    struct CacheEntry {
        Response response;
        std::chrono::system_clock::time_point expires;
        std::string etag;
        std::string last_modified;
    };

    Cache(std::chrono::seconds default_ttl = std::chrono::minutes(5))
        : default_ttl_(default_ttl) {}

    void set(const std::string& url, const Response& response,
             std::optional<std::chrono::seconds> ttl = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex_);

        CacheEntry entry{
            response,
            std::chrono::system_clock::now() + (ttl ? *ttl : default_ttl_),
            "",  // empty etag
            ""   // empty last_modified
        };

        // 从响应中提取 ETag 和 Last-Modified
        auto it_etag = response.headers().find("ETag");
        if (it_etag != response.headers().end()) {
            entry.etag = it_etag->second;
        }

        auto it_last_modified = response.headers().find("Last-Modified");
        if (it_last_modified != response.headers().end()) {
            entry.last_modified = it_last_modified->second;
        }

        cache_[url] = std::move(entry);
    }

    std::optional<Response> get(const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(url);
        if (it != cache_.end()) {
            if (std::chrono::system_clock::now() < it->second.expires) {
                return it->second.response;
            } else {
                // 过期但保留条件验证所需的字段
                stale_[url] = std::move(it->second);
                cache_.erase(it);
            }
        }

        return std::nullopt;
    }

    void invalidate(const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(url);
        stale_.erase(url);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        stale_.clear();
    }

    // 获取条件验证请求的头信息
    std::map<std::string, std::string> get_validation_headers(
        const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, std::string> headers;

        auto it = stale_.find(url);
        if (it != stale_.end()) {
            if (!it->second.etag.empty()) {
                headers["If-None-Match"] = it->second.etag;
            }

            if (!it->second.last_modified.empty()) {
                headers["If-Modified-Since"] = it->second.last_modified;
            }
        }

        return headers;
    }

    // 处理304响应，恢复缓存
    void handle_not_modified(const std::string& url) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = stale_.find(url);
        if (it != stale_.end()) {
            it->second.expires =
                std::chrono::system_clock::now() + default_ttl_;
            cache_[url] = std::move(it->second);
            stale_.erase(it);
        }
    }

private:
    std::chrono::seconds default_ttl_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::unordered_map<std::string, CacheEntry>
        stale_;  // 过期但可能仍有效的条目
    std::mutex mutex_;
};

// 速率限制器
class RateLimiter {
public:
    // 构造函数，设置每秒最大请求数
    RateLimiter(double requests_per_second)
        : requests_per_second_(requests_per_second),
          min_delay_(std::chrono::microseconds(
              static_cast<int64_t>(1000000 / requests_per_second))),
          last_request_time_(std::chrono::steady_clock::now()) {}

    // 等待，确保不超过速率限制
    void wait() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_request_time_;

        if (elapsed < min_delay_) {
            auto delay = min_delay_ - elapsed;
            std::this_thread::sleep_for(delay);
        }

        last_request_time_ = std::chrono::steady_clock::now();
    }

    // 设置新的速率限制
    void set_rate(double requests_per_second) {
        std::lock_guard<std::mutex> lock(mutex_);
        requests_per_second_ = requests_per_second;
        min_delay_ = std::chrono::microseconds(
            static_cast<int64_t>(1000000 / requests_per_second));
    }

private:
    double requests_per_second_;
    std::chrono::microseconds min_delay_;
    std::chrono::steady_clock::time_point last_request_time_;
    std::mutex mutex_;
};

// 连接池
class ConnectionPool {
public:
    ConnectionPool(size_t max_connections = 10)
        : max_connections_(max_connections) {}

    ~ConnectionPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto handle : pool_) {
            curl_easy_cleanup(handle);
        }
    }

    CURL* acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!pool_.empty()) {
            CURL* handle = pool_.back();
            pool_.pop_back();
            return handle;
        }

        // 如果池为空，创建新的连接
        return curl_easy_init();
    }

    void release(CURL* handle) {
        if (!handle)
            return;

        std::unique_lock<std::mutex> lock(mutex_);

        // 重置并添加到池中
        curl_easy_reset(handle);

        if (pool_.size() < max_connections_) {
            pool_.push_back(handle);
        } else {
            // 如果池已满，释放连接
            curl_easy_cleanup(handle);
        }
    }

private:
    size_t max_connections_;
    std::vector<CURL*> pool_;
    std::mutex mutex_;
};

// WebSocket 类
class WebSocket {
public:
    using MessageCallback = std::function<void(const std::string&, bool)>;
    using ConnectCallback = std::function<void(bool)>;
    using CloseCallback = std::function<void(int, const std::string&)>;

    WebSocket() : handle_(nullptr), running_(false), connected_(false) {}

    ~WebSocket() {
        close();
        if (handle_) {
            curl_easy_cleanup(handle_);
        }
    }

    bool connect(const std::string& url,
                 const std::map<std::string, std::string>& headers = {}) {
        if (connected_ || running_) {
            return false;
        }

        url_ = url;
        handle_ = curl_easy_init();
        if (!handle_) {
            return false;
        }

        // 设置 libcurl 选项
        curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle_, CURLOPT_CONNECT_ONLY, 2L);  // WebSocket

        // 设置请求头
        struct curl_slist* header_list = nullptr;
        for (const auto& [name, value] : headers) {
            std::string header = name + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }

        // 添加 WebSocket 特定的头
        header_list = curl_slist_append(header_list, "Connection: Upgrade");
        header_list = curl_slist_append(header_list, "Upgrade: websocket");
        header_list =
            curl_slist_append(header_list, "Sec-WebSocket-Version: 13");

        curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, header_list);

        // 执行连接
        CURLcode result = curl_easy_perform(handle_);
        curl_slist_free_all(header_list);

        if (result != CURLE_OK) {
            curl_easy_cleanup(handle_);
            handle_ = nullptr;
            if (connect_callback_) {
                connect_callback_(false);
            }
            return false;
        }

        connected_ = true;
        running_ = true;

        // 启动接收线程
        receive_thread_ = std::thread(&WebSocket::receive_loop, this);

        if (connect_callback_) {
            connect_callback_(true);
        }

        return true;
    }

    void close(int code = 1000, const std::string& reason = "Normal closure") {
        if (!connected_) {
            return;
        }

        // 发送关闭帧
        send_close_frame(code, reason);

        // 停止接收线程
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        condition_.notify_all();

        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        connected_ = false;

        if (close_callback_) {
            close_callback_(code, reason);
        }
    }

    bool send(const std::string& message, bool binary = false) {
        if (!connected_ || !handle_) {
            return false;
        }

        size_t sent = 0;
        CURLcode result;

        // Create WebSocket frame header
        std::vector<char> frame;
        frame.reserve(message.size() + 10);  // Max header size is 10 bytes

        // First byte: FIN + Opcode
        frame.push_back(
            0x80 | (binary ? 0x02 : 0x01));  // 0x80=FIN, 0x01=text, 0x02=binary

        // Second byte: Mask + Payload length
        if (message.size() <= 125) {
            frame.push_back(static_cast<char>(message.size()));
        } else if (message.size() <= 65535) {
            frame.push_back(126);
            frame.push_back((message.size() >> 8) & 0xFF);
            frame.push_back(message.size() & 0xFF);
        } else {
            frame.push_back(127);
            uint64_t len = message.size();
            for (int i = 7; i >= 0; i--) {
                frame.push_back((len >> (i * 8)) & 0xFF);
            }
        }

        // Add message data
        frame.insert(frame.end(), message.begin(), message.end());

        // Send frame
        size_t sent_total = 0;
        while (sent_total < frame.size()) {
            result = curl_easy_send(handle_, frame.data() + sent_total,
                                    frame.size() - sent_total, &sent);
            if (result != CURLE_OK) {
                break;
            }
            sent_total += sent;
        }

        return result == CURLE_OK && sent_total == frame.size();
    }

    void on_message(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }

    void on_connect(ConnectCallback callback) {
        connect_callback_ = std::move(callback);
    }

    void on_close(CloseCallback callback) {
        close_callback_ = std::move(callback);
    }

    bool is_connected() const { return connected_; }

private:
    CURL* handle_;
    std::string url_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::thread receive_thread_;
    std::mutex mutex_;
    std::condition_variable condition_;

    MessageCallback message_callback_;
    ConnectCallback connect_callback_;
    CloseCallback close_callback_;

    void receive_loop() {
        const size_t buffer_size = 65536;
        std::vector<char> buffer(buffer_size);

        while (running_) {
            size_t received = 0;
            CURLcode result = curl_easy_recv(handle_, buffer.data(),
                                             buffer.size(), &received);

            if (result != CURLE_OK) {
                break;
            }

            if (received > 0) {
                // Basic frame parsing
                if (static_cast<unsigned char>(buffer[0]) ==
                    0x88) {                      // Close frame
                    uint16_t close_code = 1005;  // No status code
                    std::string reason;

                    if (received >= 4) {  // Skip 2 bytes header
                        close_code = (static_cast<uint16_t>(buffer[2]) << 8) |
                                     static_cast<uint8_t>(buffer[3]);
                        if (received > 4) {
                            reason =
                                std::string(buffer.data() + 4, received - 4);
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        running_ = false;
                        connected_ = false;
                    }

                    if (close_callback_) {
                        close_callback_(close_code, reason);
                    }

                    break;
                } else if (static_cast<unsigned char>(buffer[0]) == 0x81 ||
                           static_cast<unsigned char>(buffer[0]) ==
                               0x82) {  // Text or Binary frame
                    // Data frame
                    bool is_binary =
                        (static_cast<unsigned char>(buffer[0]) == 0x82);
                    if (message_callback_) {
                        message_callback_(std::string(buffer.data(), received),
                                          is_binary);
                    }
                }
                // 忽略其他控制帧
            }
        }
    }

    void send_close_frame(int code, const std::string& reason) {
        if (!connected_ || !handle_) {
            return;
        }

        std::vector<char> payload(reason.size() + 2);
        payload[0] = static_cast<char>((code >> 8) & 0xFF);
        payload[1] = static_cast<char>(code & 0xFF);
        std::memcpy(payload.data() + 2, reason.data(), reason.size());

        size_t sent = 0;
        // Workaround: send close frame payload directly since WebSocket frame
        // API is unavailable.
        CURLcode result =
            curl_easy_send(handle_, payload.data(), payload.size(), &sent);
        (void)result;
    }
};

// 会话池
class SessionPool {
public:
    SessionPool(size_t max_sessions = 10) : max_sessions_(max_sessions) {}

    ~SessionPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();  // 智能指针自动清理
    }

    std::shared_ptr<Session> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!pool_.empty()) {
            auto session = pool_.back();
            pool_.pop_back();
            return session;
        }

        // 如果池为空，创建新的会话
        return std::make_shared<Session>();
    }

    void release(std::shared_ptr<Session> session) {
        if (!session)
            return;

        std::unique_lock<std::mutex> lock(mutex_);

        if (pool_.size() < max_sessions_) {
            pool_.push_back(std::move(session));
        }
        // 如果池已满，session 会自动析构
    }

private:
    size_t max_sessions_;
    std::vector<std::shared_ptr<Session>> pool_;
    std::mutex mutex_;
};

// CURL会话类
class Session {
public:
    Session()
        : connection_pool_(nullptr), cache_(nullptr), rate_limiter_(nullptr) {
        curl_global_init(CURL_GLOBAL_ALL);
        handle_ = curl_easy_init();
        if (!handle_) {
            throw Error(CURLE_FAILED_INIT, "Failed to initialize curl");
        }
    }

    explicit Session(ConnectionPool* pool)
        : connection_pool_(pool), cache_(nullptr), rate_limiter_(nullptr) {
        curl_global_init(CURL_GLOBAL_ALL);
        handle_ = pool ? pool->acquire() : curl_easy_init();
        if (!handle_) {
            throw Error(CURLE_FAILED_INIT, "Failed to initialize curl");
        }
    }

    ~Session() {
        if (handle_) {
            if (connection_pool_) {
                connection_pool_->release(handle_);
            } else {
                curl_easy_cleanup(handle_);
            }
        }
        curl_global_cleanup();
    }

    // 禁止拷贝
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // 允许移动
    Session(Session&& other) noexcept
        : handle_(other.handle_),
          connection_pool_(other.connection_pool_),
          cache_(other.cache_),
          rate_limiter_(other.rate_limiter_),
          interceptors_(std::move(other.interceptors_)) {
        other.handle_ = nullptr;
        other.connection_pool_ = nullptr;
        other.cache_ = nullptr;
        other.rate_limiter_ = nullptr;
    }

    Session& operator=(Session&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                if (connection_pool_) {
                    connection_pool_->release(handle_);
                } else {
                    curl_easy_cleanup(handle_);
                }
            }
            handle_ = other.handle_;
            connection_pool_ = other.connection_pool_;
            cache_ = other.cache_;
            rate_limiter_ = other.rate_limiter_;
            interceptors_ = std::move(other.interceptors_);
            other.handle_ = nullptr;
            other.connection_pool_ = nullptr;
            other.cache_ = nullptr;
            other.rate_limiter_ = nullptr;
        }
        return *this;
    }

    // 注册拦截器
    void add_interceptor(std::shared_ptr<Interceptor> interceptor) {
        interceptors_.push_back(std::move(interceptor));
    }

    // 设置缓存
    void set_cache(Cache* cache) { cache_ = cache; }

    // 设置速率限制器
    void set_rate_limiter(RateLimiter* limiter) { rate_limiter_ = limiter; }

    // 同步执行请求
    Response execute(const Request& request) {
        // 检查是否应该使用缓存
        if (cache_ && request.method() == Request::Method::GET) {
            auto cached_response = cache_->get(request.url());
            if (cached_response) {
                return *cached_response;
            }

            // 如果有过期但可能仍有效的缓存，添加条件验证头
            auto validation_headers =
                cache_->get_validation_headers(request.url());
            Request modified_request = request;
            for (const auto& [name, value] : validation_headers) {
                modified_request.header(name, value);
            }

            // 执行请求
            Response response = execute_internal(modified_request);

            // 处理 304 Not Modified
            if (response.status_code() == 304) {
                cache_->handle_not_modified(request.url());
                auto cached_response = cache_->get(request.url());
                if (cached_response) {
                    return *cached_response;
                }
            } else if (response.ok()) {
                // 缓存响应
                cache_->set(request.url(), response);
            }

            return response;
        }

        return execute_internal(request);
    }

    // 异步执行请求
    std::future<Response> execute_async(const Request& request) {
        return std::async(std::launch::async,
                          [this, request]() { return execute(request); });
    }

    // 简化的GET请求
    Response get(std::string_view url) {
        Request req;
        req.method(Request::Method::GET).url(url);
        return execute(req);
    }

    // 带查询参数的GET请求
    Response get(std::string_view url,
                 const std::map<std::string, std::string>& params) {
        std::string full_url = std::string(url);

        // 添加查询参数
        if (!params.empty()) {
            full_url += (full_url.find('?') == std::string::npos) ? '?' : '&';

            bool first = true;
            for (const auto& [key, value] : params) {
                if (!first) {
                    full_url += '&';
                }
                full_url += url_encode(key) + '=' + url_encode(value);
                first = false;
            }
        }

        return get(full_url);
    }

    // 简化的POST请求
    Response post(std::string_view url, std::string_view body,
                  std::string_view content_type = "application/json") {
        Request req;
        req.method(Request::Method::POST)
            .url(url)
            .body(body)
            .header("Content-Type", content_type);
        return execute(req);
    }

    // Form URL encoded POST请求
    Response post_form(std::string_view url,
                       const std::map<std::string, std::string>& params) {
        std::string body;
        bool first = true;

        for (const auto& [key, value] : params) {
            if (!first) {
                body += '&';
            }
            body += url_encode(key) + '=' + url_encode(value);
            first = false;
        }

        return post(url, body, "application/x-www-form-urlencoded");
    }

    // JSON POST请求 (使用简单字符串替代真正的JSON库)
    Response post_json(std::string_view url, std::string_view json) {
        return post(url, json, "application/json");
    }

    // PUT请求
    Response put(std::string_view url, std::string_view body,
                 std::string_view content_type = "application/json") {
        Request req;
        req.method(Request::Method::PUT)
            .url(url)
            .body(body)
            .header("Content-Type", content_type);
        return execute(req);
    }

    // DELETE请求
    Response del(std::string_view url) {
        Request req;
        req.method(Request::Method::DELETE).url(url);
        return execute(req);
    }

    // PATCH请求
    Response patch(std::string_view url, std::string_view body,
                   std::string_view content_type = "application/json") {
        Request req;
        req.method(Request::Method::PATCH)
            .url(url)
            .body(body)
            .header("Content-Type", content_type);
        return execute(req);
    }

    // HEAD请求
    Response head(std::string_view url) {
        Request req;
        req.method(Request::Method::HEAD).url(url);
        return execute(req);
    }

    // OPTIONS请求
    Response options(std::string_view url) {
        Request req;
        req.method(Request::Method::OPTIONS).url(url);
        return execute(req);
    }

    // 下载文件
    Response download(std::string_view url, std::string_view filepath,
                      std::optional<curl_off_t> resume_from = std::nullopt) {
        Request req;
        req.method(Request::Method::GET).url(url);

        if (resume_from) {
            req.resume_from(*resume_from);
        }

        // 检查文件目录是否存在，如果不存在则创建
        std::filesystem::create_directories(
            std::filesystem::path(filepath).parent_path());

        // 打开文件以写入
        FILE* file = nullptr;
        if (resume_from) {
            file = fopen(std::string(filepath).c_str(), "a+b");  // 追加模式
        } else {
            file = fopen(std::string(filepath).c_str(), "wb");  // 写入模式
        }

        if (!file) {
            throw Error(CURLE_WRITE_ERROR, "Failed to open file for writing: " +
                                               std::string(filepath));
        }

        reset();
        setup_request(req);

        // 设置文件写入回调
        curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, file_write_callback);
        curl_easy_setopt(handle_, CURLOPT_WRITEDATA, file);

        Response response = perform();
        fclose(file);

        return response;
    }

    // 上传文件
    Response upload(std::string_view url, std::string_view filepath,
                    std::string_view field_name = "file",
                    std::optional<curl_off_t> resume_from = std::nullopt) {
        MultipartForm form;
        form.add_file(field_name, filepath);

        Request req;
        req.method(Request::Method::POST).url(url).multipart_form(form);

        if (resume_from) {
            req.resume_from(*resume_from);
        }

        return execute(req);
    }

    // 设置进度回调
    void set_progress_callback(
        std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>
            callback) {
        progress_callback_.callback = std::move(callback);
        curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION,
                         &ProgressCallback::xferinfo);
        curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, &progress_callback_);
        curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 0L);
    }

    // URL编码
    static std::string url_encode(std::string_view str) {
        char* encoded = curl_easy_escape(nullptr, str.data(),
                                         static_cast<int>(str.length()));
        if (!encoded) {
            return std::string(str);
        }

        std::string result(encoded);
        curl_free(encoded);
        return result;
    }

    // URL解码
    static std::string url_decode(std::string_view str) {
        char* decoded = curl_easy_unescape(
            nullptr, str.data(), static_cast<int>(str.length()), nullptr);
        if (!decoded) {
            return std::string(str);
        }

        std::string result(decoded);
        curl_free(decoded);
        return result;
    }

    // 创建WebSocket连接
    std::shared_ptr<WebSocket> create_websocket(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {}) {
        auto ws = std::make_shared<WebSocket>();
        if (ws->connect(url, headers)) {
            return ws;
        }
        return nullptr;
    }

private:
    CURL* handle_ = nullptr;
    ConnectionPool* connection_pool_ = nullptr;
    Cache* cache_ = nullptr;
    RateLimiter* rate_limiter_ = nullptr;
    std::vector<std::shared_ptr<Interceptor>> interceptors_;
    std::vector<char> response_body_;
    std::map<std::string, std::string> response_headers_;
    char error_buffer_[CURL_ERROR_SIZE] = {0};

    struct ProgressCallback {
        std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>
            callback;

        static int xferinfo(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow) {
            auto* callback = static_cast<ProgressCallback*>(clientp);
            return callback->callback(dltotal, dlnow, ultotal, ulnow);
        }
    } progress_callback_;

    void reset() {
        curl_easy_reset(handle_);
        response_body_.clear();
        response_headers_.clear();
        error_buffer_[0] = 0;
    }

    void setup_request(const Request& request) {
        // 调用拦截器 before_request
        for (const auto& interceptor : interceptors_) {
            interceptor->before_request(handle_, request);
        }

        // 合并请求和拦截器中的拦截器
        for (const auto& interceptor : request.interceptors()) {
            interceptor->before_request(handle_, request);
        }

        // 设置URL
        curl_easy_setopt(handle_, CURLOPT_URL, request.url().c_str());

        // 设置错误缓冲区
        curl_easy_setopt(handle_, CURLOPT_ERRORBUFFER, error_buffer_);

        // 设置回调函数
        curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response_body_);
        curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &response_headers_);

        // 设置HTTP方法
        switch (request.method()) {
            case Request::Method::GET:
                curl_easy_setopt(handle_, CURLOPT_HTTPGET, 1L);
                break;
            case Request::Method::POST:
                curl_easy_setopt(handle_, CURLOPT_POST, 1L);
                if (!request.body().empty()) {
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::PUT:
                curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!request.body().empty()) {
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::DELETE:
                curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case Request::Method::PATCH:
                curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PATCH");
                if (!request.body().empty()) {
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::HEAD:
                curl_easy_setopt(handle_, CURLOPT_NOBODY, 1L);
                break;
            case Request::Method::OPTIONS:
                curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "OPTIONS");
                break;
        }

        // 设置请求头
        struct curl_slist* headers = nullptr;
        for (const auto& [name, value] : request.headers()) {
            std::string header = name + ": " + value;
            headers = curl_slist_append(headers, header.c_str());
        }

        if (headers) {
            curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers);
        }

        // 设置超时
        if (request.timeout()) {
            curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS,
                             request.timeout()->count());
        }

        // 设置连接超时
        if (request.connection_timeout()) {
            curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS,
                             request.connection_timeout()->count());
        }

        // 设置重定向
        curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION,
                         request.follow_redirects() ? 1L : 0L);
        if (request.max_redirects()) {
            curl_easy_setopt(handle_, CURLOPT_MAXREDIRS,
                             *request.max_redirects());
        }

        // 设置SSL验证
        curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER,
                         request.verify_ssl() ? 1L : 0L);
        curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST,
                         request.verify_ssl() ? 2L : 0L);

        // CA路径
        if (request.ca_path()) {
            curl_easy_setopt(handle_, CURLOPT_CAPATH,
                             request.ca_path()->c_str());
        }

        // CA信息
        if (request.ca_info()) {
            curl_easy_setopt(handle_, CURLOPT_CAINFO,
                             request.ca_info()->c_str());
        }

        // 客户端证书
        if (request.client_cert() && request.client_key()) {
            curl_easy_setopt(handle_, CURLOPT_SSLCERT,
                             request.client_cert()->c_str());
            curl_easy_setopt(handle_, CURLOPT_SSLKEY,
                             request.client_key()->c_str());
        }

        // 代理设置
        if (request.proxy()) {
            curl_easy_setopt(handle_, CURLOPT_PROXY, request.proxy()->c_str());

            if (request.proxy_type()) {
                curl_easy_setopt(handle_, CURLOPT_PROXYTYPE,
                                 *request.proxy_type());
            }

            if (request.proxy_username() && request.proxy_password()) {
                curl_easy_setopt(handle_, CURLOPT_PROXYUSERNAME,
                                 request.proxy_username()->c_str());
                curl_easy_setopt(handle_, CURLOPT_PROXYPASSWORD,
                                 request.proxy_password()->c_str());
            }
        }

        // 基本认证
        if (request.username() && request.password()) {
            curl_easy_setopt(handle_, CURLOPT_USERNAME,
                             request.username()->c_str());
            curl_easy_setopt(handle_, CURLOPT_PASSWORD,
                             request.password()->c_str());
        }

        // multipart表单
        if (request.form()) {
            curl_easy_setopt(handle_, CURLOPT_MIMEPOST, request.form());
        }

        // Cookie
        for (const auto& cookie : request.cookies()) {
            curl_easy_setopt(handle_, CURLOPT_COOKIE,
                             cookie.to_string().c_str());
        }

        // UserAgent
        if (request.user_agent()) {
            curl_easy_setopt(handle_, CURLOPT_USERAGENT,
                             request.user_agent()->c_str());
        }

        // Accept-Encoding
        if (request.accept_encoding()) {
            curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING,
                             request.accept_encoding()->c_str());
        }

        // 低速限制
        if (request.low_speed_limit() && request.low_speed_time()) {
            curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT,
                             *request.low_speed_limit());
            curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME,
                             *request.low_speed_time());
        }

        // 断点续传
        if (request.resume_from()) {
            curl_easy_setopt(handle_, CURLOPT_RESUME_FROM_LARGE,
                             *request.resume_from());
        }

        // HTTP版本
        if (request.http_version()) {
            curl_easy_setopt(handle_, CURLOPT_HTTP_VERSION,
                             *request.http_version());
        }
    }

    Response execute_internal(const Request& request) {
        // 速率限制
        if (rate_limiter_) {
            rate_limiter_->wait();
        }

        // 自动重试逻辑
        int retries_left = request.retries();
        while (true) {
            try {
                reset();
                setup_request(request);
                Response response = perform();

                // 根据请求处理CookieJar
                if (request.cookie_jar()) {
                    std::string domain;
                    {
                        // 从URL中提取域名
                        CURLU* url_handle = curl_url();
                        curl_url_set(url_handle, CURLUPART_URL,
                                     request.url().c_str(), 0);
                        char* host;
                        curl_url_get(url_handle, CURLUPART_HOST, &host, 0);
                        if (host) {
                            domain = host;
                            curl_free(host);
                        }
                        curl_url_cleanup(url_handle);
                    }

                    request.cookie_jar()->parse_cookies_from_headers(
                        response.headers(), domain);
                }

                // 调用拦截器 after_response
                for (const auto& interceptor : interceptors_) {
                    interceptor->after_response(handle_, request, response);
                }

                for (const auto& interceptor : request.interceptors()) {
                    interceptor->after_response(handle_, request, response);
                }

                return response;
            } catch (const Error& e) {
                if (retries_left > 0 && request.retry_on_error()) {
                    retries_left--;
                    std::this_thread::sleep_for(request.retry_delay());
                    continue;
                }
                throw;
            }
        }
    }

    Response perform() {
        CURLcode res = curl_easy_perform(handle_);
        if (res != CURLE_OK) {
            std::string error_msg =
                error_buffer_[0] ? error_buffer_ : curl_easy_strerror(res);
            throw Error(res, error_msg);
        }

        long status_code = 0;
        curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &status_code);

        return Response(status_code, std::move(response_body_),
                        std::move(response_headers_));
    }

    static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
        size_t realsize = size * nmemb;
        auto* body = static_cast<std::vector<char>*>(userdata);

        size_t current_size = body->size();
        body->resize(current_size + realsize);
        std::memcpy(body->data() + current_size, ptr, realsize);

        return realsize;
    }

    static size_t header_callback(char* buffer, size_t size, size_t nitems,
                                  void* userdata) {
        size_t realsize = size * nitems;
        auto* headers =
            static_cast<std::map<std::string, std::string>*>(userdata);

        std::string header(buffer, realsize);
        size_t pos = header.find(':');
        if (pos != std::string::npos) {
            std::string name = header.substr(0, pos);
            std::string value = header.substr(pos + 1);

            // 修剪空白
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);

            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            (*headers)[name] = value;
        }

        return realsize;
    }

    static size_t file_write_callback(char* ptr, size_t size, size_t nmemb,
                                      void* userdata) {
        size_t written = fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
        return written;
    }
};

// Multi会话类，用于并行请求
class MultiSession {
public:
    MultiSession() : multi_handle_(curl_multi_init()) {
        if (!multi_handle_) {
            throw Error(CURLE_FAILED_INIT,
                        "Failed to initialize curl multi handle");
        }
    }

    ~MultiSession() {
        if (multi_handle_) {
            curl_multi_cleanup(multi_handle_);
        }
    }

    // 添加请求
    void add_request(
        const Request& request,
        std::function<void(Response)> callback = nullptr,
        std::function<void(const Error&)> error_callback = nullptr) {
        CURL* handle = curl_easy_init();
        if (!handle) {
            throw Error(CURLE_FAILED_INIT, "Failed to initialize curl handle");
        }

        // 为每个请求创建一个上下文
        auto context = std::make_shared<RequestContext>();
        context->request = request;
        context->callback = std::move(callback);
        context->error_callback = std::move(error_callback);
        context->handle = handle;

        // 设置请求选项，和Session类似
        setup_request(request, context.get());

        // 添加到multi handle
        CURLMcode mc = curl_multi_add_handle(multi_handle_, handle);
        if (mc != CURLM_OK) {
            curl_easy_cleanup(handle);
            throw Error(mc, "Failed to add handle to multi session");
        }

        handles_[handle] = context;
    }

    // 执行所有请求并等待完成
    void perform() {
        int still_running = 0;
        CURLMcode mc = curl_multi_perform(multi_handle_, &still_running);

        if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM) {
            throw Error(mc, "curl_multi_perform failed");
        }

        // 当仍有请求在进行时进行循环
        while (still_running) {
            // 设置超时时间
            int numfds = 0;
            mc = curl_multi_wait(multi_handle_, nullptr, 0, 1000, &numfds);

            if (mc != CURLM_OK) {
                throw Error(mc, "curl_multi_wait failed");
            }

            // 继续执行
            mc = curl_multi_perform(multi_handle_, &still_running);

            if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM) {
                throw Error(mc, "curl_multi_perform failed");
            }

            // 检查是否有已完成的传输
            check_multi_info();
        }

        // 确保处理所有消息
        check_multi_info();
    }

private:
    struct RequestContext {
        Request request;
        std::function<void(Response)> callback;
        std::function<void(const Error&)> error_callback;
        CURL* handle;
        std::vector<char> response_body;
        std::map<std::string, std::string> response_headers;
        char error_buffer[CURL_ERROR_SIZE] = {0};
        struct curl_slist* headers = nullptr;
    };

    CURLM* multi_handle_;
    std::map<CURL*, std::shared_ptr<RequestContext>> handles_;

    void setup_request(const Request& request, RequestContext* context) {
        CURL* handle = context->handle;

        // 设置URL
        curl_easy_setopt(handle, CURLOPT_URL, request.url().c_str());

        // 设置错误缓冲区
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, context->error_buffer);

        // 设置回调函数
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &context->response_body);
        curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(handle, CURLOPT_HEADERDATA,
                         &context->response_headers);

        // 设置私有指针
        curl_easy_setopt(handle, CURLOPT_PRIVATE, context);

        // 设置HTTP方法
        switch (request.method()) {
            case Request::Method::GET:
                curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
                break;
            case Request::Method::POST:
                curl_easy_setopt(handle, CURLOPT_POST, 1L);
                if (!request.body().empty()) {
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::PUT:
                curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!request.body().empty()) {
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::DELETE:
                curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case Request::Method::PATCH:
                curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PATCH");
                if (!request.body().empty()) {
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                     request.body().data());
                    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                     request.body().size());
                }
                break;
            case Request::Method::HEAD:
                curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
                break;
            case Request::Method::OPTIONS:
                curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
                break;
        }

        // 设置请求头
        context->headers = nullptr;
        for (const auto& [name, value] : request.headers()) {
            std::string header = name + ": " + value;
            context->headers =
                curl_slist_append(context->headers, header.c_str());
        }

        if (context->headers) {
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, context->headers);
        }

        // 设置超时
        if (request.timeout()) {
            curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS,
                             request.timeout()->count());
        }

        // 设置连接超时
        if (request.connection_timeout()) {
            curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS,
                             request.connection_timeout()->count());
        }

        // 设置重定向
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION,
                         request.follow_redirects() ? 1L : 0L);
        if (request.max_redirects()) {
            curl_easy_setopt(handle, CURLOPT_MAXREDIRS,
                             *request.max_redirects());
        }

        // 设置SSL验证
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER,
                         request.verify_ssl() ? 1L : 0L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST,
                         request.verify_ssl() ? 2L : 0L);

        // 其他选项与Session类似
    }

    void check_multi_info() {
        CURLMsg* msg = nullptr;
        int msgs_left = 0;

        while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                CURL* handle = msg->easy_handle;
                CURLcode result = msg->data.result;

                // 查找请求上下文
                auto it = handles_.find(handle);
                if (it != handles_.end()) {
                    auto context = it->second;

                    try {
                        if (result != CURLE_OK) {
                            throw Error(result,
                                        context->error_buffer[0]
                                            ? context->error_buffer
                                            : curl_easy_strerror(result));
                        }

                        long status_code = 0;
                        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE,
                                          &status_code);

                        Response response(status_code,
                                          std::move(context->response_body),
                                          std::move(context->response_headers));

                        // 调用回调
                        if (context->callback) {
                            context->callback(response);
                        }
                    } catch (const Error& e) {
                        if (context->error_callback) {
                            context->error_callback(e);
                        }
                    }

                    // 清理
                    if (context->headers) {
                        curl_slist_free_all(context->headers);
                    }

                    // 从multi handle中移除
                    curl_multi_remove_handle(multi_handle_, handle);
                    curl_easy_cleanup(handle);
                    handles_.erase(it);
                }
            }
        }
    }

    static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
        size_t realsize = size * nmemb;
        auto* body = static_cast<std::vector<char>*>(userdata);

        size_t current_size = body->size();
        body->resize(current_size + realsize);
        std::memcpy(body->data() + current_size, ptr, realsize);

        return realsize;
    }

    static size_t header_callback(char* buffer, size_t size, size_t nitems,
                                  void* userdata) {
        size_t realsize = size * nitems;
        auto* headers =
            static_cast<std::map<std::string, std::string>*>(userdata);

        std::string header(buffer, realsize);
        size_t pos = header.find(':');
        if (pos != std::string::npos) {
            std::string name = header.substr(0, pos);
            std::string value = header.substr(pos + 1);

            // 修剪空白
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);

            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            (*headers)[name] = value;
        }

        return realsize;
    }
};

// 日志拦截器示例
class LoggingInterceptor : public Interceptor {
public:
    explicit LoggingInterceptor(std::ostream& out = std::cout) : out_(out) {}

    void before_request([[maybe_unused]] CURL* handle,
                        const Request& request) override {
        out_ << "Request: " << to_string(request.method()) << " "
             << request.url() << std::endl;

        for (const auto& [name, value] : request.headers()) {
            out_ << "  " << name << ": " << value << std::endl;
        }

        if (!request.body().empty()) {
            out_ << "  Body: "
                 << std::string(request.body().data(),
                                std::min(request.body().size(), size_t(100)))
                 << (request.body().size() > 100 ? "..." : "") << std::endl;
        }
    }

    void after_response([[maybe_unused]] CURL* handle,
                        [[maybe_unused]] const Request& request,
                        const Response& response) override {
        out_ << "Response: " << response.status_code() << std::endl;

        for (const auto& [name, value] : response.headers()) {
            out_ << "  " << name << ": " << value << std::endl;
        }

        if (!response.body().empty()) {
            out_ << "  Body: "
                 << std::string(response.body().data(),
                                std::min(response.body().size(), size_t(100)))
                 << (response.body().size() > 100 ? "..." : "") << std::endl;
        }
    }

private:
    std::ostream& out_;

    static std::string to_string(Request::Method method) {
        switch (method) {
            case Request::Method::GET:
                return "GET";
            case Request::Method::POST:
                return "POST";
            case Request::Method::PUT:
                return "PUT";
            case Request::Method::DELETE:
                return "DELETE";
            case Request::Method::PATCH:
                return "PATCH";
            case Request::Method::HEAD:
                return "HEAD";
            case Request::Method::OPTIONS:
                return "OPTIONS";
            default:
                return "UNKNOWN";
        }
    }
};

// 便利函数
inline Response get(std::string_view url) {
    static thread_local Session session;
    return session.get(url);
}

inline Response post(std::string_view url, std::string_view body,
                     std::string_view content_type = "application/json") {
    static thread_local Session session;
    return session.post(url, body, content_type);
}

inline Response put(std::string_view url, std::string_view body,
                    std::string_view content_type = "application/json") {
    static thread_local Session session;
    return session.put(url, body, content_type);
}

inline Response del(std::string_view url) {
    static thread_local Session session;
    return session.del(url);
}

// C++20 概念支持
template <typename T>
concept ResponseHandler = requires(T t, const Response& response) {
    { t(response) };
};

template <typename T>
concept ErrorHandler = requires(T t, const Error& error) {
    { t(error) };
};

template <ResponseHandler OnSuccess, ErrorHandler OnError>
void fetch(const Request& request, OnSuccess&& on_success, OnError&& on_error) {
    try {
        Session session;
        Response response = session.execute(request);
        on_success(response);
    } catch (const Error& error) {
        on_error(error);
    }
}

// 协程支持
template <typename T>
class Task {
public:
    struct promise_type {
        T result;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) { result = std::move(value); }

        void unhandled_exception() { exception = std::current_exception(); }
    };

    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    T result() const {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.promise().result;
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

struct Awaitable {
    Request request;
    Response response;
    Error* error = nullptr;

    Awaitable(Request req) : request(std::move(req)), response(0, {}, {}) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        try {
            Session session;
            response = session.execute(request);
            handle.resume();
        } catch (const Error& e) {
            error = new Error(e);
            handle.resume();
        } catch (...) {
            handle.resume();
            throw;
        }
    }

    Response await_resume() {
        if (error) {
            Error e = *error;
            delete error;
            throw e;
        }
        return response;
    }
};

inline Awaitable fetch(Request request) {
    return Awaitable(std::move(request));
}

inline Task<Response> fetch_async(Request request) {
    try {
        Response response = co_await fetch(std::move(request));
        co_return response;
    } catch (const Error& e) {
        throw e;
    }
}

// 高级REST客户端示例
class RestClient {
public:
    RestClient(std::string base_url) : base_url_(std::move(base_url)) {
        session_.set_cache(&cache_);
        session_.set_rate_limiter(&rate_limiter_);
        session_.add_interceptor(std::make_shared<LoggingInterceptor>());
    }

    Response get(std::string_view path,
                 const std::map<std::string, std::string>& params = {}) {
        return session_.get(make_url(path), params);
    }

    Response post(std::string_view path, std::string_view json) {
        return session_.post(make_url(path), json);
    }

    Response put(std::string_view path, std::string_view json) {
        return session_.put(make_url(path), json);
    }

    Response del(std::string_view path) { return session_.del(make_url(path)); }

    void set_header(std::string_view name, std::string_view value) {
        default_headers_[std::string(name)] = std::string(value);
    }

    void set_auth_token(std::string_view token) {
        set_header("Authorization", "Bearer " + std::string(token));
    }

    void set_rate_limit(double requests_per_second) {
        rate_limiter_.set_rate(requests_per_second);
    }

    void clear_cache() { cache_.clear(); }

private:
    std::string base_url_;
    Session session_;
    std::map<std::string, std::string> default_headers_;
    Cache cache_;
    RateLimiter rate_limiter_{10.0};  // 默认每秒10个请求

    std::string make_url(std::string_view path) {
        if (path.empty()) {
            return base_url_;
        }

        if (path[0] == '/') {
            return base_url_ + std::string(path);
        } else {
            return base_url_ + "/" + std::string(path);
        }
    }
};

}  // namespace curl