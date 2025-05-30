#include "http.hpp"

#include <spdlog/spdlog.h>
#include <boost/asio/thread_pool.hpp>
#include <filesystem>
#include <stdexcept>
#include <thread>

HttpClient::HttpClient(net::io_context& ioc)
    : resolver_(net::make_strand(ioc)), stream_(net::make_strand(ioc)) {
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

    spdlog::info("Sending {} request to {}:{}{}",
                 std::string(http::to_string(method)), std::string(host),
                 std::string(port), std::string(target));

    auto const results =
        resolver_.resolve(std::string(host), std::string(port));
    stream_.connect(results);
    stream_.expires_after(timeout_);

    http::write(stream_, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream_, buffer, res);

    spdlog::info("Received response: {} {}", static_cast<int>(res.result()),
                 res.reason());

    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

    if (ec && ec != beast::errc::not_connected) {
        spdlog::warn("Socket shutdown error: {}", ec.message());
    }

    return res;
}

auto HttpClient::jsonRequest(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, const json& json_body,
    const std::unordered_map<std::string, std::string>& headers) -> json {
    auto request_headers = headers;
    request_headers["Content-Type"] = "application/json";

    try {
        auto res =
            request(method, host, port, target, 11, "application/json",
                    json_body.empty() ? "" : json_body.dump(), request_headers);

        if (res.result() == http::status::ok ||
            res.result() == http::status::created ||
            res.result() == http::status::accepted) {
            try {
                return json::parse(res.body());
            } catch (const json::parse_error& e) {
                spdlog::error("JSON parse error: {}", e.what());
                throw;
            }
        } else {
            spdlog::error("HTTP error: {} {}", static_cast<int>(res.result()),
                          res.reason());
            throw beast::system_error(
                beast::error_code(static_cast<int>(res.result()),
                                  boost::system::generic_category()));
        }
    } catch (const beast::system_error& e) {
        spdlog::error("HTTP request failed: {}", e.what());
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

        std::string file_content;
        file_content.assign(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>());

        std::string boundary =
            "-------------------------" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());

        std::string body = "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + field_name_str +
                "\"; filename=\"" + file_path.filename().string() + "\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body += file_content + "\r\n";
        body += "--" + boundary + "--\r\n";

        std::string content_type = "multipart/form-data; boundary=" + boundary;

        return request(http::verb::post, host, port, target, 11, content_type,
                       body);
    } catch (const std::exception& e) {
        spdlog::error("File upload failed: {}", e.what());
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
        std::filesystem::path file_path = std::string(filepath);
        std::filesystem::path dir_path = file_path.parent_path();

        if (!dir_path.empty() && !std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }

        auto res = request(http::verb::get, host, port, target);

        if (res.result() != http::status::ok) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(res.result()),
                                  boost::system::generic_category()));
        }

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

        spdlog::info("File downloaded successfully to {}", file_path.string());
    } catch (const std::exception& e) {
        spdlog::error("File download failed: {}", e.what());
        throw;
    }
}

void HttpClient::runWithThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    try {
        net::thread_pool pool(num_threads);

        for (size_t i = 0; i < num_threads; ++i) {
            net::post(pool, [i]() {
                try {
                    spdlog::info("Thread {} started in thread pool", i);
                } catch (const std::exception& e) {
                    spdlog::error("Error in thread pool task: {}", e.what());
                }
            });
        }

        pool.join();
    } catch (const std::exception& e) {
        spdlog::error("Thread pool error: {}", e.what());
        throw;
    }
}

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

    for (int attempt = 0; attempt < retry_count; ++attempt) {
        try {
            spdlog::info("Request attempt {} of {}", attempt + 1, retry_count);
            return request<http::string_body>(method, host, port, target,
                                              version, content_type, body,
                                              headers);
        } catch (const beast::system_error& e) {
            spdlog::error("Request attempt {} failed: {}", attempt + 1,
                          e.what());

            if (attempt + 1 == retry_count) {
                spdlog::error("All retry attempts failed, throwing exception");
                throw;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 * (1 << attempt)));
        }
    }

    throw std::runtime_error("All retry attempts failed");
}

template <>
std::vector<http::response<http::string_body>>
HttpClient::batchRequest<http::string_body>(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers) {
    std::vector<http::response<http::string_body>> responses;
    responses.reserve(requests.size());

    for (const auto& [method, host, port, target] : requests) {
        if (host.empty() || port.empty()) {
            throw std::invalid_argument("Host and port must not be empty");
        }

        try {
            spdlog::info("Executing batch request to {}:{}{}", host, port,
                         target);
            responses.push_back(request<http::string_body>(
                method, host, port, target, 11, "", "", headers));
        } catch (const std::exception& e) {
            spdlog::error("Batch request failed for {}: {}", target, e.what());
            responses.emplace_back();
        }
    }

    return responses;
}
