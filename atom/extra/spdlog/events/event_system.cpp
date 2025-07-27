#include "event_system.h"

#include <algorithm>
#include <mutex>

namespace modern_log {

LogEventSystem::EventId LogEventSystem::subscribe(LogEvent event,
                                                  EventCallback callback) {
    std::unique_lock lock(mutex_);
    EventId id = next_id_.fetch_add(1);
    callbacks_[event].emplace_back(id, std::move(callback));
    total_subscribers_.fetch_add(1);
    return id;
}

bool LogEventSystem::unsubscribe(LogEvent event, EventId event_id) {
    std::unique_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        auto& callbacks = it->second;
        auto callback_it = std::ranges::find_if(
            callbacks,
            [event_id](const auto& entry) { return entry.id == event_id && entry.active; });

        if (callback_it != callbacks.end()) {
            callback_it->active = false;  // Mark as inactive instead of erasing
            total_subscribers_.fetch_sub(1);
            return true;
        }
    }

    return false;
}

void LogEventSystem::emit(LogEvent event, const std::any& data) {
    // Fast path: check if any subscribers exist
    if (!has_subscribers_fast(event)) {
        return;
    }

    events_emitted_.fetch_add(1);
    std::shared_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        size_t callbacks_called = 0;
        for (const auto& entry : it->second) {
            if (entry.active) {
                try {
                    entry.callback(event, data);
                    callbacks_called++;
                } catch (...) {
                    // Silently ignore callback exceptions
                }
            }
        }
        callbacks_invoked_.fetch_add(callbacks_called);

        // Cleanup inactive callbacks periodically
        if (callbacks_called * 2 < it->second.size()) {
            lock.unlock();
            cleanup_callbacks(event);
        }
    }
}

size_t LogEventSystem::subscriber_count(LogEvent event) const {
    std::shared_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        size_t count = 0;
        for (const auto& entry : it->second) {
            if (entry.active) {
                count++;
            }
        }
        return count;
    }

    return 0;
}

size_t LogEventSystem::total_subscriber_count() const {
    return total_subscribers_.load();
}

void LogEventSystem::emit_fast(LogEvent event) {
    emit(event, std::any{});
}

void LogEventSystem::emit_string(LogEvent event, std::string_view message) {
    emit(event, std::string(message));
}

std::pair<size_t, size_t> LogEventSystem::get_stats() const {
    return {events_emitted_.load(), callbacks_invoked_.load()};
}

void LogEventSystem::reset_stats() {
    events_emitted_.store(0);
    callbacks_invoked_.store(0);
}

void LogEventSystem::clear_all_subscriptions() {
    std::unique_lock lock(mutex_);
    callbacks_.clear();
    total_subscribers_.store(0);
}

bool LogEventSystem::has_subscribers_fast(LogEvent event) const {
    if (total_subscribers_.load() == 0) {
        return false;
    }

    std::shared_lock lock(mutex_);
    auto it = callbacks_.find(event);
    return it != callbacks_.end() && !it->second.empty();
}

void LogEventSystem::cleanup_callbacks(LogEvent event) {
    std::unique_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        auto& callbacks = it->second;
        auto new_end = std::remove_if(callbacks.begin(), callbacks.end(),
                                      [](const CallbackEntry& entry) {
                                          return !entry.active;
                                      });
        callbacks.erase(new_end, callbacks.end());
    }
}

}  // namespace modern_log
