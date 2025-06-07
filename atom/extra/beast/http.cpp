// http.cpp
#include "http.hpp"
#include <spdlog/spdlog.h>

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

    http::request<http::string_body> req;
    setupRequest(req, method, host, target, version, content_type, body,
                 headers);

    spdlog::debug("Sending {} request to {}:{}{}",
                  std::string(http::to_string(method)), host, port, target);

    auto const results =
        resolver_.resolve(std::string(host), std::string(port));
    stream_.connect(results);
    stream_.expires_after(timeout_);

    http::write(stream_, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream_, buffer, res);

    spdlog::debug("Received response: {} {}", static_cast<int>(res.result()),
                  res.reason());

    gracefulClose();
    return res;
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
    if (auto parent = file_path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
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
            return request(method, host, port, target, version, content_type,
                           body, headers);
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
