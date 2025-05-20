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
    m.doc() =
        "Enhanced packaged task implementation module for the atom package";

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
            "is callable).");

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
            "is callable).");

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
}