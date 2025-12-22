#include "atom/async/pool.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <future>

namespace py = pybind11;

auto make_callable_task(const py::function& func) {
    return [func]() {
        try {
            func();
        } catch (const py::error_already_set& e) {
            // 将Python异常转换为C++异常
            throw std::runtime_error(std::string("Python exception: ") +
                                     e.what());
        } catch (...) {
            throw std::runtime_error("Unknown error in Python callable");
        }
    };
}

template <typename ReturnType = void>
auto submit_task_to_pool(atom::async::ThreadPool& pool,
                         const py::function& func) {
    return pool.submit([func]() -> ReturnType {
        try {
            py::object result = func();
            if constexpr (!std::is_same_v<ReturnType, void>) {
                return result.cast<ReturnType>();
            }
        } catch (const py::error_already_set& e) {
            throw std::runtime_error(std::string("Python exception: ") +
                                     e.what());
        } catch (...) {
            throw std::runtime_error("Unknown error in Python callable");
        }
    });
}

auto make_init_func(const py::function& init_func) {
    return [init_func](std::size_t thread_id) {
        try {
            init_func(thread_id);
        } catch (const py::error_already_set& e) {
            py::print("Thread initialization error:", e.what());
        } catch (...) {
            py::print("Unknown error during thread initialization");
        }
    };
}

PYBIND11_MODULE(pool, m) {
    m.doc() = R"pbdoc(
        High-Performance Thread Pool Module
        ----------------------------------

        This module provides a high-performance thread pool implementation with
        modern C++20 features and platform-specific optimizations.

        Features:
          - Work-stealing queue optimization for load balancing
          - CPU affinity and thread priority control
          - ASIO integration for async I/O operations
          - Enhanced futures with additional functionality
          - Batch task submission and processing
          - Dynamic thread pool sizing
          - Comprehensive performance monitoring

        The module includes:
          - ThreadPool: Main thread pool implementation
          - ThreadSafeQueue: Thread-safe queue for task management
          - Options: Configuration for thread pool behavior
          - Various utility functions for optimal performance

        Example:
            >>> from atom.async.pool import ThreadPool, ThreadPriority
            >>>
            >>> # Create a high-performance thread pool
            >>> options = ThreadPool.Options.create_high_performance()
            >>> pool = ThreadPool(options)
            >>>
            >>> # Submit tasks with different priorities
            >>> future = pool.submit(lambda: expensive_computation())
            >>> result = future.get()
    )pbdoc";

    // Register exception classes
    py::register_exception<atom::async::ThreadPoolError>(m, "ThreadPoolError",
                                                         PyExc_RuntimeError);

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::ThreadPoolError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define ThreadPriority enum
    py::enum_<atom::async::ThreadPool::Options::ThreadPriority>(
        m, "ThreadPriority",
        R"pbdoc(
        Thread priority levels for the thread pool.

        Different priority levels affect how the operating system schedules
        the threads in the pool.
        )pbdoc")
        .value("LOWEST",
               atom::async::ThreadPool::Options::ThreadPriority::Lowest,
               "Lowest thread priority")
        .value("BELOW_NORMAL",
               atom::async::ThreadPool::Options::ThreadPriority::BelowNormal,
               "Below normal thread priority")
        .value("NORMAL",
               atom::async::ThreadPool::Options::ThreadPriority::Normal,
               "Normal thread priority (default)")
        .value("ABOVE_NORMAL",
               atom::async::ThreadPool::Options::ThreadPriority::AboveNormal,
               "Above normal thread priority")
        .value("HIGHEST",
               atom::async::ThreadPool::Options::ThreadPriority::Highest,
               "Highest thread priority")
        .value("TIME_CRITICAL",
               atom::async::ThreadPool::Options::ThreadPriority::TimeCritical,
               "Time critical thread priority")
        .export_values();

    // Define CpuAffinityMode enum
    py::enum_<atom::async::ThreadPool::Options::CpuAffinityMode>(
        m, "CpuAffinityMode",
        R"pbdoc(
        CPU affinity modes for thread pool optimization.

        Controls how threads are assigned to CPU cores for optimal performance.
        )pbdoc")
        .value("NONE", atom::async::ThreadPool::Options::CpuAffinityMode::None,
               "No CPU affinity settings")
        .value("SEQUENTIAL",
               atom::async::ThreadPool::Options::CpuAffinityMode::Sequential,
               "Threads assigned to cores sequentially")
        .value("SPREAD",
               atom::async::ThreadPool::Options::CpuAffinityMode::Spread,
               "Threads spread across different cores")
        .value("CORE_PINNED",
               atom::async::ThreadPool::Options::CpuAffinityMode::CorePinned,
               "Threads pinned to specified cores")
        .value("AUTOMATIC",
               atom::async::ThreadPool::Options::CpuAffinityMode::Automatic,
               "Automatically adjust (requires hardware support)")
        .export_values();

    // Define ThreadPool Options
    py::class_<atom::async::ThreadPool::Options>(m, "ThreadPoolOptions",
                                                 R"pbdoc(
        Configuration options for the ThreadPool.

        This class contains all the configurable parameters for thread pool
        behavior, performance tuning, and platform-specific optimizations.
        )pbdoc")
        .def(py::init<>(), "Create default options")
        .def_readwrite("initial_thread_count",
                       &atom::async::ThreadPool::Options::initialThreadCount,
                       "Initial number of threads (0 = hardware concurrency)")
        .def_readwrite("max_thread_count",
                       &atom::async::ThreadPool::Options::maxThreadCount,
                       "Maximum number of threads (0 = unlimited)")
        .def_readwrite("max_queue_size",
                       &atom::async::ThreadPool::Options::maxQueueSize,
                       "Maximum queue size (0 = unlimited)")
        .def_readwrite("thread_idle_timeout",
                       &atom::async::ThreadPool::Options::threadIdleTimeout,
                       "Idle thread timeout")
        .def_readwrite("allow_thread_growth",
                       &atom::async::ThreadPool::Options::allowThreadGrowth,
                       "Allow dynamic thread creation")
        .def_readwrite("allow_thread_shrink",
                       &atom::async::ThreadPool::Options::allowThreadShrink,
                       "Allow dynamic thread reduction")
        .def_readwrite("thread_priority",
                       &atom::async::ThreadPool::Options::threadPriority,
                       "Thread priority level")
        .def_readwrite("cpu_affinity_mode",
                       &atom::async::ThreadPool::Options::cpuAffinityMode,
                       "CPU affinity mode")
        .def_readwrite(
            "pinned_cores", &atom::async::ThreadPool::Options::pinnedCores,
            "List of CPU cores for pinning (used with CORE_PINNED mode)")
        .def_readwrite("use_work_stealing",
                       &atom::async::ThreadPool::Options::useWorkStealing,
                       "Enable work stealing optimization")
        .def_readwrite("set_stack_size",
                       &atom::async::ThreadPool::Options::setStackSize,
                       "Whether to set custom stack size")
        .def_readwrite("stack_size",
                       &atom::async::ThreadPool::Options::stackSize,
                       "Custom thread stack size (0 = default)")

        // Static factory methods
        .def_static("create_default",
                    &atom::async::ThreadPool::Options::createDefault,
                    "Create default configuration")
        .def_static("create_high_performance",
                    &atom::async::ThreadPool::Options::createHighPerformance,
                    "Create high-performance configuration")
        .def_static("create_low_latency",
                    &atom::async::ThreadPool::Options::createLowLatency,
                    "Create low-latency configuration")
        .def_static("create_energy_efficient",
                    &atom::async::ThreadPool::Options::createEnergyEfficient,
                    "Create energy-efficient configuration")

#ifdef ATOM_USE_ASIO
        .def_readwrite("use_asio_context",
                       &atom::async::ThreadPool::Options::useAsioContext,
                       "Whether to use ASIO context")
        .def_static("create_asio_enabled",
                    &atom::async::ThreadPool::Options::createAsioEnabled,
                    "Create ASIO-enabled configuration")
#endif
        ;

    // ThreadSafeQueue类的绑定（简化接口）
    py::class_<atom::async::ThreadSafeQueue<py::object>>(
        m, "ThreadSafeQueue",
        R"(A thread-safe queue implementation for storing Python objects.

This queue provides thread-safe operations for adding, removing, and
manipulating elements in a multi-threaded environment.

Examples:
    >>> from atom.async import ThreadSafeQueue
    >>> queue = ThreadSafeQueue()
    >>> queue.push_back("item1")
    >>> queue.push_front("item2")
    >>> item = queue.pop_front()
)")
        .def(py::init<>(), "Constructs a new empty ThreadSafeQueue.")
        .def(
            "push_back",
            [](atom::async::ThreadSafeQueue<py::object>& self, py::object obj) {
                self.pushBack(std::move(obj));
            },
            py::arg("value"), "Adds an element to the back of the queue.")
        .def(
            "push_front",
            [](atom::async::ThreadSafeQueue<py::object>& self, py::object obj) {
                self.pushFront(std::move(obj));
            },
            py::arg("value"), "Adds an element to the front of the queue.")
        .def(
            "pop_front",
            [](atom::async::ThreadSafeQueue<py::object>& self) -> py::object {
                auto opt = self.popFront();
                if (!opt) {
                    throw py::index_error("Queue is empty");
                }
                return *opt;
            },
            "Removes and returns the element at the front of the queue.")
        .def(
            "pop_back",
            [](atom::async::ThreadSafeQueue<py::object>& self) -> py::object {
                auto opt = self.popBack();
                if (!opt) {
                    throw py::index_error("Queue is empty");
                }
                return *opt;
            },
            "Removes and returns the element at the back of the queue.")
        .def("empty", &atom::async::ThreadSafeQueue<py::object>::empty,
             "Checks if the queue is empty.")
        .def("size", &atom::async::ThreadSafeQueue<py::object>::size,
             "Returns the number of elements in the queue.")
        .def("clear", &atom::async::ThreadSafeQueue<py::object>::clear,
             "Removes all elements from the queue.")
        .def("__len__", &atom::async::ThreadSafeQueue<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::ThreadSafeQueue<py::object>& self) {
                return !self.empty();
            },
            "Support for boolean evaluation.");

    // ThreadPool类的绑定
    py::class_<atom::async::ThreadPool>(
        m, "ThreadPool",
        R"(A high-performance thread pool for parallel task execution.

This thread pool efficiently distributes tasks across multiple threads, supporting
work stealing and priority-based scheduling.

Args:
    num_threads: Number of threads to create (default: hardware concurrency)
    init_func: Optional function to initialize each thread (receives thread_id)

Examples:
    >>> from atom.async import ThreadPool
    >>> # Create a thread pool with 4 threads
    >>> pool = ThreadPool(4)
    >>> # Submit a task that returns a value
    >>> future = pool.submit(lambda: 42)
    >>> result = future.result()
    >>> print(result)  # Output: 42
)")
        .def(py::init<>(), "Create ThreadPool with default options")
        .def(py::init<const atom::async::ThreadPool::Options&>(),
             py::arg("options"), "Create ThreadPool with custom options")
        .def(
            py::init([](unsigned int num_threads, py::function init_func) {
                atom::async::ThreadPool::Options options;

                if (num_threads > 0) {
                    options.initialThreadCount = num_threads;
                }

                auto pool = std::make_unique<atom::async::ThreadPool>(options);

                if (!py::isinstance<py::none>(init_func)) {
                    py::print(
                        "Warning: Thread initialization function is provided "
                        "but not supported by ThreadPool class");
                }

                return pool;
            }),
            py::arg("num_threads") = 0, py::arg("init_func") = py::none(),
            "Constructs a new ThreadPool with the specified number of threads.")
        .def("size", &atom::async::ThreadPool::getThreadCount,
             "Returns the number of threads in the pool.")
        .def("active_task_count",
             &atom::async::ThreadPool::getActiveThreadCount,
             "Returns the number of currently active tasks.")
        .def(
            "is_shutting_down",
            [](const atom::async::ThreadPool& pool) {
                return pool.isShutdown();
            },
            "Checks if the thread pool is in the process of shutting down.")
        .def("wait_for_tasks", &atom::async::ThreadPool::waitForTasks,
             R"(Waits for all queued tasks to complete.

Returns:
    True if all tasks completed, False if timed out
)")
        .def(
            "submit",
            [](atom::async::ThreadPool& pool, py::function func) {
                auto future = submit_task_to_pool<py::object>(pool, func);

                auto py_future =
                    py::module::import("concurrent.futures").attr("Future")();

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
            py::arg("func"),
            R"(Submits a task to the thread pool.

Args:
    func: Callable to execute in the thread pool

Returns:
    A Future object that can be used to retrieve the result

Examples:
    >>> def task():
    ...     return 42
    >>> future = pool.submit(task)
    >>> result = future.result()
    >>> print(result)  # Output: 42
)")
        .def(
            "submit_batch",
            [](atom::async::ThreadPool& pool, py::list tasks) {
                // 创建Python Future列表
                py::list futures;

                for (auto task : tasks) {
                    py::function func = py::cast<py::function>(task);

                    // 为每个任务创建future
                    auto future = submit_task_to_pool<py::object>(pool, func);
                    auto py_future = py::module::import("concurrent.futures")
                                         .attr("Future")();

                    // 设置回调
                    std::thread([future = std::move(future),
                                 py_future]() mutable {
                        try {
                            py::object result = future.get();
                            py_future.attr("set_result")(result);
                        } catch (const std::exception& e) {
                            py_future.attr("set_exception")(py::str(e.what()));
                        }
                    }).detach();

                    futures.append(py_future);
                }

                return futures;
            },
            py::arg("tasks"),
            R"(Submits multiple tasks to the thread pool.

Args:
    tasks: List of callables to execute in the thread pool

Returns:
    A list of Future objects corresponding to the submitted tasks

Examples:
    >>> tasks = [lambda: i*i for i in range(5)]
    >>> futures = pool.submit_batch(tasks)
    >>> results = [f.result() for f in futures]
    >>> print(results)  # Output: [0, 1, 4, 9, 16]
)")
        .def(
            "submit_detached",
            [](atom::async::ThreadPool& pool, py::function func) {
                pool.enqueueDetach(make_callable_task(func));
            },
            py::arg("func"),
            R"(Submits a task to the thread pool without returning a future.

This method is used when you don't need to wait for the task to complete
or retrieve its result.

Args:
    func: Callable to execute in the thread pool

Examples:
    >>> pool.submit_detached(lambda: print("Running in background"))
)")

        // Enhanced methods from C++ interface
        .def(
            "submit_with_promise",
            [](atom::async::ThreadPool& pool, py::function func) {
                auto future = pool.submitWithPromise([func]() -> py::object {
                    py::gil_scoped_acquire acquire;
                    return func();
                });

                // Convert Promise to Python future-like object
                auto py_future =
                    py::module::import("concurrent.futures").attr("Future")();

                std::thread([future = std::move(future), py_future]() mutable {
                    try {
                        py::gil_scoped_acquire acquire;
                        py::object result = future.get();
                        py_future.attr("set_result")(result);
                    } catch (const std::exception& e) {
                        py::gil_scoped_acquire acquire;
                        py_future.attr("set_exception")(py::str(e.what()));
                    }
                }).detach();

                return py_future;
            },
            py::arg("func"),
            R"(Submit a task using Promise-based execution.

Args:
    func: Callable to execute in the thread pool

Returns:
    A Future object that can be used to retrieve the result

Examples:
    >>> future = pool.submit_with_promise(lambda: compute_result())
    >>> result = future.result()
)")

        .def("shutdown", &atom::async::ThreadPool::shutdown,
             R"(Shutdown the thread pool and wait for all tasks to complete.)")

        .def("get_queue_size", &atom::async::ThreadPool::getQueueSize,
             R"(Get the current number of queued tasks.)")

        .def("get_statistics", &atom::async::ThreadPool::getStatistics,
             R"(Get thread pool performance statistics.)")

#ifdef ATOM_USE_ASIO
        .def(
            "get_asio_context",
            [](atom::async::ThreadPool& pool) -> py::object {
                auto* context = pool.getAsioContext();
                if (context) {
                    return py::cast(context,
                                    py::return_value_policy::reference);
                }
                return py::none();
            },
            R"(Get the underlying ASIO context if available.

Returns:
    ASIO context or None if not using ASIO
)")
#endif
        ;

    m.def(
        "create_thread_pool",
        [](py::kwargs kwargs) {
            atom::async::ThreadPool::Options options;
            py::function init_func = py::none();

            if (kwargs.contains("num_threads")) {
                unsigned int num_threads =
                    kwargs["num_threads"].cast<unsigned int>();
                if (num_threads > 0) {
                    options.initialThreadCount = num_threads;
                }
            }

            if (kwargs.contains("init_func")) {
                init_func = kwargs["init_func"].cast<py::function>();
            }

            auto pool = std::make_unique<atom::async::ThreadPool>(options);

            if (!py::isinstance<py::none>(init_func)) {
                py::print(
                    "Warning: Thread initialization function is provided "
                    "but not supported by ThreadPool class");
            }

            return pool;
        },
        R"(Factory function to create a thread pool with optimal configuration.

This function provides a more flexible way to create thread pools with
various configurations.

Keyword Args:
    num_threads: Number of threads to create (default: hardware concurrency)
    init_func: Function to initialize each thread (default: None)

Returns:
    A configured ThreadPool instance

Examples:
    >>> from atom.async import create_thread_pool
    >>> # Create a thread pool with default configuration
    >>> pool = create_thread_pool()
    >>> # Create a thread pool with custom initialization
    >>> def init_thread(thread_id):
    ...     print(f"Initializing thread {thread_id}")
    >>> pool = create_thread_pool(num_threads=4, init_func=init_thread)
)");

    // Utility functions
    m.def(
         "hardware_concurrency",
         []() { return std::thread::hardware_concurrency(); },
         "Returns the number of concurrent threads supported by the hardware.")

        .def(
            "create_optimized_pool",
            [](const std::string& profile)
                -> std::unique_ptr<atom::async::ThreadPool> {
                atom::async::ThreadPool::Options options;

                if (profile == "high_performance") {
                    options = atom::async::ThreadPool::Options::
                        createHighPerformance();
                } else if (profile == "low_latency") {
                    options =
                        atom::async::ThreadPool::Options::createLowLatency();
                } else if (profile == "energy_efficient") {
                    options = atom::async::ThreadPool::Options::
                        createEnergyEfficient();
#ifdef ATOM_USE_ASIO
                } else if (profile == "asio_enabled") {
                    options =
                        atom::async::ThreadPool::Options::createAsioEnabled();
#endif
                } else {
                    options = atom::async::ThreadPool::Options::createDefault();
                }

                return std::make_unique<atom::async::ThreadPool>(options);
            },
            py::arg("profile") = "default",
            R"pbdoc(
        Create an optimized thread pool with predefined configuration.

        Args:
            profile: Configuration profile ("default", "high_performance",
                    "low_latency", "energy_efficient", "asio_enabled")

        Returns:
            A ThreadPool configured for the specified profile

        Examples:
            >>> pool = create_optimized_pool("high_performance")
            >>> pool = create_optimized_pool("low_latency")
        )pbdoc")

        .def(
            "benchmark_thread_pool",
            [](size_t num_tasks, size_t num_threads) -> py::dict {
                using namespace std::chrono;

                auto options =
                    atom::async::ThreadPool::Options::createHighPerformance();
                options.initialThreadCount = num_threads;
                atom::async::ThreadPool pool(options);

                py::dict results;
                std::vector<atom::async::EnhancedFuture<int>> futures;

                auto start_time = high_resolution_clock::now();

                // Submit tasks
                for (size_t i = 0; i < num_tasks; ++i) {
                    auto future = pool.submit([i]() -> int {
                        // Simulate some work
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(100));
                        return static_cast<int>(i * i);
                    });
                    futures.push_back(std::move(future));
                }

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    future.get();
                }

                auto end_time = high_resolution_clock::now();
                auto total_duration =
                    duration_cast<milliseconds>(end_time - start_time);

                results["num_tasks"] = num_tasks;
                results["num_threads"] = num_threads;
                results["total_time_ms"] = total_duration.count();
                results["tasks_per_second"] =
                    (num_tasks * 1000.0) / total_duration.count();
                results["active_threads"] = pool.getActiveThreadCount();
                results["queue_size"] = pool.getQueueSize();

                return results;
            },
            py::arg("num_tasks") = 1000,
            py::arg("num_threads") = std::thread::hardware_concurrency(),
            R"pbdoc(
        Benchmark thread pool performance.

        Args:
            num_tasks: Number of tasks to execute (default: 1000)
            num_threads: Number of threads to use (default: hardware concurrency)

        Returns:
            dict: Benchmark results including timing and throughput

        Examples:
            >>> results = benchmark_thread_pool(5000, 8)
            >>> print(f"Tasks per second: {results['tasks_per_second']}")
        )pbdoc")

        .def(
            "parallel_map",
            [](py::function func, py::list items,
               size_t num_threads) -> py::list {
                auto options =
                    atom::async::ThreadPool::Options::createHighPerformance();
                options.initialThreadCount = num_threads;
                atom::async::ThreadPool pool(options);

                std::vector<atom::async::EnhancedFuture<py::object>> futures;

                // Submit all tasks
                for (auto item : items) {
                    auto future = pool.submit([func, item]() -> py::object {
                        py::gil_scoped_acquire acquire;
                        return func(item);
                    });
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

                return results;
            },
            py::arg("func"), py::arg("items"),
            py::arg("num_threads") = std::thread::hardware_concurrency(),
            R"pbdoc(
        Apply a function to all items in parallel.

        Args:
            func: Function to apply to each item
            items: List of items to process
            num_threads: Number of threads to use (default: hardware concurrency)

        Returns:
            list: Results from applying func to each item

        Examples:
            >>> items = list(range(100))
            >>> results = parallel_map(lambda x: x*x, items, 4)
            >>> print(len(results))  # 100
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO") = true;
#else
    m.attr("HAS_ASIO") = false;
#endif

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
