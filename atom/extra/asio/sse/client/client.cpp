#include "client.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <random>
#include <regex>
#include "event_store.hpp"

using namespace std::chrono_literals;

namespace atom::extra::asio::sse {

class Client::Impl {
public:
    Impl(net::io_context& io_context, const ClientConfig& config)
        : io_context_(io_context),
          resolver_(io_context),
          config_(config),
          reconnect_timer_(io_context),
          event_store_(config.store_events ? std::make_unique<EventStore>(
                                                 config.event_store_path)
                                           : nullptr) {
        event_filters_.insert(config.event_types_filter.begin(),
                              config.event_types_filter.end());

#ifdef USE_SSL
        if (config.use_ssl) {
            ssl_context_ = std::make_unique<ssl_context>(ssl_context::sslv23);
            configure_ssl();
        }
#endif
    }

    ~Impl() = default;

    void set_event_handler(EventCallback handler) {
        event_handler_ = std::move(handler);
    }

    void set_connection_handler(ConnectionCallback handler) {
        connection_handler_ = std::move(handler);
    }

    void start() {
        reconnect_count_ = 0;

        if (config_.last_event_id.empty() && event_store_) {
            config_.last_event_id = event_store_->get_latest_event_id();
            if (!config_.last_event_id.empty()) {
                SPDLOG_INFO("Resuming from last event ID: {}",
                            config_.last_event_id);
            }
        }

        co_spawn(
            io_context_,
            [this]() -> net::awaitable<void> { co_await connect(); }, detached);
    }

    void stop() {
        error_code ec;
        reconnect_timer_.cancel(ec);

#ifdef USE_SSL
        if (ssl_socket_) {
            error_code ec;
            ssl_socket_->lowest_layer().close(ec);
            ssl_socket_.reset();
        }
#else
        if (socket_) {
            error_code ec;
            ec = socket_->close(ec);
            if (ec) {
                spdlog::warn("Socket close error: {}", ec.message());
            }
            socket_.reset();
        }
#endif

        SPDLOG_INFO("Client stopped");
    }

    void reconnect() {
        stop();
        parsing_headers_ = true;
        buffer_.consume(buffer_.size());
        current_event_lines_.clear();
        schedule_reconnect();
    }

    void add_event_filter(const std::string& event_type) {
        event_filters_.insert(event_type);
    }

    void remove_event_filter(const std::string& event_type) {
        event_filters_.erase(event_type);
    }

    void clear_event_filters() { event_filters_.clear(); }

    bool is_connected() const {
#ifdef USE_SSL
        if (config_.use_ssl && ssl_socket_) {
            return ssl_socket_->lowest_layer().is_open();
        }
        return socket_ && socket_->is_open();
#else
        return socket_ && socket_->is_open();
#endif
    }

    const ClientConfig& config() const { return config_; }

private:
    net::io_context& io_context_;
    tcp::resolver resolver_;
    ClientConfig config_;
    std::unique_ptr<EventStore> event_store_;

#ifdef USE_SSL
    std::unique_ptr<ssl_context> ssl_context_;
    std::unique_ptr<ssl::stream<tcp::socket>> ssl_socket_;
#else
    std::unique_ptr<tcp::socket> socket_;
#endif

    net::streambuf buffer_;
    std::vector<std::string> current_event_lines_;
    bool parsing_headers_ = true;
    EventCallback event_handler_;
    ConnectionCallback connection_handler_;
    net::steady_timer reconnect_timer_;
    int reconnect_count_ = 0;
    std::unordered_set<std::string> event_filters_;

#ifdef USE_SSL
    void configure_ssl() {
        if (!ssl_context_)
            return;

        ssl_context_->set_default_verify_paths();

        if (!config_.ca_cert_file.empty()) {
            ssl_context_->load_verify_file(config_.ca_cert_file);
        }

        ssl_context_->set_verify_mode(config_.verify_ssl ? ssl::verify_peer
                                                         : ssl::verify_none);
    }
#endif

    net::awaitable<void> connect() {
        try {
            SPDLOG_INFO("Connecting to {}:{}{}", config_.host, config_.port,
                        config_.path);

            auto [ec, endpoints] = co_await as_tuple_awaitable(
                resolver_.async_resolve(config_.host, config_.port));

            if (ec) {
                handle_connection_error("Failed to resolve host: " +
                                        ec.message());
                co_return;
            }

#ifdef USE_SSL
            if (config_.use_ssl && ssl_context_) {
                ssl_socket_ = std::make_unique<ssl::stream<tcp::socket>>(
                    io_context_, *ssl_context_);

                if (!SSL_set_tlsext_host_name(ssl_socket_->native_handle(),
                                              config_.host.c_str())) {
                    error_code ec{static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category()};
                    handle_connection_error("SSL SNI error: " + ec.message());
                    co_return;
                }

                auto [connect_ec, _] = co_await as_tuple_awaitable(
                    net::async_connect(ssl_socket_->lowest_layer(), endpoints));

                if (connect_ec) {
                    handle_connection_error("Failed to connect: " +
                                            connect_ec.message());
                    co_return;
                }

                auto [handshake_ec] = co_await as_tuple_awaitable(
                    ssl_socket_->async_handshake(ssl::stream_base::client));

                if (handshake_ec) {
                    handle_connection_error("SSL handshake failed: " +
                                            handshake_ec.message());
                    co_return;
                }
            } else {
                socket_ = std::make_unique<tcp::socket>(io_context_);
                auto [connect_ec, _] = co_await as_tuple_awaitable(
                    net::async_connect(*socket_, endpoints));

                if (connect_ec) {
                    handle_connection_error("Failed to connect: " +
                                            connect_ec.message());
                    co_return;
                }
            }
#else
            socket_ = std::make_unique<tcp::socket>(io_context_);
            auto [connect_ec, _] = co_await as_tuple_awaitable(
                net::async_connect(*socket_, endpoints));

            if (connect_ec) {
                handle_connection_error("Failed to connect: " +
                                        connect_ec.message());
                co_return;
            }
#endif

            co_await send_request();
            reconnect_count_ = 0;
            co_await read_response();

        } catch (const std::exception& e) {
            handle_connection_error(std::string("Exception: ") + e.what());
        }

        if (config_.reconnect &&
            reconnect_count_ < config_.max_reconnect_attempts) {
            schedule_reconnect();
        }
    }

    net::awaitable<void> send_request() {
        std::string request = "GET " + config_.path +
                              " HTTP/1.1\r\n"
                              "Host: " +
                              config_.host + ":" + config_.port +
                              "\r\n"
                              "Accept: text/event-stream\r\n"
                              "Cache-Control: no-cache\r\n"
                              "Connection: keep-alive\r\n";

        if (!config_.api_key.empty()) {
            request += "X-API-Key: " + config_.api_key + "\r\n";
        }

        if (!config_.username.empty() && !config_.password.empty()) {
            std::string auth = config_.username + ":" + config_.password;
            // Note: Base64 encoding would be implemented here in a real
            // application
            std::string encoded_auth = "TODO: Base64 encode here";
            request += "Authorization: Basic " + encoded_auth + "\r\n";
        }

        if (!config_.last_event_id.empty()) {
            request += "Last-Event-ID: " + config_.last_event_id + "\r\n";
        }

        request += "\r\n";

        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            config_.use_ssl && ssl_socket_
                ? net::async_write(*ssl_socket_, net::buffer(request))
                : net::async_write(*socket_, net::buffer(request))
#else
            net::async_write(*socket_, net::buffer(request))
#endif
        );

        if (ec) {
            throw std::runtime_error("Failed to send request: " + ec.message());
        }

        SPDLOG_DEBUG("Sent HTTP request");
    }

    net::awaitable<void> read_response() {
        while (is_connected()) {
            auto [ec, bytes] = co_await as_tuple_awaitable(
#ifdef USE_SSL
                config_.use_ssl && ssl_socket_
                    ? net::async_read_until(*ssl_socket_, buffer_, "\n")
                    : net::async_read_until(*socket_, buffer_, "\n")
#else
                net::async_read_until(*socket_, buffer_, "\n")
#endif
            );

            if (ec) {
                if (ec == net::error::eof) {
                    if (connection_handler_) {
                        connection_handler_(false,
                                            "Connection closed by server");
                    }
                } else {
                    if (connection_handler_) {
                        connection_handler_(false,
                                            "Read error: " + ec.message());
                    }
                }
                break;
            }

            co_await process_data(bytes);
        }
    }

    net::awaitable<void> process_data(std::size_t bytes) {
        std::string line(net::buffers_begin(buffer_.data()),
                         net::buffers_begin(buffer_.data()) + bytes);
        buffer_.consume(bytes);

        if (parsing_headers_) {
            if (line == "\r\n" || line == "\n") {
                parsing_headers_ = false;
                if (connection_handler_) {
                    connection_handler_(true, "Connected to SSE stream");
                }
            }
            co_return;
        }

        std::string line_str =
            std::regex_replace(line, std::regex("\r\n|\n"), "");

        if (line_str.empty()) {
            if (!current_event_lines_.empty()) {
                auto event_opt = Event::deserialize(current_event_lines_);

                if (event_opt) {
                    config_.last_event_id = event_opt->id();

                    if (config_.store_events && event_store_) {
                        event_store_->store_event(*event_opt);
                    }

                    bool passes_filter =
                        event_filters_.empty() ||
                        event_filters_.contains(event_opt->event_type());

                    if (passes_filter) {
                        if (event_opt->is_compressed()) {
                            event_opt->decompress();
                        }

                        if (event_handler_) {
                            event_handler_(*event_opt);
                        }
                    }
                }

                current_event_lines_.clear();
            }
        } else {
            current_event_lines_.push_back(line_str);
        }

        co_return;
    }

    void schedule_reconnect() {
        if (!config_.reconnect ||
            reconnect_count_ >= config_.max_reconnect_attempts) {
            if (connection_handler_) {
                connection_handler_(false, "Max reconnection attempts reached");
            }
            return;
        }

        int delay = config_.reconnect_base_delay_ms *
                    (1 << std::min(reconnect_count_, 10));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(-delay / 5, delay / 5);
        delay += dist(gen);

        delay = std::min(delay, 30000);

        SPDLOG_INFO("Scheduling reconnect attempt {} in {} ms",
                    reconnect_count_ + 1, delay);

        reconnect_timer_.expires_after(std::chrono::milliseconds(delay));
        reconnect_timer_.async_wait([this](const error_code& ec) {
            if (!ec) {
                reconnect_count_++;
                co_spawn(
                    io_context_,
                    [this]() -> net::awaitable<void> { co_await connect(); },
                    detached);
            }
        });
    }

    void handle_connection_error(const std::string& message) {
        SPDLOG_ERROR("Connection error: {}", message);
        if (connection_handler_) {
            connection_handler_(false, message);
        }
    }
};

// Client public interface implementation
Client::Client(net::io_context& io_context, const ClientConfig& config)
    : pimpl_(std::make_unique<Impl>(io_context, config)) {}

Client::~Client() = default;

void Client::set_event_handler(EventCallback handler) {
    pimpl_->set_event_handler(std::move(handler));
}

void Client::set_connection_handler(ConnectionCallback handler) {
    pimpl_->set_connection_handler(std::move(handler));
}

void Client::start() { pimpl_->start(); }

void Client::stop() { pimpl_->stop(); }

void Client::reconnect() { pimpl_->reconnect(); }

void Client::add_event_filter(const std::string& event_type) {
    pimpl_->add_event_filter(event_type);
}

void Client::remove_event_filter(const std::string& event_type) {
    pimpl_->remove_event_filter(event_type);
}

void Client::clear_event_filters() { pimpl_->clear_event_filters(); }

bool Client::is_connected() const { return pimpl_->is_connected(); }

const ClientConfig& Client::config() const { return pimpl_->config(); }

}  // namespace atom::extra::asio::sse