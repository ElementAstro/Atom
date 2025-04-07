#include "atom/async/async_executor.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(async_executor, m) {
    m.doc() = R"pbdoc(
        Advanced Async Task Executor
        ---------------------------

        This module provides a high-performance asynchronous task executor with 
        thread pooling, priority-based scheduling, and multiple execution strategies.
        
        The module includes:
          - Thread pool with dynamic resizing
          - Priority-based task scheduling (LOW, NORMAL, HIGH, CRITICAL)
          - Various execution strategies (IMMEDIATE, DEFERRED, SCHEDULED)
          - Task cancellation support
          - Wait for completion functionality
          
        Example:
            >>> from atom.async.async_executor import AsyncExecutor, ExecutionStrategy, TaskPriority
            >>> 
            >>> # Create an executor with 4 threads
            >>> executor = AsyncExecutor(4)
            >>> 
            >>> # Schedule a task for immediate execution with normal priority
            >>> future = executor.schedule(
            >>>     ExecutionStrategy.IMMEDIATE, 
            >>>     TaskPriority.NORMAL,
            >>>     lambda x: x * 2, 
            >>>     10
            >>> )
            >>> 
            >>> # Get the result when ready
            >>> result = future.result()
            >>> print(result)  # Outputs: 20
            >>>
            >>> # Schedule multiple tasks with different priorities
            >>> futures = []
            >>> for i in range(10):
            >>>     priority = TaskPriority.HIGH if i % 2 == 0 else TaskPriority.LOW
            >>>     futures.append(executor.schedule(
            >>>         ExecutionStrategy.IMMEDIATE,
            >>>         priority,
            >>>         lambda x: x * x,
            >>>         i
            >>>     ))
            >>>
            >>> # Wait for all tasks to complete
            >>> executor.wait_for_all()
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the task priority enum
    py::enum_<atom::async::ExecutorTask::Priority>(
        m, "TaskPriority", "Task priority levels for the async executor")
        .value(
            "LOW", atom::async::ExecutorTask::Priority::LOW,
            "Low priority tasks will be executed after higher priority tasks")
        .value("NORMAL", atom::async::ExecutorTask::Priority::NORMAL,
               "Normal priority for most tasks")
        .value(
            "HIGH", atom::async::ExecutorTask::Priority::HIGH,
            "High priority tasks will be executed before lower priority tasks")
        .value("CRITICAL", atom::async::ExecutorTask::Priority::CRITICAL,
               "Critical priority tasks are executed first")
        .export_values();

    // Define the execution strategy enum
    py::enum_<atom::async::AsyncExecutor::ExecutionStrategy>(
        m, "ExecutionStrategy", "Execution strategies for the async executor")
        .value("IMMEDIATE",
               atom::async::AsyncExecutor::ExecutionStrategy::IMMEDIATE,
               "Execute immediately in the thread pool")
        .value("DEFERRED",
               atom::async::AsyncExecutor::ExecutionStrategy::DEFERRED,
               "Execute when explicitly requested")
        .value("SCHEDULED",
               atom::async::AsyncExecutor::ExecutionStrategy::SCHEDULED,
               "Execute at a specified time")
        .export_values();

    // Define the ThreadPool class
    py::class_<atom::async::ThreadPool>(
        m, "ThreadPool", "Thread pool with priority-based task execution")
        .def(py::init<size_t>(), py::arg("num_threads") = py::none(),
             "Constructs a ThreadPool with a specified number of threads "
             "(default: hardware concurrency)")
        .def("queue_size", &atom::async::ThreadPool::queueSize,
             "Get the number of tasks waiting in the queue")
        .def("active_task_count", &atom::async::ThreadPool::activeTaskCount,
             "Get the number of active tasks currently being processed")
        .def("size", &atom::async::ThreadPool::size,
             "Get the number of threads in the pool")
        .def("resize", &atom::async::ThreadPool::resize, py::arg("num_threads"),
             "Resize the thread pool to a specified number of threads")
        .def("clear_queue", &atom::async::ThreadPool::clearQueue,
             "Clear all pending tasks from the queue and return the number "
             "removed")
        .def("wait_for_all", &atom::async::ThreadPool::waitForAll,
             "Wait for all tasks to complete");

    // Define the AsyncExecutor class
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
        .def(py::init<size_t>(), py::arg("pool_size") = py::none(),
             "Constructs an AsyncExecutor with a specified thread pool size "
             "(default: hardware concurrency)")
        .def(
            "schedule",
            [](atom::async::AsyncExecutor& self,
               atom::async::AsyncExecutor::ExecutionStrategy strategy,
               atom::async::ExecutorTask::Priority priority, py::function func,
               py::args args) {
                return self.schedule(strategy, priority, [func, args]() {
                    py::gil_scoped_acquire acquire;
                    return func(*args).cast<py::object>();
                });
            },
            py::arg("strategy"), py::arg("priority"), py::arg("func"),
            R"(Schedule a task for execution with the specified strategy and priority.

Args:
    strategy: Execution strategy (IMMEDIATE, DEFERRED, or SCHEDULED)
    priority: Task priority (LOW, NORMAL, HIGH, or CRITICAL)
    func: Function to execute
    *args: Arguments to pass to the function

Returns:
    Future object that will contain the result of the task

Examples:
    >>> future = executor.schedule(
    >>>     ExecutionStrategy.IMMEDIATE,
    >>>     TaskPriority.HIGH,
    >>>     lambda x, y: x + y,
    >>>     10, 20
    >>> )
    >>> result = future.result()  # This will be 30
)")
        .def("execute_deferred_tasks",
             &atom::async::AsyncExecutor::executeDeferredTasks,
             "Execute all deferred tasks")
        .def("wait_for_all", &atom::async::AsyncExecutor::waitForAll,
             "Wait for all tasks to complete, including deferred tasks")
        .def("queue_size", &atom::async::AsyncExecutor::queueSize,
             "Get the number of tasks waiting in the queue")
        .def("active_task_count", &atom::async::AsyncExecutor::activeTaskCount,
             "Get the number of active tasks currently being processed")
        .def("resize", &atom::async::AsyncExecutor::resize,
             py::arg("pool_size"),
             "Resize the thread pool to a specified size");

    // Define a convenience function for getting hardware concurrency
    m.def(
        "get_hardware_concurrency",
        []() { return std::thread::hardware_concurrency(); },
        "Get the number of hardware threads available on the system");

    // Add version information
    m.attr("__version__") = "1.0.0";
}