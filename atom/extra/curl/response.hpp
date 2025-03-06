#include <map>
#include <optional>
#include <string>
#include <vector>
class Response {
public:
    Response() : status_code_(0) {}  // Add default constructor

    Response(int status_code, std::vector<char> body,
             std::map<std::string, std::string> headers)
        : status_code_(status_code),
          body_(std::move(body)),
          headers_(std::move(headers)) {}

    int status_code() const noexcept { return status_code_; }

    const std::vector<char>& body() const noexcept { return body_; }

    std::string body_string() const { return {body_.data(), body_.size()}; }

    // 将响应体解析为JSON（需要包含JSON库，这里使用简单字符串替代）
    std::string json() const { return body_string(); }

    const std::map<std::string, std::string>& headers() const noexcept {
        return headers_;
    }

    bool ok() const noexcept {
        return status_code_ >= 200 && status_code_ < 300;
    }

    bool redirect() const noexcept {
        return status_code_ >= 300 && status_code_ < 400;
    }

    bool client_error() const noexcept {
        return status_code_ >= 400 && status_code_ < 500;
    }

    bool server_error() const noexcept {
        return status_code_ >= 500 && status_code_ < 600;
    }

    bool has_header(const std::string& name) const {
        return headers_.find(name) != headers_.end();
    }

    std::string get_header(const std::string& name) const {
        auto it = headers_.find(name);
        return it != headers_.end() ? it->second : "";
    }

    std::optional<std::string> content_type() const {
        auto it = headers_.find("Content-Type");
        return it != headers_.end() ? std::optional<std::string>(it->second)
                                    : std::nullopt;
    }

    std::optional<size_t> content_length() const {
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

private:
    int status_code_;
    std::vector<char> body_;
    std::map<std::string, std::string> headers_;
};