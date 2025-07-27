#include "server.hpp"
#include <spdlog/spdlog.h>
#include <atomic>

using namespace std::chrono_literals;

namespace atom::extra::asio::sse {

// Namespace alias for concurrency primitives
namespace concurrency = atom::extra::asio::concurrency;

SSEServer::SSEServer(net::io_context& io_context, const ServerConfig& config)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(net::ip::make_address(config.address),
                                          config.port)),
      event_queue_(event_store_, config.persist_events),
      event_store_(config.event_store_path, config.max_event_history),
      auth_service_(config.auth_file),
      metrics_(),
      config_(config),
      last_cleanup_(std::chrono::steady_clock::now()),
      connection_monitor_timer_(io_context),
      perf_monitor_(concurrency::performance_monitor::instance()) {
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

    spdlog::info("Advanced SSE Server started on {}:{} with cutting-edge concurrency",
                 config_.address, config_.port);
    if (config_.require_auth) {
        spdlog::info("Authentication is required");
    }

    // Log performance capabilities
    spdlog::info("SSE Server features: lock-free queues, work-stealing thread pool, "
                 "adaptive synchronization, real-time monitoring");
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
    ATOM_MEASURE_PERFORMANCE("sse_monitor_connections");

    // Process cleanup queue first
    while (auto conn = cleanup_connections_.try_pop()) {
        spdlog::debug("Cleaning up SSE connection");
        connection_count_.get().fetch_sub(1, std::memory_order_relaxed);
    }

    // Check active connections for timeouts
    // Note: In a full implementation, we'd need a way to iterate through active connections
    // For now, we'll rely on connections self-reporting timeouts

    auto current_count = connection_count_.get().load(std::memory_order_relaxed);
    spdlog::trace("SSE server monitoring {} active connections", current_count);
}

net::awaitable<void> SSEServer::accept_connections() {
    for (;;) {
        // Check connection limit using lock-free counter
        auto current_count = connection_count_.get().load(std::memory_order_relaxed);
        if (current_count >= static_cast<std::size_t>(config_.max_connections)) {
            spdlog::warn(
                "Connection limit reached ({}), waiting for slots to free up",
                config_.max_connections);
            co_await net::steady_timer(acceptor_.get_executor(),
                                       std::chrono::seconds(1))
                .async_wait(net::use_awaitable);
            continue;
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

        // Add connection to lock-free queue
        active_connections_.push(connection);
        auto new_count = connection_count_.get().fetch_add(1, std::memory_order_relaxed) + 1;

        connection->start();

        spdlog::info("New SSE client connected. Total clients: {}", new_count);
    }
}

void SSEServer::clean_connections() {
    ATOM_MEASURE_PERFORMANCE("sse_clean_connections");

    auto now = std::chrono::steady_clock::now();

    if (now - last_cleanup_ < 5s) {
        return;
    }

    last_cleanup_ = now;

    // Process cleanup queue - connections are added here when they disconnect
    std::size_t removed = 0;
    while (auto conn = cleanup_connections_.try_pop()) {
        removed++;
        connection_count_.get().fetch_sub(1, std::memory_order_relaxed);
    }

    if (removed > 0) {
        auto current_count = connection_count_.get().load(std::memory_order_relaxed);
        spdlog::info("Cleaned up {} disconnected SSE clients. Active clients: {}",
                     removed, current_count);
    }
}

std::string generate_id() {
    static std::atomic<uint64_t> counter(0);
    return std::to_string(counter++);
}

}  // namespace atom::extra::asio::sse
