#include "atom/error/stacktrace.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

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

    // StackTraceConfig struct binding
    py::class_<atom::error::StackTraceConfig>(
        m, "StackTraceConfig",
        R"(Configuration options for stacktrace capture and formatting.

This class provides various options to customize how stack traces are captured
and formatted, including depth limits, filtering, and output formatting.

Attributes:
    max_depth (int): Maximum number of frames to capture (default: 128)
    skip_frames (int): Number of frames to skip from the top (default: 1)
    include_addresses (bool): Include memory addresses in output (default: True)
    include_modules (bool): Include module/library names (default: True)
    include_source_info (bool): Include source file and line numbers (default: True)
    demangle (bool): Demangle C++ function names (default: True)
    prettify (bool): Apply prettification to output (default: True)
    frame_prefix (str): Prefix for each frame line (default: "\t")
    unknown_function (str): Placeholder for unknown functions (default: "<unknown function>")
    unknown_module (str): Placeholder for unknown modules (default: "<unknown module>")

Examples:
    >>> from atom.error import StackTraceConfig
    >>> config = StackTraceConfig()
    >>> config.max_depth = 50
    >>> config.skip_frames = 2
)")
        .def(py::init<>(), "Constructs a StackTraceConfig with default values.")
        .def_readwrite("max_depth", &atom::error::StackTraceConfig::maxDepth,
                      "Maximum number of frames to capture")
        .def_readwrite("skip_frames", &atom::error::StackTraceConfig::skipFrames,
                      "Number of frames to skip from the top")
        .def_readwrite("include_addresses", &atom::error::StackTraceConfig::includeAddresses,
                      "Include memory addresses in output")
        .def_readwrite("include_modules", &atom::error::StackTraceConfig::includeModules,
                      "Include module/library names")
        .def_readwrite("include_source_info", &atom::error::StackTraceConfig::includeSourceInfo,
                      "Include source file and line numbers")
        .def_readwrite("demangle", &atom::error::StackTraceConfig::demangle,
                      "Demangle C++ function names")
        .def_readwrite("prettify", &atom::error::StackTraceConfig::prettify,
                      "Apply prettification to output")
        .def_readwrite("frame_prefix", &atom::error::StackTraceConfig::framePrefix,
                      "Prefix for each frame line")
        .def_readwrite("unknown_function", &atom::error::StackTraceConfig::unknownFunction,
                      "Placeholder for unknown functions")
        .def_readwrite("unknown_module", &atom::error::StackTraceConfig::unknownModule,
                      "Placeholder for unknown modules")
        .def_readwrite("frame_filter", &atom::error::StackTraceConfig::frameFilter,
                      "Custom frame filter function");

    // StackFrame struct binding
    py::class_<atom::error::StackFrame>(
        m, "StackFrame",
        R"(Information about a single stack frame.

This class represents a single frame in a stack trace, containing information
about the function, source location, and memory address.

Attributes:
    address: Memory address of the frame
    function (str): Function name (demangled if available)
    module (str): Module/library name
    source_file (str): Source file name
    source_line (int): Source line number
    offset (int): Offset within the function/module

Examples:
    >>> from atom.error import StackFrame
    >>> frame = StackFrame()
    >>> print(frame.function)
    >>> print(frame.source_file, frame.source_line)
)")
        .def(py::init<>(), "Constructs an empty StackFrame.")
        .def_readwrite("address", &atom::error::StackFrame::address,
                      "Memory address of the frame")
        .def_readwrite("function", &atom::error::StackFrame::function,
                      "Function name (demangled if available)")
        .def_readwrite("module", &atom::error::StackFrame::module,
                      "Module/library name")
        .def_readwrite("source_file", &atom::error::StackFrame::sourceFile,
                      "Source file name")
        .def_readwrite("source_line", &atom::error::StackFrame::sourceLine,
                      "Source line number")
        .def_readwrite("offset", &atom::error::StackFrame::offset,
                      "Offset within the function/module")
        .def("to_string",
             py::overload_cast<const atom::error::StackTraceConfig&>(&atom::error::StackFrame::toString, py::const_),
             py::arg("config") = atom::error::StackTraceConfig(),
             R"(Convert frame to string representation.

Args:
    config (StackTraceConfig, optional): Configuration for formatting

Returns:
    str: String representation of the frame
)")
        .def("__str__",
             [](const atom::error::StackFrame& frame) {
                 return frame.toString();
             },
             "Returns a string representation of the stack frame.")
        .def("__repr__",
             [](const atom::error::StackFrame& frame) {
                 return "<StackFrame: " + frame.function + ">";
             });

    // StackTrace class binding
    py::class_<atom::error::StackTrace>(
        m, "StackTrace",
        R"(Enhanced stack trace class with support for multiple backends.

This class provides a unified interface for capturing and formatting stack traces
using different backend implementations. It supports external libraries like
cpptrace, backward-cpp, and boost::stacktrace, with fallback to built-in
platform-specific implementations.

Examples:
    >>> from atom.error import StackTrace
    >>> trace = StackTrace()
    >>> print(trace)
    Stack trace:
      [0] main at example.cpp:10
      [1] _start at ...

    >>> # With custom configuration
    >>> from atom.error import StackTrace, StackTraceConfig
    >>> config = StackTraceConfig()
    >>> config.max_depth = 20
    >>> trace = StackTrace(config)
)")
        .def(py::init<>(),
             "Constructs a StackTrace object and captures the current stack trace.")
        .def(py::init<const atom::error::StackTraceConfig&>(),
             py::arg("config"),
             R"(Constructs a StackTrace with custom configuration.

Args:
    config (StackTraceConfig): Configuration options for stack trace capture
)")
        .def("to_string",
             py::overload_cast<>(&atom::error::StackTrace::toString, py::const_),
             R"(Get the string representation of the stack trace.

Returns:
    str: A string representing the captured stack trace with enhanced details.
)")
        .def("to_string",
             py::overload_cast<const atom::error::StackTraceConfig&>(&atom::error::StackTrace::toString, py::const_),
             py::arg("config"),
             R"(Get the string representation with custom configuration.

Args:
    config (StackTraceConfig): Configuration for formatting

Returns:
    str: A string representing the stack trace with custom formatting
)")
        .def("get_frames", &atom::error::StackTrace::getFrames,
             R"(Get individual stack frames.

Returns:
    list[StackFrame]: List of stack frames
)")
        .def("size", &atom::error::StackTrace::size,
             R"(Get the number of captured frames.

Returns:
    int: Number of frames in the stack trace
)")
        .def("empty", &atom::error::StackTrace::empty,
             R"(Check if stack trace is empty.

Returns:
    bool: True if no frames were captured
)")
        .def("get_backend_name", &atom::error::StackTrace::getBackendName,
             R"(Get the backend used for capturing this stack trace.

Returns:
    str: Name of the backend used
)")
        .def_static("set_default_config", &atom::error::StackTrace::setDefaultConfig,
                   py::arg("config"),
                   R"(Set global default configuration.

Args:
    config (StackTraceConfig): Default configuration to use for new StackTrace instances
)")
        .def_static("get_default_config", &atom::error::StackTrace::getDefaultConfig,
                   py::return_value_policy::reference,
                   R"(Get global default configuration.

Returns:
    StackTraceConfig: Current default configuration
)")
        .def_static("get_available_backends", &atom::error::StackTrace::getAvailableBackends,
                   R"(Get available backends.

Returns:
    list[str]: List of available backend names
)")
        .def_static("set_preferred_backend", &atom::error::StackTrace::setPreferredBackend,
                   py::arg("backend_name"),
                   R"(Force use of specific backend.

Args:
    backend_name (str): Name of backend to use ("auto" for automatic selection)
)")
        // Python-specific methods
        .def("__str__",
             py::overload_cast<>(&atom::error::StackTrace::toString, py::const_),
             "Returns a string representation of the stack trace.")
        .def("__repr__", [](const atom::error::StackTrace& st) {
            return "<StackTrace: " + std::to_string(st.size()) + " frames, backend=" + st.getBackendName() + ">";
        })
        .def("__len__", &atom::error::StackTrace::size,
             "Returns the number of frames in the stack trace.")
        .def("__bool__", [](const atom::error::StackTrace& st) {
            return !st.empty();
        }, "Returns True if the stack trace is not empty.");

    // Utility functions from stacktrace_utils namespace
    m.def("demangle", &atom::error::stacktrace_utils::demangle,
          py::arg("mangled"),
          R"(Demangle C++ function name.

Args:
    mangled (str): Mangled function name

Returns:
    str: Demangled function name, or original if demangling fails

Examples:
    >>> from atom.error import demangle
    >>> mangled = "_Z3foov"
    >>> print(demangle(mangled))
    foo()
)");

    m.def("prettify", &atom::error::stacktrace_utils::prettify,
          py::arg("input"),
          R"(Prettify stacktrace output.

Args:
    input (str): Raw stacktrace string

Returns:
    str: Prettified stacktrace string
)");

    m.def("format_address", &atom::error::stacktrace_utils::formatAddress,
          py::arg("address"),
          R"(Format memory address.

Args:
    address (int): Memory address

Returns:
    str: Formatted address string
)");

    m.def("get_base_name", &atom::error::stacktrace_utils::getBaseName,
          py::arg("path"),
          R"(Get base name from file path.

Args:
    path (str): Full file path

Returns:
    str: Base name (filename only)

Examples:
    >>> from atom.error import get_base_name
    >>> print(get_base_name("/path/to/file.cpp"))
    file.cpp
)");

    m.def("contains_mangled_names", &atom::error::stacktrace_utils::containsMangledNames,
          py::arg("str"),
          R"(Check if a string contains a mangled C++ name.

Args:
    str (str): String to check

Returns:
    bool: True if string appears to contain mangled names
)");

    // Convenience functions from stacktrace namespace
    m.def("current",
          py::overload_cast<>(&atom::error::stacktrace::current),
          R"(Capture current stack trace with default settings.

Returns:
    str: String representation of stack trace

Examples:
    >>> from atom.error import current
    >>> trace_str = current()
    >>> print(trace_str)
)");

    m.def("current",
          py::overload_cast<int>(&atom::error::stacktrace::current),
          py::arg("max_depth"),
          R"(Capture current stack trace with custom depth.

Args:
    max_depth (int): Maximum number of frames to capture

Returns:
    str: String representation of stack trace
)");

    m.def("current",
          py::overload_cast<const atom::error::StackTraceConfig&>(&atom::error::stacktrace::current),
          py::arg("config"),
          R"(Capture current stack trace with custom configuration.

Args:
    config (StackTraceConfig): Configuration options

Returns:
    str: String representation of stack trace
)");

    // Legacy convenience functions (kept for backward compatibility)
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
