#include "ws.hpp"
#include <stdexcept>

WSClient::WSClient(net::io_context& ioc)
    : resolver_(std::make_shared<tcp::resolver>(net::make_strand(ioc))),
      ws_(std::make_shared<websocket::stream<tcp::socket>>(
          net::make_strand(ioc))),
      ping_timer_(std::make_shared<net::steady_timer>(ioc.get_executor())) {
    if (!resolver_ || !ws_ || !ping_timer_) {
        throw std::bad_alloc();
    }
}

WSClient::~WSClient() noexcept {
    try {
        if (is_connected_ && ws_ && ws_->is_open()) {
            beast::error_code ec;
            ws_->close(websocket::close_code::normal, ec);
        }
        if (ping_timer_) {
            try {
                ping_timer_->cancel();
            } catch (const beast::system_error& e) {
                spdlog::debug(
                    "Ignoring exception from ping_timer.cancel() in "
                    "destructor: {}",
                    e.what());
            } catch (...) {
                spdlog::debug(
                    "Ignoring unknown exception from ping_timer.cancel() in "
                    "destructor");
            }
        }
    } catch (...) {
        spdlog::error("Exception in WSClient destructor");
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
                                        std::string_view port) {
    if (host.empty()) {
        throw std::invalid_argument("Host cannot be empty");
    }
    if (port.empty()) {
        throw std::invalid_argument("Port cannot be empty");
    }

    bool is_numeric_port = std::all_of(port.begin(), port.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });

    if (!is_numeric_port) {
        bool is_valid_service =
            std::all_of(port.begin(), port.end(), [](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '-';
            });
        if (!is_valid_service) {
            throw std::invalid_argument(
                "Port must be numeric or a valid service name");
        }
    }
}

void WSClient::connect(std::string_view host, std::string_view port) {
    validateConnectionParams(host, port);

    beast::error_code ec;
    last_host_ = host;
    last_port_ = port;
    retry_count_ = 0;

    if (ping_timer_) {
        ws_ = std::make_shared<websocket::stream<tcp::socket>>(
            net::make_strand(ping_timer_->get_executor()));
    } else {
        spdlog::warn(
            "Ping timer not available for strand creation in connect, using "
            "ws_->get_executor()");
        ws_ = std::make_shared<websocket::stream<tcp::socket>>(
            net::make_strand(ws_->get_executor()));
    }

    auto const results =
        resolver_->resolve(std::string(host), std::string(port), ec);
    if (ec) {
        spdlog::error("Failed to resolve host '{}': {}", host, ec.message());
        throw beast::system_error{ec};
    }

    auto ep = net::connect(beast::get_lowest_layer(*ws_), results, ec);
    if (ec) {
        spdlog::error("Failed to connect to '{}:{}': {}", host, port,
                      ec.message());
        throw beast::system_error{ec};
    }
    (void)ep;

    auto result = beast::get_lowest_layer(*ws_).set_option(
        tcp::socket::keep_alive(true), ec);
    if (ec) {
        spdlog::warn("Failed to set TCP keep-alive on socket: {} (result: {})",
                     ec.message(), result.message());
    }

    ws_->set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_->handshake(std::string(host), "/", ec);

    if (ec) {
        spdlog::error("WebSocket handshake failed for '{}:{}': {}", host, port,
                      ec.message());
        if (ws_->is_open()) {
            beast::error_code close_ec;
            auto result = beast::get_lowest_layer(*ws_).close(close_ec);
            if (close_ec) {
                spdlog::debug(
                    "Error closing socket after handshake failure: {} (result: "
                    "{})",
                    close_ec.message(), result.message());
            }
        }
        throw beast::system_error{ec};
    }

    is_connected_ = true;
    startPing();
    spdlog::info("Successfully connected to WebSocket server {}:{}", host,
                 port);
}

void WSClient::send(std::string_view message) {
    if (!is_connected_) {
        throw std::logic_error("Cannot send message: not connected");
    }

    beast::error_code ec;
    ws_->write(net::buffer(message), ec);

    if (ec) {
        is_connected_ = false;
        spdlog::error("Failed to send message: {}", ec.message());
        throw beast::system_error{ec};
    }
}

std::string WSClient::receive() {
    if (!is_connected_) {
        throw std::logic_error("Cannot receive message: not connected");
    }

    beast::flat_buffer buffer;
    beast::error_code ec;
    ws_->read(buffer, ec);

    if (ec) {
        is_connected_ = false;
        spdlog::error("Failed to receive message: {}", ec.message());
        if (ec == websocket::error::closed) {
            spdlog::info("WebSocket connection closed by peer.");
        }
        throw beast::system_error{ec};
    }

    return beast::buffers_to_string(buffer.data());
}

bool WSClient::isConnected() const noexcept { return is_connected_; }

void WSClient::close() {
    if (!is_connected_ && !(ws_ && ws_->is_open())) {
        spdlog::debug("Close called but not connected or stream not open.");
        return;
    }

    if (ping_timer_) {
        try {
            ping_timer_->cancel();
        } catch (const beast::system_error& e) {
            spdlog::warn("Error cancelling ping timer during close: {}",
                         e.code().message());
        } catch (...) {
            spdlog::warn("Unknown error cancelling ping timer during close");
        }
    }

    beast::error_code ec;
    if (ws_ && ws_->is_open()) {
        ws_->close(websocket::close_code::normal, ec);
    } else if (is_connected_) {
        spdlog::warn("Close called, was connected but stream is not open.");
    }

    is_connected_ = false;

    if (ec) {
        if (ec != net::error::operation_aborted &&
            ec != websocket::error::closed) {
            spdlog::error("Error during WebSocket close: {}", ec.message());
        } else {
            spdlog::debug("WebSocket close operation reported: {}",
                          ec.message());
        }
    } else {
        spdlog::info("WebSocket connection closed successfully.");
    }
}

void WSClient::startPing() {
    if (!is_connected_ || ping_interval_.count() <= 0 || !ws_ ||
        !ws_->is_open()) {
        return;
    }

    ping_timer_->expires_after(ping_interval_);
    ping_timer_->async_wait(net::bind_executor(
        ws_->get_executor(),
        [this, self = shared_from_this()](beast::error_code ec) {
            if (ec == net::error::operation_aborted) {
                return;
            }
            if (ec) {
                spdlog::warn("Ping timer error: {}", ec.message());
                return;
            }

            if (!is_connected_ || !ws_ || !ws_->is_open()) {
                return;
            }

            ws_->async_ping(
                {},
                net::bind_executor(
                    ws_->get_executor(),
                    [this, self = shared_from_this()](beast::error_code ec) {
                        if (ec == net::error::operation_aborted) {
                            return;
                        }
                        if (ec) {
                            spdlog::warn(
                                "Ping failed: {}. Connection might be lost.",
                                ec.message());
                            return;
                        }

                        if (is_connected_) {
                            startPing();
                        }
                    }));
        }));
}
