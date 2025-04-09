#include "atom/connection/async_udpclient.hpp"

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

    // Socket option enum
    py::enum_<atom::async::connection::UdpClient::SocketOption>(
        m, "SocketOption", "Socket configuration options")
        .value("BROADCAST",
               atom::async::connection::UdpClient::SocketOption::Broadcast,
               "Option to enable/disable broadcast")
        .value("REUSE_ADDRESS",
               atom::async::connection::UdpClient::SocketOption::ReuseAddress,
               "Option to enable/disable address reuse")
        .value(
            "RECEIVE_BUFFER_SIZE",
            atom::async::connection::UdpClient::SocketOption::ReceiveBufferSize,
            "Option to set receive buffer size")
        .value("SEND_BUFFER_SIZE",
               atom::async::connection::UdpClient::SocketOption::SendBufferSize,
               "Option to set send buffer size")
        .value("RECEIVE_TIMEOUT",
               atom::async::connection::UdpClient::SocketOption::ReceiveTimeout,
               "Option to set receive timeout")
        .value("SEND_TIMEOUT",
               atom::async::connection::UdpClient::SocketOption::SendTimeout,
               "Option to set send timeout")
        .export_values();

    // Statistics structure
    py::class_<atom::async::connection::UdpClient::Statistics>(
        m, "Statistics", "UDP client statistics")
        .def(py::init<>())
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
            "Time when statistics tracking started")
        .def("reset", &atom::async::connection::UdpClient::Statistics::reset,
             "Reset all statistics to zero");

    // UdpClient class binding
    py::class_<atom::async::connection::UdpClient>(
        m, "UdpClient",
        R"(A UDP client for sending and receiving datagrams.

This class provides methods for UDP socket communication, including sending 
and receiving datagrams, multicast support, and asynchronous operations.

Args:
    use_ipv6: Whether to use IPv6 (True) or IPv4 (False). Default is IPv4.

Examples:
    >>> from atom.connection.udp import UdpClient
    >>> client = UdpClient()
    >>> client.bind(8000)  # Bind to port 8000 on all interfaces
    >>> client.send("127.0.0.1", 9000, "Hello, UDP!")
)")
        .def(py::init<>(), "Constructs a new UDP client using IPv4.")
        .def(py::init<bool>(), py::arg("use_ipv6"),
             "Constructs a new UDP client with specified IP version.")
        .def("bind", &atom::async::connection::UdpClient::bind, 
             py::arg("port"), py::arg("address") = "",
             R"(Binds the socket to a specific port.

Args:
    port: The port to bind to
    address: Optional address to bind to (default: any)

Returns:
    True if successful, False otherwise
)")
        .def("send", py::overload_cast<const std::string&, int, const std::vector<char>&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends binary data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The binary data to send as bytes

Returns:
    True if successful, False otherwise
)")
        .def("send", py::overload_cast<const std::string&, int, const std::string&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends string data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The string data to send

Returns:
    True if successful, False otherwise

Examples:
    >>> client.send("127.0.0.1", 9000, "Hello, UDP!")
)")
        .def("send_with_timeout", &atom::async::connection::UdpClient::sendWithTimeout,
             py::arg("host"), py::arg("port"), py::arg("data"), py::arg("timeout"),
             R"(Sends data with timeout.

Args:
    host: The target host
    port: The target port
    data: The data to send
    timeout: Timeout duration in milliseconds

Returns:
    True if successful, False otherwise

Examples:
    >>> import time
    >>> from atom.connection.udp import UdpClient
    >>> client = UdpClient()
    >>> client.send_with_timeout("127.0.0.1", 9000, b"Hello", 500)  # 500ms timeout
)")
        .def("batch_send", &atom::async::connection::UdpClient::batchSend,
             py::arg("destinations"), py::arg("data"),
             R"(Batch sends data to multiple destinations.

Args:
    destinations: List of (host, port) pairs
    data: The data to send

Returns:
    Number of successful transmissions

Examples:
    >>> destinations = [("192.168.1.100", 9000), ("192.168.1.101", 9000)]
    >>> client.batch_send(destinations, b"Hello to all")
)")
        .def("receive", [](atom::async::connection::UdpClient& self, 
                          size_t size, 
                          std::chrono::milliseconds timeout) {
        std::string remoteHost;
        int remotePort;
        std::vector<char> data =
            self.receive(size, remoteHost, remotePort, timeout);
        return py::make_tuple(py::bytes(data.data(), data.size()), remoteHost,
                              remotePort);
         }, py::arg("size"), py::arg("timeout") = std::chrono::milliseconds::zero(),
         R"(Receives data synchronously.

Args:
    size: Buffer size for received data
    timeout: Optional timeout in milliseconds (zero means no timeout)

Returns:
    Tuple of (data, sender_host, sender_port)

Examples:
    >>> data, host, port = client.receive(4096, 1000)  # 1 second timeout
    >>> print(f"Received {len(data)} bytes from {host}:{port}")
)")
        .def("set_on_data_received_callback", &atom::async::connection::UdpClient::setOnDataReceivedCallback,
             py::arg("callback"),
             R"(Sets callback for data reception.

Args:
    callback: Function that takes (data, host, port) parameters

Examples:
    >>> def on_data(data, host, port):
    ...     print(f"Received {len(data)} bytes from {host}:{port}")
    ...
    >>> client.set_on_data_received_callback(on_data)
)")
        .def("set_on_error_callback", &atom::async::connection::UdpClient::setOnErrorCallback,
             py::arg("callback"),
             R"(Sets callback for errors.

Args:
    callback: Function that takes (error_message, error_code) parameters

Examples:
    >>> def on_error(message, code):
    ...     print(f"Error {code}: {message}")
    ...
    >>> client.set_on_error_callback(on_error)
)")
        .def("set_on_status_callback", &atom::async::connection::UdpClient::setOnStatusCallback,
             py::arg("callback"),
             R"(Sets callback for status updates.

Args:
    callback: Function that takes (status_message) parameter

Examples:
    >>> def on_status(status):
    ...     print(f"Status: {status}")
    ...
    >>> client.set_on_status_callback(on_status)
)")
        .def("start_receiving", &atom::async::connection::UdpClient::startReceiving,
             py::arg("buffer_size") = 4096,
             R"(Starts asynchronous data reception.

Args:
    buffer_size: Size of the receive buffer (default: 4096)

Examples:
    >>> client.bind(8000)
    >>> client.set_on_data_received_callback(lambda data, host, port: print(f"Got {len(data)} bytes"))
    >>> client.start_receiving()
    >>> # The callback will be executed when data is received
)")
        .def("stop_receiving", &atom::async::connection::UdpClient::stopReceiving,
             R"(Stops asynchronous data reception.

Examples:
    >>> client.stop_receiving()
)")
        .def("set_socket_option", &atom::async::connection::UdpClient::setSocketOption,
             py::arg("option"), py::arg("value"),
             R"(Sets a socket option.

Args:
    option: The option to set (from SocketOption enum)
    value: The option value

Returns:
    True if successful, False otherwise

Examples:
    >>> from atom.connection.udp import UdpClient, SocketOption
    >>> client = UdpClient()
    >>> client.set_socket_option(SocketOption.BROADCAST, 1)  # Enable broadcasting
)")
        .def("set_ttl", &atom::async::connection::UdpClient::setTTL,
             py::arg// filepath: d:\msys64\home\qwdma\Atom\python\connection\udp.cpp
#include "atom/connection/async_udpclient.hpp"

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

        // Socket option enum
        py::enum_<atom::async::connection::UdpClient::SocketOption>(
            m, "SocketOption", "Socket configuration options")
            .value("BROADCAST",
                   atom::async::connection::UdpClient::SocketOption::Broadcast,
                   "Option to enable/disable broadcast")
            .value(
                "REUSE_ADDRESS",
                atom::async::connection::UdpClient::SocketOption::ReuseAddress,
                "Option to enable/disable address reuse")
            .value("RECEIVE_BUFFER_SIZE",
                   atom::async::connection::UdpClient::SocketOption::
                       ReceiveBufferSize,
                   "Option to set receive buffer size")
            .value("SEND_BUFFER_SIZE",
                   atom::async::connection::UdpClient::SocketOption::
                       SendBufferSize,
                   "Option to set send buffer size")
            .value("RECEIVE_TIMEOUT",
                   atom::async::connection::UdpClient::SocketOption::
                       ReceiveTimeout,
                   "Option to set receive timeout")
            .value(
                "SEND_TIMEOUT",
                atom::async::connection::UdpClient::SocketOption::SendTimeout,
                "Option to set send timeout")
            .export_values();

        // Statistics structure
        py::class_<atom::async::connection::UdpClient::Statistics>(
            m, "Statistics", "UDP client statistics")
            .def(py::init<>())
            .def_readonly(
                "packets_sent",
                &atom::async::connection::UdpClient::Statistics::packets_sent,
                "Number of packets sent")
            .def_readonly("packets_received",
                          &atom::async::connection::UdpClient::Statistics::
                              packets_received,
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
                "Time when statistics tracking started")
            .def("reset",
                 &atom::async::connection::UdpClient::Statistics::reset,
                 "Reset all statistics to zero");

    // UdpClient class binding
    py::class_<atom::async::connection::UdpClient>(
        m, "UdpClient",
        R"(A UDP client for sending and receiving datagrams.

This class provides methods for UDP socket communication, including sending 
and receiving datagrams, multicast support, and asynchronous operations.

Args:
    use_ipv6: Whether to use IPv6 (True) or IPv4 (False). Default is IPv4.

Examples:
    >>> from atom.connection.udp import UdpClient
    >>> client = UdpClient()
    >>> client.bind(8000)  # Bind to port 8000 on all interfaces
    >>> client.send("127.0.0.1", 9000, "Hello, UDP!")
)")
        .def(py::init<>(), "Constructs a new UDP client using IPv4.")
        .def(py::init<bool>(), py::arg("use_ipv6"),
             "Constructs a new UDP client with specified IP version.")
        .def("bind", &atom::async::connection::UdpClient::bind, 
             py::arg("port"), py::arg("address") = "",
             R"(Binds the socket to a specific port.

Args:
    port: The port to bind to
    address: Optional address to bind to (default: any)

Returns:
    True if successful, False otherwise
)")
        .def("send", py::overload_cast<const std::string&, int, const std::vector<char>&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends binary data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The binary data to send as bytes

Returns:
    True if successful, False otherwise
)")
        .def("send", py::overload_cast<const std::string&, int, const std::string&>(
                 &atom::async::connection::UdpClient::send),
             py::arg("host"), py::arg("port"), py::arg("data"),
             R"(Sends string data to a specified host and port.

Args:
    host: The target host
    port: The target port
    data: The string data to send

Returns:
    True if successful, False otherwise

Examples:
    >>> client.send("127.0.0.1", 9000, "Hello, UDP!")
)")
        .def("send_with_timeout", &atom::async::connection::UdpClient::sendWithTimeout,
             py::arg("host"), py::arg("port"), py::arg("data"), py::arg("timeout"),
             R"(Sends data with timeout.

Args:
    host: The target host
    port: The target port
    data: The data to send
    timeout: Timeout duration in milliseconds

Returns:
    True if successful, False otherwise

Examples:
    >>> import time
    >>> from atom.connection.udp import UdpClient
    >>> client = UdpClient()
    >>> client.send_with_timeout("127.0.0.1", 9000, b"Hello", 500)  # 500ms timeout
)")
        .def("batch_send", &atom::async::connection::UdpClient::batchSend,
             py::arg("destinations"), py::arg("data"),
             R"(Batch sends data to multiple destinations.

Args:
    destinations: List of (host, port) pairs
    data: The data to send

Returns:
    Number of successful transmissions

Examples:
    >>> destinations = [("192.168.1.100", 9000), ("192.168.1.101", 9000)]
    >>> client.batch_send(destinations, b"Hello to all")
)")
        .def("receive", [](atom::async::connection::UdpClient& self, 
                          size_t size, 
                          std::chrono::milliseconds timeout) {
            std::string remoteHost;
            int remotePort;
            std::vector<char> data =
                self.receive(size, remoteHost, remotePort, timeout);
            return py::make_tuple(py::bytes(data.data(), data.size()),
                                  remoteHost, remotePort);
         }, py::arg("size"), py::arg("timeout") = std::chrono::milliseconds::zero(),
         R"(Receives data synchronously.

Args:
    size: Buffer size for received data
    timeout: Optional timeout in milliseconds (zero means no timeout)

Returns:
    Tuple of (data, sender_host, sender_port)

Examples:
    >>> data, host, port = client.receive(4096, 1000)  # 1 second timeout
    >>> print(f"Received {len(data)} bytes from {host}:{port}")
)")
        .def("set_on_data_received_callback", &atom::async::connection::UdpClient::setOnDataReceivedCallback,
             py::arg("callback"),
             R"(Sets callback for data reception.

Args:
    callback: Function that takes (data, host, port) parameters

Examples:
    >>> def on_data(data, host, port):
    ...     print(f"Received {len(data)} bytes from {host}:{port}")
    ...
    >>> client.set_on_data_received_callback(on_data)
)")
        .def("set_on_error_callback", &atom::async::connection::UdpClient::setOnErrorCallback,
             py::arg("callback"),
             R"(Sets callback for errors.

Args:
    callback: Function that takes (error_message, error_code) parameters

Examples:
    >>> def on_error(message, code):
    ...     print(f"Error {code}: {message}")
    ...
    >>> client.set_on_error_callback(on_error)
)")
        .def("set_on_status_callback", &atom::async::connection::UdpClient::setOnStatusCallback,
             py::arg("callback"),
             R"(Sets callback for status updates.

Args:
    callback: Function that takes (status_message) parameter

Examples:
    >>> def on_status(status):
    ...     print(f"Status: {status}")
    ...
    >>> client.set_on_status_callback(on_status)
)")
        .def("start_receiving", &atom::async::connection::UdpClient::startReceiving,
             py::arg("buffer_size") = 4096,
             R"(Starts asynchronous data reception.

Args:
    buffer_size: Size of the receive buffer (default: 4096)

Examples:
    >>> client.bind(8000)
    >>> client.set_on_data_received_callback(lambda data, host, port: print(f"Got {len(data)} bytes"))
    >>> client.start_receiving()
    >>> # The callback will be executed when data is received
)")
        .def("stop_receiving", &atom::async::connection::UdpClient::stopReceiving,
             R"(Stops asynchronous data reception.

Examples:
    >>> client.stop_receiving()
)")
        .def("set_socket_option", &atom::async::connection::UdpClient::setSocketOption,
             py::arg("option"), py::arg("value"),
             R"(Sets a socket option.

Args:
    option: The option to set (from SocketOption enum)
    value: The option value

Returns:
    True if successful, False otherwise

Examples:
    >>> from atom.connection.udp import UdpClient, SocketOption
    >>> client = UdpClient()
    >>> client.set_socket_option(SocketOption.BROADCAST, 1)  # Enable broadcasting
)")
        .def("set_ttl", &atom::async::connection::UdpClient::setTTL,
             py::arg