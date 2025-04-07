#include "atom/async/promise.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(promise, m) {
    m.doc() =
        "Promise implementation module for asynchronous operations in the atom "
        "package";

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

    // Promise class binding
    py::class_<atom::async::Promise<py::object>>(
        m, "Promise",
        R"(A Promise represents a value that may be available in the future.

This class provides methods to handle asynchronous operations with callback-based
resolution and rejection mechanisms similar to JavaScript Promises.

Examples:
    >>> from atom.async import Promise
    >>> def async_task():
    ...     p = Promise()
    ...     # Simulate async operation
    ...     import threading
    ...     def resolver():
    ...         import time
    ...         time.sleep(1)
    ...         p.resolve("Done!")
    ...     threading.Thread(target=resolver).start()
    ...     return p
    >>> promise = async_task()
    >>> result = promise.wait()  # Blocks until resolved
    >>> print(result)
    Done!
)")
        .def(py::init<>(), "Creates a new pending Promise.")
        .def("resolve", &atom::async::Promise<py::object>::resolve,
             py::arg("value"),
             R"(Resolves the promise with the given value.

Args:
    value: The value to resolve the promise with.

Raises:
    RuntimeError: If the promise is already settled (resolved or rejected).
)")
        .def("reject", &atom::async::Promise<py::object>::reject,
             py::arg("reason"),
             R"(Rejects the promise with the given reason.

Args:
    reason: The reason for rejection, typically an exception.

Raises:
    RuntimeError: If the promise is already settled (resolved or rejected).
)")
        .def("is_pending", &atom::async::Promise<py::object>::isPending,
             R"(Checks if the promise is still pending.

Returns:
    bool: True if the promise has not been resolved or rejected yet.
)")
        .def("is_fulfilled", &atom::async::Promise<py::object>::isFulfilled,
             R"(Checks if the promise has been resolved.

Returns:
    bool: True if the promise has been resolved.
)")
        .def("is_rejected", &atom::async::Promise<py::object>::isRejected,
             R"(Checks if the promise has been rejected.

Returns:
    bool: True if the promise has been rejected.
)")
        .def("wait", &atom::async::Promise<py::object>::wait,
             py::arg("timeout_ms") = 0,
             R"(Waits for the promise to be settled.

Args:
    timeout_ms: Maximum time to wait in milliseconds. 0 means wait indefinitely.

Returns:
    The resolved value if the promise is fulfilled.

Raises:
    Exception: The rejection reason if the promise is rejected.
    TimeoutError: If the timeout is reached before the promise settles.
)")
        .def(
            "then",
            [](atom::async::Promise<py::object>& self,
               py::function on_fulfilled, py::function on_rejected) {
                return self.then(on_fulfilled, on_rejected);
            },
            py::arg("on_fulfilled"), py::arg("on_rejected") = py::none(),
            R"(Attaches callbacks for the resolution and/or rejection of the Promise.

Args:
    on_fulfilled: The callback to execute when the Promise is resolved.
    on_rejected: Optional callback to execute when the Promise is rejected.

Returns:
    A new Promise that is resolved/rejected with the return value of the called handler.

Examples:
    >>> promise.then(lambda value: print(f"Success: {value}"), 
    ...              lambda reason: print(f"Failed: {reason}"))
)")
        .def(
            "catch",
            [](atom::async::Promise<py::object>& self,
               py::function on_rejected) {
                return self.then(py::none(), on_rejected);
            },
            py::arg("on_rejected"),
            R"(Attaches a callback for only the rejection of the Promise.

Args:
    on_rejected: The callback to execute when the Promise is rejected.

Returns:
    A new Promise that is resolved/rejected with the return value of the called handler.

Examples:
    >>> promise.catch(lambda reason: print(f"Failed: {reason}"))
)");

    // Static Promise methods
    m.def("resolve", &atom::async::Promise<py::object>::Resolve,
          py::arg("value"),
          R"(Creates a Promise that is resolved with the given value.

Args:
    value: The value to resolve the promise with.

Returns:
    A new Promise that is already resolved with the given value.

Examples:
    >>> from atom.async import resolve
    >>> promise = resolve("immediate value")
    >>> promise.is_fulfilled()
    True
)");

    m.def("reject", &atom::async::Promise<py::object>::Reject,
          py::arg("reason"),
          R"(Creates a Promise that is rejected with the given reason.

Args:
    reason: The reason for rejection.

Returns:
    A new Promise that is already rejected with the given reason.

Examples:
    >>> from atom.async import reject
    >>> promise = reject(ValueError("Invalid input"))
    >>> promise.is_rejected()
    True
)");

    m.def(
        "all",
        [](const std::vector<atom::async::Promise<py::object>>& promises) {
            return atom::async::Promise<py::object>::All(promises);
        },
        py::arg("promises"),
        R"(Returns a promise that resolves when all the promises in the iterable have resolved.

Args:
    promises: An iterable of promises.

Returns:
    A promise that fulfills with a list of all the resolved values when all promises are resolved,
    or rejects with the reason of the first promise that rejects.

Examples:
    >>> from atom.async import all, resolve
    >>> promise1 = resolve("one")
    >>> promise2 = resolve("two")
    >>> all_promise = all([promise1, promise2])
    >>> all_promise.wait()
    ['one', 'two']
)");

    m.def(
        "race",
        [](const std::vector<atom::async::Promise<py::object>>& promises) {
            return atom::async::Promise<py::object>::Race(promises);
        },
        py::arg("promises"),
        R"(Returns a promise that resolves or rejects as soon as one of the promises resolves or rejects.

Args:
    promises: An iterable of promises.

Returns:
    A promise that adopts the state of the first promise to settle.

Examples:
    >>> import time
    >>> from atom.async import race, Promise
    >>> p1 = Promise()
    >>> p2 = Promise()
    >>> race_promise = race([p1, p2])
    >>> # p2 will resolve first
    >>> def resolve_p1():
    ...     time.sleep(2)
    ...     p1.resolve("p1 done")
    >>> def resolve_p2():
    ...     time.sleep(1)
    ...     p2.resolve("p2 done")
    >>> import threading
    >>> threading.Thread(target=resolve_p1).start()
    >>> threading.Thread(target=resolve_p2).start()
    >>> race_promise.wait()
    'p2 done'
)");
}