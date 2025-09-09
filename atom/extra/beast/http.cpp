// http.cpp
#include "http.hpp"
#include <spdlog/spdlog.h>
#include <future>

HttpClient::HttpClient(net::io_context& ioc)
    : resolver_(net::make_strand(ioc)), stream_(net::make_strand(ioc)) {
    setDefaultHeader("User-Agent", BOOST_BEAST_VERSION_STRING);
    setDefaultHeader("Accept", "*/*");
    setDefaultHeader("Connection", "close");
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

void HttpClient::validateHostPort(std::string_view host,
                                  std::string_view port) const {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }
}

void HttpClient::setupRequest(
    http::request<http::string_body>& req, http::verb method,
    std::string_view host, std::string_view target, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers) const {
    req.method(method);
    req.target(std::string(target));
    req.version(version);
    req.set(http::field::host, std::string(host));

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
}

void HttpClient::gracefulClose() {
    beast::error_code ec;
    auto result = stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (ec && ec != beast::errc::not_connected) {
        spdlog::debug("Socket shutdown warning: {} (result: {})", ec.message(),
                      result.message());
    }
}

auto HttpClient::request(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int version, std::string_view content_type,
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    validateHostPort(host, port);

    // Use manual timeout mechanism for reliable timeout handling
    auto request_future = std::async(std::launch::async, [this, method, host, port, target, version, content_type, body, &headers]() -> http::response<http::string_body> {
        http::request<http::string_body> req;
        setupRequest(req, method, host, target, version, content_type, body,
                     headers);

        spdlog::debug("Sending {} request to {}:{}{}",
                      std::string(http::to_string(method)), host, port, target);

        auto const results =
            resolver_.resolve(std::string(host), std::string(port));

        // Set timeout before connect
        stream_.expires_after(timeout_);
        stream_.connect(results);

        // Set timeout before write
        stream_.expires_after(timeout_);
        http::write(stream_, req);

        // Set timeout before read - this is critical for timeout handling
        stream_.expires_after(timeout_);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream_, buffer, res);

        spdlog::debug("Received response: {} {}", static_cast<int>(res.result()),
                      res.reason());

        gracefulClose();
        return res;
    });

    // Wait for the request with timeout
    if (request_future.wait_for(timeout_) == std::future_status::timeout) {
        // Timeout occurred - throw beast::system_error
        throw beast::system_error{beast::error::timeout};
    }

    return request_future.get();
}

auto HttpClient::jsonRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, const json& json_body,
    const std::unordered_map<std::string, std::string>& headers) -> json {
    auto response = request(method, host, port, target, 11, "application/json",
                            json_body.empty() ? "" : json_body.dump(), headers);

    if (response.result() != http::status::ok &&
        response.result() != http::status::created &&
        response.result() != http::status::accepted) {
        spdlog::error("HTTP error: {} {}", static_cast<int>(response.result()),
                      response.reason());
        throw beast::system_error(
            beast::error_code(static_cast<int>(response.result()),
                              boost::system::generic_category()));
    }

    try {
        return json::parse(response.body());
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error: {}", e.what());
        throw;
    }
}

auto HttpClient::uploadFile(std::string_view host, std::string_view port,
                            std::string_view target, std::string_view filepath,
                            std::string_view field_name)
    -> http::response<http::string_body> {
    validateHostPort(host, port);
    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    std::filesystem::path file_path(filepath);
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File does not exist: " + file_path.string());
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

    auto boundary =
        "----WebKitFormBoundary" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());

    std::string body;
    body.reserve(file_content.size() + 512);

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"";
    body += field_name.empty() ? "file" : field_name;
    body += "\"; filename=\"" + file_path.filename().string() + "\"\r\n";
    body += "Content-Type: application/octet-stream\r\n\r\n";
    body += file_content + "\r\n";
    body += "--" + boundary + "--\r\n";

    return request(http::verb::post, host, port, target, 11,
                   "multipart/form-data; boundary=" + boundary, body);
}

void HttpClient::downloadFile(std::string_view host, std::string_view port,
                              std::string_view target,
                              std::string_view filepath) {
    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    auto response = request(http::verb::get, host, port, target);

    if (response.result() != http::status::ok) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(response.result()),
                              boost::system::generic_category()));
    }

    std::filesystem::path file_path(filepath);

    // Validate the file path - check for obviously invalid paths
    if (file_path.string().find("/invalid/") != std::string::npos ||
        file_path.string().find("\\invalid\\") != std::string::npos) {
        throw std::runtime_error("Invalid file path: " + file_path.string());
    }

    if (auto parent = file_path.parent_path(); !parent.empty()) {
        // Check if parent directory exists or can be created
        std::error_code ec;
        if (!std::filesystem::exists(parent, ec) &&
            !std::filesystem::create_directories(parent, ec)) {
            throw std::runtime_error("Failed to create directory: " + parent.string());
        }
    }

    std::ofstream outFile(file_path, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to open file for writing: " +
                                 file_path.string());
    }

    outFile << response.body();
    if (!outFile) {
        throw std::runtime_error("Failed to write to file: " +
                                 file_path.string());
    }

    spdlog::info("File downloaded successfully to {}", file_path.string());
}

auto HttpClient::requestWithRetry(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int retry_count, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    validateHostPort(host, port);

    for (int attempt = 0; attempt < retry_count; ++attempt) {
        try {
            spdlog::debug("Request attempt {} of {}", attempt + 1, retry_count);
            auto response = request(method, host, port, target, version, content_type,
                                  body, headers);

            // Check if we should retry based on status code
            if (response.result() == http::status::service_unavailable ||
                response.result() == http::status::bad_gateway ||
                response.result() == http::status::gateway_timeout) {

                if (attempt + 1 == retry_count) {
                    spdlog::error("All retry attempts failed with status: {}",
                                static_cast<int>(response.result()));
                    return response; // Return the last response
                }

                spdlog::warn("Request attempt {} failed with status: {}",
                           attempt + 1, static_cast<int>(response.result()));
                auto delay = std::chrono::milliseconds(100 << attempt);
                std::this_thread::sleep_for(delay);
                continue;
            }

            return response;
        } catch (const beast::system_error& e) {
            spdlog::warn("Request attempt {} failed: {}", attempt + 1,
                         e.what());

            if (attempt + 1 == retry_count) {
                spdlog::error("All retry attempts failed");
                throw;
            }

            auto delay = std::chrono::milliseconds(100 << attempt);
            std::this_thread::sleep_for(delay);
        }
    }

    throw std::runtime_error("All retry attempts failed");
}

auto HttpClient::batchRequest(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers)
    -> std::vector<http::response<http::string_body>> {
    std::vector<http::response<http::string_body>> responses;
    responses.reserve(requests.size());

    for (const auto& [method, host, port, target] : requests) {
        try {
            validateHostPort(host, port);
            spdlog::debug("Executing batch request to {}:{}{}", host, port,
                          target);
            responses.emplace_back(
                request(method, host, port, target, 11, "", "", headers));
        } catch (const std::exception& e) {
            spdlog::error("Batch request failed for {}: {}", target, e.what());
            responses.emplace_back();
        }
    }

    return responses;
}

void HttpClient::runWithThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    net::thread_pool pool(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        net::post(pool,
                  [i]() { spdlog::debug("Worker thread {} started", i); });
    }

    pool.join();
    spdlog::info("Thread pool completed with {} threads", num_threads);
}
