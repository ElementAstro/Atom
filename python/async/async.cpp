#include "atom/async/async.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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
                self.startAsync([func]() -> ResultType {
                    py::gil_scoped_acquire acquire;
                    return func().cast<ResultType>();
                });
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
                    return func().cast<ResultType>();
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
             "Removes completed workers from the manager and returns the "
             "number removed.");
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
            func();
            return;
        } else {
            return func().cast<ResultType>();
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
                      callback();
                  } else {
                      callback(result);
                  }
              };
              auto py_exception_handler =
                  [exception_handler](const std::exception& e) {
                      py::gil_scoped_acquire acquire;
                      exception_handler(e.what());
                  };
              auto py_complete_handler = [complete_handler]() {
                  py::gil_scoped_acquire acquire;
                  complete_handler();
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

PYBIND11_MODULE(async, m) {
    m.doc() = R"pbdoc(
        Asynchronous Task Processing Module
        ----------------------------------

        This module provides tools for executing tasks asynchronously with 
        features like timeouts, callbacks, and task management.
        
        Key components:
        - AsyncWorker: Manages a single asynchronous task
        - AsyncWorkerManager: Coordinates multiple async workers
        - Task/Future wrappers: Enhanced futures with additional capabilities
        - Retry mechanisms: Automatic retry with configurable backoff strategies
        
        Example:
            >>> from atom.async import AsyncWorkerInt, AsyncWorkerManagerInt
            >>> 
            >>> # Create a worker and start a task
            >>> worker = AsyncWorkerInt()
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
    py::register_exception<TimeoutException>(m, "TimeoutException");

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const TimeoutException& e) {
            PyErr_SetString(PyExc_TimeoutError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Declare AsyncWorker and AsyncWorkerManager for common types
    declare_async_worker<void>(m, "Void");
    declare_async_worker<bool>(m, "Bool");
    declare_async_worker<int>(m, "Int");
    declare_async_worker<double>(m, "Double");
    declare_async_worker<std::string>(m, "String");

    // BackoffStrategy enum
    declare_backoff_strategy(m);

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