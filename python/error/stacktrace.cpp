#include "atom/error/stacktrace.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(stacktrace, m) {
    m.doc() = "Stack trace implementation module for the atom package";

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

    // StackTrace class binding
    py::class_<atom::error::StackTrace>(
        m, "StackTrace",
        R"(Class for capturing and representing a stack trace with enhanced details.

This class captures the stack trace of the current execution context and represents
it as a string, including file names, line numbers, function names, module
information, and memory addresses when available.

Examples:
    >>> from atom.error import StackTrace
    >>> trace = StackTrace()
    >>> print(trace)
    Stack trace:
      [0] main at example.cpp:10
      [1] _start at ...
)")
        .def(py::init<>(),
             "Constructs a StackTrace object and captures the current stack "
             "trace.")
        .def("to_string", &atom::error::StackTrace::toString,
             R"(Get the string representation of the stack trace.

Returns:
    str: A string representing the captured stack trace with enhanced details.
)")
        // Python-specific methods
        .def("__str__", &atom::error::StackTrace::toString,
             "Returns a string representation of the stack trace.")
        .def("__repr__", [](const atom::error::StackTrace& st) {
            return "<atom.error.StackTrace object at " +
                   std::to_string(reinterpret_cast<uintptr_t>(&st)) + ">";
        });

    // Convenience functions
    m.def(
        "capture_stack_trace", []() { return atom::error::StackTrace(); },
        R"(Captures and returns the current stack trace.

This function creates a StackTrace object that represents the current execution stack.

Returns:
    StackTrace: A stack trace object containing the current execution stack information.

Examples:
    >>> from atom.error import capture_stack_trace
    >>> trace = capture_stack_trace()
    >>> print(trace)
    Stack trace:
      [0] capture_stack_trace at ...
      [1] __main__ at ...
)");

    m.def(
        "print_stack_trace",
        []() {
            atom::error::StackTrace trace;
            return trace.toString();
        },
        R"(Captures and returns a string representation of the current stack trace.

This function captures the current stack trace and returns it as a formatted string.

Returns:
    str: A string representation of the current stack trace.

Examples:
    >>> from atom.error import print_stack_trace
    >>> trace_str = print_stack_trace()
    >>> print(trace_str)
    Stack trace:
      [0] print_stack_trace at ...
      [1] __main__ at ...
)");

    // Add this function to enhance error reporting in Python
    m.def(
        "format_exception_with_traceback",
        [](const std::string& exc_type, const std::string& exc_value) {
            atom::error::StackTrace trace;
            std::string stacktrace = trace.toString();

            return "Exception: " + exc_type + "\n" + "Message: " + exc_value +
                   "\n\n" + "Native Stack Trace:\n" + stacktrace;
        },
        py::arg("exc_type"), py::arg("exc_value"),
        R"(Formats an exception with the current native stack trace.

This function combines exception information with a native C++ stack trace to provide
enhanced error reporting.

Args:
    exc_type: The type of the exception.
    exc_value: The exception message or value.

Returns:
    str: A formatted string containing the exception details and native stack trace.

Examples:
    >>> from atom.error import format_exception_with_traceback
    >>> try:
    ...     raise ValueError("Invalid input")
    ... except Exception as e:
    ...     error_report = format_exception_with_traceback(type(e).__name__, str(e))
    ...     print(error_report)
    Exception: ValueError
    Message: Invalid input

    Native Stack Trace:
      [0] format_exception_with_traceback at ...
      [1] __main__ at ...
)");

    // Context manager for capturing stack traces at specific points
    py::class_<atom::error::StackTrace>(m, "StackTraceCapture")
        .def(py::init<>())
        .def("__enter__",
             [](atom::error::StackTrace& self) -> atom::error::StackTrace& {
                 // Return self to be used in the with block
                 return self;
             })
        .def("__exit__",
             [](atom::error::StackTrace& self, py::object exc_type,
                py::object exc_val, py::object exc_tb) {
                 // Just need to implement the interface, no special cleanup
                 // needed
                 return false;  // Don't suppress exceptions
             })
        .def("get_trace", &atom::error::StackTrace::toString,
             "Returns the captured stack trace as a string.");

    // Add a function to create a decorator for instrumenting functions with
    // stack traces
    m.def(
        "trace_decorator",
        [](py::function func) {
            return py::cpp_function([func](py::args args, py::kwargs kwargs) {
                try {
                    // Call the original function
                    return func(*args, **kwargs);
                } catch (const std::exception& e) {
                    // Capture stack trace on exception
                    atom::error::StackTrace trace;
                    std::string stack_info = trace.toString();

                    // Re-raise with enhanced information
                    PyErr_SetString(PyExc_RuntimeError,
                                    (std::string(e.what()) +
                                     "\n\nNative Stack Trace:\n" + stack_info)
                                        .c_str());
                    throw py::error_already_set();
                }
            });
        },
        R"(Creates a decorator that adds stack trace capturing to functions.

This function returns a decorator that can be used to wrap other functions.
When the wrapped function throws a C++ exception, the decorator will capture
the native stack trace and include it in the error message.

Returns:
    function: A decorator function.

Examples:
    >>> from atom.error import trace_decorator
    >>> @trace_decorator
    ... def risky_function():
    ...     # Some code that might raise a C++ exception
    ...     pass
)");
}
