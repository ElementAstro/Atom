#include "atom/extra/spdlog/modern_log.h"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(spdlog, m) {
    m.doc() = R"(Modern structured logging module for the atom package.

This module provides a modern C++ logging framework with support for
structured data, performance measurement, event systems, filtering,
and various output sinks.

Features:
- Structured logging with key-value pairs and nested data
- Performance measurement with scoped timers
- Event-driven logging system with custom handlers
- Advanced filtering capabilities
- Multiple output sinks (console, file, network, etc.)
- Automatic source location capture
- Thread-safe operations

Examples:
    >>> from atom.extra.spdlog import spdlog
    >>>
    >>> # Get default logger
    >>> logger = spdlog.LogManager.default_logger()
    >>>
    >>> # Basic logging
    >>> logger.info("Application started")
    >>> logger.warn("This is a warning: {}", "something happened")
    >>> logger.error("Error occurred: code={}", 404)
    >>>
    >>> # Structured logging
    >>> data = spdlog.StructuredData()
    >>> data.add("user_id", 12345)
    >>> data.add("action", "login")
    >>> data.add("success", True)
    >>> logger.log_structured(spdlog.Level.INFO, data)
    >>>
    >>> # Performance measurement
    >>> timer = logger.time_scope("database_query")
    >>> # ... perform database query ...
    >>> # Timer automatically logs elapsed time when destroyed
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const modern_log::LogError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // LogError exception
    py::register_exception<modern_log::LogError>(m, "LogError",
                                                 PyExc_RuntimeError);

    // Level enum
    py::enum_<modern_log::Level>(m, "Level",
                                 R"(Log level enumeration.

Defines the severity levels for log messages, from most verbose (trace)
to most critical (critical).)")
        .value("TRACE", modern_log::Level::trace, "Trace level - most verbose")
        .value("DEBUG", modern_log::Level::debug,
               "Debug level - detailed information")
        .value("INFO", modern_log::Level::info,
               "Info level - general information")
        .value("WARN", modern_log::Level::warn,
               "Warning level - potential issues")
        .value("ERROR", modern_log::Level::error,
               "Error level - error conditions")
        .value("CRITICAL", modern_log::Level::critical,
               "Critical level - critical errors")
        .value("OFF", modern_log::Level::off, "Off - disable logging")
        .export_values();

    // LogContext class
    py::class_<modern_log::LogContext>(
        m, "LogContext",
        R"(Context information for enriching log messages.

This class allows you to attach additional context data to log messages,
such as request IDs, user information, or other relevant metadata.

Examples:
    >>> context = spdlog.LogContext()
    >>> context.add("request_id", "req-12345")
    >>> context.add("user_id", 67890)
    >>> logger.log_with_context(spdlog.Level.INFO, context, "User action performed")
)")
        .def(py::init<>(), "Create an empty log context")
        .def("add",
             py::overload_cast<const std::string&, const std::string&>(
                 &modern_log::LogContext::add),
             py::arg("key"), py::arg("value"),
             R"(Add a string value to the context.

Args:
    key: The context key.
    value: The string value.
)")
        .def("add",
             py::overload_cast<const std::string&, int64_t>(
                 &modern_log::LogContext::add),
             py::arg("key"), py::arg("value"),
             R"(Add an integer value to the context.

Args:
    key: The context key.
    value: The integer value.
)")
        .def("add",
             py::overload_cast<const std::string&, double>(
                 &modern_log::LogContext::add),
             py::arg("key"), py::arg("value"),
             R"(Add a double value to the context.

Args:
    key: The context key.
    value: The double value.
)")
        .def("add",
             py::overload_cast<const std::string&, bool>(
                 &modern_log::LogContext::add),
             py::arg("key"), py::arg("value"),
             R"(Add a boolean value to the context.

Args:
    key: The context key.
    value: The boolean value.
)")
        .def("remove", &modern_log::LogContext::remove, py::arg("key"),
             R"(Remove a key from the context.

Args:
    key: The key to remove.

Returns:
    True if the key was removed, false if it didn't exist.
)")
        .def("clear", &modern_log::LogContext::clear,
             R"(Clear all context data.)")
        .def("empty", &modern_log::LogContext::empty,
             R"(Check if the context is empty.

Returns:
    True if the context has no data.
)")
        .def("size", &modern_log::LogContext::size,
             R"(Get the number of context entries.

Returns:
    The number of key-value pairs in the context.
)");

    // StructuredData class
    py::class_<modern_log::StructuredData>(
        m, "StructuredData",
        R"(Structured data for logging complex information.

This class allows you to build structured log entries with nested data,
arrays, and various data types, making logs more searchable and analyzable.

Examples:
    >>> data = spdlog.StructuredData()
    >>> data.add("event", "user_login")
    >>> data.add("user_id", 12345)
    >>> data.add("timestamp", "2023-01-01T12:00:00Z")
    >>> data.add("success", True)
    >>>
    >>> # Add nested object
    >>> user_data = spdlog.StructuredData()
    >>> user_data.add("name", "John Doe")
    >>> user_data.add("email", "john@example.com")
    >>> data.add_object("user", user_data)
    >>>
    >>> logger.log_structured(spdlog.Level.INFO, data)
)")
        .def(py::init<>(), "Create empty structured data")
        .def("add",
             py::overload_cast<const std::string&, const std::string&>(
                 &modern_log::StructuredData::add),
             py::arg("key"), py::arg("value"),
             R"(Add a string value.

Args:
    key: The field key.
    value: The string value.
)")
        .def("add",
             py::overload_cast<const std::string&, int64_t>(
                 &modern_log::StructuredData::add),
             py::arg("key"), py::arg("value"),
             R"(Add an integer value.

Args:
    key: The field key.
    value: The integer value.
)")
        .def("add",
             py::overload_cast<const std::string&, double>(
                 &modern_log::StructuredData::add),
             py::arg("key"), py::arg("value"),
             R"(Add a double value.

Args:
    key: The field key.
    value: The double value.
)")
        .def("add",
             py::overload_cast<const std::string&, bool>(
                 &modern_log::StructuredData::add),
             py::arg("key"), py::arg("value"),
             R"(Add a boolean value.

Args:
    key: The field key.
    value: The boolean value.
)")
        .def("add_object", &modern_log::StructuredData::add_object,
             py::arg("key"), py::arg("object"),
             R"(Add a nested object.

Args:
    key: The field key.
    object: The nested StructuredData object.
)")
        .def("add_array", &modern_log::StructuredData::add_array,
             py::arg("key"), py::arg("array"),
             R"(Add an array of strings.

Args:
    key: The field key.
    array: The array of string values.
)")
        .def("remove", &modern_log::StructuredData::remove, py::arg("key"),
             R"(Remove a field from the structured data.

Args:
    key: The key to remove.

Returns:
    True if the key was removed, false if it didn't exist.
)")
        .def("clear", &modern_log::StructuredData::clear,
             R"(Clear all structured data.)")
        .def("empty", &modern_log::StructuredData::empty,
             R"(Check if the structured data is empty.

Returns:
    True if there are no fields.
)")
        .def("to_json", &modern_log::StructuredData::to_json,
             R"(Convert to JSON string representation.

Returns:
    JSON string representation of the structured data.
)");

    // ScopedTimer class
    py::class_<modern_log::ScopedTimer>(
        m, "ScopedTimer",
        R"(RAII timer for performance measurement.

This class automatically measures elapsed time and logs the result
when the timer is destroyed (goes out of scope).

Examples:
    >>> # Manual timer creation
    >>> timer = logger.time_scope("database_operation")
    >>> # ... perform operation ...
    >>> del timer  # Logs elapsed time
    >>>
    >>> # Or use with context manager (if implemented)
    >>> with logger.time_scope("api_call") as timer:
    ...     # ... perform API call ...
    ...     pass  # Timer automatically logs when exiting context
)")
        .def("elapsed", &modern_log::ScopedTimer::elapsed,
             R"(Get the elapsed time since timer creation.

Returns:
    Elapsed time in milliseconds.
)")
        .def("stop", &modern_log::ScopedTimer::stop,
             R"(Stop the timer and log the result.

This method can be called manually to stop timing before
the timer object is destroyed.
)");

    // Logger class
    py::class_<modern_log::Logger>(m, "Logger",
                                   R"(Main logger class for structured logging.

This class provides various methods for logging messages at different
levels, with support for structured data, performance measurement,
and context enrichment.

Examples:
    >>> logger = spdlog.LogManager.default_logger()
    >>>
    >>> # Basic logging
    >>> logger.trace("Detailed trace information")
    >>> logger.debug("Debug information: value={}", 42)
    >>> logger.info("Application started successfully")
    >>> logger.warn("Deprecated API used")
    >>> logger.error("Failed to connect to database")
    >>> logger.critical("System is shutting down")
)")
        .def(
            "trace",
            [](modern_log::Logger& self, const std::string& message) {
                self.trace("{}", message);
            },
            py::arg("message"),
            R"(Log a trace-level message.

Args:
    message: The message to log.
)")
        .def(
            "debug",
            [](modern_log::Logger& self, const std::string& message) {
                self.debug("{}", message);
            },
            py::arg("message"),
            R"(Log a debug-level message.

Args:
    message: The message to log.
)")
        .def(
            "info",
            [](modern_log::Logger& self, const std::string& message) {
                self.info("{}", message);
            },
            py::arg("message"),
            R"(Log an info-level message.

Args:
    message: The message to log.
)")
        .def(
            "warn",
            [](modern_log::Logger& self, const std::string& message) {
                self.warn("{}", message);
            },
            py::arg("message"),
            R"(Log a warning-level message.

Args:
    message: The message to log.
)")
        .def(
            "error",
            [](modern_log::Logger& self, const std::string& message) {
                self.error("{}", message);
            },
            py::arg("message"),
            R"(Log an error-level message.

Args:
    message: The message to log.
)")
        .def(
            "critical",
            [](modern_log::Logger& self, const std::string& message) {
                self.critical("{}", message);
            },
            py::arg("message"),
            R"(Log a critical-level message.

Args:
    message: The message to log.
)")
        .def("log_structured", &modern_log::Logger::log_structured,
             py::arg("level"), py::arg("data"),
             R"(Log structured data.

Args:
    level: The log level.
    data: The structured data to log.
)")
        .def("log_exception", &modern_log::Logger::log_exception,
             py::arg("level"), py::arg("exception"), py::arg("context") = "",
             R"(Log an exception with optional context.

Args:
    level: The log level.
    exception: The exception to log.
    context: Optional context string.
)")
        .def("time_scope", &modern_log::Logger::time_scope, py::arg("name"),
             py::arg("level") = modern_log::Level::info,
             R"(Create a scoped timer for performance measurement.

Args:
    name: Name of the timer.
    level: Log level for the timer result.

Returns:
    ScopedTimer object for RAII timing.
)")
        .def("set_level", &modern_log::Logger::set_level, py::arg("level"),
             R"(Set the minimum log level.

Args:
    level: The minimum level to log.
)")
        .def("get_level", &modern_log::Logger::get_level,
             R"(Get the current log level.

Returns:
    The current minimum log level.
)")
        .def("should_log", &modern_log::Logger::should_log, py::arg("level"),
             R"(Check if a message at the given level should be logged.

Args:
    level: The log level to check.

Returns:
    True if messages at this level will be logged.
)");

    // LogManager class
    py::class_<modern_log::LogManager>(
        m, "LogManager",
        R"(Manager for creating and accessing loggers.

This class provides factory methods for creating loggers and accessing
the default logger instance.

Examples:
    >>> # Get default logger
    >>> logger = spdlog.LogManager.default_logger()
    >>>
    >>> # Create named logger
    >>> custom_logger = spdlog.LogManager.create_logger("my_module")
    >>>
    >>> # Get existing logger by name
    >>> same_logger = spdlog.LogManager.get_logger("my_module")
)")
        .def_static("default_logger", &modern_log::LogManager::default_logger,
                    py::return_value_policy::reference,
                    R"(Get the default logger instance.

Returns:
    Reference to the default logger.
)")
        .def_static("create_logger", &modern_log::LogManager::create_logger,
                    py::arg("name"), py::return_value_policy::reference,
                    R"(Create a new named logger.

Args:
    name: The name of the logger.

Returns:
    Reference to the newly created logger.
)")
        .def_static("get_logger", &modern_log::LogManager::get_logger,
                    py::arg("name"), py::return_value_policy::reference,
                    R"(Get an existing logger by name.

Args:
    name: The name of the logger.

Returns:
    Reference to the logger, or None if not found.
)")
        .def_static("set_global_level",
                    &modern_log::LogManager::set_global_level, py::arg("level"),
                    R"(Set the global log level for all loggers.

Args:
    level: The global log level.
)")
        .def_static("shutdown", &modern_log::LogManager::shutdown,
                    R"(Shutdown the logging system and flush all loggers.)");

    // Convenience functions for global logging
    m.def(
        "trace",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().trace("{}", message);
        },
        py::arg("message"),
        R"(Log a trace message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "debug",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().debug("{}", message);
        },
        py::arg("message"),
        R"(Log a debug message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "info",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().info("{}", message);
        },
        py::arg("message"),
        R"(Log an info message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "warn",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().warn("{}", message);
        },
        py::arg("message"),
        R"(Log a warning message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "error",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().error("{}", message);
        },
        py::arg("message"),
        R"(Log an error message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "critical",
        [](const std::string& message) {
            modern_log::LogManager::default_logger().critical("{}", message);
        },
        py::arg("message"),
        R"(Log a critical message using the default logger.

Args:
    message: The message to log.
)");

    m.def(
        "set_level",
        [](modern_log::Level level) {
            modern_log::LogManager::set_global_level(level);
        },
        py::arg("level"),
        R"(Set the global log level.

Args:
    level: The global log level.
)");
}
