#include "atom/extra/curl/session.hpp"
#include "atom/extra/curl/request.hpp"
#include "atom/extra/curl/response.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(session, m) {
    m.doc() = R"(HTTP Session module for the atom package.

This module provides a high-level interface for making HTTP requests using libcurl,
with support for connection pooling, caching, rate limiting, and more.

Examples:
    >>> from atom.extra.curl import session
    >>> 
    >>> # Create a session
    >>> s = session.Session()
    >>> 
    >>> # Perform GET request
    >>> response = s.get("https://httpbin.org/get")
    >>> print(response.status_code())
    >>> print(response.body())
    >>> 
    >>> # Perform POST request
    >>> response = s.post("https://httpbin.org/post", '{"key": "value"}')
    >>> print(response.body())
    >>> 
    >>> # Use request builder
    >>> request = session.Request()
    >>> request.method(session.Request.Method.GET)
    >>> request.url("https://httpbin.org/get")
    >>> request.header("User-Agent", "MyApp/1.0")
    >>> response = s.execute(request)
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

    // Request::Method enum
    py::enum_<atom::extra::curl::Request::Method>(m, "Method",
                                                  R"(HTTP request method enumeration.

Defines the standard HTTP methods for requests.)")
        .value("GET", atom::extra::curl::Request::Method::GET, "HTTP GET method")
        .value("POST", atom::extra::curl::Request::Method::POST, "HTTP POST method")
        .value("PUT", atom::extra::curl::Request::Method::PUT, "HTTP PUT method")
        .value("DELETE", atom::extra::curl::Request::Method::DELETE, "HTTP DELETE method")
        .value("PATCH", atom::extra::curl::Request::Method::PATCH, "HTTP PATCH method")
        .value("HEAD", atom::extra::curl::Request::Method::HEAD, "HTTP HEAD method")
        .value("OPTIONS", atom::extra::curl::Request::Method::OPTIONS, "HTTP OPTIONS method")
        .export_values();

    // Request class binding
    py::class_<atom::extra::curl::Request>(m, "Request",
                                           R"(HTTP request builder class.

This class provides a fluent interface for building HTTP requests,
allowing you to set various options such as the URL, method, headers,
body, timeout, and more.

Examples:
    >>> request = session.Request()
    >>> request.method(session.Method.POST)
    >>> request.url("https://api.example.com/data")
    >>> request.header("Content-Type", "application/json")
    >>> request.header("Authorization", "Bearer token123")
    >>> request.body('{"name": "test", "value": 42}')
    >>> request.timeout(30)
)")
        .def(py::init<>(), "Create a new HTTP request")
        .def("method", &atom::extra::curl::Request::method,
             py::arg("method"),
             R"(Set the HTTP method for the request.

Args:
    method: The HTTP method to use.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("url", &atom::extra::curl::Request::url,
             py::arg("url"),
             R"(Set the URL for the request.

Args:
    url: The URL to request.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("header", &atom::extra::curl::Request::header,
             py::arg("name"), py::arg("value"),
             R"(Set a header for the request.

Args:
    name: The name of the header.
    value: The value of the header.

Returns:
    Reference to this Request object for method chaining.

Examples:
    >>> request.header("Content-Type", "application/json")
    >>> request.header("User-Agent", "MyApp/1.0")
)")
        .def("body", &atom::extra::curl::Request::body,
             py::arg("body"),
             R"(Set the body for the request.

Args:
    body: The body content as a string.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("timeout", &atom::extra::curl::Request::timeout,
             py::arg("timeout_seconds"),
             R"(Set the timeout for the request.

Args:
    timeout_seconds: Timeout in seconds.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("follow_redirects", &atom::extra::curl::Request::follow_redirects,
             py::arg("follow"),
             R"(Set whether to follow redirects.

Args:
    follow: Whether to follow redirects.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("verify_ssl", &atom::extra::curl::Request::verify_ssl,
             py::arg("verify"),
             R"(Set whether to verify SSL certificates.

Args:
    verify: Whether to verify SSL certificates.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("user_agent", &atom::extra::curl::Request::user_agent,
             py::arg("user_agent"),
             R"(Set the User-Agent header.

Args:
    user_agent: The User-Agent string.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("basic_auth", &atom::extra::curl::Request::basic_auth,
             py::arg("username"), py::arg("password"),
             R"(Set basic authentication credentials.

Args:
    username: The username for authentication.
    password: The password for authentication.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("bearer_token", &atom::extra::curl::Request::bearer_token,
             py::arg("token"),
             R"(Set Bearer token authentication.

Args:
    token: The Bearer token.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("proxy", &atom::extra::curl::Request::proxy,
             py::arg("proxy_url"),
             R"(Set proxy server.

Args:
    proxy_url: The proxy server URL.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("max_redirects", &atom::extra::curl::Request::max_redirects,
             py::arg("max_redirects"),
             R"(Set maximum number of redirects to follow.

Args:
    max_redirects: Maximum number of redirects.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("connect_timeout", &atom::extra::curl::Request::connect_timeout,
             py::arg("timeout_seconds"),
             R"(Set connection timeout.

Args:
    timeout_seconds: Connection timeout in seconds.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("low_speed_limit", &atom::extra::curl::Request::low_speed_limit,
             py::arg("bytes_per_second"), py::arg("time_seconds"),
             R"(Set low speed limit.

Args:
    bytes_per_second: Minimum transfer speed in bytes per second.
    time_seconds: Time period for the speed check.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("cookie", &atom::extra::curl::Request::cookie,
             py::arg("name"), py::arg("value"),
             R"(Add a cookie to the request.

Args:
    name: Cookie name.
    value: Cookie value.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("form_data", &atom::extra::curl::Request::form_data,
             py::arg("name"), py::arg("value"),
             R"(Add form data field.

Args:
    name: Field name.
    value: Field value.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("file_upload", &atom::extra::curl::Request::file_upload,
             py::arg("field_name"), py::arg("file_path"),
             R"(Add file upload field.

Args:
    field_name: Form field name for the file.
    file_path: Path to the file to upload.

Returns:
    Reference to this Request object for method chaining.
)")
        .def("retry_on_error", &atom::extra::curl::Request::retry_on_error,
             py::arg("retry"),
             R"(Set whether to retry on error.

Args:
    retry: Whether to retry on error.

Returns:
    Reference to this Request object for method chaining.
)");

    // Response class binding
    py::class_<atom::extra::curl::Response>(m, "Response",
                                            R"(HTTP response class.

This class encapsulates the data associated with an HTTP response,
including the status code, body, and headers.

Examples:
    >>> response = session.get("https://httpbin.org/get")
    >>> print(f"Status: {response.status_code()}")
    >>> print(f"Body: {response.body()}")
    >>> print(f"Headers: {response.headers()}")
    >>>
    >>> # Check if request was successful
    >>> if response.is_success():
    ...     print("Request succeeded")
)")
        .def(py::init<>(), "Create an empty response")
        .def("status_code", &atom::extra::curl::Response::status_code,
             R"(Get the HTTP status code.

Returns:
    The HTTP status code as an integer.
)")
        .def("body_string", &atom::extra::curl::Response::body_string,
             R"(Get the response body as a string.

Returns:
    The response body content.
)")
        .def("json", &atom::extra::curl::Response::json,
             R"(Parse the response body as JSON.

Returns:
    The JSON representation of the response body as a string.
)")
        .def("headers", &atom::extra::curl::Response::headers,
             R"(Get all response headers.

Returns:
    Dictionary of header names to values.
)")
        .def("get_header", &atom::extra::curl::Response::get_header,
             py::arg("name"),
             R"(Get a specific header value.

Args:
    name: The header name to retrieve.

Returns:
    The header value, or empty string if not found.
)")
        .def("has_header", &atom::extra::curl::Response::has_header,
             py::arg("name"),
             R"(Check if a header exists.

Args:
    name: The header name to check.

Returns:
    True if the header exists.
)")
        .def("ok", &atom::extra::curl::Response::ok,
             R"(Check if the response indicates success.

Returns:
    True if status code is in the 200-299 range.
)")
        .def("redirect", &atom::extra::curl::Response::redirect,
             R"(Check if the response is a redirect.

Returns:
    True if status code is in the 300-399 range.
)")
        .def("client_error", &atom::extra::curl::Response::client_error,
             R"(Check if the response is a client error.

Returns:
    True if status code is in the 400-499 range.
)")
        .def("server_error", &atom::extra::curl::Response::server_error,
             R"(Check if the response is a server error.

Returns:
    True if status code is in the 500-599 range.
)")
        .def("content_type", &atom::extra::curl::Response::content_type,
             R"(Get the Content-Type header value.

Returns:
    Optional Content-Type header value.
)")
        .def("content_length", &atom::extra::curl::Response::content_length,
             R"(Get the Content-Length header value.

Returns:
    Optional Content-Length as an integer.
)");

    // Session class binding
    py::class_<atom::extra::curl::Session>(m, "Session",
                                           R"(HTTP session class for performing requests.

This class provides a high-level interface for making HTTP requests,
handling cookies, caching, rate limiting, and more.

Examples:
    >>> session = Session()
    >>>
    >>> # Simple GET request
    >>> response = session.get("https://httpbin.org/get")
    >>>
    >>> # POST with JSON data
    >>> response = session.post_json("https://httpbin.org/post", '{"key": "value"}')
    >>>
    >>> # Custom request
    >>> request = Request()
    >>> request.method(Method.PUT).url("https://httpbin.org/put").body("data")
    >>> response = session.execute(request)
)")
        .def(py::init<>(), "Create a new HTTP session")
        .def("execute", &atom::extra::curl::Session::execute,
             py::arg("request"),
             R"(Execute an HTTP request.

Args:
    request: The HTTP request to execute.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("get",
             py::overload_cast<std::string_view>(&atom::extra::curl::Session::get),
             py::arg("url"),
             R"(Perform a GET request.

Args:
    url: The URL to request.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("get",
             py::overload_cast<std::string_view, const std::map<std::string, std::string>&>(
                 &atom::extra::curl::Session::get),
             py::arg("url"), py::arg("params"),
             R"(Perform a GET request with query parameters.

Args:
    url: The URL to request.
    params: Dictionary of query parameters to add to the URL.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("post", &atom::extra::curl::Session::post,
             py::arg("url"), py::arg("body"),
             py::arg("content_type") = "application/json",
             R"(Perform a POST request.

Args:
    url: The URL to request.
    body: The body of the request.
    content_type: The content type of the request (default: application/json).

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("post_form", &atom::extra::curl::Session::post_form,
             py::arg("url"), py::arg("params"),
             R"(Perform a form URL encoded POST request.

Args:
    url: The URL to request.
    params: Dictionary of form parameters to add to the body.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("post_json", &atom::extra::curl::Session::post_json,
             py::arg("url"), py::arg("json"),
             R"(Perform a JSON POST request.

Args:
    url: The URL to request.
    json: The JSON body of the request as a string.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("put", &atom::extra::curl::Session::put,
             py::arg("url"), py::arg("body"),
             py::arg("content_type") = "application/json",
             R"(Perform a PUT request.

Args:
    url: The URL to request.
    body: The body of the request.
    content_type: The content type of the request (default: application/json).

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("del", &atom::extra::curl::Session::del,
             py::arg("url"),
             R"(Perform a DELETE request.

Args:
    url: The URL to request.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("patch", &atom::extra::curl::Session::patch,
             py::arg("url"), py::arg("body"),
             py::arg("content_type") = "application/json",
             R"(Perform a PATCH request.

Args:
    url: The URL to request.
    body: The body of the request.
    content_type: The content type of the request (default: application/json).

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("head", &atom::extra::curl::Session::head,
             py::arg("url"),
             R"(Perform a HEAD request.

Args:
    url: The URL to request.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)")
        .def("options", &atom::extra::curl::Session::options,
             py::arg("url"),
             R"(Perform an OPTIONS request.

Args:
    url: The URL to request.

Returns:
    The HTTP response.

Raises:
    RuntimeError: If the request fails.
)");
}
