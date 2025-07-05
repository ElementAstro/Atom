#include "atom/connection/async_sockethub.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(sockethub, m) {
    m.doc() = "Socket hub module for the atom package";

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

    // Enum for LogLevel
    py::enum_<atom::async::connection::LogLevel>(
        m, "LogLevel", "Log level settings for SocketHub")
        .value("DEBUG", atom::async::connection::LogLevel::DEBUG,
               "Debug level logging")
        .value("INFO", atom::async::connection::LogLevel::INFO,
               "Info level logging")
        .value("WARNING", atom::async::connection::LogLevel::WARNING,
               "Warning level logging")
        .value("ERROR", atom::async::connection::LogLevel::ERROR,
               "Error level logging")
        .value("FATAL", atom::async::connection::LogLevel::FATAL,
               "Fatal level logging")
        .export_values();

    // Message::Type enum
    py::enum_<atom::async::connection::Message::Type>(
        m, "MessageType", "Type of message sent through SocketHub")
        .value("TEXT", atom::async::connection::Message::Type::TEXT,
               "Text message")
        .value("BINARY", atom::async::connection::Message::Type::BINARY,
               "Binary message")
        .value("PING", atom::async::connection::Message::Type::PING,
               "Ping message")
        .value("PONG", atom::async::connection::Message::Type::PONG,
               "Pong message")
        .value("CLOSE", atom::async::connection::Message::Type::CLOSE,
               "Close connection message")
        .export_values();

    // Message struct
    py::class_<atom::async::connection::Message>(
        m, "Message", "Message for communication through SocketHub")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("type", &atom::async::connection::Message::type,
                       "Type of the message (TEXT, BINARY, etc.)")
        .def_readwrite("data", &atom::async::connection::Message::data,
                       "Binary data contained in the message")
        .def_readwrite("sender_id",
                       &atom::async::connection::Message::sender_id,
                       "ID of the client that sent the message")
        .def_static(
            "create_text", &atom::async::connection::Message::createText,
            py::arg("text"), py::arg("sender") = 0, "Create a text message")
        .def_static(
            "create_binary", &atom::async::connection::Message::createBinary,
            py::arg("data"), py::arg("sender") = 0, "Create a binary message")
        .def("as_string", &atom::async::connection::Message::asString,
             "Convert message data to string");

    // SocketHubConfig struct
    py::class_<atom::async::connection::SocketHubConfig>(
        m, "SocketHubConfig",
        R"(Configuration structure for the SocketHub.

This structure allows customization of various aspects of the SocketHub's behavior,
including SSL settings, rate limiting, and connection parameters.

Examples:
    >>> from atom.connection.sockethub import SocketHubConfig, LogLevel
    >>> config = SocketHubConfig()
    >>> config.use_ssl = True
    >>> config.ssl_cert_file = "server.crt"
    >>> config.ssl_key_file = "server.key"
    >>> config.connection_timeout = 60  # 60 seconds
    >>> config.log_level = LogLevel.DEBUG
)")
        .def(py::init<>())
        .def_readwrite("use_ssl",
                       &atom::async::connection::SocketHubConfig::use_ssl,
                       "Whether to use SSL/TLS encryption")
        .def_readwrite("backlog_size",
                       &atom::async::connection::SocketHubConfig::backlog_size,
                       "Size of the connection backlog")
        .def_readwrite(
            "connection_timeout",
            &atom::async::connection::SocketHubConfig::connection_timeout,
            "Timeout for connections in seconds")
        .def_readwrite("keep_alive",
                       &atom::async::connection::SocketHubConfig::keep_alive,
                       "Whether to use keep-alive connections")
        .def_readwrite("ssl_cert_file",
                       &atom::async::connection::SocketHubConfig::ssl_cert_file,
                       "Path to SSL certificate file")
        .def_readwrite("ssl_key_file",
                       &atom::async::connection::SocketHubConfig::ssl_key_file,
                       "Path to SSL private key file")
        .def_readwrite("ssl_dh_file",
                       &atom::async::connection::SocketHubConfig::ssl_dh_file,
                       "Path to Diffie-Hellman parameters file")
        .def_readwrite("ssl_password",
                       &atom::async::connection::SocketHubConfig::ssl_password,
                       "Password for the SSL private key file")
        .def_readwrite(
            "enable_rate_limiting",
            &atom::async::connection::SocketHubConfig::enable_rate_limiting,
            "Whether to enable rate limiting")
        .def_readwrite(
            "max_connections_per_ip",
            &atom::async::connection::SocketHubConfig::max_connections_per_ip,
            "Maximum number of connections from a single IP")
        .def_readwrite(
            "max_messages_per_minute",
            &atom::async::connection::SocketHubConfig::max_messages_per_minute,
            "Maximum number of messages per minute from a client")
        .def_readwrite("log_level",
                       &atom::async::connection::SocketHubConfig::log_level,
                       "Logging level");

    // SocketHubStats struct
    py::class_<atom::async::connection::SocketHubStats>(
        m, "SocketHubStats",
        R"(Statistics for monitoring SocketHub activity.

This structure provides metrics about server usage, including connection counts
and message throughput.

Examples:
    >>> stats = hub.get_statistics()
    >>> print(f"Active connections: {stats.active_connections}")
    >>> print(f"Messages processed: {stats.messages_received}")
)")
        .def(py::init<>())
        .def_readonly(
            "total_connections",
            &atom::async::connection::SocketHubStats::total_connections,
            "Total number of connections since server start")
        .def_readonly(
            "active_connections",
            &atom::async::connection::SocketHubStats::active_connections,
            "Number of currently active connections")
        .def_readonly(
            "messages_received",
            &atom::async::connection::SocketHubStats::messages_received,
            "Total number of messages received")
        .def_readonly("messages_sent",
                      &atom::async::connection::SocketHubStats::messages_sent,
                      "Total number of messages sent")
        .def_readonly("bytes_received",
                      &atom::async::connection::SocketHubStats::bytes_received,
                      "Total bytes received")
        .def_readonly("bytes_sent",
                      &atom::async::connection::SocketHubStats::bytes_sent,
                      "Total bytes sent")
        .def_readonly("start_time",
                      &atom::async::connection::SocketHubStats::start_time,
                      "Time when the server started");

    // SocketHub class
    py::class_<atom::async::connection::SocketHub>(
        m, "SocketHub",
        R"(A high-performance asynchronous socket server hub.

This class implements a socket server that can handle multiple clients,
manage client groups, and process messages with customizable handlers.

Args:
    config: Configuration for the socket hub (optional)

Examples:
    >>> from atom.connection.sockethub import SocketHub, Message, SocketHubConfig
    >>>
    >>> # Create and configure the hub
    >>> config = SocketHubConfig()
    >>> config.connection_timeout = 60
    >>> hub = SocketHub(config)
    >>>
    >>> # Set up handlers
    >>> def on_message(message, client_id):
    ...     print(f"Received: {message.as_string()} from client {client_id}")
    ...     hub.broadcast_message(Message.create_text("Echo: " + message.as_string()))
    >>>
    >>> hub.add_message_handler(on_message)
    >>>
    >>> # Start the server
    >>> hub.start(8080)
    >>>
    >>> # Keep the server running until manually stopped
    >>> try:
    ...     # Your application logic here
    ...     pass
    >>> finally:
    ...     hub.stop()
)")
        .def(py::init<const atom::async::connection::SocketHubConfig&>(),
             py::arg("config") = atom::async::connection::SocketHubConfig{},
             "Constructs a SocketHub with the given configuration.")
        .def("start", &atom::async::connection::SocketHub::start,
             py::arg("port"),
             R"(Starts the socket server on the specified port.

Args:
    port: The TCP port on which to listen for connections

Raises:
    RuntimeError: If the server fails to start
)")
        .def("stop", &atom::async::connection::SocketHub::stop,
             R"(Stops the socket server.

This method will disconnect all clients and release resources.
)")
        .def("restart", &atom::async::connection::SocketHub::restart,
             "Restarts the socket server.")
        .def("add_message_handler",
             &atom::async::connection::SocketHub::addMessageHandler,
             py::arg("handler"),
             R"(Adds a handler function for incoming messages.

Args:
    handler: Function taking (Message, client_id) as parameters

Examples:
    >>> def message_handler(message, client_id):
    ...     print(f"Message from {client_id}: {message.as_string()}")
    ...
    >>> hub.add_message_handler(message_handler)
)")
        .def("add_connect_handler",
             &atom::async::connection::SocketHub::addConnectHandler,
             py::arg("handler"),
             R"(Adds a handler function for client connections.

Args:
    handler: Function taking (client_id, ip_address) as parameters

Examples:
    >>> def connect_handler(client_id, ip):
    ...     print(f"Client {client_id} connected from {ip}")
    ...
    >>> hub.add_connect_handler(connect_handler)
)")
        .def("add_disconnect_handler",
             &atom::async::connection::SocketHub::addDisconnectHandler,
             py::arg("handler"),
             R"(Adds a handler function for client disconnections.

Args:
    handler: Function taking (client_id, reason) as parameters

Examples:
    >>> def disconnect_handler(client_id, reason):
    ...     print(f"Client {client_id} disconnected: {reason}")
    ...
    >>> hub.add_disconnect_handler(disconnect_handler)
)")
        .def("add_error_handler",
             &atom::async::connection::SocketHub::addErrorHandler,
             py::arg("handler"),
             R"(Adds a handler function for error events.

Args:
    handler: Function taking (error_message, client_id) as parameters

Examples:
    >>> def error_handler(error, client_id):
    ...     print(f"Error for client {client_id}: {error}")
    ...
    >>> hub.add_error_handler(error_handler)
)")
        .def("broadcast_message",
             &atom::async::connection::SocketHub::broadcastMessage,
             py::arg("message"),
             R"(Broadcasts a message to all connected clients.

Args:
    message: The Message object to broadcast
)")
        .def("send_message_to_client",
             &atom::async::connection::SocketHub::sendMessageToClient,
             py::arg("client_id"), py::arg("message"),
             R"(Sends a message to a specific client.

Args:
    client_id: ID of the client to send the message to
    message: The Message object to send
)")
        .def("disconnect_client",
             &atom::async::connection::SocketHub::disconnectClient,
             py::arg("client_id"), py::arg("reason") = "",
             R"(Disconnects a specific client.

Args:
    client_id: ID of the client to disconnect
    reason: Optional reason for disconnection
)")
        .def("create_group", &atom::async::connection::SocketHub::createGroup,
             py::arg("group_name"),
             R"(Creates a new client group.

Args:
    group_name: Name of the group to create
)")
        .def("add_client_to_group",
             &atom::async::connection::SocketHub::addClientToGroup,
             py::arg("client_id"), py::arg("group_name"),
             R"(Adds a client to a group.

Args:
    client_id: ID of the client to add
    group_name: Name of the group to add the client to
)")
        .def("remove_client_from_group",
             &atom::async::connection::SocketHub::removeClientFromGroup,
             py::arg("client_id"), py::arg("group_name"),
             R"(Removes a client from a group.

Args:
    client_id: ID of the client to remove
    group_name: Name of the group to remove the client from
)")
        .def("broadcast_to_group",
             &atom::async::connection::SocketHub::broadcastToGroup,
             py::arg("group_name"), py::arg("message"),
             R"(Broadcasts a message to all clients in a group.

Args:
    group_name: Name of the group to broadcast to
    message: The Message object to broadcast
)")
        .def("set_authenticator",
             &atom::async::connection::SocketHub::setAuthenticator,
             py::arg("authenticator"),
             R"(Sets the authentication function for client connections.

Args:
    authenticator: Function taking (username, password) and returning a boolean

Examples:
    >>> def authenticate(username, password):
    ...     # Check credentials against a database, etc.
    ...     return username == "admin" and password == "secret"
    ...
    >>> hub.set_authenticator(authenticate)
)")
        .def("require_authentication",
             &atom::async::connection::SocketHub::requireAuthentication,
             py::arg("require"),
             R"(Sets whether clients must authenticate to connect.

Args:
    require: If true, clients must authenticate
)")
        .def("set_client_metadata",
             &atom::async::connection::SocketHub::setClientMetadata,
             py::arg("client_id"), py::arg("key"), py::arg("value"),
             R"(Sets metadata for a client.

Args:
    client_id: ID of the client
    key: Metadata key
    value: Metadata value
)")
        .def("get_client_metadata",
             &atom::async::connection::SocketHub::getClientMetadata,
             py::arg("client_id"), py::arg("key"),
             R"(Gets metadata for a client.

Args:
    client_id: ID of the client
    key: Metadata key

Returns:
    The metadata value, or empty string if not found
)")
        .def("get_statistics",
             &atom::async::connection::SocketHub::getStatistics,
             R"(Gets current server statistics.

Returns:
    A SocketHubStats object with server metrics
)")
        .def("enable_logging",
             &atom::async::connection::SocketHub::enableLogging,
             py::arg("enable"),
             py::arg("level") = atom::async::connection::LogLevel::INFO,
             R"(Enables or disables logging.

Args:
    enable: Whether to enable logging
    level: Log level to use
)")
        .def("set_log_handler",
             &atom::async::connection::SocketHub::setLogHandler,
             py::arg("handler"),
             R"(Sets a custom log handler function.

Args:
    handler: Function taking (log_level, message) as parameters

Examples:
    >>> def log_handler(level, message):
    ...     levels = ["DEBUG", "INFO", "WARNING", "ERROR", "FATAL"]
    ...     print(f"[{levels[int(level)]}] {message}")
    ...
    >>> hub.set_log_handler(log_handler)
)")
        .def("is_running", &atom::async::connection::SocketHub::isRunning,
             R"(Checks if the server is currently running.

Returns:
    True if the server is running, False otherwise
)")
        .def("is_client_connected",
             &atom::async::connection::SocketHub::isClientConnected,
             py::arg("client_id"),
             R"(Checks if a specific client is connected.

Args:
    client_id: ID of the client to check

Returns:
    True if the client is connected, False otherwise
)")
        .def("get_connected_clients",
             &atom::async::connection::SocketHub::getConnectedClients,
             R"(Gets a list of all connected client IDs.

Returns:
    List of client IDs
)")
        .def("get_groups", &atom::async::connection::SocketHub::getGroups,
             R"(Gets a list of all group names.

Returns:
    List of group names
)")
        .def("get_clients_in_group",
             &atom::async::connection::SocketHub::getClientsInGroup,
             py::arg("group_name"),
             R"(Gets a list of client IDs in a specific group.

Args:
    group_name: Name of the group

Returns:
    List of client IDs in the group
)")
        .def(
            "__enter__",
            [](atom::async::connection::SocketHub& self)
                -> atom::async::connection::SocketHub& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::async::connection::SocketHub& self, py::object, py::object,
               py::object) {
                if (self.isRunning()) {
                    self.stop();
                }
            },
            "Ensure server is stopped when exiting context");

    // Factory function for easier creation
    m.def(
        "create_socket_hub",
        [](int port, bool use_ssl = false) {
            atom::async::connection::SocketHubConfig config;
            config.use_ssl = use_ssl;
            auto hub =
                std::make_unique<atom::async::connection::SocketHub>(config);
            hub->start(port);
            return hub;
        },
        py::arg("port"), py::arg("use_ssl") = false,
        R"(Creates and starts a SocketHub on the specified port.

This is a convenience function that creates a SocketHub with default configuration,
then starts it on the specified port.

Args:
    port: The TCP port on which to listen for connections
    use_ssl: Whether to use SSL/TLS encryption (default: False)

Returns:
    A running SocketHub instance

Examples:
    >>> from atom.connection.sockethub import create_socket_hub
    >>> hub = create_socket_hub(8080)
    >>> # Use the hub...
    >>> hub.stop()
)");

    // Message creation helper functions
    m.def(
        "create_text_message",
        [](const std::string& text, size_t sender_id = 0) {
            return atom::async::connection::Message::createText(text,
                                                                sender_id);
        },
        py::arg("text"), py::arg("sender_id") = 0,
        R"(Creates a text message.

Args:
    text: The text content of the message
    sender_id: ID of the message sender (default: 0)

Returns:
    A Message object with TEXT type

Examples:
    >>> from atom.connection.sockethub import create_text_message
    >>> msg = create_text_message("Hello, world!")
    >>> hub.broadcast_message(msg)
)");

    m.def(
        "create_binary_message",
        [](const std::vector<char>& data, size_t sender_id = 0) {
            return atom::async::connection::Message::createBinary(data,
                                                                  sender_id);
        },
        py::arg("data"), py::arg("sender_id") = 0,
        R"(Creates a binary message.

Args:
    data: The binary content of the message as a byte array
    sender_id: ID of the message sender (default: 0)

Returns:
    A Message object with BINARY type

Examples:
    >>> from atom.connection.sockethub import create_binary_message
    >>> msg = create_binary_message(bytearray([0x01, 0x02, 0x03]))
    >>> hub.broadcast_message(msg)
)");
}
