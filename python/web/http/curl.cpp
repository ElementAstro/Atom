#include "atom/web/curl.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void init_curl(py::module_& m) {
    m.doc() = R"pbdoc(
        CURL HTTP Client Module
        ----------------------

        This module provides a comprehensive wrapper around libcurl for performing HTTP requests
        with support for synchronous and asynchronous operations, file uploads, SSL configuration,
        and advanced features like proxy support and speed limiting.

        Key Features:
        - Synchronous and asynchronous HTTP requests
        - Support for all major HTTP methods (GET, POST, PUT, DELETE, etc.)
        - File upload and download capabilities
        - SSL/TLS configuration and verification
        - Proxy support
        - Request/response callbacks
        - Speed limiting and timeout control
        - Header management

        Examples:
            >>> from atom.web.curl import CurlWrapper
            >>>
            >>> # Simple GET request
            >>> curl = CurlWrapper()
            >>> curl.set_url("https://httpbin.org/get")
            >>> curl.set_request_method("GET")
            >>> response = curl.perform()
            >>> print(response)
            >>>
            >>> # POST request with JSON data
            >>> curl = CurlWrapper()
            >>> curl.set_url("https://httpbin.org/post")
            >>> curl.set_request_method("POST")
            >>> curl.add_header("Content-Type", "application/json")
            >>> curl.set_request_body('{"key": "value"}')
            >>> response = curl.perform()
            >>>
            >>> # File upload
            >>> curl = CurlWrapper()
            >>> curl.set_url("https://httpbin.org/post")
            >>> curl.set_request_method("POST")
            >>> curl.set_upload_file("/path/to/file.txt")
            >>> response = curl.perform()
            >>>
            >>> # Asynchronous request
            >>> curl = CurlWrapper()
            >>> curl.set_url("https://httpbin.org/get")
            >>> curl.set_on_response_callback(lambda resp: print(f"Got response: {len(resp)} bytes"))
            >>> curl.perform_async()
            >>> curl.wait_all()
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

    // CurlWrapper class binding
    py::class_<atom::web::CurlWrapper>(
        m, "CurlWrapper",
        R"(A comprehensive wrapper class for performing HTTP requests using libcurl.

This class provides a high-level interface for HTTP operations including GET, POST,
file uploads, SSL configuration, and both synchronous and asynchronous request execution.

Examples:
    >>> from atom.web.curl import CurlWrapper
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://example.com")
    >>> curl.set_request_method("GET")
    >>> response = curl.perform()
)")
        .def(py::init<>(), "Constructs a new CurlWrapper object.")

        .def("set_url", &atom::web::CurlWrapper::setUrl, py::arg("url"),
             R"(Sets the URL for the HTTP request.

Args:
    url: The URL to set for the request.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://api.example.com/data")
)")

        .def("set_request_method", &atom::web::CurlWrapper::setRequestMethod,
             py::arg("method"),
             R"(Sets the HTTP request method.

Args:
    method: The HTTP method to use (e.g., "GET", "POST", "PUT", "DELETE").

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_request_method("POST")
)")

        .def("add_header", &atom::web::CurlWrapper::addHeader, py::arg("key"),
             py::arg("value"),
             R"(Adds a header to the HTTP request.

Args:
    key: The header name.
    value: The header value.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.add_header("Content-Type", "application/json")
    >>> curl.add_header("Authorization", "Bearer token123")
)")

        .def("set_on_error_callback",
             &atom::web::CurlWrapper::setOnErrorCallback, py::arg("callback"),
             R"(Sets a callback function to handle errors.

Args:
    callback: A function that takes a CURLcode parameter and handles errors.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> def error_handler(code):
    ...     print(f"CURL error occurred: {code}")
    >>> curl = CurlWrapper()
    >>> curl.set_on_error_callback(error_handler)
)")

        .def("set_on_response_callback",
             &atom::web::CurlWrapper::setOnResponseCallback,
             py::arg("callback"),
             R"(Sets a callback function to handle the response.

Args:
    callback: A function that takes a string response and processes it.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> def response_handler(response):
    ...     print(f"Received {len(response)} bytes")
    >>> curl = CurlWrapper()
    >>> curl.set_on_response_callback(response_handler)
)")

        .def("set_timeout", &atom::web::CurlWrapper::setTimeout,
             py::arg("timeout"),
             R"(Sets the timeout for the HTTP request.

Args:
    timeout: The timeout value in seconds.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_timeout(30)  # 30 second timeout
)")

        .def("set_follow_location", &atom::web::CurlWrapper::setFollowLocation,
             py::arg("follow"),
             R"(Sets whether to follow HTTP redirects.

Args:
    follow: True to follow redirects, False otherwise.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_follow_location(True)  # Follow redirects
)")

        .def("set_request_body", &atom::web::CurlWrapper::setRequestBody,
             py::arg("data"),
             R"(Sets the request body data for POST/PUT requests.

Args:
    data: The request body data as a string.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_request_body('{"name": "John", "age": 30}')
)")

        .def("set_upload_file", &atom::web::CurlWrapper::setUploadFile,
             py::arg("file_path"),
             R"(Sets a file to upload with the request.

Args:
    file_path: The path to the file to upload.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_upload_file("/path/to/document.pdf")
)")

        .def("set_proxy", &atom::web::CurlWrapper::setProxy, py::arg("proxy"),
             R"(Sets the proxy server for the request.

Args:
    proxy: The proxy server URL (e.g., "http://proxy.example.com:8080").

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_proxy("http://proxy.company.com:3128")
)")

        .def("set_ssl_options", &atom::web::CurlWrapper::setSSLOptions,
             py::arg("verify_peer"), py::arg("verify_host"),
             R"(Sets SSL/TLS verification options.

Args:
    verify_peer: Whether to verify the peer's SSL certificate.
    verify_host: Whether to verify the host's SSL certificate.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_ssl_options(True, True)   # Verify both peer and host
    >>> curl.set_ssl_options(False, False) # Skip SSL verification (insecure)
)")

        .def("perform", &atom::web::CurlWrapper::perform,
             R"(Performs the HTTP request synchronously.

Returns:
    str: The response body as a string.

Raises:
    RuntimeError: If the request fails.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://httpbin.org/get")
    >>> response = curl.perform()
    >>> print(response)
)")

        .def("perform_async", &atom::web::CurlWrapper::performAsync,
             R"(Performs the HTTP request asynchronously.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Note:
    Use wait_all() to wait for completion or set response callbacks to handle results.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://httpbin.org/get")
    >>> curl.set_on_response_callback(lambda resp: print("Done!"))
    >>> curl.perform_async()
    >>> curl.wait_all()
)")

        .def("wait_all", &atom::web::CurlWrapper::waitAll,
             R"(Waits for all asynchronous requests to complete.

This method blocks until all pending asynchronous requests have finished.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_url("https://httpbin.org/get")
    >>> curl.perform_async()
    >>> curl.wait_all()  # Wait for completion
)")

        .def("set_max_download_speed",
             &atom::web::CurlWrapper::setMaxDownloadSpeed, py::arg("speed"),
             R"(Sets the maximum download speed limit.

Args:
    speed: The maximum download speed in bytes per second.

Returns:
    CurlWrapper: Reference to this object for method chaining.

Examples:
    >>> curl = CurlWrapper()
    >>> curl.set_max_download_speed(1024 * 1024)  # Limit to 1 MB/s
)");

    // Convenience functions
    m.def(
        "simple_get",
        [](const std::string& url, long timeout = 30) {
            atom::web::CurlWrapper curl;
            curl.setUrl(url).setRequestMethod("GET").setTimeout(timeout);
            return curl.perform();
        },
        py::arg("url"), py::arg("timeout") = 30,
        R"(Convenience function for simple GET requests.

Args:
    url: The URL to fetch.
    timeout: Request timeout in seconds (default: 30).

Returns:
    str: The response body.

Examples:
    >>> from atom.web.curl import simple_get
    >>> response = simple_get("https://httpbin.org/get")
)");

    m.def(
        "simple_post",
        [](const std::string& url, const std::string& data,
           const std::string& content_type = "application/json",
           long timeout = 30) {
            atom::web::CurlWrapper curl;
            curl.setUrl(url)
                .setRequestMethod("POST")
                .addHeader("Content-Type", content_type)
                .setRequestBody(data)
                .setTimeout(timeout);
            return curl.perform();
        },
        py::arg("url"), py::arg("data"),
        py::arg("content_type") = "application/json", py::arg("timeout") = 30,
        R"(Convenience function for simple POST requests.

Args:
    url: The URL to post to.
    data: The request body data.
    content_type: The Content-Type header (default: "application/json").
    timeout: Request timeout in seconds (default: 30).

Returns:
    str: The response body.

Examples:
    >>> from atom.web.curl import simple_post
    >>> response = simple_post("https://httpbin.org/post", '{"key": "value"}')
)");
}
