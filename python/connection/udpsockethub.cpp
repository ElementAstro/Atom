#include "atom/connection/udpserver.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * @brief Binds the UdpError enum to Python.
 *
 * This function creates Python bindings for the UdpError enum which
 * represents different error conditions for UDP operations.
 *
 * @param m The pybind11 module to bind to
 */
void bindUdpError(py::module_& m) {
    py::enum_<atom::connection::UdpError>(
        m, "UdpError",
        R"(Error codes for UDP operations.

This enumeration defines different error conditions that can occur
during UDP socket operations.

Examples:
    >>> from atom.connection.udpsockethub import UdpError
    >>> # Error handling in UDP operations
)")
        .value("SocketCreationFailed", atom::connection::UdpError::SocketCreationFailed,
               "Failed to create UDP socket")
        .value("BindFailed", atom::connection::UdpError::BindFailed,
               "Failed to bind socket to port")
        .value("NetworkInitFailed", atom::connection::UdpError::NetworkInitFailed,
               "Failed to initialize network")
        .value("SendFailed", atom::connection::UdpError::SendFailed,
               "Failed to send message")
        .value("NotRunning", atom::connection::UdpError::NotRunning,
               "UDP socket hub is not running")
        .value("InvalidAddress", atom::connection::UdpError::InvalidAddress,
               "Invalid IP address")
        .value("InvalidPort", atom::connection::UdpError::InvalidPort,
               "Invalid port number")
        .export_values();
}

PYBIND11_MODULE(udpsockethub, m) {
    m.doc() = R"(UDP Socket Hub module for the atom package.

This module provides UDP socket hub functionality for managing UDP sockets
and handling message communication with multiple endpoints.

Key Features:
- UDP socket management and binding
- Message handler registration and management
- Bidirectional UDP communication
- Error handling with detailed error codes
- Buffer size configuration
- Connection state management

Classes:
- UdpSocketHub: Main UDP socket hub class
- UdpError: Enumeration of UDP error codes

Quick Start Example:
    >>> from atom.connection.udpsockethub import UdpSocketHub, UdpError
    >>> 
    >>> # Create UDP socket hub
    >>> hub = UdpSocketHub()
    >>> 
    >>> # Set up message handler
    >>> def on_message(message, ip, port):
    ...     print(f"Received from {ip}:{port}: {message}")
    >>> 
    >>> hub.add_message_handler(on_message)
    >>> 
    >>> # Start listening on port
    >>> result = hub.start(8080)
    >>> if result.has_value():
    ...     print("UDP hub started on port 8080")
    ...     
    ...     # Send message to another endpoint
    ...     send_result = hub.send_to("Hello, UDP!", "192.168.1.100", 8081)
    ...     if send_result.has_value():
    ...         print("Message sent successfully")
    >>> 
    >>> hub.stop()

Advanced Features:
- Multiple message handler support
- Dynamic handler registration/removal
- Configurable buffer sizes
- Error handling with expected<T, E> pattern
- Thread-safe operations
- Resource management with RAII
)";

    // Bind enums and data structures
    bindUdpError(m);

    // Bind the main UdpSocketHub class
    py::class_<atom::connection::UdpSocketHub>(
        m, "UdpSocketHub",
        R"(UDP socket hub for managing UDP communication.

This class provides a high-level interface for UDP socket operations,
including listening for incoming messages and sending messages to specific
addresses and ports.

Examples:
    >>> from atom.connection.udpsockethub import UdpSocketHub
    >>> 
    >>> # Create hub and set up handler
    >>> hub = UdpSocketHub()
    >>> 
    >>> def message_handler(msg, ip, port):
    ...     print(f"Got message '{msg}' from {ip}:{port}")
    >>> 
    >>> hub.add_message_handler(message_handler)
    >>> 
    >>> # Start listening and send messages
    >>> if hub.start(8080).has_value():
    ...     hub.send_to("Hello!", "127.0.0.1", 8081)
    ...     hub.stop()
)")
        .def(py::init<>(),
             R"(Constructs a UdpSocketHub.

Examples:
    >>> hub = UdpSocketHub()
)")
        .def("start", 
             [](atom::connection::UdpSocketHub& self, std::uint16_t port) {
                 auto result = self.start(port);
                 if (!result.has_value()) {
                     throw std::runtime_error("Failed to start UDP hub");
                 }
             },
             py::arg("port"),
             R"(Starts the UDP socket hub and binds it to the specified port.

Args:
    port: The port on which the UDP socket hub will listen

Raises:
    RuntimeError: If starting the hub fails

Examples:
    >>> hub.start(8080)
)")
        .def("stop", &atom::connection::UdpSocketHub::stop,
             R"(Stops the UDP socket hub.

Examples:
    >>> hub.stop()
)")
        .def("is_running", &atom::connection::UdpSocketHub::isRunning,
             R"(Checks if the UDP socket hub is currently running.

Returns:
    True if the UDP socket hub is running, False otherwise

Examples:
    >>> if hub.is_running():
    ...     print("Hub is active")
)")
        .def("add_message_handler",
             [](atom::connection::UdpSocketHub& self, py::object handler) {
                 self.addMessageHandler([handler](const std::string& message, 
                                                  const std::string& ip, 
                                                  int port) mutable {
                     try {
                         py::gil_scoped_acquire acquire;
                         handler.operator()(message, ip, port);
                     } catch (const py::error_already_set&) {
                         // Handle Python exceptions
                     }
                 });
             },
             py::arg("handler"),
             R"(Adds a message handler function to the UDP socket hub.

Args:
    handler: Function to call when a message is received
             Signature: handler(message: str, ip: str, port: int) -> None

Examples:
    >>> def my_handler(msg, ip, port):
    ...     print(f"Received: {msg} from {ip}:{port}")
    >>> hub.add_message_handler(my_handler)
)")
        .def("send_to",
             [](atom::connection::UdpSocketHub& self, std::string_view message,
                std::string_view ip, std::uint16_t port) {
                 auto result = self.sendTo(message, ip, port);
                 if (!result.has_value()) {
                     throw std::runtime_error("Failed to send message");
                 }
             },
             py::arg("message"), py::arg("ip"), py::arg("port"),
             R"(Sends a message to the specified IP address and port.

Args:
    message: The message to send
    ip: The IP address of the recipient
    port: The port of the recipient

Raises:
    RuntimeError: If sending the message fails

Examples:
    >>> hub.send_to("Hello, World!", "192.168.1.100", 8081)
)")
        .def("set_buffer_size", &atom::connection::UdpSocketHub::setBufferSize,
             py::arg("size"),
             R"(Sets the maximum buffer size for receiving messages.

Args:
    size: The new buffer size in bytes

Examples:
    >>> hub.set_buffer_size(8192)  # 8KB buffer
)")
        .def(
            "__enter__",
            [](atom::connection::UdpSocketHub& self) -> atom::connection::UdpSocketHub& {
                return self;
            },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::UdpSocketHub& self, py::object, py::object, py::object) {
                if (self.isRunning()) {
                    self.stop();
                }
            },
            "Ensure hub is stopped when exiting context");
}
