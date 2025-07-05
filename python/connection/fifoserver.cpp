#include "atom/connection/async_fifoserver.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(fifoserver, m) {
    m.doc() = "FIFO server module for the atom package";

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

    // FifoServer class binding
    py::class_<atom::async::connection::FifoServer>(
        m, "FifoServer",
        R"(A server for handling FIFO (named pipe) messages.

This class implements a server that listens for messages on a FIFO pipe
and processes them asynchronously.

Args:
    fifo_path: The path to the FIFO pipe to create and listen on.

Examples:
    >>> from atom.connection.fifoserver import FifoServer
    >>> server = FifoServer("/tmp/my_fifo")
    >>> server.start()
    >>> # Process messages...
    >>> server.stop()
)")
        .def(py::init<std::string_view>(), py::arg("fifo_path"),
             "Constructs a FifoServer that will listen on the specified FIFO "
             "path.")
        .def("start", &atom::async::connection::FifoServer::start,
             R"(Starts the server to listen for messages.

This method creates the FIFO if it doesn't exist and begins listening
for incoming messages in a background thread.

Raises:
    RuntimeError: If the server fails to start or the FIFO cannot be created.
)")
        .def("stop", &atom::async::connection::FifoServer::stop,
             R"(Stops the server.

This method stops the server, closes the FIFO, and joins any background threads.
)")
        .def("is_running", &atom::async::connection::FifoServer::isRunning,
             R"(Checks if the server is currently running.

Returns:
    True if the server is running and listening for messages, False otherwise.
)")
        .def(
            "__enter__",
            [](atom::async::connection::FifoServer& self)
                -> atom::async::connection::FifoServer& {
                self.start();
                return self;
            },
            "Support for context manager protocol (with statement).")
        .def(
            "__exit__",
            [](atom::async::connection::FifoServer& self, py::object,
               py::object, py::object) { self.stop(); },
            "Ensures server is stopped when exiting context manager.");

    // Factory function for easier creation
    m.def(
        "create_fifo_server",
        [](const std::string& path) {
            return std::make_unique<atom::async::connection::FifoServer>(path);
        },
        py::arg("path"),
        R"(Factory function to create a FIFO server.

Args:
    path: The path to the FIFO file.

Returns:
    A newly created FifoServer object.

Examples:
    >>> from atom.connection.fifoserver import create_fifo_server
    >>> server = create_fifo_server("/tmp/my_fifo")
    >>> server.start()
)");
}
