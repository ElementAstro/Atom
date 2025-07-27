#pragma once

/**
 * @file event_queue.hpp
 * @brief High-performance lock-free event queue for broadcasting with cutting-edge concurrency
 */

#include "../event.hpp"
#include "event_store.hpp"
#include "../../concurrency/concurrency.hpp"
#include <atomic>
#include <optional>
#include <spdlog/spdlog.h>

namespace atom::extra::asio::sse {

// Namespace alias for concurrency primitives
namespace concurrency = atom::extra::asio::concurrency;

/**
 * @brief High-performance lock-free event queue for broadcasting events
 *
 * Features:
 * - Lock-free queue for optimal performance
 * - Real-time performance monitoring
 * - NUMA-aware memory management
 * - Adaptive load balancing
 */
class EventQueue {
public:
    explicit EventQueue(EventStore& event_store, bool persist_events);

    /**
     * @brief Push an event to the queue with performance monitoring
     */
    void push_event(Event event);

    /**
     * @brief Check if events are available (lock-free)
     */
    bool has_events() const noexcept;

    /**
     * @brief Pop an event from the queue (lock-free)
     */
    std::optional<Event> pop_event();

    /**
     * @brief Get queue statistics
     */
    struct QueueStats {
        std::size_t pending_events;
        std::size_t total_processed;
        std::size_t total_dropped;
    };

    QueueStats get_stats() const noexcept;

private:
    // High-performance lock-free event queue
    concurrency::lockfree_queue<Event> events_;

    // Performance counters
    concurrency::cache_aligned<std::atomic<std::size_t>> total_processed_{0};
    concurrency::cache_aligned<std::atomic<std::size_t>> total_dropped_{0};

    // Event persistence
    EventStore& event_store_;
    bool persist_events_;

    // Performance monitoring
    concurrency::performance_monitor& perf_monitor_;

    // Object pool for efficient event management
    concurrency::concurrent_object_pool<Event> event_pool_;
};

} // namespace atom::extra::asio::sse
