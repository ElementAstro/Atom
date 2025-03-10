#include "response.hpp"

namespace atom::extra::curl {
Response::Response() : status_code_(0) {}

Response::Response(int status_code, std::vector<char> body,
                   std::map<std::string, std::string> headers)
    : status_code_(status_code),
      body_(std::move(body)),
      headers_(std::move(headers)) {}

int Response::status_code() const noexcept { return status_code_; }

const std::vector<char>& Response::body() const noexcept { return body_; }

std::string Response::body_string() const {
    return {body_.data(), body_.size()};
}

std::string Response::json() const { return body_string(); }

const std::map<std::string, std::string>& Response::headers() const noexcept {
    return headers_;
}

bool Response::ok() const noexcept {
    return status_code_ >= 200 && status_code_ < 300;
}

bool Response::redirect() const noexcept {
    return status_code_ >= 300 && status_code_ < 400;
}

bool Response::client_error() const noexcept {
    return status_code_ >= 400 && status_code_ < 500;
}

bool Response::server_error() const noexcept {
    return status_code_ >= 500 && status_code_ < 600;
}

bool Response::has_header(const std::string& name) const {
    return headers_.find(name) != headers_.end();
}

std::string Response::get_header(const std::string& name) const {
    auto it = headers_.find(name);
    return it != headers_.end() ? it->second : "";
}

std::optional<std::string> Response::content_type() const {
    auto it = headers_.find("Content-Type");
    return it != headers_.end() ? std::optional<std::string>(it->second)
                                : std::nullopt;
}

std::optional<size_t> Response::content_length() const {
    auto it = headers_.find("Content-Length");
    if (it != headers_.end()) {
        try {
            return std::stoul(it->second);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}
}  // namespace atom::extra::curl
