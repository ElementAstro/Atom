#ifndef ATOM_EXTRA_BEAST_HTTP_HPP
#define ATOM_EXTRA_BEAST_HTTP_HPP

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <concepts>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>
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

// Concepts for handlers
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

class HttpClient : public std::enable_shared_from_this<HttpClient> {
public:
    /**
     * @brief Constructs an HttpClient with the given I/O context.
     * @param ioc The I/O context to use for asynchronous operations.
     * @throws std::bad_alloc If memory allocation fails
     */
    explicit HttpClient(net::io_context& ioc);

    // Prevent copy construction and assignment
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Allow move construction and assignment
    HttpClient(HttpClient&&) noexcept = default;
    // 删除隐式定义的移动赋值运算符，解决了TCP流不可移动的问题
    // HttpClient& operator=(HttpClient&&) noexcept = default;

    ~HttpClient() noexcept = default;

    /**
     * @brief Sets a default header for all requests.
     * @param key The header key.
     * @param value The header value.
     * @throws std::invalid_argument If key is empty
     */
    void setDefaultHeader(std::string_view key, std::string_view value);

    /**
     * @brief Sets the timeout duration for the HTTP operations.
     * @param timeout The timeout duration in seconds.
     * @throws std::invalid_argument If timeout is zero or negative
     */
    void setTimeout(std::chrono::seconds timeout);

    /**
     * @brief Sends a synchronous HTTP request.
     * @tparam Body The type of the request body.
     * @param method The HTTP method (verb).
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param version The HTTP version (default is 11).
     * @param content_type The content type of the request body.
     * @param body The request body.
     * @param headers Additional headers to include in the request.
     * @return The HTTP response.
     * @throws std::invalid_argument If host or port is empty
     * @throws beast::system_error On connection or request failure
     */
    template <class Body = http::string_body>
    auto request(http::verb method, std::string_view host,
                 std::string_view port, std::string_view target,
                 int version = 11, std::string_view content_type = "",
                 std::string_view body = "",
                 const std::unordered_map<std::string, std::string>& headers =
                     {}) -> http::response<Body>;

    /**
     * @brief Sends an asynchronous HTTP request.
     * @tparam Body The type of the request body.
     * @tparam ResponseHandler The type of the handler to call when the
     * operation completes.
     * @param method The HTTP method (verb).
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param handler The handler to call when the operation completes.
     * @param version The HTTP version (default is 11).
     * @param content_type The content type of the request body.
     * @param body The request body.
     * @param headers Additional headers to include in the request.
     * @throws std::invalid_argument If host or port is empty
     */
    template <class Body = http::string_body,
              HttpResponseHandler ResponseHandler>
    void asyncRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, ResponseHandler&& handler, int version = 11,
        std::string_view content_type = "", std::string_view body = "",
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Sends a synchronous HTTP request with a JSON body and returns a
     * JSON response.
     * @param method The HTTP method (verb).
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param json_body The JSON body of the request.
     * @param headers Additional headers to include in the request.
     * @return The JSON response.
     * @throws std::invalid_argument If host or port is empty
     * @throws beast::system_error On connection or request failure
     * @throws json::exception If JSON parsing fails
     */
    [[nodiscard]] auto jsonRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, const json& json_body = {},
        const std::unordered_map<std::string, std::string>& headers = {})
        -> json;

    /**
     * @brief Sends an asynchronous HTTP request with a JSON body and returns a
     * JSON response.
     * @tparam ResponseHandler The type of the handler to call when the
     * operation completes.
     * @param method The HTTP method (verb).
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param handler The handler to call when the operation completes.
     * @param json_body The JSON body of the request.
     * @param headers Additional headers to include in the request.
     * @throws std::invalid_argument If host or port is empty
     */
    template <JsonResponseHandler ResponseHandler>
    void asyncJsonRequest(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, ResponseHandler&& handler,
        const json& json_body = {},
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Uploads a file to the server.
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param filepath The path to the file to upload.
     * @param field_name The field name for the file (default is "file").
     * @return The HTTP response.
     * @throws std::invalid_argument If host, port, or filepath is empty
     * @throws beast::system_error On connection or request failure
     */
    auto uploadFile(std::string_view host, std::string_view port,
                    std::string_view target, std::string_view filepath,
                    std::string_view field_name = "file")
        -> http::response<http::string_body>;

    /**
     * @brief Downloads a file from the server.
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param filepath The path to save the downloaded file.
     * @throws std::invalid_argument If host, port, or filepath is empty
     * @throws beast::system_error On connection or request failure
     */
    void downloadFile(std::string_view host, std::string_view port,
                      std::string_view target, std::string_view filepath);

    /**
     * @brief Sends a synchronous HTTP request with retry logic.
     * @tparam Body The type of the request body.
     * @param method The HTTP method (verb).
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param retry_count The number of retry attempts (default is 3).
     * @param version The HTTP version (default is 11).
     * @param content_type The content type of the request body.
     * @param body The request body.
     * @param headers Additional headers to include in the request.
     * @return The HTTP response.
     * @throws std::invalid_argument If host or port is empty
     * @throws beast::system_error On connection or request failure
     */
    template <class Body = http::string_body>
    auto requestWithRetry(
        http::verb method, std::string_view host, std::string_view port,
        std::string_view target, int retry_count = 3, int version = 11,
        std::string_view content_type = "", std::string_view body = "",
        const std::unordered_map<std::string, std::string>& headers = {})
        -> http::response<Body>;

    /**
     * @brief Sends multiple synchronous HTTP requests in a batch.
     * @tparam Body The type of the request body.
     * @param requests A vector of tuples containing the HTTP method, host,
     * port, and target for each request.
     * @param headers Additional headers to include in each request.
     * @return A vector of HTTP responses.
     * @throws std::invalid_argument If any host or port is empty
     */
    template <class Body = http::string_body>
    std::vector<http::response<Body>> batchRequest(
        const std::vector<std::tuple<http::verb, std::string, std::string,
                                     std::string>>& requests,
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Sends multiple asynchronous HTTP requests in a batch.
     * @tparam ResponseHandler The type of the handler to call when the
     * operation completes.
     * @param requests A vector of tuples containing the HTTP method, host,
     * port, and target for each request.
     * @param handler The handler to call when the operation completes.
     * @param headers Additional headers to include in each request.
     * @throws std::invalid_argument If any host or port is empty
     */
    template <BatchResponseHandler ResponseHandler>
    void asyncBatchRequest(
        const std::vector<std::tuple<http::verb, std::string, std::string,
                                     std::string>>& requests,
        ResponseHandler&& handler,
        const std::unordered_map<std::string, std::string>& headers = {});

    /**
     * @brief Runs the I/O context with a thread pool.
     * @param num_threads The number of threads in the pool.
     */
    void runWithThreadPool(size_t num_threads);

    /**
     * @brief Asynchronously downloads a file from the server.
     * @tparam ResponseHandler The type of the handler to call when the
     * operation completes.
     * @param host The server host.
     * @param port The server port.
     * @param target The target URI.
     * @param filepath The path to save the downloaded file.
     * @param handler The handler to call when the operation completes.
     * @throws std::invalid_argument If host, port, or filepath is empty
     */
    template <FileCompletionHandler ResponseHandler>
    void asyncDownloadFile(std::string_view host, std::string_view port,
                           std::string_view target, std::string_view filepath,
                           ResponseHandler&& handler);

private:
    tcp::resolver resolver_;    ///< The resolver for DNS lookups.
    beast::tcp_stream stream_;  ///< The TCP stream for HTTP communication.
    std::unordered_map<std::string, std::string>
        default_headers_;  ///< Default headers for all requests.
    std::chrono::seconds timeout_{
        30};  ///< The timeout duration for HTTP operations.
};

template <class Body>
auto HttpClient::request(http::verb method, std::string_view host,
                         std::string_view port, std::string_view target,
                         int version, std::string_view content_type,
                         std::string_view body,
                         const std::unordered_map<std::string, std::string>&
                             headers) -> http::response<Body> {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    // 修复：正确初始化 HTTP 请求
    http::request<http::string_body> req;
    req.method(method);
    req.target(std::string(target));
    req.version(version);
    req.set(http::field::host, std::string(host));
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for (const auto& [key, value] : default_headers_) {
        req.set(key, value);
    }

    for (const auto& [key, value] : headers) {
        req.set(key, value);
    }

    if (!content_type.empty()) {
        req.set(http::field::content_type, std::string(content_type));
    }

    if (!body.empty()) {
        req.body() = std::string(body);
        req.prepare_payload();
    }

    auto const results =
        resolver_.resolve(std::string(host), std::string(port));
    stream_.connect(results);

    stream_.expires_after(timeout_);

    http::write(stream_, req);

    beast::flat_buffer buffer;
    http::response<Body> res;
    http::read(stream_, buffer, res);

    beast::error_code ec;
    // 修复：处理返回值
    auto result = stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    (void)result;  // 显式忽略返回值

    return res;
}

template <class Body, HttpResponseHandler ResponseHandler>
void HttpClient::asyncRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, ResponseHandler&& handler, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers) {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    auto req = std::make_shared<http::request<http::string_body>>();
    req->method(method);
    req->target(std::string(target));
    req->version(version);
    req->set(http::field::host, std::string(host));
    req->set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for (const auto& [key, value] : default_headers_) {
        req->set(key, value);
    }

    for (const auto& [key, value] : headers) {
        req->set(key, value);
    }

    if (!content_type.empty()) {
        req->set(http::field::content_type, std::string(content_type));
    }

    if (!body.empty()) {
        req->body() = std::string(body);
        req->prepare_payload();
    }

    resolver_.async_resolve(
        std::string(host), std::string(port),
        [this, req, handler = std::forward<ResponseHandler>(handler)](
            beast::error_code ec, tcp::resolver::results_type results) {
            if (ec) {
                return handler(ec, {});
            }

            stream_.async_connect(
                results, [this, req, handler = std::move(handler)](
                             beast::error_code ec,
                             tcp::resolver::results_type::endpoint_type) {
                    if (ec) {
                        return handler(ec, {});
                    }

                    stream_.expires_after(timeout_);

                    http::async_write(
                        stream_, *req,
                        [this, req, handler = std::move(handler)](
                            beast::error_code ec, std::size_t) {
                            if (ec) {
                                return handler(ec, {});
                            }

                            auto res = std::make_shared<http::response<Body>>();
                            auto buffer =
                                std::make_shared<beast::flat_buffer>();

                            http::async_read(
                                stream_, *buffer, *res,
                                [this, res, buffer,
                                 handler = std::move(handler)](
                                    beast::error_code ec, std::size_t) {
                                    beast::error_code shutdown_ec;
                                    auto result = stream_.socket().shutdown(
                                        tcp::socket::shutdown_both,
                                        shutdown_ec);
                                    (void)result;  // 显式忽略返回值
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
    asyncRequest<http::string_body>(
        method, host, port, target,
        [handler = std::forward<ResponseHandler>(handler)](
            beast::error_code ec, http::response<http::string_body> res) {
            if (ec) {
                handler(ec, {});
            } else {
                try {
                    auto jv = json::parse(res.body());
                    handler({}, std::move(jv));
                } catch (const json::parse_error& e) {
                    handler(beast::error_code{e.id, beast::generic_category()},
                            {});
                }
            }
        },
        11, "application/json", json_body.empty() ? "" : json_body.dump(),
        headers);
}

template <class Body>
auto HttpClient::requestWithRetry(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int retry_count, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<Body> {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    beast::error_code ec;
    http::response<Body> response;
    for (int attempt = 0; attempt < retry_count; ++attempt) {
        try {
            response = request<Body>(method, host, port, target, version,
                                     content_type, body, headers);
            // If no exception was thrown, return the response
            return response;
        } catch (const beast::system_error& e) {
            ec = e.code();
            std::cerr << "Request attempt " << (attempt + 1)
                      << " failed: " << ec.message() << std::endl;
            if (attempt + 1 == retry_count) {
                throw;  // Throw the exception if this was the last retry
            }
        }
    }
    return response;
}

template <class Body>
std::vector<http::response<Body>> HttpClient::batchRequest(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers) {
    std::vector<http::response<Body>> responses;
    for (const auto& [method, host, port, target] : requests) {
        if (host.empty() || port.empty()) {
            throw std::invalid_argument("Host and port must not be empty");
        }

        try {
            responses.push_back(
                request<Body>(method, host, port, target, 11, "", "", headers));
        } catch (const std::exception& e) {
            std::cerr << "Batch request failed for " << target << ": "
                      << e.what() << std::endl;
            // Push an empty response if an exception occurs (or handle as
            // needed)
            responses.emplace_back();
        }
    }
    return responses;
}

template <BatchResponseHandler ResponseHandler>
void HttpClient::asyncBatchRequest(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    ResponseHandler&& handler,
    const std::unordered_map<std::string, std::string>& headers) {
    auto responses =
        std::make_shared<std::vector<http::response<http::string_body>>>();
    auto remaining = std::make_shared<std::atomic<int>>(requests.size());

    for (const auto& [method, host, port, target] : requests) {
        if (host.empty() || port.empty()) {
            throw std::invalid_argument("Host and port must not be empty");
        }

        asyncRequest<http::string_body>(
            method, host, port, target,
            [handler, responses, remaining](
                beast::error_code ec, http::response<http::string_body> res) {
                if (ec) {
                    std::cerr << "Error during batch request: " << ec.message()
                              << std::endl;
                    responses
                        ->emplace_back();  // Empty response in case of error
                } else {
                    responses->emplace_back(std::move(res));
                }

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
    if (host.empty() || port.empty() || filepath.empty()) {
        throw std::invalid_argument(
            "Host, port, and filepath must not be empty");
    }

    asyncRequest<http::string_body>(
        http::verb::get, host, port, target,
        [filepath, handler = std::forward<ResponseHandler>(handler)](
            beast::error_code ec, http::response<http::string_body> res) {
            if (ec) {
                handler(ec, false);
            } else {
                std::ofstream outFile(std::string(filepath), std::ios::binary);
                if (!outFile) {
                    std::cerr << "Failed to open file for writing: " << filepath
                              << std::endl;
                    handler(beast::error_code{}, false);
                    return;
                }
                outFile << res.body();
                handler({}, true);  // Download successful
            }
        });
}

#endif  // ATOM_EXTRA_BEAST_HTTP_HPP
