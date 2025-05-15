#include "atom/connection/udpclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(udp, m) {
    m.doc() = "UDP client module for the atom package";

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

    // Register UdpError enum
    py::enum_<atom::connection::UdpError>(m, "UdpError", "UDP error codes")
        .value("NONE", atom::connection::UdpError::None, "No error")
        .value("SOCKET_CREATION_FAILED",
               atom::connection::UdpError::SocketCreationFailed,
               "Socket creation failed")
        .value("BIND_FAILED", atom::connection::UdpError::BindFailed,
               "Binding to port failed")
        .value("SEND_FAILED", atom::connection::UdpError::SendFailed,
               "Send operation failed")
        .value("RECEIVE_FAILED", atom::connection::UdpError::ReceiveFailed,
               "Receive operation failed")
        .value("HOST_NOT_FOUND", atom::connection::UdpError::HostNotFound,
               "Host not found")
        .value("TIMEOUT", atom::connection::UdpError::Timeout,
               "Operation timed out")
        .value("INVALID_PARAMETER",
               atom::connection::UdpError::InvalidParameter,
               "Invalid parameter")
        .value("INTERNAL_ERROR", atom::connection::UdpError::InternalError,
               "Internal error")
        .value("MULTICAST_ERROR", atom::connection::UdpError::MulticastError,
               "Multicast operation failed")
        .value("BROADCAST_ERROR", atom::connection::UdpError::BroadcastError,
               "Broadcast operation failed")
        .value("NOT_INITIALIZED", atom::connection::UdpError::NotInitialized,
               "Client not initialized")
        .value("NOT_SUPPORTED", atom::connection::UdpError::NotSupported,
               "Operation not supported")
        .export_values();

    // RemoteEndpoint structure
    py::class_<atom::connection::RemoteEndpoint>(m, "RemoteEndpoint",
                                                 "UDP remote endpoint")
        .def(py::init<>())
        .def(py::init<std::string, uint16_t>(), py::arg("host"),
             py::arg("port"))
        .def_readwrite("host", &atom::connection::RemoteEndpoint::host,
                       "Remote host address")
        .def_readwrite("port", &atom::connection::RemoteEndpoint::port,
                       "Remote port number")
        .def("__eq__", [](const atom::connection::RemoteEndpoint& self,
                          const atom::connection::RemoteEndpoint& other) {
            return self == other;
        });

    // SocketOptions structure
    py::class_<atom::connection::SocketOptions>(
        m, "SocketOptions", "UDP socket configuration options")
        .def(py::init<>())
        .def_readwrite("reuse_address",
                       &atom::connection::SocketOptions::reuseAddress,
                       "Enable address reuse")
        .def_readwrite("reuse_port",
                       &atom::connection::SocketOptions::reusePort,
                       "Enable port reuse")
        .def_readwrite("broadcast", &atom::connection::SocketOptions::broadcast,
                       "Enable broadcast")
        .def_readwrite("send_buffer_size",
                       &atom::connection::SocketOptions::sendBufferSize,
                       "Send buffer size (0 = system default)")
        .def_readwrite("receive_buffer_size",
                       &atom::connection::SocketOptions::receiveBufferSize,
                       "Receive buffer size (0 = system default)")
        .def_readwrite("ttl", &atom::connection::SocketOptions::ttl,
                       "Time-to-live value (0 = system default)")
        .def_readwrite("non_blocking",
                       &atom::connection::SocketOptions::nonBlocking,
                       "Use non-blocking sockets")
        .def_readwrite("send_timeout",
                       &atom::connection::SocketOptions::sendTimeout,
                       "Send timeout (0 = no timeout)")
        .def_readwrite("receive_timeout",
                       &atom::connection::SocketOptions::receiveTimeout,
                       "Receive timeout (0 = no timeout)");

    // Statistics structure
    py::class_<atom::connection::UdpStatistics>(m, "UdpStatistics",
                                                "UDP client statistics")
        .def(py::init<>())
        .def_readonly("packets_received",
                      &atom::connection::UdpStatistics::packetsReceived,
                      "Number of packets received")
        .def_readonly("packets_sent",
                      &atom::connection::UdpStatistics::packetsSent,
                      "Number of packets sent")
        .def_readonly("bytes_received",
                      &atom::connection::UdpStatistics::bytesReceived,
                      "Number of bytes received")
        .def_readonly("bytes_sent", &atom::connection::UdpStatistics::bytesSent,
                      "Number of bytes sent")
        .def_readonly("receive_errors",
                      &atom::connection::UdpStatistics::receiveErrors,
                      "Number of receive errors")
        .def_readonly("send_errors",
                      &atom::connection::UdpStatistics::sendErrors,
                      "Number of send errors")
        .def_readonly("last_activity",
                      &atom::connection::UdpStatistics::lastActivity,
                      "Time of last activity")
        .def("reset", &atom::connection::UdpStatistics::reset,
             "Reset all statistics to zero");

    // UdpClient class binding
    py::class_<atom::connection::UdpClient>(
        m, "UdpClient",
        R"(A modern UDP client for sending and receiving datagrams.

This class provides methods for UDP socket communication, including sending 
and receiving datagrams, multicast support, broadcast support, and asynchronous operations.

Examples:
    >>> from atom.connection.udp import UdpClient
    >>> client = UdpClient()  # Create client with ephemeral port
    >>> client.bind(8000)     # Or bind to specific port
    >>> client.send(RemoteEndpoint("127.0.0.1", 9000), "Hello, UDP!")
)")
        .def(py::init<>(), "Constructs a new UDP client with ephemeral port.")
        .def(py::init<uint16_t>(), py::arg("port"),
             "Constructs a new UDP client bound to a specific port.")
        .def(py::init<uint16_t, const atom::connection::SocketOptions&>(),
             py::arg("port"), py::arg("options"),
             "Constructs a new UDP client with specific port and socket "
             "options.")
        .def(
            "bind",
            [](atom::connection::UdpClient& self, uint16_t port) {
                auto result = self.bind(port);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to bind: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("port"),
            R"(Binds the socket to a specific port.

Args:
    port: The port to bind to

Returns:
    True if successful

Raises:
    RuntimeError: If binding fails
)")
        .def(
            "send",
            [](atom::connection::UdpClient& self,
               const atom::connection::RemoteEndpoint& endpoint,
               const std::string& data) {
                auto result = self.send(endpoint, data);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to send: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("endpoint"), py::arg("data"),
            R"(Sends data to a specified endpoint.

Args:
    endpoint: The target endpoint (host and port)
    data: The data to send (string or bytes)

Returns:
    Number of bytes sent

Raises:
    RuntimeError: If sending fails
)")
        .def(
            "send",
            [](atom::connection::UdpClient& self, const std::string& host,
               uint16_t port, const std::string& data) {
                atom::connection::RemoteEndpoint endpoint{host, port};
                auto result = self.send(endpoint, data);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to send: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("host"), py::arg("port"), py::arg("data"),
            R"(Sends data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The data to send (string or bytes)

Returns:
    Number of bytes sent

Raises:
    RuntimeError: If sending fails
)")
        .def(
            "send_broadcast",
            [](atom::connection::UdpClient& self, uint16_t port,
               const std::string& data) {
                auto result = self.sendBroadcast(port, data);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to broadcast: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("port"), py::arg("data"),
            R"(Sends a broadcast message to all hosts on the network.

Args:
    port: The target port
    data: The data to broadcast

Returns:
    Number of bytes sent

Raises:
    RuntimeError: If broadcasting fails
)")
        .def(
            "send_multiple",
            [](atom::connection::UdpClient& self,
               const std::vector<atom::connection::RemoteEndpoint>& endpoints,
               const std::string& data) {
                auto result = self.sendMultiple(
                    endpoints, std::span(data.data(), data.size()));
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to send multiple: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("endpoints"), py::arg("data"),
            R"(Sends data to multiple destinations at once.

Args:
    endpoints: List of destination endpoints
    data: The data to send

Returns:
    Number of successful transmissions

Raises:
    RuntimeError: If the operation fails completely
)")
        .def(
            "receive",
            [](atom::connection::UdpClient& self, size_t maxSize,
               const std::chrono::milliseconds& timeout) {
                auto result = self.receive(maxSize, timeout);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to receive: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                auto [data, endpoint] = result.value();
                return py::make_tuple(py::bytes(data.data(), data.size()),
                                      endpoint);
            },
            py::arg("max_size"),
            py::arg("timeout") = std::chrono::milliseconds::zero(),
            R"(Receives data synchronously.

Args:
    max_size: Maximum buffer size for received data
    timeout: Optional timeout in milliseconds (zero means no timeout)

Returns:
    Tuple of (data as bytes, sender endpoint)

Raises:
    RuntimeError: If receiving fails
)")
        .def(
            "join_multicast_group",
            [](atom::connection::UdpClient& self,
               const std::string& groupAddress) {
                auto result = self.joinMulticastGroup(groupAddress);
                if (!result.has_value()) {
                    throw std::runtime_error(
                        "Failed to join multicast group: " +
                        std::to_string(
                            static_cast<int>(result.error().error())));
                }
                return result.value();
            },
            py::arg("group_address"),
            R"(Joins a multicast group to receive multicasted messages.

Args:
    group_address: The multicast group address (e.g., "224.0.0.1")

Returns:
    True if successful

Raises:
    RuntimeError: If joining fails
)")
        .def(
            "leave_multicast_group",
            [](atom::connection::UdpClient& self,
               const std::string& groupAddress) {
                auto result = self.leaveMulticastGroup(groupAddress);
                if (!result.has_value()) {
                    throw std::runtime_error(
                        "Failed to leave multicast group: " +
                        std::to_string(
                            static_cast<int>(result.error().error())));
                }
                return result.value();
            },
            py::arg("group_address"),
            R"(Leaves a previously joined multicast group.

Args:
    group_address: The multicast group address

Returns:
    True if successful

Raises:
    RuntimeError: If leaving fails
)")
        .def(
            "send_to_multicast_group",
            [](atom::connection::UdpClient& self,
               const std::string& groupAddress, uint16_t port,
               const std::string& data) {
                auto result = self.sendToMulticastGroup(
                    groupAddress, port, std::span(data.data(), data.size()));
                if (!result.has_value()) {
                    throw std::runtime_error(
                        "Failed to send to multicast group: " +
                        std::to_string(
                            static_cast<int>(result.error().error())));
                }
                return result.value();
            },
            py::arg("group_address"), py::arg("port"), py::arg("data"),
            R"(Sends data to a multicast group.

Args:
    group_address: The multicast group address
    port: The target port
    data: The data to send

Returns:
    Number of bytes sent

Raises:
    RuntimeError: If sending fails
)")
        .def(
            "set_on_data_received_callback",
            [](atom::connection::UdpClient& self, py::function callback) {
                self.setOnDataReceivedCallback(
                    [callback = std::move(callback)](
                        std::span<const char> data,
                        const atom::connection::RemoteEndpoint& endpoint) {
                        py::gil_scoped_acquire
                            gil;  // Acquire GIL before calling Python code
                        try {
                            callback(py::bytes(data.data(), data.size()),
                                     endpoint);
                        } catch (const py::error_already_set& e) {
                            // Log Python exception
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for data reception.

Args:
    callback: Function that takes (data, endpoint) parameters

Examples:
    >>> def on_data(data, endpoint):
    ...     print(f"Received {len(data)} bytes from {endpoint.host}:{endpoint.port}")
    ...
    >>> client.set_on_data_received_callback(on_data)
)")
        .def(
            "set_on_error_callback",
            [](atom::connection::UdpClient& self, py::function callback) {
                self.setOnErrorCallback([callback = std::move(callback)](
                                            atom::connection::UdpError error,
                                            const std::string& message) {
                    py::gil_scoped_acquire
                        gil;  // Acquire GIL before calling Python code
                    try {
                        callback(error, message);
                    } catch (const py::error_already_set& e) {
                        // Log Python exception
                        PyErr_Print();
                    }
                });
            },
            py::arg("callback"),
            R"(Sets callback for errors.

Args:
    callback: Function that takes (error_code, error_message) parameters

Examples:
    >>> def on_error(error, message):
    ...     print(f"Error {error}: {message}")
    ...
    >>> client.set_on_error_callback(on_error)
)")
        .def(
            "set_on_status_change_callback",
            [](atom::connection::UdpClient& self, py::function callback) {
                self.setOnStatusChangeCallback(
                    [callback = std::move(callback)](bool status) {
                        py::gil_scoped_acquire
                            gil;  // Acquire GIL before calling Python code
                        try {
                            callback(status);
                        } catch (const py::error_already_set& e) {
                            // Log Python exception
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for status changes.

Args:
    callback: Function that takes (status) parameter where status is a boolean

Examples:
    >>> def on_status(status):
    ...     print(f"Connection status: {'active' if status else 'inactive'}")
    ...
    >>> client.set_on_status_change_callback(on_status)
)")
        .def(
            "start_receiving",
            [](atom::connection::UdpClient& self, size_t bufferSize) {
                auto result = self.startReceiving(bufferSize);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to start receiving: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("buffer_size") = 8192,
            R"(Starts asynchronous data reception.

Args:
    buffer_size: Size of the receive buffer (default: 8192)

Returns:
    True if background receiving started successfully

Raises:
    RuntimeError: If starting receiver fails
)")
        .def("stop_receiving", &atom::connection::UdpClient::stopReceiving,
             R"(Stops asynchronous data reception.

Examples:
    >>> client.stop_receiving()
)")
        .def("is_receiving", &atom::connection::UdpClient::isReceiving,
             R"(Check if the client is currently receiving data asynchronously.

Returns:
    True if receiving, False otherwise
)")
        .def("get_statistics", &atom::connection::UdpClient::getStatistics,
             R"(Get socket statistics.

Returns:
    UdpStatistics object with current statistics
)")
        .def("reset_statistics", &atom::connection::UdpClient::resetStatistics,
             R"(Reset socket statistics to zero.)")
        .def(
            "set_socket_options",
            [](atom::connection::UdpClient& self,
               const atom::connection::SocketOptions& options) {
                auto result = self.setSocketOptions(options);
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to set socket options: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            py::arg("options"),
            R"(Configure socket options.

Args:
    options: SocketOptions object with desired configuration

Returns:
    True if options were set successfully

Raises:
    RuntimeError: If setting options fails
)")
        .def("close", &atom::connection::UdpClient::close,
             R"(Close the socket and clean up resources.)")
        .def("is_bound", &atom::connection::UdpClient::isBound,
             R"(Check if socket is bound to a port.

Returns:
    True if socket is bound, False otherwise
)")
        .def(
            "get_local_port",
            [](atom::connection::UdpClient& self) {
                auto result = self.getLocalPort();
                if (!result.has_value()) {
                    throw std::runtime_error("Failed to get local port: " +
                                             std::to_string(static_cast<int>(
                                                 result.error().error())));
                }
                return result.value();
            },
            R"(Get the local port the socket is bound to.

Returns:
    The local port number

Raises:
    RuntimeError: If getting the port fails
)")
        .def_static("is_ipv6_supported",
                    &atom::connection::UdpClient::isIPv6Supported,
                    R"(Check if IPv6 is supported on this system.

Returns:
    True if IPv6 is supported, False otherwise
)");
}