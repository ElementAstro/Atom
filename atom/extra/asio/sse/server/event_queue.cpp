#include "event_queue.hpp"
#include <spdlog/spdlog.h>

namespace atom::extra::asio::sse {

EventQueue::EventQueue(EventStore& event_store, bool persist_events)
    : event_store_(event_store)
    , persist_events_(persist_events)
    , perf_monitor_(concurrency::performance_monitor::instance()) {

    spdlog::info("High-performance SSE event queue initialized with lock-free mechanisms");
}

void EventQueue::push_event(Event event) {
    ATOM_MEASURE_PERFORMANCE("sse_event_push");

    // Use lock-free queue for optimal performance
    events_.push(std::move(event));

    // Update performance counters
    total_processed_.get().fetch_add(1, std::memory_order_relaxed);

    // Handle persistence asynchronously for better performance
    if (persist_events_) {
        // Submit to work-stealing thread pool for optimal performance
        auto& concurrency_mgr = concurrency::get_concurrency_manager();
        concurrency_mgr.submit_monitored("sse_event_persist", [this, event = events_.try_pop()]() {
            if (event) {
                try {
                    event_store_.store_event(event.value());
                    spdlog::trace("SSE event persisted successfully");
                } catch (const std::exception& e) {
                    spdlog::error("Failed to persist SSE event: {}", e.what());
                    total_dropped_.get().fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    spdlog::trace("SSE event pushed to lock-free queue, total processed: {}",
                 total_processed_.get().load(std::memory_order_relaxed));
}

bool EventQueue::has_events() const noexcept {
    return !events_.empty();
}

std::optional<Event> EventQueue::pop_event() {
    ATOM_MEASURE_PERFORMANCE("sse_event_pop");

    auto event = events_.try_pop();
    if (event) {
        spdlog::trace("SSE event popped from lock-free queue");
    }

    return event;
}

EventQueue::QueueStats EventQueue::get_stats() const noexcept {
    return {
        .pending_events = events_.size(),
        .total_processed = total_processed_.get().load(std::memory_order_relaxed),
        .total_dropped = total_dropped_.get().load(std::memory_order_relaxed)
    };
}

}  // namespace atom::extra::asio::sse
