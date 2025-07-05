#pragma once

/**
 * @file client.hpp
 * @brief SSE client implementation
 */

#include "../asio_compatibility.hpp"
#include "client_config.hpp"
#include "event.hpp"

#include <functional>
#include <memory>

namespace atom::extra::asio::sse {

/**
 * @brief Callback type for handling received events.
 *
 * The callback receives a constant reference to the received Event object.
 */
using EventCallback = std::function<void(const Event&)>;

/**
 * @brief Callback type for connection status changes.
 *
 * The callback receives a boolean indicating connection status (true for
 * connected, false for disconnected) and a message string describing the status
 * or error.
 */
using ConnectionCallback =
    std::function<void(bool connected, const std::string& message)>;

/**
 * @class Client
 * @brief SSE client with support for reconnection, filtering, and event
 * persistence.
 *
 * This class implements a Server-Sent Events (SSE) client that supports
 * automatic reconnection, event filtering, persistent event storage, and
 * user-defined event/connection callbacks. The client can be started and
 * stopped, and allows dynamic management of event filters.
 */
class Client {
public:
    /**
     * @brief Constructs an SSE client.
     * @param io_context The ASIO I/O context to use for networking.
     * @param config The client configuration parameters.
     */
    Client(net::io_context& io_context, const ClientConfig& config);

    /**
     * @brief Destructor for the SSE client.
     */
    ~Client();

    /**
     * @brief Set the event handler callback.
     * @param handler The callback to invoke for each received event.
     */
    void set_event_handler(EventCallback handler);

    /**
     * @brief Set the connection status handler callback.
     * @param handler The callback to invoke on connection or disconnection.
     */
    void set_connection_handler(ConnectionCallback handler);

    /**
     * @brief Start the SSE client and initiate connection.
     */
    void start();

    /**
     * @brief Stop the SSE client and close the connection.
     */
    void stop();

    /**
     * @brief Attempt to reconnect to the SSE server.
     */
    void reconnect();

    /**
     * @brief Add an event type to the filter list.
     * @param event_type The event type to filter/subscribe to.
     */
    void add_event_filter(const std::string& event_type);

    /**
     * @brief Remove an event type from the filter list.
     * @param event_type The event type to remove from filtering.
     */
    void remove_event_filter(const std::string& event_type);

    /**
     * @brief Clear all event type filters.
     */
    void clear_event_filters();

    /**
     * @brief Check if the client is currently connected.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] bool is_connected() const;

    /**
     * @brief Get the current client configuration.
     * @return Reference to the ClientConfig instance.
     */
    [[nodiscard]] const ClientConfig& config() const;

private:
    class Impl;  ///< Opaque implementation class (PIMPL idiom).
    std::unique_ptr<Impl> pimpl_;  ///< Pointer to implementation.
};

}  // namespace atom::extra::asio::sse
