#include "connection_pool.hpp"
#include <spdlog/spdlog.h>

namespace atom::beast::pool {

// PooledConnection implementations

PooledConnection::PooledConnection(net::io_context& ioc,
                                 std::string_view host,
                                 std::string_view port,
                                 std::chrono::seconds timeout)
    : stream_(std::make_unique<beast::tcp_stream>(net::make_strand(ioc)))
    , last_used_(std::chrono::steady_clock::now())
    , created_at_(std::chrono::steady_clock::now())
    , host_(host)
    , port_(port)
    , timeout_(timeout) {

    spdlog::debug("Created pooled connection for {}:{}", host_, port_);
}

PooledConnection::~PooledConnection() {
    close();
    spdlog::debug("Destroyed pooled connection for {}:{} (used {} times)",
                 host_, port_, use_count_.load(std::memory_order_relaxed));
}

bool PooledConnection::try_acquire() noexcept {
    State expected = State::IDLE;
    if (state_.compare_exchange_strong(expected, State::IN_USE,
                                      std::memory_order_acq_rel)) {
        last_used_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        use_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void PooledConnection::release() noexcept {
    State expected = State::IN_USE;
    if (state_.compare_exchange_strong(expected, State::IDLE,
                                      std::memory_order_acq_rel)) {
        last_used_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }
}

void PooledConnection::connect() {
    State expected = State::IDLE;
    if (!state_.compare_exchange_strong(expected, State::CONNECTING,
                                       std::memory_order_acq_rel)) {
        throw std::logic_error("Connection is not in idle state");
    }

    try {
        tcp::resolver resolver(stream_->get_executor());
        auto const results = resolver.resolve(host_, port_);

        stream_->expires_after(timeout_);
        stream_->connect(results);

        state_.store(State::IDLE, std::memory_order_release);
        spdlog::debug("Successfully connected to {}:{}", host_, port_);
    } catch (const std::exception& e) {
        state_.store(State::ERROR, std::memory_order_release);
        spdlog::error("Failed to connect to {}:{}: {}", host_, port_, e.what());
        throw;
    }
}

void PooledConnection::close() noexcept {
    state_.store(State::CLOSED, std::memory_order_release);
    if (stream_) {
        beast::error_code ec;
        stream_->socket().shutdown(tcp::socket::shutdown_both, ec);
        stream_->close();
    }
}

bool PooledConnection::is_healthy() const noexcept {
    auto current_state = state_.load(std::memory_order_acquire);
    if (current_state == State::ERROR || current_state == State::CLOSED) {
        return false;
    }

    // Check if connection has been idle too long
    auto now = std::chrono::steady_clock::now();
    auto last_use = last_used_.load(std::memory_order_acquire);
    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_use);

    return idle_time < std::chrono::seconds{300}; // 5 minutes max idle time
}

PooledConnection::Statistics PooledConnection::get_statistics() const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto created = created_at_.load(std::memory_order_acquire);
    auto last_use = last_used_.load(std::memory_order_acquire);

    return Statistics{
        state_.load(std::memory_order_acquire),
        std::chrono::duration_cast<std::chrono::seconds>(now - created),
        std::chrono::duration_cast<std::chrono::seconds>(now - last_use),
        use_count_.load(std::memory_order_relaxed),
        host_ + ":" + port_
    };
}

// LockFreeConnectionPool implementations

LockFreeConnectionPool::LockFreeConnectionPool(net::io_context& ioc)
    : ioc_(ioc)
    , cleanup_timer_(std::make_unique<net::steady_timer>(ioc)) {

    start_cleanup_timer();
    spdlog::info("Initialized lock-free connection pool");
}

LockFreeConnectionPool::~LockFreeConnectionPool() {
    if (cleanup_timer_) {
        cleanup_timer_->cancel();
    }
    cleanup_all_connections();
    spdlog::info("Destroyed connection pool with {} total connections created",
                total_connections_.load(std::memory_order_relaxed));
}

std::shared_ptr<PooledConnection> LockFreeConnectionPool::acquire_connection(std::string_view host,
                                                                            std::string_view port) {
    PoolKey key{std::string(host), std::string(port)};

    // Try to get connection from pool
    auto* queue = get_or_create_pool(key);
    if (queue) {
        ConnectionPtr conn;
        if (queue->try_dequeue(conn) && conn && conn->is_healthy()) {
            if (conn->try_acquire()) {
                pool_hits_.fetch_add(1, std::memory_order_relaxed);
                spdlog::debug("Reusing pooled connection for {}:{}", host, port);
                return conn;
            }
        }
    }

    // Create new connection
    pool_misses_.fetch_add(1, std::memory_order_relaxed);
    auto conn = std::make_shared<PooledConnection>(
        ioc_, host, port,
        std::chrono::seconds{connection_timeout_seconds_.load(std::memory_order_relaxed)});

    conn->connect();
    if (conn->try_acquire()) {
        total_connections_.fetch_add(1, std::memory_order_relaxed);
        active_connections_.fetch_add(1, std::memory_order_relaxed);
        spdlog::debug("Created new connection for {}:{}", host, port);
        return conn;
    }

    throw std::runtime_error("Failed to acquire newly created connection");
}

void LockFreeConnectionPool::release_connection(std::shared_ptr<PooledConnection> conn) {
    if (!conn) return;

    conn->release();
    active_connections_.fetch_sub(1, std::memory_order_relaxed);

    if (!conn->is_healthy()) {
        spdlog::debug("Discarding unhealthy connection for {}:{}",
                     conn->host(), conn->port());
        return;
    }

    PoolKey key{conn->host(), conn->port()};
    auto* queue = get_or_create_pool(key);
    if (queue) {
        queue->enqueue(std::move(conn));
        spdlog::debug("Returned connection to pool for {}:{}", key.host, key.port);
    }
}

LockFreeConnectionPool::PoolStatistics LockFreeConnectionPool::get_statistics() const noexcept {
    auto hits = pool_hits_.load(std::memory_order_relaxed);
    auto misses = pool_misses_.load(std::memory_order_relaxed);
    auto total_requests = hits + misses;

    return PoolStatistics{
        total_connections_.load(std::memory_order_relaxed),
        active_connections_.load(std::memory_order_relaxed),
        hits,
        misses,
        total_requests > 0 ? static_cast<double>(hits) / total_requests * 100.0 : 0.0,
        pools_.size()
    };
}

LockFreeConnectionPool::ConnectionQueue* LockFreeConnectionPool::get_or_create_pool(const PoolKey& key) {
    {
        std::lock_guard<concurrency::AdaptiveSpinLock> lock(pools_mutex_);
        auto it = pools_.find(key);
        if (it != pools_.end()) {
            return it->second.get();
        }
    }

    // Create new pool
    auto new_queue = std::make_unique<ConnectionQueue>();
    auto* queue_ptr = new_queue.get();

    {
        std::lock_guard<concurrency::AdaptiveSpinLock> lock(pools_mutex_);
        auto [it, inserted] = pools_.emplace(key, std::move(new_queue));
        return inserted ? queue_ptr : it->second.get();
    }
}

void LockFreeConnectionPool::start_cleanup_timer() {
    cleanup_timer_->expires_after(cleanup_interval_);
    cleanup_timer_->async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            cleanup_idle_connections();
            start_cleanup_timer();
        }
    });
}

void LockFreeConnectionPool::cleanup_idle_connections() {
    std::size_t cleaned = 0;
    auto max_idle = std::chrono::seconds{max_idle_time_seconds_.load(std::memory_order_relaxed)};

    std::lock_guard<concurrency::AdaptiveSpinLock> lock(pools_mutex_);
    for (auto& [key, queue] : pools_) {
        ConnectionPtr conn;
        while (queue->try_dequeue(conn)) {
            if (conn && conn->is_healthy()) {
                auto stats = conn->get_statistics();
                if (stats.idle_time < max_idle) {
                    queue->enqueue(std::move(conn));
                } else {
                    ++cleaned;
                }
            } else {
                ++cleaned;
            }
        }
    }

    if (cleaned > 0) {
        spdlog::debug("Cleaned up {} idle connections", cleaned);
    }
}

void LockFreeConnectionPool::cleanup_all_connections() {
    std::lock_guard<concurrency::AdaptiveSpinLock> lock(pools_mutex_);
    pools_.clear();
}

} // namespace atom::beast::pool
