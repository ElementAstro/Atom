#include "atom/error/error_handler.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(error_handler, m) {
    m.doc() = "Thread-safe error handling system with aggregation and reporting";

    // AggregationStrategy enum
    py::enum_<atom::error::AggregationStrategy>(
        m, "AggregationStrategy",
        R"(Error aggregation strategy.

Defines how errors should be aggregated for reporting and analysis.

Examples:
    >>> from atom.error import AggregationStrategy
    >>> strategy = AggregationStrategy.BySeverity
)")
        .value("None", atom::error::AggregationStrategy::None, "No aggregation")
        .value("BySeverity", atom::error::AggregationStrategy::BySeverity, "Aggregate by severity level")
        .value("ByCategory", atom::error::AggregationStrategy::ByCategory, "Aggregate by error category")
        .value("ByCode", atom::error::AggregationStrategy::ByCode, "Aggregate by error code")
        .value("ByCorrelation", atom::error::AggregationStrategy::ByCorrelation, "Aggregate by correlation ID")
        .value("ByTimeWindow", atom::error::AggregationStrategy::ByTimeWindow, "Aggregate by time window")
        .export_values();

    // ErrorReporter class
    py::class_<atom::error::ErrorReporter, std::shared_ptr<atom::error::ErrorReporter>>(
        m, "ErrorReporter",
        R"(Thread-safe error reporter for collecting and processing errors.

This class provides a thread-safe mechanism for reporting and processing
errors asynchronously with support for filtering, aggregation, and statistics.

Examples:
    >>> from atom.error import ErrorReporter, ErrorContext
    >>> reporter = ErrorReporter()
    >>> reporter.start()
    >>> 
    >>> def my_handler(context):
    ...     print(f"Error: {context.get_message()}")
    >>> 
    >>> reporter.add_handler("my_handler", my_handler)
    >>> context = ErrorContext.create(100, "Test error")
    >>> reporter.report_error(context)
    >>> reporter.stop()
)")
        .def(py::init<>(), "Constructs an ErrorReporter.")
        .def("start", &atom::error::ErrorReporter::start,
             R"(Start the error reporter.

Starts the background processing thread for handling errors.
)")
        .def("stop", &atom::error::ErrorReporter::stop,
             R"(Stop the error reporter.

Stops the background processing thread and waits for pending errors.
)")
        .def("is_running", &atom::error::ErrorReporter::isRunning,
             R"(Check if the reporter is running.

Returns:
    bool: True if the reporter is running
)")
        .def("report_error", &atom::error::ErrorReporter::reportError,
             py::arg("context"),
             R"(Report an error.

Args:
    context (ErrorContext): The error context to report

Examples:
    >>> context = ErrorContext.create(100, "Test error")
    >>> reporter.report_error(context)
)")
        .def("add_handler", &atom::error::ErrorReporter::addHandler,
             py::arg("name"), py::arg("handler"),
             R"(Add error handler callback.

Args:
    name (str): Handler name
    handler (callable): Callback function that takes ErrorContext

Examples:
    >>> def log_error(context):
    ...     print(f"Error {context.get_error_code()}: {context.get_message()}")
    >>> reporter.add_handler("logger", log_error)
)")
        .def("remove_handler", &atom::error::ErrorReporter::removeHandler,
             py::arg("name"),
             R"(Remove error handler callback.

Args:
    name (str): Handler name to remove
)")
        .def("add_filter", &atom::error::ErrorReporter::addFilter,
             py::arg("name"), py::arg("filter"),
             R"(Add error filter.

Args:
    name (str): Filter name
    filter (callable): Filter function that takes ErrorContext and returns bool

Examples:
    >>> def critical_only(context):
    ...     return context.get_severity() == ErrorSeverity.Critical
    >>> reporter.add_filter("critical_filter", critical_only)
)")
        .def("remove_filter", &atom::error::ErrorReporter::removeFilter,
             py::arg("name"),
             R"(Remove error filter.

Args:
    name (str): Filter name to remove
)")
        .def("set_aggregation_strategy", &atom::error::ErrorReporter::setAggregationStrategy,
             py::arg("strategy"),
             R"(Set aggregation strategy.

Args:
    strategy (AggregationStrategy): The aggregation strategy to use
)")
        .def("set_aggregation_window", &atom::error::ErrorReporter::setAggregationWindow,
             py::arg("window"),
             R"(Set aggregation time window.

Args:
    window (timedelta): Time window for time-based aggregation
)")
        .def("get_statistics", &atom::error::ErrorReporter::getStatistics,
             R"(Get error statistics.

Returns:
    dict[str, int]: Dictionary of statistics including total errors, 
                    processed errors, filtered errors, and dropped errors
)")
        .def("clear_statistics", &atom::error::ErrorReporter::clearStatistics,
             R"(Clear error statistics.

Resets all statistics counters to zero.
)")
        .def("set_max_queue_size", &atom::error::ErrorReporter::setMaxQueueSize,
             py::arg("max_size"),
             R"(Set maximum queue size.

Args:
    max_size (int): Maximum number of errors to queue
)")
        .def("get_queue_size", &atom::error::ErrorReporter::getQueueSize,
             R"(Get current queue size.

Returns:
    int: Number of errors currently in the queue
)");

    // ErrorAggregator class
    py::class_<atom::error::ErrorAggregator, std::shared_ptr<atom::error::ErrorAggregator>>(
        m, "ErrorAggregator",
        R"(Thread-safe error aggregator for collecting related errors.

This class aggregates errors by various criteria for analysis and reporting.

Examples:
    >>> from atom.error import ErrorAggregator, ErrorContext
    >>> aggregator = ErrorAggregator()
    >>> context = ErrorContext.create(100, "Test error")
    >>> aggregator.add_error(context)
    >>> errors = aggregator.get_aggregated_errors("some_key")
)")
        .def(py::init<>(), "Constructs an ErrorAggregator.")
        .def("add_error", &atom::error::ErrorAggregator::addError,
             py::arg("context"),
             R"(Add error to aggregation.

Args:
    context (ErrorContext): The error context to aggregate
)")
        .def("get_aggregated_errors", &atom::error::ErrorAggregator::getAggregatedErrors,
             py::arg("key"),
             R"(Get aggregated errors by key.

Args:
    key (str): Aggregation key

Returns:
    list[ErrorContext]: List of aggregated error contexts
)")
        .def("get_aggregation_keys", &atom::error::ErrorAggregator::getAggregationKeys,
             R"(Get all aggregation keys.

Returns:
    list[str]: List of all aggregation keys
)")
        .def("clear", &atom::error::ErrorAggregator::clear,
             R"(Clear aggregated errors.

Removes all aggregated errors.
)")
        .def("clear_older_than", &atom::error::ErrorAggregator::clearOlderThan,
             py::arg("max_age"),
             R"(Clear aggregated errors older than specified time.

Args:
    max_age (timedelta): Maximum age of errors to keep
)")
        .def("get_statistics", &atom::error::ErrorAggregator::getStatistics,
             R"(Get aggregation statistics.

Returns:
    dict[str, int]: Dictionary of aggregation statistics
)");

    // GlobalErrorHandler class
    py::class_<atom::error::GlobalErrorHandler>(
        m, "GlobalErrorHandler",
        R"(Global error handler singleton.

Provides a global error handling infrastructure with reporter and aggregator.

Examples:
    >>> from atom.error import GlobalErrorHandler, ErrorContext
    >>> handler = GlobalErrorHandler.get_instance()
    >>> handler.initialize()
    >>> context = ErrorContext.create(100, "Test error")
    >>> handler.report_error(context)
    >>> handler.shutdown()
)")
        .def_static("get_instance", &atom::error::GlobalErrorHandler::getInstance,
                   py::return_value_policy::reference,
                   R"(Get the singleton instance.

Returns:
    GlobalErrorHandler: The singleton instance
)")
        .def("initialize", &atom::error::GlobalErrorHandler::initialize,
             R"(Initialize the global error handler.

Starts the error reporter and sets up the error handling infrastructure.
)")
        .def("shutdown", &atom::error::GlobalErrorHandler::shutdown,
             R"(Shutdown the global error handler.

Stops the error reporter and cleans up resources.
)")
        .def("report_error", &atom::error::GlobalErrorHandler::reportError,
             py::arg("context"),
             R"(Report an error globally.

Args:
    context (ErrorContext): The error context to report
)")
        .def("get_reporter", &atom::error::GlobalErrorHandler::getReporter,
             py::return_value_policy::reference,
             R"(Get the error reporter.

Returns:
    ErrorReporter: The error reporter instance
)")
        .def("get_aggregator", &atom::error::GlobalErrorHandler::getAggregator,
             py::return_value_policy::reference,
             R"(Get the error aggregator.

Returns:
    ErrorAggregator: The error aggregator instance
)")
        .def("set_global_handler", &atom::error::GlobalErrorHandler::setGlobalHandler,
             py::arg("handler"),
             R"(Set global error handler callback.

Args:
    handler (callable): Global error handler function
)")
        .def("set_unhandled_exception_handler", &atom::error::GlobalErrorHandler::setUnhandledExceptionHandler,
             R"(Set unhandled exception handler.

Installs a handler for unhandled C++ exceptions.
)");

    // ThreadLocalErrorHandler class
    py::class_<atom::error::ThreadLocalErrorHandler, std::shared_ptr<atom::error::ThreadLocalErrorHandler>>(
        m, "ThreadLocalErrorHandler",
        R"(RAII helper for thread-local error handling.

Provides thread-local error handling with automatic cleanup.

Examples:
    >>> from atom.error import ThreadLocalErrorHandler, ErrorContext
    >>> handler = ThreadLocalErrorHandler()
    >>> 
    >>> def my_handler(context):
    ...     print(f"Thread error: {context.get_message()}")
    >>> 
    >>> handler.set_handler(my_handler)
    >>> context = ErrorContext.create(100, "Test error")
    >>> handler.report_error(context)
)")
        .def(py::init<>(), "Constructs a ThreadLocalErrorHandler.")
        .def("set_handler", &atom::error::ThreadLocalErrorHandler::setHandler,
             py::arg("handler"),
             R"(Set thread-local error handler.

Args:
    handler (callable): Error handler function for this thread
)")
        .def("report_error", &atom::error::ThreadLocalErrorHandler::reportError,
             py::arg("context"),
             R"(Report error in current thread.

Args:
    context (ErrorContext): The error context to report
)")
        .def("get_statistics", &atom::error::ThreadLocalErrorHandler::getStatistics,
             R"(Get thread-local error statistics.

Returns:
    dict[str, int]: Dictionary of thread-local statistics
)");
}

