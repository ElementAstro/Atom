#pragma once

/**
 * @file server.hpp
 * @brief Main SSE server implementation
 */

#include "../../asio_compatibility.hpp"
#include "../event.hpp"
#include "auth_service.hpp"
#include "connection.hpp"
#include "event_queue.hpp"
#include "event_store.hpp"
#include "metrics.hpp"
#include "server_config.hpp"

#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

namespace atom::extra::asio::sse {

/**
 * @brief Main SSE server with coroutine-based connection handling.
 *
 * The SSEServer class manages client connections, event broadcasting,
 * authentication, event storage, and server metrics. It uses coroutines
 * for efficient asynchronous connection handling and provides methods
 * for broadcasting events, retrieving metrics, and managing configuration.
 */
class SSEServer {
public:
    /**
     * @brief Construct the SSEServer.
     * @param io_context The ASIO I/O context for asynchronous operations.
     * @param config The server configuration parameters.
     *
     * Initializes the server, sets up event storage, authentication,
     * metrics, and prepares to accept client connections.
     */
    SSEServer(net::io_context& io_context, const ServerConfig& config);

    /**
     * @brief Broadcast an event to all connected clients.
     * @tparam E The event type (must satisfy EventType).
     * @param event The event object to broadcast.
     *
     * Pushes the event to the event queue and triggers connection cleanup.
     */
    template <EventType E>
    void broadcast_event(E&& event) {
        event_queue_.push_event(std::forward<E>(event));
        clean_connections();
    }

    /**
     * @brief Get server metrics.
     * @return A JSON object containing current server metrics.
     *
     * Returns statistics such as connection counts, event counts,
     * authentication results, and data throughput.
     */
    nlohmann::json get_metrics() const;

    /**
     * @brief Get current configuration.
     * @return Reference to the current ServerConfig object.
     */
    const ServerConfig& config() const { return config_; }

    /**
     * @brief Update compression setting.
     * @param enabled True to enable compression, false to disable.
     *
     * Dynamically enables or disables event data compression for clients.
     */
    void set_compression_enabled(bool enabled);

private:
    /**
     * @brief ASIO I/O context for asynchronous operations.
     */
    net::io_context& io_context_;

    /**
     * @brief TCP acceptor for incoming client connections.
     */
    tcp::acceptor acceptor_;

    /**
     * @brief List of active SSE client connections.
     */
    std::vector<SSEConnection::pointer> connections_;

    /**
     * @brief Mutex for thread-safe access to the connections list.
     */
    std::mutex connections_mutex_;

    /**
     * @brief Event queue for broadcasting events to clients.
     */
    EventQueue event_queue_;

    /**
     * @brief Persistent event storage.
     */
    EventStore event_store_;

    /**
     * @brief Authentication service for client validation.
     */
    AuthService auth_service_;

    /**
     * @brief Server metrics collector.
     */
    ServerMetrics metrics_;

    /**
     * @brief Server configuration parameters.
     */
    ServerConfig config_;

    /**
     * @brief Timestamp of the last connection cleanup.
     */
    std::chrono::steady_clock::time_point last_cleanup_;

    /**
     * @brief Timer for periodic connection monitoring.
     */
    net::steady_timer connection_monitor_timer_;

#ifdef USE_SSL
    /**
     * @brief SSL context for secure connections (if enabled).
     */
    std::unique_ptr<ssl_context> ssl_context_;

    /**
     * @brief Configure SSL context with certificates and keys.
     */
    void configure_ssl();
#endif

    /**
     * @brief Start the periodic connection monitor.
     *
     * Begins a timer to regularly check and clean up inactive connections.
     */
    void start_connection_monitor();

    /**
     * @brief Monitor and manage client connections.
     *
     * Checks for inactive or stale connections and removes them as needed.
     */
    void monitor_connections();

    /**
     * @brief Coroutine for accepting incoming client connections.
     *
     * Asynchronously accepts new connections and initializes SSE sessions.
     */
    net::awaitable<void> accept_connections();

    /**
     * @brief Clean up inactive or closed client connections.
     *
     * Removes connections that are no longer active from the internal list.
     */
    void clean_connections();
};

/**
 * @brief Helper to generate unique IDs.
 * @return A unique string identifier.
 *
 * Used for generating unique event or connection IDs.
 */
std::string generate_id();

}  // namespace atom::extra::asio::sse
