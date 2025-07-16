#include "async_tcpclient.hpp"

#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <shared_mutex>

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <spdlog/spdlog.h>

namespace atom::async::connection {

// Helper for exponential backoff with jitter
class BackoffCalculator {
public:
    BackoffCalculator(std::chrono::milliseconds initial_delay,
                      std::chrono::milliseconds max_delay, double factor = 2.0,
                      double jitter = 0.1)
        : initial_delay_(initial_delay),
          max_delay_(max_delay),
          factor_(factor),
          jitter_(jitter),
          current_delay_(initial_delay),
          attempt_(0),
          random_engine_(std::random_device()()) {}

    std::chrono::milliseconds nextDelay() {
        if (attempt_ > 30) { // Reset after many attempts to avoid potential overflow
            reset();
        }

        if (attempt_ > 0) {
            current_delay_ = std::min(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::duration<double, std::milli>(current_delay_.count() * factor_)),
                max_delay_);
        }

        std::uniform_real_distribution<double> dist(1.0 - jitter_, 1.0 + jitter_);
        double jitter_factor = dist(random_engine_);
        auto jittered_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double, std::milli>(current_delay_.count() * jitter_factor));

        attempt_++;
        return jittered_delay;
    }

    void reset() {
        current_delay_ = initial_delay_;
        attempt_ = 0;
    }

private:
    std::chrono::milliseconds initial_delay_;
    std::chrono::milliseconds max_delay_;
    double factor_;
    double jitter_;
    std::chrono::milliseconds current_delay_;
    int attempt_;
    std::mt19937 random_engine_;
};

class TcpClient::Impl {
public:
    Impl(const ConnectionConfig& config)
        : config_(config),
          io_context_(),
          work_guard_(asio::make_work_guard(io_context_)),
          ssl_context_(asio::ssl::context::sslv23),
          state_(ConnectionState::Disconnected),
          backoff_calculator_(config.reconnect_delay, std::chrono::seconds(30), 1.5, 0.2) {
        if (config_.use_ssl) {
            configureSslContext();
            ssl_socket_ = std::make_unique<ssl_socket_t>(io_context_, ssl_context_);
        } else {
            plain_socket_ = std::make_unique<asio::ip::tcp::socket>(io_context_);
        }

        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                spdlog::error("IO context exception: {}", e.what());
            }
        });
    }

    ~Impl() {
        disconnect();
        try {
            work_guard_.reset();
            io_context_.stop();
            if (io_thread_.joinable()) {
                io_thread_.join();
            }
        } catch (const std::exception& e) {
            spdlog::error("Error during TCP client cleanup: {}", e.what());
        }
    }

    bool connect(const std::string& host, int port, std::optional<std::chrono::milliseconds> timeout) {
        ConnectionState old_state = state_.load(std::memory_order_relaxed);
        while (true) {
            if (old_state == ConnectionState::Connected || old_state == ConnectionState::Connecting) {
                return true;
            }
            if (state_.compare_exchange_weak(old_state, ConnectionState::Connecting)) {
                break;
            }
        }

        last_host_ = host;
        last_port_ = port;

        {
            std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
            if (on_connecting_) on_connecting_();
        }

        stats_.connection_attempts++;
        auto actual_timeout = timeout.value_or(config_.connect_timeout);

        try {
            asio::ip::tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));
            auto connect_promise_ptr = std::make_shared<std::promise<bool>>();
            auto connect_future = connect_promise_ptr->get_future();
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(actual_timeout);

            auto handle_connect = [this, timer, promise_ptr = connect_promise_ptr](
                const asio::error_code& ec, const asio::ip::tcp::endpoint& /*endpoint*/) {
                timer->cancel();
                if (ec) {
                    handleConnectError("Connect error: " + ec.message(), *promise_ptr);
                    return;
                }

                if (config_.use_ssl) {
                    ssl_socket_->async_handshake(asio::ssl::stream_base::client,
                        [this, promise_ptr](const asio::error_code& handshake_ec) {
                            if (handshake_ec) {
                                handleConnectError("SSL handshake error: " + handshake_ec.message(), *promise_ptr);
                                return;
                            }
                            handleSuccessfulConnection(*promise_ptr);
                        });
                } else {
                    handleSuccessfulConnection(*promise_ptr);
                }
            };

            timer->async_wait([this, promise_ptr = connect_promise_ptr](const asio::error_code& ec) {
                if (ec == asio::error::operation_aborted) return;
                if (config_.use_ssl) ssl_socket_->lowest_layer().cancel();
                else plain_socket_->cancel();
                handleConnectError("Connection timed out", *promise_ptr);
            });

            if (config_.use_ssl) {
                asio::async_connect(ssl_socket_->lowest_layer(), endpoints, handle_connect);
            } else {
                asio::async_connect(*plain_socket_, endpoints, handle_connect);
            }

            return connect_future.get();
        } catch (const std::exception& e) {
            auto promise = std::promise<bool>();
            handleConnectError(std::string("Connection exception: ") + e.what(), promise);
            return false;
        }
    }

    std::future<bool> connectAsync(const std::string& host, int port) {
        return std::async(std::launch::async, [this, host, port]() { return connect(host, port, std::nullopt); });
    }

    void disconnect() {
        ConnectionState old_state = state_.exchange(ConnectionState::Disconnected);
        if (old_state == ConnectionState::Disconnected) return;

        try {
            if (config_.use_ssl && ssl_socket_) {
                ssl_socket_->lowest_layer().cancel();
                ssl_socket_->lowest_layer().close();
            } else if (plain_socket_) {
                plain_socket_->cancel();
                plain_socket_->close();
            }
            if (heartbeat_timer_) heartbeat_timer_->cancel();
        } catch (const std::exception& e) {
            spdlog::error("Error during disconnect: {}", e.what());
        }

        backoff_calculator_.reset();
        {
            std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
            if (on_disconnected_) on_disconnected_();
        }
        spdlog::info("Disconnected from server.");
    }

    void configureReconnection(int attempts, std::chrono::milliseconds delay) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_.reconnect_attempts = attempts;
        config_.reconnect_delay = delay;
        backoff_calculator_ = BackoffCalculator(delay, std::chrono::seconds(30), 1.5, 0.2);
    }

    void setHeartbeatInterval(std::chrono::milliseconds interval, const std::vector<char>& data) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_.heartbeat_interval = interval;
        heartbeat_data_ = data.empty() ? std::vector<char>{'P', 'I', 'N', 'G'} : data;
        if (state_ == ConnectionState::Connected && heartbeat_timer_) {
            startHeartbeat();
        }
    }

    bool send(const std::vector<char>& data) {
        if (state_ != ConnectionState::Connected) {
            spdlog::error("Cannot send: not connected");
            return false;
        }
        try {
            size_t bytes_written = config_.use_ssl ? asio::write(*ssl_socket_, asio::buffer(data))
                                                   : asio::write(*plain_socket_, asio::buffer(data));
            stats_.total_bytes_sent += bytes_written;
            stats_.last_activity_time = std::chrono::steady_clock::now();
            spdlog::info("Sent data of size: {}", bytes_written);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Send error: {}", e.what());
            handleError(e.what());
            return false;
        }
    }

    bool sendString(const std::string& data) {
        return send(std::vector<char>(data.begin(), data.end()));
    }

    bool sendWithTimeout(const std::vector<char>& data, std::chrono::milliseconds timeout) {
        if (state_ != ConnectionState::Connected) {
            spdlog::error("Cannot send: not connected");
            return false;
        }
        try {
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(timeout);
            auto send_promise = std::make_shared<std::promise<bool>>();
            auto send_future = send_promise->get_future();

            timer->async_wait([this, send_promise](const asio::error_code& ec) {
                if (ec == asio::error::operation_aborted) return;
                spdlog::error("Send operation timed out");
                send_promise->set_value(false);
                if (config_.use_ssl) ssl_socket_->lowest_layer().cancel();
                else plain_socket_->cancel();
            });

            auto write_callback = [this, timer, send_promise](const asio::error_code& ec, std::size_t bytes_transferred) {
                timer->cancel();
                if (ec) {
                    spdlog::error("Async write error: {}", ec.message());
                    send_promise->set_value(false);
                    handleError(ec.message());
                    return;
                }
                stats_.total_bytes_sent += bytes_transferred;
                stats_.last_activity_time = std::chrono::steady_clock::now();
                send_promise->set_value(true);
                spdlog::info("Sent data of size: {}", bytes_transferred);
            };

            if (config_.use_ssl) {
                asio::async_write(*ssl_socket_, asio::buffer(data), write_callback);
            } else {
                asio::async_write(*plain_socket_, asio::buffer(data), write_callback);
            }
            return send_future.get();
        } catch (const std::exception& e) {
            spdlog::error("Send with timeout error: {}", e.what());
            handleError(e.what());
            return false;
        }
    }

    std::future<std::vector<char>> receive(size_t size, std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(config_.read_timeout);
        return std::async(std::launch::async, [this, size, actual_timeout]() {
            if (state_ != ConnectionState::Connected) {
                spdlog::error("Cannot receive: not connected");
                return std::vector<char>();
            }
            try {
                std::vector<char> data(size);
                auto timer = std::make_shared<asio::steady_timer>(io_context_);
                timer->expires_after(actual_timeout);
                auto receive_promise = std::make_shared<std::promise<std::vector<char>>>();
                auto receive_future = receive_promise->get_future();

                timer->async_wait([this, receive_promise](const asio::error_code& ec) {
                    if (ec == asio::error::operation_aborted) return;
                    spdlog::error("Receive operation timed out");
                    receive_promise->set_value({});
                    if (config_.use_ssl) ssl_socket_->lowest_layer().cancel();
                    else plain_socket_->cancel();
                });

                auto read_callback = [this, data, timer, receive_promise](const asio::error_code& ec, std::size_t len) mutable {
                    timer->cancel();
                    if (ec) {
                        spdlog::error("Async read error: {}", ec.message());
                        receive_promise->set_value({});
                        handleError(ec.message());
                        return;
                    }
                    stats_.total_bytes_received += len;
                    stats_.last_activity_time = std::chrono::steady_clock::now();
                    data.resize(len);
                    receive_promise->set_value(data);
                    spdlog::info("Received data of size: {}", len);
                };

                if (config_.use_ssl) {
                    asio::async_read(*ssl_socket_, asio::buffer(data, size), read_callback);
                } else {
                    asio::async_read(*plain_socket_, asio::buffer(data, size), read_callback);
                }
                return receive_future.get();
            } catch (const std::exception& e) {
                spdlog::error("Receive error: {}", e.what());
                handleError(e.what());
                return std::vector<char>();
            }
        });
    }

    std::future<std::string> receiveUntil(char delimiter, std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(config_.read_timeout);
        return std::async(std::launch::async, [this, delimiter, actual_timeout]() {
            if (state_ != ConnectionState::Connected) {
                spdlog::error("Cannot receive: not connected");
                return std::string();
            }
            try {
                auto timer = std::make_shared<asio::steady_timer>(io_context_);
                timer->expires_after(actual_timeout);
                auto receive_promise = std::make_shared<std::promise<std::string>>();
                auto receive_future = receive_promise->get_future();
                auto buffer = std::make_shared<asio::streambuf>();

                timer->async_wait([this, receive_promise](const asio::error_code& ec) {
                    if (ec == asio::error::operation_aborted) return;
                    spdlog::error("Receive until operation timed out");
                    receive_promise->set_value({});
                    if (config_.use_ssl) ssl_socket_->lowest_layer().cancel();
                    else plain_socket_->cancel();
                });

                auto read_until_callback = [this, buffer, timer, receive_promise](const asio::error_code& ec, std::size_t len) {
                    timer->cancel();
                    if (ec) {
                        spdlog::error("Async read until error: {}", ec.message());
                        receive_promise->set_value({});
                        handleError(ec.message());
                        return;
                    }
                    stats_.total_bytes_received += len;
                    stats_.last_activity_time = std::chrono::steady_clock::now();
                    std::string data(asio::buffers_begin(buffer->data()), asio::buffers_begin(buffer->data()) + len);
                    buffer->consume(len);
                    receive_promise->set_value(data);
                    spdlog::info("Received data until delimiter, size: {}", len);
                };

                if (config_.use_ssl) {
                    asio::async_read_until(*ssl_socket_, *buffer, delimiter, read_until_callback);
                } else {
                    asio::async_read_until(*plain_socket_, *buffer, delimiter, read_until_callback);
                }
                return receive_future.get();
            } catch (const std::exception& e) {
                spdlog::error("Receive until error: {}", e.what());
                handleError(e.what());
                return std::string();
            }
        });
    }

    std::future<std::vector<char>> requestResponse(const std::vector<char>& request, size_t response_size, std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(std::chrono::milliseconds(config_.write_timeout.count() + config_.read_timeout.count()));
        return std::async(std::launch::async, [this, request, response_size, actual_timeout]() {
            if (!send(request)) {
                spdlog::error("Request-response cycle failed at request stage");
                return std::vector<char>();
            }
            return receive(response_size, actual_timeout).get();
        });
    }

    void setProxyConfig(const ProxyConfig& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        proxy_config_ = config;
        if (proxy_config_.enabled) {
            spdlog::info("Proxy configuration set: {}:{}", proxy_config_.host, proxy_config_.port);
        } else {
            spdlog::info("Proxy disabled");
        }
    }

    void configureSslCertificates(const std::string& cert_path, const std::string& key_path, const std::string& ca_path) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_.ssl_certificate_path = cert_path;
        config_.ssl_private_key_path = key_path;
        config_.ca_certificate_path = ca_path;
        if (config_.use_ssl) {
            configureSslContext();
        }
    }

    ConnectionState getConnectionState() const { return state_.load(); }
    bool isConnected() const { return state_ == ConnectionState::Connected; }
    std::string getErrorMessage() const { std::lock_guard<std::mutex> lock(error_mutex_); return last_error_; }

    ConnectionStats getStats() const {
        ConnectionStats stats_copy;
        stats_copy.total_bytes_sent = stats_.total_bytes_sent.load();
        stats_copy.total_bytes_received = stats_.total_bytes_received.load();
        stats_copy.connection_attempts = stats_.connection_attempts.load();
        stats_copy.successful_connections = stats_.successful_connections.load();
        stats_copy.failed_connections = stats_.failed_connections.load();
        stats_copy.last_connected_time = stats_.last_connected_time.load();
        stats_copy.last_activity_time = stats_.last_activity_time.load();
        stats_copy.average_latency = stats_.average_latency.load();
        return stats_copy;
    }

    void resetStats() {
        stats_.total_bytes_sent = 0;
        stats_.total_bytes_received = 0;
        stats_.connection_attempts = 0;
        stats_.successful_connections = 0;
        stats_.failed_connections = 0;
        stats_.last_connected_time = std::chrono::steady_clock::time_point{};
        stats_.last_activity_time = std::chrono::steady_clock::time_point{};
        stats_.average_latency = std::chrono::milliseconds{0};
    }

    std::string getRemoteAddress() const {
        try {
            if (state_ == ConnectionState::Connected) {
                return config_.use_ssl ? ssl_socket_->lowest_layer().remote_endpoint().address().to_string()
                                       : plain_socket_->remote_endpoint().address().to_string();
            }
        } catch (const std::exception&) {}
        return last_host_;
    }

    int getRemotePort() const {
        try {
            if (state_ == ConnectionState::Connected) {
                return config_.use_ssl ? ssl_socket_->lowest_layer().remote_endpoint().port()
                                       : plain_socket_->remote_endpoint().port();
            }
        } catch (const std::exception&) {}
        return last_port_;
    }

    void setProperty(const std::string& key, const std::string& value) { std::unique_lock<std::shared_mutex> lock(properties_mutex_); properties_[key] = value; }
    std::string getProperty(const std::string& key) const { std::shared_lock<std::shared_mutex> lock(properties_mutex_); auto it = properties_.find(key); return it != properties_.end() ? it->second : ""; }

    void setOnConnectingCallback(const OnConnectingCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_connecting_ = callback; }
    void setOnConnectedCallback(const OnConnectedCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_connected_ = callback; }
    void setOnDisconnectedCallback(const OnDisconnectedCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_disconnected_ = callback; }
    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_data_received_ = callback; }
    void setOnErrorCallback(const OnErrorCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_error_ = callback; }
    void setOnStateChangedCallback(const OnStateChangedCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_state_changed_ = callback; }
    void setOnHeartbeatCallback(const OnHeartbeatCallback& callback) { std::unique_lock<std::shared_mutex> lock(callbacks_mutex_); on_heartbeat_ = callback; }

private:
    using ssl_socket_t = asio::ssl::stream<asio::ip::tcp::socket>;

    void configureSslContext() {
        try {
            ssl_context_.set_verify_mode(config_.verify_ssl ? asio::ssl::verify_peer : asio::ssl::verify_none);
            if (!config_.ca_certificate_path.empty()) ssl_context_.load_verify_file(config_.ca_certificate_path);
            if (!config_.ssl_certificate_path.empty()) ssl_context_.use_certificate_file(config_.ssl_certificate_path, asio::ssl::context::pem);
            if (!config_.ssl_private_key_path.empty()) ssl_context_.use_private_key_file(config_.ssl_private_key_path, asio::ssl::context::pem);
            spdlog::info("SSL context configured");
        } catch (const std::exception& e) {
            spdlog::error("SSL context configuration error: {}", e.what());
        }
    }

    void handleConnectError(const std::string& message, std::promise<bool>& promise) {
        spdlog::error(message);
        logError(message);
        stats_.failed_connections++;
        changeState(ConnectionState::Failed);
        promise.set_value(false);
        {
            std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
            if (on_error_) on_error_(message);
        }
    }

    void handleSuccessfulConnection(std::promise<bool>& connect_promise) {
        stats_.successful_connections++;
        auto now = std::chrono::steady_clock::now();
        stats_.last_connected_time = now;
        stats_.last_activity_time = now;
        changeState(ConnectionState::Connected);
        connect_promise.set_value(true);
        startReceiving();
        if (config_.heartbeat_interval.count() > 0) startHeartbeat();
        {
            std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
            if (on_connected_) on_connected_();
        }
        spdlog::info("Connected to {}:{}", last_host_, last_port_);
        backoff_calculator_.reset();
    }

    void startReceiving() {
        if (state_ != ConnectionState::Connected) return;
        receive_buffer_.resize(config_.receive_buffer_size);
        auto receive_handler = [this](std::error_code ec, std::size_t length) { handleReceive(ec, length); };
        if (config_.use_ssl) {
            ssl_socket_->async_read_some(asio::buffer(receive_buffer_), receive_handler);
        } else {
            plain_socket_->async_read_some(asio::buffer(receive_buffer_), receive_handler);
        }
    }

    void handleReceive(const std::error_code& ec, std::size_t length) {
        if (!ec) {
            stats_.total_bytes_received += length;
            stats_.last_activity_time = std::chrono::steady_clock::now();
            {
                std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
                if (on_data_received_) on_data_received_({receive_buffer_.begin(), receive_buffer_.begin() + length});
            }
            startReceiving();
        } else {
            handleError(ec.message());
        }
    }

    void startHeartbeat() {
        if (!heartbeat_timer_) heartbeat_timer_ = std::make_unique<asio::steady_timer>(io_context_);
        heartbeat_timer_->expires_after(config_.heartbeat_interval);
        heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
            if (!ec && state_ == ConnectionState::Connected) {
                send(heartbeat_data_);
                {
                    std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
                    if (on_heartbeat_) on_heartbeat_();
                }
                startHeartbeat();
            }
        });
    }

    void handleError(const std::string& error) {
        ConnectionState expected = ConnectionState::Connected;
        if (state_.compare_exchange_strong(expected, ConnectionState::Disconnected)) {
            spdlog::error("Connection error: {}", error);
            logError(error);
            {
                std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
                if (on_error_) on_error_(error);
                if (on_disconnected_) on_disconnected_();
            }
            if (config_.auto_reconnect && config_.reconnect_attempts > 0) {
                attemptReconnect();
            }
        }
    }

    void attemptReconnect() {
        ConnectionState expected = ConnectionState::Disconnected;
        if (!state_.compare_exchange_strong(expected, ConnectionState::Reconnecting)) return;

        auto delay = backoff_calculator_.nextDelay();
        spdlog::info("Attempting reconnection in {}ms...", delay.count());

        auto reconnect_timer = std::make_shared<asio::steady_timer>(io_context_);
        reconnect_timer->expires_after(delay);
        reconnect_timer->async_wait([this, reconnect_timer](const asio::error_code& ec) {
            if (!ec && state_ == ConnectionState::Reconnecting) {
                connect(last_host_, last_port_, config_.connect_timeout);
            }
        });
    }

    void changeState(ConnectionState new_state) {
        ConnectionState old_state = state_.exchange(new_state);
        if (old_state != new_state) {
            std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
            if (on_state_changed_) on_state_changed_(old_state, new_state);
        }
    }

    void logError(const std::string& message) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = message;
    }

    ConnectionConfig config_;
    ProxyConfig proxy_config_;
    mutable std::mutex config_mutex_;

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ssl::context ssl_context_;
    std::unique_ptr<asio::ip::tcp::socket> plain_socket_;
    std::unique_ptr<ssl_socket_t> ssl_socket_;
    std::thread io_thread_;

    std::atomic<ConnectionState> state_;
    std::string last_error_;
    mutable std::mutex error_mutex_;
    std::string last_host_;
    int last_port_{0};

    std::unique_ptr<asio::steady_timer> heartbeat_timer_;
    BackoffCalculator backoff_calculator_;

    std::vector<char> receive_buffer_;
    std::vector<char> heartbeat_data_{'P', 'I', 'N', 'G'};

    ConnectionStats stats_;

    std::unordered_map<std::string, std::string> properties_;
    mutable std::shared_mutex properties_mutex_;

    OnConnectingCallback on_connecting_;
    OnConnectedCallback on_connected_;
    OnDisconnectedCallback on_disconnected_;
    OnDataReceivedCallback on_data_received_;
    OnErrorCallback on_error_;
    OnStateChangedCallback on_state_changed_;
    OnHeartbeatCallback on_heartbeat_;
    mutable std::shared_mutex callbacks_mutex_;
};

TcpClient::TcpClient(const ConnectionConfig& config) : impl_(std::make_unique<Impl>(config)) {}
TcpClient::~TcpClient() = default;

bool TcpClient::connect(const std::string& host, int port, std::optional<std::chrono::milliseconds> timeout) { return impl_->connect(host, port, timeout); }
std::future<bool> TcpClient::connectAsync(const std::string& host, int port) { return impl_->connectAsync(host, port); }
void TcpClient::disconnect() { impl_->disconnect(); }
void TcpClient::configureReconnection(int attempts, std::chrono::milliseconds delay) { impl_->configureReconnection(attempts, delay); }
void TcpClient::setHeartbeatInterval(std::chrono::milliseconds interval, const std::vector<char>& data) { impl_->setHeartbeatInterval(interval, data); }
bool TcpClient::send(const std::vector<char>& data) { return impl_->send(data); }
bool TcpClient::sendString(const std::string& data) { return impl_->sendString(data); }
bool TcpClient::sendWithTimeout(const std::vector<char>& data, std::chrono::milliseconds timeout) { return impl_->sendWithTimeout(data, timeout); }
std::future<std::vector<char>> TcpClient::receive(size_t size, std::optional<std::chrono::milliseconds> timeout) { return impl_->receive(size, timeout); }
std::future<std::string> TcpClient::receiveUntil(char delimiter, std::optional<std::chrono::milliseconds> timeout) { return impl_->receiveUntil(delimiter, timeout); }
std::future<std::vector<char>> TcpClient::requestResponse(const std::vector<char>& request, size_t response_size, std::optional<std::chrono::milliseconds> timeout) { return impl_->requestResponse(request, response_size, timeout); }
void TcpClient::setProxyConfig(const ProxyConfig& config) { impl_->setProxyConfig(config); }
void TcpClient::configureSslCertificates(const std::string& cert_path, const std::string& key_path, const std::string& ca_path) { impl_->configureSslCertificates(cert_path, key_path, ca_path); }
ConnectionState TcpClient::getConnectionState() const { return impl_->getConnectionState(); }
bool TcpClient::isConnected() const { return impl_->isConnected(); }
std::string TcpClient::getErrorMessage() const { return impl_->getErrorMessage(); }
ConnectionStats TcpClient::getStats() const { return impl_->getStats(); }
void TcpClient::resetStats() { impl_->resetStats(); }
std::string TcpClient::getRemoteAddress() const { return impl_->getRemoteAddress(); }
int TcpClient::getRemotePort() const { return impl_->getRemotePort(); }
void TcpClient::setProperty(const std::string& key, const std::string& value) { impl_->setProperty(key, value); }
std::string TcpClient::getProperty(const std::string& key) const { return impl_->getProperty(key); }
void TcpClient::setOnConnectingCallback(const OnConnectingCallback& callback) { impl_->setOnConnectingCallback(callback); }
void TcpClient::setOnConnectedCallback(const OnConnectedCallback& callback) { impl_->setOnConnectedCallback(callback); }
void TcpClient::setOnDisconnectedCallback(const OnDisconnectedCallback& callback) { impl_->setOnDisconnectedCallback(callback); }
void TcpClient::setOnDataReceivedCallback(const OnDataReceivedCallback& callback) { impl_->setOnDataReceivedCallback(callback); }
void TcpClient::setOnErrorCallback(const OnErrorCallback& callback) { impl_->setOnErrorCallback(callback); }
void TcpClient::setOnStateChangedCallback(const OnStateChangedCallback& callback) { impl_->setOnStateChangedCallback(callback); }
void TcpClient::setOnHeartbeatCallback(const OnHeartbeatCallback& callback) { impl_->setOnHeartbeatCallback(callback); }

}  // namespace atom::async::connection
