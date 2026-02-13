#ifndef ATOM_EXTRA_BEAST_CONNECTION_POOL_HPP
#define ATOM_EXTRA_BEAST_CONNECTION_POOL_HPP

#include "concurrency_primitives.hpp"
#include "lock_free_queue.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace atom::beast::pool {

namespace net = boost::asio;
namespace beast = boost::beast;
using tcp = net::ip::tcp;

/**
 * @brief High-performance connection with advanced lifecycle management
 */
class PooledConnection {
public:
    enum class State : std::uint8_t {
        IDLE = 0,
        IN_USE = 1,
        CONNECTING = 2,
        ERROR = 3,
        CLOSED = 4
    };

private:
    std::unique_ptr<beast::tcp_stream> stream_;
    std::atomic<State> state_{State::IDLE};
    std::atomic<std::chrono::steady_clock::time_point> last_used_;
    std::atomic<std::chrono::steady_clock::time_point> created_at_;
    std::atomic<std::size_t> use_count_{0};
    std::string host_;
    std::string port_;
    std::chrono::seconds timeout_;

public:
    explicit PooledConnection(net::io_context& ioc,
                             std::string_view host,
                             std::string_view port,
                             std::chrono::seconds timeout = std::chrono::seconds{30});

    ~PooledConnection();

    /**
     * @brief Attempts to acquire the connection for exclusive use
     */
    [[nodiscard]] bool try_acquire() noexcept;

    /**
     * @brief Releases the connection back to idle state
     */
    void release() noexcept;

    /**
     * @brief Connects to the target host if not already connected
     */
    void connect();

    /**
     * @brief Closes the connection
     */
    void close() noexcept;

    /**
     * @brief Returns the underlying stream
     */
    [[nodiscard]] beast::tcp_stream& stream() noexcept { return *stream_; }

    /**
     * @brief Checks if connection is healthy and usable
     */
    [[nodiscard]] bool is_healthy() const noexcept;

    /**
     * @brief Returns connection statistics
     */
    struct Statistics {
        State state;
        std::chrono::seconds age;
        std::chrono::seconds idle_time;
        std::size_t use_count;
        std::string endpoint;
    };

    [[nodiscard]] Statistics get_statistics() const noexcept;

    [[nodiscard]] const std::string& host() const noexcept { return host_; }
    [[nodiscard]] const std::string& port() const noexcept { return port_; }
    [[nodiscard]] State state() const noexcept { return state_.load(std::memory_order_acquire); }
};

/**
 * @brief Lock-free connection pool with advanced load balancing
 */
class LockFreeConnectionPool {
private:
    using ConnectionPtr = std::shared_ptr<PooledConnection>;
    using ConnectionQueue = concurrency::LockFreeMPMCQueue<ConnectionPtr>;

    struct PoolKey {
        std::string host;
        std::string port;

        bool operator==(const PoolKey& other) const noexcept {
            return host == other.host && port == other.port;
        }
    };

    struct PoolKeyHash {
        std::size_t operator()(const PoolKey& key) const noexcept {
            std::size_t h1 = std::hash<std::string>{}(key.host);
            std::size_t h2 = std::hash<std::string>{}(key.port);
            return h1 ^ (h2 << 1);
        }
    };

    net::io_context& ioc_;
    std::unordered_map<PoolKey, std::unique_ptr<ConnectionQueue>, PoolKeyHash> pools_;
    concurrency::AdaptiveSpinLock pools_mutex_;

    // Pool configuration
    std::atomic<std::size_t> max_connections_per_host_{20};
    std::atomic<std::size_t> max_idle_time_seconds_{300};
    std::atomic<std::size_t> connection_timeout_seconds_{30};

    // Statistics
    std::atomic<std::size_t> total_connections_{0};
    std::atomic<std::size_t> active_connections_{0};
    std::atomic<std::size_t> pool_hits_{0};
    std::atomic<std::size_t> pool_misses_{0};

    // Cleanup timer
    std::unique_ptr<net::steady_timer> cleanup_timer_;
    std::chrono::seconds cleanup_interval_{60};

public:
    explicit LockFreeConnectionPool(net::io_context& ioc);

    ~LockFreeConnectionPool();

    /**
     * @brief Acquires a connection from the pool or creates a new one
     */
    [[nodiscard]] ConnectionPtr acquire_connection(std::string_view host,
                                                  std::string_view port);

    /**
     * @brief Returns a connection to the pool
     */
    void release_connection(ConnectionPtr conn);

    /**
     * @brief Configuration methods
     */
    void set_max_connections_per_host(std::size_t max_conn) noexcept {
        max_connections_per_host_.store(max_conn, std::memory_order_relaxed);
    }

    void set_max_idle_time(std::chrono::seconds idle_time) noexcept {
        max_idle_time_seconds_.store(idle_time.count(), std::memory_order_relaxed);
    }

    void set_connection_timeout(std::chrono::seconds timeout) noexcept {
        connection_timeout_seconds_.store(timeout.count(), std::memory_order_relaxed);
    }

    /**
     * @brief Returns pool statistics
     */
    struct PoolStatistics {
        std::size_t total_connections;
        std::size_t active_connections;
        std::size_t pool_hits;
        std::size_t pool_misses;
        double hit_ratio;
        std::size_t pool_count;
    };

    [[nodiscard]] PoolStatistics get_statistics() const noexcept;

private:
    ConnectionQueue* get_or_create_pool(const PoolKey& key);
    void start_cleanup_timer();
    void cleanup_idle_connections();
    void cleanup_all_connections();
};

} // namespace atom::beast::pool

#endif // ATOM_EXTRA_BEAST_CONNECTION_POOL_HPP
