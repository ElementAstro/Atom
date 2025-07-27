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
 * @brief High-performance event system for logging with optimized callback management.
 *
 * This class implements a thread-safe event system for logging, allowing
 * components to subscribe to, unsubscribe from, and emit log-related events.
 * Subscribers can register callbacks for specific LogEvent types and receive
 * event data via std::any. Each subscription is assigned a unique ID for later
 * removal. The system supports querying the number of subscribers for a given
 * event and clearing all subscriptions.
 *
 * Performance optimizations:
 * - Pre-allocated callback vectors to reduce allocations
 * - Fast path for events with no subscribers
 * - Optimized callback storage and invocation
 * - Reduced memory allocations during event emission
 * - Lock-free fast path for common operations
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

    /**
     * @brief Optimized callback storage structure.
     */
    struct CallbackEntry {
        EventId id;
        EventCallback callback;
        bool active = true;  ///< Whether this callback is active

        CallbackEntry(EventId id, EventCallback cb)
            : id(id), callback(std::move(cb)) {}
    };

private:
    std::unordered_map<LogEvent, std::vector<CallbackEntry>>
        callbacks_;  ///< Map of event type to list of callback entries.
    mutable std::shared_mutex
        mutex_;  ///< Mutex for thread-safe access to the callback map.
    std::atomic<EventId> next_id_{
        1};  ///< Counter for generating unique subscription IDs.

    // Performance optimization fields
    std::atomic<size_t> total_subscribers_{0};  ///< Total number of active subscribers
    mutable std::atomic<size_t> events_emitted_{0};  ///< Statistics counter
    mutable std::atomic<size_t> callbacks_invoked_{0};  ///< Statistics counter

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
     * @brief Emit (publish) a log event to all subscribers (optimized).
     *
     * Invokes all registered callbacks for the specified event, passing the
     * provided data. Uses fast path when no subscribers exist.
     *
     * @param event The LogEvent type to emit.
     * @param data Optional event data (default: empty std::any).
     */
    void emit(LogEvent event, const std::any& data = {});

    /**
     * @brief Fast emit without data payload (optimized for common case).
     *
     * @param event The LogEvent type to emit.
     */
    void emit_fast(LogEvent event);

    /**
     * @brief Emit event with string data (optimized).
     *
     * @param event The LogEvent type to emit.
     * @param message String message to emit.
     */
    void emit_string(LogEvent event, std::string_view message);

    /**
     * @brief Get the number of subscribers for a specific event.
     *
     * @param event The LogEvent type to query.
     * @return The number of registered subscribers for the event.
     */
    size_t subscriber_count(LogEvent event) const;

    /**
     * @brief Get total number of active subscribers across all events.
     *
     * @return Total number of active subscribers.
     */
    size_t total_subscriber_count() const;

    /**
     * @brief Clear all event subscriptions.
     *
     * Removes all registered callbacks for all event types.
     */
    void clear_all_subscriptions();

    /**
     * @brief Get event system statistics.
     *
     * @return Pair of (events_emitted, callbacks_invoked).
     */
    std::pair<size_t, size_t> get_stats() const;

    /**
     * @brief Reset event system statistics.
     */
    void reset_stats();

private:
    /**
     * @brief Cleanup inactive callback entries.
     * @param event The event type to cleanup.
     */
    void cleanup_callbacks(LogEvent event);

    /**
     * @brief Check if any subscribers exist for an event (fast check).
     * @param event The event type to check.
     * @return True if subscribers exist.
     */
    bool has_subscribers_fast(LogEvent event) const;
};

}  // namespace modern_log
