#include "atom/extra/curl/connection_pool.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(connection_pool, m) {
    m.doc() = R"(Connection pooling module for HTTP requests.

This module provides a connection pool for managing and reusing CURL handles,
which can improve performance when making multiple HTTP requests.

Examples:
    >>> from atom.extra.curl import connection_pool, session
    >>>
    >>> # Create a connection pool with max 20 connections
    >>> pool = connection_pool.ConnectionPool(20)
    >>>
    >>> # Create a session with the pool
    >>> # (Note: Session integration would be done in C++)
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

    // ConnectionPool class binding
    py::class_<atom::extra::curl::ConnectionPool>(
        m, "ConnectionPool",
        R"(Connection pool for managing and reusing CURL handles.

This class provides a pool of CURL handles that can be reused across
multiple HTTP requests, improving performance by avoiding the overhead
of creating and destroying handles for each request.

Examples:
    >>> # Create a connection pool
    >>> pool = ConnectionPool(10)  # Max 10 connections
    >>>
    >>> # Use with Session (integration done in C++)
    >>> # session = Session(pool)
)")
        .def(py::init<size_t>(), py::arg("max_connections") = 10,
             R"(Constructor for the ConnectionPool class.

Args:
    max_connections: The maximum number of connections to maintain in the pool
                     (default: 10).
)");
}
