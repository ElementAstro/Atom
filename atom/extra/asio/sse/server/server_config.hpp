#pragma once

/**
 * @file server_config.hpp
 * @brief Server configuration management
 */

#include <cstdint>
#include <string>

namespace atom::extra::asio::sse {

/**
 * @brief Server configuration parameters
 *
 * This struct holds all configurable parameters for the SSE server,
 * including network settings, SSL options, authentication, event storage,
 * and connection management.
 */
struct ServerConfig {
    /**
     * @brief TCP port number the server listens on.
     * @details Default is 8080.
     */
    uint16_t port = 8080;

    /**
     * @brief IP address the server binds to.
     * @details Default is "0.0.0.0" (all interfaces).
     */
    std::string address = "0.0.0.0";

    /**
     * @brief Enable SSL/TLS for secure connections.
     * @details Default is false.
     */
    bool enable_ssl = false;

    /**
     * @brief Path to the SSL certificate file.
     * @details Used only if enable_ssl is true. Default is "server.crt".
     */
    std::string cert_file = "server.crt";

    /**
     * @brief Path to the SSL private key file.
     * @details Used only if enable_ssl is true. Default is "server.key".
     */
    std::string key_file = "server.key";

    /**
     * @brief Path to the authentication file (e.g., JSON with user
     * credentials).
     * @details Default is "auth.json".
     */
    std::string auth_file = "auth.json";

    /**
     * @brief Require authentication for clients.
     * @details Default is false.
     */
    bool require_auth = false;

    /**
     * @brief Maximum number of events to keep in history.
     * @details Default is 1000.
     */
    size_t max_event_history = 1000;

    /**
     * @brief Persist events to disk.
     * @details If true, events are saved to event_store_path. Default is true.
     */
    bool persist_events = true;

    /**
     * @brief Directory path for storing persisted events.
     * @details Default is "events".
     */
    std::string event_store_path = "events";

    /**
     * @brief Interval in seconds for sending heartbeat messages to clients.
     * @details Default is 30 seconds.
     */
    int heartbeat_interval_seconds = 30;

    /**
     * @brief Maximum number of simultaneous client connections.
     * @details Default is 1000.
     */
    int max_connections = 1000;

    /**
     * @brief Enable compression for event data.
     * @details Default is false.
     */
    bool enable_compression = false;

    /**
     * @brief Timeout in seconds for inactive connections.
     * @details Default is 300 seconds.
     */
    int connection_timeout_seconds = 300;

    /**
     * @brief Load configuration from a JSON file.
     * @param filename Path to the JSON configuration file.
     * @return Loaded ServerConfig object.
     *
     * This static method reads the specified JSON file and populates
     * a ServerConfig object with the configuration parameters.
     * Throws an exception if the file cannot be read or parsed.
     */
    static ServerConfig from_file(const std::string& filename);

    /**
     * @brief Save the current configuration to a JSON file.
     * @param filename Path to the JSON configuration file.
     *
     * This method writes the current configuration parameters to
     * the specified JSON file. Throws an exception if the file
     * cannot be written.
     */
    void save_to_file(const std::string& filename) const;
};

}  // namespace atom::extra::asio::sse