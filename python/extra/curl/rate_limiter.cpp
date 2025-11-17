#include "atom/extra/curl/rate_limiter.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(rate_limiter, m) {
    m.doc() = R"(Rate limiting module for HTTP requests.

This module provides a mechanism to control the rate at which requests are
made, ensuring that the number of requests per second does not exceed a
specified limit.

Examples:
    >>> from atom.extra.curl import rate_limiter
    >>>
    >>> # Create a rate limiter allowing 10 requests per second
    >>> limiter = rate_limiter.RateLimiter(10.0)
    >>>
    >>> # Wait before making a request
    >>> limiter.wait()
    >>> # Make your request here
    >>>
    >>> # Update the rate limit
    >>> limiter.set_rate(5.0)  # Now allow only 5 requests per second
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

    // RateLimiter class binding
    py::class_<atom::extra::curl::RateLimiter>(
        m, "RateLimiter",
        R"(Class for limiting the rate of requests.

This class provides a mechanism to control the rate at which requests are
made, ensuring that the number of requests per second does not exceed a
specified limit. It uses a mutex to ensure thread safety.

Examples:
    >>> # Create a rate limiter
    >>> limiter = RateLimiter(10.0)  # 10 requests per second
    >>>
    >>> # Use in a loop
    >>> for i in range(100):
    ...     limiter.wait()
    ...     # Make your request here
    ...     response = session.get(f"https://api.example.com/item/{i}")
)")
        .def(py::init<double>(), py::arg("requests_per_second"),
             R"(Constructor for the RateLimiter class.

Args:
    requests_per_second: The maximum number of requests allowed per second.

Raises:
    ValueError: If requests_per_second is not positive.
)")
        .def("wait", &atom::extra::curl::RateLimiter::wait,
             R"(Wait to ensure that the rate limit is not exceeded.

This method blocks the current thread until the rate limit allows
another request to be made.
)")
        .def("set_rate", &atom::extra::curl::RateLimiter::set_rate,
             py::arg("requests_per_second"),
             R"(Set a new rate limit.

Args:
    requests_per_second: The new maximum number of requests allowed per second.

Raises:
    ValueError: If requests_per_second is not positive.
)");
}
