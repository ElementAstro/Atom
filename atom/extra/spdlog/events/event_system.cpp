#include "event_system.h"

#include <algorithm>
#include <mutex>

namespace modern_log {

LogEventSystem::EventId LogEventSystem::subscribe(LogEvent event,
                                                  EventCallback callback) {
    std::unique_lock lock(mutex_);
    EventId id = next_id_.fetch_add(1);
    callbacks_[event].emplace_back(id, std::move(callback));
    return id;
}

bool LogEventSystem::unsubscribe(LogEvent event, EventId event_id) {
    std::unique_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        auto& callbacks = it->second;
        auto callback_it = std::ranges::find_if(
            callbacks,
            [event_id](const auto& pair) { return pair.first == event_id; });

        if (callback_it != callbacks.end()) {
            callbacks.erase(callback_it);
            return true;
        }
    }

    return false;
}

void LogEventSystem::emit(LogEvent event, const std::any& data) {
    std::shared_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        for (const auto& [id, callback] : it->second) {
            try {
                callback(event, data);
            } catch (...) {
            }
        }
    }
}

size_t LogEventSystem::subscriber_count(LogEvent event) const {
    std::shared_lock lock(mutex_);

    if (auto it = callbacks_.find(event); it != callbacks_.end()) {
        return it->second.size();
    }

    return 0;
}

void LogEventSystem::clear_all_subscriptions() {
    std::unique_lock lock(mutex_);
    callbacks_.clear();
}

}  // namespace modern_log
