#include "atom/async/thread_wrapper.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(async, m) {
    m.doc() =
        "Asynchronous utilities implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Thread class binding
    py::class_<atom::async::Thread>(
        m, "Thread",
        R"(A wrapper class for managing threads with enhanced functionality.

This class provides a convenient interface for managing threads, allowing
for starting, stopping, and joining threads easily.

Examples:
    >>> from atom.async import Thread
    >>> def task(x):
    ...     return x * 2
    >>> thread = Thread()
    >>> thread.start(task, 5)
    >>> thread.join()
)")
        .def(py::init<>(), "Constructs a new Thread object.")
        .def(
            "start",
            [](atom::async::Thread& self, py::function func, py::args args) {
                self.start([func, args]() {
                    py::gil_scoped_acquire acquire;
                    try {
                        func(*args);
                    } catch (const py::error_already_set& e) {
                        PyErr_SetString(PyExc_RuntimeError, e.what());
                    }
                });
            },
            py::arg("func"), py::kw_only(), py::arg("args") = py::tuple(),
            R"(Starts a new thread with the specified callable object and arguments.

Args:
    func: The callable object to execute in the new thread.
    args: The arguments to pass to the callable object.

Raises:
    RuntimeError: If the thread cannot be started.
)")
        .def(
            "start_with_result",
            [](atom::async::Thread& self, py::function func, py::args args) {
                auto future = self.startWithResult<py::object>([func, args]() {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(*args);
                    } catch (const py::error_already_set& e) {
                        throw std::runtime_error(e.what());
                    }
                });

                return py::cast(future);
            },
            py::arg("func"), py::kw_only(), py::arg("args") = py::tuple(),
            R"(Starts a thread with a function that returns a value.

Args:
    func: The callable object to execute in the new thread.
    args: The arguments to pass to the callable object.

Returns:
    A future object that will contain the result.

Raises:
    RuntimeError: If the thread cannot be started.
)")
        .def("request_stop", &atom::async::Thread::requestStop,
             R"(Requests the thread to stop execution.)")
        .def("join", &atom::async::Thread::join,
             R"(Waits for the thread to finish execution.

Raises:
    RuntimeError: If joining the thread throws an exception.
)")
        .def("try_join_for", &atom::async::Thread::tryJoinFor,
             py::arg("timeout_duration"),
             R"(Tries to join the thread with a timeout.

Args:
    timeout_duration: The maximum time to wait.

Returns:
    True if joined successfully, False if timed out.
)")
        .def("running", &atom::async::Thread::running,
             R"(Checks if the thread is currently running.

Returns:
    True if the thread is running, False otherwise.
)")
        .def("should_stop", &atom::async::Thread::shouldStop,
             R"(Checks if the thread should stop.

Returns:
    True if the thread should stop, False otherwise.
)")
        .def("__bool__", &atom::async::Thread::running,
             "Support for boolean evaluation.");

    // Add helper functions
    m.def(
        "sleep_for",
        [](const std::chrono::microseconds& duration) {
            py::gil_scoped_release release;
            std::this_thread::sleep_for(duration);
        },
        py::arg("duration"),
        R"(Blocks the execution of the current thread for at least the specified duration.

Args:
    duration: The minimum time duration to block for.
)");

    m.def(
        "yield_now",
        []() {
            py::gil_scoped_release release;
            std::this_thread::yield();
        },
        R"(Suggests that the implementation reschedule execution of threads.

This function provides a hint to the implementation to reschedule the
execution of threads, allowing other threads to run.
)");

    m.def(
        "get_thread_id", []() { return std::this_thread::get_id(); },
        R"(Gets the ID of the current thread.

Returns:
    The ID of the current thread.
)");

    m.def(
        "hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        R"(Gets the number of concurrent threads supported by the implementation.

Returns:
    The number of concurrent threads supported by the implementation.
    If the value is not well-defined or not computable, returns 0.
)");
}