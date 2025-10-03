#include "atom/async/future.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for AwaitableEnhancedFuture classes
template <typename T>
void declare_awaitable_enhanced_future(py::module& m, const std::string& type_name);
void declare_awaitable_enhanced_future_void(py::module& m);

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
        .def(
            "wait_for",
            [](EnhancedFutureT& self, int timeout_ms) {
                return self.waitFor(std::chrono::milliseconds(timeout_ms));
            },
            py::arg("timeout"),
            R"pbdoc(
            Waits for the future with a timeout.

            Args:
                timeout: The timeout duration in milliseconds

            Returns:
                Optional value: The value if ready, or None if timed out

            Examples:
                >>> future = makeEnhancedFuture(lambda: 42)
                >>> result = future.wait_for(5000)  # Wait up to 5 seconds
                >>> if result is not None:
                ...     print(f"Result: {result}")
                ... else:
                ...     print("Timed out")
            )pbdoc")
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

// AwaitableEnhancedFuture template for different return types
template <typename T>
void declare_awaitable_enhanced_future(py::module& m, const std::string& type_name) {
    using namespace atom::async;
    using AwaitableEnhancedFutureT = AwaitableEnhancedFuture<T>;

    std::string class_name = "AwaitableEnhancedFuture" + type_name;

    py::class_<AwaitableEnhancedFutureT>(m, class_name.c_str(),
        R"pbdoc(
        Coroutine-compatible awaitable wrapper for EnhancedFuture.

        This class provides C++20 coroutine support for EnhancedFuture objects,
        allowing them to be used with async/await syntax in compatible environments.
        It implements the awaitable protocol for efficient coroutine integration.

        Note: This class is primarily for advanced use cases and coroutine integration.
        For most Python use cases, use EnhancedFuture directly.
        )pbdoc")
        .def(py::init<std::shared_future<T>>(), py::arg("future"),
             R"pbdoc(
             Constructs an AwaitableEnhancedFuture from a shared_future.

             Args:
                 future: The shared_future to wrap for coroutine support.
             )pbdoc")
        .def("await_ready", &AwaitableEnhancedFutureT::await_ready,
             R"pbdoc(
             Checks if the future is ready without blocking.

             Returns:
                 bool: True if the future is ready, False otherwise.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc")
        .def("await_resume", &AwaitableEnhancedFutureT::await_resume,
             R"pbdoc(
             Resumes execution and returns the result.

             Returns:
                 The result of the future operation.

             Raises:
                 Exception: Any exception that occurred during execution.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc");
}

// AwaitableEnhancedFuture void specialization
void declare_awaitable_enhanced_future_void(py::module& m) {
    using namespace atom::async;
    using AwaitableEnhancedFutureVoid = AwaitableEnhancedFuture<void>;

    py::class_<AwaitableEnhancedFutureVoid>(m, "AwaitableEnhancedFutureVoid",
        R"pbdoc(
        Coroutine-compatible awaitable wrapper for EnhancedFuture<void>.

        This class provides C++20 coroutine support for void EnhancedFuture objects,
        allowing them to be used with async/await syntax in compatible environments.
        It implements the awaitable protocol for efficient coroutine integration.

        Note: This class is primarily for advanced use cases and coroutine integration.
        For most Python use cases, use EnhancedFutureVoid directly.
        )pbdoc")
        .def(py::init<std::shared_future<void>>(), py::arg("future"),
             R"pbdoc(
             Constructs an AwaitableEnhancedFutureVoid from a shared_future<void>.

             Args:
                 future: The shared_future<void> to wrap for coroutine support.
             )pbdoc")
        .def("await_ready", &AwaitableEnhancedFutureVoid::await_ready,
             R"pbdoc(
             Checks if the future is ready without blocking.

             Returns:
                 bool: True if the future is ready, False otherwise.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc")
        .def("await_resume", &AwaitableEnhancedFutureVoid::await_resume,
             R"pbdoc(
             Resumes execution after the future completes.

             Raises:
                 Exception: Any exception that occurred during execution.

             Note: This is part of the coroutine awaitable protocol.
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
                    py::object result = func();
                    return result.cast<py::object>();
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
                    py::object result = callback();
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
        cancellation support, coroutine integration, and more.

        Key components:
          - EnhancedFuture: Extended future with additional functionality
          - AwaitableEnhancedFuture: Coroutine-compatible awaitable wrapper
          - makeEnhancedFuture: Factory function to create enhanced futures
          - makeOptimizedFuture: Platform-optimized future creation
          - co_makeEnhancedFuture: Coroutine-based factory functions
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
            >>> all_futures = whenAll([future1, future2, future3])
            >>> results = all_futures  # [10, 20, 20]
            >>>
            >>> # With timeout and callbacks
            >>> future = makeEnhancedFuture(lambda: compute_something())
            >>> future.on_complete(lambda x: print(f"Result: {x}"))
            >>> result = future.wait_for(5000)  # 5 seconds timeout
            >>>
            >>> # Coroutine support
            >>> awaitable = AwaitableEnhancedFutureInt(future1.get_shared_future())
            >>> # Use with coroutine frameworks
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::InvalidFutureException>(
        m, "InvalidFutureException");

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::InvalidFutureException& e) {
            throw py::value_error(e.what());
        } catch (const std::future_error& e) {
            throw std::runtime_error(e.what());
        } catch (const std::invalid_argument& e) {
            throw py::value_error(e.what());
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
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

    // Declare AwaitableEnhancedFuture for different types (coroutine support)
    declare_awaitable_enhanced_future<int>(m, "Int");
    declare_awaitable_enhanced_future<float>(m, "Float");
    declare_awaitable_enhanced_future<double>(m, "Double");
    declare_awaitable_enhanced_future<std::string>(m, "String");
    declare_awaitable_enhanced_future<bool>(m, "Bool");
    declare_awaitable_enhanced_future<py::object>(m, "Object");
    declare_awaitable_enhanced_future_void(m);

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

    // makeOptimizedFuture factory function (platform-optimized)
    m.def(
        "makeOptimizedFuture",
        [](py::function func) {
            return atom::async::makeOptimizedFuture([func]() -> py::object {
                py::gil_scoped_acquire acquire;
                py::object result = func();
                return result.is_none() ? py::none() : result;
            });
        },
        py::arg("func"),
        R"pbdoc(
    Creates a platform-optimized EnhancedFuture from a function.

    This function uses platform-specific optimizations (ASIO thread pool on
    supported platforms, macOS Grand Central Dispatch, etc.) for better
    performance compared to the standard makeEnhancedFuture.

    Args:
        func: The function to execute asynchronously

    Returns:
        An EnhancedFuture for the result of the function

    Examples:
        >>> future = makeOptimizedFuture(lambda: expensive_computation())
        >>> result = future.get()
    )pbdoc");

    // Coroutine-based factory functions
    m.def(
        "co_makeEnhancedFuture",
        [](py::object value) {
            // For Python bindings, we simulate coroutine behavior
            // by creating a ready future with the given value
            auto promise = std::promise<py::object>();
            promise.set_value(value);
            return atom::async::EnhancedFuture<py::object>(promise.get_future().share());
        },
        py::arg("value"),
        R"pbdoc(
    Creates an EnhancedFuture using coroutine-style syntax with a ready value.

    This function creates a future that is immediately ready with the given value,
    simulating coroutine behavior for Python integration.

    Args:
        value: The value to set the future to

    Returns:
        An EnhancedFuture that is immediately ready with the value

    Examples:
        >>> future = co_makeEnhancedFuture(42)
        >>> print(future.is_ready())  # True
        >>> print(future.get())       # 42
    )pbdoc");

    m.def(
        "co_makeEnhancedFutureVoid",
        []() {
            // Create a ready void future
            auto promise = std::promise<void>();
            promise.set_value();
            return atom::async::EnhancedFuture<void>(promise.get_future().share());
        },
        R"pbdoc(
    Creates an EnhancedFuture<void> using coroutine-style syntax.

    This function creates a void future that is immediately ready,
    simulating coroutine behavior for Python integration.

    Returns:
        An EnhancedFuture<void> that is immediately ready

    Examples:
        >>> future = co_makeEnhancedFutureVoid()
        >>> print(future.is_ready())  # True
        >>> future.get()  # Returns immediately
    )pbdoc");

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

    // Enhanced parallelProcess function
    m.def(
        "parallelProcess",
        [](py::list items, py::function func, size_t num_tasks) {
            std::vector<py::object> items_vec;
            for (auto item : items) {
                items_vec.push_back(item.cast<py::object>());
            }

            if (num_tasks == 0) {
                // Use platform-specific detection like the C++ version
                num_tasks = std::max(
                    size_t(1),
                    static_cast<size_t>(std::thread::hardware_concurrency()));
                if (num_tasks == 0) {
                    num_tasks = 2;  // Fallback
                }
            }

            std::vector<atom::async::EnhancedFuture<py::object>> futures;
            size_t total_size = items_vec.size();

            if (total_size == 0) {
                return futures;
            }

            size_t items_per_task = (total_size + num_tasks - 1) / num_tasks;

            for (size_t i = 0; i < num_tasks && i * items_per_task < total_size; ++i) {
                size_t start_idx = i * items_per_task;
                size_t end_idx = std::min(start_idx + items_per_task, total_size);

                std::vector<py::object> chunk(items_vec.begin() + start_idx,
                                              items_vec.begin() + end_idx);

                if (chunk.empty()) {
                    continue;
                }

                futures.push_back(atom::async::makeOptimizedFuture(
                    [func, chunk = std::move(chunk)]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        py::list results;
                        for (const auto& item : chunk) {
                            py::object result = func(item);
                            results.append(result);
                        }
                        return results;
                    }));
            }

            return futures;
        },
        py::arg("items"), py::arg("func"), py::arg("num_tasks") = 0,
        R"pbdoc(
    Processes items in parallel using multiple threads with platform optimizations.

    This function divides the input items into chunks and processes them in parallel
    using platform-optimized futures. It automatically determines the optimal number
    of tasks based on hardware concurrency if not specified.

    Args:
        items: List of items to process
        func: Function to apply to each item
        num_tasks: Number of parallel tasks to use (0 = auto-detect based on CPU cores)

    Returns:
        List of EnhancedFuture objects containing the results for each chunk

    Examples:
        >>> items = list(range(100))
        >>> futures = parallelProcess(items, lambda x: x * x, 4)
        >>> # Collect all results
        >>> all_results = []
        >>> for future in futures:
        ...     chunk_results = future.get()
        ...     all_results.extend(chunk_results)
        >>> print(len(all_results))  # 100
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

    // Additional utility functions for creating ready futures
    m.def(
        "make_ready_future",
        [](py::object value) {
            auto promise = std::promise<py::object>();
            promise.set_value(value);
            return atom::async::EnhancedFuture<py::object>(promise.get_future().share());
        },
        py::arg("value"),
        R"pbdoc(
        Creates an EnhancedFuture that is immediately ready with the given value.

        Args:
            value: The value to set the future to.

        Returns:
            EnhancedFuture: A future that is already completed with the value.

        Examples:
            >>> future = make_ready_future(42)
            >>> print(future.is_ready())  # True
            >>> print(future.get())       # 42
        )pbdoc");

    m.def(
        "make_ready_future_void",
        []() {
            auto promise = std::promise<void>();
            promise.set_value();
            return atom::async::EnhancedFuture<void>(promise.get_future().share());
        },
        R"pbdoc(
        Creates an EnhancedFuture<void> that is immediately ready.

        Returns:
            EnhancedFuture<void>: A future that is already completed.

        Examples:
            >>> future = make_ready_future_void()
            >>> print(future.is_ready())  # True
            >>> future.get()  # Returns immediately
        )pbdoc");

    m.def(
        "make_exceptional_future",
        [](py::object exception) {
            auto promise = std::promise<py::object>();
            try {
                throw py::cast<std::runtime_error>(exception);
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
            return atom::async::EnhancedFuture<py::object>(promise.get_future().share());
        },
        py::arg("exception"),
        R"pbdoc(
        Creates an EnhancedFuture that is immediately ready with an exception.

        Args:
            exception: The exception to set the future to.

        Returns:
            EnhancedFuture: A future that will throw the exception when accessed.

        Examples:
            >>> future = make_exceptional_future(RuntimeError("Something went wrong"))
            >>> try:
            ...     future.get()
            ... except RuntimeError as e:
            ...     print(f"Caught: {e}")
        )pbdoc");

    // Add version information
    m.attr("__version__") = "1.0.0";
}
