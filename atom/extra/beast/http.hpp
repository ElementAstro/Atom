#ifndef ATOM_EXTRA_BEAST_HTTP_HPP
#define ATOM_EXTRA_BEAST_HTTP_HPP

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

template <typename T>
concept HttpResponseHandler =
    requires(T h, beast::error_code ec, http::response<http::string_body> res) {
        { h(ec, res) } -> std::same_as<void>;
    };

template <typename T>
concept JsonResponseHandler = requires(T h, beast::error_code ec, json j) {
    { h(ec, j) } -> std::same_as<void>;
};

template <typename T>
concept BatchResponseHandler =
    requires(T h, std::vector<http::response<http::string_body>> res) {
        { h(res) } -> std::same_as<void>;
    };

template <typename T>
concept FileCompletionHandler =
    requires(T h, beast::error_code ec, bool success) {
        { h(ec, success) } -> std::same_as<void>;
    };

/**
 * @brief High-performance HTTP client for synchronous and asynchronous HTTP
 * requests
 *
 * This class provides a comprehensive HTTP client implementation using
 * Boost.Beast, supporting both synchronous and asynchronous operations with
 * connection pooling, retry logic, and batch processing capabilities.
 */
class HttpClient : public std::enable_shared_from_this<HttpClient> {
public:
    /**
     * @brief Constructs an HttpClient with optimized I/O context
     * @param ioc The I/O context for asynchronous operations
     * @throws std::bad_alloc If memory allocation fails
     */
    explicit HttpClient(net::io_context& ioc);

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept = default;
    HttpClient& operator=(HttpClient&&) = delete;

    ~HttpClient() noexcept = default;

    /**
     * @brief Sets a default header for all requests
     * @param key The header key
     * @param value The header value
     * @throws std::invalid_argument If key is empty
     */
    void setDefaultHeader(std::string_view key, std::string_view value);

    /**
     * @brief Sets the timeout duration for HTTP operations
     * @param timeout The timeout duration in seconds
     * @throws std::invalid_argument If timeout is zero or negative
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Sends a synchronous HTTP request with optimal performance
     * @param method The HTTP method (GET, POST, etc.)
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param version The HTTP version (default is 11)
     * @param content_type The content type header
     * @param body The request body
     * @param headers Additional headers
     * @return The HTTP response
     * @throws std::invalid_argument If host or port is empty
     * @throws beast::system_error On connection or request failure
     */
    auto request(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, int version = 11,
        std::string_view content_type = "", std::string_view body = "",
        const std::unordered_map<std::string, std::string>& headers = {})
        -> http::response<http::string_body>;

    /**
     * @brief Sends an asynchronous HTTP request with callback
     * @param method The HTTP method
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param handler The completion handler
     * @param version The HTTP version (default is 11)
     * @param content_type The content type header
     * @param body The request body
     * @param headers Additional headers
     * @throws std::invalid_argument If host or port is empty
     */
    template <HttpResponseHandler ResponseHandler>
    void asyncRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, ResponseHandler&& handler, int version = 11,
        std::string_view content_type = "", std::string_view body = "",
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Sends a synchronous JSON request with automatic parsing
     * @param method The HTTP method
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param json_body The JSON request body
     * @param headers Additional headers
     * @return The parsed JSON response
     * @throws std::invalid_argument If host or port is empty
     * @throws beast::system_error On connection failure
     * @throws json::exception If JSON parsing fails
     */
    [[nodiscard]] auto jsonRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, const json& json_body = {},
        const std::unordered_map<std::string, std::string>& headers = {})
        -> json;

    /**
     * @brief Sends an asynchronous JSON request with automatic parsing
     * @param method The HTTP method
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param handler The JSON completion handler
     * @param json_body The JSON request body
     * @param headers Additional headers
     * @throws std::invalid_argument If host or port is empty
     */
    template <JsonResponseHandler ResponseHandler>
    void asyncJsonRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, ResponseHandler&& handler,
        const json& json_body = {},
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Uploads a file using multipart form data
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param filepath The local file path
     * @param field_name The form field name (default is "file")
     * @return The HTTP response
     * @throws std::invalid_argument If parameters are empty
     * @throws beast::system_error On upload failure
     */
    auto uploadFile(std::string_view host, std::string_view port,
                    std::string_view target, std::string_view filepath,
                    std::string_view field_name = "file")
        -> http::response<http::string_body>;

    /**
     * @brief Downloads a file from the server
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param filepath The local save path
     * @throws std::invalid_argument If parameters are empty
     * @throws beast::system_error On download failure
     */
    void downloadFile(std::string_view host, std::string_view port,
                      std::string_view target, std::string_view filepath);

    /**
     * @brief Asynchronously downloads a file from the server
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param filepath The local save path
     * @param handler The completion handler
     * @throws std::invalid_argument If parameters are empty
     */
    template <FileCompletionHandler ResponseHandler>
    void asyncDownloadFile(std::string_view host, std::string_view port,
                           std::string_view target, std::string_view filepath,
                           ResponseHandler&& handler);

    /**
     * @brief Sends a request with exponential backoff retry logic
     * @param method The HTTP method
     * @param host The server hostname
     * @param port The server port
     * @param target The target URI path
     * @param retry_count Maximum retry attempts (default is 3)
     * @param version The HTTP version
     * @param content_type The content type header
     * @param body The request body
     * @param headers Additional headers
     * @return The HTTP response
     * @throws std::invalid_argument If parameters are invalid
     * @throws beast::system_error After all retries fail
     */
    auto requestWithRetry(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, int retry_count = 3, int version = 11,
        std::string_view content_type = "", std::string_view body = "",
        const std::unordered_map<std::string, std::string>& headers = {})
        -> http::response<http::string_body>;

    /**
     * @brief Sends multiple requests in parallel batch
     * @param requests Vector of request tuples (method, host, port, target)
     * @param headers Common headers for all requests
     * @return Vector of HTTP responses
     * @throws std::invalid_argument If any parameters are invalid
     */
    auto batchRequest(
        const std::vector<std::tuple<http::verb, std::string, std::string,
                                     std::string>>& requests,
        const std::unordered_map<std::string, std::string>& headers = {})
        -> std::vector<http::response<http::string_body>>;

    /**
     * @brief Sends multiple asynchronous requests in parallel batch
     * @param requests Vector of request tuples
     * @param handler The batch completion handler
     * @param headers Common headers for all requests
     * @throws std::invalid_argument If any parameters are invalid
     */
    template <BatchResponseHandler ResponseHandler>
    void asyncBatchRequest(
        const std::vector<std::tuple<http::verb, std::string, std::string,
                                     std::string>>& requests,
        ResponseHandler&& handler,
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Runs the I/O context with optimized thread pool
     * @param num_threads The number of worker threads
     * @throws std::invalid_argument If num_threads is zero
     */
    void runWithThreadPool(size_t num_threads);

private:
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    std::unordered_map<std::string, std::string> default_headers_;
    std::chrono::seconds timeout_{30};

    void validateHostPort(std::string_view host, std::string_view port) const;
    void setupRequest(
        http::request<http::string_body>& req, http::verb method,
        std::string_view host, std::string_view target, int version,
        std::string_view content_type, std::string_view body,
        const std::unordered_map<std::string, std::string>& headers) const;
    void gracefulClose();
};

template <HttpResponseHandler ResponseHandler>
void HttpClient::asyncRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, ResponseHandler&& handler, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers) {
    validateHostPort(host, port);

    auto req = std::make_shared<http::request<http::string_body>>();
    setupRequest(*req, method, host, target, version, content_type, body,
                 headers);

    resolver_.async_resolve(
        std::string(host), std::string(port),
        [this, req, handler = std::forward<ResponseHandler>(handler)](
            beast::error_code ec, tcp::resolver::results_type results) mutable {
            if (ec)
                return handler(ec, {});

            stream_.async_connect(
                results,
                [this, req, handler = std::move(handler)](
                    beast::error_code ec,
                    tcp::resolver::results_type::endpoint_type) mutable {
                    if (ec)
                        return handler(ec, {});

                    stream_.expires_after(timeout_);
                    http::async_write(
                        stream_, *req,
                        [this, req, handler = std::move(handler)](
                            beast::error_code ec, std::size_t) mutable {
                            if (ec)
                                return handler(ec, {});

                            auto res = std::make_shared<
                                http::response<http::string_body>>();
                            auto buffer =
                                std::make_shared<beast::flat_buffer>();

                            http::async_read(
                                stream_, *buffer, *res,
                                [this, res, buffer,
                                 handler = std::move(handler)](
                                    beast::error_code ec, std::size_t) mutable {
                                    gracefulClose();
                                    handler(ec, std::move(*res));
                                });
                        });
                });
        });
}

template <JsonResponseHandler ResponseHandler>
void HttpClient::asyncJsonRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, ResponseHandler&& handler, const json& json_body,
    const std::unordered_map<std::string, std::string>& headers) {
    asyncRequest(
        method, host, port, target,
        [handler = std::forward<ResponseHandler>(handler)](
            beast::error_code ec,
            http::response<http::string_body> res) mutable {
            if (ec) {
                handler(ec, {});
            } else {
                try {
                    auto parsed_json = json::parse(res.body());
                    handler({}, std::move(parsed_json));
                } catch (const json::parse_error& e) {
                    handler(beast::error_code{e.id, beast::generic_category()},
                            {});
                }
            }
        },
        11, "application/json", json_body.empty() ? "" : json_body.dump(),
        headers);
}

template <BatchResponseHandler ResponseHandler>
void HttpClient::asyncBatchRequest(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    ResponseHandler&& handler,
    const std::unordered_map<std::string, std::string>& headers) {
    auto responses =
        std::make_shared<std::vector<http::response<http::string_body>>>();
    auto remaining = std::make_shared<std::atomic<size_t>>(requests.size());

    responses->reserve(requests.size());
    responses->resize(requests.size());

    for (size_t i = 0; i < requests.size(); ++i) {
        const auto& [method, host, port, target] = requests[i];
        validateHostPort(host, port);

        asyncRequest(
            method, host, port, target,
            [handler, responses, remaining, i](
                beast::error_code ec, http::response<http::string_body> res) {
                (*responses)[i] =
                    ec ? http::response<http::string_body>{} : std::move(res);

                if (--(*remaining) == 0) {
                    handler(*responses);
                }
            },
            11, "", "", headers);
    }
}

template <FileCompletionHandler ResponseHandler>
void HttpClient::asyncDownloadFile(std::string_view host, std::string_view port,
                                   std::string_view target,
                                   std::string_view filepath,
                                   ResponseHandler&& handler) {
    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    asyncRequest(http::verb::get, host, port, target,
                 [filepath = std::string(filepath),
                  handler = std::forward<ResponseHandler>(handler)](
                     beast::error_code ec,
                     http::response<http::string_body> res) mutable {
                     if (ec) {
                         handler(ec, false);
                         return;
                     }

                     try {
                         std::filesystem::path file_path(filepath);
                         if (auto parent = file_path.parent_path();
                             !parent.empty()) {
                             std::filesystem::create_directories(parent);
                         }

                         std::ofstream outFile(filepath, std::ios::binary);
                         if (!outFile) {
                             handler(beast::error_code{}, false);
                             return;
                         }

                         outFile << res.body();
                         handler({}, outFile.good());
                     } catch (...) {
                         handler(beast::error_code{}, false);
                     }
                 });
}

#endif
