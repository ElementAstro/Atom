#include "async_tcpclient.hpp"

#include <algorithm>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

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
        // Reset after many attempts to avoid potential overflow
        if (attempt_ > 30) {
            reset();
        }

        // Calculate next delay with exponential backoff
        if (attempt_ > 0) {
            current_delay_ =
                std::min(std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::duration<double, std::milli>(
                                 current_delay_.count() * factor_)),
                         max_delay_);
        }

        // Apply jitter
        std::uniform_real_distribution<double> dist(1.0 - jitter_,
                                                    1.0 + jitter_);
        double jitter_factor = dist(random_engine_);

        auto jittered_delay =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<double, std::milli>(
                    current_delay_.count() * jitter_factor));

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
          backoff_calculator_(config.reconnect_delay, std::chrono::seconds(30),
                              1.5, 0.2),
          stats_(),
          properties_() {
        // Set up SSL context if needed
        if (config_.use_ssl) {
            configureSslContext();
            ssl_socket_ =
                std::make_unique<ssl_socket_t>(io_context_, ssl_context_);
        } else {
            plain_socket_ =
                std::make_unique<asio::ip::tcp::socket>(io_context_);
        }

        // Start the IO thread
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                logError("IO context exception: " + std::string(e.what()));
            }
        });
    }

    ~Impl() {
        // Clean shutdown
        disconnect();

        // Stop IO service and join thread
        try {
            work_guard_.reset();
            io_context_.stop();

            if (io_thread_.joinable()) {
                io_thread_.join();
            }
        } catch (const std::exception& e) {
            // Log but don't throw from destructor
            std::cerr << "Error during TCP client cleanup: " << e.what()
                      << std::endl;
        }
    }

    bool connect(const std::string& host, int port,
                 std::optional<std::chrono::milliseconds> timeout) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Already connected or connecting
        if (state_ == ConnectionState::Connected ||
            state_ == ConnectionState::Connecting) {
            return true;
        }

        last_host_ = host;
        last_port_ = port;

        changeState(ConnectionState::Connecting);

        if (on_connecting_) {
            on_connecting_();
        }

        stats_.connection_attempts++;

        auto actual_timeout = timeout.value_or(config_.connect_timeout);

        try {
            asio::ip::tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            // 使用共享指针来包装promise对象
            auto connect_promise_ptr = std::make_shared<std::promise<bool>>();
            auto connect_future = connect_promise_ptr->get_future();

            // Create a timer for timeout handling
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(actual_timeout);

            // Set up connection handlers
            auto handle_connect =
                [this, timer, promise_ptr = connect_promise_ptr](
                    const asio::error_code& ec,
                    const asio::ip::tcp::endpoint& _endpoint) {
                    timer->cancel();

                    if (ec) {
                        logError("Connect error: " + ec.message());
                        stats_.failed_connections++;
                        changeState(ConnectionState::Failed);
                        promise_ptr->set_value(false);

                        if (on_error_) {
                            on_error_("Connect error: " + ec.message());
                        }
                        return;
                    }

                    if (config_.use_ssl) {
                        // Perform SSL handshake
                        ssl_socket_->async_handshake(
                            asio::ssl::stream_base::client,
                            [this, timer, promise_ptr](
                                const asio::error_code& handshake_ec) {
                                if (handshake_ec) {
                                    logError("SSL handshake error: " +
                                             handshake_ec.message());
                                    stats_.failed_connections++;
                                    changeState(ConnectionState::Failed);
                                    promise_ptr->set_value(false);

                                    if (on_error_) {
                                        on_error_("SSL handshake error: " +
                                                  handshake_ec.message());
                                    }
                                    return;
                                }

                                handleSuccessfulConnection(*promise_ptr);
                            });
                    } else {
                        handleSuccessfulConnection(*promise_ptr);
                    }
                };

            // Set up timeout handler
            timer->async_wait([this, promise_ptr = connect_promise_ptr](
                                  const asio::error_code& ec) {
                if (ec == asio::error::operation_aborted) {
                    return;
                }
                logError("Connection timed out");
                if (config_.use_ssl) {
                    ssl_socket_->lowest_layer().cancel();
                } else {
                    plain_socket_->cancel();
                }
                stats_.failed_connections++;
                changeState(ConnectionState::Failed);
                promise_ptr->set_value(false);
                if (on_error_) {
                    on_error_("Connection timed out");
                }
            });

            // Initiate async connection
            if (config_.use_ssl) {
                asio::async_connect(ssl_socket_->lowest_layer(), endpoints,
                                    handle_connect);
            } else {
                asio::async_connect(*plain_socket_, endpoints, handle_connect);
            }

            // Wait for the connection to complete
            return connect_future.get();

        } catch (const std::exception& e) {
            logError(std::string("Connection exception: ") + e.what());
            stats_.failed_connections++;
            changeState(ConnectionState::Failed);

            if (on_error_) {
                on_error_(std::string("Connection exception: ") + e.what());
            }
            return false;
        }
    }

    std::future<bool> connectAsync(const std::string& host, int port) {
        return std::async(std::launch::async, [this, host, port]() {
            return connect(host, port, std::nullopt);
        });
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == ConnectionState::Disconnected) {
            return;
        }

        try {
            // Cancel any pending operations
            if (config_.use_ssl) {
                ssl_socket_->lowest_layer().cancel();
                ssl_socket_->lowest_layer().close();
            } else if (plain_socket_) {
                plain_socket_->cancel();
                plain_socket_->close();
            }

            // Cancel heartbeat timer
            if (heartbeat_timer_) {
                heartbeat_timer_->cancel();
            }

            changeState(ConnectionState::Disconnected);

            backoff_calculator_.reset();

            if (on_disconnected_) {
                on_disconnected_();
            }

            logInfo("Disconnected from server.");
        } catch (const std::exception& e) {
            logError(std::string("Error during disconnect: ") + e.what());
        }
    }

    void configureReconnection(int attempts, std::chrono::milliseconds delay) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.reconnect_attempts = attempts;
        config_.reconnect_delay = delay;
        backoff_calculator_ =
            BackoffCalculator(delay, std::chrono::seconds(30), 1.5, 0.2);
    }

    void setHeartbeatInterval(std::chrono::milliseconds interval,
                              const std::vector<char>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.heartbeat_interval = interval;
        heartbeat_data_ =
            data.empty() ? std::vector<char>{'P', 'I', 'N', 'G'} : data;

        // If connected, restart the heartbeat with new settings
        if (state_ == ConnectionState::Connected && heartbeat_timer_) {
            startHeartbeat();
        }
    }

    bool send(const std::vector<char>& data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            logError("Cannot send: not connected");
            return false;
        }

        try {
            size_t bytes_written;
            if (config_.use_ssl) {
                bytes_written = asio::write(*ssl_socket_, asio::buffer(data));
            } else {
                bytes_written = asio::write(*plain_socket_, asio::buffer(data));
            }

            stats_.total_bytes_sent += bytes_written;
            stats_.last_activity_time = std::chrono::steady_clock::now();

            logInfo("Sent data of size: " + std::to_string(bytes_written));
            return true;
        } catch (const std::exception& e) {
            logError(std::string("Send error: ") + e.what());
            handleError(e.what());
            return false;
        }
    }

    bool sendString(const std::string& data) {
        return send(std::vector<char>(data.begin(), data.end()));
    }

    bool sendWithTimeout(const std::vector<char>& data,
                         std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            logError("Cannot send: not connected");
            return false;
        }

        try {
            // Create a timer for the timeout
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(timeout);

            // Set up a promise to track the result
            auto send_promise = std::make_shared<std::promise<bool>>();
            auto send_future = send_promise->get_future();

            // Start the timeout timer
            timer->async_wait(
                [this, timer, send_promise](const asio::error_code& ec) {
                    if (ec == asio::error::operation_aborted) {
                        // Timer canceled, operation completed in time
                        return;
                    }

                    logError("Send operation timed out");
                    send_promise->set_value(false);

                    // Cancel the socket operation
                    if (config_.use_ssl) {
                        ssl_socket_->lowest_layer().cancel();
                    } else {
                        plain_socket_->cancel();
                    }
                });

            // Start the async write operation
            if (config_.use_ssl) {
                asio::async_write(
                    *ssl_socket_, asio::buffer(data),
                    [this, timer, send_promise](const asio::error_code& ec,
                                                std::size_t bytes_transferred) {
                        timer->cancel();

                        if (ec) {
                            logError("Async write error: " + ec.message());
                            send_promise->set_value(false);
                            handleError(ec.message());
                            return;
                        }

                        stats_.total_bytes_sent += bytes_transferred;
                        stats_.last_activity_time =
                            std::chrono::steady_clock::now();

                        send_promise->set_value(true);
                        logInfo("Sent data of size: " +
                                std::to_string(bytes_transferred));
                    });
            } else {
                asio::async_write(
                    *plain_socket_, asio::buffer(data),
                    [this, timer, send_promise](const asio::error_code& ec,
                                                std::size_t bytes_transferred) {
                        timer->cancel();

                        if (ec) {
                            logError("Async write error: " + ec.message());
                            send_promise->set_value(false);
                            handleError(ec.message());
                            return;
                        }

                        stats_.total_bytes_sent += bytes_transferred;
                        stats_.last_activity_time =
                            std::chrono::steady_clock::now();

                        send_promise->set_value(true);
                        logInfo("Sent data of size: " +
                                std::to_string(bytes_transferred));
                    });
            }

            return send_future.get();

        } catch (const std::exception& e) {
            logError(std::string("Send with timeout error: ") + e.what());
            handleError(e.what());
            return false;
        }
    }

    std::future<std::vector<char>> receive(
        size_t size, std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(config_.read_timeout);

        return std::async(std::launch::async, [this, size, actual_timeout]() {
            std::lock_guard<std::mutex> lock(mutex_);

            if (state_ != ConnectionState::Connected) {
                logError("Cannot receive: not connected");
                return std::vector<char>();
            }

            try {
                std::vector<char> data(size);

                // Create a timer for timeout
                auto timer = std::make_shared<asio::steady_timer>(io_context_);
                timer->expires_after(actual_timeout);

                // Set up a promise to track the result
                auto receive_promise =
                    std::make_shared<std::promise<std::vector<char>>>();
                auto receive_future = receive_promise->get_future();

                // Start the timeout timer
                timer->async_wait(
                    [this, timer, receive_promise](const asio::error_code& ec) {
                        if (ec == asio::error::operation_aborted) {
                            // Timer canceled, operation completed in time
                            return;
                        }

                        logError("Receive operation timed out");
                        receive_promise->set_value(std::vector<char>());

                        // Cancel the socket operation
                        if (config_.use_ssl) {
                            ssl_socket_->lowest_layer().cancel();
                        } else {
                            plain_socket_->cancel();
                        }
                    });

                // Start the async read operation
                if (config_.use_ssl) {
                    asio::async_read(
                        *ssl_socket_, asio::buffer(data, size),
                        [this, data, timer, receive_promise](
                            const asio::error_code& ec,
                            std::size_t bytes_transferred) {
                            timer->cancel();

                            if (ec) {
                                logError("Async read error: " + ec.message());
                                receive_promise->set_value(std::vector<char>());
                                handleError(ec.message());
                                return;
                            }

                            stats_.total_bytes_received += bytes_transferred;
                            stats_.last_activity_time =
                                std::chrono::steady_clock::now();

                            // Resize data to actual bytes received
                            auto result_data = data;
                            result_data.resize(bytes_transferred);
                            receive_promise->set_value(result_data);

                            logInfo("Received data of size: " +
                                    std::to_string(bytes_transferred));
                        });
                } else {
                    asio::async_read(
                        *plain_socket_, asio::buffer(data, size),
                        [this, data, timer, receive_promise](
                            const asio::error_code& ec,
                            std::size_t bytes_transferred) {
                            timer->cancel();

                            if (ec) {
                                logError("Async read error: " + ec.message());
                                receive_promise->set_value(std::vector<char>());
                                handleError(ec.message());
                                return;
                            }

                            stats_.total_bytes_received += bytes_transferred;
                            stats_.last_activity_time =
                                std::chrono::steady_clock::now();

                            // Resize data to actual bytes received
                            auto result_data = data;
                            result_data.resize(bytes_transferred);
                            receive_promise->set_value(result_data);

                            logInfo("Received data of size: " +
                                    std::to_string(bytes_transferred));
                        });
                }

                return receive_future.get();

            } catch (const std::exception& e) {
                logError(std::string("Receive error: ") + e.what());
                handleError(e.what());
                return std::vector<char>();
            }
        });
    }

    std::future<std::string> receiveUntil(
        char delimiter, std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(config_.read_timeout);

        return std::async(std::launch::async, [this, delimiter,
                                               actual_timeout]() {
            std::lock_guard<std::mutex> lock(mutex_);

            if (state_ != ConnectionState::Connected) {
                logError("Cannot receive: not connected");
                return std::string();
            }

            try {
                // Create a timer for timeout
                auto timer = std::make_shared<asio::steady_timer>(io_context_);
                timer->expires_after(actual_timeout);

                // Set up a promise to track the result
                auto receive_promise =
                    std::make_shared<std::promise<std::string>>();
                auto receive_future = receive_promise->get_future();

                // Buffer for the result
                auto buffer = std::make_shared<asio::streambuf>();

                // Start the timeout timer
                timer->async_wait(
                    [this, timer, receive_promise](const asio::error_code& ec) {
                        if (ec == asio::error::operation_aborted) {
                            // Timer canceled, operation completed in time
                            return;
                        }

                        logError("Receive until operation timed out");
                        receive_promise->set_value(std::string());

                        // Cancel the socket operation
                        if (config_.use_ssl) {
                            ssl_socket_->lowest_layer().cancel();
                        } else {
                            plain_socket_->cancel();
                        }
                    });

                // Start the async read until operation
                if (config_.use_ssl) {
                    asio::async_read_until(
                        *ssl_socket_, *buffer, delimiter,
                        [this, buffer, timer, receive_promise](
                            const asio::error_code& ec,
                            std::size_t bytes_transferred) {
                            timer->cancel();

                            if (ec) {
                                logError("Async read until error: " +
                                         ec.message());
                                receive_promise->set_value(std::string());
                                handleError(ec.message());
                                return;
                            }

                            stats_.total_bytes_received += bytes_transferred;
                            stats_.last_activity_time =
                                std::chrono::steady_clock::now();

                            // Extract data from streambuf to string
                            std::string data(
                                asio::buffers_begin(buffer->data()),
                                asio::buffers_begin(buffer->data()) +
                                    bytes_transferred);

                            buffer->consume(bytes_transferred);
                            receive_promise->set_value(data);

                            logInfo("Received data until delimiter, size: " +
                                    std::to_string(bytes_transferred));
                        });
                } else {
                    asio::async_read_until(
                        *plain_socket_, *buffer, delimiter,
                        [this, buffer, timer, receive_promise](
                            const asio::error_code& ec,
                            std::size_t bytes_transferred) {
                            timer->cancel();

                            if (ec) {
                                logError("Async read until error: " +
                                         ec.message());
                                receive_promise->set_value(std::string());
                                handleError(ec.message());
                                return;
                            }

                            stats_.total_bytes_received += bytes_transferred;
                            stats_.last_activity_time =
                                std::chrono::steady_clock::now();

                            // Extract data from streambuf to string
                            std::string data(
                                asio::buffers_begin(buffer->data()),
                                asio::buffers_begin(buffer->data()) +
                                    bytes_transferred);

                            buffer->consume(bytes_transferred);
                            receive_promise->set_value(data);

                            logInfo("Received data until delimiter, size: " +
                                    std::to_string(bytes_transferred));
                        });
                }

                return receive_future.get();

            } catch (const std::exception& e) {
                logError(std::string("Receive until error: ") + e.what());
                handleError(e.what());
                return std::string();
            }
        });
    }

    std::future<std::vector<char>> requestResponse(
        const std::vector<char>& request, size_t response_size,
        std::optional<std::chrono::milliseconds> timeout) {
        auto actual_timeout = timeout.value_or(std::chrono::milliseconds(
            config_.write_timeout.count() + config_.read_timeout.count()));

        return std::async(std::launch::async, [this, request, response_size,
                                               actual_timeout]() {
            // Send the request
            if (!send(request)) {
                logError("Request-response cycle failed at request stage");
                return std::vector<char>();
            }

            // Wait for the response
            auto response_future = receive(response_size, actual_timeout);
            return response_future.get();
        });
    }

    void setProxyConfig(const ProxyConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        proxy_config_ = config;
        // Actual proxy implementation would set up the proxy connection here
        if (proxy_config_.enabled) {
            logInfo("Proxy configuration set: " + proxy_config_.host + ":" +
                    std::to_string(proxy_config_.port));
        } else {
            logInfo("Proxy disabled");
        }
    }

    void configureSslCertificates(const std::string& cert_path,
                                  const std::string& key_path,
                                  const std::string& ca_path) {
        std::lock_guard<std::mutex> lock(mutex_);

        config_.ssl_certificate_path = cert_path;
        config_.ssl_private_key_path = key_path;
        config_.ca_certificate_path = ca_path;

        // Reconfigure the SSL context if needed
        if (config_.use_ssl) {
            configureSslContext();
        }
    }

    ConnectionState getConnectionState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    bool isConnected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == ConnectionState::Connected;
    }

    std::string getErrorMessage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    const ConnectionStats& getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void resetStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = ConnectionStats();
    }

    std::string getRemoteAddress() const {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == ConnectionState::Connected) {
                if (config_.use_ssl) {
                    return ssl_socket_->lowest_layer()
                        .remote_endpoint()
                        .address()
                        .to_string();
                } else {
                    return plain_socket_->remote_endpoint()
                        .address()
                        .to_string();
                }
            }
        } catch (const std::exception& e) {
            // Ignore errors and return the last known host
        }
        return last_host_;
    }

    int getRemotePort() const {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == ConnectionState::Connected) {
                if (config_.use_ssl) {
                    return ssl_socket_->lowest_layer().remote_endpoint().port();
                } else {
                    return plain_socket_->remote_endpoint().port();
                }
            }
        } catch (const std::exception& e) {
            // Ignore errors and return the last known port
        }
        return last_port_;
    }

    void setProperty(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        properties_[key] = value;
    }

    std::string getProperty(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = properties_.find(key);
        if (it != properties_.end()) {
            return it->second;
        }
        return "";
    }

    void setOnConnectingCallback(const OnConnectingCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_connecting_ = callback;
    }

    void setOnConnectedCallback(const OnConnectedCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_connected_ = callback;
    }

    void setOnDisconnectedCallback(const OnDisconnectedCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_disconnected_ = callback;
    }

    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_data_received_ = callback;
    }

    void setOnErrorCallback(const OnErrorCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_error_ = callback;
    }

    void setOnStateChangedCallback(const OnStateChangedCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_state_changed_ = callback;
    }

    void setOnHeartbeatCallback(const OnHeartbeatCallback& callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_heartbeat_ = callback;
    }

private:
    using ssl_socket_t = asio::ssl::stream<asio::ip::tcp::socket>;

    void configureSslContext() {
        try {
            if (config_.verify_ssl) {
                ssl_context_.set_verify_mode(asio::ssl::verify_peer);
            } else {
                ssl_context_.set_verify_mode(asio::ssl::verify_none);
            }

            // Load certificates if provided
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

            logInfo("SSL context configured");
        } catch (const std::exception& e) {
            logError(std::string("SSL context configuration error: ") +
                     e.what());
        }
    }

    // 修改函数签名，接受引用而不是值
    void handleSuccessfulConnection(std::promise<bool>& connect_promise) {
        stats_.successful_connections++;
        stats_.last_connected_time = std::chrono::steady_clock::now();
        stats_.last_activity_time = stats_.last_connected_time;

        changeState(ConnectionState::Connected);
        connect_promise.set_value(true);

        // Start continuous reading
        startReceiving();

        // Start heartbeat if enabled
        if (config_.heartbeat_interval.count() > 0) {
            startHeartbeat();
        }

        if (on_connected_) {
            on_connected_();
        }

        logInfo("Connected to " + last_host_ + ":" +
                std::to_string(last_port_));

        // Reset backoff calculator since connection succeeded
        backoff_calculator_.reset();
    }

    void startReceiving() {
        if (state_ != ConnectionState::Connected) {
            return;
        }

        receive_buffer_.resize(config_.receive_buffer_size);

        if (config_.use_ssl) {
            ssl_socket_->async_read_some(
                asio::buffer(receive_buffer_),
                [this](std::error_code ec, std::size_t length) {
                    handleReceive(ec, length);
                });
        } else {
            plain_socket_->async_read_some(
                asio::buffer(receive_buffer_),
                [this](std::error_code ec, std::size_t length) {
                    handleReceive(ec, length);
                });
        }
    }

    void handleReceive(const std::error_code& ec, std::size_t length) {
        if (!ec) {
            stats_.total_bytes_received += length;
            stats_.last_activity_time = std::chrono::steady_clock::now();

            if (on_data_received_) {
                on_data_received_(std::vector<char>(
                    receive_buffer_.begin(), receive_buffer_.begin() + length));
            }

            // Continue reading
            startReceiving();
        } else {
            handleError(ec.message());
        }
    }

    void startHeartbeat() {
        // Create new timer if needed
        if (!heartbeat_timer_) {
            heartbeat_timer_ =
                std::make_unique<asio::steady_timer>(io_context_);
        }

        heartbeat_timer_->expires_after(config_.heartbeat_interval);
        heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
            if (!ec && state_ == ConnectionState::Connected) {
                // Send heartbeat data
                send(heartbeat_data_);

                if (on_heartbeat_) {
                    on_heartbeat_();
                }

                // Reschedule heartbeat
                startHeartbeat();
            }
        });
    }

    void handleError(const std::string& error) {
        if (state_ == ConnectionState::Connected) {
            logError("Connection error: " + error);

            if (on_error_) {
                on_error_(error);
            }

            // Set state to disconnected
            changeState(ConnectionState::Disconnected);

            if (on_disconnected_) {
                on_disconnected_();
            }

            // Try to reconnect if auto-reconnect is enabled
            if (config_.auto_reconnect && config_.reconnect_attempts > 0) {
                attemptReconnect();
            }
        }
    }

    void attemptReconnect() {
        if (state_ == ConnectionState::Reconnecting) {
            return;
        }

        changeState(ConnectionState::Reconnecting);

        // Use the backoff calculator for delay
        auto delay = backoff_calculator_.nextDelay();

        logInfo("Attempting reconnection in " + std::to_string(delay.count()) +
                "ms...");

        // Schedule reconnection attempt
        auto reconnect_timer =
            std::make_shared<asio::steady_timer>(io_context_);
        reconnect_timer->expires_after(delay);
        reconnect_timer->async_wait(
            [this, reconnect_timer](const asio::error_code& ec) {
                if (!ec && state_ == ConnectionState::Reconnecting) {
                    // Try to connect again
                    connect(last_host_, last_port_, config_.connect_timeout);
                }
            });
    }

    void changeState(ConnectionState new_state) {
        if (state_ != new_state) {
            ConnectionState old_state = state_;
            state_ = new_state;

            if (on_state_changed_) {
                on_state_changed_(old_state, new_state);
            }
        }
    }

    void logInfo(const std::string& message) {
        std::cout << "[INFO] TcpClient: " << message << std::endl;
    }

    void logError(const std::string& message) {
        std::cerr << "[ERROR] TcpClient: " << message << std::endl;
        last_error_ = message;
    }

    // Configuration
    ConnectionConfig config_;
    ProxyConfig proxy_config_;

    // Core networking components
    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ssl::context ssl_context_;
    std::unique_ptr<asio::ip::tcp::socket> plain_socket_;
    std::unique_ptr<ssl_socket_t> ssl_socket_;
    std::thread io_thread_;

    // State management
    mutable std::mutex mutex_;
    ConnectionState state_;
    std::string last_error_;
    std::string last_host_;
    int last_port_{0};

    // Timers
    std::unique_ptr<asio::steady_timer> heartbeat_timer_;
    BackoffCalculator backoff_calculator_;

    // Buffers and data
    std::vector<char> receive_buffer_;
    std::vector<char> heartbeat_data_{'P', 'I', 'N', 'G'};

    // Statistics
    ConnectionStats stats_;

    // Properties
    std::unordered_map<std::string, std::string> properties_;

    // Callbacks
    OnConnectingCallback on_connecting_;
    OnConnectedCallback on_connected_;
    OnDisconnectedCallback on_disconnected_;
    OnDataReceivedCallback on_data_received_;
    OnErrorCallback on_error_;
    OnStateChangedCallback on_state_changed_;
    OnHeartbeatCallback on_heartbeat_;
};

// Implementation of TcpClient methods that delegate to Impl

TcpClient::TcpClient(const ConnectionConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

TcpClient::~TcpClient() = default;

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

bool TcpClient::send(const std::vector<char>& data) {
    return impl_->send(data);
}

bool TcpClient::sendString(const std::string& data) {
    return impl_->sendString(data);
}

bool TcpClient::sendWithTimeout(const std::vector<char>& data,
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
    const std::vector<char>& request, size_t response_size,
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

const ConnectionStats& TcpClient::getStats() const { return impl_->getStats(); }

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

void TcpClient::setOnConnectingCallback(const OnConnectingCallback& callback) {
    impl_->setOnConnectingCallback(callback);
}

void TcpClient::setOnConnectedCallback(const OnConnectedCallback& callback) {
    impl_->setOnConnectedCallback(callback);
}

void TcpClient::setOnDisconnectedCallback(
    const OnDisconnectedCallback& callback) {
    impl_->setOnDisconnectedCallback(callback);
}

void TcpClient::setOnDataReceivedCallback(
    const OnDataReceivedCallback& callback) {
    impl_->setOnDataReceivedCallback(callback);
}

void TcpClient::setOnErrorCallback(const OnErrorCallback& callback) {
    impl_->setOnErrorCallback(callback);
}

void TcpClient::setOnStateChangedCallback(
    const OnStateChangedCallback& callback) {
    impl_->setOnStateChangedCallback(callback);
}

void TcpClient::setOnHeartbeatCallback(const OnHeartbeatCallback& callback) {
    impl_->setOnHeartbeatCallback(callback);
}

}  // namespace atom::async::connection