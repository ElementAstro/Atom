#include "atom/utils/stopwatcher.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(stopwatcher, m) {
    m.doc() = "High-precision stopwatch utility for timing operations";

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

    // StopWatcherState enum
    py::enum_<atom::utils::StopWatcherState>(
        m, "StopWatcherState",
        R"(States that a StopWatcher instance can be in.

Attributes:
    IDLE: Initial state, before first start
    RUNNING: Timer is currently running
    PAUSED: Timer is paused, can be resumed
    STOPPED: Timer is stopped, must be reset before starting again
)")
        .value("IDLE", atom::utils::StopWatcherState::Idle)
        .value("RUNNING", atom::utils::StopWatcherState::Running)
        .value("PAUSED", atom::utils::StopWatcherState::Paused)
        .value("STOPPED", atom::utils::StopWatcherState::Stopped);

    // StopWatcher class
    py::class_<atom::utils::StopWatcher>(
        m, "StopWatcher",
        R"(A high-precision stopwatch class for timing operations.

This class provides functionality to measure elapsed time with millisecond precision.
It supports operations like start, stop, pause, resume and lap timing.

Examples:
    >>> from atom.utils import StopWatcher
    >>> sw = StopWatcher()
    >>> sw.start()
    >>> # ... do some work ...
    >>> lap_time = sw.lap()  # Record intermediate time
    >>> # ... do more work ...
    >>> sw.stop()
    >>> print(f"Total time: {sw.elapsed_formatted()}")
)")
        .def(py::init<>(), "Constructs a new StopWatcher instance.")
        .def("start", &atom::utils::StopWatcher::start,
             R"(Starts the stopwatch.

Raises:
    RuntimeError: If the stopwatch is already running.
)")
        .def("stop", &atom::utils::StopWatcher::stop,
             R"(Stops the stopwatch.

Returns:
    bool: True if successfully stopped, False if already stopped.
)")
        .def("pause", &atom::utils::StopWatcher::pause,
             R"(Pauses the stopwatch without resetting.

Returns:
    bool: True if successfully paused, False if not running.

Raises:
    RuntimeError: If the stopwatch is not running.
)")
        .def("resume", &atom::utils::StopWatcher::resume,
             R"(Resumes the stopwatch from paused state.

Returns:
    bool: True if successfully resumed, False if not paused.

Raises:
    RuntimeError: If the stopwatch is not paused.
)")
        .def("reset", &atom::utils::StopWatcher::reset,
             R"(Resets the stopwatch to initial state.

Clears all recorded lap times and callbacks.
)")
        .def("elapsed_milliseconds",
             &atom::utils::StopWatcher::elapsedMilliseconds,
             R"(Gets the elapsed time in milliseconds.

Returns:
    float: The elapsed time with millisecond precision.
)")
        .def("elapsed_seconds", &atom::utils::StopWatcher::elapsedSeconds,
             R"(Gets the elapsed time in seconds.

Returns:
    float: The elapsed time with second precision.
)")
        .def("elapsed_formatted", &atom::utils::StopWatcher::elapsedFormatted,
             R"(Gets the elapsed time as formatted string (HH:MM:SS.mmm).

Returns:
    str: Formatted time string.
)")
        .def("get_state", &atom::utils::StopWatcher::getState,
             R"(Gets the current state of the stopwatch.

Returns:
    StopWatcherState: Current state.
)")
        .def(
            "get_lap_times",
            [](const atom::utils::StopWatcher& self) {
                auto span = self.getLapTimes();
                return std::vector<double>(span.begin(), span.end());
            },
            R"(Gets all recorded lap times.

Returns:
    list[float]: List of lap times in milliseconds.
)")
        .def("get_average_lap_time",
             &atom::utils::StopWatcher::getAverageLapTime,
             R"(Gets the average of all recorded lap times.

Returns:
    float: Average lap time in milliseconds, 0 if no laps recorded.
)")
        .def("get_lap_count", &atom::utils::StopWatcher::getLapCount,
             R"(Gets the total number of laps recorded.

Returns:
    int: Number of laps.
)")
        .def(
            "register_callback",
            [](atom::utils::StopWatcher& self, py::function callback,
               int milliseconds) {
                self.registerCallback([callback]() { callback(); },
                                      milliseconds);
            },
            py::arg("callback"), py::arg("milliseconds"),
            R"(Registers a callback to be called after specified time.

Args:
    callback: Python function to be called with no arguments.
    milliseconds: Time in milliseconds after which callback should trigger.

Raises:
    ValueError: If milliseconds is negative.
)")
        .def("lap", &atom::utils::StopWatcher::lap,
             R"(Records current time as a lap time.

Returns:
    float: The recorded lap time in milliseconds.

Raises:
    RuntimeError: If stopwatch is not running.
)")
        .def("is_running", &atom::utils::StopWatcher::isRunning,
             R"(Checks if the stopwatch is running.

Returns:
    bool: True if running, False otherwise.
)")
        // Python-specific methods
        .def(
            "__enter__",
            [](atom::utils::StopWatcher& self) {
                auto result = self.start();
                if (!result) {
                    throw std::runtime_error(
                        "Failed to start stopwatch: " +
                        std::to_string(static_cast<int>(result.error())));
                }
                return &self;
            },
            R"(Enables use of StopWatcher in 'with' statements.

When entering a context using 'with StopWatcher() as sw:', the stopwatch starts automatically.

Returns:
    StopWatcher: The StopWatcher instance for use in the context.
)")
        .def(
            "__exit__",
            [](atom::utils::StopWatcher& self, py::object exc_type,
               py::object exc_value, py::object traceback) {
                auto result = self.stop();  // Store the return value
                (void)result;               // Explicitly ignore the value
                return false;               // Don't suppress exceptions
            },
            R"(Handles exiting a 'with' context.

When exiting a context started with 'with StopWatcher() as sw:', the stopwatch stops automatically.
)")
        .def(
            "__str__",
            [](const atom::utils::StopWatcher& self) {
                return "StopWatcher(elapsed=" + self.elapsedFormatted() +
                       ", state=" +
                       std::to_string(static_cast<int>(self.getState())) +
                       ", lap_count=" + std::to_string(self.getLapCount()) +
                       ")";
            },
            "Returns a string representation of the StopWatcher.");

    // Add StateTransitionError exception
    static py::exception<std::runtime_error> state_transition_error(
        m, "StateTransitionError");
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::runtime_error& e) {
            std::string error = e.what();
            if (error.find("stopwatch") != std::string::npos) {
                state_transition_error(error.c_str());
            } else {
                throw;
            }
        }
    });

    // Add context manager utility function
    m.def(
        "timed_execution",
        [](py::function func) {
            atom::utils::StopWatcher sw;
            auto start_result = sw.start();
            if (!start_result) {
                throw std::runtime_error(
                    "Failed to start stopwatch: " +
                    std::to_string(static_cast<int>(start_result.error())));
            }
            py::object result = func();
            auto stop_result = sw.stop();  // Store the return value
            (void)stop_result;             // Explicitly acknowledge it
            return py::make_tuple(result, sw.elapsedMilliseconds());
        },
        py::arg("function"),
        R"(Utility function to measure execution time of a function.

Args:
    function: Function to execute and time.

Returns:
    tuple: A tuple containing (function_result, elapsed_time_ms).

Examples:
    >>> from atom.utils import timed_execution
    >>> def my_func():
    ...     # some code
    ...     return "result"
    >>> result, time_ms = timed_execution(my_func)
)");

    // Time conversion utilities
    m.def(
        "format_time",
        [](double milliseconds) {
            int hours = static_cast<int>(milliseconds / (1000 * 60 * 60));
            milliseconds -= hours * (1000 * 60 * 60);

            int minutes = static_cast<int>(milliseconds / (1000 * 60));
            milliseconds -= minutes * (1000 * 60);

            int seconds = static_cast<int>(milliseconds / 1000);
            milliseconds -= seconds * 1000;

            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", hours,
                     minutes, seconds, static_cast<int>(milliseconds));
            return std::string(buffer);
        },
        py::arg("milliseconds"),
        R"(Formats time in milliseconds to HH:MM:SS.mmm format.

Args:
    milliseconds: Time in milliseconds.

Returns:
    str: Formatted time string.

Examples:
    >>> from atom.utils import format_time
    >>> formatted = format_time(65432)  # "00:01:05.432"
)");
}
