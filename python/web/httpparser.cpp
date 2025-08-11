#include "atom/web/httpparser.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;
namespace aw = atom::web;

PYBIND11_MODULE(httpparser, m) {
    m.doc() = "HTTP parser implementation module for the atom package";

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

    // Enum class HttpMethod binding
    py::enum_<aw::HttpMethod>(m, "HttpMethod",
                              R"(HTTP method enumeration.

This enum represents the standard HTTP methods used in HTTP requests.

Examples:
    >>> from atom.web.httpparser import HttpMethod
    >>> method = HttpMethod.GET
    >>> method == HttpMethod.POST
    False
)")
        .value("GET", aw::HttpMethod::GET, "HTTP GET method")
        .value("POST", aw::HttpMethod::POST, "HTTP POST method")
        .value("PUT", aw::HttpMethod::PUT, "HTTP PUT method")
        .value("DELETE", aw::HttpMethod::DELETE, "HTTP DELETE method")
        .value("HEAD", aw::HttpMethod::HEAD, "HTTP HEAD method")
        .value("OPTIONS", aw::HttpMethod::OPTIONS, "HTTP OPTIONS method")
        .value("PATCH", aw::HttpMethod::PATCH, "HTTP PATCH method")
        .value("TRACE", aw::HttpMethod::TRACE, "HTTP TRACE method")
        .value("CONNECT", aw::HttpMethod::CONNECT, "HTTP CONNECT method")
        .value("UNKNOWN", aw::HttpMethod::UNKNOWN, "Unknown HTTP method")
        .export_values();

    // Enum class HttpVersion binding
    py::enum_<aw::HttpVersion>(m, "HttpVersion",
                               R"(HTTP version enumeration.

This enum represents the different HTTP protocol versions.

Examples:
    >>> from atom.web.httpparser import HttpVersion
    >>> version = HttpVersion.HTTP_1_1
    >>> version == HttpVersion.HTTP_2_0
    False
)")
        .value("HTTP_1_0", aw::HttpVersion::HTTP_1_0, "HTTP version 1.0")
        .value("HTTP_1_1", aw::HttpVersion::HTTP_1_1, "HTTP version 1.1")
        .value("HTTP_2_0", aw::HttpVersion::HTTP_2_0, "HTTP version 2.0")
        .value("HTTP_3_0", aw::HttpVersion::HTTP_3_0, "HTTP version 3.0")
        .value("UNKNOWN", aw::HttpVersion::UNKNOWN, "Unknown HTTP version")
        .export_values();

    // HttpStatus class binding
    py::class_<aw::HttpStatus>(m, "HttpStatus",
                               R"(HTTP status code and description.

This class represents an HTTP response status, consisting of a numeric code and a text description.

Attributes:
    code: The numeric HTTP status code.
    description: The textual description of the status.

Examples:
    >>> from atom.web.httpparser import HttpStatus
    >>> status = HttpStatus.OK()
    >>> status.code
    200
    >>> status.description
    'OK'
)")
        .def(py::init<int, const std::string&>(), py::arg("code"),
             py::arg("description"),
             "Constructs a new HttpStatus object with given code and "
             "description.")
        .def_readwrite("code", &aw::HttpStatus::code, "HTTP status code")
        .def_readwrite("description", &aw::HttpStatus::description,
                       "HTTP status description")
        // Static factory methods
        .def_static("OK", &aw::HttpStatus::OK, "Returns HTTP 200 OK status")
        .def_static("Created", &aw::HttpStatus::Created,
                    "Returns HTTP 201 Created status")
        .def_static("Accepted", &aw::HttpStatus::Accepted,
                    "Returns HTTP 202 Accepted status")
        .def_static("NoContent", &aw::HttpStatus::NoContent,
                    "Returns HTTP 204 No Content status")
        .def_static("MovedPermanently", &aw::HttpStatus::MovedPermanently,
                    "Returns HTTP 301 Moved Permanently status")
        .def_static("Found", &aw::HttpStatus::Found,
                    "Returns HTTP 302 Found status")
        .def_static("BadRequest", &aw::HttpStatus::BadRequest,
                    "Returns HTTP 400 Bad Request status")
        .def_static("Unauthorized", &aw::HttpStatus::Unauthorized,
                    "Returns HTTP 401 Unauthorized status")
        .def_static("Forbidden", &aw::HttpStatus::Forbidden,
                    "Returns HTTP 403 Forbidden status")
        .def_static("NotFound", &aw::HttpStatus::NotFound,
                    "Returns HTTP 404 Not Found status")
        .def_static("MethodNotAllowed", &aw::HttpStatus::MethodNotAllowed,
                    "Returns HTTP 405 Method Not Allowed status")
        .def_static("InternalServerError", &aw::HttpStatus::InternalServerError,
                    "Returns HTTP 500 Internal Server Error status")
        .def_static("NotImplemented", &aw::HttpStatus::NotImplemented,
                    "Returns HTTP 501 Not Implemented status")
        .def_static("BadGateway", &aw::HttpStatus::BadGateway,
                    "Returns HTTP 502 Bad Gateway status")
        .def_static("ServiceUnavailable", &aw::HttpStatus::ServiceUnavailable,
                    "Returns HTTP 503 Service Unavailable status")
        // Python-specific methods
        .def(
            "__str__",
            [](const aw::HttpStatus& status) {
                return std::to_string(status.code) + " " + status.description;
            },
            "Returns string representation of the status code and description")
        .def(
            "__eq__",
            [](const aw::HttpStatus& a, const aw::HttpStatus& b) {
                return a.code == b.code;
            },
            py::is_operator())
        .def(
            "__ne__",
            [](const aw::HttpStatus& a, const aw::HttpStatus& b) {
                return a.code != b.code;
            },
            py::is_operator());

    // Cookie struct binding
    py::class_<aw::Cookie>(m, "Cookie",
                           R"(HTTP Cookie representation.

This class represents an HTTP cookie with its various attributes.

Attributes:
    name: The name of the cookie.
    value: The value of the cookie.
    expires: Optional expiration time.
    max_age: Optional maximum age in seconds.
    domain: Optional domain for which the cookie is valid.
    path: Optional path for which the cookie is valid.
    secure: Whether the cookie should only be sent over HTTPS.
    http_only: Whether the cookie is accessible only through HTTP.
    same_site: Optional SameSite attribute ("Strict", "Lax", or "None").

Examples:
    >>> from atom.web.httpparser import Cookie
    >>> cookie = Cookie()
    >>> cookie.name = "sessionid"
    >>> cookie.value = "abc123"
    >>> cookie.http_only = True
)")
        .def(py::init<>(), "Constructs a new Cookie object.")
        .def_readwrite("name", &aw::Cookie::name, "Cookie name")
        .def_readwrite("value", &aw::Cookie::value, "Cookie value")
        .def_readwrite("expires", &aw::Cookie::expires,
                       "Optional expiration time")
        .def_readwrite("max_age", &aw::Cookie::maxAge,
                       "Optional maximum age in seconds")
        .def_readwrite("domain", &aw::Cookie::domain,
                       "Optional domain for which the cookie is valid")
        .def_readwrite("path", &aw::Cookie::path,
                       "Optional path for which the cookie is valid")
        .def_readwrite("secure", &aw::Cookie::secure,
                       "Whether the cookie should only be sent over HTTPS")
        .def_readwrite("http_only", &aw::Cookie::httpOnly,
                       "Whether the cookie is accessible only through HTTP")
        .def_readwrite("same_site", &aw::Cookie::sameSite,
                       "Optional SameSite attribute")
        // Python-specific methods
        .def(
            "__str__",
            [](const aw::Cookie& cookie) {
                return cookie.name + "=" + cookie.value;
            },
            "Returns string representation of the cookie");

    // HttpHeaderParser class binding
    py::class_<aw::HttpHeaderParser>(
        m, "HttpHeaderParser",
        R"(Parser for HTTP headers, requests, and responses.

This class provides functionality to parse, manipulate, and construct HTTP headers,
requests, and responses.

Examples:
    >>> from atom.web.httpparser import HttpHeaderParser
    >>> parser = HttpHeaderParser()
    >>> parser.parse_request("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
    True
    >>> parser.get_method()
    <HttpMethod.GET: 0>
)")
        .def(py::init<>(), "Constructs a new HttpHeaderParser object.")
        .def("parse_headers", &aw::HttpHeaderParser::parseHeaders,
             py::arg("raw_headers"),
             R"(Parses raw HTTP headers.

Args:
    raw_headers: The raw HTTP headers as a string.
)")
        .def("parse_request", &aw::HttpHeaderParser::parseRequest,
             py::arg("raw_request"),
             R"(Parses a complete HTTP request.

Args:
    raw_request: The raw HTTP request as a string.

Returns:
    bool: True if the request was parsed successfully, False otherwise.
)")
        .def("parse_response", &aw::HttpHeaderParser::parseResponse,
             py::arg("raw_response"),
             R"(Parses a complete HTTP response.

Args:
    raw_response: The raw HTTP response as a string.

Returns:
    bool: True if the response was parsed successfully, False otherwise.
)")
        .def("set_header_value", &aw::HttpHeaderParser::setHeaderValue,
             py::arg("key"), py::arg("value"),
             R"(Sets the value of a specific header.

Args:
    key: The header name.
    value: The header value.
)")
        .def("set_headers", &aw::HttpHeaderParser::setHeaders,
             py::arg("headers"),
             R"(Sets multiple header fields at once.

Args:
    headers: A dictionary mapping header names to lists of values.
)")
        .def("add_header_value", &aw::HttpHeaderParser::addHeaderValue,
             py::arg("key"), py::arg("value"),
             R"(Adds a value to an existing header or creates a new header.

Args:
    key: The header name.
    value: The value to add.
)")
        .def("get_header_values", &aw::HttpHeaderParser::getHeaderValues,
             py::arg("key"),
             R"(Gets all values for a specific header.

Args:
    key: The header name.

Returns:
    List of values for the header, or None if the header doesn't exist.
)")
        .def("get_header_value", &aw::HttpHeaderParser::getHeaderValue,
             py::arg("key"),
             R"(Gets the first value for a specific header.

Args:
    key: The header name.

Returns:
    The first value for the header, or None if the header doesn't exist.
)")
        .def("remove_header", &aw::HttpHeaderParser::removeHeader,
             py::arg("key"),
             R"(Removes a specific header.

Args:
    key: The header name to remove.
)")
        .def("get_all_headers", &aw::HttpHeaderParser::getAllHeaders,
             "Returns all parsed headers as a dictionary.")
        .def("has_header", &aw::HttpHeaderParser::hasHeader, py::arg("key"),
             R"(Checks if a specific header exists.

Args:
    key: The header name to check.

Returns:
    bool: True if the header exists, False otherwise.
)")
        .def("clear_headers", &aw::HttpHeaderParser::clearHeaders,
             "Clears all headers.")
        .def("add_cookie", &aw::HttpHeaderParser::addCookie, py::arg("cookie"),
             R"(Adds a cookie.

Args:
    cookie: The Cookie object to add.
)")
        .def("parse_cookies", &aw::HttpHeaderParser::parseCookies,
             py::arg("cookie_str"),
             R"(Parses a Cookie header string.

Args:
    cookie_str: The Cookie header string to parse.

Returns:
    dict: A dictionary mapping cookie names to values.
)")
        .def("get_all_cookies", &aw::HttpHeaderParser::getAllCookies,
             "Returns all cookies.")
        .def("get_cookie", &aw::HttpHeaderParser::getCookie, py::arg("name"),
             R"(Gets a specific cookie by name.

Args:
    name: The name of the cookie to retrieve.

Returns:
    The Cookie object, or None if not found.
)")
        .def("remove_cookie", &aw::HttpHeaderParser::removeCookie,
             py::arg("name"),
             R"(Removes a specific cookie.

Args:
    name: The name of the cookie to remove.
)")
        .def("parse_url_parameters", &aw::HttpHeaderParser::parseUrlParameters,
             py::arg("url"),
             R"(Parses URL query parameters.

Args:
    url: The URL containing query parameters.

Returns:
    dict: A dictionary mapping parameter names to values.
)")
        .def("set_method", &aw::HttpHeaderParser::setMethod, py::arg("method"),
             R"(Sets the HTTP method.

Args:
    method: The HTTP method to set.
)")
        .def("get_method", &aw::HttpHeaderParser::getMethod,
             "Returns the current HTTP method.")
        .def_static("string_to_method", &aw::HttpHeaderParser::stringToMethod,
                    py::arg("method_str"),
                    R"(Converts a string to an HTTP method enum value.

Args:
    method_str: The method name as a string.

Returns:
    The corresponding HttpMethod enum value.
)")
        .def_static("method_to_string", &aw::HttpHeaderParser::methodToString,
                    py::arg("method"),
                    R"(Converts an HTTP method enum value to a string.

Args:
    method: The HttpMethod enum value.

Returns:
    The method name as a string.
)")
        .def("set_status", &aw::HttpHeaderParser::setStatus, py::arg("status"),
             R"(Sets the HTTP status.

Args:
    status: The HttpStatus object to set.
)")
        .def("get_status", &aw::HttpHeaderParser::getStatus,
             "Returns the current HTTP status.")
        .def("set_path", &aw::HttpHeaderParser::setPath, py::arg("path"),
             R"(Sets the URL path.

Args:
    path: The URL path to set.
)")
        .def("get_path", &aw::HttpHeaderParser::getPath,
             "Returns the current URL path.")
        .def("set_version", &aw::HttpHeaderParser::setVersion,
             py::arg("version"),
             R"(Sets the HTTP version.

Args:
    version: The HTTP version to set.
)")
        .def("get_version", &aw::HttpHeaderParser::getVersion,
             "Returns the current HTTP version.")
        .def("set_body", &aw::HttpHeaderParser::setBody, py::arg("body"),
             R"(Sets the request or response body.

Args:
    body: The body content to set.
)")
        .def("get_body", &aw::HttpHeaderParser::getBody,
             "Returns the current body content.")
        .def("build_request", &aw::HttpHeaderParser::buildRequest,
             "Builds and returns a complete HTTP request string.")
        .def("build_response", &aw::HttpHeaderParser::buildResponse,
             "Builds and returns a complete HTTP response string.")
        .def_static("url_encode", &aw::HttpHeaderParser::urlEncode,
                    py::arg("str"),
                    R"(URL-encodes a string.

Args:
    str: The string to encode.

Returns:
    The URL-encoded string.
)")
        .def_static("url_decode", &aw::HttpHeaderParser::urlDecode,
                    py::arg("str"),
                    R"(URL-decodes a string.

Args:
    str: The string to decode.

Returns:
    The URL-decoded string.
)");

    // Convenience functions
    m.def(
        "parse_request",
        [](const std::string& raw_request) {
            aw::HttpHeaderParser parser;
            bool success = parser.parseRequest(raw_request);
            if (!success) {
                throw py::value_error("Failed to parse HTTP request");
            }
            return parser;
        },
        py::arg("raw_request"),
        R"(Parses an HTTP request and returns a parser object.

Args:
    raw_request: The raw HTTP request string.

Returns:
    HttpHeaderParser: Parser with the parsed request data.

Raises:
    ValueError: If parsing fails.

Examples:
    >>> from atom.web.httpparser import parse_request
    >>> parser = parse_request("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
    >>> parser.get_path()
    '/'
)");

    m.def(
        "parse_response",
        [](const std::string& raw_response) {
            aw::HttpHeaderParser parser;
            bool success = parser.parseResponse(raw_response);
            if (!success) {
                throw py::value_error("Failed to parse HTTP response");
            }
            return parser;
        },
        py::arg("raw_response"),
        R"(Parses an HTTP response and returns a parser object.

Args:
    raw_response: The raw HTTP response string.

Returns:
    HttpHeaderParser: Parser with the parsed response data.

Raises:
    ValueError: If parsing fails.

Examples:
    >>> from atom.web.httpparser import parse_response
    >>> parser = parse_response("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html></html>")
    >>> parser.get_status().code
    200
)");

    m.def("url_encode", &aw::HttpHeaderParser::urlEncode, py::arg("string"),
          R"(URL-encodes a string.

Args:
    string: The string to encode.

Returns:
    The URL-encoded string.

Examples:
    >>> from atom.web.httpparser import url_encode
    >>> url_encode("Hello World!")
    'Hello%20World%21'
)");

    m.def("url_decode", &aw::HttpHeaderParser::urlDecode, py::arg("string"),
          R"(URL-decodes a string.

Args:
    string: The string to decode.

Returns:
    The URL-decoded string.

Examples:
    >>> from atom.web.httpparser import url_decode
    >>> url_decode("Hello%20World%21")
    'Hello World!'
)");

    m.def(
        "create_request",
        [](aw::HttpMethod method, const std::string& path,
           aw::HttpVersion version,
           const std::map<std::string, std::vector<std::string>>& headers = {},
           const std::string& body = "") {
            aw::HttpHeaderParser parser;
            parser.setMethod(method);
            parser.setPath(path);
            parser.setVersion(version);
            parser.setHeaders(headers);
            if (!body.empty()) {
                parser.setBody(body);
            }
            return parser;
        },
        py::arg("method"), py::arg("path"), py::arg("version"),
        py::arg("headers") = std::map<std::string, std::vector<std::string>>(),
        py::arg("body") = "",
        R"(Creates an HTTP request parser with the specified parameters.

Args:
    method: The HTTP method.
    path: The request path.
    version: The HTTP version.
    headers: Optional dictionary of headers.
    body: Optional request body.

Returns:
    HttpHeaderParser: Parser configured with the request data.

Examples:
    >>> from atom.web.httpparser import create_request, HttpMethod, HttpVersion
    >>> parser = create_request(HttpMethod.POST, "/api/data", HttpVersion.HTTP_1_1,
    ...                         {"Content-Type": ["application/json"]}, '{"key": "value"}')
    >>> parser.build_request()
    'POST /api/data HTTP/1.1\r\nContent-Type: application/json\r\n\r\n{"key": "value"}'
)");

    m.def(
        "create_response",
        [](const aw::HttpStatus& status, aw::HttpVersion version,
           const std::map<std::string, std::vector<std::string>>& headers = {},
           const std::string& body = "") {
            aw::HttpHeaderParser parser;
            parser.setStatus(status);
            parser.setVersion(version);
            parser.setHeaders(headers);
            if (!body.empty()) {
                parser.setBody(body);
            }
            return parser;
        },
        py::arg("status"), py::arg("version"),
        py::arg("headers") = std::map<std::string, std::vector<std::string>>(),
        py::arg("body") = "",
        R"(Creates an HTTP response parser with the specified parameters.

Args:
    status: The HTTP status.
    version: The HTTP version.
    headers: Optional dictionary of headers.
    body: Optional response body.

Returns:
    HttpHeaderParser: Parser configured with the response data.

Examples:
    >>> from atom.web.httpparser import create_response, HttpStatus, HttpVersion
    >>> parser = create_response(HttpStatus.OK(), HttpVersion.HTTP_1_1,
    ...                          {"Content-Type": ["text/html"]}, '<html>Hello</html>')
    >>> parser.build_response()
    'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html>Hello</html>'
)");
}
