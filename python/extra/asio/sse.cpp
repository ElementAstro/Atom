#include "atom/extra/asio/sse/client/client.hpp"
#include "atom/extra/asio/sse/client/client_config.hpp"
#include "atom/extra/asio/sse/event.hpp"
#include "atom/extra/asio/sse/event_store.hpp"
#include "atom/extra/asio/sse/server/server.hpp"
#include "atom/extra/asio/sse/server/server_config.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::extra::asio::sse;

PYBIND11_MODULE(sse, m) {
    m.doc() = R"(Server-Sent Events (SSE) module for the atom package.

This module provides a modern C++20 SSE client and server implementation with support for
event persistence, automatic reconnection, filtering, authentication, and SSL/TLS.

Features:
- SSE client with automatic reconnection and event filtering
- SSE server with broadcasting, channels, and authentication
- Event persistence and history
- SSL/TLS support for secure connections
- Compression support for event data
- Metadata and JSON support for events

Examples:
    >>> from atom.extra.asio import sse
    >>> import asio
    >>>
    >>> # Create SSE client
    >>> io_context = asio.io_context()
    >>> config = sse.ClientConfig()
    >>> config.host = "localhost"
    >>> config.port = "8080"
    >>> config.path = "/events"
    >>>
    >>> client = sse.Client(io_context, config)
    >>>
    >>> # Set event handler
    >>> def on_event(event):
    ...     print(f"Received: {event.event_type()} - {event.data()}")
    >>>
    >>> client.set_event_handler(on_event)
    >>> client.start()
    >>>
    >>> # Create SSE server
    >>> server_config = sse.ServerConfig()
    >>> server_config.port = 8080
    >>> server = sse.SSEServer(io_context, server_config)
    >>>
    >>> # Broadcast event
    >>> event = sse.Event("1", "message", "Hello SSE!")
    >>> server.broadcast_event(event)
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Event class
    py::class_<Event>(
        m, "Event",
        R"(Represents a Server-Sent Event with metadata and payload.

This class encapsulates all information for a single SSE event, including
its unique identifier, type, data payload, metadata, timestamp, and flags.

Examples:
    >>> event = sse.Event("1", "message", "Hello World")
    >>> print(event.id())
    >>> print(event.event_type())
    >>> print(event.data())
    >>>
    >>> # Event with metadata
    >>> event = sse.Event("2", "update", "Data updated", {"user": "alice"})
    >>> meta = event.get_metadata("user")
    >>> if meta:
    ...     print(f"User: {meta}")
)")
        .def(py::init<std::string, std::string, std::string>(), py::arg("id"),
             py::arg("event_type"), py::arg("data"),
             R"(Construct an Event with string data.

Args:
    id: The unique event identifier.
    event_type: The type of the event.
    data: The event payload as a string.
)")
        .def(py::init<std::string, std::string, std::string,
                      std::unordered_map<std::string, std::string>>(),
             py::arg("id"), py::arg("event_type"), py::arg("data"),
             py::arg("metadata"),
             R"(Construct an Event with string data and metadata.

Args:
    id: The unique event identifier.
    event_type: The type of the event.
    data: The event payload as a string.
    metadata: Metadata key-value pairs.
)")
        .def(py::init<std::string, std::string, nlohmann::json>(),
             py::arg("id"), py::arg("event_type"), py::arg("json_data"),
             R"(Construct an Event with JSON data.

Args:
    id: The unique event identifier.
    event_type: The type of the event.
    json_data: The event payload as JSON.
)")
        .def("id", &Event::id, R"(Get the event's unique identifier.

Returns:
    str: The event ID.
)")
        .def("event_type", &Event::event_type, R"(Get the event type.

Returns:
    str: The event type string.
)")
        .def("data", &Event::data, R"(Get the event data as a string.

Returns:
    str: The event data.
)")
        .def("timestamp", &Event::timestamp,
             R"(Get the event's timestamp (nanoseconds since epoch).

Returns:
    int: The event timestamp.
)")
        .def("is_json", &Event::is_json, R"(Check if the event data is JSON.

Returns:
    bool: True if the data is JSON, false otherwise.
)")
        .def("is_compressed", &Event::is_compressed,
             R"(Check if the event data is compressed.

Returns:
    bool: True if the data is compressed, false otherwise.
)")
        .def("get_metadata", &Event::get_metadata, py::arg("key"),
             R"(Retrieve a metadata value by key.

Args:
    key: The metadata key.

Returns:
    Optional[str]: The value if present, None otherwise.
)")
        .def("add_metadata", &Event::add_metadata, py::arg("key"),
             py::arg("value"),
             R"(Add or update a metadata key-value pair.

Args:
    key: The metadata key.
    value: The metadata value.
)")
        .def("parse_json", &Event::parse_json,
             R"(Parse the event data as JSON.

Returns:
    dict: The parsed JSON object.

Raises:
    RuntimeError: If the data is not valid JSON.
)")
        .def("compress", &Event::compress,
             R"(Compress the event data (if compression is supported).)")
        .def("decompress", &Event::decompress,
             R"(Decompress the event data (if compression is supported).)")
        .def("serialize", &Event::serialize,
             R"(Serialize the event to a string in SSE format.

Returns:
    str: The serialized event string.
)")
        .def_static("deserialize", &Event::deserialize, py::arg("lines"),
                    R"(Deserialize an Event from a sequence of SSE lines.

Args:
    lines: The lines representing the event.

Returns:
    Optional[Event]: The deserialized Event if successful, None otherwise.
)");

    // MessageEvent class
    py::class_<MessageEvent, Event>(
        m, "MessageEvent",
        R"(Specialized event type for plain messages.

Examples:
    >>> msg = sse.MessageEvent("1", "Hello World")
    >>> print(msg.event_type())  # "message"
)")
        .def(py::init<std::string, std::string>(), py::arg("id"),
             py::arg("message"),
             R"(Construct a MessageEvent.

Args:
    id: The event ID.
    message: The message payload.
)");

    // ClientConfig struct
    py::class_<ClientConfig>(
        m, "ClientConfig",
        R"(Client configuration parameters for SSE connection.

This structure holds all configuration options required for connecting to an
SSE server, including connection details, authentication, SSL options,
reconnection policy, event storage, and event filtering.

Examples:
    >>> config = sse.ClientConfig()
    >>> config.host = "example.com"
    >>> config.port = "443"
    >>> config.use_ssl = True
    >>> config.api_key = "my-api-key"
    >>> config.reconnect = True
    >>> config.max_reconnect_attempts = 5
)")
        .def(py::init<>(), "Create default client configuration")
        .def_readwrite("host", &ClientConfig::host,
                       "Hostname or IP address of the SSE server")
        .def_readwrite("port", &ClientConfig::port,
                       "Port number for the SSE server")
        .def_readwrite("path", &ClientConfig::path, "Path to the SSE endpoint")
        .def_readwrite("use_ssl", &ClientConfig::use_ssl,
                       "Whether to use SSL/TLS for the connection")
        .def_readwrite("verify_ssl", &ClientConfig::verify_ssl,
                       "Whether to verify the server's SSL certificate")
        .def_readwrite("ca_cert_file", &ClientConfig::ca_cert_file,
                       "Path to the CA certificate file for SSL verification")
        .def_readwrite("api_key", &ClientConfig::api_key,
                       "API key for authentication, if required")
        .def_readwrite("username", &ClientConfig::username,
                       "Username for authentication, if required")
        .def_readwrite("password", &ClientConfig::password,
                       "Password for authentication, if required")
        .def_readwrite("reconnect", &ClientConfig::reconnect,
                       "Whether to automatically reconnect on disconnect")
        .def_readwrite("max_reconnect_attempts",
                       &ClientConfig::max_reconnect_attempts,
                       "Maximum number of reconnection attempts")
        .def_readwrite("reconnect_base_delay_ms",
                       &ClientConfig::reconnect_base_delay_ms,
                       "Base delay (ms) between reconnection attempts")
        .def_readwrite("store_events", &ClientConfig::store_events,
                       "Whether to persistently store received events")
        .def_readwrite("event_store_path", &ClientConfig::event_store_path,
                       "Directory path for event storage")
        .def_readwrite("last_event_id", &ClientConfig::last_event_id,
                       "ID of the last received event (for resuming)")
        .def_readwrite("event_types_filter", &ClientConfig::event_types_filter,
                       "List of event types to filter/subscribe to")
        .def_static("from_file", &ClientConfig::from_file, py::arg("filename"),
                    R"(Load configuration from a JSON file.

Args:
    filename: Path to the JSON configuration file.

Returns:
    ClientConfig: Loaded configuration object.
)")
        .def("save_to_file", &ClientConfig::save_to_file, py::arg("filename"),
             R"(Save the current configuration to a JSON file.

Args:
    filename: Path to the JSON configuration file.
)");

    // Client class
    py::class_<Client>(
        m, "Client",
        R"(SSE client with support for reconnection, filtering, and event persistence.

This class implements a Server-Sent Events (SSE) client that supports
automatic reconnection, event filtering, persistent event storage, and
user-defined event/connection callbacks.

Examples:
    >>> import asio
    >>> io_context = asio.io_context()
    >>> config = sse.ClientConfig()
    >>> client = sse.Client(io_context, config)
    >>>
    >>> def on_event(event):
    ...     print(f"Event: {event.data()}")
    >>>
    >>> def on_connection(connected, message):
    ...     print(f"Connected: {connected}, Message: {message}")
    >>>
    >>> client.set_event_handler(on_event)
    >>> client.set_connection_handler(on_connection)
    >>> client.start()
)")
        .def(py::init<net::io_context&, const ClientConfig&>(),
             py::arg("io_context"), py::arg("config"),
             R"(Construct an SSE client.

Args:
    io_context: The ASIO I/O context to use for networking.
    config: The client configuration parameters.
)")
        .def("set_event_handler", &Client::set_event_handler,
             py::arg("handler"),
             R"(Set the event handler callback.

Args:
    handler: The callback to invoke for each received event.
)")
        .def("set_connection_handler", &Client::set_connection_handler,
             py::arg("handler"),
             R"(Set the connection status handler callback.

Args:
    handler: The callback to invoke on connection or disconnection.
)")
        .def("start", &Client::start,
             R"(Start the SSE client and initiate connection.)")
        .def("stop", &Client::stop,
             R"(Stop the SSE client and close the connection.)")
        .def("reconnect", &Client::reconnect,
             R"(Attempt to reconnect to the SSE server.)")
        .def("add_event_filter", &Client::add_event_filter,
             py::arg("event_type"),
             R"(Add an event type to the filter list.

Args:
    event_type: The event type to filter/subscribe to.
)")
        .def("remove_event_filter", &Client::remove_event_filter,
             py::arg("event_type"),
             R"(Remove an event type from the filter list.

Args:
    event_type: The event type to remove from filtering.
)")
        .def("clear_event_filters", &Client::clear_event_filters,
             R"(Clear all event type filters.)")
        .def("is_connected", &Client::is_connected,
             R"(Check if the client is currently connected.

Returns:
    bool: True if connected, false otherwise.
)")
        .def("config", &Client::config,
             R"(Get the current client configuration.

Returns:
    ClientConfig: Reference to the ClientConfig instance.
)");

    // ServerConfig struct
    py::class_<ServerConfig>(m, "ServerConfig",
                             R"(Server configuration parameters.

This structure holds all configurable parameters for the SSE server,
including network settings, SSL options, authentication, event storage,
and connection management.

Examples:
    >>> config = sse.ServerConfig()
    >>> config.port = 8080
    >>> config.address = "0.0.0.0"
    >>> config.enable_ssl = True
    >>> config.cert_file = "server.crt"
    >>> config.key_file = "server.key"
    >>> config.require_auth = True
    >>> config.max_connections = 1000
)")
        .def(py::init<>(), "Create default server configuration")
        .def_readwrite("port", &ServerConfig::port,
                       "TCP port number the server listens on")
        .def_readwrite("address", &ServerConfig::address,
                       "IP address the server binds to")
        .def_readwrite("enable_ssl", &ServerConfig::enable_ssl,
                       "Enable SSL/TLS for secure connections")
        .def_readwrite("cert_file", &ServerConfig::cert_file,
                       "Path to the SSL certificate file")
        .def_readwrite("key_file", &ServerConfig::key_file,
                       "Path to the SSL private key file")
        .def_readwrite("auth_file", &ServerConfig::auth_file,
                       "Path to the authentication file")
        .def_readwrite("require_auth", &ServerConfig::require_auth,
                       "Require authentication for clients")
        .def_readwrite("max_event_history", &ServerConfig::max_event_history,
                       "Maximum number of events to keep in history")
        .def_readwrite("persist_events", &ServerConfig::persist_events,
                       "Persist events to disk")
        .def_readwrite("event_store_path", &ServerConfig::event_store_path,
                       "Directory path for storing persisted events")
        .def_readwrite("heartbeat_interval_seconds",
                       &ServerConfig::heartbeat_interval_seconds,
                       "Interval in seconds for sending heartbeat messages")
        .def_readwrite("max_connections", &ServerConfig::max_connections,
                       "Maximum number of simultaneous client connections")
        .def_readwrite("enable_compression", &ServerConfig::enable_compression,
                       "Enable compression for event data")
        .def_readwrite("connection_timeout_seconds",
                       &ServerConfig::connection_timeout_seconds,
                       "Timeout in seconds for inactive connections")
        .def_static("from_file", &ServerConfig::from_file, py::arg("filename"),
                    R"(Load configuration from a JSON file.

Args:
    filename: Path to the JSON configuration file.

Returns:
    ServerConfig: Loaded configuration object.
)")
        .def("save_to_file", &ServerConfig::save_to_file, py::arg("filename"),
             R"(Save the current configuration to a JSON file.

Args:
    filename: Path to the JSON configuration file.
)");

    // SSEServer class
    py::class_<SSEServer>(
        m, "SSEServer",
        R"(Main SSE server with coroutine-based connection handling.

The SSEServer class manages client connections, event broadcasting,
authentication, event storage, and server metrics.

Examples:
    >>> import asio
    >>> io_context = asio.io_context()
    >>> config = sse.ServerConfig()
    >>> config.port = 8080
    >>> server = sse.SSEServer(io_context, config)
    >>>
    >>> # Broadcast event to all clients
    >>> event = sse.Event("1", "notification", "Server started")
    >>> server.broadcast_event(event)
    >>>
    >>> # Get server metrics
    >>> metrics = server.get_metrics()
    >>> print(metrics)
)")
        .def(py::init<net::io_context&, const ServerConfig&>(),
             py::arg("io_context"), py::arg("config"),
             R"(Construct the SSEServer.

Args:
    io_context: The ASIO I/O context for asynchronous operations.
    config: The server configuration parameters.
)")
        .def(
            "broadcast_event",
            [](SSEServer& self, const Event& event) {
                self.broadcast_event(event);
            },
            py::arg("event"),
            R"(Broadcast an event to all connected clients.

Args:
    event: The event object to broadcast.
)")
        .def("get_metrics", &SSEServer::get_metrics,
             R"(Get server metrics.

Returns:
    dict: A JSON object containing current server metrics.
)")
        .def("config", &SSEServer::config,
             R"(Get current configuration.

Returns:
    ServerConfig: Reference to the current ServerConfig object.
)");

    // EventStore class
    py::class_<EventStore>(m, "EventStore",
                           R"(Persistent storage for SSE events.

This class provides persistent storage for events, allowing clients to
retrieve missed events when reconnecting.

Examples:
    >>> store = sse.EventStore("events")
    >>> event = sse.Event("1", "message", "Hello")
    >>> store.store_event(event)
    >>> events = store.get_events_after_id("0", 10)
)")
        .def(py::init<const std::string&>(), py::arg("storage_path"),
             R"(Construct an EventStore.

Args:
    storage_path: Directory path for storing events.
)")
        .def("store_event", &EventStore::store_event, py::arg("event"),
             R"(Store an event persistently.

Args:
    event: The event to store.
)")
        .def("get_events_after_id", &EventStore::get_events_after_id,
             py::arg("last_id"), py::arg("max_count") = 100,
             py::arg("channel") = "",
             R"(Retrieve events after a given event ID.

Args:
    last_id: The last event ID received.
    max_count: Maximum number of events to retrieve (default: 100).
    channel: Optional channel filter (default: "").

Returns:
    list[Event]: List of events after the specified ID.
)")
        .def("clear", &EventStore::clear, R"(Clear all stored events.)");
}
