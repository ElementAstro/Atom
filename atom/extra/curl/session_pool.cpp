#include "session_pool.hpp"
#include "session.hpp"
#include <algorithm>

namespace atom::extra::curl {

SessionPool::SessionPool(const Config& config) : config_(config) {
    spdlog::info("Initializing simplified session pool with max_pool_size: {}, timeout: {}s",
                 config_.max_pool_size, config_.timeout.count());

    // Pre-allocate some sessions
    available_sessions_.reserve(config_.max_pool_size);
}

SessionPool::~SessionPool() {
    spdlog::info("Destroying session pool. Stats - Acquired: {}, Released: {}, Created: {}, Cache hits: {}",
                 stats_.acquire_count.load(), stats_.release_count.load(),
                 stats_.create_count.load(), stats_.cache_hits.load());
}

std::shared_ptr<Session> SessionPool::acquire() {
    stats_.acquire_count.fetch_add(1, std::memory_order_relaxed);

    // Try to get session from pool
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (!available_sessions_.empty()) {
            auto session = available_sessions_.back();
            available_sessions_.pop_back();
            stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
            return session;
        }
    }

    // Pool miss - create new session
    stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
    return createSession();
}

void SessionPool::release(std::shared_ptr<Session> session) {
    if (!session) {
        return;
    }

    stats_.release_count.fetch_add(1, std::memory_order_relaxed);

    // Session will be reused as-is (reset is private)

    // Return to pool if there's space
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (available_sessions_.size() < config_.max_pool_size) {
            available_sessions_.push_back(session);
            return;
        }
    }

    // Pool is full, session will be destroyed automatically
    stats_.contention_count.fetch_add(1, std::memory_order_relaxed);
}

size_t SessionPool::size() const noexcept {
    // Return approximate size without locking for performance
    return available_sessions_.size();
}

std::shared_ptr<Session> SessionPool::createSession() {
    stats_.create_count.fetch_add(1, std::memory_order_relaxed);
    return std::make_shared<Session>();
}

}  // namespace atom::extra::curl
