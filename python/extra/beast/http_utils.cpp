#include "atom/extra/beast/http_utils.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace http_utils;

PYBIND11_MODULE(http_utils, m) {
    m.doc() = "HTTP utilities module for the atom package";

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

    // Basic Authentication utility
    m.def("basic_auth", &basicAuth, py::arg("username"), py::arg("password"),
          R"(Creates a Basic Authentication Authorization header value.

Args:
    username: The username for authentication
    password: The password for authentication

Returns:
    The encoded Authorization header value (e.g., "Basic dXNlcjpwYXNz")

Examples:
    >>> from atom.http_utils import basic_auth
    >>> header_value = basic_auth("user", "pass")
    >>> # Use as: headers = {"Authorization": header_value}
)");

    // Compression utilities
    m.def(
        "compress",
        [](const std::string& data, bool is_gzip) {
            return compress(data, is_gzip);
        },
        py::arg("data"), py::arg("is_gzip") = true,
        R"(Compresses data using GZIP or DEFLATE algorithm.

Args:
    data: The data to compress (string)
    is_gzip: If True, uses GZIP format; otherwise, uses DEFLATE. Defaults to True.

Returns:
    The compressed data as a string.

Raises:
    RuntimeError: If compression initialization or execution fails.
)");

    m.def(
        "decompress",
        [](const std::string& data, bool is_gzip) {
            return decompress(data, is_gzip);
        },
        py::arg("data"), py::arg("is_gzip") = true,
        R"(Decompresses data compressed with GZIP or DEFLATE.

Args:
    data: The compressed data (string)
    is_gzip: If True, assumes GZIP format; otherwise, assumes DEFLATE. Defaults to True.

Returns:
    The decompressed data as a string.

Raises:
    RuntimeError: If decompression initialization or execution fails.
)");

    // URL encoding utilities
    m.def(
        "url_encode", [](const std::string& input) { return urlEncode(input); },
        py::arg("input"),
        R"(URL-encodes a string according to RFC 3986.

Unreserved characters (A-Z, a-z, 0-9, '-', '_', '.', '~') are not encoded.
Spaces are encoded as '%20' (not '+').

Args:
    input: The string to encode.

Returns:
    The URL-encoded string.

Examples:
    >>> from atom.http_utils import url_encode
    >>> url_encode("Hello World!")
    'Hello%20World%21'
)");

    m.def(
        "url_decode", [](const std::string& input) { return urlDecode(input); },
        py::arg("input"),
        R"(URL-decodes a string.

Decodes percent-encoded sequences (e.g., %20 becomes space).
Handles '+' as a space for compatibility with form data, though url_encode uses %20.

Args:
    input: The URL-encoded string.

Returns:
    The decoded string. Invalid percent-encoded sequences are passed through unchanged.

Examples:
    >>> from atom.http_utils import url_decode
    >>> url_decode("Hello%20World%21")
    'Hello World!'
)");

    // Query string builder
    m.def("build_query_string", &buildQueryString, py::arg("params"),
          R"(Builds a URL query string from a dictionary of parameters.

Keys and values are URL-encoded.

Args:
    params: A dictionary where keys are parameter names and values are parameter values.

Returns:
    The formatted query string (e.g., "key1=value1&key2=value2"). 
    Does not include the leading '?'.

Examples:
    >>> from atom.http_utils import build_query_string
    >>> build_query_string({"name": "John Doe", "age": "30"})
    'name=John%20Doe&age=30'
)");

    // Cookie parsing and building
    m.def(
        "parse_cookies",
        [](const std::string& cookie_header) {
            return parseCookies(cookie_header);
        },
        py::arg("cookie_header"),
        R"(Parses a Cookie header string into a dictionary of cookie names and values.

Args:
    cookie_header: The value of the Cookie HTTP header (e.g., "name1=value1; name2=value2").

Returns:
    A dictionary containing the parsed cookie names and values. Leading/trailing whitespace is trimmed.

Examples:
    >>> from atom.http_utils import parse_cookies
    >>> parse_cookies("session=abc123; user=john_doe")
    {'session': 'abc123', 'user': 'john_doe'}
)");

    m.def(
        "build_cookie_string", &buildCookieString, py::arg("cookies"),
        R"(Builds a Cookie header string from a dictionary of cookie names and values.

Args:
    cookies: A dictionary containing cookie names and values.

Returns:
    The formatted Cookie header string (e.g., "name1=value1; name2=value2").

Examples:
    >>> from atom.http_utils import build_cookie_string
    >>> build_cookie_string({"session": "abc123", "user": "john_doe"})
    'session=abc123; user=john_doe'
)");

    // CookieManager class
    py::class_<CookieManager>(
        m, "CookieManager",
        R"(Manages HTTP cookies, parsing Set-Cookie headers and adding Cookie headers.

Provides cookie storage and retrieval based on host matching. This is a simplified
implementation and does not handle all aspects of cookie management (e.g., complex
Path/Domain matching, Expires parsing, Max-Age, SameSite).

Examples:
    >>> from atom.http_utils import CookieManager
    >>> cm = CookieManager()
    >>> # After receiving a response with cookies:
    >>> cm.extract_cookies("example.com", response)
    >>> # When making a new request:
    >>> cm.add_cookies_to_request("example.com", "/api", True, request)
)")
        .def(py::init<>(), "Constructs a new CookieManager.")
        .def(
            "extract_cookies", &CookieManager::extractCookies,
            py::arg("request_host"), py::arg("response"),
            R"(Extracts cookies from the Set-Cookie headers of an HTTP response.

Args:
    request_host: The original host the request was sent to (used for default domain).
    response: The HTTP response object.
)")
        .def(
            "add_cookies_to_request", &CookieManager::addCookiesToRequest,
            py::arg("request_host"), py::arg("request_path"),
            py::arg("is_secure"), py::arg("request"),
            R"(Adds applicable stored cookies to the Cookie header of an HTTP request.

Args:
    request_host: The host the request is being sent to.
    request_path: The path the request is being sent to.
    is_secure: True if the connection is secure (HTTPS), False otherwise.
    request: The HTTP request object to modify.
)")
        .def("clear_cookies", &CookieManager::clearCookies,
             "Clears all cookies stored in the manager.")
        .def(
            "get_cookie",
            [](CookieManager& self, const std::string& host,
               const std::string& name) { return self.getCookie(host, name); },
            py::arg("host"), py::arg("name"),
            R"(Gets the value of a specific cookie for a given host and name.

Note: This is a simplified retrieval and might not reflect the exact cookie that
would be sent in a request due to path/domain matching rules. Prefer using
add_cookies_to_request for accurate cookie selection.

Args:
    host: The host context.
    name: The name of the cookie.

Returns:
    The cookie value if found matching the host and name (considering domain matching),
    otherwise an empty string. Returns the first match found.
)");
}