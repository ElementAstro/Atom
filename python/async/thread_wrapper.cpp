#include "atom/async/thread_wrapper.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <future>
#include <sstream>  // For std::ostringstream
#include <thread>
#include <variant>
#include "pool.hpp"

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
    m.doc() = "Thread wrapper implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (py::error_already_set& e) {
            // If a pybind11 error is already set, just restore it.
            // This can happen if a C++ exception derived from
            // py::error_already_set is caught.
            e.restore();
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
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
             R"(Checks if the thread should stop.
)");

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
    py::class_<atom::async::Task<py::object>>(
        m, "Task", R"(A simple C++20 coroutine task wrapper.)")
        .def("__repr__", [](const atom::async::Task<py::object>&) {
            return "<Task coroutine object>";
        });

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

    // 添加使用线程池运行函数的功能
    m.def(
        "run_in_thread_pool",
        [](py::function func, py::args py_args_tuple) {
            // 使用全局线程池提交任务
            return atom::async::globalThreadPool().submit(
                [func, py_args_tuple]() -> py::object {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(*py_args_tuple);
                    } catch (const py::error_already_set& e) {
                        // Python异常已经被设置，直接传播
                        throw;
                    } catch (const std::exception& e) {
                        // 转换C++异常为Python异常
                        PyErr_SetString(PyExc_RuntimeError, e.what());
                        throw py::error_already_set();
                    } catch (...) {
                        // 处理未知异常
                        PyErr_SetString(PyExc_RuntimeError,
                                        "Unknown C++ exception");
                        throw py::error_already_set();
                    }
                });
        },
        py::arg("func"),
        R"(Runs a function in the thread pool and returns a future for the result.)");
}