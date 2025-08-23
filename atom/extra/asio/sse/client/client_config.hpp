#pragma once

/**
 * @file client_config.hpp
 * @brief Configuration management for SSE client
 */

#include <string>
#include <vector>

namespace atom::extra::asio::sse {

/**
 * @struct ClientConfig
 * @brief Client configuration parameters for SSE connection.
 *
 * This structure holds all configuration options required for connecting to an
 * SSE server, including connection details, authentication, SSL options,
 * reconnection policy, event storage, and event filtering.
 */
struct ClientConfig {
    std::string host =
        "localhost";            ///< Hostname or IP address of the SSE server.
    std::string port = "8080";  ///< Port number for the SSE server.
    std::string path = "/events";  ///< Path to the SSE endpoint.
    bool use_ssl = false;  ///< Whether to use SSL/TLS for the connection.
    bool verify_ssl =
        true;  ///< Whether to verify the server's SSL certificate.
    std::string ca_cert_file;  ///< Path to the CA certificate file for SSL
                               ///< verification.
    std::string api_key;       ///< API key for authentication, if required.
    std::string username;      ///< Username for authentication, if required.
    std::string password;      ///< Password for authentication, if required.
    bool reconnect =
        true;  ///< Whether to automatically reconnect on disconnect.
    int max_reconnect_attempts =
        10;  ///< Maximum number of reconnection attempts.
    int reconnect_base_delay_ms =
        1000;  ///< Base delay (ms) between reconnection attempts.
    bool store_events =
        true;  ///< Whether to persistently store received events.
    std::string event_store_path =
        "client_events";  ///< Directory path for event storage.
    std::string
        last_event_id;  ///< ID of the last received event (for resuming).
    std::vector<std::string>
        event_types_filter;  ///< List of event types to filter/subscribe to.

    /**
     * @brief Load configuration from a JSON file.
     *
     * Reads the configuration from the specified JSON file and returns a
     * ClientConfig instance. If the file does not exist or cannot be parsed,
     * default values are used.
     *
     * @param filename Path to the configuration file.
     * @return ClientConfig instance with loaded or default values.
     */
    static ClientConfig from_file(const std::string& filename);

    /**
     * @brief Save configuration to a JSON file.
     *
     * Serializes the current configuration to the specified JSON file.
     *
     * @param filename Path where to save the configuration.
     */
    void save_to_file(const std::string& filename) const;
};

}  // namespace atom::extra::asio::sse
