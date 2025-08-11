#include "atom/connection/async_tcpclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(tcpclient, m) {
    m.doc() = "TCP client module for the atom package";

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

    // Define ConnectionState enum
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

    // Define ConnectionConfig struct
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

    // Define ProxyConfig struct
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

    // Define ConnectionStats struct
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

    // Define TcpClient class
    py::class_<atom::async::connection::TcpClient>(
        m, "TcpClient",
        R"(A TCP client for asynchronous networking operations.

This class implements a client for TCP/IP networking with support for SSL/TLS,
automatic reconnection, heartbeats, and configurable timeouts.

Args:
    config: Connection configuration (optional)

Examples:
    >>> from atom.connection.tcpclient import TcpClient, ConnectionConfig
    >>> config = ConnectionConfig()
    >>> config.keep_alive = True
    >>> config.connect_timeout = 5000  # 5 seconds
    >>>
    >>> client = TcpClient(config)
    >>> client.connect("example.com", 80)
    >>> client.send_string("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
    >>>
    >>> # Asynchronous receive
    >>> future = client.receive_until('\n', 1000)
    >>> response = future.result()
    >>> print(response)
)")
        .def(py::init<const atom::async::connection::ConnectionConfig&>(),
             py::arg("config") = atom::async::connection::ConnectionConfig{},
             "Construct a new TCP Client with optional configuration")
        .def("connect", &atom::async::connection::TcpClient::connect,
             py::arg("host"), py::arg("port"),
             py::arg("timeout") = std::nullopt,
             R"(Connect to a TCP server.

Args:
    host: Server hostname or IP address
    port: Server port number
    timeout: Optional connection timeout in milliseconds (overrides config timeout)

Returns:
    True if connection initiated successfully

Examples:
    >>> client = TcpClient()
    >>> client.connect("example.com", 80, 5000)  # 5 second timeout
)")
        .def("connect_async", &atom::async::connection::TcpClient::connectAsync,
             py::arg("host"), py::arg("port"),
             R"(Connect asynchronously to a TCP server.

Args:
    host: Server hostname or IP address
    port: Server port number

Returns:
    Future object with connection result (True if successful)

Examples:
    >>> future = client.connect_async("example.com", 80)
    >>> if future.result():
    ...     print("Connected successfully")
)")
        .def("disconnect", &atom::async::connection::TcpClient::disconnect,
             R"(Disconnect from the server.

This method closes the connection and cleans up resources.
)")
        .def("configure_reconnection",
             &atom::async::connection::TcpClient::configureReconnection,
             py::arg("attempts"), py::arg("delay") = std::chrono::seconds(1),
             R"(Configure reconnection behavior.

Args:
    attempts: Number of reconnection attempts (0 to disable)
    delay: Delay between attempts in milliseconds

Examples:
    >>> # Try reconnecting up to 5 times with 2 second delay between attempts
    >>> client.configure_reconnection(5, 2000)
)")
        .def("set_heartbeat_interval",
             &atom::async::connection::TcpClient::setHeartbeatInterval,
             py::arg("interval"), py::arg("data") = std::vector<char>{},
             R"(Set the heartbeat interval and optional data.

Args:
    interval: Interval between heartbeats in milliseconds
    data: Optional heartbeat data to send

Examples:
    >>> # Send heartbeat every 30 seconds
    >>> client.set_heartbeat_interval(30000)
)")
        .def("send", &atom::async::connection::TcpClient::send, py::arg("data"),
             R"(Send raw data to the server.

Args:
    data: Binary data to send as bytes or bytearray

Returns:
    True if send successful

Examples:
    >>> client.send(b"\x01\x02\x03\x04")
)")
        .def("send_string", &atom::async::connection::TcpClient::sendString,
             py::arg("data"),
             R"(Send string data to the server.

Args:
    data: String data to send

Returns:
    True if send successful

Examples:
    >>> client.send_string("Hello, server!")
)")
        .def("send_with_timeout",
             &atom::async::connection::TcpClient::sendWithTimeout,
             py::arg("data"), py::arg("timeout"),
             R"(Send data with a specific timeout.

Args:
    data: Data to send as bytes or bytearray
    timeout: Send timeout in milliseconds

Returns:
    True if send successful

Examples:
    >>> # Send with a 2 second timeout
    >>> client.send_with_timeout(b"Hello", 2000)
)")
        .def("receive", &atom::async::connection::TcpClient::receive,
             py::arg("size"), py::arg("timeout") = std::nullopt,
             R"(Receive specific amount of data.

Args:
    size: Amount of data to receive in bytes
    timeout: Optional receive timeout in milliseconds

Returns:
    Future object with received data

Examples:
    >>> future = client.receive(1024)  # Receive up to 1KB
    >>> data = future.result()  # Blocks until data is received or timeout
    >>> print(f"Received {len(data)} bytes")
)")
        .def("receive_until", &atom::async::connection::TcpClient::receiveUntil,
             py::arg("delimiter"), py::arg("timeout") = std::nullopt,
             R"(Receive data until a delimiter is found.

Args:
    delimiter: Character to stop receiving at when encountered
    timeout: Optional receive timeout in milliseconds

Returns:
    Future object with received string data

Examples:
    >>> # Receive data until a newline character
    >>> future = client.receive_until('\n', 5000)  # 5 second timeout
    >>> line = future.result()
    >>> print(f"Received line: {line}")
)")
        .def("request_response",
             &atom::async::connection::TcpClient::requestResponse,
             py::arg("request"), py::arg("response_size"),
             py::arg("timeout") = std::nullopt,
             R"(Perform a request-response cycle.

Sends a request and waits for a response of a specific size.

Args:
    request: Request data to send
    response_size: Expected response size in bytes
    timeout: Optional operation timeout in milliseconds

Returns:
    Future object with response data

Examples:
    >>> request = b"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
    >>> future = client.request_response(request, 1024)
    >>> response = future.result()
    >>> print(response)
)")
        .def("set_proxy_config",
             &atom::async::connection::TcpClient::setProxyConfig,
             py::arg("config"),
             R"(Set proxy configuration.

Args:
    config: Proxy configuration object

Examples:
    >>> from atom.connection.tcpclient import ProxyConfig
    >>> proxy = ProxyConfig()
    >>> proxy.host = "proxy.example.com"
    >>> proxy.port = 8080
    >>> proxy.enabled = True
    >>> client.set_proxy_config(proxy)
)")
        .def("configure_ssl_certificates",
             &atom::async::connection::TcpClient::configureSslCertificates,
             py::arg("cert_path"), py::arg("key_path"), py::arg("ca_path"),
             R"(Configure SSL certificates.

Args:
    cert_path: Path to certificate file
    key_path: Path to private key file
    ca_path: Path to CA certificate file

Examples:
    >>> client.configure_ssl_certificates(
    ...     "client.crt", "client.key", "ca.crt")
)")
        .def("get_connection_state",
             &atom::async::connection::TcpClient::getConnectionState,
             R"(Get the current connection state.

Returns:
    Current ConnectionState enum value
)")
        .def("is_connected", &atom::async::connection::TcpClient::isConnected,
             R"(Check if client is connected.

Returns:
    True if connected to server
)")
        .def("get_error_message",
             &atom::async::connection::TcpClient::getErrorMessage,
             R"(Get the most recent error message.

Returns:
    Error message string
)")
        .def("get_stats", &atom::async::connection::TcpClient::getStats,
             R"(Get connection statistics.

Returns:
    ConnectionStats object with usage metrics
)")
        .def("reset_stats", &atom::async::connection::TcpClient::resetStats,
             R"(Reset connection statistics.

This method zeroes all counters in the statistics object.
)")
        .def("get_remote_address",
             &atom::async::connection::TcpClient::getRemoteAddress,
             R"(Get the remote endpoint address.

Returns:
    Remote address as string
)")
        .def("get_remote_port",
             &atom::async::connection::TcpClient::getRemotePort,
             R"(Get the remote endpoint port.

Returns:
    Remote port number
)")
        .def("set_property", &atom::async::connection::TcpClient::setProperty,
             py::arg("key"), py::arg("value"),
             R"(Set a property for this connection.

Args:
    key: Property key name
    value: Property value

Examples:
    >>> client.set_property("description", "Main server connection")
)")
        .def("get_property", &atom::async::connection::TcpClient::getProperty,
             py::arg("key"),
             R"(Get a connection property.

Args:
    key: Property key name

Returns:
    Property value or empty string if not found

Examples:
    >>> desc = client.get_property("description")
    >>> print(f"Connection description: {desc}")
)")
        .def("set_on_connecting_callback",
             &atom::async::connection::TcpClient::setOnConnectingCallback,
             py::arg("callback"),
             R"(Set callback for connection initiation.

Args:
    callback: Function to be called when connection is being initiated

Examples:
    >>> def on_connecting():
    ...     print("Connecting to server...")
    ...
    >>> client.set_on_connecting_callback(on_connecting)
)")
        .def("set_on_connected_callback",
             &atom::async::connection::TcpClient::setOnConnectedCallback,
             py::arg("callback"),
             R"(Set callback for successful connection.

Args:
    callback: Function to be called when connection succeeds

Examples:
    >>> def on_connected():
    ...     print("Successfully connected to server")
    ...
    >>> client.set_on_connected_callback(on_connected)
)")
        .def("set_on_disconnected_callback",
             &atom::async::connection::TcpClient::setOnDisconnectedCallback,
             py::arg("callback"),
             R"(Set callback for disconnection.

Args:
    callback: Function to be called when disconnected from server

Examples:
    >>> def on_disconnected():
    ...     print("Disconnected from server")
    ...
    >>> client.set_on_disconnected_callback(on_disconnected)
)")
        .def("set_on_data_received_callback",
             &atom::async::connection::TcpClient::setOnDataReceivedCallback,
             py::arg("callback"),
             R"(Set callback for data reception.

Args:
    callback: Function to be called when data is received, taking the received data as parameter

Examples:
    >>> def on_data_received(data):
    ...     print(f"Received {len(data)} bytes")
    ...
    >>> client.set_on_data_received_callback(on_data_received)
)")
        .def("set_on_error_callback",
             &atom::async::connection::TcpClient::setOnErrorCallback,
             py::arg("callback"),
             R"(Set callback for error reporting.

Args:
    callback: Function to be called on error, taking error message as parameter

Examples:
    >>> def on_error(error_msg):
    ...     print(f"Error: {error_msg}")
    ...
    >>> client.set_on_error_callback(on_error)
)")
        .def("set_on_state_changed_callback",
             &atom::async::connection::TcpClient::setOnStateChangedCallback,
             py::arg("callback"),
             R"(Set callback for state changes.

Args:
    callback: Function taking (new_state, previous_state) as parameters

Examples:
    >>> def on_state_changed(new_state, old_state):
    ...     print(f"State changed from {old_state} to {new_state}")
    ...
    >>> client.set_on_state_changed_callback(on_state_changed)
)")
        .def("set_on_heartbeat_callback",
             &atom::async::connection::TcpClient::setOnHeartbeatCallback,
             py::arg("callback"),
             R"(Set callback for heartbeat events.

Args:
    callback: Function to be called when heartbeat is sent

Examples:
    >>> def on_heartbeat():
    ...     print("Heartbeat sent")
    ...
    >>> client.set_on_heartbeat_callback(on_heartbeat)
)")
        .def(
            "__enter__",
            [](atom::async::connection::TcpClient& self)
                -> atom::async::connection::TcpClient& { return self; },
            "Support for context manager protocol (with statement).")
        .def(
            "__exit__",
            [](atom::async::connection::TcpClient& self, py::object, py::object,
               py::object) { self.disconnect(); },
            "Ensures client is disconnected when exiting context.");

    // Factory function for easier creation
    m.def(
        "create_client",
        [](const std::string& host, int port) {
            auto client =
                std::make_unique<atom::async::connection::TcpClient>();
            if (!client->connect(host, port)) {
                throw std::runtime_error("Failed to connect to " + host + ":" +
                                         std::to_string(port));
            }
            return client;
        },
        py::arg("host"), py::arg("port"),
        R"(Creates and connects a TCP client in one step.

Args:
    host: Server hostname or IP address
    port: Server port number

Returns:
    A connected TcpClient instance

Raises:
    RuntimeError: If connection fails

Examples:
    >>> from atom.connection.tcpclient import create_client
    >>> try:
    ...     client = create_client("example.com", 80)
    ...     # Use the client...
    ... except RuntimeError as e:
    ...     print(f"Connection failed: {e}")
)");

    // Helper function for creating a secure client
    m.def(
        "create_secure_client",
        [](const std::string& host, int port, const std::string& cert_path = "",
           const std::string& key_path = "", const std::string& ca_path = "") {
            atom::async::connection::ConnectionConfig config;
            config.use_ssl = true;

            if (!cert_path.empty()) {
                config.ssl_certificate_path = cert_path;
                config.ssl_private_key_path = key_path;
                config.ca_certificate_path = ca_path;
            }

            auto client =
                std::make_unique<atom::async::connection::TcpClient>(config);
            if (!client->connect(host, port)) {
                throw std::runtime_error("Failed to connect to " + host + ":" +
                                         std::to_string(port));
            }
            return client;
        },
        py::arg("host"), py::arg("port"), py::arg("cert_path") = "",
        py::arg("key_path") = "", py::arg("ca_path") = "",
        R"(Creates and connects a secure (SSL/TLS) TCP client.

Args:
    host: Server hostname or IP address
    port: Server port number
    cert_path: Optional path to certificate file
    key_path: Optional path to private key file
    ca_path: Optional path to CA certificate file

Returns:
    A connected secure TcpClient instance

Raises:
    RuntimeError: If connection fails

Examples:
    >>> from atom.connection.tcpclient import create_secure_client
    >>> try:
    ...     client = create_secure_client("example.com", 443)
    ...     # Use the secure client...
    ... except RuntimeError as e:
    ...     print(f"Secure connection failed: {e}")
)");
}
