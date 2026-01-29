#include "atom/extra/curl/cookie.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(cookie, m) {
    m.doc() = R"(HTTP cookie management module for the atom package.

This module provides classes for managing HTTP cookies, including
individual cookies and cookie jars for storing collections of cookies.

Examples:
    >>> from atom.extra.curl import cookie
    >>> import datetime
    >>>
    >>> # Create a cookie
    >>> c = cookie.Cookie("session_id", "abc123", "example.com", "/")
    >>> print(c.to_string())
    >>>
    >>> # Create a cookie jar
    >>> jar = cookie.CookieJar()
    >>> jar.set_cookie(c)
    >>>
    >>> # Get a cookie
    >>> retrieved = jar.get_cookie("session_id")
    >>> if retrieved:
    ...     print(f"Cookie value: {retrieved.value()}")
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

    // Cookie class binding
    py::class_<atom::extra::curl::Cookie>(m, "Cookie",
                                          R"(Represents an HTTP cookie.

This class encapsulates the data associated with an HTTP cookie,
including its name, value, domain, path, security settings, and
expiration time.

Examples:
    >>> # Create a simple cookie
    >>> cookie = Cookie("name", "value")
    >>>
    >>> # Create a secure cookie with expiration
    >>> import datetime
    >>> expires = datetime.datetime.now() + datetime.timedelta(days=7)
    >>> cookie = Cookie("session", "xyz", "example.com", "/", True, True, expires)
    >>>
    >>> # Convert to string for HTTP header
    >>> header_value = cookie.to_string()
)")
        .def(py::init<>(), "Create an empty cookie")
        .def(py::init<std::string, std::string, std::string, std::string, bool,
                      bool,
                      std::optional<std::chrono::system_clock::time_point>>(),
             py::arg("name"), py::arg("value"), py::arg("domain") = "",
             py::arg("path") = "/", py::arg("secure") = false,
             py::arg("http_only") = false, py::arg("expires") = std::nullopt,
             R"(Constructor for the Cookie class.

Args:
    name: The name of the cookie.
    value: The value of the cookie.
    domain: The domain the cookie is valid for (default: "").
    path: The path the cookie is valid for (default: "/").
    secure: True if the cookie should only be transmitted over HTTPS (default: False).
    http_only: True if the cookie should only be accessible through HTTP(S) (default: False).
    expires: Optional expiration time for the cookie (default: None for session cookie).
)")
        .def(
            "to_string", &atom::extra::curl::Cookie::to_string,
            R"(Convert the cookie to a string representation suitable for HTTP header.

Returns:
    A string representation of the cookie.
)")
        .def("name", &atom::extra::curl::Cookie::name,
             R"(Get the name of the cookie.

Returns:
    The name of the cookie.
)")
        .def("value", &atom::extra::curl::Cookie::value,
             R"(Get the value of the cookie.

Returns:
    The value of the cookie.
)")
        .def("domain", &atom::extra::curl::Cookie::domain,
             R"(Get the domain the cookie is valid for.

Returns:
    The domain the cookie is valid for.
)")
        .def("path", &atom::extra::curl::Cookie::path,
             R"(Get the path the cookie is valid for.

Returns:
    The path the cookie is valid for.
)")
        .def("secure", &atom::extra::curl::Cookie::secure,
             R"(Check if the cookie should only be transmitted over HTTPS.

Returns:
    True if the cookie should only be transmitted over HTTPS.
)")
        .def("http_only", &atom::extra::curl::Cookie::http_only,
             R"(Check if the cookie should only be accessible through HTTP(S).

Returns:
    True if the cookie should only be accessible through HTTP(S).
)")
        .def("expires", &atom::extra::curl::Cookie::expires,
             R"(Get the expiration time of the cookie.

Returns:
    Optional expiration time for the cookie, or None for session cookie.
)")
        .def("is_expired", &atom::extra::curl::Cookie::is_expired,
             R"(Check if the cookie has expired.

Returns:
    True if the cookie has expired, False otherwise.
)");

    // CookieJar class binding
    py::class_<atom::extra::curl::CookieJar>(
        m, "CookieJar",
        R"(Manages a collection of HTTP cookies.

This class provides methods for storing, retrieving, and managing HTTP
cookies. It also supports loading and saving cookies to a file in the
Netscape cookie file format.

Examples:
    >>> jar = CookieJar()
    >>>
    >>> # Add cookies
    >>> jar.set_cookie(Cookie("session", "abc123"))
    >>> jar.set_cookie(Cookie("user", "john"))
    >>>
    >>> # Get a specific cookie
    >>> session_cookie = jar.get_cookie("session")
    >>>
    >>> # Get all cookies
    >>> all_cookies = jar.get_cookies()
    >>>
    >>> # Save to file
    >>> jar.save_to_file("cookies.txt")
    >>>
    >>> # Load from file
    >>> jar.load_from_file("cookies.txt")
)")
        .def(py::init<>(), "Create an empty cookie jar")
        .def("set_cookie", &atom::extra::curl::CookieJar::set_cookie,
             py::arg("cookie"),
             R"(Set a cookie in the cookie jar.

If a cookie with the same name already exists, it will be overwritten.

Args:
    cookie: The cookie to set.
)")
        .def("get_cookie", &atom::extra::curl::CookieJar::get_cookie,
             py::arg("name"),
             R"(Get a cookie from the cookie jar by name.

Args:
    name: The name of the cookie to get.

Returns:
    Optional Cookie object if found and not expired, None otherwise.
)")
        .def("get_cookies", &atom::extra::curl::CookieJar::get_cookies,
             R"(Get all cookies from the cookie jar that have not expired.

Returns:
    List of Cookie objects.
)")
        .def("clear", &atom::extra::curl::CookieJar::clear,
             R"(Clear all cookies from the cookie jar.)")
        .def("load_from_file", &atom::extra::curl::CookieJar::load_from_file,
             py::arg("filename"),
             R"(Load cookies from a file in the Netscape cookie file format.

Args:
    filename: The name of the file to load cookies from.

Returns:
    True if the cookies were successfully loaded, False otherwise.
)")
        .def("save_to_file", &atom::extra::curl::CookieJar::save_to_file,
             py::arg("filename"),
             R"(Save cookies to a file in the Netscape cookie file format.

Args:
    filename: The name of the file to save cookies to.

Returns:
    True if the cookies were successfully saved, False otherwise.
)")
        .def("parse_cookies_from_headers",
             &atom::extra::curl::CookieJar::parse_cookies_from_headers,
             py::arg("headers"), py::arg("domain"),
             R"(Parse cookies from HTTP headers and add them to the cookie jar.

Args:
    headers: Dictionary of HTTP header names to header values.
    domain: The domain to use for cookies that do not specify a domain.
)");
}
