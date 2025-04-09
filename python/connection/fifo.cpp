#include "atom/connection/async_fifoclient.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(fifo, m) {
    m.doc() = "FIFO client module for the atom package";

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

    // FifoClient class binding
    py::class_<atom::async::connection::FifoClient>(
        m, "FifoClient",
        R"(A class for asynchronous interaction with a FIFO (First In, First Out) pipe.

This class provides methods to read from and write to a FIFO pipe,
handling timeouts and ensuring proper resource management.

Args:
    fifo_path: The path to the FIFO file to be used for communication.

Examples:
    >>> from atom.connection.fifo import FifoClient
    >>> client = FifoClient("/tmp/my_fifo")
    >>> client.write("Hello, world!")
    True
    >>> response = client.read()
)")
        .def(py::init<std::string>(), py::arg("fifo_path"),
             "Constructs a FifoClient with the specified FIFO path.")
        .def("write", &atom::async::connection::FifoClient::write,
             py::arg("data"),
             py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
             R"(Writes data to the FIFO.

Args:
    data: The data to be written to the FIFO, as a string.
    timeout: Optional timeout for the write operation, in milliseconds.

Returns:
    True if the data was successfully written, False if there was an error.

Examples:
    >>> client.write("Hello", 1000)  # 1 second timeout
    True
)")
        .def("read", &atom::async::connection::FifoClient::read,
             py::arg("timeout") = std::optional<std::chrono::milliseconds>(),
             R"(Reads data from the FIFO.

Args:
    timeout: Optional timeout for the read operation, in milliseconds.

Returns:
    A string containing the data read from the FIFO, or None if the read failed.

Examples:
    >>> data = client.read(500)  # 500ms timeout
    >>> if data:
    ...     print(f"Received: {data}")
)")
        .def("is_open", &atom::async::connection::FifoClient::isOpen,
             R"(Checks if the FIFO is currently open.

Returns:
    True if the FIFO is open, False otherwise.
)")
        .def("close", &atom::async::connection::FifoClient::close,
             R"(Closes the FIFO.

This will release any resources associated with the FIFO.
)")
        .def(
            "__enter__",
            [](atom::async::connection::FifoClient& self)
                -> atom::async::connection::FifoClient& { return self; },
            "Support for context manager protocol (with statement).")
        .def(
            "__exit__",
            [](atom::async::connection::FifoClient& self, py::object,
               py::object, py::object) { self.close(); },
            "Ensures FIFO is closed when exiting context manager.");

    // Factory function for easier creation
    m.def(
        "create_fifo_client",
        [](const std::string& path) {
            return std::make_unique<atom::async::connection::FifoClient>(path);
        },
        py::arg("path"),
        R"(Factory function to create a FIFO client.

Args:
    path: The path to the FIFO file.

Returns:
    A newly created FifoClient object.

Examples:
    >>> from atom.connection.fifo import create_fifo_client
    >>> client = create_fifo_client("/tmp/my_fifo")
)");
}