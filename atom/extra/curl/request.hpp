#ifndef ATOM_EXTRA_CURL_REQUEST_HPP
#define ATOM_EXTRA_CURL_REQUEST_HPP

#include <curl/curl.h>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cookie.hpp"
#include "interceptor.hpp"
#include "multipart.hpp"

#ifdef _WIN32
#undef DELETE
#endif

namespace atom::extra::curl {
/**
 * @brief Class representing an HTTP request.
 *
 * This class provides a fluent interface for building HTTP requests,
 * allowing you to set various options such as the URL, method, headers,
 * body, timeout, and more.
 */
class Request {
public:
    /**
     * @brief Enum class representing the HTTP method.
     */
    enum class Method {
        /** @brief HTTP GET method. */
        GET,
        /** @brief HTTP POST method. */
        POST,
        /** @brief HTTP PUT method. */
        PUT,
        /** @brief HTTP DELETE method. */
        DELETE,
        /** @brief HTTP PATCH method. */
        PATCH,
        /** @brief HTTP HEAD method. */
        HEAD,
        /** @brief HTTP OPTIONS method. */
        OPTIONS
    };

    /**
     * @brief Default constructor for the Request class.
     *
     * Initializes a new Request object with default values.
     */
    Request() = default;

    /**
     * @brief Sets the HTTP method for the request.
     *
     * @param m The HTTP method to use.
     * @return A reference to the Request object.
     */
    Request& method(Method m);

    /**
     * @brief Sets the URL for the request.
     *
     * @param url The URL to request.
     * @return A reference to the Request object.
     */
    Request& url(std::string_view url);

    /**
     * @brief Sets a header for the request.
     *
     * @param name The name of the header.
     * @param value The value of the header.
     * @return A reference to the Request object.
     */
    Request& header(std::string_view name, std::string_view value);

    /**
     * @brief Sets multiple headers for the request.
     *
     * @param headers A map of header names to header values.
     * @return A reference to the Request object.
     */
    Request& headers(std::map<std::string, std::string> headers);

    /**
     * @brief Sets the body for the request.
     *
     * @param body A vector of characters representing the body.
     * @return A reference to the Request object.
     */
    Request& body(std::vector<char> body);

    /**
     * @brief Sets the body for the request.
     *
     * @param body A string view representing the body.
     * @return A reference to the Request object.
     */
    Request& body(std::string_view body);

    /**
     * @brief Sets the timeout for the request.
     *
     * @param timeout The timeout in milliseconds.
     * @return A reference to the Request object.
     */
    Request& timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Sets the connection timeout for the request.
     *
     * @param timeout The connection timeout in milliseconds.
     * @return A reference to the Request object.
     */
    Request& connection_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Sets whether to follow redirects.
     *
     * @param follow True to follow redirects, false otherwise.
     * @return A reference to the Request object.
     */
    Request& follow_redirects(bool follow);

    /**
     * @brief Sets the maximum number of redirects to follow.
     *
     * @param max The maximum number of redirects to follow.
     * @return A reference to the Request object.
     */
    Request& max_redirects(long max);

    /**
     * @brief Sets whether to verify SSL certificates.
     *
     * @param verify True to verify SSL certificates, false otherwise.
     * @return A reference to the Request object.
     */
    Request& verify_ssl(bool verify);

    /**
     * @brief Sets the path to the CA bundle.
     *
     * @param path The path to the CA bundle.
     * @return A reference to the Request object.
     */
    Request& ca_path(std::string_view path);

    /**
     * @brief Sets the CA info.
     *
     * @param info The CA info.
     * @return A reference to the Request object.
     */
    Request& ca_info(std::string_view info);

    /**
     * @brief Sets the client certificate and key.
     *
     * @param cert The path to the client certificate.
     * @param key The path to the client key.
     * @return A reference to the Request object.
     */
    Request& client_cert(std::string_view cert, std::string_view key);

    /**
     * @brief Sets the proxy for the request.
     *
     * @param proxy The proxy URL.
     * @return A reference to the Request object.
     */
    Request& proxy(std::string_view proxy);

    /**
     * @brief Sets the proxy type for the request.
     *
     * @param type The proxy type.
     * @return A reference to the Request object.
     */
    Request& proxy_type(curl_proxytype type);

    /**
     * @brief Sets the proxy authentication credentials.
     *
     * @param username The proxy username.
     * @param password The proxy password.
     * @return A reference to the Request object.
     */
    Request& proxy_auth(std::string_view username, std::string_view password);

    /**
     * @brief Sets the basic authentication credentials.
     *
     * @param username The username.
     * @param password The password.
     * @return A reference to the Request object.
     */
    Request& basic_auth(std::string_view username, std::string_view password);

    /**
     * @brief Sets the bearer authentication token.
     *
     * @param token The bearer token.
     * @return A reference to the Request object.
     */
    Request& bearer_auth(std::string_view token);

    /**
     * @brief Sets the multipart form for the request.
     *
     * @param form The multipart form to use.
     * @return A reference to the Request object.
     */
    Request& multipart_form(MultipartForm& form);

    /**
     * @brief Sets a cookie for the request.
     *
     * @param cookie The cookie to set.
     * @return A reference to the Request object.
     */
    Request& cookie(const Cookie& cookie);

    /**
     * @brief Sets the cookie jar for the request.
     *
     * @param jar The cookie jar to use.
     * @return A reference to the Request object.
     */
    Request& cookie_jar(CookieJar* jar);

    /**
     * @brief Sets the user agent for the request.
     *
     * @param agent The user agent string.
     * @return A reference to the Request object.
     */
    Request& user_agent(std::string_view agent);

    /**
     * @brief Sets the accept encoding for the request.
     *
     * @param encoding The accept encoding string.
     * @return A reference to the Request object.
     */
    Request& accept_encoding(std::string_view encoding);

    /**
     * @brief Sets the low speed limit for the request.
     *
     * @param limit The low speed limit in bytes per second.
     * @return A reference to the Request object.
     */
    Request& low_speed_limit(long limit);

    /**
     * @brief Sets the low speed time for the request.
     *
     * @param time The low speed time in seconds.
     * @return A reference to the Request object.
     */
    Request& low_speed_time(long time);

    /**
     * @brief Sets the offset to resume from for the request.
     *
     * @param offset The offset to resume from in bytes.
     * @return A reference to the Request object.
     */
    Request& resume_from(curl_off_t offset);

    /**
     * @brief Sets the HTTP version for the request.
     *
     * @param version The HTTP version to use.
     * @return A reference to the Request object.
     */
    Request& http_version(long version);

    /**
     * @brief Enables or disables HTTP/2 for the request.
     *
     * @param enabled True to enable HTTP/2, false to disable it.
     * @return A reference to the Request object.
     */
    Request& http2(bool enabled = true);

    /**
     * @brief Enables or disables HTTP/3 for the request.
     *
     * @param enabled True to enable HTTP/3, false to disable it.
     * @return A reference to the Request object.
     */
    Request& http3(bool enabled = true);

    /**
     * @brief Sets the number of retries for the request.
     *
     * @param count The number of retries.
     * @return A reference to the Request object.
     */
    Request& retries(int count);

    /**
     * @brief Sets the delay between retries for the request.
     *
     * @param delay The delay between retries in milliseconds.
     * @return A reference to the Request object.
     */
    Request& retry_delay(std::chrono::milliseconds delay);

    /**
     * @brief Sets whether to retry the request on error.
     *
     * @param retry True to retry on error, false otherwise.
     * @return A reference to the Request object.
     */
    Request& retry_on_error(bool retry);

    /**
     * @brief Adds an interceptor to the request.
     *
     * @param interceptor A shared pointer to the interceptor to add.
     * @return A reference to the Request object.
     */
    Request& add_interceptor(std::shared_ptr<Interceptor> interceptor);

    /**
     * @brief Gets the URL for the request.
     *
     * @return The URL for the request.
     */
    const std::string& url() const noexcept;

    /**
     * @brief Gets the HTTP method for the request.
     *
     * @return The HTTP method for the request.
     */
    Method method() const noexcept;

    /**
     * @brief Gets the headers for the request.
     *
     * @return The headers for the request.
     */
    const std::map<std::string, std::string>& headers() const noexcept;

    /**
     * @brief Gets the body for the request.
     *
     * @return The body for the request.
     */
    const std::vector<char>& body() const noexcept;

    /**
     * @brief Gets the timeout for the request.
     *
     * @return The timeout for the request, or std::nullopt if no timeout is
     * set.
     */
    std::optional<std::chrono::milliseconds> timeout() const noexcept;

    /**
     * @brief Gets the connection timeout for the request.
     *
     * @return The connection timeout for the request, or std::nullopt if no
     * connection timeout is set.
     */
    std::optional<std::chrono::milliseconds> connection_timeout()
        const noexcept;

    /**
     * @brief Gets whether to follow redirects.
     *
     * @return True if redirects should be followed, false otherwise.
     */
    bool follow_redirects() const noexcept;

    /**
     * @brief Gets the maximum number of redirects to follow.
     *
     * @return The maximum number of redirects to follow, or std::nullopt if no
     * maximum is set.
     */
    std::optional<long> max_redirects() const noexcept;

    /**
     * @brief Gets whether to verify SSL certificates.
     *
     * @return True if SSL certificates should be verified, false otherwise.
     */
    bool verify_ssl() const noexcept;

    /**
     * @brief Gets the path to the CA bundle.
     *
     * @return The path to the CA bundle, or std::nullopt if no path is set.
     */
    const std::optional<std::string>& ca_path() const noexcept;

    /**
     * @brief Gets the CA info.
     *
     * @return The CA info, or std::nullopt if no info is set.
     */
    const std::optional<std::string>& ca_info() const noexcept;

    /**
     * @brief Gets the client certificate.
     *
     * @return The client certificate, or std::nullopt if no certificate is set.
     */
    const std::optional<std::string>& client_cert() const noexcept;

    /**
     * @brief Gets the client key.
     *
     * @return The client key, or std::nullopt if no key is set.
     */
    const std::optional<std::string>& client_key() const noexcept;

    /**
     * @brief Gets the proxy URL.
     *
     * @return The proxy URL, or std::nullopt if no proxy is set.
     */
    const std::optional<std::string>& proxy() const noexcept;

    /**
     * @brief Gets the proxy type.
     *
     * @return The proxy type, or std::nullopt if no proxy type is set.
     */
    std::optional<curl_proxytype> proxy_type() const noexcept;

    /**
     * @brief Gets the proxy username.
     *
     * @return The proxy username, or std::nullopt if no username is set.
     */
    const std::optional<std::string>& proxy_username() const noexcept;

    /**
     * @brief Gets the proxy password.
     *
     * @return The proxy password, or std::nullopt if no password is set.
     */
    const std::optional<std::string>& proxy_password() const noexcept;

    /**
     * @brief Gets the username for basic authentication.
     *
     * @return The username for basic authentication, or std::nullopt if no
     * username is set.
     */
    const std::optional<std::string>& username() const noexcept;

    /**
     * @brief Gets the password for basic authentication.
     *
     * @return The password for basic authentication, or std::nullopt if no
     * password is set.
     */
    const std::optional<std::string>& password() const noexcept;

    /**
     * @brief Gets the multipart form.
     *
     * @return The multipart form, or nullptr if no form is set.
     */
    curl_mime* form() const noexcept;

    /**
     * @brief Gets the cookies for the request.
     *
     * @return The cookies for the request.
     */
    const std::vector<Cookie>& cookies() const noexcept;

    /**
     * @brief Gets the cookie jar for the request.
     *
     * @return The cookie jar for the request, or nullptr if no cookie jar is
     * set.
     */
    CookieJar* cookie_jar() const noexcept;

    /**
     * @brief Gets the user agent string.
     *
     * @return The user agent string, or std::nullopt if no user agent is set.
     */
    const std::optional<std::string>& user_agent() const noexcept;

    /**
     * @brief Gets the accept encoding string.
     *
     * @return The accept encoding string, or std::nullopt if no accept encoding
     * is set.
     */
    const std::optional<std::string>& accept_encoding() const noexcept;

    /**
     * @brief Gets the low speed limit.
     *
     * @return The low speed limit, or std::nullopt if no limit is set.
     */
    std::optional<long> low_speed_limit() const noexcept;

    /**
     * @brief Gets the low speed time.
     *
     * @return The low speed time, or std::nullopt if no time is set.
     */
    std::optional<long> low_speed_time() const noexcept;

    /**
     * @brief Gets the offset to resume from.
     *
     * @return The offset to resume from, or std::nullopt if no offset is set.
     */
    std::optional<curl_off_t> resume_from() const noexcept;

    /**
     * @brief Gets the HTTP version.
     *
     * @return The HTTP version, or std::nullopt if no version is set.
     */
    std::optional<long> http_version() const noexcept;

    /**
     * @brief Gets the number of retries.
     *
     * @return The number of retries.
     */
    int retries() const noexcept;

    /**
     * @brief Gets the retry delay.
     *
     * @return The retry delay.
     */
    std::chrono::milliseconds retry_delay() const noexcept;

    /**
     * @brief Gets whether to retry on error.
     *
     * @return True if the request should be retried on error, false otherwise.
     */
    bool retry_on_error() const noexcept;

    /**
     * @brief Gets the interceptors for the request.
     *
     * @return The interceptors for the request.
     */
    const std::vector<std::shared_ptr<Interceptor>>& interceptors()
        const noexcept;

private:
    /** @brief The URL to request. */
    std::string url_;
    /** @brief The HTTP method to use. */
    Method method_ = Method::GET;
    /** @brief The headers to send with the request. */
    std::map<std::string, std::string> headers_;
    /** @brief The body to send with the request. */
    std::vector<char> body_;
    /** @brief The timeout for the request. */
    std::optional<std::chrono::milliseconds> timeout_;
    /** @brief The connection timeout for the request. */
    std::optional<std::chrono::milliseconds> connection_timeout_;
    /** @brief Whether to follow redirects. */
    bool follow_redirects_ = true;
    /** @brief The maximum number of redirects to follow. */
    std::optional<long> max_redirects_;
    /** @brief Whether to verify SSL certificates. */
    bool verify_ssl_ = true;
    /** @brief The path to the CA bundle. */
    std::optional<std::string> ca_path_;
    /** @brief The CA info. */
    std::optional<std::string> ca_info_;
    /** @brief The client certificate. */
    std::optional<std::string> client_cert_;
    /** @brief The client key. */
    std::optional<std::string> client_key_;
    /** @brief The proxy URL. */
    std::optional<std::string> proxy_;
    /** @brief The proxy type. */
    std::optional<curl_proxytype> proxy_type_;
    /** @brief The proxy username. */
    std::optional<std::string> proxy_username_;
    /** @brief The proxy password. */
    std::optional<std::string> proxy_password_;
    /** @brief The username for basic authentication. */
    std::optional<std::string> username_;
    /** @brief The password for basic authentication. */
    std::optional<std::string> password_;
    /** @brief The multipart form to send with the request. */
    curl_mime* form_ = nullptr;
    /** @brief The cookies to send with the request. */
    std::vector<Cookie> cookies_;
    /** @brief The cookie jar to use for the request. */
    CookieJar* cookie_jar_ = nullptr;
    /** @brief The user agent string to send with the request. */
    std::optional<std::string> user_agent_;
    /** @brief The accept encoding string to send with the request. */
    std::optional<std::string> accept_encoding_;
    /** @brief The low speed limit for the request. */
    std::optional<long> low_speed_limit_;
    /** @brief The low speed time for the request. */
    std::optional<long> low_speed_time_;
    /** @brief The offset to resume from for the request. */
    std::optional<curl_off_t> resume_from_;
    /** @brief The HTTP version to use for the request. */
    std::optional<long> http_version_;
    /** @brief The number of retries for the request. */
    int retries_ = 0;
    /** @brief The delay between retries for the request. */
    std::chrono::milliseconds retry_delay_ = std::chrono::seconds(1);
    /** @brief Whether to retry the request on error. */
    bool retry_on_error_ = false;
    /** @brief The interceptors to use for the request. */
    std::vector<std::shared_ptr<Interceptor>> interceptors_;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_REQUEST_HPP