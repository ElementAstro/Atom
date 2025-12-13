/*
 * curl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-3

Description: Modern C++20 HTTP client wrapper for libcurl

**************************************************/

#ifndef ATOM_WEB_HTTP_CURL_HPP
#define ATOM_WEB_HTTP_CURL_HPP

#include <curl/curl.h>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom/type/compat.hpp"

namespace atom::web {

// Use compatibility expected type for cross-compiler support
template <typename T, typename E>
using expected = atom::type::compat::expected<T, E>;
template <typename E>
using unexpected = atom::type::compat::unexpected<E>;

/**
 * @brief HTTP method enumeration
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE_,
    PATCH,
    HEAD,
    OPTIONS,
    TRACE,
    CONNECT
};

/**
 * @brief Convert HttpMethod to string
 */
[[nodiscard]] constexpr auto httpMethodToString(HttpMethod method) noexcept
    -> std::string_view {
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE_:
            return "DELETE";
        case HttpMethod::PATCH:
            return "PATCH";
        case HttpMethod::HEAD:
            return "HEAD";
        case HttpMethod::OPTIONS:
            return "OPTIONS";
        case HttpMethod::TRACE:
            return "TRACE";
        case HttpMethod::CONNECT:
            return "CONNECT";
        default:
            return "GET";
    }
}

/**
 * @brief HTTP error codes
 */
enum class HttpError {
    Success = 0,
    ConnectionFailed,
    Timeout,
    SSLError,
    DNSError,
    InvalidUrl,
    TooManyRedirects,
    RequestCancelled,
    NetworkError,
    InvalidResponse,
    AuthenticationFailed,
    PermissionDenied,
    NotFound,
    ServerError,
    Unknown
};

/**
 * @brief Get error message for HTTP error code
 */
[[nodiscard]] constexpr auto httpErrorToString(HttpError error) noexcept
    -> std::string_view {
    switch (error) {
        case HttpError::Success:
            return "Success";
        case HttpError::ConnectionFailed:
            return "Connection failed";
        case HttpError::Timeout:
            return "Request timed out";
        case HttpError::SSLError:
            return "SSL/TLS error";
        case HttpError::DNSError:
            return "DNS resolution failed";
        case HttpError::InvalidUrl:
            return "Invalid URL";
        case HttpError::TooManyRedirects:
            return "Too many redirects";
        case HttpError::RequestCancelled:
            return "Request cancelled";
        case HttpError::NetworkError:
            return "Network error";
        case HttpError::InvalidResponse:
            return "Invalid response";
        case HttpError::AuthenticationFailed:
            return "Authentication failed";
        case HttpError::PermissionDenied:
            return "Permission denied";
        case HttpError::NotFound:
            return "Not found";
        case HttpError::ServerError:
            return "Server error";
        case HttpError::Unknown:
            return "Unknown error";
        default:
            return "Unknown error";
    }
}

/**
 * @brief HTTP response structure containing status, headers, and body.
 */
struct HttpResponse {
    int statusCode{0};
    std::string statusMessage;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds responseTime{0};
    size_t downloadedBytes{0};
    size_t uploadedBytes{0};
    std::string effectiveUrl;
    std::string contentType;
    std::string redirectUrl;
    std::vector<std::string> cookies;

    [[nodiscard]] auto isSuccess() const noexcept -> bool {
        return statusCode >= 200 && statusCode < 300;
    }

    [[nodiscard]] auto isRedirect() const noexcept -> bool {
        return statusCode >= 300 && statusCode < 400;
    }

    [[nodiscard]] auto isClientError() const noexcept -> bool {
        return statusCode >= 400 && statusCode < 500;
    }

    [[nodiscard]] auto isServerError() const noexcept -> bool {
        return statusCode >= 500;
    }

    [[nodiscard]] auto isError() const noexcept -> bool {
        return statusCode >= 400;
    }

    [[nodiscard]] auto getHeader(std::string_view name) const
        -> std::optional<std::string> {
        std::string lowerName(name);
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (const auto& [key, value] : headers) {
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lowerKey == lowerName) {
                return value;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto getContentLength() const -> std::optional<size_t> {
        if (auto cl = getHeader("content-length")) {
            try {
                return std::stoull(*cl);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return body.empty(); }

    [[nodiscard]] explicit operator bool() const noexcept {
        return isSuccess();
    }
};

/**
 * @brief Form field for multipart form data
 */
struct FormField {
    std::string name;
    std::variant<std::string, std::vector<std::byte>> value;
    std::string filename;     ///< For file uploads
    std::string contentType;  ///< MIME type for the field

    [[nodiscard]] static auto text(std::string_view name,
                                   std::string_view value) -> FormField {
        return {std::string(name), std::string(value), "", ""};
    }

    [[nodiscard]] static auto file(
        std::string_view name, std::string_view filename,
        std::span<const std::byte> data,
        std::string_view contentType = "application/octet-stream")
        -> FormField {
        return {std::string(name),
                std::vector<std::byte>(data.begin(), data.end()),
                std::string(filename), std::string(contentType)};
    }
};

/**
 * @brief Request configuration options.
 */
struct RequestConfig {
    std::chrono::seconds timeout{30};
    std::chrono::seconds connectTimeout{10};
    bool followRedirects{true};
    int maxRedirects{10};
    bool verifySSL{true};
    bool verifyHost{true};
    std::string userAgent{"atom-http/1.0"};
    std::string proxy;
    std::optional<std::pair<std::string, std::string>> basicAuth;
    std::optional<std::string> bearerToken;
    std::optional<std::string> clientCert;
    std::optional<std::string> clientKey;
    std::optional<std::string> caCert;
    size_t maxDownloadSpeed{0};  ///< Bytes per second, 0 = unlimited
    size_t maxUploadSpeed{0};    ///< Bytes per second, 0 = unlimited
    bool keepAlive{true};
    std::chrono::seconds keepAliveIdle{60};
    std::chrono::seconds keepAliveInterval{60};
    bool compressRequest{false};
    bool acceptCompressed{true};
    std::string acceptLanguage;
    std::vector<std::string> dnsServers;  ///< Custom DNS servers
};

/**
 * @brief HTTP request builder for fluent API
 */
class HttpRequest {
public:
    HttpRequest() = default;
    explicit HttpRequest(std::string_view url);

    // Fluent setters
    auto url(std::string_view url) -> HttpRequest&;
    auto method(HttpMethod method) -> HttpRequest&;
    auto header(std::string_view key, std::string_view value) -> HttpRequest&;
    auto headers(const std::unordered_map<std::string, std::string>& hdrs)
        -> HttpRequest&;
    auto body(std::string_view data) -> HttpRequest&;
    auto body(std::span<const std::byte> data) -> HttpRequest&;
    auto jsonBody(std::string_view json) -> HttpRequest&;
    auto formData(const std::vector<FormField>& fields) -> HttpRequest&;
    auto queryParam(std::string_view key,
                    std::string_view value) -> HttpRequest&;
    auto timeout(std::chrono::seconds t) -> HttpRequest&;
    auto connectTimeout(std::chrono::seconds t) -> HttpRequest&;
    auto followRedirects(bool follow) -> HttpRequest&;
    auto maxRedirects(int max) -> HttpRequest&;
    auto verifySSL(bool verify) -> HttpRequest&;
    auto basicAuth(std::string_view username,
                   std::string_view password) -> HttpRequest&;
    auto bearerToken(std::string_view token) -> HttpRequest&;
    auto userAgent(std::string_view ua) -> HttpRequest&;
    auto proxy(std::string_view proxyUrl) -> HttpRequest&;
    auto config(const RequestConfig& cfg) -> HttpRequest&;

    // Getters
    [[nodiscard]] auto getUrl() const -> std::string_view { return url_; }
    [[nodiscard]] auto getMethod() const -> HttpMethod { return method_; }
    [[nodiscard]] auto getHeaders() const
        -> const std::unordered_map<std::string, std::string>& {
        return headers_;
    }
    [[nodiscard]] auto getBody() const -> std::string_view { return body_; }
    [[nodiscard]] auto getConfig() const -> const RequestConfig& {
        return config_;
    }

    // Execute methods
    [[nodiscard]] auto send() const -> expected<HttpResponse, HttpError>;
    [[nodiscard]] auto sendAsync() const
        -> std::future<expected<HttpResponse, HttpError>>;
    [[nodiscard]] auto sendAsync(std::stop_token stopToken) const
        -> std::future<expected<HttpResponse, HttpError>>;

private:
    std::string url_;
    HttpMethod method_{HttpMethod::GET};
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    std::vector<FormField> formFields_;
    std::unordered_map<std::string, std::string> queryParams_;
    RequestConfig config_;
    bool isMultipart_{false};
};

/**
 * @brief A comprehensive wrapper class for performing HTTP requests using
 * libcurl.
 */
class CurlWrapper {
public:
    /**
     * @brief Constructor for CurlWrapper.
     */
    CurlWrapper();

    /**
     * @brief Destructor for CurlWrapper.
     */
    ~CurlWrapper();

    CurlWrapper(const CurlWrapper& other) = delete;
    auto operator=(const CurlWrapper& other) -> CurlWrapper& = delete;
    CurlWrapper(CurlWrapper&& other) noexcept = delete;
    auto operator=(CurlWrapper&& other) noexcept -> CurlWrapper& = delete;

    /**
     * @brief Sets the URL for the HTTP request.
     * @param url The URL to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setUrl(std::string_view url) -> CurlWrapper&;

    /**
     * @brief Sets the HTTP request method.
     * @param method The HTTP method to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestMethod(HttpMethod method) -> CurlWrapper&;

    /**
     * @brief Sets the HTTP request method (e.g., GET, POST).
     * @param method The HTTP request method to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestMethod(std::string_view method) -> CurlWrapper&;

    /**
     * @brief Adds a custom header to the HTTP request.
     * @param key The header key.
     * @param value The header value.
     * @return Reference to the CurlWrapper object.
     */
    auto addHeader(std::string_view key,
                   std::string_view value) -> CurlWrapper&;

    /**
     * @brief Sets the callback function for handling errors.
     * @param callback The callback function.
     */
    void setOnErrorCallback(std::function<void(std::string_view)> callback);

    /**
     * @brief Sets the callback function for handling responses.
     * @param callback The callback function.
     */
    void setOnResponseCallback(std::function<void(std::string_view)> callback);

    /**
     * @brief Sets the progress callback for download/upload progress.
     * @param callback The callback function (downloaded, total, uploaded,
     * uploadTotal).
     */
    void setProgressCallback(
        std::function<void(size_t, size_t, size_t, size_t)> callback);

    /**
     * @brief Sets the timeout for the HTTP request.
     * @param timeout The timeout duration.
     * @return Reference to the CurlWrapper object.
     */
    auto setTimeout(std::chrono::seconds timeout) -> CurlWrapper&;

    /**
     * @brief Sets the timeout for the HTTP request.
     * @param timeout The timeout value in seconds.
     * @return Reference to the CurlWrapper object.
     */
    auto setTimeout(long timeout) -> CurlWrapper&;

    /**
     * @brief Sets whether to follow redirects.
     * @param follow Boolean value indicating whether to follow redirects.
     * @return Reference to the CurlWrapper object.
     */
    auto setFollowLocation(bool follow) -> CurlWrapper&;

    /**
     * @brief Sets the request body for POST requests.
     * @param data The request body data.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestBody(std::string_view data) -> CurlWrapper&;

    /**
     * @brief Sets the request body from binary data.
     * @param data The binary data.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestBody(std::span<const std::byte> data) -> CurlWrapper&;

    /**
     * @brief Sets the file path for uploading a file.
     * @param filePath The file path to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setUploadFile(std::string_view filePath) -> CurlWrapper&;

    /**
     * @brief Sets the proxy for the HTTP request.
     * @param proxy The proxy URL to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setProxy(std::string_view proxy) -> CurlWrapper&;

    /**
     * @brief Sets SSL options for the HTTP request.
     * @param verifyPeer Boolean value indicating whether to verify the peer's
     * SSL certificate.
     * @param verifyHost Boolean value indicating whether to verify the host's
     * SSL certificate.
     * @return Reference to the CurlWrapper object.
     */
    auto setSSLOptions(bool verifyPeer, bool verifyHost) -> CurlWrapper&;

    /**
     * @brief Performs the HTTP request synchronously.
     * @return The response as a string.
     */
    auto perform() -> std::string;

    /**
     * @brief Performs the HTTP request and returns structured response.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] auto execute() -> expected<HttpResponse, HttpError>;

    /**
     * @brief Performs an asynchronous HTTP request.
     */
    void performAsync();

    /**
     * @brief Performs an asynchronous HTTP request with stop token support.
     * @param stopToken Stop token for cancellation.
     * @return Future containing the response.
     */
    [[nodiscard]] auto performAsyncCancellable(std::stop_token stopToken)
        -> std::future<HttpResponse>;

    /**
     * @brief Performs an asynchronous HTTP request with expected result.
     * @param stopToken Stop token for cancellation.
     * @return Future containing expected response or error.
     */
    [[nodiscard]] auto executeAsync(std::stop_token stopToken = {})
        -> std::future<expected<HttpResponse, HttpError>>;

    /**
     * @brief Waits for all asynchronous requests to complete.
     */
    void waitAll();

    /**
     * @brief Perform a GET request with simplified API.
     * @param url The URL to request.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto get(std::string_view url,
                                  const RequestConfig& config = {})
        -> expected<HttpResponse, HttpError>;

    /**
     * @brief Perform a POST request with simplified API.
     * @param url The URL to request.
     * @param body The request body.
     * @param contentType The content type header.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto post(
        std::string_view url, std::string_view body,
        std::string_view contentType = "application/json",
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Perform a PUT request with simplified API.
     * @param url The URL to request.
     * @param body The request body.
     * @param contentType The content type header.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto put(
        std::string_view url, std::string_view body,
        std::string_view contentType = "application/json",
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Perform a PATCH request with simplified API.
     * @param url The URL to request.
     * @param body The request body.
     * @param contentType The content type header.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto patch(
        std::string_view url, std::string_view body,
        std::string_view contentType = "application/json",
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Perform a DELETE request with simplified API.
     * @param url The URL to request.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto del(std::string_view url,
                                  const RequestConfig& config = {})
        -> expected<HttpResponse, HttpError>;

    /**
     * @brief Perform a HEAD request.
     * @param url The URL to request.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto head(std::string_view url,
                                   const RequestConfig& config = {})
        -> expected<HttpResponse, HttpError>;

    /**
     * @brief Download a file to disk.
     * @param url The URL to download from.
     * @param filePath The local file path to save to.
     * @param progressCallback Optional progress callback.
     * @param stopToken Optional stop token for cancellation.
     * @return Expected containing downloaded bytes or HttpError.
     */
    [[nodiscard]] static auto downloadFile(
        std::string_view url, std::string_view filePath,
        std::function<void(size_t, size_t)> progressCallback = nullptr,
        std::stop_token stopToken = {}) -> expected<size_t, HttpError>;

    /**
     * @brief Upload a file.
     * @param url The URL to upload to.
     * @param filePath The local file path to upload.
     * @param fieldName The form field name.
     * @param progressCallback Optional progress callback.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto uploadFile(
        std::string_view url, std::string_view filePath,
        std::string_view fieldName = "file",
        std::function<void(size_t, size_t)> progressCallback = nullptr,
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Post JSON data.
     * @param url The URL to request.
     * @param json The JSON string.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto postJson(
        std::string_view url, std::string_view json,
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Post form data (application/x-www-form-urlencoded).
     * @param url The URL to request.
     * @param formData The form data as key-value pairs.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto postForm(
        std::string_view url,
        const std::unordered_map<std::string, std::string>& formData,
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Post multipart form data.
     * @param url The URL to request.
     * @param fields The form fields.
     * @param config Optional request configuration.
     * @return Expected containing HttpResponse or HttpError.
     */
    [[nodiscard]] static auto postMultipart(
        std::string_view url, const std::vector<FormField>& fields,
        const RequestConfig& config = {}) -> expected<HttpResponse, HttpError>;

    /**
     * @brief Sets the maximum download speed.
     * @param bytesPerSecond The maximum download speed in bytes per second.
     */
    void setMaxDownloadSpeed(long bytesPerSecond);

    /**
     * @brief Sets the maximum upload speed.
     * @param bytesPerSecond The maximum upload speed in bytes per second.
     */
    void setMaxUploadSpeed(long bytesPerSecond);

    /**
     * @brief Apply request configuration.
     * @param config The configuration to apply.
     */
    void applyConfig(const RequestConfig& config);

    /**
     * @brief Get the last HTTP response code.
     * @return The HTTP status code.
     */
    [[nodiscard]] auto getResponseCode() const -> int;

    /**
     * @brief Get the effective URL after redirects.
     * @return The effective URL.
     */
    [[nodiscard]] auto getEffectiveUrl() const -> std::string;

    /**
     * @brief Get the content type of the response.
     * @return The content type.
     */
    [[nodiscard]] auto getContentType() const -> std::string;

    /**
     * @brief Get the total download size.
     * @return The download size in bytes.
     */
    [[nodiscard]] auto getDownloadSize() const -> size_t;

    /**
     * @brief Reset the wrapper for reuse.
     */
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace atom::web

#endif  // ATOM_WEB_HTTP_CURL_HPP
