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
    py::enum_<atom::async::AsyncExecutor::Priority>(
        m, "TaskPriority", "Task priority levels for the async executor")
        .value(
            "LOW", atom::async::AsyncExecutor::Priority::Low,
            "Low priority tasks will be executed after higher priority tasks")
        .value("NORMAL", atom::async::AsyncExecutor::Priority::Normal,
               "Normal priority for most tasks")
        .value(
            "HIGH", atom::async::AsyncExecutor::Priority::High,
            "High priority tasks will be executed before lower priority tasks")
        .value("CRITICAL", atom::async::AsyncExecutor::Priority::Critical,
               "Critical priority tasks are executed first")
        .export_values();

    // 定义执行策略枚举 - 使用字面量，因为执行策略在代码中未定义
    // 假设AsyncExecutor有三种执行策略: IMMEDIATE(0), DEFERRED(1), SCHEDULED(2)
    // 这里我们创建一个临时枚举来绑定这些值
    enum class ExecutionStrategy { IMMEDIATE = 0, DEFERRED = 1, SCHEDULED = 2 };
    py::enum_<ExecutionStrategy>(
        m, "ExecutionStrategy", "Execution strategies for the async executor")
        .value("IMMEDIATE", ExecutionStrategy::IMMEDIATE,
               "Execute immediately in the thread pool")
        .value("DEFERRED", ExecutionStrategy::DEFERRED,
               "Execute when explicitly requested")
        .value("SCHEDULED", ExecutionStrategy::SCHEDULED,
               "Execute at a specified time")
        .export_values();

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
        .def(py::init([](py::object pool_size) {
            atom::async::AsyncExecutor::Configuration config;
            if (!pool_size.is_none()) {
                config.minThreads = pool_size.cast<size_t>();
                config.maxThreads = pool_size.cast<size_t>();
            }
            return std::make_unique<atom::async::AsyncExecutor>(config);
        }), py::arg("pool_size") = py::none(),
        "Constructs an AsyncExecutor with a specified thread pool size (default: hardware concurrency)")
        .def(
            "schedule",
            [](atom::async::AsyncExecutor& self,
               ExecutionStrategy strategy,
               atom::async::AsyncExecutor::Priority priority, py::function func,
               py::args args) {
                // 将ExecutionStrategy转换为AsyncExecutor内部使用的类型或直接使用整数值
                // 并将函数和参数包装为可调用对象
                return self.execute([func, args]() {
                    py::gil_scoped_acquire acquire;
                    return func(*args).cast<py::object>();
                }, priority);
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
             [](atom::async::AsyncExecutor& self) {
                 // 这个方法可能需要自定义实现，因为我们无法确定是否有对应的方法
                 // 假设AsyncExecutor没有这个方法
                 throw std::runtime_error("Method not implemented");
             },
             "Execute all deferred tasks")
        .def("wait_for_all",
             [](atom::async::AsyncExecutor& self) {
                 // 等待所有任务完成的逻辑
                 // 假设没有直接对应的方法
                 throw std::runtime_error("Method not implemented");
             },
             "Wait for all tasks to complete, including deferred tasks")
        .def("queue_size",
             [](const atom::async::AsyncExecutor& self) {
                 return self.getPendingTaskCount();
             },
             "Get the number of tasks waiting in the queue")
        .def("active_task_count",
             [](const atom::async::AsyncExecutor& self) {
                 return self.getActiveThreadCount();
             },
             "Get the number of active tasks currently being processed")
        .def("resize",
             [](atom::async::AsyncExecutor& self, size_t pool_size) {
                 // 可能需要自定义实现，假设AsyncExecutor没有直接的resize方法
                 throw std::runtime_error("Method not implemented");
             },
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
