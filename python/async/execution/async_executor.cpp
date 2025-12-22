#include "atom/async/async_executor.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(async_executor, m) {
    m.doc() = R"pbdoc(
        Advanced Async Task Executor
        ---------------------------

        This module provides a high-performance asynchronous task executor with
        thread pooling, priority-based scheduling, and work-stealing queues.

        The module includes:
          - Thread pool with configurable min/max threads
          - Priority-based task scheduling (Low, Normal, High, Critical)
          - Work-stealing queue optimization for load balancing
          - Task execution monitoring and statistics
          - Exception handling with source location information

        Example:
            >>> from atom.async.async_executor import AsyncExecutor, TaskPriority
            >>>
            >>> # Create an executor with default configuration
            >>> executor = AsyncExecutor()
            >>> executor.start()
            >>>
            >>> # Execute a task with normal priority
            >>> future = executor.execute(lambda: 42, TaskPriority.NORMAL)
            >>> result = future.get()
            >>> print(result)  # Outputs: 42
            >>>
            >>> # Execute multiple tasks with different priorities
            >>> futures = []
            >>> for i in range(10):
            >>>     priority = TaskPriority.HIGH if i % 2 == 0 else TaskPriority.LOW
            >>>     future = executor.execute(lambda x=i: x * x, priority)
            >>>     futures.append(future)
            >>>
            >>> # Collect results
            >>> results = [f.get() for f in futures]
            >>> print(results)
            >>>
            >>> # Stop the executor
            >>> executor.stop()
    )pbdoc";

    // Register exception classes
    py::register_exception<atom::async::ExecutorException>(
        m, "ExecutorException", PyExc_RuntimeError);
    py::register_exception<atom::async::TaskException>(m, "TaskException",
                                                       PyExc_RuntimeError);

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::TaskException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::async::ExecutorException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the task priority enum
    py::enum_<atom::async::AsyncExecutor::Priority>(m, "TaskPriority",
                                                    R"pbdoc(
        Task priority levels for the async executor.

        Higher priority tasks are executed before lower priority tasks.
        The numeric values indicate relative priority weights.
        )pbdoc")
        .value("LOW", atom::async::AsyncExecutor::Priority::Low,
               "Low priority tasks (priority weight: 0)")
        .value("NORMAL", atom::async::AsyncExecutor::Priority::Normal,
               "Normal priority tasks (priority weight: 50, default)")
        .value("HIGH", atom::async::AsyncExecutor::Priority::High,
               "High priority tasks (priority weight: 100)")
        .value("CRITICAL", atom::async::AsyncExecutor::Priority::Critical,
               "Critical priority tasks (priority weight: 200)")
        .export_values();

    // Define the Configuration struct
    py::class_<atom::async::AsyncExecutor::Configuration>(m, "Configuration",
                                                          R"pbdoc(
        Configuration options for the AsyncExecutor.

        This struct contains all the configurable parameters for the thread pool
        and task execution behavior.
        )pbdoc")
        .def(py::init<>(), "Create default configuration")
        .def_readwrite("min_threads",
                       &atom::async::AsyncExecutor::Configuration::minThreads,
                       "Minimum number of threads in the pool")
        .def_readwrite("max_threads",
                       &atom::async::AsyncExecutor::Configuration::maxThreads,
                       "Maximum number of threads in the pool")
        .def_readwrite(
            "queue_size_per_thread",
            &atom::async::AsyncExecutor::Configuration::queueSizePerThread,
            "Queue size per thread for work-stealing")
        .def_readwrite(
            "thread_idle_timeout",
            &atom::async::AsyncExecutor::Configuration::threadIdleTimeout,
            "Timeout before idle threads are terminated")
        .def_readwrite("set_priority",
                       &atom::async::AsyncExecutor::Configuration::setPriority,
                       "Whether to set thread priority")
        .def_readwrite(
            "thread_priority",
            &atom::async::AsyncExecutor::Configuration::threadPriority,
            "Thread priority (platform-dependent)")
        .def_readwrite("pin_threads",
                       &atom::async::AsyncExecutor::Configuration::pinThreads,
                       "Whether to pin threads to CPU cores")
        .def_readwrite(
            "use_work_stealing",
            &atom::async::AsyncExecutor::Configuration::useWorkStealing,
            "Enable work-stealing optimization")
        .def_readwrite("stat_interval",
                       &atom::async::AsyncExecutor::Configuration::statInterval,
                       "Statistics collection interval");

    // AsyncExecutor类绑定
    py::class_<atom::async::AsyncExecutor>(
        m, "AsyncExecutor",
        R"(High-level executor for asynchronous tasks with various execution strategies.

This class provides a convenient interface for executing tasks asynchronously
with different execution strategies and priorities.

Args:
    pool_size: Size of the underlying thread pool. Default is hardware concurrency.

Examples:
    >>> executor = AsyncExecutor(4)  # Create an executor with 4 threads
    >>>
    >>> # Schedule an immediate task
    >>> future = executor.schedule(
    >>>     ExecutionStrategy.IMMEDIATE,
    >>>     TaskPriority.NORMAL,
    >>>     lambda x: x * 2,
    >>>     10
    >>> )
    >>>
    >>> # Wait for the result
    >>> result = future.result()
)")
        .def(py::init<>(), "Create executor with default configuration")
        .def(py::init<const atom::async::AsyncExecutor::Configuration&>(),
             py::arg("config"), "Create executor with custom configuration")
        .def("start", &atom::async::AsyncExecutor::start,
             R"pbdoc(
             Start the thread pool.

             This must be called before executing any tasks.
             )pbdoc")
        .def("stop", &atom::async::AsyncExecutor::stop,
             R"pbdoc(
             Stop the thread pool and wait for all threads to finish.

             After calling this, no new tasks can be executed.
             )pbdoc")
        .def("is_running", &atom::async::AsyncExecutor::isRunning,
             R"pbdoc(
             Check if the executor is currently running.

             Returns:
                 bool: True if the executor is running, False otherwise.
             )pbdoc")
        .def(
            "execute",
            [](atom::async::AsyncExecutor& self, py::function func,
               atom::async::AsyncExecutor::Priority priority) {
                return self.execute(
                    [func]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        return func();
                    },
                    priority);
            },
            py::arg("func"),
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            R"pbdoc(
            Execute a function asynchronously and return a future.

            Args:
                func: The function to execute
                priority: Task priority (default: Normal)

            Returns:
                std::future: A future that will contain the result

            Examples:
                >>> future = executor.execute(lambda: 42)
                >>> result = future.get()  # 42
            )pbdoc")
        .def(
            "execute_void",
            [](atom::async::AsyncExecutor& self, py::function func,
               atom::async::AsyncExecutor::Priority priority) {
                self.execute(
                    [func]() {
                        py::gil_scoped_acquire acquire;
                        func();
                    },
                    priority);
            },
            py::arg("func"),
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            R"pbdoc(
            Execute a void function asynchronously.

            Args:
                func: The function to execute (should not return a value)
                priority: Task priority (default: Normal)

            Examples:
                >>> executor.execute_void(lambda: print("Hello World"))
            )pbdoc")
        .def("get_active_thread_count",
             &atom::async::AsyncExecutor::getActiveThreadCount,
             R"pbdoc(
             Get the number of currently active threads.

             Returns:
                 size_t: Number of threads currently executing tasks.
             )pbdoc")
        .def("get_pending_task_count",
             &atom::async::AsyncExecutor::getPendingTaskCount,
             R"pbdoc(
             Get the number of tasks waiting to be executed.

             Returns:
                 size_t: Number of pending tasks in the queue.
             )pbdoc")
        .def("get_completed_task_count",
             &atom::async::AsyncExecutor::getCompletedTaskCount,
             R"pbdoc(
             Get the total number of completed tasks.

             Returns:
                 size_t: Total number of tasks that have been completed.
             )pbdoc")

        // Static methods for global instance
        .def_static(
            "submit",
            [](py::function func,
               atom::async::AsyncExecutor::Priority priority) {
                return atom::async::AsyncExecutor::submit(
                    [func]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        return func();
                    },
                    priority);
            },
            py::arg("func"),
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            R"pbdoc(
            Submit a task to the global executor instance.

            Args:
                func: The function to execute
                priority: Task priority (default: Normal)

            Returns:
                std::future: A future that will contain the result

            Examples:
                >>> future = AsyncExecutor.submit(lambda: 42)
                >>> result = future.get()  # 42
            )pbdoc")
        .def_static("get_instance", &atom::async::AsyncExecutor::getInstance,
                    py::return_value_policy::reference,
                    R"pbdoc(
            Get the global executor instance.

            Returns:
                AsyncExecutor: Reference to the global executor instance

            Examples:
                >>> global_executor = AsyncExecutor.get_instance()
                >>> global_executor.start()
            )pbdoc")

        // Enhanced execution methods with better error handling
        .def(
            "execute_with_timeout",
            [](atom::async::AsyncExecutor& self, py::function func,
               atom::async::AsyncExecutor::Priority priority,
               std::chrono::milliseconds timeout) {
                auto future = self.execute(
                    [func]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        return func();
                    },
                    priority);

                if (future.wait_for(timeout) == std::future_status::timeout) {
                    throw py::value_error("Task execution timed out");
                }

                return future.get();
            },
            py::arg("func"),
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            py::arg("timeout") = std::chrono::milliseconds(5000),
            R"pbdoc(
            Execute a function with a timeout.

            Args:
                func: The function to execute
                priority: Task priority (default: Normal)
                timeout: Maximum time to wait for completion (default: 5000ms)

            Returns:
                The result of the function

            Raises:
                ValueError: If the task times out

            Examples:
                >>> result = executor.execute_with_timeout(lambda: slow_function(), timeout=1000)
            )pbdoc");

    // Utility functions
    m.def(
         "get_hardware_concurrency",
         []() -> unsigned int { return std::thread::hardware_concurrency(); },
         R"pbdoc(
        Get the number of hardware threads available on the system.

        Returns:
            int: Number of concurrent threads supported by the implementation.
        )pbdoc")

        .def(
            "create_default_config",
            []() -> atom::async::AsyncExecutor::Configuration {
                return atom::async::AsyncExecutor::Configuration{};
            },
            R"pbdoc(
        Create a default configuration for AsyncExecutor.

        Returns:
            Configuration: A configuration object with default values

        Examples:
            >>> config = create_default_config()
            >>> config.max_threads = 8
            >>> executor = AsyncExecutor(config)
        )pbdoc")

        .def(
            "create_optimized_config",
            [](size_t thread_count)
                -> atom::async::AsyncExecutor::Configuration {
                auto config = atom::async::AsyncExecutor::Configuration{};
                config.minThreads = std::max<size_t>(2, thread_count / 2);
                config.maxThreads = thread_count;
                config.useWorkStealing = true;
                config.setPriority = true;
                return config;
            },
            py::arg("thread_count") = std::thread::hardware_concurrency(),
            R"pbdoc(
        Create an optimized configuration for AsyncExecutor.

        Args:
            thread_count: Number of threads to use (default: hardware concurrency)

        Returns:
            Configuration: An optimized configuration object

        Examples:
            >>> config = create_optimized_config(8)
            >>> executor = AsyncExecutor(config)
        )pbdoc")

        .def(
            "benchmark_executor",
            [](size_t num_tasks,
               atom::async::AsyncExecutor::Priority priority) -> py::dict {
                using namespace std::chrono;

                auto config = atom::async::AsyncExecutor::Configuration{};
                config.maxThreads = std::thread::hardware_concurrency();
                atom::async::AsyncExecutor executor(config);
                executor.start();

                py::dict results;
                std::vector<std::future<int>> futures;

                auto start_time = high_resolution_clock::now();

                // Submit tasks
                for (size_t i = 0; i < num_tasks; ++i) {
                    auto future = executor.execute(
                        [i]() -> int {
                            // Simulate some work
                            std::this_thread::sleep_for(
                                std::chrono::microseconds(100));
                            return static_cast<int>(i * i);
                        },
                        priority);
                    futures.push_back(std::move(future));
                }

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    future.get();
                }

                auto end_time = high_resolution_clock::now();
                auto total_duration =
                    duration_cast<milliseconds>(end_time - start_time);

                executor.stop();

                results["num_tasks"] = num_tasks;
                results["total_time_ms"] = total_duration.count();
                results["tasks_per_second"] =
                    (num_tasks * 1000.0) / total_duration.count();
                results["completed_tasks"] = executor.getCompletedTaskCount();

                return results;
            },
            py::arg("num_tasks") = 1000,
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            R"pbdoc(
        Benchmark the executor performance.

        Args:
            num_tasks: Number of tasks to execute (default: 1000)
            priority: Task priority to use (default: Normal)

        Returns:
            dict: Benchmark results including timing and throughput

        Examples:
            >>> results = benchmark_executor(5000, TaskPriority.HIGH)
            >>> print(f"Tasks per second: {results['tasks_per_second']}")
        )pbdoc")

        .def(
            "execute_parallel_tasks",
            [](py::list tasks,
               atom::async::AsyncExecutor::Priority priority) -> py::list {
                auto config = atom::async::AsyncExecutor::Configuration{};
                atom::async::AsyncExecutor executor(config);
                executor.start();

                std::vector<std::future<py::object>> futures;

                // Submit all tasks
                for (auto task : tasks) {
                    auto func = task.cast<py::function>();
                    auto future = executor.execute(
                        [func]() -> py::object {
                            py::gil_scoped_acquire acquire;
                            return func();
                        },
                        priority);
                    futures.push_back(std::move(future));
                }

                // Collect results
                py::list results;
                for (auto& future : futures) {
                    try {
                        results.append(future.get());
                    } catch (const std::exception& e) {
                        results.append(
                            py::str("Error: " + std::string(e.what())));
                    }
                }

                executor.stop();
                return results;
            },
            py::arg("tasks"),
            py::arg("priority") = atom::async::AsyncExecutor::Priority::Normal,
            R"pbdoc(
        Execute multiple tasks in parallel and return results.

        Args:
            tasks: List of functions to execute
            priority: Task priority to use (default: Normal)

        Returns:
            list: Results from all tasks in the same order

        Examples:
            >>> tasks = [lambda: i*i for i in range(10)]
            >>> results = execute_parallel_tasks(tasks)
            >>> print(results)  # [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";
    m.attr("CACHE_LINE_SIZE") = ATOM_CACHE_LINE_SIZE;

#ifdef ATOM_PLATFORM_WINDOWS
    m.attr("PLATFORM") = "Windows";
#elif defined(ATOM_PLATFORM_APPLE)
    m.attr("PLATFORM") = "macOS";
#elif defined(ATOM_PLATFORM_LINUX)
    m.attr("PLATFORM") = "Linux";
#else
    m.attr("PLATFORM") = "Unknown";
#endif
}
