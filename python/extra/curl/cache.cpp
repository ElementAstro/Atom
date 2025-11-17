#include "atom/extra/curl/cache.hpp"
#include "atom/extra/curl/response.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(cache, m) {
    m.doc() = R"(HTTP response caching module for the atom package.

This module provides a simple caching mechanism for HTTP responses,
allowing you to store and retrieve responses based on their URL.
It supports expiration and validation headers for efficient caching.

Examples:
    >>> from atom.extra.curl import cache
    >>>
    >>> # Create a cache with 5 minute default TTL
    >>> c = cache.Cache()
    >>>
    >>> # Create a cache with custom TTL
    >>> import datetime
    >>> c = cache.Cache(datetime.timedelta(minutes=10))
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

    // CacheEntry struct
    py::class_<atom::extra::curl::Cache::CacheEntry>(
        m, "CacheEntry",
        R"(Structure representing a cache entry.

This structure holds the cached response, its expiration time,
ETag, and Last-Modified header for validation.)")
        .def(py::init<>(), "Create an empty cache entry")
        .def_readwrite("response",
                       &atom::extra::curl::Cache::CacheEntry::response,
                       "The cached HTTP response")
        .def_readwrite("expires",
                       &atom::extra::curl::Cache::CacheEntry::expires,
                       "The expiration time of the cache entry")
        .def_readwrite("etag", &atom::extra::curl::Cache::CacheEntry::etag,
                       "The ETag header associated with the response")
        .def_readwrite("last_modified",
                       &atom::extra::curl::Cache::CacheEntry::last_modified,
                       "The Last-Modified header associated with the response");

    // Cache class binding
    py::class_<atom::extra::curl::Cache>(m, "Cache",
                                         R"(Class for caching HTTP responses.

This class provides a simple caching mechanism for HTTP responses,
allowing you to store and retrieve responses based on their URL.
It supports expiration and validation headers for efficient caching.

Examples:
    >>> cache = Cache()
    >>>
    >>> # Set a cache entry
    >>> cache.set("https://example.com", response)
    >>>
    >>> # Get a cached response
    >>> cached_response = cache.get("https://example.com")
    >>> if cached_response:
    ...     print("Cache hit!")
    >>>
    >>> # Invalidate a cache entry
    >>> cache.invalidate("https://example.com")
    >>>
    >>> # Clear all cache
    >>> cache.clear()
)")
        .def(py::init<std::chrono::seconds>(),
             py::arg("default_ttl") = std::chrono::minutes(5),
             R"(Constructor for the Cache class.

Args:
    default_ttl: The default time-to-live for cache entries, in seconds.
                 Defaults to 5 minutes.
)")
        .def("set", &atom::extra::curl::Cache::set, py::arg("url"),
             py::arg("response"), py::arg("ttl") = std::nullopt,
             R"(Set a cache entry for the given URL.

Args:
    url: The URL to cache the response for.
    response: The HTTP response to cache.
    ttl: Optional time-to-live for the cache entry, in seconds.
         If not provided, the default TTL is used.
)")
        .def("get", &atom::extra::curl::Cache::get, py::arg("url"),
             R"(Retrieve a cached response for the given URL.

Args:
    url: The URL to retrieve the cached response for.

Returns:
    Optional Response object if a valid cache entry exists, None otherwise.
)")
        .def("invalidate", &atom::extra::curl::Cache::invalidate,
             py::arg("url"),
             R"(Invalidate the cache entry for the given URL.

Args:
    url: The URL to invalidate the cache entry for.
)")
        .def("clear", &atom::extra::curl::Cache::clear,
             R"(Clear the entire cache.)")
        .def("get_validation_headers",
             &atom::extra::curl::Cache::get_validation_headers, py::arg("url"),
             R"(Get the validation headers for the given URL.

These headers can be used to perform conditional requests to
validate the cached response with the server.

Args:
    url: The URL to get the validation headers for.

Returns:
    Dictionary of header names to header values.
)")
        .def("handle_not_modified",
             &atom::extra::curl::Cache::handle_not_modified, py::arg("url"),
             R"(Handle a "Not Modified" response from the server.

This method updates the expiration time of the cache entry
when the server returns a "304 Not Modified" response,
indicating that the cached response is still valid.

Args:
    url: The URL that received the "Not Modified" response.
)");
}
