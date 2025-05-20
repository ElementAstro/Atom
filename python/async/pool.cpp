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
    m.doc() = "Thread pool module for the atom package";

    // 注册异常转换
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

    // 注册ThreadPoolError异常
    py::register_exception<atom::async::ThreadPoolError>(m, "ThreadPoolError",
                                                         PyExc_RuntimeError);

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
                if (!opt.has_value()) {
                    throw py::index_error("Queue is empty");
                }
                return opt.value();
            },
            "Removes and returns the element at the front of the queue.")
        .def(
            "pop_back",
            [](atom::async::ThreadSafeQueue<py::object>& self) -> py::object {
                auto opt = self.popBack();
                if (!opt.has_value()) {
                    throw py::index_error("Queue is empty");
                }
                return opt.value();
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
)");

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

    m.def(
        "hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        "Returns the number of concurrent threads supported by the hardware.");
}