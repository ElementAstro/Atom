#include "connection_pool.hpp"

namespace atom::extra::curl {

ConnectionPool::ConnectionPool(size_t max_connections)
    : max_connections_(max_connections) {
    spdlog::info("Initialized simplified connection pool with max_connections: {}", max_connections);

    // Pre-allocate some handles
    available_handles_.reserve(max_connections);
}

ConnectionPool::~ConnectionPool() {
    spdlog::info("Destroying connection pool, cleaning up {} connections", available_handles_.size());

    // Clean up all remaining connections
    std::lock_guard<std::mutex> lock(pool_mutex_);
    for (CURL* handle : available_handles_) {
        if (handle) {
            curl_easy_cleanup(handle);
            stats_.destroy_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    spdlog::info("Connection pool destroyed. Stats - Acquired: {}, Released: {}, Created: {}, Destroyed: {}",
                 stats_.acquire_count.load(), stats_.release_count.load(),
                 stats_.create_count.load(), stats_.destroy_count.load());
}

CURL* ConnectionPool::acquire() noexcept {
    stats_.acquire_count.fetch_add(1, std::memory_order_relaxed);

    // Try to get handle from pool
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (!available_handles_.empty()) {
            CURL* handle = available_handles_.back();
            available_handles_.pop_back();
            return handle;
        }
    }

    // Pool is empty, create new handle
    return createHandle();
}

void ConnectionPool::release(CURL* handle) noexcept {
    if (!handle) {
        return;
    }

    stats_.release_count.fetch_add(1, std::memory_order_relaxed);

    // Reset the handle to clean state
    curl_easy_reset(handle);

    // Return to pool if there's space
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (available_handles_.size() < max_connections_) {
            available_handles_.push_back(handle);
            return;
        }
    }

    // Pool is full, destroy handle
    curl_easy_cleanup(handle);
    stats_.destroy_count.fetch_add(1, std::memory_order_relaxed);
}

size_t ConnectionPool::size() const noexcept {
    // Return approximate size without locking for performance
    return available_handles_.size();
}

CURL* ConnectionPool::createHandle() noexcept {
    CURL* handle = curl_easy_init();
    if (handle) {
        stats_.create_count.fetch_add(1, std::memory_order_relaxed);
    }
    return handle;
}

void ConnectionPool::destroyHandle(CURL* handle) noexcept {
    if (handle) {
        curl_easy_cleanup(handle);
        stats_.destroy_count.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace atom::extra::curl
