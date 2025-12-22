#include "atom/error/error_context.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(error_context, m) {
    m.doc() =
        "Error context system for capturing additional debugging information";

    // ErrorId type alias
    m.attr("ErrorId") = py::type::of<std::string>();

    // generateErrorId function
    m.def("generate_error_id", &atom::error::generateErrorId,
          R"(Generate a unique error ID.

Returns:
    str: A unique error identifier

Examples:
    >>> from atom.error import generate_error_id
    >>> error_id = generate_error_id()
    >>> print(error_id)
)");

    // ErrorContext class
    py::class_<atom::error::ErrorContext,
               std::shared_ptr<atom::error::ErrorContext>>(
        m, "ErrorContext",
        R"(Error context information for enhanced debugging.

This class captures comprehensive information about an error including
error code, message, timestamp, thread ID, metadata, and custom user data.

Examples:
    >>> from atom.error import ErrorContext, FileError
    >>> context = ErrorContext(int(FileError.NotFound), "Config file not found")
    >>> context.set_user_data("filename", "config.json")
    >>> context.add_tag("configuration")
    >>> print(context.get_message())
)")
        .def(py::init<int, std::string>(), py::arg("error_code"),
             py::arg("message") = "",
             R"(Constructor with basic error information.

Args:
    error_code (int): The error code
    message (str, optional): Error message
)")
        // Basic error information
        .def("get_error_id", &atom::error::ErrorContext::getErrorId,
             R"(Get the unique error ID.

Returns:
    str: The error ID
)")
        .def("get_error_code", &atom::error::ErrorContext::getErrorCode,
             R"(Get the error code.

Returns:
    int: The error code
)")
        .def("get_message", &atom::error::ErrorContext::getMessage,
             R"(Get the error message.

Returns:
    str: The error message
)")
        .def("get_timestamp", &atom::error::ErrorContext::getTimestamp,
             R"(Get the timestamp when the error occurred.

Returns:
    datetime: The timestamp
)")
        .def(
            "get_thread_id",
            [](const atom::error::ErrorContext& ctx) {
                std::ostringstream oss;
                oss << ctx.getThreadId();
                return oss.str();
            },
            R"(Get the thread ID where the error occurred.

Returns:
    str: String representation of the thread ID
)")
        // Error metadata
        .def("get_severity", &atom::error::ErrorContext::getSeverity,
             R"(Get the error severity level.

Returns:
    ErrorSeverity: The severity level
)")
        .def("get_category", &atom::error::ErrorContext::getCategory,
             R"(Get the error category.

Returns:
    ErrorCategory: The error category
)")
        .def("get_recovery_strategy",
             &atom::error::ErrorContext::getRecoveryStrategy,
             R"(Get the recovery strategy.

Returns:
    ErrorRecoveryStrategy: The recovery strategy
)")
        .def("get_metadata", &atom::error::ErrorContext::getMetadata,
             py::return_value_policy::reference_internal,
             R"(Get the complete error metadata.

Returns:
    ErrorMetadata: The error metadata
)")
        // Context information - user data
        .def(
            "set_user_data",
            [](atom::error::ErrorContext& ctx, const std::string& key,
               py::object value) -> atom::error::ErrorContext& {
                // Convert Python object to std::any
                if (py::isinstance<py::str>(value)) {
                    ctx.setUserData(key, std::any(value.cast<std::string>()));
                } else if (py::isinstance<py::int_>(value)) {
                    ctx.setUserData(key, std::any(value.cast<int>()));
                } else if (py::isinstance<py::float_>(value)) {
                    ctx.setUserData(key, std::any(value.cast<double>()));
                } else if (py::isinstance<py::bool_>(value)) {
                    ctx.setUserData(key, std::any(value.cast<bool>()));
                } else {
                    // For other types, store as string representation
                    ctx.setUserData(
                        key, std::any(py::str(value).cast<std::string>()));
                }
                return ctx;
            },
            py::arg("key"), py::arg("value"),
            py::return_value_policy::reference_internal,
            R"(Set user-defined data.

Args:
    key (str): The data key
    value: The data value (supports str, int, float, bool)

Returns:
    ErrorContext: Self for method chaining

Examples:
    >>> context.set_user_data("user_id", 12345)
    >>> context.set_user_data("operation", "file_read")
)")
        .def(
            "get_user_data",
            [](const atom::error::ErrorContext& ctx,
               const std::string& key) -> py::object {
                auto data = ctx.getUserData(key);
                if (!data.has_value()) {
                    return py::none();
                }
                // Try to convert back to Python types
                try {
                    if (auto* str_val = std::any_cast<std::string>(&data)) {
                        return py::cast(*str_val);
                    } else if (auto* int_val = std::any_cast<int>(&data)) {
                        return py::cast(*int_val);
                    } else if (auto* double_val =
                                   std::any_cast<double>(&data)) {
                        return py::cast(*double_val);
                    } else if (auto* bool_val = std::any_cast<bool>(&data)) {
                        return py::cast(*bool_val);
                    }
                } catch (...) {
                    return py::none();
                }
                return py::none();
            },
            py::arg("key"),
            R"(Get user-defined data.

Args:
    key (str): The data key

Returns:
    The data value, or None if not found
)")
        .def("has_user_data", &atom::error::ErrorContext::hasUserData,
             py::arg("key"),
             R"(Check if user data exists.

Args:
    key (str): The data key

Returns:
    bool: True if the key exists
)")
        // System information
        .def("set_system_info", &atom::error::ErrorContext::setSystemInfo,
             py::arg("key"), py::arg("value"),
             py::return_value_policy::reference_internal,
             R"(Set system information.

Args:
    key (str): The info key
    value (str): The info value

Returns:
    ErrorContext: Self for method chaining

Examples:
    >>> context.set_system_info("hostname", "server01")
    >>> context.set_system_info("pid", "12345")
)")
        .def("get_system_info", &atom::error::ErrorContext::getSystemInfo,
             py::arg("key"),
             R"(Get system information.

Args:
    key (str): The info key

Returns:
    str: The info value, or empty string if not found
)")
        // Tags
        .def("add_tag", &atom::error::ErrorContext::addTag, py::arg("tag"),
             py::return_value_policy::reference_internal,
             R"(Add a tag to the error context.

Args:
    tag (str): The tag to add

Returns:
    ErrorContext: Self for method chaining

Examples:
    >>> context.add_tag("critical")
    >>> context.add_tag("user-facing")
)")
        .def("get_tags", &atom::error::ErrorContext::getTags,
             R"(Get all tags.

Returns:
    list[str]: List of tags
)")
        .def("has_tag", &atom::error::ErrorContext::hasTag, py::arg("tag"),
             R"(Check if a tag exists.

Args:
    tag (str): The tag to check

Returns:
    bool: True if the tag exists
)")
        // Error correlation
        .def("set_correlation_id", &atom::error::ErrorContext::setCorrelationId,
             py::arg("correlation_id"),
             py::return_value_policy::reference_internal,
             R"(Set correlation ID for tracking related errors.

Args:
    correlation_id (str): The correlation ID

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_correlation_id", &atom::error::ErrorContext::getCorrelationId,
             R"(Get the correlation ID.

Returns:
    str: The correlation ID
)")
        .def("set_parent_error_id",
             &atom::error::ErrorContext::setParentErrorId, py::arg("parent_id"),
             py::return_value_policy::reference_internal,
             R"(Set parent error ID for error chains.

Args:
    parent_id (str): The parent error ID

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_parent_error_id",
             &atom::error::ErrorContext::getParentErrorId,
             R"(Get the parent error ID.

Returns:
    str: The parent error ID
)")
        .def("add_child_error_id", &atom::error::ErrorContext::addChildErrorId,
             py::arg("child_id"), py::return_value_policy::reference_internal,
             R"(Add a child error ID.

Args:
    child_id (str): The child error ID

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_child_error_ids",
             &atom::error::ErrorContext::getChildErrorIds,
             R"(Get all child error IDs.

Returns:
    list[str]: List of child error IDs
)")
        // Retry information
        .def("increment_retry_count",
             &atom::error::ErrorContext::incrementRetryCount,
             py::return_value_policy::reference_internal,
             R"(Increment the retry count.

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_retry_count", &atom::error::ErrorContext::getRetryCount,
             R"(Get the current retry count.

Returns:
    int: The retry count
)")
        .def("set_max_retries", &atom::error::ErrorContext::setMaxRetries,
             py::arg("max_retries"),
             py::return_value_policy::reference_internal,
             R"(Set the maximum number of retries.

Args:
    max_retries (int): Maximum retries allowed

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_max_retries", &atom::error::ErrorContext::getMaxRetries,
             R"(Get the maximum number of retries.

Returns:
    int: Maximum retries allowed
)")
        .def("can_retry", &atom::error::ErrorContext::canRetry,
             R"(Check if the operation can be retried.

Returns:
    bool: True if retry count is less than max retries
)")
        // Stack trace integration
        .def("set_stack_trace", &atom::error::ErrorContext::setStackTrace,
             py::arg("stack_trace"),
             py::return_value_policy::reference_internal,
             R"(Set the stack trace.

Args:
    stack_trace (str): The stack trace string

Returns:
    ErrorContext: Self for method chaining
)")
        .def("get_stack_trace", &atom::error::ErrorContext::getStackTrace,
             R"(Get the stack trace.

Returns:
    str: The stack trace
)")
        // Serialization
        .def("to_json", &atom::error::ErrorContext::toJson,
             R"(Convert error context to JSON string.

Returns:
    str: JSON representation of the error context

Examples:
    >>> json_str = context.to_json()
    >>> print(json_str)
)")
        .def("to_string", &atom::error::ErrorContext::toString,
             R"(Convert error context to human-readable string.

Returns:
    str: String representation of the error context
)")
        .def("__str__", &atom::error::ErrorContext::toString,
             "Returns a string representation of the error context.")
        .def("__repr__",
             [](const atom::error::ErrorContext& ctx) {
                 return "<ErrorContext: " + ctx.getErrorId() +
                        ", code=" + std::to_string(ctx.getErrorCode()) + ">";
             })
        // Static factory methods
        .def_static("create", &atom::error::ErrorContext::create,
                    py::arg("error_code"), py::arg("message") = "",
                    R"(Create a new error context.

Args:
    error_code (int): The error code
    message (str, optional): Error message

Returns:
    ErrorContext: A new error context instance

Examples:
    >>> from atom.error import ErrorContext, NetworkError
    >>> context = ErrorContext.create(int(NetworkError.ConnectionLost), "Connection dropped")
)")
        .def_static("create_with_correlation",
                    &atom::error::ErrorContext::createWithCorrelation,
                    py::arg("error_code"), py::arg("correlation_id"),
                    py::arg("message") = "",
                    R"(Create a new error context with correlation ID.

Args:
    error_code (int): The error code
    correlation_id (str): Correlation ID for tracking related errors
    message (str, optional): Error message

Returns:
    ErrorContext: A new error context instance with correlation ID set
)");

    // ErrorContextManager class
    py::class_<atom::error::ErrorContextManager>(
        m, "ErrorContextManager",
        R"(Error context manager for tracking error contexts across the application.

This singleton class manages a registry of error contexts, allowing for
error tracking, correlation, and statistics gathering.

Examples:
    >>> from atom.error import ErrorContextManager, ErrorContext
    >>> manager = ErrorContextManager.get_instance()
    >>> context = ErrorContext.create(100, "Test error")
    >>> manager.register_context(context)
    >>> retrieved = manager.get_context(context.get_error_id())
)")
        .def_static("get_instance",
                    &atom::error::ErrorContextManager::getInstance,
                    py::return_value_policy::reference,
                    R"(Get the singleton instance.

Returns:
    ErrorContextManager: The singleton instance
)")
        .def("register_context",
             &atom::error::ErrorContextManager::registerContext,
             py::arg("context"),
             R"(Register an error context.

Args:
    context (ErrorContext): The error context to register
)")
        .def("get_context", &atom::error::ErrorContextManager::getContext,
             py::arg("error_id"),
             R"(Get error context by ID.

Args:
    error_id (str): The error ID to look up

Returns:
    ErrorContext: The error context, or None if not found
)")
        .def("get_contexts_by_correlation",
             &atom::error::ErrorContextManager::getContextsByCorrelation,
             py::arg("correlation_id"),
             R"(Get all contexts with a specific correlation ID.

Args:
    correlation_id (str): The correlation ID

Returns:
    list[ErrorContext]: List of error contexts with the correlation ID
)")
        .def("get_statistics", &atom::error::ErrorContextManager::getStatistics,
             R"(Get error context statistics.

Returns:
    dict[str, int]: Dictionary of statistics (e.g., total errors, by severity, by category)
)")
        .def("cleanup", &atom::error::ErrorContextManager::cleanup,
             py::arg("max_age") = std::chrono::minutes(60),
             R"(Clear old error contexts.

Args:
    max_age (timedelta, optional): Maximum age of contexts to keep (default: 60 minutes)
)")
        .def("clear", &atom::error::ErrorContextManager::clear,
             R"(Clear all error contexts.

This removes all registered error contexts from the manager.
)");

    // ScopedErrorContext class
    py::class_<atom::error::ScopedErrorContext>(
        m, "ScopedErrorContext",
        R"(RAII helper for automatic error context management.

This class automatically registers an error context when created and
can be used with Python's context manager protocol.

Examples:
    >>> from atom.error import ScopedErrorContext, ErrorContext
    >>> context = ErrorContext.create(100, "Test error")
    >>> with ScopedErrorContext(context) as scoped:
    ...     # Context is automatically managed
    ...     print(scoped.get_context().get_message())
)")
        .def(py::init<std::shared_ptr<atom::error::ErrorContext>>(),
             py::arg("context"),
             R"(Constructor with error context.

Args:
    context (ErrorContext): The error context to manage
)")
        .def("get_context", &atom::error::ScopedErrorContext::getContext,
             R"(Get the managed error context.

Returns:
    ErrorContext: The error context
)")
        .def(
            "__enter__",
            [](atom::error::ScopedErrorContext& self)
                -> atom::error::ScopedErrorContext& { return self; },
            py::return_value_policy::reference_internal,
            "Enter the context manager.")
        .def(
            "__exit__",
            [](atom::error::ScopedErrorContext& self, py::object exc_type,
               py::object exc_val, py::object exc_tb) {
                return false;  // Don't suppress exceptions
            },
            "Exit the context manager.");
}
