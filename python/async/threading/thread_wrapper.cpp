#include "atom/async/threading/thread_wrapper.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <future>
#include <sstream>
#include <thread>
#include <variant>

namespace py = pybind11;
using namespace atom::async;

// Helper for task cancellation
struct TaskCancellation {
    std::stop_source source;

    TaskCancellation() : source(std::stop_source()) {}

    bool request_stop() { return source.request_stop(); }

    bool stop_requested() const { return source.stop_requested(); }

    std::stop_token get_token() const { return source.get_token(); }
};

// Helper for async task with cancellation support
template <typename R>
class AsyncTask {
public:
    AsyncTask(std::function<R(std::stop_token)> func)
        : cancellation_(std::make_shared<TaskCancellation>()),
          future_(std::async(std::launch::async,
                             [func, cancellation = cancellation_]() -> R {
                                 return func(cancellation->get_token());
                             })) {}

    bool cancel() { return cancellation_->request_stop(); }

    bool is_cancelled() const { return cancellation_->stop_requested(); }

    std::variant<R, std::exception_ptr> get_result(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        if (timeout.count() > 0) {
            auto status = future_.wait_for(timeout);
            if (status ==
                std::future_status::timeout) {  // Check for timeout explicitly
                throw std::runtime_error("Task not completed within timeout");
            }
            // if (status != std::future_status::ready) { // Original check,
            // also fine
            //    throw std::runtime_error("Task not completed or deferred");
            // }
        }

        try {
            return future_.get();
        } catch (...) {
            return std::current_exception();
        }
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        return future_.wait_for(timeout_duration) == std::future_status::ready;
    }

    bool is_ready() {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

private:
    std::shared_ptr<TaskCancellation> cancellation_;
    std::future<R> future_;
};

PYBIND11_MODULE(thread_wrapper, m) {
    m.doc() = R"pbdoc(
        Advanced Thread Management and Coroutine Support Module
        ======================================================

        This module provides comprehensive thread management capabilities including:
        - Enhanced Thread wrapper with C++20 jthread support
        - Asynchronous task execution with cancellation
        - C++20 coroutine Task support
        - Thread pool integration
        - Parallel execution utilities

        Key Features:
        - Stop token support for cooperative cancellation
        - Timeout and periodic execution
        - Exception-safe thread management
        - Future-based result handling
        - Hardware concurrency detection

        Examples:
            >>> from atom.async.thread_wrapper import Thread, create_async_task
            >>>
            >>> # Basic thread usage
            >>> thread = Thread()
            >>> thread.start(lambda: print("Hello from thread!"))
            >>> thread.join()
            >>>
            >>> # Async task with cancellation
            >>> task = create_async_task(lambda stop_token: expensive_computation())
            >>> result = task.get_result(timeout=milliseconds(5000))
        )pbdoc";

    // Register exception classes
    py::register_exception<atom::async::ThreadException>(m, "ThreadException");
    py::register_exception<atom::async::ThreadPoolException>(
        m, "ThreadPoolException");

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::ThreadException& e) {
            throw std::runtime_error(e.what());
        } catch (const atom::async::ThreadPoolException& e) {
            throw std::runtime_error(e.what());
        } catch (const std::invalid_argument& e) {
            throw py::value_error(e.what());
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(e.what());
        } catch (py::error_already_set& e) {
            // If a pybind11 error is already set, just restore it.
            e.restore();
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
        }
    });

    // Thread class binding
    py::class_<atom::async::Thread,
               std::shared_ptr<atom::async::Thread>>(  // Assuming Thread is
                                                       // managed by shared_ptr
                                                       // if returned by
                                                       // factories
        m, "Thread",
        R"(A wrapper class for managing a C++20 jthread with enhanced functionality.

This class provides a convenient interface for managing threads, allowing for
starting, stopping, and joining threads easily.

Examples:
    >>> from atom.async import Thread
    >>> def worker(stop_token, name):
    ...     import time
    ...     print(f"Worker {name} started")
    ...     while not stop_token.stop_requested():
    ...         time.sleep(0.1)
    ...     print(f"Worker {name} stopped")
    >>> thread = Thread()
    >>> thread.start(worker, "thread1")
    >>> # Let it run for a bit
    >>> import time; time.sleep(0.2)
    >>> thread.request_stop()
    >>> thread.join()
)")
        .def(py::init<>(), "Constructs a new Thread object.")
        .def(
            "start",
            [](atom::async::Thread& self, py::function py_func,
               py::args py_args_tuple) {
                self.start([&self, py_func,
                            py_args_tuple]() {  // This lambda is void()
                    py::gil_scoped_acquire acquire;
                    std::stop_token st = self.getStopToken();

                    try {
                        py_func(py::cast(st), *py_args_tuple);
                    } catch (const py::error_already_set& e) {
                        if (e.matches(PyExc_TypeError)) {
                            PyErr_Clear();
                            py_func(*py_args_tuple);
                        } else {
                            throw;
                        }
                    } catch (...) {
                        throw;
                    }
                });
            },
            py::arg(
                "func"),  // py::args are implicitly handled by py_args_tuple
            R"(Starts a new thread with the specified callable object and arguments.
The callable can optionally accept a `StopToken` as its first argument.

Args:
    func: The callable object to execute in the new thread.
    *args: The arguments to pass to the callable object (after the optional StopToken).

Raises:
    RuntimeError: If the thread cannot be started.
)")
        .def(
            "start_with_result",
            [](atom::async::Thread& self, py::function func, py::args args) {
                return self.startWithResult<py::object>(
                    [func, args]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        return func(*args);
                    });
            },
            py::arg("func"),  // py::args handled by lambda
            R"(Starts a thread with a function that returns a value.

Args:
    func: The callable object to execute in the new thread.
    *args: The arguments to pass to the callable object.

Returns:
    A future that will contain the result.
)")
        .def("request_stop", &atom::async::Thread::requestStop,
             R"(Requests the thread to stop execution.
)")
        .def("join", &atom::async::Thread::join,
             R"(Waits for the thread to finish execution.
)")
        .def("try_join_for",
             static_cast<bool (atom::async::Thread::*)(
                 const std::chrono::milliseconds&)>(
                 &atom::async::Thread::tryJoinFor),
             py::arg("timeout"),
             R"(Tries to join the thread with a timeout.

Args:
    timeout: The maximum time to wait (e.g., atom.async.milliseconds(500)).

Returns:
    True if joined successfully, False if timed out.
)")
        .def("running", &atom::async::Thread::running,
             R"(Checks if the thread is currently running.
)")
        .def(
            "get_id",
            [](const atom::async::Thread& self) {
                std::ostringstream oss;
                oss << self.getId();
                return py::str(oss.str());
            },
            R"(Gets the ID of the thread.
)")
        .def("should_stop", &atom::async::Thread::shouldStop,
             R"pbdoc(
             Checks if the thread should stop.

             Returns:
                 bool: True if a stop has been requested, False otherwise.
             )pbdoc")
        .def(
            "start_with_result",
            [](atom::async::Thread& self, py::function func) {
                return self.startWithResult<py::object>([func]() -> py::object {
                    py::gil_scoped_acquire acquire;
                    return func();
                });
            },
            py::arg("func"),
            R"pbdoc(
            Start the thread with a function that returns a result.

            Args:
                func: Function to execute that returns a value.

            Returns:
                std::future: A future that will contain the result.

            Examples:
                >>> thread = Thread()
                >>> future = thread.start_with_result(lambda: 42)
                >>> result = future.get()  # 42
            )pbdoc")
        .def(
            "set_timeout",
            [](atom::async::Thread& self, std::chrono::milliseconds timeout) {
                self.setTimeout(timeout);
            },
            py::arg("timeout"),
            R"pbdoc(
            Set a timeout for the thread execution.

            Args:
                timeout: Timeout duration in milliseconds.
            )pbdoc")
        .def(
            "start_periodic",
            [](atom::async::Thread& self, py::function func,
               std::chrono::milliseconds interval) {
                self.startPeriodic(
                    [func]() {
                        py::gil_scoped_acquire acquire;
                        func();
                    },
                    interval);
            },
            py::arg("func"), py::arg("interval"),
            R"pbdoc(
            Start a thread that executes a function periodically.

            Args:
                func: Function to execute periodically.
                interval: Interval between executions in milliseconds.
            )pbdoc")
        .def(
            "start_delayed",
            [](atom::async::Thread& self, std::chrono::milliseconds delay,
               py::function func) {
                self.startDelayed(delay, [func]() {
                    py::gil_scoped_acquire acquire;
                    func();
                });
            },
            py::arg("delay"), py::arg("func"),
            R"pbdoc(
            Start a thread with a delay before execution.

            Args:
                delay: Delay before execution in milliseconds.
                func: Function to execute after the delay.
            )pbdoc")
        .def("set_thread_name", &atom::async::Thread::setThreadName,
             py::arg("name"),
             R"pbdoc(
             Set the name of the thread for debugging purposes.

             Args:
                 name: Name to assign to the thread.
             )pbdoc")
        .def(
            "try_join_for",
            [](atom::async::Thread& self, std::chrono::milliseconds timeout) {
                return self.tryJoinFor(timeout);
            },
            py::arg("timeout"),
            R"pbdoc(
            Try to join the thread with a timeout.

            Args:
                timeout: Maximum time to wait for the thread to finish.

            Returns:
                bool: True if the thread finished within the timeout, False otherwise.
            )pbdoc")
        .def("swap", &atom::async::Thread::swap, py::arg("other"),
             R"pbdoc(
             Swap this thread with another thread.

             Args:
                 other: Another Thread object to swap with.
             )pbdoc")
        .def("get_id", &atom::async::Thread::getId,
             R"pbdoc(
             Get the thread ID.

             Returns:
                 std::thread::id: The unique identifier of the thread.
             )pbdoc")
        .def("get_stop_token", &atom::async::Thread::getStopToken,
             R"pbdoc(
             Get the stop token for this thread.

             Returns:
                 std::stop_token: Token that can be used to check for stop requests.
             )pbdoc")
        .def("detach", &atom::async::Thread::detach,
             R"pbdoc(
             Detach the thread, allowing it to run independently.

             After calling this method, the thread will continue to run
             but cannot be joined.
             )pbdoc")
        .def("joinable", &atom::async::Thread::joinable,
             R"pbdoc(
             Check if the thread is joinable.

             Returns:
                 bool: True if the thread can be joined, False otherwise.
             )pbdoc")
        .def("get_native_handle", &atom::async::Thread::getNativeHandle,
             R"pbdoc(
             Get the native handle of the thread.

             Returns:
                 The platform-specific native thread handle.

             Note: This is an advanced feature for platform-specific operations.
             )pbdoc")
        .def_static("get_hardware_concurrency",
                    &atom::async::Thread::getHardwareConcurrency,
                    R"pbdoc(
             Get the number of concurrent threads supported by the implementation.

             Returns:
                 int: Number of concurrent threads supported, or 0 if not determinable.

             This is useful for determining optimal thread pool sizes.
             )pbdoc")
        .def_static("set_current_thread_name",
                    &atom::async::Thread::setCurrentThreadName, py::arg("name"),
                    R"pbdoc(
             Set the name of the current thread for debugging purposes.

             Args:
                 name: Name to assign to the current thread.

             This is useful for debugging and profiling tools.
             )pbdoc");

    // TaskCancellation class binding
    py::class_<TaskCancellation, std::shared_ptr<TaskCancellation>>(
        m, "TaskCancellation",
        R"(Provides cancellation support for asynchronous tasks.)")
        .def(py::init<>(), "Constructs a new TaskCancellation object.")
        .def("request_stop", &TaskCancellation::request_stop,
             R"(Requests cancellation.)")
        .def("stop_requested", &TaskCancellation::stop_requested,
             R"(Checks if cancellation has been requested.)");

    // StopToken binding
    py::class_<std::stop_token>(
        m, "StopToken",
        R"(A token that can be used to check if cancellation has been requested.)")
        .def(py::init<>())  // Default constructor
        .def("stop_requested", &std::stop_token::stop_requested,
             R"(Checks if cancellation has been requested.)")
        // Add other members if needed, e.g., stop_possible, constructor from
        // stop_source
        ;

    // AsyncTask template instantiation for Python object
    py::class_<AsyncTask<py::object>, std::shared_ptr<AsyncTask<py::object>>>(
        m, "AsyncTask", R"(An asynchronous task with cancellation support.)")
        // Constructor for AsyncTask typically takes a
        // std::function<R(std::stop_token)>. Exposing a direct py::init might
        // require a factory if the lambda capture is complex. For now, assuming
        // it's created via create_async_task.
        .def("cancel", &AsyncTask<py::object>::cancel,
             R"(Requests cancellation of the task.)")
        .def("is_cancelled", &AsyncTask<py::object>::is_cancelled,
             R"(Checks if the task has been cancelled.)")
        .def(
            "get_result",
            [](AsyncTask<py::object>& self, std::chrono::milliseconds timeout) {
                py::gil_scoped_release
                    release_gil;  // Allow other Python threads to run while
                                  // waiting
                auto result_variant = self.get_result(timeout);
                py::gil_scoped_acquire
                    acquire_gil;  // Re-acquire GIL for Python object
                                  // manipulation

                if (std::holds_alternative<py::object>(result_variant)) {
                    return std::get<py::object>(result_variant);
                } else {
                    std::rethrow_exception(
                        std::get<std::exception_ptr>(result_variant));
                    return py::object();  // Unreachable
                }
            },
            py::arg("timeout") = std::chrono::milliseconds(0),
            R"(Gets the result of the task.)")
        .def(
            "wait_for",
            [](AsyncTask<py::object>& self, std::chrono::milliseconds timeout) {
                py::gil_scoped_release release_gil;
                bool result = self.wait_for(timeout);
                // py::gil_scoped_acquire acquire_gil; // Not strictly needed if
                // only returning bool
                return result;
            },
            py::arg("timeout"),
            R"(Waits for the task to complete up to the specified timeout.)")
        .def("is_ready", &AsyncTask<py::object>::is_ready,
             R"(Checks if the task has completed.)");

    // Future API for Python use (std::shared_future<py::object>)
    py::class_<std::shared_future<py::object>>(  // Changed from std::future if
                                                 // ThreadPool returns
                                                 // shared_future or if future
                                                 // is shared
        m, "Future",  // Consider if this should be std::future or
                      // std::shared_future based on usage
        R"(A future representing the result of an asynchronous operation.)")
        .def(
            "result",
            [](std::shared_future<py::object>& self) {
                py::gil_scoped_release release_gil;
                // py::object result_obj = self.get(); // self.get() can only be
                // called once on std::future For std::shared_future, it's okay.
                // py::gil_scoped_acquire acquire_gil;
                // return result_obj;
                // More robustly, handle exceptions from get()
                try {
                    return self.get();  // This will re-acquire GIL if
                                        // py::object construction needs it
                } catch (const std::exception& e) {
                    py::gil_scoped_acquire
                        acquire_gil;  // Ensure GIL for PyErr_SetString
                    PyErr_SetString(
                        PyExc_RuntimeError,
                        (std::string("Task failed: ") + e.what()).c_str());
                    throw py::error_already_set();
                }
            },
            R"(Gets the result of the asynchronous operation.)")
        .def(
            "wait",
            [](std::shared_future<py::object>& self) {
                py::gil_scoped_release release_gil;
                self.wait();
            },
            R"(Waits for the operation to complete.)")
        .def(
            "wait_for",
            [](std::shared_future<py::object>& self,
               std::chrono::milliseconds timeout) {
                py::gil_scoped_release release_gil;
                return self.wait_for(timeout) == std::future_status::ready;
            },
            py::arg("timeout"),
            R"(Waits for the operation to complete up to the specified timeout.)")
        .def(
            "is_ready",  // Renamed from valid() for clarity, or use valid() if
                         // that's the C++ method
            [](std::shared_future<py::object>& self) {
                return self.wait_for(std::chrono::seconds(0)) ==
                       std::future_status::ready;
            },
            R"(Checks if the operation has completed.)");

    // Task class binding (C++20 coroutine Task)
    py::class_<atom::async::Task<py::object>>(m, "Task",
                                              R"pbdoc(
        A simple C++20 coroutine task wrapper.

        This class provides a coroutine-based asynchronous programming model
        for non-blocking execution. It supports cooperative cancellation,
        completion callbacks, and exception handling.

        Examples:
            >>> async def my_coroutine():
            ...     # Coroutine implementation
            ...     return "result"
            >>> task = Task(my_coroutine())
            >>> task.set_completion_callback(lambda: print("Task completed"))
            >>> result = task.get_result()
        )pbdoc")
        .def("__repr__",
             [](const atom::async::Task<py::object>&) {
                 return "<Task coroutine object>";
             })
        .def("is_completed", &atom::async::Task<py::object>::isCompleted,
             R"pbdoc(
             Check if the task has completed.

             Returns:
                 bool: True if the task has completed (either successfully or with an exception).

             This method is non-blocking and can be called multiple times.
             )pbdoc")
        .def("get_result", &atom::async::Task<py::object>::getResult,
             R"pbdoc(
             Get the result of the task.

             Returns:
                 The result of the task execution.

             Raises:
                 Exception: If the task completed with an exception.
                 RuntimeError: If the task has not completed yet.

             Note: This method will block if the task is not yet completed.
             )pbdoc")
        .def(
            "set_completion_callback",
            [](atom::async::Task<py::object>& self, py::function callback) {
                self.setCompletionCallback([callback]() {
                    py::gil_scoped_acquire acquire;
                    callback();
                });
            },
            py::arg("callback"),
            R"pbdoc(
            Set a callback to be called when the task completes.

            Args:
                callback: Function to call when the task completes.

            The callback will be called regardless of whether the task
            completes successfully or with an exception.
            )pbdoc")
        .def("has_exception", &atom::async::Task<py::object>::hasException,
             R"pbdoc(
             Check if the task completed with an exception.

             Returns:
                 bool: True if the task completed with an exception, False otherwise.

             This method can be used to check for exceptions without
             triggering them by calling get_result().
             )pbdoc");

    // Task void specialization
    py::class_<atom::async::Task<void>>(m, "TaskVoid",
                                        R"pbdoc(
        A C++20 coroutine task wrapper for void operations.

        This specialization is optimized for tasks that don't return a value
        but may still need completion tracking and exception handling.
        )pbdoc")
        .def("__repr__",
             [](const atom::async::Task<void>&) {
                 return "<TaskVoid coroutine object>";
             })
        .def("is_completed", &atom::async::Task<void>::isCompleted,
             R"pbdoc(
             Check if the void task has completed.

             Returns:
                 bool: True if the task has completed.
             )pbdoc")
        .def("wait", &atom::async::Task<void>::wait,
             R"pbdoc(
             Wait for the void task to complete.

             Raises:
                 Exception: If the task completed with an exception.
             )pbdoc")
        .def(
            "set_completion_callback",
            [](atom::async::Task<void>& self, py::function callback) {
                self.setCompletionCallback([callback]() {
                    py::gil_scoped_acquire acquire;
                    callback();
                });
            },
            py::arg("callback"),
            R"pbdoc(
            Set a callback to be called when the void task completes.

            Args:
                callback: Function to call when the task completes.
            )pbdoc")
        .def("has_exception", &atom::async::Task<void>::hasException,
             R"pbdoc(
             Check if the void task completed with an exception.

             Returns:
                 bool: True if the task completed with an exception.
             )pbdoc");

    // Factory functions
    m.def(
        "create_thread",
        []() {
            return std::make_shared<atom::async::Thread>();
        },  // Return shared_ptr if class is bound with it
        R"(Creates a new Thread object.)");

    m.def(
        "create_async_task",
        [](py::function func) {
            return std::make_shared<AsyncTask<py::object>>(
                [func](std::stop_token st) -> py::object {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(py::cast(st));
                    } catch (const py::error_already_set& e) {
                        if (e.matches(PyExc_TypeError)) {
                            PyErr_Clear();
                            return func();
                        }
                        throw;
                    }
                });
        },
        py::arg("func"),
        R"(Creates a new AsyncTask with cancellation support.)");

    m.def(
        "current_thread_id",
        []() {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            return py::str(oss.str());
        },
        R"(Gets the ID of the current thread.)");

    m.def(
        "sleep",
        [](std::chrono::milliseconds duration) {
            py::gil_scoped_release release;  // Release GIL while sleeping
            std::this_thread::sleep_for(duration);
        },
        py::arg("duration"),
        R"(Suspends the current thread for the specified duration.)");

    m.def(
        "yield_now",
        []() {
            py::gil_scoped_release release;  // Release GIL during yield
            std::this_thread::yield();
        },
        R"(Suggests that the implementation reschedules execution of threads.)");

    // Helper functions for time durations
    m.def(
        "milliseconds",
        [](long long ms) { return std::chrono::milliseconds(ms); },
        py::arg("ms"), R"(Creates a milliseconds duration.)");
    m.def(
        "seconds", [](long long s) { return std::chrono::seconds(s); },
        py::arg("s"), R"(Creates a seconds duration.)");
    m.def(
        "minutes", [](long long m) { return std::chrono::minutes(m); },
        py::arg("m"), R"(Creates a minutes duration.)");

    m.def(
        "hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        R"(Gets the number of concurrent threads supported.)");

    m.def(
        "run_in_background",
        [](py::function func, py::args py_args_tuple) {
            auto thread_obj = std::make_shared<atom::async::Thread>();
            thread_obj->start([func,
                               py_args_tuple]() {  // This lambda is void()
                py::gil_scoped_acquire acquire;
                try {
                    func(*py_args_tuple);
                } catch (const py::error_already_set& e) {
                    py::error_already_set temp_e =
                        e;  // Create a non-const copy
                    temp_e.restore();
                    PyErr_WriteUnraisable(func.ptr());
                } catch (const std::exception& e) {
                    // 记录C++异常信息
                    py::print("C++ exception in background thread:", e.what());
                    PyErr_SetString(PyExc_RuntimeError, e.what());
                    PyErr_WriteUnraisable(func.ptr());
                } catch (...) {
                    // 处理未知异常
                    py::print("Unknown exception in background thread");
                    PyErr_SetString(PyExc_RuntimeError,
                                    "Unknown C++ exception");
                    PyErr_WriteUnraisable(func.ptr());
                }
            });
            return thread_obj;
        },
        py::arg("func"),  // py_args_tuple captures *args
        R"(Runs a function in a background thread.)");

    // Thread pool utility functions
    m.def(
        "run_in_thread_pool",
        [](py::function func, py::args py_args_tuple) {
            // Use global thread pool to submit task
            return atom::async::globalThreadPool().submit(
                [func, py_args_tuple]() -> py::object {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(*py_args_tuple);
                    } catch (const py::error_already_set& e) {
                        // Python exception already set, propagate directly
                        throw;
                    } catch (const std::exception& e) {
                        // Convert C++ exception to Python exception
                        PyErr_SetString(PyExc_RuntimeError, e.what());
                        throw py::error_already_set();
                    } catch (...) {
                        // Handle unknown exceptions
                        PyErr_SetString(PyExc_RuntimeError,
                                        "Unknown C++ exception");
                        throw py::error_already_set();
                    }
                });
        },
        py::arg("func"),
        R"pbdoc(
        Runs a function in the thread pool and returns a future for the result.

        Args:
            func: Function to execute in the thread pool.
            *args: Arguments to pass to the function.

        Returns:
            std::future: A future that will contain the result.

        Examples:
            >>> future = run_in_thread_pool(lambda x: x * 2, 21)
            >>> result = future.get()  # 42
        )pbdoc");

    // Additional utility functions
    m.def(
        "parallel_for_each",
        [](py::list items, py::function func, unsigned int num_threads) {
            std::vector<py::object> items_vec;
            for (auto item : items) {
                items_vec.push_back(item.cast<py::object>());
            }

            atom::async::parallel_for_each(
                items_vec.begin(), items_vec.end(),
                [func](const py::object& item) {
                    py::gil_scoped_acquire acquire;
                    func(item);
                },
                num_threads);
        },
        py::arg("items"), py::arg("func"),
        py::arg("num_threads") = std::thread::hardware_concurrency(),
        R"pbdoc(
        Execute a function in parallel for each item in a collection.

        Args:
            items: List of items to process.
            func: Function to apply to each item.
            num_threads: Number of threads to use (default: hardware concurrency).

        Examples:
            >>> items = [1, 2, 3, 4, 5]
            >>> parallel_for_each(items, lambda x: print(f"Processing {x}"))
        )pbdoc");

    // Advanced thread management utilities
    m.def(
        "get_current_thread_id",
        []() {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            return py::str(oss.str());
        },
        R"pbdoc(
        Get the ID of the current thread.

        Returns:
            str: String representation of the current thread ID.

        This is useful for debugging and logging purposes.
        )pbdoc");

    m.def(
        "is_main_thread",
        []() {
            static const auto main_thread_id = std::this_thread::get_id();
            return std::this_thread::get_id() == main_thread_id;
        },
        R"pbdoc(
        Check if the current thread is the main thread.

        Returns:
            bool: True if running in the main thread, False otherwise.

        This can be useful for ensuring certain operations only
        happen on the main thread.
        )pbdoc");

    m.def(
        "get_thread_count",
        []() { return atom::async::globalThreadPool().getThreadCount(); },
        R"pbdoc(
        Get the number of threads in the global thread pool.

        Returns:
            int: Number of threads currently in the global thread pool.
        )pbdoc");

    m.def(
        "get_active_thread_count",
        []() { return atom::async::globalThreadPool().getActiveThreadCount(); },
        R"pbdoc(
        Get the number of active threads in the global thread pool.

        Returns:
            int: Number of threads currently executing tasks.
        )pbdoc");

    m.def(
        "get_pending_task_count",
        []() { return atom::async::globalThreadPool().getPendingTaskCount(); },
        R"pbdoc(
        Get the number of pending tasks in the global thread pool.

        Returns:
            int: Number of tasks waiting to be executed.
        )pbdoc");

    // Thread synchronization utilities
    m.def(
        "wait_for_all_threads",
        []() { atom::async::globalThreadPool().waitForAllTasks(); },
        R"pbdoc(
        Wait for all pending tasks in the global thread pool to complete.

        This function blocks until all currently queued tasks have finished
        executing. It's useful for ensuring all background work is done
        before program termination.
        )pbdoc");

    m.def(
        "shutdown_thread_pool",
        []() { atom::async::globalThreadPool().shutdown(); },
        R"pbdoc(
        Shutdown the global thread pool gracefully.

        This stops accepting new tasks and waits for all current tasks
        to complete before shutting down all threads.

        Warning: After calling this, the thread pool cannot be restarted.
        )pbdoc");

    // Performance monitoring utilities
    m.def(
        "get_cpu_usage",
        []() -> py::dict {
            py::dict result;
            result["hardware_concurrency"] =
                std::thread::hardware_concurrency();
            result["active_threads"] =
                atom::async::globalThreadPool().getActiveThreadCount();
            result["total_threads"] =
                atom::async::globalThreadPool().getThreadCount();
            result["pending_tasks"] =
                atom::async::globalThreadPool().getPendingTaskCount();
            return result;
        },
        R"pbdoc(
        Get comprehensive CPU and thread usage information.

        Returns:
            dict: Dictionary containing:
                - hardware_concurrency: Number of CPU cores
                - active_threads: Currently active threads
                - total_threads: Total threads in pool
                - pending_tasks: Tasks waiting for execution

        This is useful for monitoring and performance tuning.
        )pbdoc");

    // Static thread utility functions
    m.def("set_current_thread_name", &atom::async::Thread::setCurrentThreadName,
          py::arg("name"),
          R"pbdoc(
          Set the name of the current thread for debugging purposes.

          Args:
              name: Name to assign to the current thread.

          Thread names are visible in debuggers and profiling tools,
          making it easier to identify specific threads during development.
          )pbdoc");
}
