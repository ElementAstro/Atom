#pragma once

#include <any>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include "../core/types.h"


namespace modern_log {

/**
 * @class LogEventSystem
 * @brief Event system for logging: provides event subscription and publishing
 * mechanisms.
 *
 * This class implements a thread-safe event system for logging, allowing
 * components to subscribe to, unsubscribe from, and emit log-related events.
 * Subscribers can register callbacks for specific LogEvent types and receive
 * event data via std::any. Each subscription is assigned a unique ID for later
 * removal. The system supports querying the number of subscribers for a given
 * event and clearing all subscriptions.
 */
class LogEventSystem {
public:
    /**
     * @brief Type alias for event callback functions.
     *
     * The callback receives the LogEvent type and an associated data payload
     * (std::any).
     */
    using EventCallback = std::function<void(LogEvent, const std::any&)>;

    /**
     * @brief Type alias for unique event subscription IDs.
     */
    using EventId = size_t;

private:
    std::unordered_map<LogEvent, std::vector<std::pair<EventId, EventCallback>>>
        callbacks_;  ///< Map of event type to list of (ID, callback) pairs.
    mutable std::shared_mutex
        mutex_;  ///< Mutex for thread-safe access to the callback map.
    std::atomic<EventId> next_id_{
        1};  ///< Counter for generating unique subscription IDs.

public:
    /**
     * @brief Subscribe to a specific log event.
     *
     * Registers a callback to be invoked when the specified event is emitted.
     * Returns a unique EventId that can be used to unsubscribe later.
     *
     * @param event The LogEvent type to subscribe to.
     * @param callback The callback function to invoke when the event is
     * emitted.
     * @return EventId assigned to this subscription.
     */
    EventId subscribe(LogEvent event, EventCallback callback);

    /**
     * @brief Unsubscribe from a specific log event.
     *
     * Removes the callback associated with the given EventId for the specified
     * event.
     *
     * @param event The LogEvent type to unsubscribe from.
     * @param event_id The EventId returned by subscribe().
     * @return True if the subscription was found and removed, false otherwise.
     */
    bool unsubscribe(LogEvent event, EventId event_id);

    /**
     * @brief Emit (publish) a log event to all subscribers.
     *
     * Invokes all registered callbacks for the specified event, passing the
     * provided data.
     *
     * @param event The LogEvent type to emit.
     * @param data Optional event data (default: empty std::any).
     */
    void emit(LogEvent event, const std::any& data = {});

    /**
     * @brief Get the number of subscribers for a specific event.
     *
     * @param event The LogEvent type to query.
     * @return The number of registered subscribers for the event.
     */
    size_t subscriber_count(LogEvent event) const;

    /**
     * @brief Clear all event subscriptions.
     *
     * Removes all registered callbacks for all event types.
     */
    void clear_all_subscriptions();
};

}  // namespace modern_log
