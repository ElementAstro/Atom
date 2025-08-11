#include "atom/async/future.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

// Template for declaring EnhancedFuture with different return types
template <typename T>
void declare_enhanced_future(py::module& m, const std::string& type_name) {
    using namespace atom::async;
    using EnhancedFutureT = EnhancedFuture<T>;

    std::string class_name = "EnhancedFuture" + type_name;

    py::class_<EnhancedFutureT>(m, class_name.c_str(),
                                R"pbdoc(
        Enhanced future class with additional functionality beyond standard futures.

        This class extends std::future with features like chaining operations,
        callbacks, timeouts, cancellation, and more.

        Args:
            future: A shared_future to wrap (typically created by makeEnhancedFuture)

        Examples:
            >>> from atom.async.future import makeEnhancedFuture
            >>>
            >>> # Create an enhanced future
            >>> future = makeEnhancedFuture(lambda: 42)
            >>>
            >>> # Chain operations
            >>> result_future = future.then(lambda x: x * 2)
            >>>
            >>> # Add completion callback
            >>> future.on_complete(lambda x: print(f"Result: {x}"))
            >>>
            >>> # Get result with timeout
            >>> result = future.wait_for(5000)  # 5 seconds timeout
        )pbdoc")

        // Wait and get methods
        .def("is_done", &EnhancedFutureT::isDone,
             "Checks if the future is done")
        .def("wait", &EnhancedFutureT::wait,
             R"pbdoc(
             Waits synchronously for the future to complete.

             Returns:
                 The value of the future.

             Raises:
                 RuntimeError: If the future is cancelled or throws an exception.
             )pbdoc")
        /*
        .def("wait_for",
             &EnhancedFutureT::waitFor,
             [](EnhancedFutureT& self, int timeout_ms) {
                 return self.waitFor(std::chrono::milliseconds(timeout_ms));
             },
             py::arg("timeout"),
             R"pbdoc(
             Waits for the future with a timeout and auto-cancels if not ready.

             Args:
                 timeout: The timeout duration in milliseconds

             Returns:
                 The value if ready, or None if timed out
             )pbdoc")
        */
        .def("is_ready", &EnhancedFutureT::isReady,
             "Checks if the future is ready")
        .def("get", &EnhancedFutureT::get,
             R"pbdoc(
             Gets the result of the future.

             Returns:
                 The value of the future.

             Raises:
                 RuntimeError: If the future is cancelled or throws an exception.
             )pbdoc")

        // Cancellation
        .def("cancel", &EnhancedFutureT::cancel,
             "Cancels the future, preventing further processing")
        .def("is_cancelled", &EnhancedFutureT::isCancelled,
             "Checks if the future has been cancelled")

        // Exception handling
        .def("get_exception", &EnhancedFutureT::getException,
             "Gets the exception associated with the future, if any")

        // Continuation methods
        .def(
            "then",
            [](EnhancedFutureT& self, py::function func) {
                return self.then([func](const T& value) {
                    py::gil_scoped_acquire acquire;
                    return func(value).template cast<py::object>();
                });
            },
            py::arg("func"),
            R"pbdoc(
        Chains another operation to be called after the future is done.

        Args:
            func: The function to call when the future is done

        Returns:
            A new EnhancedFuture for the result of the function

        Examples:
            >>> future = makeEnhancedFuture(lambda: 10)
            >>> future2 = future.then(lambda x: x * 2)
            >>> result = future2.get()  # Will be 20
        )pbdoc")

        .def(
            "catching",
            [](EnhancedFutureT& self, py::function func) {
                return self.catching([func](std::exception_ptr eptr) {
                    py::gil_scoped_acquire acquire;
                    try {
                        if (eptr)
                            std::rethrow_exception(eptr);
                        return py::none().cast<T>();
                    } catch (const std::exception& e) {
                        return func(e.what()).cast<T>();
                    }
                });
            },
            py::arg("func"),
            R"pbdoc(
        Provides exception handling for the future.

        Args:
            func: The function to call when an exception occurs

        Returns:
            A new EnhancedFuture that will handle exceptions

        Examples:
            >>> def might_fail():
            >>>     raise ValueError("Something went wrong")
            >>>
            >>> future = makeEnhancedFuture(might_fail)
            >>> safe_future = future.catching(lambda err: f"Error: {err}")
            >>> result = safe_future.get()  # Will be "Error: Something went wrong"
        )pbdoc")

        .def(
            "retry",
            [](EnhancedFutureT& self, py::function func, int max_retries,
               std::optional<int> backoff_ms = std::nullopt) {
                return self.retry(
                    [func](const T& value) {
                        py::gil_scoped_acquire acquire;
                        return func(value).template cast<py::object>();
                    },
                    max_retries, backoff_ms);
            },
            py::arg("func"), py::arg("max_retries"),
            py::arg("backoff_ms") = py::none(),
            R"pbdoc(
        Retries the operation associated with the future.

        Args:
            func: The function to call when retrying
            max_retries: The maximum number of retries
            backoff_ms: Optional backoff time between retries in milliseconds

        Returns:
            A new EnhancedFuture for the retry operation

        Examples:
            >>> future = makeEnhancedFuture(lambda: 10)
            >>> retry_future = future.retry(lambda x: x * 2, 3, 100)
        )pbdoc")

        .def(
            "on_complete",
            [](EnhancedFutureT& self, py::function callback) {
                self.onComplete([callback](const T& value) {
                    py::gil_scoped_acquire acquire;
                    callback(value);
                });
            },
            py::arg("callback"),
            R"pbdoc(
        Sets a completion callback to be called when the future is done.

        Args:
            callback: The callback function to add

        Examples:
            >>> future = makeEnhancedFuture(lambda: 42)
            >>> future.on_complete(lambda x: print(f"Result: {x}"))
        )pbdoc");
}

// Void specialization
void declare_enhanced_future_void(py::module& m) {
    using namespace atom::async;
    using EnhancedFutureVoid = EnhancedFuture<void>;

    py::class_<EnhancedFutureVoid>(m, "EnhancedFutureVoid",
                                   R"pbdoc(
        Enhanced future class for void operations.

        This class extends std::future<void> with features like chaining operations,
        callbacks, timeouts, cancellation, and more.

        Args:
            future: A shared_future to wrap (typically created by makeEnhancedFuture)

        Examples:
            >>> from atom.async.future import makeEnhancedFuture
            >>>
            >>> # Create a void enhanced future
            >>> future = makeEnhancedFuture(lambda: None)
            >>>
            >>> # Chain operations
            >>> result_future = future.then(lambda: "Operation completed")
            >>>
            >>> # Add completion callback
            >>> future.on_complete(lambda: print("Done!"))
        )pbdoc")

        // Wait and get methods
        .def("is_done", &EnhancedFutureVoid::isDone,
             "Checks if the future is done")
        .def("wait", &EnhancedFutureVoid::wait,
             R"pbdoc(
             Waits synchronously for the future to complete.

             Raises:
                 RuntimeError: If the future is cancelled or throws an exception.
             )pbdoc")
        .def("wait_for",
             static_cast<bool (EnhancedFutureVoid::*)(
                 std::chrono::milliseconds)>(&EnhancedFutureVoid::waitFor),
             py::arg("timeout"),
             R"pbdoc(
             Waits for the future with a timeout and auto-cancels if not ready.

             Args:
                 timeout: The timeout duration in milliseconds

             Returns:
                 True if completed successfully, False if timed out
             )pbdoc")
        .def("is_ready", &EnhancedFutureVoid::isReady,
             "Checks if the future is ready")
        .def("get", &EnhancedFutureVoid::get,
             R"pbdoc(
             Waits for the future to complete.

             Raises:
                 RuntimeError: If the future is cancelled or throws an exception.
             )pbdoc")

        // Cancellation
        .def("cancel", &EnhancedFutureVoid::cancel,
             "Cancels the future, preventing further processing")
        .def("is_cancelled", &EnhancedFutureVoid::isCancelled,
             "Checks if the future has been cancelled")

        // Exception handling
        .def("get_exception", &EnhancedFutureVoid::getException,
             "Gets the exception associated with the future, if any")

        // Continuation methods
        .def(
            "then",
            [](EnhancedFutureVoid& self, py::function func) {
                return self.then([func]() {
                    py::gil_scoped_acquire acquire;
                    return func().cast<py::object>();
                });
            },
            py::arg("func"),
            R"pbdoc(
        Chains another operation to be called after the future is done.

        Args:
            func: The function to call when the future is done

        Returns:
            A new EnhancedFuture for the result of the function

        Examples:
            >>> future = makeEnhancedFuture(lambda: None)
            >>> future2 = future.then(lambda: "Done!")
            >>> result = future2.get()  # Will be "Done!"
        )pbdoc")

        .def(
            "on_complete",
            [](EnhancedFutureVoid& self, py::function callback) {
                self.onComplete([callback]() {
                    py::gil_scoped_acquire acquire;
                    callback();
                });
            },
            py::arg("callback"),
            R"pbdoc(
        Sets a completion callback to be called when the future is done.

        Args:
            callback: The callback function to add

        Examples:
            >>> future = makeEnhancedFuture(lambda: None)
            >>> future.on_complete(lambda: print("Task completed!"))
        )pbdoc");
}

PYBIND11_MODULE(future, m) {
    m.doc() = R"pbdoc(
        Enhanced Future and Async Processing Module
        -----------------------------------------

        This module provides enhanced future classes with additional functionality
        beyond standard futures, including chaining operations, callbacks, timeouts,
        cancellation support, and more.

        Key components:
          - EnhancedFuture: Extended future with additional functionality
          - makeEnhancedFuture: Factory function to create enhanced futures
          - whenAll: Synchronization for multiple futures
          - parallelProcess: Utility for parallel data processing

        Example:
            >>> from atom.async.future import makeEnhancedFuture, whenAll
            >>>
            >>> # Create enhanced futures
            >>> future1 = makeEnhancedFuture(lambda: 10)
            >>> future2 = makeEnhancedFuture(lambda: 20)
            >>>
            >>> # Chain operations
            >>> future3 = future1.then(lambda x: x * 2)
            >>>
            >>> # Synchronize multiple futures
            >>> all_futures = whenAll(future1, future2, future3)
            >>> results = all_futures.get()  # [10, 20, 20]
            >>>
            >>> # With timeout and callbacks
            >>> future = makeEnhancedFuture(lambda: compute_something())
            >>> future.on_complete(lambda x: print(f"Result: {x}"))
            >>> result = future.wait_for(5000)  # 5 seconds timeout
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::InvalidFutureException>(
        m, "InvalidFutureException");

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::InvalidFutureException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::future_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Declare EnhancedFuture for different types
    declare_enhanced_future<int>(m, "Int");
    declare_enhanced_future<float>(m, "Float");
    declare_enhanced_future<double>(m, "Double");
    declare_enhanced_future<std::string>(m, "String");
    declare_enhanced_future<bool>(m, "Bool");
    declare_enhanced_future<py::object>(m, "Object");
    declare_enhanced_future_void(m);

    // makeEnhancedFuture factory function
    m.def(
        "makeEnhancedFuture",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> py::object {
                py::gil_scoped_acquire acquire;
                py::object result = func();
                return result.is_none() ? py::none() : result;
            });
        },
        py::arg("func"),
        R"pbdoc(
    Creates an EnhancedFuture from a function.

    Args:
        func: The function to execute asynchronously

    Returns:
        An EnhancedFuture for the result of the function

    Examples:
        >>> future = makeEnhancedFuture(lambda: 42)
        >>> result = future.get()  # 42
    )pbdoc");

    // Typed makeEnhancedFuture factory functions
    m.def(
        "makeEnhancedFutureInt",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> int {
                py::gil_scoped_acquire acquire;
                return func().cast<int>();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureInt from a function");

    m.def(
        "makeEnhancedFutureFloat",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> float {
                py::gil_scoped_acquire acquire;
                return func().cast<float>();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureFloat from a function");

    m.def(
        "makeEnhancedFutureDouble",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> double {
                py::gil_scoped_acquire acquire;
                return func().cast<double>();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureDouble from a function");

    m.def(
        "makeEnhancedFutureString",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> std::string {
                py::gil_scoped_acquire acquire;
                return func().cast<std::string>();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureString from a function");

    m.def(
        "makeEnhancedFutureBool",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> bool {
                py::gil_scoped_acquire acquire;
                return func().cast<bool>();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureBool from a function");

    m.def(
        "makeEnhancedFutureVoid",
        [](py::function func) {
            return atom::async::makeEnhancedFuture([func]() -> void {
                py::gil_scoped_acquire acquire;
                func();
            });
        },
        py::arg("func"), "Creates an EnhancedFutureVoid from a function");

    // whenAll functions
    m.def(
        "whenAll",
        [](std::vector<py::object> futures,
           std::optional<std::chrono::milliseconds> timeout) {
            // This is a simplified implementation for Python binding
            py::gil_scoped_release release;

            // Wait for all futures to complete
            try {
                for (auto& fut : futures) {
                    if (py::hasattr(fut, "wait_for") && timeout) {
                        fut.attr("wait_for")(timeout.value().count());
                    } else if (py::hasattr(fut, "wait")) {
                        fut.attr("wait")();
                    }
                }

                // Collect results
                std::vector<py::object> results;
                for (auto& fut : futures) {
                    if (py::hasattr(fut, "get")) {
                        results.push_back(fut.attr("get")());
                    } else {
                        results.push_back(py::none());
                    }
                }

                return results;
            } catch (const py::error_already_set& e) {
                py::gil_scoped_acquire acquire;
                throw;
            }
        },
        py::arg("futures"), py::arg("timeout") = py::none(),
        R"pbdoc(
    Waits for all futures to complete and returns their results.

    Args:
        futures: List of futures to wait for
        timeout: Optional timeout in milliseconds

    Returns:
        List of results from all futures

    Examples:
        >>> future1 = makeEnhancedFuture(lambda: 10)
        >>> future2 = makeEnhancedFuture(lambda: 20)
        >>> results = whenAll([future1, future2])  # [10, 20]
    )pbdoc");

    // parallelProcess function
    m.def(
        "parallelProcess",
        [](py::list items, py::function func, size_t chunk_size) {
            std::vector<py::object> items_vec;
            for (auto item : items) {
                items_vec.push_back(item.cast<py::object>());
            }

            if (chunk_size == 0) {
                chunk_size = std::max(
                    size_t(1),
                    static_cast<size_t>(std::thread::hardware_concurrency()));
            }

            std::vector<atom::async::EnhancedFuture<py::object>> futures;

            for (size_t i = 0; i < items_vec.size(); i += chunk_size) {
                size_t end_idx = std::min(i + chunk_size, items_vec.size());
                std::vector<py::object> chunk(items_vec.begin() + i,
                                              items_vec.begin() + end_idx);

                futures.push_back(atom::async::EnhancedFuture<py::object>(
                    std::async(std::launch::async,
                               [func, chunk]() -> py::object {
                                   py::gil_scoped_acquire acquire;
                                   py::list results;
                                   for (auto& item : chunk) {
                                       results.append(func(item));
                                   }
                                   return results;
                               })
                        .share()));
            }

            return futures;
        },
        py::arg("items"), py::arg("func"), py::arg("chunk_size") = 0,
        R"pbdoc(
    Processes items in parallel using multiple threads.

    Args:
        items: List of items to process
        func: Function to apply to each item
        chunk_size: Size of chunks to process together (0 = auto)

    Returns:
        List of futures containing the results

    Examples:
        >>> items = list(range(100))
        >>> futures = parallelProcess(items, lambda x: x * x)
        >>> results = [f.get() for f in futures]
    )pbdoc");

    // Utility functions
    m.def(
        "getWithTimeout",
        [](py::object future, double timeout_seconds) {
            std::chrono::duration<double> timeout(timeout_seconds);

            py::gil_scoped_release release;
            auto start_time = std::chrono::steady_clock::now();

            while (std::chrono::steady_clock::now() - start_time < timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                py::gil_scoped_acquire acquire;
                if (py::hasattr(future, "is_done") &&
                    future.attr("is_done")().cast<bool>()) {
                    return future.attr("get")();
                }
            }

            py::gil_scoped_acquire acquire;
            throw atom::async::InvalidFutureException(
                __FILE__, __LINE__, __func__,
                "Timeout occurred waiting for future");
        },
        py::arg("future"), py::arg("timeout"),
        R"pbdoc(
    Gets the result of a future with a timeout.

    Args:
        future: The future to get the result from
        timeout: The timeout in seconds

    Returns:
        The result of the future

    Raises:
        InvalidFutureException: If the timeout is reached
    )pbdoc");

    // Hardware concurrency info
    m.def(
        "hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        "Returns the number of concurrent threads supported by the "
        "implementation");

    // Add version information
    m.attr("__version__") = "1.0.0";
}
