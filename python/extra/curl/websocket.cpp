#include "atom/extra/curl/websocket.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(websocket, m) {
    m.doc() = R"(WebSocket client module for the atom package.

This module provides a class for creating and managing WebSocket connections
using libcurl, with support for sending and receiving messages and handling
connection events.

Examples:
    >>> from atom.extra.curl import websocket
    >>>
    >>> # Create a WebSocket connection
    >>> ws = websocket.WebSocket()
    >>>
    >>> # Set up callbacks
    >>> def on_message(message, is_binary):
    ...     print(f"Received: {message}")
    >>>
    >>> def on_connect(success):
    ...     if success:
    ...         print("Connected!")
    ...     else:
    ...         print("Connection failed!")
    >>>
    >>> def on_close(code, reason):
    ...     print(f"Closed: {code} - {reason}")
    >>>
    >>> ws.on_message(on_message)
    >>> ws.on_connect(on_connect)
    >>> ws.on_close(on_close)
    >>>
    >>> # Connect to WebSocket server
    >>> if ws.connect("wss://echo.websocket.org"):
    ...     ws.send("Hello, WebSocket!")
    ...     # ... do other work ...
    ...     ws.close()
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

    // WebSocket class binding
    py::class_<atom::extra::curl::WebSocket,
               std::shared_ptr<atom::extra::curl::WebSocket>>(
        m, "WebSocket",
        R"(A class for creating and managing WebSocket connections.

This class provides a simple interface for establishing WebSocket
connections, sending and receiving messages, and handling connection events.

Examples:
    >>> ws = WebSocket()
    >>>
    >>> # Set message handler
    >>> ws.on_message(lambda msg, binary: print(f"Got: {msg}"))
    >>>
    >>> # Connect and send
    >>> if ws.connect("wss://example.com/ws"):
    ...     ws.send("Hello!")
    ...     # Keep connection alive...
    ...     ws.close()
)")
        .def(py::init<>(), "Create a new WebSocket connection")
        .def("connect", &atom::extra::curl::WebSocket::connect, py::arg("url"),
             py::arg("headers") = std::map<std::string, std::string>{},
             py::call_guard<py::gil_scoped_release>(),
             R"(Establish a WebSocket connection to the specified URL.

Args:
    url: The URL of the WebSocket server (ws:// or wss://).
    headers: Optional dictionary of HTTP headers to send with the connection request.

Returns:
    True if the connection was successfully established, False otherwise.
)")
        .def("close", &atom::extra::curl::WebSocket::close,
             py::arg("code") = 1000, py::arg("reason") = "Normal closure",
             R"(Close the WebSocket connection.

Args:
    code: The WebSocket close code (default: 1000 - Normal closure).
    reason: A human-readable reason for the closure (default: "Normal closure").
)")
        .def("send", &atom::extra::curl::WebSocket::send, py::arg("message"),
             py::arg("binary") = false,
             R"(Send a message to the WebSocket server.

Args:
    message: The content of the message to send.
    binary: True if the message is binary, False if it's text (default: False).

Returns:
    True if the message was successfully sent, False otherwise.
)")
        .def("on_message", &atom::extra::curl::WebSocket::on_message,
             py::arg("callback"),
             R"(Set the message callback function.

Args:
    callback: A function to be called when a new message is received.
              The function should accept two parameters:
              - message (str): The content of the received message
              - binary (bool): True if the message is binary, False if text

Examples:
    >>> def handle_message(message, is_binary):
    ...     if is_binary:
    ...         print(f"Binary message: {len(message)} bytes")
    ...     else:
    ...         print(f"Text message: {message}")
    >>>
    >>> ws.on_message(handle_message)
)")
        .def("on_connect", &atom::extra::curl::WebSocket::on_connect,
             py::arg("callback"),
             R"(Set the connect callback function.

Args:
    callback: A function to be called when the WebSocket connection is
              established or when the connection attempt fails.
              The function should accept one parameter:
              - success (bool): True if connection was successful, False otherwise

Examples:
    >>> def handle_connect(success):
    ...     if success:
    ...         print("Connected successfully!")
    ...     else:
    ...         print("Connection failed!")
    >>>
    >>> ws.on_connect(handle_connect)
)")
        .def("on_close", &atom::extra::curl::WebSocket::on_close,
             py::arg("callback"),
             R"(Set the close callback function.

Args:
    callback: A function to be called when the WebSocket connection is closed.
              The function should accept two parameters:
              - code (int): The WebSocket close code
              - reason (str): A human-readable reason for the closure

Examples:
    >>> def handle_close(code, reason):
    ...     print(f"Connection closed: {code} - {reason}")
    >>>
    >>> ws.on_close(handle_close)
)")
        .def("is_connected", &atom::extra::curl::WebSocket::is_connected,
             R"(Check if the WebSocket connection is currently established.

Returns:
    True if the connection is established, False otherwise.
)");
}
