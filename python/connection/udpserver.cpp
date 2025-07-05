#include "atom/connection/async_udpserver.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(udpserver, m) {
    m.doc() = "UDP server module for the atom package";

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

    // Define SocketOption enum
    py::enum_<atom::async::connection::SocketOption>(
        m, "SocketOption", "Socket options for UDP server configuration")
        .value("Broadcast", atom::async::connection::SocketOption::Broadcast,
               "Enable/disable broadcasting")
        .value("ReuseAddress",
               atom::async::connection::SocketOption::ReuseAddress,
               "Enable/disable address reuse")
        .value("ReceiveBufferSize",
               atom::async::connection::SocketOption::ReceiveBufferSize,
               "Set receive buffer size")
        .value("SendBufferSize",
               atom::async::connection::SocketOption::SendBufferSize,
               "Set send buffer size")
        .value("ReceiveTimeout",
               atom::async::connection::SocketOption::ReceiveTimeout,
               "Set receive timeout")
        .value("SendTimeout",
               atom::async::connection::SocketOption::SendTimeout,
               "Set send timeout")
        .export_values();

    // Define Statistics struct
    py::class_<atom::async::connection::UdpSocketHub::Statistics>(
        m, "Statistics",
        R"(Statistics for monitoring UDP server activity.

This structure provides metrics about server usage, including message and byte counts.

Attributes:
    bytes_received: Total bytes received
    bytes_sent: Total bytes sent
    messages_received: Total number of messages received
    messages_sent: Total number of messages sent
    errors: Total number of errors encountered
)")
        .def(py::init<>())
        .def_readwrite(
            "bytes_received",
            &atom::async::connection::UdpSocketHub::Statistics::bytesReceived)
        .def_readwrite(
            "bytes_sent",
            &atom::async::connection::UdpSocketHub::Statistics::bytesSent)
        .def_readwrite("messages_received",
                       &atom::async::connection::UdpSocketHub::Statistics::
                           messagesReceived)
        .def_readwrite(
            "messages_sent",
            &atom::async::connection::UdpSocketHub::Statistics::messagesSent)
        .def_readwrite(
            "errors",
            &atom::async::connection::UdpSocketHub::Statistics::errors);

    // Define UdpSocketHub class
    py::class_<atom::async::connection::UdpSocketHub>(
        m, "UdpSocketHub",
        R"(A hub for managing UDP sockets and message handling.

This class provides a high-level interface for UDP communication with support for
asynchronous operations, multicast, broadcast, and more.

Args:
    num_threads: Optional number of worker threads (default: uses system-determined optimal value)

Examples:
    >>> from atom.connection.udpserver import UdpSocketHub
    >>> server = UdpSocketHub()
    >>>
    >>> # Set up message handler
    >>> def on_message(message, addr, port):
    ...     print(f"Received from {addr}:{port}: {message}")
    ...     return "Response: " + message
    >>>
    >>> server.add_message_handler(on_message)
    >>> server.start(8080)  # Start listening on port 8080
)")
        .def(py::init<>(), "Constructs a UDP socket hub with default settings.")
        .def(py::init<unsigned int>(), py::arg("num_threads"),
             "Constructs a UDP socket hub with a specific number of worker "
             "threads.")
        .def("start", &atom::async::connection::UdpSocketHub::start,
             py::arg("port"), py::arg("ipv6") = false,
             R"(Starts the UDP server on the specified port.

Args:
    port: The port to listen on
    ipv6: Whether to use IPv6 (defaults to False, using IPv4)

Returns:
    True if started successfully, False otherwise

Examples:
    >>> server.start(5000)  # Start on port 5000 with IPv4
    >>> # or
    >>> server.start(5000, True)  # Start on port 5000 with IPv6
)")
        .def("stop", &atom::async::connection::UdpSocketHub::stop,
             R"(Stops the UDP server.

This method stops the server, closes the socket, and joins any worker threads.
)")
        .def("is_running", &atom::async::connection::UdpSocketHub::isRunning,
             R"(Checks if the server is currently running.

Returns:
    True if running, False otherwise
)")
        .def("add_message_handler",
             &atom::async::connection::UdpSocketHub::addMessageHandler,
             py::arg("handler"),
             R"(Adds a message handler callback.

Args:
    handler: Function to be called when a message is received.
             Should take (message, ip, port) as parameters.

Examples:
    >>> def message_handler(message, ip, port):
    ...     print(f"Received message from {ip}:{port}: {message}")
    ...
    >>> server.add_message_handler(message_handler)
)")
        .def("remove_message_handler",
             &atom::async::connection::UdpSocketHub::removeMessageHandler,
             py::arg("handler"),
             R"(Removes a previously added message handler.

Args:
    handler: The handler function to remove
)")
        .def("add_error_handler",
             &atom::async::connection::UdpSocketHub::addErrorHandler,
             py::arg("handler"),
             R"(Adds an error handler callback.

Args:
    handler: Function to be called when an error occurs.
             Should take (error_message, error_code) as parameters.

Examples:
    >>> def error_handler(message, error_code):
    ...     print(f"Error {error_code}: {message}")
    ...
    >>> server.add_error_handler(error_handler)
)")
        .def("remove_error_handler",
             &atom::async::connection::UdpSocketHub::removeErrorHandler,
             py::arg("handler"),
             R"(Removes a previously added error handler.

Args:
    handler: The handler function to remove
)")
        .def("send_to", &atom::async::connection::UdpSocketHub::sendTo,
             py::arg("message"), py::arg("ip"), py::arg("port"),
             R"(Sends a message to a specific endpoint.

Args:
    message: The message to send
    ip: The destination IP address
    port: The destination port

Returns:
    True if the message was queued for sending, False otherwise

Examples:
    >>> server.send_to("Hello", "192.168.1.100", 8080)
)")
        .def("broadcast", &atom::async::connection::UdpSocketHub::broadcast,
             py::arg("message"), py::arg("port"),
             R"(Broadcasts a message to all devices on the network.

Args:
    message: The message to broadcast
    port: The destination port

Returns:
    True if the message was queued for broadcasting, False otherwise

Examples:
    >>> server.broadcast("Announcement", 8080)
)")
        .def("join_multicast_group",
             &atom::async::connection::UdpSocketHub::joinMulticastGroup,
             py::arg("multicast_address"),
             R"(Joins a multicast group.

Args:
    multicast_address: The multicast group address (e.g., "224.0.0.1")

Returns:
    True if joined successfully, False otherwise
)")
        .def("leave_multicast_group",
             &atom::async::connection::UdpSocketHub::leaveMulticastGroup,
             py::arg("multicast_address"),
             R"(Leaves a multicast group.

Args:
    multicast_address: The multicast group address

Returns:
    True if left successfully, False otherwise
)")
        .def("send_to_multicast",
             &atom::async::connection::UdpSocketHub::sendToMulticast,
             py::arg("message"), py::arg("multicast_address"), py::arg("port"),
             R"(Sends a message to a multicast group.

Args:
    message: The message to send
    multicast_address: The multicast group address
    port: The destination port

Returns:
    True if the message was queued for sending, False otherwise

Examples:
    >>> server.send_to_multicast("Hello group", "224.0.0.1", 8080)
)")
        .def("set_receive_buffer_size",
             &atom::async::connection::UdpSocketHub::setReceiveBufferSize,
             py::arg("size"),
             R"(Sets the receive buffer size.

Args:
    size: The buffer size in bytes

Returns:
    True if set successfully, False otherwise
)")
        .def("set_receive_timeout",
             &atom::async::connection::UdpSocketHub::setReceiveTimeout,
             py::arg("timeout"),
             R"(Sets timeout for receive operations.

Args:
    timeout: The timeout duration in milliseconds

Returns:
    True if set successfully, False otherwise

Examples:
    >>> from datetime import timedelta
    >>> server.set_receive_timeout(timedelta(seconds=5))
)")
        .def("get_statistics",
             &atom::async::connection::UdpSocketHub::getStatistics,
             R"(Gets the current statistics for this socket hub.

Returns:
    A Statistics object containing usage metrics
)")
        .def("reset_statistics",
             &atom::async::connection::UdpSocketHub::resetStatistics,
             "Resets the statistics counters to zero.")
        .def("add_allowed_ip",
             &atom::async::connection::UdpSocketHub::addAllowedIp,
             py::arg("ip"),
             R"(Adds an IP filter to allow messages only from specific IPs.

Args:
    ip: The IP address to allow

Examples:
    >>> server.add_allowed_ip("192.168.1.100")
)")
        .def("remove_allowed_ip",
             &atom::async::connection::UdpSocketHub::removeAllowedIp,
             py::arg("ip"),
             R"(Removes an IP from the allowed list.

Args:
    ip: The IP address to remove
)")
        .def("clear_ip_filters",
             &atom::async::connection::UdpSocketHub::clearIpFilters,
             "Clears all IP filters.")
        .def(
            "__enter__",
            [](atom::async::connection::UdpSocketHub& self)
                -> atom::async::connection::UdpSocketHub& { return self; },
            "Support for context manager protocol (with statement).")
        .def(
            "__exit__",
            [](atom::async::connection::UdpSocketHub& self, py::object,
               py::object, py::object) { self.stop(); },
            "Ensures server is stopped when exiting context.");

    // Create convenience factory function
    m.def(
        "create_server",
        [](unsigned short port, bool ipv6 = false) {
            auto server =
                std::make_unique<atom::async::connection::UdpSocketHub>();
            if (!server->start(port, ipv6)) {
                throw std::runtime_error("Failed to start UDP server");
            }
            return server;
        },
        py::arg("port"), py::arg("ipv6") = false,
        R"(Creates and starts a UDP server on the specified port.

This is a convenience function that creates a UdpSocketHub and starts it.

Args:
    port: The port to listen on
    ipv6: Whether to use IPv6 (defaults to False, using IPv4)

Returns:
    A running UdpSocketHub instance

Examples:
    >>> from atom.connection.udpserver import create_server
    >>> server = create_server(5000)  # Create and start a server on port 5000
)");

    // Create multicast server factory function
    m.def(
        "create_multicast_server",
        [](unsigned short port, const std::string& multicast_address) {
            auto server =
                std::make_unique<atom::async::connection::UdpSocketHub>();
            if (!server->start(port) ||
                !server->joinMulticastGroup(multicast_address)) {
                throw std::runtime_error(
                    "Failed to start multicast UDP server");
            }
            return server;
        },
        py::arg("port"), py::arg("multicast_address"),
        R"(Creates and starts a UDP server configured for multicast.

This function creates a UdpSocketHub, starts it, and joins a multicast group.

Args:
    port: The port to listen on
    multicast_address: The multicast group address to join

Returns:
    A running UdpSocketHub instance configured for multicast

Examples:
    >>> from atom.connection.udpserver import create_multicast_server
    >>> server = create_multicast_server(5000, "224.0.0.1")
)");
}
