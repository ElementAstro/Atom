#include "atom/utils/qtimer.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <chrono>

namespace py = pybind11;

PYBIND11_MODULE(timer, m) {
    m.doc() = "Timer utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::TimerException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::logic_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ElapsedTimer class binding
    py::class_<atom::utils::ElapsedTimer>(
        m, "ElapsedTimer",
        R"(Class to measure elapsed time using high-resolution clock.

This class provides functionality to measure elapsed time in various units
(nanoseconds, microseconds, milliseconds, seconds, minutes, hours).

Examples:
    >>> from atom.utils import ElapsedTimer
    >>> timer = ElapsedTimer(True)  # Start immediately
    >>> # Do some work
    >>> elapsed_ms = timer.elapsed_ms()
    >>> print(f"Operation took {elapsed_ms} ms")
)")
        .def(py::init<>(),
             "Initializes the timer. The timer is initially not started.")
        .def(py::init<bool>(), py::arg("start_now"),
             "Initializes the timer and optionally starts it immediately.")
        .def("start", &atom::utils::ElapsedTimer::start,
             "Start or restart the timer.")
        .def("invalidate", &atom::utils::ElapsedTimer::invalidate,
             "Invalidate the timer.")
        .def("is_valid", &atom::utils::ElapsedTimer::isValid,
             "Check if the timer has been started and is valid.")
        .def("elapsed_ns", &atom::utils::ElapsedTimer::elapsedNs,
             "Get elapsed time in nanoseconds.")
        .def("elapsed_us", &atom::utils::ElapsedTimer::elapsedUs,
             "Get elapsed time in microseconds.")
        .def("elapsed_ms", &atom::utils::ElapsedTimer::elapsedMs,
             "Get elapsed time in milliseconds.")
        .def("elapsed_sec", &atom::utils::ElapsedTimer::elapsedSec,
             "Get elapsed time in seconds.")
        .def("elapsed_min", &atom::utils::ElapsedTimer::elapsedMin,
             "Get elapsed time in minutes.")
        .def("elapsed_hrs", &atom::utils::ElapsedTimer::elapsedHrs,
             "Get elapsed time in hours.")
        .def("elapsed",
             &atom::utils::ElapsedTimer::elapsed<std::chrono::milliseconds>,
             "Get elapsed time in milliseconds.")
        .def("has_expired", &atom::utils::ElapsedTimer::hasExpired,
             py::arg("ms"),
             "Check if a specified duration (in milliseconds) has passed.")
        .def("remaining_time_ms", &atom::utils::ElapsedTimer::remainingTimeMs,
             py::arg("ms"),
             "Get the remaining time until the specified duration (in "
             "milliseconds) has passed.")
        .def_static(
            "current_time_ms", &atom::utils::ElapsedTimer::currentTimeMs,
            "Get the current absolute time in milliseconds since epoch.")
        .def(
            "__eq__",
            [](const atom::utils::ElapsedTimer& self,
               const atom::utils::ElapsedTimer& other) {
                return self == other;
            },
            py::is_operator())
        .def(
            "__lt__",
            [](const atom::utils::ElapsedTimer& self,
               const atom::utils::ElapsedTimer& other) { return self < other; },
            py::is_operator())
        .def(
            "__le__",
            [](const atom::utils::ElapsedTimer& self,
               const atom::utils::ElapsedTimer& other) {
                return self <= other;
            },
            py::is_operator())
        .def(
            "__gt__",
            [](const atom::utils::ElapsedTimer& self,
               const atom::utils::ElapsedTimer& other) { return self > other; },
            py::is_operator())
        .def(
            "__ge__",
            [](const atom::utils::ElapsedTimer& self,
               const atom::utils::ElapsedTimer& other) {
                return self >= other;
            },
            py::is_operator());

    // PrecisionMode enum for Timer
    py::enum_<atom::utils::Timer::PrecisionMode>(m, "PrecisionMode",
                                                 "Timer precision modes")
        .value("PRECISE", atom::utils::Timer::PrecisionMode::PRECISE,
               "More CPU intensive but more precise timing")
        .value("COARSE", atom::utils::Timer::PrecisionMode::COARSE,
               "Less CPU intensive but less precise timing")
        .export_values();

    // Timer class binding
    py::class_<atom::utils::Timer, std::shared_ptr<atom::utils::Timer>>(
        m, "Timer",
        R"(Modern C++ timer class inspired by Qt's QTimer.
        
This class provides timer functionality with callbacks, single-shot mode,
and customizable precision.

Examples:
    >>> from atom.utils import Timer
    >>> def callback():
    ...     print("Timer expired!")
    >>> # Create a timer that fires every 1000 ms
    >>> timer = Timer(callback)
    >>> timer.set_interval(1000)
    >>> timer.start()
    >>> # Later...
    >>> timer.stop()
)")
        .def(py::init<>(), "Default constructor")
        .def(py::init<atom::utils::Timer::Callback>(), py::arg("callback"),
             "Constructor with callback function to call when timer expires")
        .def("set_callback", &atom::utils::Timer::setCallback,
             py::arg("callback"), "Sets the callback function")
        .def("set_interval", &atom::utils::Timer::setInterval,
             py::arg("milliseconds"), "Sets the interval between timeouts")
        .def("interval", &atom::utils::Timer::interval,
             "Gets the current interval in milliseconds")
        .def("set_precision_mode", &atom::utils::Timer::setPrecisionMode,
             py::arg("mode"), "Sets the precision mode (PRECISE or COARSE)")
        .def("precision_mode", &atom::utils::Timer::precisionMode,
             "Gets the current precision mode")
        .def("set_single_shot", &atom::utils::Timer::setSingleShot,
             py::arg("single_shot"),
             "Sets whether the timer is a single-shot timer")
        .def("is_single_shot", &atom::utils::Timer::isSingleShot,
             "Checks if timer is set to single-shot mode")
        .def("is_active", &atom::utils::Timer::isActive,
             "Checks if timer is currently active")
        .def("start", py::overload_cast<>(&atom::utils::Timer::start),
             "Starts or restarts the timer")
        .def("start", py::overload_cast<int64_t>(&atom::utils::Timer::start),
             py::arg("milliseconds"),
             "Starts or restarts the timer with a specified interval")
        .def("stop", &atom::utils::Timer::stop, "Stops the timer")
        .def("remaining_time", &atom::utils::Timer::remainingTime,
             "Gets the time remaining before the next timeout")
        .def_static(
            "single_shot", &atom::utils::Timer::singleShot,
            py::arg("milliseconds"), py::arg("callback"),
            py::arg("mode") = atom::utils::Timer::PrecisionMode::PRECISE,
            R"(Creates a single-shot timer that calls the provided callback after the specified interval.
                  
Args:
    milliseconds: Interval in milliseconds
    callback: Function to call when timer expires
    mode: Precision mode (default: PRECISE)

Returns:
    A Timer object configured as single-shot

Examples:
    >>> from atom.utils import Timer
    >>> def callback():
    ...     print("Single shot timer fired!")
    >>> timer = Timer.single_shot(1000, callback)
                  )");
}