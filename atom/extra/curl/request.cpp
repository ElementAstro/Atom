#include "request.hpp"

#include "multipart.hpp"

namespace atom::extra::curl {
Request& Request::method(Method m) {
    method_ = m;
    return *this;
}

Request& Request::url(std::string_view url) {
    url_ = std::string(url);
    return *this;
}

Request& Request::header(std::string_view name, std::string_view value) {
    headers_[std::string(name)] = std::string(value);
    return *this;
}

Request& Request::headers(std::map<std::string, std::string> headers) {
    headers_ = std::move(headers);
    return *this;
}

Request& Request::body(std::vector<char> body) {
    body_ = std::move(body);
    return *this;
}

Request& Request::body(std::string_view body) {
    body_.assign(body.begin(), body.end());
    return *this;
}

Request& Request::timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
}

Request& Request::connection_timeout(std::chrono::milliseconds timeout) {
    connection_timeout_ = timeout;
    return *this;
}

Request& Request::follow_redirects(bool follow) {
    follow_redirects_ = follow;
    return *this;
}

Request& Request::max_redirects(long max) {
    max_redirects_ = max;
    return *this;
}

Request& Request::verify_ssl(bool verify) {
    verify_ssl_ = verify;
    return *this;
}

Request& Request::ca_path(std::string_view path) {
    ca_path_ = std::string(path);
    return *this;
}

Request& Request::ca_info(std::string_view info) {
    ca_info_ = std::string(info);
    return *this;
}

Request& Request::client_cert(std::string_view cert, std::string_view key) {
    client_cert_ = std::string(cert);
    client_key_ = std::string(key);
    return *this;
}

Request& Request::proxy(std::string_view proxy) {
    proxy_ = std::string(proxy);
    return *this;
}

Request& Request::proxy_type(curl_proxytype type) {
    proxy_type_ = type;
    return *this;
}

Request& Request::proxy_auth(std::string_view username,
                             std::string_view password) {
    proxy_username_ = std::string(username);
    proxy_password_ = std::string(password);
    return *this;
}

Request& Request::basic_auth(std::string_view username,
                             std::string_view password) {
    username_ = std::string(username);
    password_ = std::string(password);
    return *this;
}

Request& Request::bearer_auth(std::string_view token) {
    header("Authorization", "Bearer " + std::string(token));
    return *this;
}

Request& Request::multipart_form(MultipartForm& form) {
    form_ = form.handle();
    return *this;
}

Request& Request::cookie(const Cookie& cookie) {
    cookies_.push_back(cookie);
    return *this;
}

Request& Request::cookie_jar(CookieJar* jar) {
    cookie_jar_ = jar;
    return *this;
}

Request& Request::user_agent(std::string_view agent) {
    user_agent_ = std::string(agent);
    return *this;
}

Request& Request::accept_encoding(std::string_view encoding) {
    accept_encoding_ = std::string(encoding);
    return *this;
}

Request& Request::low_speed_limit(long limit) {
    low_speed_limit_ = limit;
    return *this;
}

Request& Request::low_speed_time(long time) {
    low_speed_time_ = time;
    return *this;
}

Request& Request::resume_from(curl_off_t offset) {
    resume_from_ = offset;
    return *this;
}

Request& Request::http_version(long version) {
    http_version_ = version;
    return *this;
}

Request& Request::http2(bool enabled) {
    http_version_ = enabled ? CURL_HTTP_VERSION_2 : CURL_HTTP_VERSION_1_1;
    return *this;
}

Request& Request::http3(bool enabled) {
    http_version_ = enabled ? CURL_HTTP_VERSION_3 : CURL_HTTP_VERSION_1_1;
    return *this;
}

Request& Request::retries(int count) {
    retries_ = count;
    return *this;
}

Request& Request::retry_delay(std::chrono::milliseconds delay) {
    retry_delay_ = delay;
    return *this;
}

Request& Request::retry_on_error(bool retry) {
    retry_on_error_ = retry;
    return *this;
}

Request& Request::add_interceptor(std::shared_ptr<Interceptor> interceptor) {
    interceptors_.push_back(std::move(interceptor));
    return *this;
}

const std::string& Request::url() const noexcept { return url_; }

Request::Method Request::method() const noexcept { return method_; }

const std::map<std::string, std::string>& Request::headers() const noexcept {
    return headers_;
}

const std::vector<char>& Request::body() const noexcept { return body_; }

std::optional<std::chrono::milliseconds> Request::timeout() const noexcept {
    return timeout_;
}

std::optional<std::chrono::milliseconds> Request::connection_timeout()
    const noexcept {
    return connection_timeout_;
}

bool Request::follow_redirects() const noexcept { return follow_redirects_; }

std::optional<long> Request::max_redirects() const noexcept {
    return max_redirects_;
}

bool Request::verify_ssl() const noexcept { return verify_ssl_; }

const std::optional<std::string>& Request::ca_path() const noexcept {
    return ca_path_;
}

const std::optional<std::string>& Request::ca_info() const noexcept {
    return ca_info_;
}

const std::optional<std::string>& Request::client_cert() const noexcept {
    return client_cert_;
}

const std::optional<std::string>& Request::client_key() const noexcept {
    return client_key_;
}

const std::optional<std::string>& Request::proxy() const noexcept {
    return proxy_;
}

std::optional<curl_proxytype> Request::proxy_type() const noexcept {
    return proxy_type_;
}

const std::optional<std::string>& Request::proxy_username() const noexcept {
    return proxy_username_;
}

const std::optional<std::string>& Request::proxy_password() const noexcept {
    return proxy_password_;
}

const std::optional<std::string>& Request::username() const noexcept {
    return username_;
}

const std::optional<std::string>& Request::password() const noexcept {
    return password_;
}

curl_mime* Request::form() const noexcept { return form_; }

const std::vector<Cookie>& Request::cookies() const noexcept {
    return cookies_;
}

CookieJar* Request::cookie_jar() const noexcept { return cookie_jar_; }

const std::optional<std::string>& Request::user_agent() const noexcept {
    return user_agent_;
}

const std::optional<std::string>& Request::accept_encoding() const noexcept {
    return accept_encoding_;
}

std::optional<long> Request::low_speed_limit() const noexcept {
    return low_speed_limit_;
}

std::optional<long> Request::low_speed_time() const noexcept {
    return low_speed_time_;
}

std::optional<curl_off_t> Request::resume_from() const noexcept {
    return resume_from_;
}

std::optional<long> Request::http_version() const noexcept {
    return http_version_;
}

int Request::retries() const noexcept { return retries_; }

std::chrono::milliseconds Request::retry_delay() const noexcept {
    return retry_delay_;
}

bool Request::retry_on_error() const noexcept { return retry_on_error_; }

const std::vector<std::shared_ptr<Interceptor>>& Request::interceptors()
    const noexcept {
    return interceptors_;
}
}  // namespace atom::extra::curl
