#ifndef ATOM_EXTRA_CURL_REQUEST_HPP
#define ATOM_EXTRA_CURL_REQUEST_HPP

#include <curl/curl.h>
#include <chrono>
#include <map>
#include <string_view>
#include <vector>

#include "cookie.hpp"
#include "interceptor.hpp"
#include "multipart.hpp"

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

#endif