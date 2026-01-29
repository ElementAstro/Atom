#include "atom/connection/tcpclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * @brief Binds the TcpClient::Options struct to Python.
 *
 * This function creates Python bindings for the Options struct which
 * provides configuration settings for the synchronous TCP client.
 *
 * @param m The pybind11 module to bind to
 */
void bindTcpClientOptions(py::module_& m) {
    py::class_<atom::connection::TcpClient::Options>(
        m, "TcpClientOptions",
        R"(Configuration options for the synchronous TCP client.

This structure provides various settings to control TCP client behavior,
including IPv6 support, keep-alive settings, and buffer sizes.

Examples:
    >>> from atom.connection.sync_tcpclient import TcpClientOptions
    >>> options = TcpClientOptions()
    >>> options.ipv6_enabled = True
    >>> options.keep_alive = True
    >>> options.receive_buffer_size = 16384
)")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("ipv6_enabled",
                       &atom::connection::TcpClient::Options::ipv6_enabled,
                       "Enable IPv6 support")
        .def_readwrite("keep_alive",
                       &atom::connection::TcpClient::Options::keep_alive,
                       "Enable TCP keepalive")
        .def_readwrite("no_delay",
                       &atom::connection::TcpClient::Options::no_delay,
                       "Disable Nagle's algorithm")
        .def_readwrite(
            "receive_buffer_size",
            &atom::connection::TcpClient::Options::receive_buffer_size,
            "Size of receive buffer in bytes")
        .def_readwrite("send_buffer_size",
                       &atom::connection::TcpClient::Options::send_buffer_size,
                       "Size of send buffer in bytes");
}

PYBIND11_MODULE(sync_tcpclient, m) {
    m.doc() = R"(Synchronous TCP client module for the atom package.

This module provides synchronous TCP client functionality with modern C++20 features,
including coroutine support, SSL/TLS capabilities, and comprehensive error handling.

Key Features:
- Synchronous TCP connections with blocking operations
- Coroutine-based asynchronous operations
- Modern C++20 expected<T, E> error handling
- Configurable socket options and timeouts
- Callback-based event handling
- Resource management with RAII

Classes:
- TcpClient: Main synchronous TCP client class
- TcpClientOptions: Configuration for client behavior
- TaskVoid: Coroutine task for void operations

Quick Start Example:
    >>> from atom.connection.sync_tcpclient import TcpClient, TcpClientOptions
    >>>
    >>> # Create client with custom options
    >>> options = TcpClientOptions()
    >>> options.keep_alive = True
    >>> options.receive_buffer_size = 8192
    >>> client = TcpClient(options)
    >>>
    >>> # Connect to server
    >>> result = client.connect("example.com", 80)
    >>> if result.has_value():
    ...     print("Connected successfully!")
    ...
    ...     # Send data
    ...     data = b"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
    ...     send_result = client.send(data)
    ...     if send_result.has_value():
    ...         print(f"Sent {send_result.value()} bytes")
    ...
    ...         # Receive response
    ...         recv_result = client.receive(1024)
    ...         if recv_result.has_value():
    ...             print(f"Received: {recv_result.value()}")
    >>>
    >>> client.disconnect()
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
        } catch (const std::system_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Bind supporting structures
    bindTcpClientOptions(m);

    // Bind the main TcpClient class
    py::class_<atom::connection::TcpClient>(
        m, "TcpClient",
        R"(Synchronous TCP client for connecting to servers.

This class provides methods for establishing TCP connections, sending and receiving data,
with modern C++20 features including expected<T, E> error handling and coroutine support.

Examples:
    >>> from atom.connection.sync_tcpclient import TcpClient, TcpClientOptions
    >>>
    >>> # Create client with options
    >>> options = TcpClientOptions()
    >>> options.keep_alive = True
    >>> client = TcpClient(options)
    >>>
    >>> # Connect and communicate
    >>> if client.connect("example.com", 80).has_value():
    ...     client.send(b"Hello, server!")
    ...     response = client.receive(1024)
    ...     if response.has_value():
    ...         print(f"Received: {response.value()}")
    >>> client.disconnect()
)")
        .def(py::init<atom::connection::TcpClient::Options>(),
             py::arg("options"),
             "Constructs a TcpClient with the specified options.")
        .def(
            "connect",
            [](atom::connection::TcpClient& self, std::string_view host,
               uint16_t port) {
                auto result = self.connect(host, port);
                if (!result.has_value()) {
                    throw std::runtime_error("Connection failed");
                }
            },
            py::arg("host"), py::arg("port"),
            R"(Connects to a TCP server.

Args:
    host: The hostname or IP address of the server
    port: The port number of the server

Raises:
    RuntimeError: If the connection fails

Examples:
    >>> client.connect("example.com", 80)
)")
        .def(
            "connect",
            [](atom::connection::TcpClient& self, std::string_view host,
               uint16_t port, std::chrono::milliseconds timeout) {
                auto result = self.connect(host, port, timeout);
                if (!result.has_value()) {
                    throw std::runtime_error("Connection failed");
                }
            },
            py::arg("host"), py::arg("port"), py::arg("timeout"),
            R"(Connects to a TCP server with timeout.

Args:
    host: The hostname or IP address of the server
    port: The port number of the server
    timeout: Connection timeout in milliseconds

Raises:
    RuntimeError: If the connection fails

Examples:
    >>> client.connect("example.com", 80, 5000)  # 5 second timeout
)")
        .def("connect_async", &atom::connection::TcpClient::connect_async,
             py::arg("host"), py::arg("port"),
             py::arg("timeout") = std::chrono::milliseconds::zero(),
             R"(Asynchronously connects to a TCP server.

Args:
    host: The hostname or IP address of the server
    port: The port number of the server
    timeout: Optional connection timeout in milliseconds

Returns:
    Task that completes when connection succeeds or fails

Examples:
    >>> task = client.connect_async("example.com", 80)
    >>> # Task will complete asynchronously
)")
        .def("disconnect", &atom::connection::TcpClient::disconnect,
             R"(Disconnects from the server.

This method closes the connection and releases resources.

Examples:
    >>> client.disconnect()
)")
        .def(
            "send",
            [](atom::connection::TcpClient& self, py::bytes data) {
                std::string str_data = data;
                std::span<const char> span_data(str_data.data(),
                                                str_data.size());
                auto result = self.send(span_data);
                if (!result.has_value()) {
                    throw std::runtime_error("Send failed");
                }
                return result.value();
            },
            py::arg("data"),
            R"(Sends data to the server.

Args:
    data: The data to be sent as bytes

Returns:
    Number of bytes sent

Raises:
    RuntimeError: If the send operation fails

Examples:
    >>> bytes_sent = client.send(b"Hello, server!")
    >>> print(f"Sent {bytes_sent} bytes")
)")
        .def(
            "send_async",
            [](atom::connection::TcpClient& self, py::bytes data) {
                std::string str_data = data;
                std::span<const char> span_data(str_data.data(),
                                                str_data.size());
                return self.send_async(span_data);
            },
            py::arg("data"),
            R"(Asynchronously sends data to the server.

Args:
    data: The data to be sent as bytes

Returns:
    Task that completes when send succeeds or fails

Examples:
    >>> task = client.send_async(b"Hello, server!")
)")
        .def(
            "receive",
            [](atom::connection::TcpClient& self, size_t max_size) {
                auto result = self.receive(max_size);
                if (!result.has_value()) {
                    throw std::runtime_error("Receive failed");
                }
                const auto& data = result.value();
                return py::bytes(data.data(), data.size());
            },
            py::arg("max_size"),
            R"(Receives data from the server.

Args:
    max_size: Maximum number of bytes to receive

Returns:
    Received data as bytes

Raises:
    RuntimeError: If the receive operation fails

Examples:
    >>> data = client.receive(1024)
    >>> print(f"Received: {data}")
)")
        .def(
            "receive",
            [](atom::connection::TcpClient& self, size_t max_size,
               std::chrono::milliseconds timeout) {
                auto result = self.receive(max_size, timeout);
                if (!result.has_value()) {
                    throw std::runtime_error("Receive failed");
                }
                const auto& data = result.value();
                return py::bytes(data.data(), data.size());
            },
            py::arg("max_size"), py::arg("timeout"),
            R"(Receives data from the server with timeout.

Args:
    max_size: Maximum number of bytes to receive
    timeout: Receive timeout in milliseconds

Returns:
    Received data as bytes

Raises:
    RuntimeError: If the receive operation fails

Examples:
    >>> data = client.receive(1024, 5000)  # 5 second timeout
)")
        .def("is_connected", &atom::connection::TcpClient::isConnected,
             R"(Checks if the client is connected to the server.

Returns:
    True if connected, False otherwise

Examples:
    >>> if client.is_connected():
    ...     print("Client is connected")
)")
        .def(
            "set_on_connected_callback",
            [](atom::connection::TcpClient& self, py::object callback) {
                self.setOnConnectedCallback([callback]() mutable {
                    try {
                        py::gil_scoped_acquire acquire;
                        callback.operator()();
                    } catch (const py::error_already_set&) {
                        // Handle Python exceptions
                    }
                });
            },
            py::arg("callback"),
            R"(Sets callback for connection events.

Args:
    callback: Function to call when connected

Examples:
    >>> def on_connected():
    ...     print("Connected to server")
    >>> client.set_on_connected_callback(on_connected)
)")
        .def(
            "set_on_disconnected_callback",
            [](atom::connection::TcpClient& self, py::object callback) {
                self.setOnDisconnectedCallback([callback]() mutable {
                    try {
                        py::gil_scoped_acquire acquire;
                        callback.operator()();
                    } catch (const py::error_already_set&) {
                        // Handle Python exceptions
                    }
                });
            },
            py::arg("callback"),
            R"(Sets callback for disconnection events.

Args:
    callback: Function to call when disconnected

Examples:
    >>> def on_disconnected():
    ...     print("Disconnected from server")
    >>> client.set_on_disconnected_callback(on_disconnected)
)")
        .def(
            "set_on_data_received_callback",
            [](atom::connection::TcpClient& self, py::object callback) {
                self.setOnDataReceivedCallback(
                    [callback](std::span<const char> data) mutable {
                        try {
                            py::gil_scoped_acquire acquire;
                            py::bytes py_data(data.data(), data.size());
                            callback.operator()(py_data);
                        } catch (const py::error_already_set&) {
                            // Handle Python exceptions
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for data reception events.

Args:
    callback: Function to call when data is received

Examples:
    >>> def on_data(data):
    ...     print(f"Received {len(data)} bytes")
    >>> client.set_on_data_received_callback(on_data)
)")
        .def(
            "set_on_error_callback",
            [](atom::connection::TcpClient& self, py::object callback) {
                self.setOnErrorCallback(
                    [callback](const std::system_error& error) mutable {
                        try {
                            py::gil_scoped_acquire acquire;
                            callback.operator()(error.what());
                        } catch (const py::error_already_set&) {
                            // Handle Python exceptions
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for error events.

Args:
    callback: Function to call when errors occur

Examples:
    >>> def on_error(error_msg):
    ...     print(f"Error: {error_msg}")
    >>> client.set_on_error_callback(on_error)
)")
        .def("start_receiving", &atom::connection::TcpClient::startReceiving,
             R"(Starts receiving data from the server.

This method begins the asynchronous data reception process.
Data will be delivered via the data received callback.

Examples:
    >>> client.start_receiving()
)")
        .def("stop_receiving", &atom::connection::TcpClient::stopReceiving,
             R"(Stops receiving data from the server.

This method stops the asynchronous data reception process.

Examples:
    >>> client.stop_receiving()
)")
        .def(
            "__enter__",
            [](atom::connection::TcpClient& self)
                -> atom::connection::TcpClient& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::TcpClient& self, py::object, py::object,
               py::object) {
                if (self.isConnected()) {
                    self.disconnect();
                }
            },
            "Ensure client is disconnected when exiting context");
}
