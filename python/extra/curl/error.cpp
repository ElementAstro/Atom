#include "atom/extra/curl/error.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(error, m) {
    m.doc() = R"(CURL error handling module for the atom package.

This module provides custom exception classes for handling curl errors,
with additional information about the curl error code and message.

Examples:
    >>> from atom.extra.curl import error, session
    >>>
    >>> try:
    ...     s = session.Session()
    ...     response = s.get("https://invalid-url-that-does-not-exist.com")
    ... except RuntimeError as e:
    ...     print(f"Request failed: {e}")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::extra::curl::Error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Error class binding
    py::class_<atom::extra::curl::Error, std::runtime_error>(
        m, "Error",
        R"(Custom exception class for curl errors.

This class inherits from RuntimeError and provides additional
information about the curl error, such as the curl error code and message.

Examples:
    >>> try:
    ...     # Some curl operation that might fail
    ...     pass
    ... except error.Error as e:
    ...     print(f"CURL error code: {e.code()}")
    ...     print(f"Error message: {str(e)}")
)")
        .def("code", &atom::extra::curl::Error::code,
             R"(Get the curl error code.

Returns:
    The curl error code (CURLcode).
)")
        .def("multi_code", &atom::extra::curl::Error::multi_code,
             R"(Get the curl multi error code, if available.

Returns:
    Optional curl multi error code (CURLMcode), or None if not available.
)");
}
