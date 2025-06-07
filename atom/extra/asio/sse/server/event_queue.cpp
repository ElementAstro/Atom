#include "event_queue.hpp"

namespace atom::extra::asio::sse {

EventQueue::EventQueue(EventStore& event_store, bool persist_events)
    : event_store_(event_store), persist_events_(persist_events) {}

void EventQueue::push_event(Event event) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push(std::move(event));
    event_available_.store(true);

    if (persist_events_) {
        event_store_.store_event(events_.back());
    }
}

bool EventQueue::has_events() const { return event_available_.load(); }

std::optional<Event> EventQueue::pop_event() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.empty()) {
        event_available_.store(false);
        return std::nullopt;
    }

    Event event = std::move(events_.front());
    events_.pop();
    event_available_.store(!events_.empty());
    return event;
}

}  // namespace atom::extra::asio::sse