#include "ws.hpp"

#include <algorithm>
#include <stdexcept>

// 使用正确的预处理指令检查头文件
#if __has_include(<atom/log/loguru.hpp>)
#include <atom/log/loguru.hpp>
#else
#include <loguru.hpp>
#endif

WSClient::WSClient(net::io_context& ioc)
    : resolver_(std::make_shared<tcp::resolver>(net::make_strand(ioc))),
      ws_(std::make_shared<websocket::stream<tcp::socket>>(
          net::make_strand(ioc))),
      ping_timer_(std::make_shared<net::steady_timer>(ioc)) {
    if (!resolver_ || !ws_ || !ping_timer_) {
        throw std::bad_alloc();
    }
}

WSClient::~WSClient() noexcept {
    try {
        if (is_connected_) {
            beast::error_code ec;
            ws_->close(websocket::close_code::normal, ec);
            // Ignore error since we're in destructor
        }
    } catch (...) {
        // Prevent exceptions from escaping destructor
    }
}

void WSClient::setTimeout(std::chrono::seconds timeout) noexcept {
    timeout_ = timeout;
}

void WSClient::setReconnectOptions(int retries, std::chrono::seconds interval) {
    if (retries < 0) {
        throw std::invalid_argument("Retry count cannot be negative");
    }
    if (interval.count() <= 0) {
        throw std::invalid_argument("Reconnect interval must be positive");
    }

    max_retries_ = retries;
    reconnect_interval_ = interval;
}

void WSClient::setPingInterval(std::chrono::seconds interval) {
    if (interval.count() <= 0) {
        throw std::invalid_argument("Ping interval must be positive");
    }
    ping_interval_ = interval;
}

void WSClient::validateConnectionParams(std::string_view host,
                                        std::string_view port) const {
    if (host.empty()) {
        throw std::invalid_argument("Host cannot be empty");
    }

    if (port.empty()) {
        throw std::invalid_argument("Port cannot be empty");
    }

    // Check if port is numeric
    if (!std::all_of(port.begin(), port.end(),
                     [](char c) { return std::isdigit(c); })) {
        // Port can also be a service name like "http"
        if (!std::all_of(port.begin(), port.end(), [](char c) {
                return std::isalpha(c) || std::isdigit(c);
            })) {
            throw std::invalid_argument(
                "Port must be numeric or a valid service name");
        }
    }
}

void WSClient::connect(std::string_view host, std::string_view port) {
    validateConnectionParams(host, port);

    beast::error_code ec;

    // Store for potential reconnection
    last_host_ = host;
    last_port_ = port;

    try {
        auto const results =
            resolver_->resolve(std::string(host), std::string(port), ec);
        if (ec) {
            LOG_F(ERROR, "Failed to resolve host: {}", ec.message());
            throw beast::system_error{ec};
        }

        // 修复：保存connect的返回值
        auto connect_result =
            beast::get_lowest_layer(*ws_).connect(results->endpoint(), ec);
        if (ec) {
            LOG_F(ERROR, "Failed to connect: {}", ec.message());
            throw beast::system_error{ec};
        }
        (void)connect_result;  // 显式忽略返回值

        // 修复：保存set_option的返回值
        auto set_option_result = beast::get_lowest_layer(*ws_).set_option(
            tcp::socket::keep_alive(true), ec);
        if (ec) {
            LOG_F(ERROR, "Failed to set socket options: {}", ec.message());
            throw beast::system_error{ec};
        }
        (void)set_option_result;  // 显式忽略返回值

        ws_->handshake(std::string(host), "/", ec);
        if (ec) {
            LOG_F(ERROR, "Failed during handshake: {}", ec.message());
            throw beast::system_error{ec};
        }

        is_connected_ = true;
        startPing();
    } catch (const beast::system_error& e) {
        // Close socket if open
        if (ws_->is_open()) {
            beast::error_code close_ec;
            // 修复：保存close的返回值
            auto close_result = ws_->next_layer().close(close_ec);
            (void)close_result;  // 显式忽略返回值
        }
        throw;  // Rethrow the exception
    }
}

void WSClient::send(std::string_view message) {
    if (!is_connected_) {
        throw std::logic_error("Cannot send message when not connected");
    }

    beast::error_code ec;
    ws_->write(net::buffer(message), ec);

    if (ec) {
        is_connected_ = false;
        LOG_F(ERROR, "Failed to send message: {}", ec.message());
        throw beast::system_error{ec};
    }
}

std::string WSClient::receive() {
    if (!is_connected_) {
        throw std::logic_error("Cannot receive message when not connected");
    }

    beast::flat_buffer buffer;
    beast::error_code ec;

    ws_->read(buffer, ec);
    if (ec) {
        is_connected_ = false;
        LOG_F(ERROR, "Failed to receive message: {}", ec.message());
        throw beast::system_error{ec};
    }

    return beast::buffers_to_string(buffer.data());
}

bool WSClient::isConnected() const noexcept { return is_connected_; }

void WSClient::close() {
    if (!is_connected_) {
        return;  // Already closed, nothing to do
    }

    beast::error_code ec;
    ws_->close(websocket::close_code::normal, ec);

    if (ec) {
        LOG_F(ERROR, "Failed to close connection: {}", ec.message());
        throw beast::system_error{ec};
    }

    is_connected_ = false;
}

void WSClient::startPing() {
    if (!is_connected_ || ping_interval_.count() <= 0) {
        return;
    }

    ping_timer_->expires_after(ping_interval_);
    ping_timer_->async_wait(
        [this, self = shared_from_this()](beast::error_code ec) {
            if (ec || !is_connected_) {
                // Timer canceled or connection closed
                return;
            }

            // Send ping
            ws_->async_ping(
                {}, [this, self = shared_from_this()](beast::error_code ec) {
                    if (!ec && is_connected_) {
                        // Only restart ping timer if still connected and no
                        // error
                        startPing();
                    } else if (ec) {
                        LOG_F(WARNING, "Ping failed: {}", ec.message());
                        // Connection might be broken, but let the next
                        // read/write discover that
                    }
                });
        });
}

void WSClient::asyncSendJson(
    const json& json_data,  // 修复：重命名变量以避免拼写警告
    std::function<void(beast::error_code, std::size_t)> handler) {
    if (!is_connected_) {
        net::post(
            ws_->get_executor(),
            [handler, ec = beast::error_code(
                          net::error::not_connected,  // 修复：使用正确的错误码
                          beast::generic_category())]() mutable {
                handler(ec, 0);
            });
        return;
    }

    try {
        std::string message = json_data.dump();  // 修复：使用新变量名
        asyncSend(message, std::move(handler));
    } catch (const std::exception& e) {
        LOG_F(ERROR, "JSON serialization error: {}", e.what());
        net::post(
            ws_->get_executor(),
            [handler,
             ec = beast::error_code(
                 net::error::invalid_argument,  // 修复：使用正确的错误码
                 beast::generic_category())]() mutable { handler(ec, 0); });
    }
}
