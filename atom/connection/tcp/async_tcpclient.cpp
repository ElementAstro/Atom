/*
 * async_tcpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "async_tcpclient.hpp"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <variant>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <spdlog/spdlog.h>

namespace atom::async::connection {

namespace {
constexpr std::array<char, 4> DEFAULT_HEARTBEAT_DATA = {'P', 'I', 'N', 'G'};
}

/**
 * @brief Socket wrapper that unifies SSL and plain socket operations
 *
 * Uses std::variant to eliminate SSL/non-SSL branching throughout the code
 */
class SocketWrapper {
public:
    using PlainSocket = asio::ip::tcp::socket;
    using SslSocket = asio::ssl::stream<asio::ip::tcp::socket>;

    explicit SocketWrapper(asio::io_context& io_ctx, bool use_ssl,
                           asio::ssl::context& ssl_ctx)
        : use_ssl_(use_ssl) {
        if (use_ssl) {
            socket_ = std::make_unique<SslSocket>(io_ctx, ssl_ctx);
        } else {
            socket_ = std::make_unique<PlainSocket>(io_ctx);
        }
    }

    [[nodiscard]] bool useSsl() const noexcept { return use_ssl_; }

    // Get the lowest layer for connection operations
    auto& lowestLayer() {
        if (use_ssl_) {
            return std::get<std::unique_ptr<SslSocket>>(socket_)
                ->lowest_layer();
        }
        return std::get<std::unique_ptr<PlainSocket>>(socket_)->lowest_layer();
    }

    // Async connect
    template <typename Endpoints, typename Handler>
    void asyncConnect(const Endpoints& endpoints, Handler&& handler) {
        asio::async_connect(lowestLayer(), endpoints,
                            std::forward<Handler>(handler));
    }

    // SSL handshake (no-op for plain socket)
    template <typename Handler>
    void asyncHandshake(Handler&& handler) {
        if (use_ssl_) {
            std::get<std::unique_ptr<SslSocket>>(socket_)->async_handshake(
                asio::ssl::stream_base::client, std::forward<Handler>(handler));
        } else {
            // No handshake needed, call handler immediately
            asio::post(lowestLayer().get_executor(),
                       [h = std::forward<Handler>(handler)]() mutable {
                           h(asio::error_code{});
                       });
        }
    }

    // Sync write
    size_t write(const asio::const_buffer& buffer, asio::error_code& ec) {
        if (use_ssl_) {
            return asio::write(*std::get<std::unique_ptr<SslSocket>>(socket_),
                               buffer, ec);
        }
        return asio::write(*std::get<std::unique_ptr<PlainSocket>>(socket_),
                           buffer, ec);
    }

    // Async write
    template <typename Buffer, typename Handler>
    void asyncWrite(const Buffer& buffer, Handler&& handler) {
        if (use_ssl_) {
            asio::async_write(*std::get<std::unique_ptr<SslSocket>>(socket_),
                              buffer, std::forward<Handler>(handler));
        } else {
            asio::async_write(*std::get<std::unique_ptr<PlainSocket>>(socket_),
                              buffer, std::forward<Handler>(handler));
        }
    }

    // Async read
    template <typename Buffer, typename Handler>
    void asyncRead(const Buffer& buffer, Handler&& handler) {
        if (use_ssl_) {
            asio::async_read(*std::get<std::unique_ptr<SslSocket>>(socket_),
                             buffer, std::forward<Handler>(handler));
        } else {
            asio::async_read(*std::get<std::unique_ptr<PlainSocket>>(socket_),
                             buffer, std::forward<Handler>(handler));
        }
    }

    // Async read some
    template <typename Buffer, typename Handler>
    void asyncReadSome(const Buffer& buffer, Handler&& handler) {
        if (use_ssl_) {
            std::get<std::unique_ptr<SslSocket>>(socket_)->async_read_some(
                buffer, std::forward<Handler>(handler));
        } else {
            std::get<std::unique_ptr<PlainSocket>>(socket_)->async_read_some(
                buffer, std::forward<Handler>(handler));
        }
    }

    // Async read until
    template <typename Buffer, typename Handler>
    void asyncReadUntil(Buffer& buffer, char delimiter, Handler&& handler) {
        if (use_ssl_) {
            asio::async_read_until(
                *std::get<std::unique_ptr<SslSocket>>(socket_), buffer,
                delimiter, std::forward<Handler>(handler));
        } else {
            asio::async_read_until(
                *std::get<std::unique_ptr<PlainSocket>>(socket_), buffer,
                delimiter, std::forward<Handler>(handler));
        }
    }

    void cancel() {
        asio::error_code ec;
        lowestLayer().cancel(ec);
    }

    void close() {
        asio::error_code ec;
        lowestLayer().close(ec);
    }

    [[nodiscard]] asio::ip::tcp::endpoint remoteEndpoint() const {
        asio::error_code ec;
        if (use_ssl_) {
            return std::get<std::unique_ptr<SslSocket>>(socket_)
                ->lowest_layer()
                .remote_endpoint(ec);
        }
        return std::get<std::unique_ptr<PlainSocket>>(socket_)->remote_endpoint(
            ec);
    }

private:
    bool use_ssl_;
    std::variant<std::unique_ptr<PlainSocket>, std::unique_ptr<SslSocket>>
        socket_;
};

class TcpClient::Impl : public std::enable_shared_from_this<TcpClient::Impl> {
public:
    explicit Impl(const TcpClientConfig& config)
        : config_(config),
          io_context_(),
          work_guard_(asio::make_work_guard(io_context_)),
          ssl_context_(asio::ssl::context::sslv23),
          state_(ConnectionState::Disconnected),
          backoff_calculator_(config.reconnect_delay, std::chrono::seconds(30),
                              1.5, 0.2) {
        if (config_.use_ssl) {
            configureSslContext();
        }

        socket_ = std::make_unique<SocketWrapper>(io_context_, config_.use_ssl,
                                                  ssl_context_);

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

    bool connect(const std::string& host, int port,
                 std::optional<std::chrono::milliseconds> timeout) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        // Post the connection to the IO context
        asio::post(io_context_, [this, host, port, timeout,
                                 promise = std::move(promise)]() mutable {
            connectInternal(host, port, timeout, std::move(promise));
        });

        return future.get();
    }

    std::future<bool> connectAsync(const std::string& host, int port) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        asio::post(io_context_, [this, host, port,
                                 promise = std::move(promise)]() mutable {
            connectInternal(host, port, std::nullopt, std::move(promise));
        });

        return future;
    }

    void disconnect() {
        std::unique_lock lock(mutex_);

        if (state_ == ConnectionState::Disconnected) {
            return;
        }

        try {
            socket_->cancel();
            socket_->close();

            if (heartbeat_timer_) {
                heartbeat_timer_->cancel();
            }

            auto old_state = state_;
            state_ = ConnectionState::Disconnected;
            lock.unlock();

            backoff_calculator_.reset();

            if (on_state_changed_) {
                on_state_changed_(old_state, ConnectionState::Disconnected);
            }
            if (on_disconnected_) {
                on_disconnected_();
            }

            spdlog::info("Disconnected from server");
        } catch (const std::exception& e) {
            spdlog::error("Error during disconnect: {}", e.what());
        }
    }

    void configureReconnection(int attempts, std::chrono::milliseconds delay) {
        std::unique_lock lock(mutex_);
        config_.reconnect_attempts = attempts;
        config_.reconnect_delay = delay;
        backoff_calculator_ =
            BackoffCalculator(delay, std::chrono::seconds(30), 1.5, 0.2);
    }

    void setHeartbeatInterval(std::chrono::milliseconds interval,
                              const std::vector<char>& data) {
        std::unique_lock lock(mutex_);
        config_.heartbeat_interval = interval;
        heartbeat_data_ =
            data.empty() ? std::vector<char>(DEFAULT_HEARTBEAT_DATA.begin(),
                                             DEFAULT_HEARTBEAT_DATA.end())
                         : data;

        if (state_ == ConnectionState::Connected) {
            lock.unlock();
            startHeartbeat();
        }
    }

    bool send(std::span<const char> data) {
        std::shared_lock lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            spdlog::warn("Cannot send: not connected");
            return false;
        }

        try {
            asio::error_code ec;
            size_t bytes_written =
                socket_->write(asio::buffer(data.data(), data.size()), ec);

            if (ec) {
                lock.unlock();
                handleError(ec.message());
                return false;
            }

            stats_.total_bytes_sent += bytes_written;
            stats_.updateLastActivityTime();
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Send error: {}", e.what());
            lock.unlock();
            handleError(e.what());
            return false;
        }
    }

    bool sendString(std::string_view data) {
        return send(std::span<const char>(data.data(), data.size()));
    }

    bool sendWithTimeout(std::span<const char> data,
                         std::chrono::milliseconds timeout) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        asio::post(
            io_context_,
            [this, data_copy = std::vector<char>(data.begin(), data.end()),
             timeout, promise]() mutable {
                std::shared_lock lock(mutex_);

                if (state_ != ConnectionState::Connected) {
                    promise->set_value(false);
                    return;
                }

                auto timer = std::make_shared<asio::steady_timer>(io_context_);
                timer->expires_after(timeout);

                auto completed = std::make_shared<std::atomic<bool>>(false);

                timer->async_wait(
                    [this, completed, promise](const asio::error_code& ec) {
                        if (ec == asio::error::operation_aborted ||
                            completed->exchange(true)) {
                            return;
                        }
                        socket_->cancel();
                        promise->set_value(false);
                    });

                socket_->asyncWrite(
                    asio::buffer(data_copy),
                    [this, timer, completed, promise](
                        const asio::error_code& ec, std::size_t bytes) {
                        if (completed->exchange(true)) {
                            return;
                        }
                        timer->cancel();

                        if (ec) {
                            promise->set_value(false);
                            return;
                        }

                        stats_.total_bytes_sent += bytes;
                        stats_.updateLastActivityTime();
                        promise->set_value(true);
                    });
            });

        return future.get();
    }

    std::future<std::vector<char>> receive(
        size_t size, std::optional<std::chrono::milliseconds> timeout) {
        auto promise = std::make_shared<std::promise<std::vector<char>>>();
        auto future = promise->get_future();

        asio::post(io_context_, [this, size, timeout,
                                 promise = std::move(promise)]() mutable {
            receiveInternal(size, timeout, std::move(promise));
        });

        return future;
    }

    std::future<std::string> receiveUntil(
        char delimiter, std::optional<std::chrono::milliseconds> timeout) {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();

        asio::post(io_context_, [this, delimiter, timeout,
                                 promise = std::move(promise)]() mutable {
            receiveUntilInternal(delimiter, timeout, std::move(promise));
        });

        return future;
    }

    std::future<std::vector<char>> requestResponse(
        std::span<const char> request, size_t response_size,
        std::optional<std::chrono::milliseconds> timeout) {
        auto promise = std::make_shared<std::promise<std::vector<char>>>();
        auto future = promise->get_future();

        auto request_copy =
            std::make_shared<std::vector<char>>(request.begin(), request.end());

        asio::post(io_context_, [this, request_copy, response_size, timeout,
                                 promise = std::move(promise)]() mutable {
            if (!send(*request_copy)) {
                promise->set_value({});
                return;
            }

            receiveInternal(response_size, timeout, std::move(promise));
        });

        return future;
    }

    void setProxyConfig(const ProxyConfig& config) {
        std::unique_lock lock(mutex_);
        proxy_config_ = config;
        spdlog::info("Proxy configuration {}",
                     config.enabled ? "enabled" : "disabled");
    }

    void configureSslCertificates(const std::string& cert_path,
                                  const std::string& key_path,
                                  const std::string& ca_path) {
        std::unique_lock lock(mutex_);
        config_.ssl_certificate_path = cert_path;
        config_.ssl_private_key_path = key_path;
        config_.ca_certificate_path = ca_path;

        if (config_.use_ssl) {
            configureSslContext();
        }
    }

    [[nodiscard]] ConnectionState getConnectionState() const {
        std::shared_lock lock(mutex_);
        return state_;
    }

    [[nodiscard]] bool isConnected() const {
        std::shared_lock lock(mutex_);
        return state_ == ConnectionState::Connected;
    }

    [[nodiscard]] std::string getErrorMessage() const {
        std::shared_lock lock(mutex_);
        return last_error_;
    }

    [[nodiscard]] ConnectionStats getStats() const {
        return stats_;  // Already thread-safe via atomics
    }

    void resetStats() { stats_.reset(); }

    [[nodiscard]] std::string getRemoteAddress() const {
        std::shared_lock lock(mutex_);
        if (state_ == ConnectionState::Connected) {
            try {
                return socket_->remoteEndpoint().address().to_string();
            } catch (...) {
            }
        }
        return last_host_;
    }

    [[nodiscard]] int getRemotePort() const {
        std::shared_lock lock(mutex_);
        if (state_ == ConnectionState::Connected) {
            try {
                return socket_->remoteEndpoint().port();
            } catch (...) {
            }
        }
        return last_port_;
    }

    void setProperty(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);
        properties_[key] = value;
    }

    [[nodiscard]] std::string getProperty(const std::string& key) const {
        std::shared_lock lock(mutex_);
        auto it = properties_.find(key);
        return it != properties_.end() ? it->second : "";
    }

    // Callback setters
    void setOnConnectingCallback(TcpClient::OnConnectingCallback cb) {
        std::unique_lock lock(mutex_);
        on_connecting_ = std::move(cb);
    }
    void setOnConnectedCallback(TcpClient::OnConnectedCallback cb) {
        std::unique_lock lock(mutex_);
        on_connected_ = std::move(cb);
    }
    void setOnDisconnectedCallback(TcpClient::OnDisconnectedCallback cb) {
        std::unique_lock lock(mutex_);
        on_disconnected_ = std::move(cb);
    }
    void setOnDataReceivedCallback(TcpClient::OnDataReceivedCallback cb) {
        std::unique_lock lock(mutex_);
        on_data_received_ = std::move(cb);
    }
    void setOnErrorCallback(TcpClient::OnErrorCallback cb) {
        std::unique_lock lock(mutex_);
        on_error_ = std::move(cb);
    }
    void setOnStateChangedCallback(TcpClient::OnStateChangedCallback cb) {
        std::unique_lock lock(mutex_);
        on_state_changed_ = std::move(cb);
    }
    void setOnHeartbeatCallback(TcpClient::OnHeartbeatCallback cb) {
        std::unique_lock lock(mutex_);
        on_heartbeat_ = std::move(cb);
    }

private:
    void configureSslContext() {
        try {
            ssl_context_.set_verify_mode(config_.verify_ssl
                                             ? asio::ssl::verify_peer
                                             : asio::ssl::verify_none);

            if (!config_.ca_certificate_path.empty()) {
                ssl_context_.load_verify_file(config_.ca_certificate_path);
            }
            if (!config_.ssl_certificate_path.empty()) {
                ssl_context_.use_certificate_file(config_.ssl_certificate_path,
                                                  asio::ssl::context::pem);
            }
            if (!config_.ssl_private_key_path.empty()) {
                ssl_context_.use_private_key_file(config_.ssl_private_key_path,
                                                  asio::ssl::context::pem);
            }
        } catch (const std::exception& e) {
            spdlog::error("SSL context configuration error: {}", e.what());
        }
    }

    void connectInternal(const std::string& host, int port,
                         std::optional<std::chrono::milliseconds> timeout,
                         std::shared_ptr<std::promise<bool>> promise) {
        std::unique_lock lock(mutex_);

        if (state_ == ConnectionState::Connected ||
            state_ == ConnectionState::Connecting) {
            promise->set_value(state_ == ConnectionState::Connected);
            return;
        }

        last_host_ = host;
        last_port_ = port;
        changeStateLocked(ConnectionState::Connecting);
        stats_.connection_attempts++;

        lock.unlock();

        if (on_connecting_) {
            on_connecting_();
        }

        auto actual_timeout = timeout.value_or(config_.connect_timeout);

        try {
            asio::ip::tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(actual_timeout);

            auto completed = std::make_shared<std::atomic<bool>>(false);

            timer->async_wait(
                [this, completed, promise](const asio::error_code& ec) {
                    if (ec == asio::error::operation_aborted ||
                        completed->exchange(true)) {
                        return;
                    }
                    socket_->cancel();
                    handleConnectionFailure(*promise, "Connection timed out");
                });

            socket_->asyncConnect(
                endpoints, [this, timer, completed, promise](
                               const asio::error_code& ec,
                               const asio::ip::tcp::endpoint&) {
                    if (completed->exchange(true)) {
                        return;
                    }
                    timer->cancel();

                    if (ec) {
                        handleConnectionFailure(
                            *promise, "Connect error: " + ec.message());
                        return;
                    }

                    // Perform SSL handshake if needed
                    socket_->asyncHandshake(
                        [this, promise](const asio::error_code& handshake_ec) {
                            if (handshake_ec) {
                                handleConnectionFailure(
                                    *promise, "SSL handshake error: " +
                                                  handshake_ec.message());
                                return;
                            }
                            handleSuccessfulConnection(*promise);
                        });
                });

        } catch (const std::exception& e) {
            handleConnectionFailure(
                *promise, std::string("Connection exception: ") + e.what());
        }
    }

    void handleSuccessfulConnection(std::promise<bool>& promise) {
        std::unique_lock lock(mutex_);

        stats_.successful_connections++;
        stats_.updateLastConnectedTime();
        stats_.updateLastActivityTime();

        changeStateLocked(ConnectionState::Connected);
        lock.unlock();

        promise.set_value(true);

        startReceiving();

        if (config_.heartbeat_interval.count() > 0) {
            startHeartbeat();
        }

        if (on_connected_) {
            on_connected_();
        }

        backoff_calculator_.reset();
        spdlog::info("Connected to {}:{}", last_host_, last_port_);
    }

    void handleConnectionFailure(std::promise<bool>& promise,
                                 const std::string& error) {
        spdlog::error("{}", error);

        std::unique_lock lock(mutex_);
        stats_.failed_connections++;
        last_error_ = error;
        changeStateLocked(ConnectionState::Failed);
        lock.unlock();

        promise.set_value(false);

        if (on_error_) {
            on_error_(error);
        }
    }

    void startReceiving() {
        std::shared_lock lock(mutex_);
        if (state_ != ConnectionState::Connected) {
            return;
        }

        receive_buffer_.resize(config_.receive_buffer_size);
        lock.unlock();

        socket_->asyncReadSome(
            asio::buffer(receive_buffer_),
            [this](const asio::error_code& ec, std::size_t length) {
                handleReceive(ec, length);
            });
    }

    void handleReceive(const asio::error_code& ec, std::size_t length) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                handleError(ec.message());
            }
            return;
        }

        stats_.total_bytes_received += length;
        stats_.updateLastActivityTime();

        if (on_data_received_) {
            on_data_received_(std::vector<char>(
                receive_buffer_.begin(), receive_buffer_.begin() + length));
        }

        startReceiving();
    }

    void startHeartbeat() {
        if (!heartbeat_timer_) {
            heartbeat_timer_ =
                std::make_unique<asio::steady_timer>(io_context_);
        }

        heartbeat_timer_->expires_after(config_.heartbeat_interval);
        heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
            if (ec) {
                return;
            }

            std::shared_lock lock(mutex_);
            if (state_ != ConnectionState::Connected) {
                return;
            }
            lock.unlock();

            send(heartbeat_data_);

            if (on_heartbeat_) {
                on_heartbeat_();
            }

            startHeartbeat();
        });
    }

    void handleError(const std::string& error) {
        std::unique_lock lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return;
        }

        spdlog::error("Connection error: {}", error);
        last_error_ = error;
        changeStateLocked(ConnectionState::Disconnected);

        lock.unlock();

        if (on_error_) {
            on_error_(error);
        }
        if (on_disconnected_) {
            on_disconnected_();
        }

        if (config_.auto_reconnect && config_.reconnect_attempts > 0) {
            attemptReconnect();
        }
    }

    void attemptReconnect() {
        std::unique_lock lock(mutex_);
        if (state_ == ConnectionState::Reconnecting) {
            return;
        }

        changeStateLocked(ConnectionState::Reconnecting);
        lock.unlock();

        auto delay = backoff_calculator_.nextDelay();
        spdlog::info("Attempting reconnection in {} ms...", delay.count());

        auto timer = std::make_shared<asio::steady_timer>(io_context_);
        timer->expires_after(delay);
        timer->async_wait([this, timer](const asio::error_code& ec) {
            if (ec) {
                return;
            }

            std::shared_lock lock(mutex_);
            if (state_ != ConnectionState::Reconnecting) {
                return;
            }
            lock.unlock();

            connect(last_host_, last_port_, config_.connect_timeout);
        });
    }

    void changeStateLocked(ConnectionState new_state) {
        if (state_ != new_state) {
            auto old_state = state_;
            state_ = new_state;

            if (on_state_changed_) {
                on_state_changed_(old_state, new_state);
            }
        }
    }

    void receiveInternal(
        size_t size, std::optional<std::chrono::milliseconds> timeout,
        std::shared_ptr<std::promise<std::vector<char>>> promise) {
        std::shared_lock lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            promise->set_value({});
            return;
        }
        lock.unlock();

        auto buffer = std::make_shared<std::vector<char>>(size);
        auto actual_timeout = timeout.value_or(config_.read_timeout);

        auto timer = std::make_shared<asio::steady_timer>(io_context_);
        timer->expires_after(actual_timeout);

        auto completed = std::make_shared<std::atomic<bool>>(false);

        timer->async_wait(
            [this, completed, promise](const asio::error_code& ec) {
                if (ec == asio::error::operation_aborted ||
                    completed->exchange(true)) {
                    return;
                }
                socket_->cancel();
                promise->set_value({});
            });

        socket_->asyncRead(asio::buffer(*buffer),
                           [this, buffer, timer, completed, promise](
                               const asio::error_code& ec, std::size_t bytes) {
                               if (completed->exchange(true)) {
                                   return;
                               }
                               timer->cancel();

                               if (ec) {
                                   promise->set_value({});
                                   return;
                               }

                               stats_.total_bytes_received += bytes;
                               stats_.updateLastActivityTime();

                               buffer->resize(bytes);
                               promise->set_value(std::move(*buffer));
                           });
    }

    void receiveUntilInternal(
        char delimiter, std::optional<std::chrono::milliseconds> timeout,
        std::shared_ptr<std::promise<std::string>> promise) {
        std::shared_lock lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            promise->set_value({});
            return;
        }
        lock.unlock();

        auto buffer = std::make_shared<asio::streambuf>();
        auto actual_timeout = timeout.value_or(config_.read_timeout);

        auto timer = std::make_shared<asio::steady_timer>(io_context_);
        timer->expires_after(actual_timeout);

        auto completed = std::make_shared<std::atomic<bool>>(false);

        timer->async_wait(
            [this, completed, promise](const asio::error_code& ec) {
                if (ec == asio::error::operation_aborted ||
                    completed->exchange(true)) {
                    return;
                }
                socket_->cancel();
                promise->set_value({});
            });

        socket_->asyncReadUntil(
            *buffer, delimiter,
            [this, buffer, timer, completed, promise](
                const asio::error_code& ec, std::size_t bytes) {
                if (completed->exchange(true)) {
                    return;
                }
                timer->cancel();

                if (ec) {
                    promise->set_value({});
                    return;
                }

                stats_.total_bytes_received += bytes;
                stats_.updateLastActivityTime();

                std::string data(asio::buffers_begin(buffer->data()),
                                 asio::buffers_begin(buffer->data()) + bytes);
                buffer->consume(bytes);
                promise->set_value(std::move(data));
            });
    }

    // Configuration
    TcpClientConfig config_;
    ProxyConfig proxy_config_;

    // Core networking
    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ssl::context ssl_context_;
    std::unique_ptr<SocketWrapper> socket_;
    std::thread io_thread_;

    // State management - use shared_mutex for read-heavy workloads
    mutable std::shared_mutex mutex_;
    ConnectionState state_;
    std::string last_error_;
    std::string last_host_;
    int last_port_{0};

    // Timers and backoff
    std::unique_ptr<asio::steady_timer> heartbeat_timer_;
    BackoffCalculator backoff_calculator_;

    // Buffers
    std::vector<char> receive_buffer_;
    std::vector<char> heartbeat_data_{DEFAULT_HEARTBEAT_DATA.begin(),
                                      DEFAULT_HEARTBEAT_DATA.end()};

    // Statistics (thread-safe via atomics)
    ConnectionStats stats_;

    // Properties
    std::unordered_map<std::string, std::string> properties_;

    // Callbacks
    TcpClient::OnConnectingCallback on_connecting_;
    TcpClient::OnConnectedCallback on_connected_;
    TcpClient::OnDisconnectedCallback on_disconnected_;
    TcpClient::OnDataReceivedCallback on_data_received_;
    TcpClient::OnErrorCallback on_error_;
    TcpClient::OnStateChangedCallback on_state_changed_;
    TcpClient::OnHeartbeatCallback on_heartbeat_;
};

// TcpClient public implementation

TcpClient::TcpClient(const TcpClientConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

TcpClient::~TcpClient() = default;

TcpClient::TcpClient(TcpClient&&) noexcept = default;
TcpClient& TcpClient::operator=(TcpClient&&) noexcept = default;

bool TcpClient::connect(const std::string& host, int port,
                        std::optional<std::chrono::milliseconds> timeout) {
    return impl_->connect(host, port, timeout);
}

std::future<bool> TcpClient::connectAsync(const std::string& host, int port) {
    return impl_->connectAsync(host, port);
}

void TcpClient::disconnect() { impl_->disconnect(); }

void TcpClient::configureReconnection(int attempts,
                                      std::chrono::milliseconds delay) {
    impl_->configureReconnection(attempts, delay);
}

void TcpClient::setHeartbeatInterval(std::chrono::milliseconds interval,
                                     const std::vector<char>& data) {
    impl_->setHeartbeatInterval(interval, data);
}

bool TcpClient::send(std::span<const char> data) { return impl_->send(data); }

bool TcpClient::sendString(std::string_view data) {
    return impl_->sendString(data);
}

bool TcpClient::sendWithTimeout(std::span<const char> data,
                                std::chrono::milliseconds timeout) {
    return impl_->sendWithTimeout(data, timeout);
}

std::future<std::vector<char>> TcpClient::receive(
    size_t size, std::optional<std::chrono::milliseconds> timeout) {
    return impl_->receive(size, timeout);
}

std::future<std::string> TcpClient::receiveUntil(
    char delimiter, std::optional<std::chrono::milliseconds> timeout) {
    return impl_->receiveUntil(delimiter, timeout);
}

std::future<std::vector<char>> TcpClient::requestResponse(
    std::span<const char> request, size_t response_size,
    std::optional<std::chrono::milliseconds> timeout) {
    return impl_->requestResponse(request, response_size, timeout);
}

void TcpClient::setProxyConfig(const ProxyConfig& config) {
    impl_->setProxyConfig(config);
}

void TcpClient::configureSslCertificates(const std::string& cert_path,
                                         const std::string& key_path,
                                         const std::string& ca_path) {
    impl_->configureSslCertificates(cert_path, key_path, ca_path);
}

ConnectionState TcpClient::getConnectionState() const {
    return impl_->getConnectionState();
}

bool TcpClient::isConnected() const { return impl_->isConnected(); }

std::string TcpClient::getErrorMessage() const {
    return impl_->getErrorMessage();
}

ConnectionStats TcpClient::getStats() const { return impl_->getStats(); }

void TcpClient::resetStats() { impl_->resetStats(); }

std::string TcpClient::getRemoteAddress() const {
    return impl_->getRemoteAddress();
}

int TcpClient::getRemotePort() const { return impl_->getRemotePort(); }

void TcpClient::setProperty(const std::string& key, const std::string& value) {
    impl_->setProperty(key, value);
}

std::string TcpClient::getProperty(const std::string& key) const {
    return impl_->getProperty(key);
}

void TcpClient::setOnConnectingCallback(OnConnectingCallback callback) {
    impl_->setOnConnectingCallback(std::move(callback));
}

void TcpClient::setOnConnectedCallback(OnConnectedCallback callback) {
    impl_->setOnConnectedCallback(std::move(callback));
}

void TcpClient::setOnDisconnectedCallback(OnDisconnectedCallback callback) {
    impl_->setOnDisconnectedCallback(std::move(callback));
}

void TcpClient::setOnDataReceivedCallback(OnDataReceivedCallback callback) {
    impl_->setOnDataReceivedCallback(std::move(callback));
}

void TcpClient::setOnErrorCallback(OnErrorCallback callback) {
    impl_->setOnErrorCallback(std::move(callback));
}

void TcpClient::setOnStateChangedCallback(OnStateChangedCallback callback) {
    impl_->setOnStateChangedCallback(std::move(callback));
}

void TcpClient::setOnHeartbeatCallback(OnHeartbeatCallback callback) {
    impl_->setOnHeartbeatCallback(std::move(callback));
}

}  // namespace atom::async::connection
