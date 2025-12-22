#include "atom/connection/async_udpclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(async_udpclient, m) {
    m.doc() = R"(Asynchronous UDP client module for the atom package.

This module provides asynchronous UDP client functionality using Asio,
with support for multicast, broadcast, statistics tracking, and flexible callbacks.

Key Features:
- Asynchronous UDP socket operations with Asio
- IPv4 and IPv6 support
- Multicast group management
- Broadcast messaging
- Batch sending to multiple destinations
- Statistics tracking
- Socket option configuration
- Callback-based event handling

Classes:
- UdpClient: Main asynchronous UDP client class
- Statistics: UDP communication metrics
- SocketOption: Socket configuration options

Quick Start Example:
    >>> from atom.connection.async_udpclient import UdpClient
    >>>
    >>> # Create client
    >>> client = UdpClient()
    >>> client.bind(8000)
    >>>
    >>> # Set up callback
    >>> def on_data(data, host, port):
    ...     print(f"Received from {host}:{port}: {data}")
    >>>
    >>> client.set_on_data_received_callback(on_data)
    >>>
    >>> # Send and receive
    >>> client.send("127.0.0.1", 9000, "Hello, UDP!")
    >>> client.start_receiving(4096)
    >>> # ... receiving in background ...
    >>> client.stop_receiving()
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

    // SocketOption enum
    py::enum_<atom::async::connection::UdpClient::SocketOption>(
        m, "SocketOption", "Socket configuration options")
        .value("BROADCAST",
               atom::async::connection::UdpClient::SocketOption::Broadcast,
               "Enable broadcast")
        .value("REUSE_ADDRESS",
               atom::async::connection::UdpClient::SocketOption::ReuseAddress,
               "Enable address reuse")
        .value(
            "RECEIVE_BUFFER_SIZE",
            atom::async::connection::UdpClient::SocketOption::ReceiveBufferSize,
            "Set receive buffer size")
        .value("SEND_BUFFER_SIZE",
               atom::async::connection::UdpClient::SocketOption::SendBufferSize,
               "Set send buffer size")
        .value("RECEIVE_TIMEOUT",
               atom::async::connection::UdpClient::SocketOption::ReceiveTimeout,
               "Set receive timeout")
        .value("SEND_TIMEOUT",
               atom::async::connection::UdpClient::SocketOption::SendTimeout,
               "Set send timeout")
        .export_values();

    // Statistics struct
    py::class_<atom::async::connection::UdpClient::Statistics>(
        m, "Statistics",
        R"(Statistics for UDP client operations.

This structure tracks metrics about UDP communication.

Examples:
    >>> stats = client.get_statistics()
    >>> print(f"Packets sent: {stats.packets_sent}")
    >>> print(f"Bytes received: {stats.bytes_received}")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly(
            "packets_sent",
            &atom::async::connection::UdpClient::Statistics::packets_sent,
            "Number of packets sent")
        .def_readonly(
            "packets_received",
            &atom::async::connection::UdpClient::Statistics::packets_received,
            "Number of packets received")
        .def_readonly(
            "bytes_sent",
            &atom::async::connection::UdpClient::Statistics::bytes_sent,
            "Number of bytes sent")
        .def_readonly(
            "bytes_received",
            &atom::async::connection::UdpClient::Statistics::bytes_received,
            "Number of bytes received")
        .def_readonly(
            "start_time",
            &atom::async::connection::UdpClient::Statistics::start_time,
            "Time when statistics started")
        .def("reset", &atom::async::connection::UdpClient::Statistics::reset,
             "Reset all statistics to zero");

    // UdpClient class
    py::class_<atom::async::connection::UdpClient>(
        m, "UdpClient",
        R"(Asynchronous UDP client for sending and receiving datagrams.

This class provides comprehensive UDP communication with asynchronous
operations, multicast support, and flexible configuration.

Examples:
    >>> client = UdpClient()
    >>> client.bind(8000)
    >>>
    >>> # Send data
    >>> if client.send("192.168.1.100", 9000, "Hello!"):
    ...     print("Sent successfully")
    >>>
    >>> # Receive with callback
    >>> def on_data(data, host, port):
    ...     print(f"From {host}:{port}: {data}")
    >>>
    >>> client.set_on_data_received_callback(on_data)
    >>> client.start_receiving()
)")
        .def(py::init<>(), "Constructs a new UDP client.")
        .def(py::init<bool>(), py::arg("use_ipv6"),
             "Constructs a UDP client with specified IP version.")
        .def("bind", &atom::async::connection::UdpClient::bind, py::arg("port"),
             py::arg("address") = "",
             R"(Binds the socket to a specific port.

Args:
    port: The port to bind to
    address: Optional address to bind to (default: any)

Returns:
    True if successful, False otherwise
)")
        .def("send",
             py::overload_cast<const std::string&, int,
                               const std::vector<char>&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The data to send as bytes

Returns:
    True if successful, False otherwise
)")
        .def("send",
             py::overload_cast<const std::string&, int, const std::string&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends string data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The string data to send

Returns:
    True if successful, False otherwise
)")
        .def("send_with_timeout",
             &atom::async::connection::UdpClient::sendWithTimeout,
             py::arg("host"), py::arg("port"), py::arg("data"),
             py::arg("timeout"),
             R"(Sends data with timeout.

Args:
    host: The target host
    port: The target port
    data: The data to send
    timeout: Timeout duration

Returns:
    True if successful, False otherwise
)")
        .def("batch_send", &atom::async::connection::UdpClient::batchSend,
             py::arg("destinations"), py::arg("data"),
             R"(Batch sends data to multiple destinations.

Args:
    destinations: List of (host, port) tuples
    data: The data to send

Returns:
    Number of successful transmissions
)")
        .def(
            "receive",
            [](atom::async::connection::UdpClient& self, size_t size,
               std::chrono::milliseconds timeout) {
                std::string remoteHost;
                int remotePort;
                auto data = self.receive(size, remoteHost, remotePort, timeout);
                return py::make_tuple(py::bytes(data.data(), data.size()),
                                      remoteHost, remotePort);
            },
            py::arg("size"),
            py::arg("timeout") = std::chrono::milliseconds::zero(),
            R"(Receives data synchronously.

Args:
    size: Buffer size for received data
    timeout: Optional timeout (zero means no timeout)

Returns:
    Tuple of (data as bytes, sender host, sender port)
)")
        .def(
            "set_on_data_received_callback",
            [](atom::async::connection::UdpClient& self,
               py::function callback) {
                self.setOnDataReceivedCallback(
                    [callback = std::move(callback)](
                        const std::vector<char>& data, const std::string& host,
                        int port) {
                        py::gil_scoped_acquire gil;
                        try {
                            callback(py::bytes(data.data(), data.size()), host,
                                     port);
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for data reception.

Args:
    callback: Function that takes (data, host, port) parameters
)")
        .def(
            "set_on_error_callback",
            [](atom::async::connection::UdpClient& self,
               py::function callback) {
                self.setOnErrorCallback(
                    [callback = std::move(callback)](const std::string& error,
                                                     int code) {
                        py::gil_scoped_acquire gil;
                        try {
                            callback(error, code);
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("callback"),
            R"(Sets callback for errors.

Args:
    callback: Function that takes (error_message, error_code) parameters
)")
        .def(
            "set_on_status_callback",
            [](atom::async::connection::UdpClient& self,
               py::function callback) {
                self.setOnStatusCallback([callback = std::move(callback)](
                                             const std::string& status) {
                    py::gil_scoped_acquire gil;
                    try {
                        callback(status);
                    } catch (const py::error_already_set& e) {
                        PyErr_Print();
                    }
                });
            },
            py::arg("callback"),
            R"(Sets callback for status updates.

Args:
    callback: Function that takes a status message parameter
)")
        .def("start_receiving",
             &atom::async::connection::UdpClient::startReceiving,
             py::arg("buffer_size") = 4096,
             R"(Starts asynchronous data reception.

Args:
    buffer_size: Size of the receive buffer (default: 4096)
)")
        .def("stop_receiving",
             &atom::async::connection::UdpClient::stopReceiving,
             R"(Stops asynchronous data reception.)")
        .def("set_socket_option",
             &atom::async::connection::UdpClient::setSocketOption,
             py::arg("option"), py::arg("value"),
             R"(Sets a socket option.

Args:
    option: The option to set
    value: The option value

Returns:
    True if successful, False otherwise
)")
        .def("set_ttl", &atom::async::connection::UdpClient::setTTL,
             py::arg("ttl"),
             R"(Sets the Time To Live (TTL) value.

Args:
    ttl: The TTL value

Returns:
    True if successful, False otherwise
)")
        .def("join_multicast_group",
             &atom::async::connection::UdpClient::joinMulticastGroup,
             py::arg("multicast_address"), py::arg("interface_address") = "",
             R"(Joins a multicast group.

Args:
    multicast_address: The multicast group address
    interface_address: The local interface address (optional)

Returns:
    True if successful, False otherwise
)")
        .def("leave_multicast_group",
             &atom::async::connection::UdpClient::leaveMulticastGroup,
             py::arg("multicast_address"), py::arg("interface_address") = "",
             R"(Leaves a multicast group.

Args:
    multicast_address: The multicast group address
    interface_address: The local interface address (optional)

Returns:
    True if successful, False otherwise
)")
        .def("get_local_endpoint",
             &atom::async::connection::UdpClient::getLocalEndpoint,
             R"(Gets the local endpoint information.

Returns:
    Tuple of (address, port)
)")
        .def("is_open", &atom::async::connection::UdpClient::isOpen,
             R"(Checks if the socket is open.

Returns:
    True if open, False otherwise
)")
        .def("close", &atom::async::connection::UdpClient::close,
             R"(Closes the socket.)")
        .def("get_statistics",
             &atom::async::connection::UdpClient::getStatistics,
             R"(Gets current statistics.

Returns:
    Statistics object with current metrics
)")
        .def("reset_statistics",
             &atom::async::connection::UdpClient::resetStatistics,
             R"(Resets statistics to zero.)");

    // Factory function
    m.def(
        "create_udp_client",
        [](int port, bool use_ipv6) {
            auto client =
                std::make_unique<atom::async::connection::UdpClient>(use_ipv6);
            if (client->bind(port)) {
                return client;
            }
            throw std::runtime_error("Failed to bind to port " +
                                     std::to_string(port));
        },
        py::arg("port") = 0, py::arg("use_ipv6") = false,
        R"(Creates a UDP client bound to a specific port.

Args:
    port: The port to bind to (0 for ephemeral port)
    use_ipv6: Whether to use IPv6 (default: False)

Returns:
    A newly created and bound UdpClient

Raises:
    RuntimeError: If binding fails

Examples:
    >>> client = create_udp_client(8000)
)");
}
