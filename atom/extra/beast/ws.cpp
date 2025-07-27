#include "ws.hpp"
#include <stdexcept>

WSClient::WSClient(net::io_context& ioc)
    : resolver_(std::make_shared<tcp::resolver>(net::make_strand(ioc))),
      ws_(std::make_shared<websocket::stream<tcp::socket>>(
          net::make_strand(ioc))),
      ping_timer_(std::make_shared<net::steady_timer>(ioc.get_executor())),
      incoming_message_queue_(std::make_unique<atom::beast::concurrency::LockFreeMPMCQueue<std::string>>()),
      outgoing_message_queue_(std::make_unique<atom::beast::concurrency::LockFreeMPMCQueue<std::string>>()),
      performance_monitor_(&atom::beast::monitoring::get_global_performance_monitor()) {

    if (!resolver_ || !ws_ || !ping_timer_) {
        throw std::bad_alloc();
    }

    spdlog::info("WSClient initialized with lock-free message queues and performance monitoring");
}

WSClient::~WSClient() noexcept {
    try {
        if (is_connected_.load(std::memory_order_acquire) && ws_ && ws_->is_open()) {
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

    is_connected_.store(true, std::memory_order_release);

    // Record connection opened
    if (performance_monitor_) {
        performance_monitor_->record_ws_connection_opened();
    }

    startPing();
    spdlog::info("Successfully connected to WebSocket server {}:{}", host, port);
}

void WSClient::send(std::string_view message) {
    if (!is_connected_.load(std::memory_order_acquire)) {
        throw std::logic_error("Cannot send message: not connected");
    }

    beast::error_code ec;
    ws_->write(net::buffer(message), ec);

    if (ec) {
        is_connected_.store(false, std::memory_order_release);
        if (performance_monitor_) {
            performance_monitor_->record_ws_connection_closed();
        }
        spdlog::error("Failed to send message: {}", ec.message());
        throw beast::system_error{ec};
    }

    // Record message sent
    if (performance_monitor_) {
        performance_monitor_->record_ws_message_sent(message.size());
    }

    spdlog::debug("Message sent successfully: {} bytes", message.size());
}

std::string WSClient::receive() {
    if (!is_connected_.load(std::memory_order_acquire)) {
        throw std::logic_error("Cannot receive message: not connected");
    }

    beast::flat_buffer buffer;
    beast::error_code ec;
    auto start_time = std::chrono::steady_clock::now();
    ws_->read(buffer, ec);

    if (ec) {
        is_connected_.store(false, std::memory_order_release);
        if (performance_monitor_) {
            performance_monitor_->record_ws_connection_closed();
        }
        spdlog::error("Failed to receive message: {}", ec.message());
        if (ec == websocket::error::closed) {
            spdlog::info("WebSocket connection closed by peer.");
        }
        throw beast::system_error{ec};
    }

    auto message = beast::buffers_to_string(buffer.data());

    // Record message received
    if (performance_monitor_) {
        performance_monitor_->record_ws_message_received(message.size(), start_time);
    }

    // Try to enqueue message in lock-free queue
    if (incoming_message_queue_ && !incoming_message_queue_->empty()) {
        // Check backpressure
        if (backpressure_enabled_.load(std::memory_order_acquire) &&
            current_queue_size_.load(std::memory_order_acquire) >= backpressure_threshold_.load(std::memory_order_acquire)) {
            spdlog::warn("Incoming message queue backpressure active, dropping message");
        } else {
            incoming_message_queue_->enqueue(std::string(message));
            current_queue_size_.fetch_add(1, std::memory_order_acq_rel);
        }
    }

    return message;
}

bool WSClient::isConnected() const noexcept {
    return is_connected_.load(std::memory_order_acquire);
}

void WSClient::configureMessageQueue(std::size_t max_queue_size, std::size_t backpressure_threshold) {
    max_queue_size_.store(max_queue_size, std::memory_order_release);
    backpressure_threshold_.store(backpressure_threshold, std::memory_order_release);

    spdlog::info("Message queue configured: max_size={}, backpressure_threshold={}",
                max_queue_size, backpressure_threshold);
}

void WSClient::setBackpressureEnabled(bool enabled) noexcept {
    backpressure_enabled_.store(enabled, std::memory_order_release);
    spdlog::info("Backpressure control {}", enabled ? "enabled" : "disabled");
}

WSClient::QueueStatistics WSClient::getQueueStatistics() const noexcept {
    return QueueStatistics{
        incoming_message_queue_ ? incoming_message_queue_->size() : 0,
        outgoing_message_queue_ ? outgoing_message_queue_->size() : 0,
        max_queue_size_.load(std::memory_order_acquire),
        backpressure_enabled_.load(std::memory_order_acquire) &&
            current_queue_size_.load(std::memory_order_acquire) >= backpressure_threshold_.load(std::memory_order_acquire),
        backpressure_threshold_.load(std::memory_order_acquire)
    };
}

bool WSClient::tryReceiveMessage(std::string& message) noexcept {
    if (!incoming_message_queue_) {
        return false;
    }

    if (incoming_message_queue_->try_dequeue(message)) {
        current_queue_size_.fetch_sub(1, std::memory_order_acq_rel);
        return true;
    }

    return false;
}

bool WSClient::trySendMessage(std::string_view message) noexcept {
    if (!outgoing_message_queue_ || !is_connected_.load(std::memory_order_acquire)) {
        return false;
    }

    // Check backpressure
    if (backpressure_enabled_.load(std::memory_order_acquire) &&
        current_queue_size_.load(std::memory_order_acquire) >= backpressure_threshold_.load(std::memory_order_acquire)) {
        return false;
    }

    outgoing_message_queue_->enqueue(std::string(message));
    current_queue_size_.fetch_add(1, std::memory_order_acq_rel);

    // Try to send immediately if possible
    try {
        send(message);

        // Remove from queue since it was sent successfully
        std::string dummy;
        if (outgoing_message_queue_->try_dequeue(dummy)) {
            current_queue_size_.fetch_sub(1, std::memory_order_acq_rel);
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::debug("Failed to send queued message immediately: {}", e.what());
        return true; // Message is still queued for later retry
    }
}

void WSClient::close() {
    bool was_connected = is_connected_.load(std::memory_order_acquire);
    if (!was_connected && !(ws_ && ws_->is_open())) {
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
    } else if (was_connected) {
        spdlog::warn("Close called, was connected but stream is not open.");
    }

    is_connected_.store(false, std::memory_order_release);

    // Record connection closed
    if (performance_monitor_ && was_connected) {
        performance_monitor_->record_ws_connection_closed();
    }

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
    if (!is_connected_.load(std::memory_order_acquire) || ping_interval_.count() <= 0 || !ws_ ||
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

            if (!is_connected_.load(std::memory_order_acquire) || !ws_ || !ws_->is_open()) {
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

                        if (is_connected_.load(std::memory_order_acquire)) {
                            startPing();
                        }
                    }));
        }));
}
