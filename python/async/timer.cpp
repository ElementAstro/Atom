#include "atom/async/timer.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(timer, m) {
    m.doc() = "Timer implementation module for the atom package";

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
)");
}
