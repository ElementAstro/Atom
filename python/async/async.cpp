#include "atom/async/async.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for exception classes
class TimeoutException;

// Declare TimeoutException
void declare_timeout_exception(py::module& m) {
    py::register_exception<TimeoutException>(m, "TimeoutException",
                                             R"pbdoc(
        Exception thrown when a timeout occurs during asynchronous operations.

        This exception is raised when an operation exceeds its specified timeout
        duration, such as waiting for a task to complete or getting a result
        from a future.
        )pbdoc");
}

// Declare platform utilities
void declare_platform_utilities(py::module& m) {
    auto platform_module = m.def_submodule("platform",
                                           R"pbdoc(
        Platform-specific threading utilities.

        This submodule provides platform-specific functionality for thread
        priority management, CPU affinity, and other low-level threading
        operations.
        )pbdoc");

    // Priority constants
    auto priority_class =
        py::class_<atom::platform::Priority>(platform_module, "Priority",
                                             R"pbdoc(
        Platform-specific priority constants.

        These constants provide platform-appropriate priority values for
        different operating systems (Windows, macOS, Linux).
        )pbdoc");

    priority_class.def_readonly_static(
        "LOW", &atom::platform::Priority::LOW,
        "Low priority value for the current platform");
    priority_class.def_readonly_static(
        "NORMAL", &atom::platform::Priority::NORMAL,
        "Normal priority value for the current platform");
    priority_class.def_readonly_static(
        "HIGH", &atom::platform::Priority::HIGH,
        "High priority value for the current platform");
    priority_class.def_readonly_static(
        "CRITICAL", &atom::platform::Priority::CRITICAL,
        "Critical priority value for the current platform");

    // Platform utility functions
    platform_module.def("yield_thread", &atom::platform::yieldThread,
                        R"pbdoc(
        Yields the current thread to allow other threads to run.

        This is a hint to the scheduler that the current thread is willing
        to give up its remaining time slice.
        )pbdoc");

    platform_module.def(
        "sleep_for",
        [](double seconds) {
            atom::platform::sleepFor(std::chrono::nanoseconds(
                static_cast<long long>(seconds * 1e9)));
        },
        py::arg("seconds"),
        R"pbdoc(
        Sleeps for the specified duration.

        Args:
            seconds: Duration to sleep in seconds (can be fractional).

        Examples:
            >>> import atom.async.platform as platform
            >>> platform.sleep_for(0.1)  # Sleep for 100ms
        )pbdoc");
}

// Declare Priority enum for AsyncWorker
void declare_async_worker_priority(py::module& m) {
    py::enum_<atom::async::AsyncWorker<int>::Priority>(m, "AsyncWorkerPriority",
                                                       R"pbdoc(
        Task priority levels for AsyncWorker.

        Controls the thread priority when executing asynchronous tasks.
        Higher priority tasks may receive more CPU time and better scheduling.

        Values:
            LOW: Low priority execution, suitable for background tasks
            NORMAL: Normal priority execution, default for most tasks
            HIGH: High priority execution, for important tasks
            CRITICAL: Critical priority execution, for time-sensitive tasks
        )pbdoc")
        .value("LOW", atom::async::AsyncWorker<int>::Priority::LOW,
               "Low priority execution for background tasks")
        .value("NORMAL", atom::async::AsyncWorker<int>::Priority::NORMAL,
               "Normal priority execution, default level")
        .value("HIGH", atom::async::AsyncWorker<int>::Priority::HIGH,
               "High priority execution for important tasks")
        .value("CRITICAL", atom::async::AsyncWorker<int>::Priority::CRITICAL,
               "Critical priority execution for time-sensitive tasks");
}

// Helper functions
template <typename ResultType>
void declare_async_worker(py::module& m, const std::string& suffix) {
    using WorkerType = atom::async::AsyncWorker<ResultType>;
    using ManagerType = atom::async::AsyncWorkerManager<ResultType>;

    std::string worker_name = "AsyncWorker" + suffix;
    std::string manager_name = "AsyncWorkerManager" + suffix;

    // Register AsyncWorker class
    py::class_<WorkerType, std::shared_ptr<WorkerType>>(m, worker_name.c_str(),
                                                        R"pbdoc(
        Class for performing asynchronous tasks with specific result type.

        This class allows you to start a task asynchronously and get the result when
        it's done. It also provides functionality to cancel the task, check if it's
        done or active, validate the result, set a callback function, and set a
        timeout.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(
            "start_async",
            [](WorkerType& self, py::function func) {
                if constexpr (std::is_same_v<ResultType, void>) {
                    self.startAsync([func]() {
                        py::gil_scoped_acquire acquire;
                        py::object result = func();
                    });
                } else {
                    self.startAsync([func]() -> ResultType {
                        py::gil_scoped_acquire acquire;
                        py::object result = func();
                        return result.cast<ResultType>();
                    });
                }
            },
            py::arg("func"),
            R"pbdoc(
             Starts the task asynchronously.

             Args:
                 func: The function to be executed asynchronously.

             Raises:
                 ValueError: If func is null or invalid.
             )pbdoc")
        .def(
            "get_result",
            [](WorkerType& self, std::chrono::milliseconds timeout) {
                return self.getResult(timeout);
            },
            py::arg("timeout") = std::chrono::milliseconds(0),
            R"pbdoc(
             Gets the result of the task with timeout option.

             Args:
                 timeout: Optional timeout duration in milliseconds (0 means no timeout).

             Returns:
                 The result of the task.

             Raises:
                 ValueError: If the task is not valid.
                 TimeoutException: If the timeout is reached.
             )pbdoc")
        .def(
            "cancel", &WorkerType::cancel,
            "Cancels the task. If the task is valid, waits for it to complete.")
        .def("is_done", &WorkerType::isDone,
             "Checks if the task is done. Returns True if done, False "
             "otherwise.")
        .def("is_active", &WorkerType::isActive,
             "Checks if the task is active. Returns True if active, False "
             "otherwise.")
        .def(
            "set_timeout",
            [](WorkerType& self, double seconds) {
                self.setTimeout(
                    std::chrono::seconds(static_cast<int>(seconds)));
            },
            py::arg("seconds"),
            R"pbdoc(
             Sets a timeout for the task.

             Args:
                 seconds: The timeout duration in seconds.

             Raises:
                 ValueError: If timeout is negative.
             )pbdoc")
        .def("wait_for_completion", &WorkerType::waitForCompletion,
             R"pbdoc(
             Waits for the task to complete.

             If a timeout is set, waits until the task is done or the timeout is reached.
             If a callback function is set and the task is done, the callback is called.

             Raises:
                 TimeoutException: If the timeout is reached.
             )pbdoc")
        .def(
            "set_priority",
            [](WorkerType& self, typename WorkerType::Priority priority) {
                self.setPriority(priority);
            },
            py::arg("priority"),
            R"pbdoc(
             Sets the thread priority for this worker.

             Args:
                 priority: The priority level from AsyncWorkerPriority enum.

             Examples:
                 >>> worker.set_priority(AsyncWorkerPriority.HIGH)
             )pbdoc")
        .def(
            "set_preferred_cpu",
            [](WorkerType& self, size_t cpu_id) {
                self.setPreferredCPU(cpu_id);
            },
            py::arg("cpu_id"),
            R"pbdoc(
             Sets the preferred CPU core for this worker.

             Args:
                 cpu_id: The CPU core ID to prefer for execution.

             Examples:
                 >>> worker.set_preferred_cpu(2)  # Use CPU core 2
             )pbdoc")
        .def("is_cancellation_requested", &WorkerType::isCancellationRequested,
             R"pbdoc(
             Checks if cancellation has been requested for this worker.

             Returns:
                 bool: True if cancellation was requested, False otherwise.
             )pbdoc");

    // Register callback version separately for non-void types
    if constexpr (!std::is_same_v<ResultType, void>) {
        py::class_<WorkerType, std::shared_ptr<WorkerType>>(m,
                                                            worker_name.c_str())
            .def(
                "validate",
                [](WorkerType& self, py::function validator) {
                    return self.validate(
                        [validator](ResultType result) -> bool {
                            py::gil_scoped_acquire acquire;
                            return validator(result).template cast<bool>();
                        });
                },
                py::arg("validator"),
                R"pbdoc(
                Validates the result of the task using a validator function.

                Args:
                    validator: The function used to validate the result.

                Returns:
                    True if the result is valid, False otherwise.
                )pbdoc")
            .def(
                "set_callback",
                [](WorkerType& self, py::function callback) {
                    self.setCallback([callback](ResultType result) {
                        py::gil_scoped_acquire acquire;
                        callback(result);
                    });
                },
                py::arg("callback"),
                R"pbdoc(
                Sets a callback function to be called when the task is done.

                Args:
                    callback: The callback function to be set.

                Raises:
                    ValueError: If callback is empty.
                )pbdoc");
    }

    // Register AsyncWorkerManager class
    py::class_<ManagerType, std::shared_ptr<ManagerType>>(m,
                                                          manager_name.c_str(),
                                                          R"pbdoc(
        Class for managing multiple AsyncWorker instances.

        This class provides functionality to create and manage multiple AsyncWorker
        instances with specific result type.
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(
            "create_worker",
            [](ManagerType& self, py::function func) {
                return self.createWorker([func]() -> ResultType {
                    py::gil_scoped_acquire acquire;
                    py::object result = func();
                    return result.cast<ResultType>();
                });
            },
            py::arg("func"),
            R"pbdoc(
             Creates a new AsyncWorker instance and starts the task asynchronously.

             Args:
                 func: The function to be executed asynchronously.

             Returns:
                 A shared pointer to the created AsyncWorker instance.
             )pbdoc")
        .def("cancel_all", &ManagerType::cancelAll,
             "Cancels all the managed tasks.")
        .def("all_done", &ManagerType::allDone,
             "Checks if all the managed tasks are done.")
        .def("wait_for_all", &ManagerType::waitForAll,
             py::arg("timeout") = std::chrono::milliseconds(0),
             R"pbdoc(
             Waits for all the managed tasks to complete.

             Args:
                 timeout: Optional timeout for each task in milliseconds (0 means no timeout)

             Raises:
                 TimeoutException: If any task exceeds the timeout.
             )pbdoc")
        .def("is_done", &ManagerType::isDone, py::arg("worker"),
             R"pbdoc(
             Checks if a specific task is done.

             Args:
                 worker: The AsyncWorker instance to check.

             Returns:
                 True if the task is done, False otherwise.

             Raises:
                 ValueError: If worker is null.
             )pbdoc")
        .def("cancel", &ManagerType::cancel, py::arg("worker"),
             R"pbdoc(
             Cancels a specific task.

             Args:
                 worker: The AsyncWorker instance to cancel.

             Raises:
                 ValueError: If worker is null.
             )pbdoc")
        .def("size", &ManagerType::size, "Gets the number of managed workers.")
        .def("prune_completed_workers", &ManagerType::pruneCompletedWorkers,
             R"pbdoc(
             Removes completed workers from the manager.

             Returns:
                 int: The number of workers that were removed.

             Examples:
                 >>> removed_count = manager.prune_completed_workers()
                 >>> print(f"Removed {removed_count} completed workers")
             )pbdoc");
}

// Declare BackoffStrategy enum
void declare_backoff_strategy(py::module& m) {
    py::enum_<atom::async::BackoffStrategy>(
        m, "BackoffStrategy",
        "Retry strategy enum for different backoff strategies")
        .value("FIXED", atom::async::BackoffStrategy::FIXED,
               "Use a fixed delay between retries")
        .value("LINEAR", atom::async::BackoffStrategy::LINEAR,
               "Use a linearly increasing delay between retries")
        .value("EXPONENTIAL", atom::async::BackoffStrategy::EXPONENTIAL,
               "Use an exponentially increasing delay between retries")
        .export_values();
}

// Helper for asyncRetry with Python functions
template <typename ResultType>
std::function<ResultType()> create_py_function(py::function func) {
    return [func]() -> ResultType {
        py::gil_scoped_acquire acquire;
        if constexpr (std::is_same_v<ResultType, void>) {
            py::object result = func();
            return;
        } else {
            py::object result = func();
            return result.cast<ResultType>();
        }
    };
}

// Async retry bindings for different return types
template <typename ResultType>
void declare_async_retry(py::module& m, const std::string& suffix) {
    m.def(("async_retry" + suffix).c_str(),
          [](py::function func, int attempts_left,
             std::chrono::milliseconds initial_delay,
             atom::async::BackoffStrategy strategy,
             std::chrono::milliseconds max_total_delay, py::function callback,
             py::function exception_handler, py::function complete_handler) {
              auto py_func = create_py_function<ResultType>(func);
              auto py_callback = [callback](auto result) {
                  py::gil_scoped_acquire acquire;
                  if constexpr (std::is_same_v<ResultType, void>) {
                      py::object cb_result = callback();
                  } else {
                      py::object cb_result = callback(result);
                  }
              };
              auto py_exception_handler =
                  [exception_handler](const std::exception& e) {
                      py::gil_scoped_acquire acquire;
                      py::object eh_result = exception_handler(e.what());
                  };
              auto py_complete_handler = [complete_handler]() {
                  py::gil_scoped_acquire acquire;
                  py::object ch_result = complete_handler();
              };

              return atom::async::asyncRetry(
                  std::move(py_func), attempts_left, initial_delay, strategy,
                  max_total_delay, std::move(py_callback),
                  std::move(py_exception_handler),
                  std::move(py_complete_handler));
          },
          py::arg("func"), py::arg("attempts_left") = 3,
          py::arg("initial_delay") = std::chrono::milliseconds(100),
          py::arg("strategy") = atom::async::BackoffStrategy::EXPONENTIAL,
          py::arg("max_total_delay") = std::chrono::milliseconds(10000),
          py::arg("callback") = py::cpp_function([](auto) {}),
          py::arg("exception_handler") =
              py::cpp_function([](const std::string&) {}),
          py::arg("complete_handler") = py::cpp_function([]() {}),
          R"pbdoc(
          Creates a future for async retry execution.

          Args:
              func: The function to be executed asynchronously
              attempts_left: Number of attempts (default: 3)
              initial_delay: Initial delay between retries in milliseconds (default: 100ms)
              strategy: Backoff strategy (default: EXPONENTIAL)
              max_total_delay: Maximum total delay in milliseconds (default: 10000ms)
              callback: Callback function called on success (default: no-op)
              exception_handler: Handler called when exceptions occur (default: no-op)
              complete_handler: Handler called when all attempts complete (default: no-op)

          Returns:
              A future with the result of the async operation

          Raises:
              ValueError: If invalid parameters are provided
          )pbdoc");
}

// Declare Task and TaskPromise classes for coroutine support
void declare_task_classes(py::module& m) {
    // Task class template - we'll bind common instantiations
    py::class_<atom::async::Task<void>>(m, "TaskVoid",
                                        R"pbdoc(
        A coroutine task that represents an asynchronous operation returning void.

        This class provides C++20 coroutine support for asynchronous operations
        that don't return a value. It can be awaited and provides methods to
        check completion status and handle exceptions.
        )pbdoc")
        .def("done", &atom::async::Task<void>::done,
             R"pbdoc(
             Checks if the task has completed.

             Returns:
                 bool: True if the task has finished execution, False otherwise.
             )pbdoc")
        .def("get_result", &atom::async::Task<void>::getResult,
             R"pbdoc(
             Gets the result of the task (blocks if not complete).

             This method will block until the task completes and then return.
             If the task threw an exception, it will be re-thrown here.

             Raises:
                 Exception: Any exception that occurred during task execution.
             )pbdoc");

    py::class_<atom::async::Task<int>>(m, "TaskInt",
                                       R"pbdoc(
        A coroutine task that represents an asynchronous operation returning an integer.

        This class provides C++20 coroutine support for asynchronous operations
        that return an integer value. It can be awaited and provides methods to
        check completion status and retrieve the result.
        )pbdoc")
        .def("done", &atom::async::Task<int>::done,
             R"pbdoc(
             Checks if the task has completed.

             Returns:
                 bool: True if the task has finished execution, False otherwise.
             )pbdoc")
        .def("get_result", &atom::async::Task<int>::getResult,
             R"pbdoc(
             Gets the result of the task (blocks if not complete).

             This method will block until the task completes and then return
             the integer result.

             Returns:
                 int: The result of the asynchronous operation.

             Raises:
                 Exception: Any exception that occurred during task execution.
             )pbdoc");

    py::class_<atom::async::Task<std::string>>(m, "TaskString",
                                               R"pbdoc(
        A coroutine task that represents an asynchronous operation returning a string.

        This class provides C++20 coroutine support for asynchronous operations
        that return a string value. It can be awaited and provides methods to
        check completion status and retrieve the result.
        )pbdoc")
        .def("done", &atom::async::Task<std::string>::done,
             R"pbdoc(
             Checks if the task has completed.

             Returns:
                 bool: True if the task has finished execution, False otherwise.
             )pbdoc")
        .def("get_result", &atom::async::Task<std::string>::getResult,
             R"pbdoc(
             Gets the result of the task (blocks if not complete).

             This method will block until the task completes and then return
             the string result.

             Returns:
                 str: The result of the asynchronous operation.

             Raises:
                 Exception: Any exception that occurred during task execution.
             )pbdoc");
}

PYBIND11_MODULE(async, m) {
    m.doc() = R"pbdoc(
        Asynchronous Task Processing Module
        ----------------------------------

        This module provides tools for executing tasks asynchronously with
        features like timeouts, callbacks, task management, and priority control.

        Key components:
        - AsyncWorker: Manages a single asynchronous task with priority and CPU affinity
        - AsyncWorkerManager: Coordinates multiple async workers
        - AsyncWorkerPriority: Priority levels for task execution
        - Task/Future wrappers: Enhanced futures with additional capabilities
        - Retry mechanisms: Automatic retry with configurable backoff strategies

        Example:
            >>> from atom.async import AsyncWorkerInt, AsyncWorkerManagerInt, AsyncWorkerPriority
            >>>
            >>> # Create a worker and start a high-priority task
            >>> worker = AsyncWorkerInt()
            >>> worker.set_priority(AsyncWorkerPriority.HIGH)
            >>> worker.set_preferred_cpu(2)  # Use CPU core 2
            >>> worker.start_async(lambda: 42)
            >>>
            >>> # Get the result (with optional timeout)
            >>> result = worker.get_result(timeout=5000)  # 5 seconds timeout
            >>> print(result)  # Output: 42
            >>>
            >>> # Create a worker manager for multiple tasks
            >>> manager = AsyncWorkerManagerInt()
            >>> workers = [
            >>>     manager.create_worker(lambda: i * 10)
            >>>     for i in range(5)
            >>> ]
            >>>
            >>> # Wait for all tasks to complete
            >>> manager.wait_for_all()
            >>>
            >>> # Collect results
            >>> results = [w.get_result() for w in workers]
            >>> print(results)  # Output: [0, 10, 20, 30, 40]
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            throw py::value_error(e.what());
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
        }
    });

    // Declare exception classes
    declare_timeout_exception(m);

    // Declare platform utilities
    declare_platform_utilities(m);

    // Declare Task and TaskPromise classes
    declare_task_classes(m);

    // Register AsyncWorker Priority enum
    declare_async_worker_priority(m);

    // Declare AsyncWorker and AsyncWorkerManager for common types
    declare_async_worker<void>(m, "Void");
    declare_async_worker<bool>(m, "Bool");
    declare_async_worker<int>(m, "Int");
    declare_async_worker<double>(m, "Double");
    declare_async_worker<std::string>(m, "String");

    declare_async_worker_manager<void>(m, "Void");
    declare_async_worker_manager<bool>(m, "Bool");
    declare_async_worker_manager<int>(m, "Int");
    declare_async_worker_manager<double>(m, "Double");
    declare_async_worker_manager<std::string>(m, "String");

    // BackoffStrategy enum
    py::enum_<atom::async::BackoffStrategy>(m, "BackoffStrategy",
                                            R"pbdoc(
        Backoff strategy for retry operations.

        Defines how delays between retry attempts are calculated.
        Different strategies provide different patterns of delay growth.
        )pbdoc")
        .value("FIXED", atom::async::BackoffStrategy::FIXED,
               "Fixed delay between retries")
        .value("LINEAR", atom::async::BackoffStrategy::LINEAR,
               "Linear increase in delay between retries")
        .value("EXPONENTIAL", atom::async::BackoffStrategy::EXPONENTIAL,
               "Exponential increase in delay between retries");

    // AsyncRetry functions for different return types
    declare_async_retry<void>(m, "");  // Default no suffix for void type
    declare_async_retry<bool>(m, "_bool");
    declare_async_retry<int>(m, "_int");
    declare_async_retry<double>(m, "_double");
    declare_async_retry<std::string>(m, "_string");

    // Utility functions
    m.def(
        "get_with_timeout",
        [](py::object future, double timeout_seconds) {
            std::chrono::duration<double> timeout(timeout_seconds);

            py::object result = py::none();
            PyThreadState* _save;
            _save = PyEval_SaveThread();
            try {
                // Wait for the specified timeout
                std::this_thread::sleep_for(timeout);
                PyEval_RestoreThread(_save);

                // Check if result is ready
                if (py::hasattr(future, "done") &&
                    !future.attr("done")().cast<bool>()) {
                    throw TimeoutException(
                        __FILE__, __LINE__, __func__,
                        "Timeout occurred waiting for future");
                }

                // Get result
                if (py::hasattr(future, "result")) {
                    result = future.attr("result")();
                }
            } catch (...) {
                PyEval_RestoreThread(_save);
                throw;
            }

            return result;
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
              TimeoutException: If the timeout is reached
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
