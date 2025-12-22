#include "atom/connection/sockethub.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(sync_sockethub, m) {
    m.doc() = R"(Synchronous socket hub module for the atom package.

This module provides synchronous socket server functionality with modern C++20 features,
including concepts, RAII resource management, and comprehensive client management.

Key Features:
- High-performance socket server with client management
- Message broadcasting and targeted sending
- Client event tracking (connect/disconnect)
- Client information and statistics
- Timeout management
- Modern C++20 concepts for type safety

Classes:
- SocketHub: Main synchronous socket server class
- ClientInfo: Information about connected clients

Quick Start Example:
    >>> from atom.connection.sync_sockethub import SocketHub
    >>>
    >>> # Create and start the hub
    >>> hub = SocketHub()
    >>>
    >>> # Set up handlers
    >>> def on_message(msg):
    ...     print(f"Received: {msg}")
    ...     hub.broadcast(f"Echo: {msg}")
    >>>
    >>> def on_connect(client_id, addr):
    ...     print(f"Client {client_id} connected from {addr}")
    >>>
    >>> hub.add_handler(on_message)
    >>> hub.add_connect_handler(on_connect)
    >>>
    >>> # Start the server
    >>> hub.start(8080)
    >>> # Server is now running...
    >>> hub.stop()
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

    // ClientInfo struct
    py::class_<atom::connection::ClientInfo>(
        m, "ClientInfo",
        R"(Information about a connected client.

This structure contains details about a client's connection including
ID, address, connection time, and data transfer statistics.

Examples:
    >>> clients = hub.get_connected_clients()
    >>> for client in clients:
    ...     print(f"Client {client.id} from {client.address}")
    ...     print(f"Bytes sent: {client.bytes_sent}")
)")
        .def(py::init<>(), "Default constructor")
        .def_readonly("id", &atom::connection::ClientInfo::id,
                      "Unique client identifier")
        .def_readonly("address", &atom::connection::ClientInfo::address,
                      "Client IP address")
        .def_readonly("connected_time",
                      &atom::connection::ClientInfo::connectedTime,
                      "Time when client connected")
        .def_readonly("bytes_received",
                      &atom::connection::ClientInfo::bytesReceived,
                      "Total bytes received from this client")
        .def_readonly("bytes_sent", &atom::connection::ClientInfo::bytesSent,
                      "Total bytes sent to this client");

    // SocketHub class
    py::class_<atom::connection::SocketHub>(
        m, "SocketHub",
        R"(High-performance synchronous socket connection manager.

This class implements a socket server that can handle multiple clients,
process messages with registered handlers, and manage client connections
with comprehensive event tracking.

Examples:
    >>> from atom.connection.sync_sockethub import SocketHub
    >>>
    >>> # Create hub and set up handlers
    >>> hub = SocketHub()
    >>>
    >>> def message_handler(msg):
    ...     print(f"Message: {msg}")
    >>>
    >>> def connect_handler(client_id, addr):
    ...     print(f"Client {client_id} connected")
    >>>
    >>> hub.add_handler(message_handler)
    >>> hub.add_connect_handler(connect_handler)
    >>>
    >>> # Start and manage server
    >>> hub.start(8080)
    >>> print(f"Server running with {hub.get_client_count()} clients")
    >>> hub.stop()
)")
        .def(py::init<>(), "Constructs a SocketHub instance.")
        .def("start", &atom::connection::SocketHub::start, py::arg("port"),
             R"(Starts the socket service on the specified port.

Args:
    port: The port number to listen on (1-65535)

Raises:
    ValueError: If port is invalid
    RuntimeError: If socket creation fails
)")
        .def("stop", &atom::connection::SocketHub::stop,
             R"(Stops the socket service and disconnects all clients.)")
        .def(
            "add_handler",
            [](atom::connection::SocketHub& self, py::function handler) {
                self.addHandlerImpl(
                    [handler = std::move(handler)](std::string_view msg) {
                        py::gil_scoped_acquire gil;
                        try {
                            handler(py::str(msg.data(), msg.size()));
                        } catch (const py::error_already_set& e) {
                            PyErr_Print();
                        }
                    });
            },
            py::arg("handler"),
            R"(Adds a message handler function.

Args:
    handler: Function that takes a string message parameter

Examples:
    >>> def on_message(msg):
    ...     print(f"Received: {msg}")
    >>> hub.add_handler(on_message)
)")
        .def(
            "add_connect_handler",
            [](atom::connection::SocketHub& self, py::function handler) {
                self.addConnectHandlerImpl([handler = std::move(handler)](
                                               int clientId,
                                               std::string_view clientAddr) {
                    py::gil_scoped_acquire gil;
                    try {
                        handler(clientId,
                                py::str(clientAddr.data(), clientAddr.size()));
                    } catch (const py::error_already_set& e) {
                        PyErr_Print();
                    }
                });
            },
            py::arg("handler"),
            R"(Adds a client connect event handler.

Args:
    handler: Function that takes (client_id, client_address) parameters

Examples:
    >>> def on_connect(client_id, addr):
    ...     print(f"Client {client_id} connected from {addr}")
    >>> hub.add_connect_handler(on_connect)
)")
        .def(
            "add_disconnect_handler",
            [](atom::connection::SocketHub& self, py::function handler) {
                self.addDisconnectHandlerImpl([handler = std::move(handler)](
                                                  int clientId,
                                                  std::string_view clientAddr) {
                    py::gil_scoped_acquire gil;
                    try {
                        handler(clientId,
                                py::str(clientAddr.data(), clientAddr.size()));
                    } catch (const py::error_already_set& e) {
                        PyErr_Print();
                    }
                });
            },
            py::arg("handler"),
            R"(Adds a client disconnect event handler.

Args:
    handler: Function that takes (client_id, client_address) parameters

Examples:
    >>> def on_disconnect(client_id, addr):
    ...     print(f"Client {client_id} disconnected")
    >>> hub.add_disconnect_handler(on_disconnect)
)")
        .def("broadcast", &atom::connection::SocketHub::broadcast,
             py::arg("message"),
             R"(Broadcasts a message to all connected clients.

Args:
    message: The message string to broadcast

Returns:
    Number of clients the message was sent to

Examples:
    >>> count = hub.broadcast("Hello, everyone!")
    >>> print(f"Message sent to {count} clients")
)")
        .def("send_to", &atom::connection::SocketHub::sendTo,
             py::arg("client_id"), py::arg("message"),
             R"(Sends a message to a specific client.

Args:
    client_id: The ID of the client to send to
    message: The message string to send

Returns:
    True if message was sent successfully, False otherwise

Examples:
    >>> if hub.send_to(42, "Hello, client 42!"):
    ...     print("Message sent")
)")
        .def("get_connected_clients",
             &atom::connection::SocketHub::getConnectedClients,
             R"(Gets information about all connected clients.

Returns:
    List of ClientInfo objects for each connected client

Examples:
    >>> clients = hub.get_connected_clients()
    >>> for client in clients:
    ...     print(f"Client {client.id}: {client.address}")
)")
        .def("get_client_count", &atom::connection::SocketHub::getClientCount,
             R"(Gets the number of connected clients.

Returns:
    The current number of connected clients

Examples:
    >>> count = hub.get_client_count()
    >>> print(f"Currently {count} clients connected")
)")
        .def("is_running", &atom::connection::SocketHub::isRunning,
             R"(Checks if the socket service is running.

Returns:
    True if running, False otherwise

Examples:
    >>> if hub.is_running():
    ...     print("Server is active")
)")
        .def("set_client_timeout",
             &atom::connection::SocketHub::setClientTimeout, py::arg("timeout"),
             R"(Sets the client timeout duration.

Args:
    timeout: Timeout duration in seconds

Examples:
    >>> hub.set_client_timeout(30)  # 30 second timeout
)")
        .def("get_port", &atom::connection::SocketHub::getPort,
             R"(Gets the port the server is running on.

Returns:
    The server port, or 0 if not running

Examples:
    >>> port = hub.get_port()
    >>> if port > 0:
    ...     print(f"Server running on port {port}")
)")
        .def(
            "__enter__",
            [](atom::connection::SocketHub& self)
                -> atom::connection::SocketHub& { return self; },
            "Support for context manager protocol")
        .def(
            "__exit__",
            [](atom::connection::SocketHub& self, py::object, py::object,
               py::object) {
                if (self.isRunning()) {
                    self.stop();
                }
            },
            "Ensure server is stopped when exiting context");

    // Factory function
    m.def(
        "create_socket_hub",
        [](int port) {
            auto hub = std::make_unique<atom::connection::SocketHub>();
            hub->start(port);
            return hub;
        },
        py::arg("port"),
        R"(Creates and starts a socket hub on the specified port.

Args:
    port: The port to listen on

Returns:
    A running SocketHub instance

Raises:
    RuntimeError: If the hub fails to start

Examples:
    >>> hub = create_socket_hub(8080)
    >>> # Use the hub...
    >>> hub.stop()
)");
}
