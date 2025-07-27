#ifndef ATOM_EXTRA_CURL_CONNECTION_POOL_HPP
#define ATOM_EXTRA_CURL_CONNECTION_POOL_HPP

#include <curl/curl.h>
#include <atomic>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>

namespace atom::extra::curl {

/**
 * @brief Simplified connection pool for CURL handles
 *
 * This provides a thread-safe pool of CURL handles using standard containers
 * and mutexes. The complex lock-free implementation has been removed in favor
 * of simplicity and maintainability.
 */
class ConnectionPool {

public:
    /**
     * @brief Constructor for connection pool
     * @param max_connections Maximum number of connections to maintain
     */
    explicit ConnectionPool(size_t max_connections = 10);

    /**
     * @brief Destructor - safely cleans up all connections
     */
    ~ConnectionPool();

    /**
     * @brief Acquire a CURL handle from the pool
     * @return CURL handle or nullptr if pool is empty
     */
    CURL* acquire() noexcept;

    /**
     * @brief Release a CURL handle back to the pool
     * @param handle CURL handle to return to pool
     */
    void release(CURL* handle) noexcept;

    /**
     * @brief Get current pool size
     * @return Current number of available connections
     */
    size_t size() const noexcept;

    /**
     * @brief Get pool statistics
     */
    struct Statistics {
        std::atomic<uint64_t> acquire_count{0};
        std::atomic<uint64_t> release_count{0};
        std::atomic<uint64_t> create_count{0};
        std::atomic<uint64_t> destroy_count{0};
        std::atomic<uint64_t> contention_count{0};
    };

    const Statistics& getStatistics() const noexcept { return stats_; }

private:
    // Simplified implementation using standard containers
    std::vector<CURL*> available_handles_;
    std::mutex pool_mutex_;
    const size_t max_connections_;
    mutable Statistics stats_;

    /**
     * @brief Create a new CURL handle
     */
    CURL* createHandle() noexcept;

    /**
     * @brief Destroy a CURL handle
     */
    void destroyHandle(CURL* handle) noexcept;
};

}  // namespace atom::extra::curl

#endif
