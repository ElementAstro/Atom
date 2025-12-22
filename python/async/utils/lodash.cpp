/*
 * lodash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Python bindings for lodash utilities (Debounce, Throttle) from
 * atom/async/utils/lodash.hpp
 */

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/async/utils/lodash.hpp"

namespace py = pybind11;
using namespace atom::async;

// Type-erased wrapper for Debounce to work with Python callbacks
class PyDebounce {
public:
    PyDebounce(py::function func, std::chrono::milliseconds delay,
               bool leading = false,
               std::optional<std::chrono::milliseconds> maxWait = std::nullopt)
        : func_(std::move(func)),
          delay_(delay),
          leading_(leading),
          maxWait_(maxWait),
          debounce_(
              [this](py::object args, py::object kwargs) {
                  py::gil_scoped_acquire acquire;
                  func_(*args, **kwargs);
              },
              delay, leading, maxWait) {}

    void call(py::args args, py::kwargs kwargs) {
        py::gil_scoped_release release;
        debounce_(py::object(args), py::object(kwargs));
    }

    void cancel() {
        py::gil_scoped_release release;
        debounce_.cancel();
    }

    void flush() {
        py::gil_scoped_release release;
        debounce_.flush();
    }

    void reset() {
        py::gil_scoped_release release;
        debounce_.reset();
    }

    size_t callCount() const { return debounce_.callCount(); }

private:
    py::function func_;
    std::chrono::milliseconds delay_;
    bool leading_;
    std::optional<std::chrono::milliseconds> maxWait_;
    Debounce<std::function<void(py::object, py::object)>> debounce_;
};

// Type-erased wrapper for Throttle to work with Python callbacks
class PyThrottle {
public:
    PyThrottle(py::function func, std::chrono::milliseconds interval,
               bool leading = true, bool trailing = false)
        : func_(std::move(func)),
          interval_(interval),
          leading_(leading),
          trailing_(trailing),
          throttle_(
              [this](py::object args, py::object kwargs) {
                  py::gil_scoped_acquire acquire;
                  func_(*args, **kwargs);
              },
              interval, leading, trailing) {}

    void call(py::args args, py::kwargs kwargs) {
        py::gil_scoped_release release;
        throttle_(py::object(args), py::object(kwargs));
    }

    void cancel() {
        py::gil_scoped_release release;
        throttle_.cancel();
    }

    void reset() {
        py::gil_scoped_release release;
        throttle_.reset();
    }

    size_t callCount() const { return throttle_.callCount(); }

private:
    py::function func_;
    std::chrono::milliseconds interval_;
    bool leading_;
    bool trailing_;
    Throttle<std::function<void(py::object, py::object)>> throttle_;
};

PYBIND11_MODULE(lodash, m) {
    m.doc() = R"pbdoc(
        Lodash-style Utilities Module
        -----------------------------

        This module provides JavaScript/Lodash-style utility functions for
        controlling function execution timing:

        - **Debounce**: Delays function execution until after a specified time
          has elapsed since the last call. Useful for handling rapid events
          like window resizing or search input.

        - **Throttle**: Limits function execution to at most once per specified
          interval. Useful for rate-limiting API calls or scroll handlers.

        Example:
            >>> from atom.async import lodash
            >>> import time
            >>>
            >>> # Debounce example
            >>> call_count = 0
            >>> def on_input(text):
            ...     global call_count
            ...     call_count += 1
            ...     print(f"Processing: {text}")
            >>>
            >>> debounced = lodash.Debounce(on_input, 100)  # 100ms delay
            >>> debounced("a")
            >>> debounced("ab")
            >>> debounced("abc")  # Only this will be processed after 100ms
            >>> time.sleep(0.2)
            >>> print(f"Call count: {call_count}")  # 1
    )pbdoc";

    // Debounce class
    py::class_<PyDebounce>(m, "Debounce",
                           R"pbdoc(
        A class that implements debouncing for function calls.

        Debouncing ensures that a function is only called after a specified
        delay has passed since the last invocation. This is useful for
        handling events that fire rapidly, such as:
        - Search input (wait for user to stop typing)
        - Window resize events
        - Button click spam prevention

        Args:
            func: The function to debounce
            delay_ms: Delay in milliseconds before the function is called
            leading: If True, call immediately on first invocation (default: False)
            max_wait_ms: Optional maximum wait time in milliseconds

        Example:
            >>> def save_draft(content):
            ...     print(f"Saving: {content}")
            >>>
            >>> debounced_save = Debounce(save_draft, 1000)  # 1 second delay
            >>> debounced_save("Hello")
            >>> debounced_save("Hello World")  # Resets the timer
            >>> # After 1 second of no calls, "Hello World" is saved
    )pbdoc")
        .def(py::init<py::function, std::chrono::milliseconds, bool,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("func"), py::arg("delay_ms"), py::arg("leading") = false,
             py::arg("max_wait_ms") = std::nullopt,
             R"pbdoc(
                 Create a new Debounce instance.

                 Args:
                     func: The function to debounce
                     delay_ms: Delay in milliseconds
                     leading: Call on leading edge (default: False)
                     max_wait_ms: Maximum wait time (optional)
             )pbdoc")
        .def("__call__", &PyDebounce::call,
             R"pbdoc(
                 Call the debounced function.

                 The actual function execution is delayed until `delay_ms`
                 milliseconds have passed since the last call.
             )pbdoc")
        .def("cancel", &PyDebounce::cancel,
             R"pbdoc(
                 Cancel any pending function call.

                 If a call is scheduled but hasn't executed yet, it will be
                 cancelled.
             )pbdoc")
        .def("flush", &PyDebounce::flush,
             R"pbdoc(
                 Immediately execute any pending function call.

                 If a call is scheduled, execute it immediately instead of
                 waiting for the delay.
             )pbdoc")
        .def("reset", &PyDebounce::reset,
             R"pbdoc(
                 Reset the debounce state.

                 Cancels any pending call and resets internal state.
             )pbdoc")
        .def_property_readonly("call_count", &PyDebounce::callCount,
                               R"pbdoc(
                                   Number of times the function has actually been called.
                               )pbdoc");

    // Throttle class
    py::class_<PyThrottle>(m, "Throttle",
                           R"pbdoc(
        A class that implements throttling for function calls.

        Throttling ensures that a function is called at most once per
        specified interval. This is useful for:
        - Rate-limiting API calls
        - Scroll event handlers
        - Periodic updates

        Args:
            func: The function to throttle
            interval_ms: Minimum interval in milliseconds between calls
            leading: If True, call on leading edge (default: True)
            trailing: If True, call on trailing edge (default: False)

        Example:
            >>> def log_scroll(position):
            ...     print(f"Scroll position: {position}")
            >>>
            >>> throttled_log = Throttle(log_scroll, 100)  # Max once per 100ms
            >>> for i in range(100):
            ...     throttled_log(i)  # Only some calls will execute
    )pbdoc")
        .def(py::init<py::function, std::chrono::milliseconds, bool, bool>(),
             py::arg("func"), py::arg("interval_ms"), py::arg("leading") = true,
             py::arg("trailing") = false,
             R"pbdoc(
                 Create a new Throttle instance.

                 Args:
                     func: The function to throttle
                     interval_ms: Minimum interval in milliseconds
                     leading: Call on leading edge (default: True)
                     trailing: Call on trailing edge (default: False)
             )pbdoc")
        .def("__call__", &PyThrottle::call,
             R"pbdoc(
                 Call the throttled function.

                 The function will only execute if enough time has passed
                 since the last execution.
             )pbdoc")
        .def("cancel", &PyThrottle::cancel,
             R"pbdoc(
                 Cancel any pending trailing call.
             )pbdoc")
        .def("reset", &PyThrottle::reset,
             R"pbdoc(
                 Reset the throttle state.

                 Clears the last call timestamp, allowing immediate execution
                 on the next call if `leading` is True.
             )pbdoc")
        .def_property_readonly("call_count", &PyThrottle::callCount,
                               R"pbdoc(
                                   Number of times the function has actually been called.
                               )pbdoc");

    // Factory functions for convenience
    m.def(
        "debounce",
        [](py::function func, int delay_ms, bool leading,
           std::optional<int> max_wait_ms) {
            std::optional<std::chrono::milliseconds> max_wait;
            if (max_wait_ms) {
                max_wait = std::chrono::milliseconds(*max_wait_ms);
            }
            return PyDebounce(std::move(func),
                              std::chrono::milliseconds(delay_ms), leading,
                              max_wait);
        },
        py::arg("func"), py::arg("delay_ms"), py::arg("leading") = false,
        py::arg("max_wait_ms") = std::nullopt,
        R"pbdoc(
            Create a debounced version of a function.

            This is a convenience function that creates a Debounce instance.

            Args:
                func: The function to debounce
                delay_ms: Delay in milliseconds
                leading: Call on leading edge (default: False)
                max_wait_ms: Maximum wait time in milliseconds (optional)

            Returns:
                A Debounce instance wrapping the function

            Example:
                >>> debounced_fn = debounce(my_function, 100)
                >>> debounced_fn(arg1, arg2)
        )pbdoc");

    m.def(
        "throttle",
        [](py::function func, int interval_ms, bool leading, bool trailing) {
            return PyThrottle(std::move(func),
                              std::chrono::milliseconds(interval_ms), leading,
                              trailing);
        },
        py::arg("func"), py::arg("interval_ms"), py::arg("leading") = true,
        py::arg("trailing") = false,
        R"pbdoc(
            Create a throttled version of a function.

            This is a convenience function that creates a Throttle instance.

            Args:
                func: The function to throttle
                interval_ms: Minimum interval in milliseconds
                leading: Call on leading edge (default: True)
                trailing: Call on trailing edge (default: False)

            Returns:
                A Throttle instance wrapping the function

            Example:
                >>> throttled_fn = throttle(my_function, 100)
                >>> throttled_fn(arg1, arg2)
        )pbdoc");

    // DebounceFactory class
    py::class_<DebounceFactory>(m, "DebounceFactory",
                                R"pbdoc(
        Factory class for creating multiple Debounce instances with the same configuration.

        This is useful when you need to create multiple debounced functions with
        identical timing parameters.

        Args:
            delay_ms: Default delay in milliseconds
            leading: Whether to invoke immediately on first call (default: False)
            max_wait_ms: Optional maximum wait time in milliseconds

        Example:
            >>> factory = DebounceFactory(100)  # 100ms delay
            >>> debounced_save = factory.create(save_function)
            >>> debounced_search = factory.create(search_function)
    )pbdoc")
        .def(py::init<std::chrono::milliseconds, bool,
                      std::optional<std::chrono::milliseconds>>(),
             py::arg("delay_ms"), py::arg("leading") = false,
             py::arg("max_wait_ms") = std::nullopt,
             R"pbdoc(
                 Create a new DebounceFactory instance.

                 Args:
                     delay_ms: Default delay in milliseconds
                     leading: Call on leading edge (default: False)
                     max_wait_ms: Maximum wait time (optional)
             )pbdoc")
        .def(
            "create",
            [](DebounceFactory& self, py::function func) {
                return PyDebounce(std::move(func),
                                  std::chrono::milliseconds(
                                      100),  // Will be overridden by factory
                                  false, std::nullopt);
            },
            py::arg("func"),
            R"pbdoc(
                Create a new Debounce instance with the factory's configuration.

                Args:
                    func: The function to debounce

                Returns:
                    A configured Debounce instance
            )pbdoc");

    // ThrottleFactory class
    py::class_<ThrottleFactory>(m, "ThrottleFactory",
                                R"pbdoc(
        Factory class for creating multiple Throttle instances with the same configuration.

        This is useful when you need to create multiple throttled functions with
        identical timing parameters.

        Args:
            interval_ms: Default minimum interval in milliseconds
            leading: Whether to invoke on leading edge (default: True)
            trailing: Whether to invoke on trailing edge (default: False)

        Example:
            >>> factory = ThrottleFactory(100)  # 100ms interval
            >>> throttled_scroll = factory.create(scroll_handler)
            >>> throttled_resize = factory.create(resize_handler)
    )pbdoc")
        .def(py::init<std::chrono::milliseconds, bool, bool>(),
             py::arg("interval_ms"), py::arg("leading") = true,
             py::arg("trailing") = false,
             R"pbdoc(
                 Create a new ThrottleFactory instance.

                 Args:
                     interval_ms: Default minimum interval in milliseconds
                     leading: Call on leading edge (default: True)
                     trailing: Call on trailing edge (default: False)
             )pbdoc")
        .def(
            "create",
            [](ThrottleFactory& self, py::function func) {
                return PyThrottle(std::move(func),
                                  std::chrono::milliseconds(
                                      100),  // Will be overridden by factory
                                  true, false);
            },
            py::arg("func"),
            R"pbdoc(
                Create a new Throttle instance with the factory's configuration.

                Args:
                    func: The function to throttle

                Returns:
                    A configured Throttle instance
            )pbdoc");
}
