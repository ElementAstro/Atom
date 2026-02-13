#pragma once

/**
 * @file event_queue.hpp
 * @brief Thread-safe event queue for broadcasting
 */

#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include "../event.hpp"
#include "event_store.hpp"

namespace atom::extra::asio::sse {

/**
 * @brief Thread-safe event queue for broadcasting events
 */
class EventQueue {
public:
    explicit EventQueue(ServerEventStore& event_store, bool persist_events);

    void push_event(Event event);
    bool has_events() const;
    std::optional<Event> pop_event();

private:
    std::queue<Event> events_;
    std::mutex mutex_;
    std::atomic<bool> event_available_{false};
    ServerEventStore& event_store_;
    bool persist_events_;
};

}  // namespace atom::extra::asio::sse
