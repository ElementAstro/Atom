#include "atom/async/packaged_task.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename ResultType>
auto create_wrapped_task(const py::function& func) {
    return [func]() -> ResultType {
        try {
            py::object result = func();
            if constexpr (!std::is_same_v<ResultType, void>) {
                return result.cast<ResultType>();
            }
        } catch (const py::error_already_set& e) {
            throw std::runtime_error(std::string("Python exception: ") +
                                     e.what());
        } catch (...) {
            throw std::runtime_error("Unknown error in Python callable");
        }
    };
}

PYBIND11_MODULE(packaged_task, m) {
    m.doc() = R"pbdoc(
        Enhanced Packaged Task Implementation Module
        ------------------------------------------

        This module provides high-performance packaged task implementations with
        advanced features for asynchronous task execution and management.

        Features:
          - Enhanced packaged tasks with cancellation support
          - Callback registration for task completion
          - ASIO integration for async I/O operations
          - Lock-free callback queues for high performance
          - Exception handling and error propagation
          - Future-based result retrieval

        The module includes:
          - PackagedTask: Enhanced packaged task for tasks with return values
          - VoidPackagedTask: Enhanced packaged task for void tasks
          - InvalidPackagedTaskException: Exception for invalid task operations
          - Utility functions for task creation and execution

        Example:
            >>> from atom.async.packaged_task import PackagedTask, VoidPackagedTask
            >>>
            >>> # Task with return value
            >>> task = PackagedTask(lambda: 42)
            >>> future = task.get_future()
            >>> task()  # Execute the task
            >>> result = future.result()  # Get result: 42
            >>>
            >>> # Void task
            >>> void_task = VoidPackagedTask(lambda: print("Hello"))
            >>> void_future = void_task.get_future()
            >>> void_task()  # Execute: prints "Hello"
            >>> void_future.result()  # Wait for completion
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::InvalidPackagedTaskException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register InvalidPackagedTaskException
    py::register_exception<atom::async::InvalidPackagedTaskException>(
        m, "InvalidPackagedTaskException", PyExc_RuntimeError);

    // PackagedTask class for non-void return type (using py::object for
    // flexibility)
    py::class_<atom::async::EnhancedPackagedTask<py::object>>(
        m, "PackagedTask",
        R"(Enhanced packaged task for executing deferred operations.

This class wraps a callable object and provides mechanisms to execute it
asynchronously, returning its result through a future.

Examples:
    >>> from atom.async import PackagedTask
    >>> task = PackagedTask(lambda: 42)
    >>> future = task.get_future()
    >>> task()  # Execute the task
    >>> result = future.result()
    >>> print(result)  # Output: 42
)")
        .def(py::init([](py::function func) {
                 auto wrapped_task = create_wrapped_task<py::object>(func);
                 return std::make_unique<
                     atom::async::EnhancedPackagedTask<py::object>>(
                     std::move(wrapped_task));
             }),
             py::arg("task"),
             "Creates a new PackagedTask with the given callable.")
        .def("__call__",
             &atom::async::EnhancedPackagedTask<py::object>::operator(),
             "Execute the packaged task.")
        .def(
            "get_future",
            [](atom::async::EnhancedPackagedTask<py::object>& self) {
                auto future = self.getEnhancedFuture();

                // Create Python Future object
                auto py_future =
                    py::module::import("concurrent.futures").attr("Future")();

                // Use a background thread to wait for the C++ future and set
                // the Python future
                std::thread([future = std::move(future), py_future]() mutable {
                    try {
                        py::object result = future.get();
                        py_future.attr("set_result")(result);
                    } catch (const std::exception& e) {
                        py_future.attr("set_exception")(py::str(e.what()));
                    }
                }).detach();

                return py_future;
            },
            R"(Get a future associated with this packaged task.

Returns:
    A Future object that can be used to retrieve the result when ready.
)")
        .def("cancel", &atom::async::EnhancedPackagedTask<py::object>::cancel,
             R"(Cancel the execution of the task.

Returns:
    True if the task was successfully cancelled, False if it was already running or finished.
)")
        .def("is_cancelled",
             &atom::async::EnhancedPackagedTask<py::object>::isCancelled,
             "Check if the task has been cancelled.")
        .def(
            "__bool__",
            [](const atom::async::EnhancedPackagedTask<py::object>& self) {
                return static_cast<bool>(self);
            },
            "Check if the packaged task is valid (not cancelled and function "
            "is callable).")
        .def(
            "on_complete",
            [](atom::async::EnhancedPackagedTask<py::object>& self,
               py::function callback) {
                // Store the callback in a way that can be called from C++
                auto stored_callback =
                    std::make_shared<py::function>(std::move(callback));
                self.onComplete([stored_callback](const py::object& result) {
                    py::gil_scoped_acquire acquire;
                    try {
                        (*stored_callback)(result);
                    } catch (const py::error_already_set& e) {
                        // Log Python exception but don't propagate
                        py::print("Callback error:", e.what());
                    }
                });
            },
            py::arg("callback"),
            R"pbdoc(
            Register a callback to be called when the task completes.

            Args:
                callback: Function to call with the task result when complete

            Examples:
                >>> task = PackagedTask(lambda: 42)
                >>> task.on_complete(lambda result: print(f"Result: {result}"))
                >>> task()  # Executes task and calls callback
            )pbdoc")

#ifdef ATOM_USE_ASIO
        .def(
            "set_asio_context",
            [](atom::async::EnhancedPackagedTask<py::object>& /*self*/,
               py::object /*context*/) {
                // Note: This would require proper ASIO context binding
                // For now, we'll provide a placeholder
                py::print(
                    "ASIO context setting not fully implemented in Python "
                    "bindings");
            },
            py::arg("context"),
            R"pbdoc(
            Set the ASIO context for async execution.

            Args:
                context: ASIO io_context for async operations

            Note:
                This feature requires ASIO support to be enabled
            )pbdoc")
        .def(
            "get_asio_context",
            [](const atom::async::EnhancedPackagedTask<py::object>& /*self*/)
                -> py::object {
                return py::none();  // Placeholder implementation
            },
            R"pbdoc(
            Get the current ASIO context.

            Returns:
                ASIO context or None if not set

            Note:
                This feature requires ASIO support to be enabled
            )pbdoc")
#endif
        ;

    // PackagedTask class for void return type
    py::class_<atom::async::EnhancedPackagedTask<void>>(
        m, "VoidPackagedTask",
        R"(Enhanced packaged task for executing deferred operations without return values.

This class wraps a callable object and provides mechanisms to execute it
asynchronously, signaling completion through a future.

Examples:
    >>> from atom.async import VoidPackagedTask
    >>> task = VoidPackagedTask(lambda: print("Task executed"))
    >>> future = task.get_future()
    >>> task()  # Execute the task
    Task executed
    >>> future.result()  # Waits for completion, returns None
)")
        .def(py::init([](py::function func) {
                 auto wrapped_task = [func]() {
                     try {
                         func();
                     } catch (const py::error_already_set& e) {
                         throw std::runtime_error(
                             std::string("Python exception: ") + e.what());
                     } catch (...) {
                         throw std::runtime_error(
                             "Unknown error in Python callable");
                     }
                 };
                 return std::make_unique<
                     atom::async::EnhancedPackagedTask<void>>(
                     std::move(wrapped_task));
             }),
             py::arg("task"),
             "Creates a new VoidPackagedTask with the given callable.")
        .def("__call__", &atom::async::EnhancedPackagedTask<void>::operator(),
             "Execute the packaged task.")
        .def(
            "get_future",
            [](atom::async::EnhancedPackagedTask<void>& self) {
                auto future = self.getEnhancedFuture();

                // Create Python Future object
                auto py_future =
                    py::module::import("concurrent.futures").attr("Future")();

                // Use a background thread to wait for the C++ future and set
                // the Python future
                std::thread([future = std::move(future), py_future]() mutable {
                    try {
                        future.get();  // Void future, just wait for completion
                        py_future.attr("set_result")(py::none());
                    } catch (const std::exception& e) {
                        py_future.attr("set_exception")(py::str(e.what()));
                    }
                }).detach();

                return py_future;
            },
            R"(Get a future associated with this packaged task.

Returns:
    A Future object that can be used to wait for task completion.
)")
        .def("cancel", &atom::async::EnhancedPackagedTask<void>::cancel,
             R"(Cancel the execution of the task.

Returns:
    True if the task was successfully cancelled, False if it was already running or finished.
)")
        .def("is_cancelled",
             &atom::async::EnhancedPackagedTask<void>::isCancelled,
             "Check if the task has been cancelled.")
        .def(
            "__bool__",
            [](const atom::async::EnhancedPackagedTask<void>& self) {
                return static_cast<bool>(self);
            },
            "Check if the packaged task is valid (not cancelled and function "
            "is callable).")
        .def(
            "on_complete",
            [](atom::async::EnhancedPackagedTask<void>& self,
               py::function callback) {
                // Store the callback in a way that can be called from C++
                auto stored_callback =
                    std::make_shared<py::function>(std::move(callback));
                self.onComplete([stored_callback]() {
                    py::gil_scoped_acquire acquire;
                    try {
                        // Call the Python function without arguments for void
                        // tasks
                        py::object result = (*stored_callback)();
                    } catch (const py::error_already_set& e) {
                        // Log Python exception but don't propagate
                        py::print("Callback error:", e.what());
                    }
                });
            },
            py::arg("callback"),
            R"pbdoc(
            Register a callback to be called when the task completes.

            Args:
                callback: Function to call when the task completes (no arguments)

            Examples:
                >>> task = VoidPackagedTask(lambda: print("Task done"))
                >>> task.on_complete(lambda: print("Callback executed"))
                >>> task()  # Executes task and calls callback
            )pbdoc")

#ifdef ATOM_USE_ASIO
        .def(
            "set_asio_context",
            [](atom::async::EnhancedPackagedTask<void>& /*self*/,
               py::object /*context*/) {
                // Note: This would require proper ASIO context binding
                // For now, we'll provide a placeholder
                py::print(
                    "ASIO context setting not fully implemented in Python "
                    "bindings");
            },
            py::arg("context"),
            R"pbdoc(
            Set the ASIO context for async execution.

            Args:
                context: ASIO io_context for async operations

            Note:
                This feature requires ASIO support to be enabled
            )pbdoc")
        .def(
            "get_asio_context",
            [](const atom::async::EnhancedPackagedTask<void>& /*self*/)
                -> py::object {
                return py::none();  // Placeholder implementation
            },
            R"pbdoc(
            Get the current ASIO context.

            Returns:
                ASIO context or None if not set

            Note:
                This feature requires ASIO support to be enabled
            )pbdoc")
#endif
        ;

    // Factory functions for creating packaged tasks
    m.def(
        "make_packaged_task",
        [](py::function func, bool returns_value = true) -> py::object {
            if (returns_value) {
                auto wrapped_task = create_wrapped_task<py::object>(func);
                auto task = std::make_unique<
                    atom::async::EnhancedPackagedTask<py::object>>(
                    std::move(wrapped_task));
                return py::cast(std::move(task));
            } else {
                auto wrapped_task = [func]() {
                    try {
                        func();
                    } catch (const py::error_already_set& e) {
                        throw std::runtime_error(
                            std::string("Python exception: ") + e.what());
                    } catch (...) {
                        throw std::runtime_error(
                            "Unknown error in Python callable");
                    }
                };
                auto task =
                    std::make_unique<atom::async::EnhancedPackagedTask<void>>(
                        std::move(wrapped_task));
                return py::cast(std::move(task));
            }
        },
        py::arg("task"), py::arg("returns_value") = true,
        R"(Create a packaged task from a callable.

Args:
    task: The callable to wrap in a packaged task.
    returns_value: If True, creates a PackagedTask that returns a value.
                   If False, creates a VoidPackagedTask for tasks without return values.

Returns:
    A PackagedTask or VoidPackagedTask instance.

Examples:
    >>> from atom.async import make_packaged_task
    >>> task1 = make_packaged_task(lambda: 42)
    >>> task2 = make_packaged_task(lambda: print("Hello"), returns_value=False)
)");

    // Additional utility functions
    m.def(
        "run_packaged_task",
        [](py::function func) {
            // Create a task
            auto wrapped_task = create_wrapped_task<py::object>(func);
            auto task =
                std::make_unique<atom::async::EnhancedPackagedTask<py::object>>(
                    std::move(wrapped_task));

            // Get the future
            auto future = task->getEnhancedFuture();

            // Create a Python future
            auto py_future =
                py::module::import("concurrent.futures").attr("Future")();

            // Run the task in a separate thread
            std::thread([task = std::move(task), future, py_future]() mutable {
                try {
                    (*task)();                         // Execute the task
                    py::object result = future.get();  // Get the result
                    py_future.attr("set_result")(result);
                } catch (const std::exception& e) {
                    py_future.attr("set_exception")(py::str(e.what()));
                }
            }).detach();

            return py_future;
        },
        py::arg("task"),
        R"(Run a callable as a packaged task and return its future.

This is a convenience function that creates a packaged task,
executes it in a background thread, and returns a future.

Args:
    task: The callable to execute.

Returns:
    A Future object that will contain the result when ready.

Examples:
    >>> from atom.async import run_packaged_task
    >>> future = run_packaged_task(lambda: 42)
    >>> result = future.result()
    >>> print(result)  # Output: 42
)");

    // Advanced utility functions
    m.def(
         "run_void_packaged_task",
         [](py::function func) {
             // Create a void task
             auto wrapped_task = [func]() {
                 try {
                     func();
                 } catch (const py::error_already_set& e) {
                     throw std::runtime_error(
                         std::string("Python exception: ") + e.what());
                 } catch (...) {
                     throw std::runtime_error(
                         "Unknown error in Python callable");
                 }
             };
             auto task =
                 std::make_unique<atom::async::EnhancedPackagedTask<void>>(
                     std::move(wrapped_task));

             // Get the future
             auto future = task->getEnhancedFuture();

             // Create a Python future
             auto py_future =
                 py::module::import("concurrent.futures").attr("Future")();

             // Run the task in a separate thread
             std::thread([task = std::move(task), future, py_future]() mutable {
                 try {
                     (*task)();     // Execute the task
                     future.get();  // Wait for completion
                     py_future.attr("set_result")(py::none());
                 } catch (const std::exception& e) {
                     py_future.attr("set_exception")(py::str(e.what()));
                 }
             }).detach();

             return py_future;
         },
         py::arg("task"),
         R"pbdoc(
        Run a void callable as a packaged task and return its future.

        This is a convenience function for void tasks that creates a packaged task,
        executes it in a background thread, and returns a future for completion.

        Args:
            task: The callable to execute (should not return a value)

        Returns:
            A Future object that will be set to None when the task completes

        Examples:
            >>> from atom.async.packaged_task import run_void_packaged_task
            >>> future = run_void_packaged_task(lambda: print("Hello World"))
            >>> future.result()  # Waits for completion, prints "Hello World"
        )pbdoc")

        .def(
            "create_task_chain",
            [](py::list tasks) -> py::object {
                if (tasks.empty()) {
                    throw std::invalid_argument("Task list cannot be empty");
                }

                // Create a chain of tasks that execute sequentially
                auto py_future =
                    py::module::import("concurrent.futures").attr("Future")();

                std::thread([tasks, py_future]() {
                    py::list results;
                    try {
                        for (auto task_func : tasks) {
                            py::function func = task_func.cast<py::function>();

                            // Create and execute each task
                            auto wrapped_task =
                                create_wrapped_task<py::object>(func);
                            auto task = std::make_unique<
                                atom::async::EnhancedPackagedTask<py::object>>(
                                std::move(wrapped_task));

                            auto future = task->getEnhancedFuture();
                            (*task)();                         // Execute
                            py::object result = future.get();  // Get result
                            results.append(result);
                        }
                        py_future.attr("set_result")(results);
                    } catch (const std::exception& e) {
                        py_future.attr("set_exception")(py::str(e.what()));
                    }
                }).detach();

                return py_future;
            },
            py::arg("tasks"),
            R"pbdoc(
        Create a chain of tasks that execute sequentially.

        Args:
            tasks: List of callable objects to execute in sequence

        Returns:
            A Future object that will contain a list of results

        Examples:
            >>> tasks = [lambda: 1, lambda: 2, lambda: 3]
            >>> future = create_task_chain(tasks)
            >>> results = future.result()  # [1, 2, 3]
        )pbdoc")

        .def(
            "benchmark_packaged_task",
            [](py::function func, size_t num_iterations) -> py::dict {
                using namespace std::chrono;

                py::dict results;
                std::vector<double> execution_times;
                execution_times.reserve(num_iterations);

                for (size_t i = 0; i < num_iterations; ++i) {
                    auto start = high_resolution_clock::now();

                    // Create and execute task
                    auto wrapped_task = create_wrapped_task<py::object>(func);
                    auto task = std::make_unique<
                        atom::async::EnhancedPackagedTask<py::object>>(
                        std::move(wrapped_task));

                    auto future = task->getEnhancedFuture();
                    (*task)();
                    future.get();  // Wait for completion

                    auto end = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(end - start);
                    execution_times.push_back(duration.count());
                }

                // Calculate statistics
                double total_time = std::accumulate(execution_times.begin(),
                                                    execution_times.end(), 0.0);
                double avg_time = total_time / num_iterations;
                double min_time = *std::min_element(execution_times.begin(),
                                                    execution_times.end());
                double max_time = *std::max_element(execution_times.begin(),
                                                    execution_times.end());

                results[py::str("num_iterations")] = num_iterations;
                results[py::str("total_time_us")] = total_time;
                results[py::str("average_time_us")] = avg_time;
                results[py::str("min_time_us")] = min_time;
                results[py::str("max_time_us")] = max_time;
                results[py::str("throughput_ops_per_sec")] =
                    (num_iterations * 1000000.0) / total_time;

                return results;
            },
            py::arg("func"), py::arg("num_iterations") = 1000,
            R"pbdoc(
        Benchmark packaged task performance.

        Args:
            func: Function to benchmark
            num_iterations: Number of iterations to run (default: 1000)

        Returns:
            dict: Benchmark results with timing statistics

        Examples:
            >>> results = benchmark_packaged_task(lambda: sum(range(1000)), 100)
            >>> print(f"Average time: {results['average_time_us']:.2f} μs")
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

#ifdef ATOM_USE_LOCKFREE_QUEUE
    m.attr("HAS_LOCKFREE_QUEUE") = true;
#else
    m.attr("HAS_LOCKFREE_QUEUE") = false;
#endif

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO") = true;
#else
    m.attr("HAS_ASIO") = false;
#endif

    m.attr("HARDWARE_CONSTRUCTIVE_INTERFERENCE_SIZE") =
        hardware_constructive_interference_size;
    m.attr("HARDWARE_DESTRUCTIVE_INTERFERENCE_SIZE") =
        hardware_destructive_interference_size;
}
