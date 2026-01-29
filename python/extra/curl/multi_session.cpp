/*
 * multi_session.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Python bindings for multi-session HTTP client from
 * atom/extra/curl/multi_session.hpp
 */

#include "atom/extra/curl/multi_session.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(multi_session, m) {
    m.doc() = R"pbdoc(
        Multi-Session HTTP Client Module
        --------------------------------

        This module provides a multi-session HTTP client that can perform
        multiple concurrent HTTP requests efficiently using libcurl's multi interface.

        Features:
        - Concurrent HTTP requests
        - Automatic connection reuse
        - Progress callbacks
        - Batch request processing
        - Asynchronous execution

        Examples:
            >>> from atom.extra.curl import multi_session
            >>>
            >>> # Create a multi-session client
            >>> client = multi_session.MultiSession()
            >>>
            >>> # Add multiple requests
            >>> client.add_request("https://api.example.com/users/1")
            >>> client.add_request("https://api.example.com/users/2")
            >>> client.add_request("https://api.example.com/users/3")
            >>>
            >>> # Execute all requests concurrently
            >>> responses = client.execute_all()
            >>> for response in responses:
            ...     print(response)
    )pbdoc";

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

    // MultiSession class binding
    py::class_<atom::extra::curl::MultiSession>(m, "MultiSession",
                                                R"pbdoc(
        Multi-session HTTP client for concurrent requests.

        This class allows you to queue multiple HTTP requests and execute them
        concurrently, improving performance when making many requests.

        Examples:
            >>> client = MultiSession()
            >>> client.add_get("https://api.example.com/data1")
            >>> client.add_get("https://api.example.com/data2")
            >>> results = client.execute_all()
    )pbdoc")
        .def(py::init<>(), "Create a new multi-session client")
        .def("add_get", &atom::extra::curl::MultiSession::addGet,
             py::arg("url"),
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Add a GET request to the queue.

                 Args:
                     url: The URL to request
                     headers: Optional headers to include

                 Returns:
                     Request ID for tracking
             )pbdoc")
        .def("add_post", &atom::extra::curl::MultiSession::addPost,
             py::arg("url"), py::arg("body") = "",
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Add a POST request to the queue.

                 Args:
                     url: The URL to request
                     body: The request body
                     headers: Optional headers to include

                 Returns:
                     Request ID for tracking
             )pbdoc")
        .def("execute_all", &atom::extra::curl::MultiSession::executeAll,
             R"pbdoc(
                 Execute all queued requests concurrently.

                 Returns:
                     List of responses in the order requests were added
             )pbdoc")
        .def("clear", &atom::extra::curl::MultiSession::clear,
             "Clear all queued requests")
        .def("set_max_concurrent",
             &atom::extra::curl::MultiSession::setMaxConcurrent,
             py::arg("max_concurrent"),
             R"pbdoc(
                 Set the maximum number of concurrent requests.

                 Args:
                     max_concurrent: Maximum concurrent request count
             )pbdoc")
        .def("set_timeout", &atom::extra::curl::MultiSession::setTimeout,
             py::arg("timeout_ms"),
             R"pbdoc(
                 Set the timeout for all requests.

                 Args:
                     timeout_ms: Timeout in milliseconds
             )pbdoc")
        .def("pending_count", &atom::extra::curl::MultiSession::pendingCount,
             "Get the number of pending requests");
}
