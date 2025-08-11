#pragma once

/**
 * @file event_queue.hpp
 * @brief Thread-safe event queue for broadcasting
 */

#include "../event.hpp"
#include "event_store.hpp"
#include <atomic>
#include <mutex>
#include <optional>
#include <queue>

namespace atom::extra::asio::sse {

/**
 * @brief Thread-safe event queue for broadcasting events
 */
class EventQueue {
public:
    explicit EventQueue(EventStore& event_store, bool persist_events);

    void push_event(Event event);
    bool has_events() const;
    std::optional<Event> pop_event();

private:
    std::queue<Event> events_;
    std::mutex mutex_;
    std::atomic<bool> event_available_{false};
    EventStore& event_store_;
    bool persist_events_;
};

} // namespace sse_server
