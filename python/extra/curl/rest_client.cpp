/*
 * rest_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Python bindings for REST client from atom/extra/curl/rest_client.hpp
 */

#include "atom/extra/curl/rest_client.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(rest_client, m) {
    m.doc() = R"pbdoc(
        REST Client Module
        -----------------

        This module provides a high-level REST client for making HTTP requests
        with automatic JSON serialization/deserialization, retry logic, and
        error handling.

        Features:
        - Automatic JSON serialization/deserialization
        - Configurable retry logic with exponential backoff
        - Request/response interceptors
        - Connection pooling
        - Rate limiting support
        - Timeout configuration

        Examples:
            >>> from atom.extra.curl import rest_client
            >>>
            >>> # Create a REST client
            >>> client = rest_client.RestClient("https://api.example.com")
            >>>
            >>> # Perform GET request
            >>> response = client.get("/users")
            >>> print(response)
            >>>
            >>> # Perform POST request with JSON body
            >>> data = {"name": "John", "email": "john@example.com"}
            >>> response = client.post("/users", data)
            >>>
            >>> # Configure retry logic
            >>> client.set_max_retries(3)
            >>> client.set_retry_delay(1000)  # 1 second
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

    // RestClient class binding
    py::class_<atom::extra::curl::RestClient>(m, "RestClient",
                                              R"pbdoc(
        High-level REST client for making HTTP requests.

        This class provides a convenient interface for interacting with REST APIs,
        with support for automatic JSON handling, retry logic, and error handling.

        Args:
            base_url: The base URL for all requests

        Examples:
            >>> client = RestClient("https://api.example.com")
            >>> response = client.get("/users/1")
            >>> print(response)
    )pbdoc")
        .def(py::init<const std::string&>(), py::arg("base_url"),
             "Create a REST client with the specified base URL")
        .def("get", &atom::extra::curl::RestClient::get, py::arg("endpoint"),
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Perform a GET request.

                 Args:
                     endpoint: The API endpoint (appended to base URL)
                     headers: Optional headers to include

                 Returns:
                     The response body as a string
             )pbdoc")
        .def("post", &atom::extra::curl::RestClient::post, py::arg("endpoint"),
             py::arg("body") = "",
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Perform a POST request.

                 Args:
                     endpoint: The API endpoint
                     body: The request body
                     headers: Optional headers to include

                 Returns:
                     The response body as a string
             )pbdoc")
        .def("put", &atom::extra::curl::RestClient::put, py::arg("endpoint"),
             py::arg("body") = "",
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Perform a PUT request.

                 Args:
                     endpoint: The API endpoint
                     body: The request body
                     headers: Optional headers to include

                 Returns:
                     The response body as a string
             )pbdoc")
        .def("del", &atom::extra::curl::RestClient::del, py::arg("endpoint"),
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Perform a DELETE request.

                 Args:
                     endpoint: The API endpoint
                     headers: Optional headers to include

                 Returns:
                     The response body as a string
             )pbdoc")
        .def("patch", &atom::extra::curl::RestClient::patch,
             py::arg("endpoint"), py::arg("body") = "",
             py::arg("headers") = std::map<std::string, std::string>{},
             R"pbdoc(
                 Perform a PATCH request.

                 Args:
                     endpoint: The API endpoint
                     body: The request body
                     headers: Optional headers to include

                 Returns:
                     The response body as a string
             )pbdoc")
        .def("set_default_header",
             &atom::extra::curl::RestClient::setDefaultHeader, py::arg("key"),
             py::arg("value"),
             R"pbdoc(
                 Set a default header for all requests.

                 Args:
                     key: Header name
                     value: Header value
             )pbdoc")
        .def("set_timeout", &atom::extra::curl::RestClient::setTimeout,
             py::arg("timeout_ms"),
             R"pbdoc(
                 Set the request timeout.

                 Args:
                     timeout_ms: Timeout in milliseconds
             )pbdoc")
        .def("set_max_retries", &atom::extra::curl::RestClient::setMaxRetries,
             py::arg("max_retries"),
             R"pbdoc(
                 Set the maximum number of retries for failed requests.

                 Args:
                     max_retries: Maximum retry count
             )pbdoc")
        .def("set_retry_delay", &atom::extra::curl::RestClient::setRetryDelay,
             py::arg("delay_ms"),
             R"pbdoc(
                 Set the delay between retries.

                 Args:
                     delay_ms: Delay in milliseconds
             )pbdoc");
}
