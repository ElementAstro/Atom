#include "atom/extra/beast/ws.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using json = nlohmann::json;

PYBIND11_MODULE(ws, m) {
    m.doc() = "WebSocket client module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const boost::system::system_error& e) {
            PyErr_SetString(PyExc_ConnectionError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // WSClient class binding
    py::class_<WSClient, std::shared_ptr<WSClient>>(
        m, "WSClient",
        R"(A WebSocket client for managing WebSocket connections and communication.

This class provides methods to connect to WebSocket servers, send and receive
messages, and manage connection settings like timeouts and reconnection.

Args:
    io_context: The I/O context to use for asynchronous operations.

Examples:
    >>> from atom.ws import WSClient
    >>> import asyncio
    >>> 
    >>> # Create a WebSocket client
    >>> client = WSClient()
    >>> 
    >>> # Connect to a WebSocket server
    >>> client.connect("echo.websocket.org", "80")
    >>> 
    >>> # Send a message
    >>> client.send("Hello, WebSocket!")
    >>> 
    >>> # Receive a message
    >>> response = client.receive()
    >>> print(response)
)")
        .def(py::init<net::io_context&>(), py::arg("io_context") = py::none(),
             R"(Constructs a WSClient with the given I/O context.

Args:
    io_context: The I/O context to use for asynchronous operations.
               If None, a default one will be used.
)")
        .def("set_timeout", &WSClient::setTimeout, py::arg("timeout"),
             R"(Sets the timeout duration for the WebSocket operations.

Args:
    timeout: The timeout duration in seconds.
)")
        .def("set_reconnect_options", &WSClient::setReconnectOptions,
             py::arg("retries"), py::arg("interval"),
             R"(Sets the reconnection options.

Args:
    retries: The number of reconnection attempts.
    interval: The interval between reconnection attempts in seconds.

Raises:
    ValueError: If retries is negative or interval is zero.
)")
        .def("set_ping_interval", &WSClient::setPingInterval,
             py::arg("interval"),
             R"(Sets the interval for sending ping messages.

Args:
    interval: The ping interval in seconds.

Raises:
    ValueError: If interval is zero or negative.
)")
        .def("connect", &WSClient::connect, py::arg("host"), py::arg("port"),
             R"(Connects to the WebSocket server.

Args:
    host: The server host.
    port: The server port.

Raises:
    ConnectionError: On connection failure.
    ValueError: If host or port is invalid.
)")
        .def("send", &WSClient::send, py::arg("message"),
             R"(Sends a message to the WebSocket server.

Args:
    message: The message to send.

Raises:
    ConnectionError: On sending failure.
    RuntimeError: If not connected.
)")
        .def("receive", &WSClient::receive,
             R"(Receives a message from the WebSocket server.

Returns:
    The received message.

Raises:
    ConnectionError: On receiving failure.
    RuntimeError: If not connected.
)")
        .def("is_connected", &WSClient::isConnected,
             R"(Checks if the connection is established.

Returns:
    True if connected, False otherwise.
)")
        .def("close", &WSClient::close,
             R"(Closes the WebSocket connection.

Raises:
    ConnectionError: On closing failure.
)")
        // Asynchronous methods with Python callbacks
        .def(
            "async_connect",
            [](WSClient& self, std::string_view host, std::string_view port,
               py::function handler) {
                self.asyncConnect(
                    host, port,
                    [handler = std::move(handler)](beast::error_code ec) {
                        py::gil_scoped_acquire acquire;
                        handler(ec.value(), ec.message());
                    });
            },
            py::arg("host"), py::arg("port"), py::arg("handler"),
            R"(Asynchronously connects to the WebSocket server.

Args:
    host: The server host.
    port: The server port.
    handler: The callback function to call when the operation completes.
             Should accept two parameters: error_code (int) and error_message (str).

Raises:
    ValueError: If host or port is invalid.
)")
        .def(
            "async_send",
            [](WSClient& self, std::string_view message, py::function handler) {
                self.asyncSend(message, [handler = std::move(handler)](
                                            beast::error_code ec,
                                            std::size_t bytes_transferred) {
                    py::gil_scoped_acquire acquire;
                    handler(ec.value(), ec.message(), bytes_transferred);
                });
            },
            py::arg("message"), py::arg("handler"),
            R"(Asynchronously sends a message to the WebSocket server.

Args:
    message: The message to send.
    handler: The callback function to call when the operation completes.
             Should accept three parameters: error_code (int), error_message (str), and bytes_transferred (int).

Raises:
    RuntimeError: If not connected.
)")
        .def(
            "async_receive",
            [](WSClient& self, py::function handler) {
                self.asyncReceive(
                    [handler = std::move(handler)](beast::error_code ec,
                                                   std::string message) {
                        py::gil_scoped_acquire acquire;
                        handler(ec.value(), ec.message(), message);
                    });
            },
            py::arg("handler"),
            R"(Asynchronously receives a message from the WebSocket server.

Args:
    handler: The callback function to call when the operation completes.
             Should accept three parameters: error_code (int), error_message (str), and message (str).

Raises:
    RuntimeError: If not connected.
)")
        .def(
            "async_close",
            [](WSClient& self, py::function handler) {
                self.asyncClose(
                    [handler = std::move(handler)](beast::error_code ec) {
                        py::gil_scoped_acquire acquire;
                        handler(ec.value(), ec.message());
                    });
            },
            py::arg("handler"),
            R"(Asynchronously closes the WebSocket connection.

Args:
    handler: The callback function to call when the operation completes.
             Should accept two parameters: error_code (int) and error_message (str).
)")
        .def(
            "async_send_json",
            [](WSClient& self, const py::object& json_data,
               py::function handler) {
                // Convert Python dict to nlohmann::json
                py::str json_str = py::str(
                    py::module::import("json").attr("dumps")(json_data));
                json cpp_json = json::parse(static_cast<std::string>(json_str));

                self.asyncSendJson(
                    cpp_json,
                    [handler = std::move(handler)](
                        beast::error_code ec, std::size_t bytes_transferred) {
                        py::gil_scoped_acquire acquire;
                        handler(ec.value(), ec.message(), bytes_transferred);
                    });
            },
            py::arg("json_data"), py::arg("handler"),
            R"(Asynchronously sends a JSON object to the WebSocket server.

Args:
    json_data: The JSON object to send.
    handler: The callback function to call when the operation completes.
             Should accept three parameters: error_code (int), error_message (str), and bytes_transferred (int).

Raises:
    RuntimeError: If not connected.
)")
        .def(
            "async_receive_json",
            [](WSClient& self, py::function handler) {
                self.asyncReceiveJson([handler = std::move(handler)](
                                          beast::error_code ec, json jdata) {
                    py::gil_scoped_acquire acquire;
                    // Convert nlohmann::json to Python dict
                    py::object py_json = py::none();
                    if (!ec) {
                        py::module json_module = py::module::import("json");
                        py_json = json_module.attr("loads")(jdata.dump());
                    }
                    handler(ec.value(), ec.message(), py_json);
                });
            },
            py::arg("handler"),
            R"(Asynchronously receives a JSON object from the WebSocket server.

Args:
    handler: The callback function to call when the operation completes.
             Should accept three parameters: error_code (int), error_message (str), and json_data (dict).

Raises:
    RuntimeError: If not connected.
)");
}