#include "server.hpp"
#include <spdlog/spdlog.h>
#include <atomic>

using namespace std::chrono_literals;

namespace atom::extra::asio::sse {

SSEServer::SSEServer(net::io_context& io_context, const ServerConfig& config)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(net::ip::make_address(config.address),
                                          config.port)),
      event_store_(config.event_store_path, config.max_event_history),
      event_queue_(event_store_, config.persist_events),
      auth_service_(config.auth_file),
      metrics_(),
      config_(config),
      last_cleanup_(std::chrono::steady_clock::now()),
      connection_monitor_timer_(io_context) {
#ifdef USE_SSL
    if (config.enable_ssl) {
        ssl_context_ = std::make_unique<ssl_context>(ssl_context::sslv23);
        configure_ssl();
    }
#endif

    start_connection_monitor();

    co_spawn(
        acceptor_.get_executor(),
        [this]() -> net::awaitable<void> { co_await accept_connections(); },
        detached);

    spdlog::info("SSE Server started on {}:{}", config_.address, config_.port);
    if (config_.require_auth) {
        spdlog::info("Authentication is required");
    }
}

nlohmann::json SSEServer::get_metrics() const { return metrics_.get_metrics(); }

void SSEServer::set_compression_enabled(bool enabled) {
    config_.enable_compression = enabled;
}

#ifdef USE_SSL
void SSEServer::configure_ssl() {
    if (!ssl_context_)
        return;

    ssl_context_->set_options(ssl::context::default_workarounds |
                              ssl::context::no_sslv2 |
                              ssl::context::single_dh_use);

    try {
        ssl_context_->use_certificate_chain_file(config_.cert_file);
        ssl_context_->use_private_key_file(config_.key_file, ssl::context::pem);

        spdlog::info("SSL configured with cert: {} and key: {}",
                     config_.cert_file, config_.key_file);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("SSL configuration error: {}", e.what());
        throw;
    }
}
#endif

void SSEServer::start_connection_monitor() {
    connection_monitor_timer_.expires_after(std::chrono::seconds(10));
    connection_monitor_timer_.async_wait([this](const error_code& ec) {
        if (!ec) {
            monitor_connections();
            start_connection_monitor();
        }
    });
}

void SSEServer::monitor_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    std::vector<SSEConnection::pointer> timed_out;
    for (const auto& conn : connections_) {
        if (conn->is_timed_out()) {
            timed_out.push_back(conn);
        }
    }

    for (auto& conn : timed_out) {
        spdlog::info("Closing timed out connection");
        conn->close();
    }

    clean_connections();
}

net::awaitable<void> SSEServer::accept_connections() {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            if (connections_.size() >=
                static_cast<size_t>(config_.max_connections)) {
                spdlog::warn(
                    "Connection limit reached ({}), waiting for slots to free "
                    "up",
                    config_.max_connections);
                co_await net::steady_timer(acceptor_.get_executor(),
                                           std::chrono::seconds(1))
                    .async_wait(net::use_awaitable);
                continue;
            }
        }

        auto [ec, socket] =
            co_await as_tuple_awaitable(acceptor_.async_accept());

        if (ec) {
            SPDLOG_ERROR("Accept error: {}", ec.message());
            continue;
        }

        SSEConnection::pointer connection;

#ifdef USE_SSL
        if (config_.enable_ssl && ssl_context_) {
            connection = SSEConnection::create(
                io_context_, *ssl_context_, event_queue_, event_store_,
                auth_service_, metrics_, config_);
            connection->socket().lowest_layer() = std::move(socket);
        } else {
            connection =
                SSEConnection::create(io_context_, event_queue_, event_store_,
                                      auth_service_, metrics_, config_);
            connection->socket() = std::move(socket);
        }
#else
        connection =
            SSEConnection::create(io_context_, event_queue_, event_store_,
                                  auth_service_, metrics_, config_);
        connection->socket() = std::move(socket);
#endif

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.push_back(connection);
        }

        connection->start();

        spdlog::info("New client connected. Total clients: {}",
                     connections_.size());
    }
}

void SSEServer::clean_connections() {
    auto now = std::chrono::steady_clock::now();

    if (now - last_cleanup_ < 5s) {
        return;
    }

    last_cleanup_ = now;

    std::lock_guard<std::mutex> lock(connections_mutex_);

    auto before_size = connections_.size();
    std::erase_if(connections_,
                  [](const auto& conn) { return !conn->is_connected(); });

    auto removed = before_size - connections_.size();
    if (removed > 0) {
        spdlog::info("Removed {} disconnected clients. Total clients: {}",
                     removed, connections_.size());
    }
}

std::string generate_id() {
    static std::atomic<uint64_t> counter(0);
    return std::to_string(counter++);
}

}  // namespace atom::extra::asio::sse
