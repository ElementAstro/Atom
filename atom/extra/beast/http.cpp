#include "http.hpp"

#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <filesystem>
#include <stdexcept>
#include <thread>

#if __has_include(<atom/log/loguru.hpp>)
#include <atom/log/loguru.hpp>
#else
#include <loguru.hpp>
#endif

HttpClient::HttpClient(net::io_context& ioc)
    : resolver_(net::make_strand(ioc)), stream_(net::make_strand(ioc)) {
    // Set default headers
    setDefaultHeader("User-Agent", BOOST_BEAST_VERSION_STRING);
    setDefaultHeader("Accept", "*/*");
}

void HttpClient::setDefaultHeader(std::string_view key,
                                  std::string_view value) {
    if (key.empty()) {
        throw std::invalid_argument("Header key must not be empty");
    }

    default_headers_[std::string(key)] = std::string(value);
}

void HttpClient::setTimeout(std::chrono::seconds timeout) {
    if (timeout <= std::chrono::seconds(0)) {
        throw std::invalid_argument("Timeout must be positive");
    }

    timeout_ = timeout;
}

// 提供 request 方法的显式特化，避免链接错误
template <>
auto HttpClient::request<http::string_body>(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int version, std::string_view content_type,
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    // 初始化 HTTP 请求
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

    LOG_F(INFO, "Sending %s request to %s:%s%s",
          std::string(http::to_string(method)).c_str(),
          std::string(host).c_str(), std::string(port).c_str(),
          std::string(target).c_str());

    auto const results =
        resolver_.resolve(std::string(host), std::string(port));
    stream_.connect(results);

    // 设置超时
    stream_.expires_after(timeout_);

    http::write(stream_, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream_, buffer, res);

    LOG_F(INFO, "Received response: %d %s", static_cast<int>(res.result()),
          res.reason().to_string().c_str());

    // 关闭连接
    beast::error_code ec;
    auto result = stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    (void)result;

    if (ec && ec != beast::errc::not_connected) {
        LOG_F(WARNING, "Socket shutdown error: %s", ec.message().c_str());
    }

    return res;
}

auto HttpClient::jsonRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, const json& json_body,
    const std::unordered_map<std::string, std::string>& headers) -> json {
    // Add content-type header for JSON
    auto request_headers = headers;
    request_headers["Content-Type"] = "application/json";

    try {
        auto res =
            request(method, host, port, target, 11, "application/json",
                    json_body.empty() ? "" : json_body.dump(), request_headers);

        // Check if response has a valid JSON
        if (res.result() == http::status::ok ||
            res.result() == http::status::created ||
            res.result() == http::status::accepted) {
            try {
                return json::parse(res.body());
            } catch (const json::parse_error& e) {
                LOG_F(ERROR, "JSON parse error: %s", e.what());
                throw;
            }
        } else {
            LOG_F(ERROR, "HTTP error: %d %s", static_cast<int>(res.result()),
                  res.reason().data());
            // 修复：使用 boost::system::generic_category() 代替
            // beast::http::error::get_http_category()
            throw beast::system_error(
                beast::error_code(static_cast<int>(res.result()),
                                  boost::system::generic_category()));
        }
    } catch (const beast::system_error& e) {
        LOG_F(ERROR, "HTTP request failed: %s", e.what());
        throw;
    }
}

auto HttpClient::uploadFile(std::string_view host, std::string_view port,
                            std::string_view target, std::string_view filepath,
                            std::string_view field_name)
    -> http::response<http::string_body> {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    std::string field_name_str =
        field_name.empty() ? "file" : std::string(field_name);
    // 修复：正确声明 std::filesystem::path 对象
    std::filesystem::path file_path = std::string(filepath);

    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File does not exist: " + file_path.string());
    }

    try {
        std::ifstream file(file_path.string(), std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file: " +
                                     file_path.string());
        }

        // Read file content using C++20 file mapping if available (fallback to
        // buffer read)
        std::string file_content;
        file_content.assign(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>());

        // Generate a unique boundary
        std::string boundary =
            "-------------------------" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());

        // Build multipart form data
        std::string body = "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + field_name_str +
                "\"; filename=\"" + file_path.filename().string() + "\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body += file_content + "\r\n";
        body += "--" + boundary + "--\r\n";

        // Set content type with boundary
        std::string content_type = "multipart/form-data; boundary=" + boundary;

        // Send request
        return request(http::verb::post, host, port, target, 11, content_type,
                       body);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "File upload failed: %s", e.what());
        throw;
    }
}

void HttpClient::downloadFile(std::string_view host, std::string_view port,
                              std::string_view target,
                              std::string_view filepath) {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    try {
        // Make directory if it doesn't exist
        // 修复：正确声明 std::filesystem::path 对象
        std::filesystem::path file_path = std::string(filepath);
        std::filesystem::path dir_path = file_path.parent_path();

        if (!dir_path.empty() && !std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }

        auto res = request(http::verb::get, host, port, target);

        // Check if we got a successful response
        if (res.result() != http::status::ok) {
            // 修复：使用 boost::system::generic_category() 代替
            // beast::http::error::get_http_category()
            throw beast::system_error(
                beast::error_code(static_cast<int>(res.result()),
                                  boost::system::generic_category()));
        }

        // Write to file
        std::ofstream outFile(file_path.string(), std::ios::binary);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing: " +
                                     file_path.string());
        }

        outFile << res.body();
        outFile.close();

        if (!outFile) {
            throw std::runtime_error("Failed to write to file: " +
                                     file_path.string());
        }

        LOG_F(INFO, "File downloaded successfully to %s",
              file_path.string().c_str());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "File download failed: %s", e.what());
        throw;
    }
}

void HttpClient::runWithThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    try {
        // Create a thread pool
        net::thread_pool pool(num_threads);

        // Example of how to use the thread pool (must be customized by user)
        for (size_t i = 0; i < num_threads; ++i) {
            // 修复：移除未使用的 this 捕获
            net::post(pool, [i]() {
                try {
                    // This is a placeholder - real implementation should use
                    // actual requests
                    LOG_F(INFO, "Thread %zu started in thread pool", i);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Error in thread pool task: %s", e.what());
                }
            });
        }

        // Wait for all tasks to complete
        pool.join();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Thread pool error: %s", e.what());
        throw;
    }
}

// 实现 requestWithRetry 模板的显式特化
template <>
auto HttpClient::requestWithRetry<http::string_body>(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int retry_count, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }

    beast::error_code ec;
    http::response<http::string_body> response;

    for (int attempt = 0; attempt < retry_count; ++attempt) {
        try {
            LOG_F(INFO, "Request attempt %d of %d", attempt + 1, retry_count);
            response =
                request<http::string_body>(method, host, port, target, version,
                                           content_type, body, headers);
            // 请求成功，返回响应
            return response;
        } catch (const beast::system_error& e) {
            ec = e.code();
            LOG_F(ERROR, "Request attempt %d failed: %s", attempt + 1,
                  ec.message().c_str());

            if (attempt + 1 == retry_count) {
                LOG_F(ERROR, "All retry attempts failed, throwing exception");
                throw;  // 如果这是最后一次重试，则抛出异常
            }

            // 在重试前等待一段时间（采用指数退避策略）
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 * (1 << attempt)));
        }
    }

    return response;
}

// 实现 batchRequest 模板的显式特化
template <>
std::vector<http::response<http::string_body>>
HttpClient::batchRequest<http::string_body>(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers) {
    std::vector<http::response<http::string_body>> responses;

    for (const auto& [method, host, port, target] : requests) {
        if (host.empty() || port.empty()) {
            throw std::invalid_argument("Host and port must not be empty");
        }

        try {
            LOG_F(INFO, "Executing batch request to %s:%s%s", host.c_str(),
                  port.c_str(), target.c_str());

            responses.push_back(request<http::string_body>(
                method, host, port, target, 11, "", "", headers));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Batch request failed for %s: %s", target.c_str(),
                  e.what());
            // 发生异常时添加空响应（或根据需要处理）
            responses.emplace_back();
        }
    }

    return responses;
}
