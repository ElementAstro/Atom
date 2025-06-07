#ifndef ATOM_EXTRA_ASIO_SSE_EVENT_STORE_HPP
#define ATOM_EXTRA_ASIO_SSE_EVENT_STORE_HPP

/**
 * @file event_store.hpp
 * @brief Persistent storage management for events
 */

#include <mutex>
#include <string>
#include <unordered_set>
#include "event.hpp"

namespace atom::extra::asio::sse {

/**
 * @brief Manages persistent storage of events.
 *
 * This class provides thread-safe persistent storage for SSE events.
 * It ensures that each event is stored only once, supports querying
 * for previously seen events, and maintains the latest event ID for
 * resuming event streams. Events are stored as JSON files in a specified
 * directory, and the store automatically loads existing events on startup.
 */
class EventStore {
public:
    /**
     * @brief Constructs an EventStore with the given storage path.
     *
     * Creates the storage directory if it does not exist and loads
     * all existing events from disk into memory.
     *
     * @param store_path Path to the directory where events will be stored.
     */
    explicit EventStore(const std::string& store_path);

    /**
     * @brief Stores an event persistently if it has not been seen before.
     *
     * The event is serialized to JSON and written to a file in the storage
     * directory. Duplicate events (by ID) are ignored.
     *
     * @param event The event to store.
     */
    void store_event(const Event& event);

    /**
     * @brief Checks if an event with the given ID has already been stored.
     *
     * Thread-safe.
     *
     * @param event_id The event ID to check.
     * @return true if the event has been seen, false otherwise.
     */
    [[nodiscard]] bool has_seen_event(const std::string& event_id) const;

    /**
     * @brief Gets the ID of the latest (most recent) event stored.
     *
     * Thread-safe.
     *
     * @return The latest event ID, or an empty string if no events are stored.
     */
    [[nodiscard]] std::string get_latest_event_id() const;

private:
    /**
     * @brief Loads all existing events from the storage directory.
     *
     * Populates the set of seen event IDs and updates the cached latest event
     * ID and timestamp. Called automatically during construction.
     */
    void load_existing_events();

    std::string store_path_;  ///< Directory path for event storage.
    std::unordered_set<std::string>
        event_ids_;                         ///< Set of all stored event IDs.
    mutable std::string cached_latest_id_;  ///< Cached latest event ID.
    mutable uint64_t cached_latest_timestamp_ =
        0;                      ///< Cached latest event timestamp.
    mutable std::mutex mutex_;  ///< Mutex for thread safety.
};

}  // namespace atom::extra::asio::sse

#endif  // ATOM_EXTRA_ASIO_SSE_EVENT_STORE_HPP