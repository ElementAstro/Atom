#pragma once

/**
 * @file metrics.hpp
 * @brief Server metrics tracking
 */

#include <atomic>
#include <chrono>
#include "atom/type/json.hpp"


namespace atom::extra::asio::sse {

/**
 * @brief Server metrics collector.
 *
 * The ServerMetrics class tracks various runtime statistics for the SSE server,
 * such as connection counts, event counts, authentication results, and data
 * throughput. It provides thread-safe methods for updating and retrieving
 * metrics, and can export the current metrics as a JSON object for monitoring
 * or diagnostics.
 */
class ServerMetrics {
public:
    /**
     * @brief Increment the current and total connection counters.
     *
     * Should be called when a new client connection is established.
     */
    void increment_connection_count();

    /**
     * @brief Decrement the current connection counter.
     *
     * Should be called when a client connection is closed.
     */
    void decrement_connection_count();

    /**
     * @brief Increment the total event counter.
     *
     * Should be called when a new event is sent or processed.
     */
    void increment_event_count();

    /**
     * @brief Record the size of an event sent to clients.
     * @param size_bytes The size of the event in bytes.
     *
     * Adds the specified number of bytes to the total bytes sent counter.
     */
    void record_event_size(size_t size_bytes);

    /**
     * @brief Record an authentication failure.
     *
     * Should be called when a client fails authentication.
     */
    void record_auth_failure();

    /**
     * @brief Record an authentication success.
     *
     * Should be called when a client successfully authenticates.
     */
    void record_auth_success();

    /**
     * @brief Retrieve the current server metrics as a JSON object.
     * @return A nlohmann::json object containing all tracked metrics.
     *
     * The JSON includes connection counts, event counts, authentication stats,
     * total bytes sent, and server uptime.
     */
    nlohmann::json get_metrics() const;

private:
    /**
     * @brief Total number of client connections since server start.
     */
    std::atomic<uint64_t> total_connections_{0};

    /**
     * @brief Current number of active client connections.
     */
    std::atomic<uint64_t> current_connections_{0};

    /**
     * @brief Maximum number of concurrent client connections observed.
     */
    std::atomic<uint64_t> max_concurrent_connections_{0};

    /**
     * @brief Total number of events sent or processed.
     */
    std::atomic<uint64_t> total_events_{0};

    /**
     * @brief Total number of bytes sent to clients.
     */
    std::atomic<uint64_t> total_bytes_sent_{0};

    /**
     * @brief Total number of successful authentications.
     */
    std::atomic<uint64_t> auth_successes_{0};

    /**
     * @brief Total number of failed authentication attempts.
     */
    std::atomic<uint64_t> auth_failures_{0};

    /**
     * @brief Server start time for calculating uptime.
     */
    std::chrono::steady_clock::time_point start_time_{
        std::chrono::steady_clock::now()};

    /**
     * @brief Update the maximum concurrent connections counter if needed.
     *
     * Called internally whenever the current connection count changes.
     */
    void update_max_concurrent();
};

}  // namespace atom::extra::asio::sse