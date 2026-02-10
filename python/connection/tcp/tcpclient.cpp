#include "atom/connection/tcp/async_tcpclient.hpp"
#include "atom/connection/tcp/tcp_common.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * @brief Registers exception translations for the TCP client module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
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
}

/**
 * @brief Binds the ConnectionState enum to Python.
 *
 * This function creates Python bindings for the ConnectionState enum which
 * represents different states of the TCP client connection.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionState(py::module_& m) {
    py::enum_<atom::async::connection::ConnectionState>(
        m, "ConnectionState", "States of the TCP client connection")
        .value("Disconnected",
               atom::async::connection::ConnectionState::Disconnected,
               "Client is disconnected from the server")
        .value("Connecting",
               atom::async::connection::ConnectionState::Connecting,
               "Client is attempting to connect to the server")
        .value("Connected", atom::async::connection::ConnectionState::Connected,
               "Client is successfully connected to the server")
        .value("Reconnecting",
               atom::async::connection::ConnectionState::Reconnecting,
               "Client is attempting to reconnect after disconnection")
        .value("Failed", atom::async::connection::ConnectionState::Failed,
               "Connection attempt has failed")
        .export_values();
}

/**
 * @brief Binds the ConnectionConfig struct to Python.
 *
 * This function creates Python bindings for the ConnectionConfig struct which
 * provides various settings to control TCP connection behavior.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionConfig(py::module_& m) {
    py::class_<atom::async::connection::ConnectionConfig>(
        m, "ConnectionConfig",
        R"(Configuration for TCP client connections.

This structure provides various settings to control connection behavior,
including timeouts, SSL settings, and reconnection parameters.

Examples:
    >>> from atom.connection.tcpclient import ConnectionConfig
    >>> config = ConnectionConfig()
    >>> config.use_ssl = True
    >>> config.connect_timeout = 10000  # 10 seconds
    >>> config.reconnect_attempts = 5
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("use_ssl",
                       &atom::async::connection::ConnectionConfig::use_ssl,
                       "Whether to use SSL/TLS encryption")
        .def_readwrite("verify_ssl",
                       &atom::async::connection::ConnectionConfig::verify_ssl,
                       "Whether to verify SSL certificates")
        .def_readwrite(
            "connect_timeout",
            &atom::async::connection::ConnectionConfig::connect_timeout,
            "Timeout for connection attempts in milliseconds")
        .def_readwrite("read_timeout",
                       &atom::async::connection::ConnectionConfig::read_timeout,
                       "Timeout for read operations in milliseconds")
        .def_readwrite(
            "write_timeout",
            &atom::async::connection::ConnectionConfig::write_timeout,
            "Timeout for write operations in milliseconds")
        .def_readwrite("keep_alive",
                       &atom::async::connection::ConnectionConfig::keep_alive,
                       "Whether to use TCP keep-alive")
        .def_readwrite(
            "reconnect_attempts",
            &atom::async::connection::ConnectionConfig::reconnect_attempts,
            "Number of reconnection attempts")
        .def_readwrite(
            "reconnect_delay",
            &atom::async::connection::ConnectionConfig::reconnect_delay,
            "Delay between reconnection attempts in milliseconds")
        .def_readwrite(
            "heartbeat_interval",
            &atom::async::connection::ConnectionConfig::heartbeat_interval,
            "Interval between heartbeat messages in milliseconds")
        .def_readwrite(
            "receive_buffer_size",
            &atom::async::connection::ConnectionConfig::receive_buffer_size,
            "Size of the receive buffer in bytes")
        .def_readwrite(
            "auto_reconnect",
            &atom::async::connection::ConnectionConfig::auto_reconnect,
            "Whether to automatically reconnect on disconnection")
        .def_readwrite(
            "ssl_certificate_path",
            &atom::async::connection::ConnectionConfig::ssl_certificate_path,
            "Path to the SSL certificate file")
        .def_readwrite(
            "ssl_private_key_path",
            &atom::async::connection::ConnectionConfig::ssl_private_key_path,
            "Path to the SSL private key file")
        .def_readwrite(
            "ca_certificate_path",
            &atom::async::connection::ConnectionConfig::ca_certificate_path,
            "Path to the Certificate Authority certificate file");
}

/**
 * @brief Binds the ProxyConfig struct to Python.
 *
 * This function creates Python bindings for the ProxyConfig struct which
 * provides settings for connecting through a proxy server.
 *
 * @param m The pybind11 module to bind to
 */
void bindProxyConfig(py::module_& m) {
    py::class_<atom::async::connection::ProxyConfig>(
        m, "ProxyConfig",
        R"(Configuration for connection proxy.

This structure provides settings for connecting through a proxy server.

Examples:
    >>> from atom.connection.tcpclient import ProxyConfig
    >>> proxy = ProxyConfig()
    >>> proxy.host = "proxy.example.com"
    >>> proxy.port = 8080
    >>> proxy.enabled = True
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("host", &atom::async::connection::ProxyConfig::host,
                       "Proxy server hostname or IP address")
        .def_readwrite("port", &atom::async::connection::ProxyConfig::port,
                       "Proxy server port")
        .def_readwrite("username",
                       &atom::async::connection::ProxyConfig::username,
                       "Username for proxy authentication")
        .def_readwrite("password",
                       &atom::async::connection::ProxyConfig::password,
                       "Password for proxy authentication")
        .def_readwrite("enabled",
                       &atom::async::connection::ProxyConfig::enabled,
                       "Whether to use the proxy");
}

/**
 * @brief Binds the ConnectionStats struct to Python.
 *
 * This function creates Python bindings for the ConnectionStats struct which
 * provides various metrics about connection usage and performance.
 *
 * @param m The pybind11 module to bind to
 */
void bindConnectionStats(py::module_& m) {
    py::class_<atom::async::connection::ConnectionStats>(
        m, "ConnectionStats",
        R"(Statistics for the TCP client connection.

This structure provides various metrics about connection usage and performance.

Examples:
    >>> stats = client.get_stats()
    >>> print(f"Bytes received: {stats.total_bytes_received}")
    >>> print(f"Average latency: {stats.average_latency} ms")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly(
            "total_bytes_sent",
            &atom::async::connection::ConnectionStats::total_bytes_sent,
            "Total bytes sent over this connection")
        .def_readonly(
            "total_bytes_received",
            &atom::async::connection::ConnectionStats::total_bytes_received,
            "Total bytes received over this connection")
        .def_readonly(
            "connection_attempts",
            &atom::async::connection::ConnectionStats::connection_attempts,
            "Number of connection attempts made")
        .def_readonly(
            "successful_connections",
            &atom::async::connection::ConnectionStats::successful_connections,
            "Number of successful connections")
        .def_readonly(
            "failed_connections",
            &atom::async::connection::ConnectionStats::failed_connections,
            "Number of failed connection attempts")
        .def_readonly(
            "last_connected_time",
            &atom::async::connection::ConnectionStats::last_connected_time,
            "Time of last successful connection")
        .def_readonly(
            "last_activity_time",
            &atom::async::connection::ConnectionStats::last_activity_time,
            "Time of last send or receive activity")
        .def_readonly(
            "average_latency",
            &atom::async::connection::ConnectionStats::average_latency,
            "Average connection latency in milliseconds");
}

/**
 * @brief Binds the TcpClient class to Python.
 *
 * This function creates Python bindings for the async TcpClient class which
 * provides asynchronous TCP client functionality with modern C++20 features.
 *
 * @param m The pybind11 module to bind to
 */
void bindTcpClient(py::module_& m) {
    py::class_<atom::async::connection::TcpClient>(
        m, "TcpClient",
        R"(Asynchronous TCP client for connecting to servers.

This class provides methods for establishing TCP connections, sending and receiving data,
and managing connection state with support for SSL/TLS, proxies, and automatic reconnection.

Examples:
    >>> from atom.connection.tcpclient import TcpClient, ConnectionConfig
    >>>
    >>> # Create client with custom configuration
    >>> config = ConnectionConfig()
    >>> config.use_ssl = True
    >>> config.auto_reconnect = True
    >>> client = TcpClient(config)
    >>>
    >>> # Set up event handlers
    >>> def on_connected():
    ...     print("Connected to server")
    ...     client.send_string("Hello, server!")
    >>>
    >>> def on_data_received(data):
    ...     print(f"Received: {data}")
    >>>
    >>> client.set_on_connected_callback(on_connected)
    >>> client.set_on_data_received_callback(on_data_received)
    >>>
    >>> # Connect to server
    >>> if client.connect("example.com", 443):
    ...     print("Connection initiated")
    >>>
    >>> # Keep the client running
    >>> try:
    ...     # Your application logic here
    ...     pass
    >>> finally:
    ...     client.disconnect()
)")
        .def(py::init<const atom::async::connection::ConnectionConfig&>(),
             py::arg("config") = atom::async::connection::ConnectionConfig{},
             "Constructs a TcpClient with the given configuration.")
        .def("connect",
             py::overload_cast<const std::string&, int>(
                 &atom::async::connection::TcpClient::connect),
             py::arg("host"), py::arg("port"),
             R"(Connects to a TCP server.

Args:
    host: Server hostname or IP address
    port: Server port number

Returns:
    True if connection was initiated successfully, False otherwise

Examples:
    >>> client.connect("example.com", 80)
    True
)")
        .def("connect",
             py::overload_cast<const std::string&, int,
                               std::optional<std::chrono::milliseconds>>(
                 &atom::async::connection::TcpClient::connect),
             py::arg("host"), py::arg("port"), py::arg("timeout"),
             R"(Connects to a TCP server with timeout.

Args:
    host: Server hostname or IP address
    port: Server port number
    timeout: Connection timeout in milliseconds

Returns:
    True if connection was initiated successfully, False otherwise
)")
        .def("connect_async", &atom::async::connection::TcpClient::connectAsync,
             py::arg("host"), py::arg("port"),
             R"(Connects asynchronously to a TCP server.

Args:
    host: Server hostname or IP address
    port: Server port number

Returns:
    Future that resolves to True if connection succeeds, False otherwise
)")
        .def("disconnect", &atom::async::connection::TcpClient::disconnect,
             R"(Disconnects from the server.

This method closes the connection and cleans up resources.
)")
        .def("configure_reconnection",
             &atom::async::connection::TcpClient::configureReconnection,
             py::arg("attempts"), py::arg("delay") = std::chrono::seconds(1),
             R"(Configures automatic reconnection behavior.

Args:
    attempts: Number of reconnection attempts (0 to disable)
    delay: Delay between reconnection attempts in milliseconds

Examples:
    >>> client.configure_reconnection(5, 2000)  # 5 attempts, 2 second delay
)")
        .def("set_heartbeat_interval",
             &atom::async::connection::TcpClient::setHeartbeatInterval,
             py::arg("interval"), py::arg("data") = std::vector<char>{},
             R"(Sets the heartbeat interval for keep-alive messages.

Args:
    interval: Interval between heartbeats in milliseconds
    data: Optional heartbeat data to send (default: empty)

Examples:
    >>> client.set_heartbeat_interval(30000)  # 30 second heartbeat
)")
        .def("send", &atom::async::connection::TcpClient::send, py::arg("data"),
             R"(Sends raw data to the server.

Args:
    data: Binary data to send as a list of bytes

Returns:
    True if send was successful, False otherwise

Examples:
    >>> client.send([0x48, 0x65, 0x6c, 0x6c, 0x6f])  # "Hello"
    True
)")
        .def("send_string", &atom::async::connection::TcpClient::sendString,
             py::arg("data"),
             R"(Sends string data to the server.

Args:
    data: String data to send

Returns:
    True if send was successful, False otherwise

Examples:
    >>> client.send_string("Hello, server!")
    True
)")
        .def("send_with_timeout",
             &atom::async::connection::TcpClient::sendWithTimeout,
             py::arg("data"), py::arg("timeout"),
             R"(Sends data with a timeout.

Args:
    data: Binary data to send
    timeout: Send timeout in milliseconds

Returns:
    True if send was successful, False otherwise
)")
        .def("receive", &atom::async::connection::TcpClient::receive,
             py::arg("size"), py::arg("timeout") = std::nullopt,
             R"(Receives a specific amount of data.

Args:
    size: Number of bytes to receive
    timeout: Optional receive timeout in milliseconds

Returns:
    Future that resolves to the received data

Examples:
    >>> future = client.receive(1024)
    >>> data = future.get()  # Wait for data
)")
        .def("receive_until", &atom::async::connection::TcpClient::receiveUntil,
             py::arg("delimiter"), py::arg("timeout") = std::nullopt,
             R"(Receives data until a delimiter is found.

Args:
    delimiter: Character to stop receiving at
    timeout: Optional receive timeout in milliseconds

Returns:
    Future that resolves to the received string

Examples:
    >>> future = client.receive_until('\n')
    >>> line = future.get()  # Wait for line
)")
        .def("request_response",
             &atom::async::connection::TcpClient::requestResponse,
             py::arg("request"), py::arg("response_size"),
             py::arg("timeout") = std::nullopt,
             R"(Performs a request-response cycle.

Args:
    request: Data to send as request
    response_size: Expected response size in bytes
    timeout: Optional operation timeout in milliseconds

Returns:
    Future that resolves to the response data

Examples:
    >>> request = [0x01, 0x02, 0x03]
    >>> future = client.request_response(request, 1024)
    >>> response = future.get()
)")
        .def("set_proxy_config",
             &atom::async::connection::TcpClient::setProxyConfig,
             py::arg("config"),
             R"(Sets proxy configuration for connections.

Args:
    config: ProxyConfig object with proxy settings

Examples:
    >>> proxy = ProxyConfig()
    >>> proxy.host = "proxy.example.com"
    >>> proxy.port = 8080
    >>> proxy.enabled = True
    >>> client.set_proxy_config(proxy)
)")
        .def("configure_ssl_certificates",
             &atom::async::connection::TcpClient::configureSslCertificates,
             py::arg("cert_path"), py::arg("key_path"), py::arg("ca_path"),
             R"(Configures SSL certificates for secure connections.

Args:
    cert_path: Path to client certificate file
    key_path: Path to private key file
    ca_path: Path to CA certificate file

Examples:
    >>> client.configure_ssl_certificates("client.crt", "client.key", "ca.crt")
)")
        .def("get_connection_state",
             &atom::async::connection::TcpClient::getConnectionState,
             R"(Gets the current connection state.

Returns:
    ConnectionState enum value representing current state

Examples:
    >>> state = client.get_connection_state()
    >>> if state == ConnectionState.Connected:
    ...     print("Client is connected")
)")
        .def("is_connected", &atom::async::connection::TcpClient::isConnected,
             R"(Checks if the client is connected to a server.

Returns:
    True if connected, False otherwise

Examples:
    >>> if client.is_connected():
    ...     client.send_string("Hello!")
)")
        .def("get_error_message",
             &atom::async::connection::TcpClient::getErrorMessage,
             R"(Gets the most recent error message.

Returns:
    String containing the last error message

Examples:
    >>> if not client.connect("invalid-host", 80):
    ...     print(f"Connection failed: {client.get_error_message()}")
)")
        .def("get_stats", &atom::async::connection::TcpClient::getStats,
             R"(Gets connection statistics.

Returns:
    ConnectionStats object with usage metrics

Examples:
    >>> stats = client.get_stats()
    >>> print(f"Bytes sent: {stats.total_bytes_sent}")
)")
        .def("reset_stats", &atom::async::connection::TcpClient::resetStats,
             R"(Resets connection statistics to zero.

Examples:
    >>> client.reset_stats()
)")
        .def("get_remote_address",
             &atom::async::connection::TcpClient::getRemoteAddress,
             R"(Gets the remote server address.

Returns:
    String containing the remote server address

Examples:
    >>> print(f"Connected to: {client.get_remote_address()}")
)")
        .def("get_remote_port",
             &atom::async::connection::TcpClient::getRemotePort,
             R"(Gets the remote server port.

Returns:
    Integer port number of the remote server

Examples:
    >>> print(f"Connected to port: {client.get_remote_port()}")
)")
        .def("set_property", &atom::async::connection::TcpClient::setProperty,
             py::arg("key"), py::arg("value"),
             R"(Sets a custom property for this connection.

Args:
    key: Property key
    value: Property value

Examples:
    >>> client.set_property("session_id", "abc123")
)")
        .def("get_property", &atom::async::connection::TcpClient::getProperty,
             py::arg("key"),
             R"(Gets a custom property value.

Args:
    key: Property key

Returns:
    Property value or empty string if not found

Examples:
    >>> session_id = client.get_property("session_id")
)")
        .def("set_on_connecting_callback",
             &atom::async::connection::TcpClient::setOnConnectingCallback,
             py::arg("callback"),
             R"(Sets callback for connection initiation events.

Args:
    callback: Function to call when connection starts

Examples:
    >>> def on_connecting():
    ...     print("Connecting to server...")
    >>> client.set_on_connecting_callback(on_connecting)
)")
        .def("set_on_connected_callback",
             &atom::async::connection::TcpClient::setOnConnectedCallback,
             py::arg("callback"),
             R"(Sets callback for successful connection events.

Args:
    callback: Function to call when connected

Examples:
    >>> def on_connected():
    ...     print("Successfully connected!")
    >>> client.set_on_connected_callback(on_connected)
)")
        .def("set_on_disconnected_callback",
             &atom::async::connection::TcpClient::setOnDisconnectedCallback,
             py::arg("callback"),
             R"(Sets callback for disconnection events.

Args:
    callback: Function to call when disconnected

Examples:
    >>> def on_disconnected():
    ...     print("Disconnected from server")
    >>> client.set_on_disconnected_callback(on_disconnected)
)")
        .def("set_on_data_received_callback",
             &atom::async::connection::TcpClient::setOnDataReceivedCallback,
             py::arg("callback"),
             R"(Sets callback for data reception events.

Args:
    callback: Function to call when data is received

Examples:
    >>> def on_data(data):
    ...     print(f"Received {len(data)} bytes")
    >>> client.set_on_data_received_callback(on_data)
)")
        .def("set_on_error_callback",
             &atom::async::connection::TcpClient::setOnErrorCallback,
             py::arg("callback"),
             R"(Sets callback for error events.

Args:
    callback: Function to call when errors occur

Examples:
    >>> def on_error(error):
    ...     print(f"Error: {error}")
    >>> client.set_on_error_callback(on_error)
)")
        .def("set_on_state_changed_callback",
             &atom::async::connection::TcpClient::setOnStateChangedCallback,
             py::arg("callback"),
             R"(Sets callback for connection state changes.

Args:
    callback: Function to call when state changes

Examples:
    >>> def on_state_change(old_state, new_state):
    ...     print(f"State changed: {old_state} -> {new_state}")
    >>> client.set_on_state_changed_callback(on_state_change)
)")
        .def("set_on_heartbeat_callback",
             &atom::async::connection::TcpClient::setOnHeartbeatCallback,
             py::arg("callback"),
             R"(Sets callback for heartbeat events.

Args:
    callback: Function to call on heartbeat

Examples:
    >>> def on_heartbeat():
    ...     print("Heartbeat sent")
    >>> client.set_on_heartbeat_callback(on_heartbeat)
)")
        .def(
            "__enter__",
            [](atom::async::connection::TcpClient& self)
                -> atom::async::connection::TcpClient& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::async::connection::TcpClient& self, py::object, py::object,
               py::object) {
                if (self.isConnected()) {
                    self.disconnect();
                }
            },
            "Ensure client is disconnected when exiting context");
}

/**
 * @brief Binds utility functions to Python.
 *
 * This function creates Python bindings for utility functions that help
 * with TCP client operations and connection management.
 *
 * @param m The pybind11 module to bind to
 */
void bindUtilityFunctions(py::module_& m) {
    // Factory function for creating TCP clients with common configurations
    m.def(
        "create_tcp_client",
        [](const std::string& host, int port, bool use_ssl = false,
           bool auto_reconnect = true) {
            atom::async::connection::ConnectionConfig config;
            config.use_ssl = use_ssl;
            config.auto_reconnect = auto_reconnect;
            auto client =
                std::make_unique<atom::async::connection::TcpClient>(config);
            if (client->connect(host, port)) {
                return client;
            }
            throw std::runtime_error("Failed to connect to " + host + ":" +
                                     std::to_string(port));
        },
        py::arg("host"), py::arg("port"), py::arg("use_ssl") = false,
        py::arg("auto_reconnect") = true,
        R"(Creates and connects a TCP client with common configuration.

This is a convenience function that creates a TcpClient with sensible defaults,
then connects it to the specified server.

Args:
    host: Server hostname or IP address
    port: Server port number
    use_ssl: Whether to use SSL/TLS encryption (default: False)
    auto_reconnect: Whether to enable automatic reconnection (default: True)

Returns:
    A connected TcpClient instance

Raises:
    RuntimeError: If connection fails

Examples:
    >>> from atom.connection.tcpclient import create_tcp_client
    >>> client = create_tcp_client("example.com", 443, use_ssl=True)
    >>> client.send_string("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
)");

    // Utility function to test connectivity
    m.def(
        "test_connection",
        [](const std::string& host, int port,
           std::chrono::milliseconds timeout =
               std::chrono::milliseconds(5000)) {
            atom::async::connection::ConnectionConfig config;
            config.connect_timeout = timeout;
            atom::async::connection::TcpClient client(config);
            bool result = client.connect(host, port);
            if (result) {
                client.disconnect();
            }
            return result;
        },
        py::arg("host"), py::arg("port"),
        py::arg("timeout") = std::chrono::milliseconds(5000),
        R"(Tests connectivity to a TCP server.

This utility function attempts to connect to a server and immediately
disconnects, useful for testing if a server is reachable.

Args:
    host: Server hostname or IP address
    port: Server port number
    timeout: Connection timeout in milliseconds (default: 5000)

Returns:
    True if connection succeeds, False otherwise

Examples:
    >>> from atom.connection.tcpclient import test_connection
    >>> if test_connection("google.com", 80):
    ...     print("Google is reachable")
)");

    // Utility function to create SSL configuration
    m.def(
        "create_ssl_config",
        [](bool verify_ssl = true, const std::string& cert_path = "",
           const std::string& key_path = "", const std::string& ca_path = "") {
            atom::async::connection::ConnectionConfig config;
            config.use_ssl = true;
            config.verify_ssl = verify_ssl;
            config.ssl_certificate_path = cert_path;
            config.ssl_private_key_path = key_path;
            config.ca_certificate_path = ca_path;
            return config;
        },
        py::arg("verify_ssl") = true, py::arg("cert_path") = "",
        py::arg("key_path") = "", py::arg("ca_path") = "",
        R"(Creates a ConnectionConfig with SSL settings.

This utility function creates a pre-configured ConnectionConfig object
with SSL/TLS settings for secure connections.

Args:
    verify_ssl: Whether to verify SSL certificates (default: True)
    cert_path: Path to client certificate file (optional)
    key_path: Path to private key file (optional)
    ca_path: Path to CA certificate file (optional)

Returns:
    ConnectionConfig object with SSL configuration

Examples:
    >>> from atom.connection.tcpclient import create_ssl_config, TcpClient
    >>> config = create_ssl_config(verify_ssl=False)
    >>> client = TcpClient(config)
)");

    // Connection state helper functions
    m.def(
        "connection_state_to_string",
        [](atom::async::connection::ConnectionState state) {
            switch (state) {
                case atom::async::connection::ConnectionState::Disconnected:
                    return "Disconnected";
                case atom::async::connection::ConnectionState::Connecting:
                    return "Connecting";
                case atom::async::connection::ConnectionState::Connected:
                    return "Connected";
                case atom::async::connection::ConnectionState::Reconnecting:
                    return "Reconnecting";
                case atom::async::connection::ConnectionState::Failed:
                    return "Failed";
                default:
                    return "Unknown";
            }
        },
        py::arg("state"),
        R"(Converts a ConnectionState enum to a human-readable string.

Args:
    state: ConnectionState enum value

Returns:
    String representation of the connection state

Examples:
    >>> from atom.connection.tcpclient import connection_state_to_string, ConnectionState
    >>> print(connection_state_to_string(ConnectionState.Connected))
    'Connected'
)");
}

/**
 * @brief Adds comprehensive module documentation.
 *
 * This function adds detailed documentation and examples for the entire
 * tcpclient module to help users understand its capabilities.
 *
 * @param m The pybind11 module to add documentation to
 */
void addModuleDocumentation(py::module_& m) {
    // Module documentation is set in the PYBIND11_MODULE macro
    // Add module-level constants
    m.def(
        "get_default_connect_timeout", []() { return 5000; },
        "Gets the default connection timeout in milliseconds");
    m.def(
        "get_default_read_timeout", []() { return 5000; },
        "Gets the default read timeout in milliseconds");
    m.def(
        "get_default_write_timeout", []() { return 5000; },
        "Gets the default write timeout in milliseconds");
    m.def(
        "get_default_heartbeat_interval", []() { return 30000; },
        "Gets the default heartbeat interval in milliseconds");
    m.def(
        "get_default_reconnect_attempts", []() { return 3; },
        "Gets the default number of reconnection attempts");
    m.def(
        "get_default_reconnect_delay", []() { return 1000; },
        "Gets the default reconnection delay in milliseconds");
}

PYBIND11_MODULE(tcpclient, m) {
    m.doc() = R"(TCP client module for the atom package.

This module provides asynchronous TCP client functionality with modern C++20 features,
including SSL/TLS support, automatic reconnection, proxy support, and comprehensive
event handling.

Key Features:
- Asynchronous TCP connections with coroutine support
- SSL/TLS encryption with certificate validation
- Automatic reconnection with configurable retry logic
- Proxy server support (HTTP/SOCKS)
- Heartbeat/keep-alive functionality
- Comprehensive connection statistics
- Event-driven architecture with callbacks
- Context manager support for resource management

Classes:
- TcpClient: Main asynchronous TCP client class
- ConnectionConfig: Configuration for client behavior
- ConnectionStats: Connection usage statistics
- ConnectionState: Enumeration of connection states
- ProxyConfig: Proxy server configuration

Quick Start Example:
    >>> from atom.connection.tcpclient import TcpClient, ConnectionConfig
    >>>
    >>> # Create client with SSL
    >>> config = ConnectionConfig()
    >>> config.use_ssl = True
    >>> config.auto_reconnect = True
    >>> client = TcpClient(config)
    >>>
    >>> # Set up event handlers
    >>> def on_connected():
    ...     print("Connected!")
    ...     client.send_string("Hello, server!")
    >>>
    >>> def on_data_received(data):
    ...     print(f"Received: {data}")
    >>>
    >>> client.set_on_connected_callback(on_connected)
    >>> client.set_on_data_received_callback(on_data_received)
    >>>
    >>> # Connect and use
    >>> with client:
    ...     if client.connect("example.com", 443):
    ...         # Connection is managed automatically
    ...         pass
)";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind core enums and data structures
    bindConnectionState(m);
    bindConnectionConfig(m);
    bindProxyConfig(m);
    bindConnectionStats(m);
    bindTcpClient(m);

    // Bind utility functions
    bindUtilityFunctions(m);

    // Add module documentation
    addModuleDocumentation(m);
}
