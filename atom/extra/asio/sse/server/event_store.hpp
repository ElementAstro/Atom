#pragma once

/**
 * @file event_store.hpp
 * @brief Persistent event storage
 */

#include <deque>
#include <limits>
#include <shared_mutex>
#include <string>
#include <vector>
#include "../event.hpp"

namespace atom::extra::asio::sse {

/**
 * @brief Persistent event storage with in-memory caching.
 *
 * The EventStore class provides mechanisms for storing, retrieving,
 * and persisting server-sent events. It maintains an in-memory cache
 * of recent events and supports persistence to disk for durability.
 * Thread-safe access is ensured for concurrent operations.
 */
class EventStore {
public:
    /**
     * @brief Construct an EventStore.
     * @param store_path Directory path for storing persisted events.
     * @param max_events Maximum number of events to keep in memory (default:
     * 1000).
     *
     * Initializes the event store, loads existing events from disk if
     * available, and sets the maximum number of events to retain in memory.
     */
    explicit EventStore(const std::string& store_path,
                        size_t max_events = 1000);

    /**
     * @brief Store a new event in the event store.
     * @param event The event to store.
     *
     * Adds the event to the in-memory cache and persists it to disk.
     * If the cache exceeds max_events, the oldest event is removed.
     */
    void store_event(const Event& event);

    /**
     * @brief Retrieve a list of recent events.
     * @param limit Maximum number of events to return (default: all).
     * @param event_type Optional filter for event type (default: all types).
     * @return Vector of events matching the criteria, ordered from oldest to
     * newest.
     *
     * Returns up to @p limit events, optionally filtered by @p event_type.
     */
    std::vector<Event> get_events(
        size_t limit = std::numeric_limits<size_t>::max(),
        const std::string& event_type = "") const;

    /**
     * @brief Retrieve events that occurred since a given timestamp.
     * @param timestamp Only events with a timestamp greater than this value are
     * returned.
     * @param event_type Optional filter for event type (default: all types).
     * @return Vector of events matching the criteria, ordered from oldest to
     * newest.
     *
     * Returns all events newer than @p timestamp, optionally filtered by @p
     * event_type.
     */
    std::vector<Event> get_events_since(
        uint64_t timestamp, const std::string& event_type = "") const;

    /**
     * @brief Clear all events from the store.
     *
     * Removes all events from the in-memory cache and deletes persisted events
     * from disk.
     */
    void clear();

private:
    /**
     * @brief Directory path for storing persisted events.
     */
    std::string store_path_;

    /**
     * @brief Maximum number of events to keep in memory.
     */
    size_t max_events_;

    /**
     * @brief In-memory cache of recent events.
     */
    std::deque<Event> events_;

    /**
     * @brief Mutex for thread-safe access to the event store.
     */
    mutable std::shared_mutex mutex_;

    /**
     * @brief Load events from persistent storage into memory.
     *
     * Reads events from disk and populates the in-memory cache.
     */
    void load_events();

    /**
     * @brief Persist a single event to disk.
     * @param event The event to persist.
     *
     * Writes the event to the persistent storage for durability.
     */
    void persist_event(const Event& event);
};

}  // namespace atom::extra::asio::sse
