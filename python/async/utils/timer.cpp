#include "atom/async/timer.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(timer, m) {
    m.doc() = R"pbdoc(
        Timer Implementation Module
        --------------------------

        This module provides high-performance timer functionality for scheduling
        and executing tasks with precise timing control and advanced features.

        Features:
          - High-precision task scheduling with millisecond accuracy
          - One-time and recurring task execution with customizable intervals
          - Priority-based task execution for complex scheduling scenarios
          - Thread-safe timer operations with atomic synchronization
          - Pause/resume functionality for dynamic timer control
          - Task cancellation and cleanup mechanisms
          - Callback system for task execution monitoring

        Timer Types:
          - Timer: Main timer class for scheduling tasks
          - TimerTask: Individual task representation with execution parameters

        Execution Strategies:
          - setTimeout: Schedule one-time task execution after a delay
          - setInterval: Schedule recurring task execution at intervals
          - Priority-based scheduling for complex task management
          - Asynchronous execution with future-based result handling

        Example:
            >>> from atom.async.timer import Timer, create_timer
            >>>
            >>> # Create a timer instance
            >>> timer = Timer()
            >>>
            >>> # Schedule a one-time task
            >>> def greet(name):
            ...     print(f"Hello, {name}!")
            >>> future = timer.set_timeout(greet, 1000, "World")
            >>>
            >>> # Schedule a recurring task
            >>> def heartbeat():
            ...     print("System is alive")
            >>> timer.set_interval(heartbeat, 5000, 10, 1)  # Every 5s, 10 times, priority 1
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

    // TimerTask class binding
    py::class_<atom::async::TimerTask>(
        m, "TimerTask",
        R"(Represents a task to be scheduled and executed by the Timer.

This class encapsulates a function to be executed at a scheduled time with
options for repetition and priority settings.

Args:
    func: The function to be executed when the task runs.
    delay: The delay in milliseconds before the first execution.
    repeat_count: The number of times the task should be repeated. -1 for infinite repetition.
    priority: The priority of the task.

Examples:
    >>> from atom.async import TimerTask
    >>> def print_hello():
    ...     print("Hello, World!")
    >>> task = TimerTask(print_hello, 1000, 1, 0)
)")
        .def(py::init<std::function<void()>, unsigned int, int, int>(),
             py::arg("func"), py::arg("delay"), py::arg("repeat_count"),
             py::arg("priority"),
             "Constructs a new TimerTask with the specified parameters.")
        .def("run", &atom::async::TimerTask::run,
             R"(Executes the task's associated function.

Raises:
    Exception: Propagates any exceptions thrown by the task function.
)")
        .def("get_next_execution_time",
             &atom::async::TimerTask::getNextExecutionTime,
             R"(Get the next scheduled execution time of the task.

Returns:
    The steady clock time point representing the next execution time.
)")
        .def_readwrite("func", &atom::async::TimerTask::m_func,
                       "The function to be executed.")
        .def_readwrite("delay", &atom::async::TimerTask::m_delay,
                       "The delay before the first execution in milliseconds.")
        .def_readwrite("repeat_count", &atom::async::TimerTask::m_repeatCount,
                       "The number of repetitions remaining.")
        .def_readwrite("priority", &atom::async::TimerTask::m_priority,
                       "The priority of the task.");

    // Timer class binding
    py::class_<atom::async::Timer>(
        m, "Timer",
        R"(Represents a timer for scheduling and executing tasks.

This class provides methods to schedule one-time or recurring tasks with
precise timing control and priority settings.

Examples:
    >>> from atom.async import Timer
    >>> timer = Timer()
    >>> def print_message(msg):
    ...     print(f"Message: {msg}")
    >>> # Execute once after 1 second
    >>> future = timer.set_timeout(print_message, 1000, "Hello!")
    >>> # Execute every 2 seconds, 5 times
    >>> timer.set_interval(print_message, 2000, 5, 0, "Tick!")
)")
        .def(py::init<>(), "Constructs a new Timer object.")
        .def(
            "set_timeout",
            [](atom::async::Timer& self, py::function func, unsigned int delay,
               py::args args) {
                if (args.size() == 0) {
                    return self.setTimeout([func]() { func(); }, delay);
                } else {
                    return self.setTimeout([func, args]() { func(*args); },
                                           delay);
                }
            },
            py::arg("func"), py::arg("delay"),
            R"(Schedules a task to be executed once after a specified delay.

Args:
    func: The function to be executed.
    delay: The delay in milliseconds before the function is executed.
    *args: The arguments to be passed to the function.

Returns:
    A future representing the result of the function execution.

Raises:
    ValueError: If the function is null or delay is invalid.

Examples:
    >>> def greet(name):
    ...     print(f"Hello, {name}!")
    >>> future = timer.set_timeout(greet, 1000, "World")  # Execute after 1 second
)")
        .def(
            "set_interval",
            [](atom::async::Timer& self, py::function func,
               unsigned int interval, int repeat_count, int priority,
               py::args args) {
                if (args.size() == 0) {
                    self.setInterval([func]() { func(); }, interval,
                                     repeat_count, priority);
                } else {
                    self.setInterval([func, args]() { func(*args); }, interval,
                                     repeat_count, priority);
                }
            },
            py::arg("func"), py::arg("interval"), py::arg("repeat_count") = -1,
            py::arg("priority") = 0,
            R"(Schedules a task to be executed repeatedly at a specified interval.

Args:
    func: The function to be executed.
    interval: The interval in milliseconds between executions.
    repeat_count: The number of times the function should be repeated. -1 for infinite repetition.
    priority: The priority of the task.
    *args: The arguments to be passed to the function.

Raises:
    ValueError: If func is null, interval is 0, or repeat_count is < -1.

Examples:
    >>> def update_status(status):
    ...     print(f"Status: {status}")
    >>> # Execute every 5 seconds, infinitely
    >>> timer.set_interval(update_status, 5000, -1, 0, "Running")
)")
        .def("now", &atom::async::Timer::now,
             R"(Get the current time according to the timer's clock.

Returns:
    The current steady clock time point.
)")
        .def("cancel_all_tasks", &atom::async::Timer::cancelAllTasks,
             R"(Cancels all scheduled tasks.

Examples:
    >>> timer.cancel_all_tasks()  # Cancel all pending tasks
)")
        .def("pause", &atom::async::Timer::pause,
             R"(Pauses the execution of scheduled tasks.

Examples:
    >>> timer.pause()  # Pause task execution
)")
        .def("resume", &atom::async::Timer::resume,
             R"(Resumes the execution of scheduled tasks after pausing.

Examples:
    >>> timer.resume()  # Resume task execution
)")
        .def("stop", &atom::async::Timer::stop,
             R"(Stops the timer and cancels all tasks.

Examples:
    >>> timer.stop()  # Stop the timer completely
)")
        .def("wait", &atom::async::Timer::wait,
             R"(Blocks the calling thread until all tasks are completed.

Examples:
    >>> timer.wait()  # Wait for all tasks to complete
)")
        .def(
            "set_callback",
            [](atom::async::Timer& self, py::function func) {
                self.setCallback([func]() { func(); });
            },
            py::arg("func"),
            R"(Sets a callback function to be called when a task is executed.

Args:
    func: The callback function to be set.

Raises:
    ValueError: If the function is null.

Examples:
    >>> def on_task_executed():
    ...     print("A task was executed!")
    >>> timer.set_callback(on_task_executed)
)")
        .def("get_task_count", &atom::async::Timer::getTaskCount,
             R"(Gets the number of tasks currently scheduled in the timer.

Returns:
    The number of scheduled tasks.

Examples:
    >>> count = timer.get_task_count()
    >>> print(f"There are {count} tasks scheduled")
)");

    // Helper functions for chrono durations
    m.def(
        "milliseconds",
        [](unsigned int ms) { return std::chrono::milliseconds(ms); },
        py::arg("ms"),
        R"(Create a milliseconds duration.

Args:
    ms: Number of milliseconds.

Returns:
    A milliseconds duration representing the specified time.

Examples:
    >>> from atom.async import milliseconds
    >>> delay = milliseconds(500)  # 500ms
)");

    m.def(
        "seconds", [](unsigned int s) { return std::chrono::seconds(s); },
        py::arg("s"),
        R"(Create a seconds duration.

Args:
    s: Number of seconds.

Returns:
    A seconds duration representing the specified time.

Examples:
    >>> from atom.async import seconds
    >>> delay = seconds(2)  # 2 seconds
)");

    m.def(
        "minutes", [](unsigned int m) { return std::chrono::minutes(m); },
        py::arg("m"),
        R"(Create a minutes duration.

Args:
    m: Number of minutes.

Returns:
    A minutes duration representing the specified time.

Examples:
    >>> from atom.async import minutes
    >>> delay = minutes(5)  # 5 minutes
)");

    // Factory function
    m.def(
        "create_timer", []() { return std::make_unique<atom::async::Timer>(); },
        R"(Creates a new Timer object.

Returns:
    A new Timer instance.

Examples:
    >>> from atom.async import create_timer
    >>> timer = create_timer()
)");

    // Convenience function to create and schedule a timeout
    m.def(
         "schedule_timeout",
         [](py::function func, unsigned int delay, py::args args) {
             auto timer = std::make_shared<atom::async::Timer>();
             auto future =
                 timer->setTimeout([func, args]() { func(*args); }, delay);
             return py::make_tuple(timer, future);
         },
         py::arg("func"), py::arg("delay"),
         R"(Creates a new Timer and schedules a one-time task.

Args:
    func: The function to be executed.
    delay: The delay in milliseconds before the function is executed.
    *args: The arguments to be passed to the function.

Returns:
    A tuple containing (timer, future).

Examples:
    >>> from atom.async import schedule_timeout
    >>> def alert(message):
    ...     print(f"Alert: {message}")
    >>> timer, future = schedule_timeout(alert, 2000, "Time's up!")
)")

        .def(
            "schedule_interval",
            [](py::function func, unsigned int interval, int repeat_count,
               int priority, py::args args) {
                auto timer = std::make_shared<atom::async::Timer>();
                timer->setInterval([func, args]() { func(*args); }, interval,
                                   repeat_count, priority);
                return timer;
            },
            py::arg("func"), py::arg("interval"), py::arg("repeat_count") = -1,
            py::arg("priority") = 0,
            R"pbdoc(
        Create a new Timer and schedule a recurring task.

        Args:
            func: The function to be executed
            interval: The interval in milliseconds between executions
            repeat_count: The number of times to repeat (-1 for infinite)
            priority: The priority of the task
            *args: The arguments to be passed to the function

        Returns:
            A Timer instance with the scheduled task

        Examples:
            >>> def status_check(service):
            ...     print(f"Checking {service} status")
            >>> timer = schedule_interval(status_check, 10000, 5, 1, "database")
        )pbdoc")

        .def(
            "benchmark_timer_performance",
            [](size_t num_tasks, unsigned int delay_ms) -> py::dict {
                using namespace std::chrono;

                py::dict results;
                atom::async::Timer timer;

                // Benchmark task scheduling
                auto start = high_resolution_clock::now();

                std::vector<atom::async::EnhancedFuture<void>> futures;
                futures.reserve(num_tasks);

                for (size_t i = 0; i < num_tasks; ++i) {
                    auto future = timer.setTimeout(
                        []() {
                            // Simulate some work
                            volatile int dummy = 0;
                            for (int j = 0; j < 50; ++j) {
                                dummy += j;
                            }
                        },
                        delay_ms);
                    futures.push_back(std::move(future));
                }

                auto scheduling_end = high_resolution_clock::now();
                auto scheduling_duration =
                    duration_cast<microseconds>(scheduling_end - start);

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    try {
                        future.get();
                    } catch (...) {
                        // Ignore task execution errors for benchmarking
                    }
                }

                auto completion_end = high_resolution_clock::now();
                auto total_duration =
                    duration_cast<microseconds>(completion_end - start);

                // Calculate statistics
                double scheduling_time_us = scheduling_duration.count();
                double total_time_us = total_duration.count();
                double tasks_per_second =
                    (num_tasks * 1000000.0) / total_time_us;
                double avg_scheduling_time = scheduling_time_us / num_tasks;

                results[py::str("num_tasks")] = num_tasks;
                results[py::str("delay_ms")] = delay_ms;
                results[py::str("scheduling_time_us")] = scheduling_time_us;
                results[py::str("total_time_us")] = total_time_us;
                results[py::str("tasks_per_second")] = tasks_per_second;
                results[py::str("avg_scheduling_time_us")] =
                    avg_scheduling_time;
                results[py::str("final_task_count")] = timer.getTaskCount();

                return results;
            },
            py::arg("num_tasks") = 1000, py::arg("delay_ms") = 100,
            R"pbdoc(
        Benchmark timer performance with multiple tasks.

        Args:
            num_tasks: Number of tasks to schedule (default: 1000)
            delay_ms: Delay for each task in milliseconds (default: 100)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_timer_performance(500, 50)
            >>> print(f"Tasks per second: {results['tasks_per_second']:.2f}")
            >>> print(f"Avg scheduling time: {results['avg_scheduling_time_us']:.2f} μs")
        )pbdoc")

        .def(
            "create_timer_pool",
            [](size_t pool_size) -> py::list {
                py::list timers;
                for (size_t i = 0; i < pool_size; ++i) {
                    timers.append(std::make_unique<atom::async::Timer>());
                }
                return timers;
            },
            py::arg("pool_size"),
            R"pbdoc(
        Create a pool of timer instances for load distribution.

        Args:
            pool_size: Number of timer instances to create

        Returns:
            list: List of Timer instances

        Examples:
            >>> timer_pool = create_timer_pool(4)
            >>> # Use different timers for different task types
            >>> timer_pool[0].set_timeout(task1, 1000)
            >>> timer_pool[1].set_timeout(task2, 2000)
        )pbdoc")

        .def(
            "create_scheduled_task_manager",
            []() -> py::dict {
                py::dict manager;
                manager[py::str("timer")] =
                    std::make_unique<atom::async::Timer>();
                manager[py::str("tasks")] = py::list();
                manager[py::str("active")] = true;

                return manager;
            },
            R"pbdoc(
        Create a task manager for organizing scheduled tasks.

        Returns:
            dict: Task manager with timer, task list, and status

        Examples:
            >>> manager = create_scheduled_task_manager()
            >>> timer = manager["timer"]
            >>> tasks = manager["tasks"]
            >>> timer.set_timeout(lambda: print("Task executed"), 1000)
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

    // Feature detection
    m.attr("HAS_HIGH_PRECISION_TIMING") = true;
    m.attr("HAS_PRIORITY_SCHEDULING") = true;
    m.attr("HAS_ASYNC_EXECUTION") = true;

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO") = true;
    m.attr("TIMER_BACKEND") = "ASIO";
#else
    m.attr("HAS_ASIO") = false;
    m.attr("TIMER_BACKEND") = "Standard";
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

    // Platform information
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
