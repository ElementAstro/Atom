#include "atom/system/signal.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(signal, m) {
    m.doc() = "Signal handling and management module for the atom package";

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

    // Define SignalStats structure
    py::class_<SignalStats>(m, "SignalStats",
                            "Structure to store signal statistics")
        .def(py::init<>())
        .def_property_readonly(
            "received",
            [](const SignalStats& stats) { return stats.received.load(); },
            "Total number of signals received")
        .def_property_readonly(
            "processed",
            [](const SignalStats& stats) { return stats.processed.load(); },
            "Total number of signals processed")
        .def_property_readonly(
            "dropped",
            [](const SignalStats& stats) { return stats.dropped.load(); },
            "Total number of signals dropped")
        .def_property_readonly(
            "handler_errors",
            [](const SignalStats& stats) { return stats.handlerErrors.load(); },
            "Total number of handler errors")
        .def_property_readonly(
            "last_received",
            [](const SignalStats& stats) { return stats.lastReceived; },
            "Timestamp of last received signal")
        .def_property_readonly(
            "last_processed",
            [](const SignalStats& stats) { return stats.lastProcessed; },
            "Timestamp of last processed signal")
        .def("__repr__", [](const SignalStats& stats) {
            return "<SignalStats received=" +
                   std::to_string(stats.received.load()) +
                   " processed=" + std::to_string(stats.processed.load()) +
                   " dropped=" + std::to_string(stats.dropped.load()) +
                   " handler_errors=" +
                   std::to_string(stats.handlerErrors.load()) + ">";
        });

    // Define SignalHandlerRegistry class
    py::class_<SignalHandlerRegistry>(
        m, "SignalHandlerRegistry",
        R"(Singleton class to manage signal handlers and dispatch signals.

This class handles registering and dispatching signal handlers with priorities.
It also provides a mechanism to set up default crash signal handlers.

Examples:
    >>> from atom.system import signal
    >>> registry = signal.SignalHandlerRegistry.get_instance()
    >>>
    >>> # Define a simple handler
    >>> def handle_interrupt(sig_id):
    ...     print(f"Received interrupt signal: {sig_id}")
    ...
    >>> # Register the handler for SIGINT (usually 2)
    >>> handler_id = registry.set_signal_handler(2, handle_interrupt)
)")
/*
.def_static("get_instance", &SignalHandlerRegistry::getInstance,
                    py::return_value_policy::reference,
                    R"(Get the singleton instance of the SignalHandlerRegistry.

Returns:
    Reference to the singleton SignalHandlerRegistry instance.
)")
*/

        .def(
            "set_signal_handler", &SignalHandlerRegistry::setSignalHandler,
            py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
            py::arg("handler_name") = "",
            R"(Set a signal handler for a specific signal with an optional priority.

Args:
    signal: The signal ID to handle.
    handler: The handler function to execute.
    priority: The priority of the handler. Default is 0.
    handler_name: Optional name for the handler for debugging purposes.

Returns:
    A unique identifier for this handler registration.

Examples:
    >>> def my_handler(sig_id):
    ...     print(f"Handling signal {sig_id}")
    ...
    >>> handler_id = registry.set_signal_handler(15, my_handler, 10, "SIGTERM handler")
)")
        .def("remove_signal_handler_by_id",
             &SignalHandlerRegistry::removeSignalHandlerById,
             py::arg("handler_id"),
             R"(Remove a specific signal handler by its identifier.

Args:
    handler_id: The identifier returned by set_signal_handler

Returns:
    True if handler was successfully removed, False otherwise

Examples:
    >>> success = registry.remove_signal_handler_by_id(handler_id)
)")
        .def("remove_signal_handler",
             &SignalHandlerRegistry::removeSignalHandler, py::arg("signal"),
             py::arg("handler"),
             R"(Remove a specific signal handler for a signal.

Args:
    signal: The signal ID to stop handling.
    handler: The handler function to remove.

Returns:
    True if handler was successfully removed, False otherwise
)")
        .def("set_standard_crash_handler_signals",
             &SignalHandlerRegistry::setStandardCrashHandlerSignals,
             py::arg("handler"), py::arg("priority") = 0,
             py::arg("handler_name") = "",
             R"(Set handlers for standard crash signals.

Args:
    handler: The handler function to execute for crash signals.
    priority: The priority of the handler. Default is 0.
    handler_name: Optional name for the handler for debugging purposes.

Returns:
    Vector of handler IDs created for each signal

Examples:
    >>> def crash_handler(sig_id):
    ...     print(f"Application is crashing with signal {sig_id}")
    ...     # Perform cleanup
    ...
    >>> handler_ids = registry.set_standard_crash_handler_signals(crash_handler)
)")
        .def("process_all_pending_signals",
             &SignalHandlerRegistry::processAllPendingSignals,
             py::arg("timeout") = std::chrono::milliseconds(0),
             R"(Process all pending signals synchronously

Args:
    timeout: Maximum time to spend processing signals (0 means no limit)

Returns:
    Number of signals processed

Examples:
    >>> # Process signals with a 100ms timeout
    >>> processed = registry.process_all_pending_signals(100)
    >>> print(f"Processed {processed} signals")
)")
        .def("has_handlers_for_signal",
             &SignalHandlerRegistry::hasHandlersForSignal, py::arg("signal"),
             R"(Check if a signal has any registered handlers

Args:
    signal: The signal ID to check

Returns:
    True if the signal has registered handlers
)")
        .def("get_signal_stats", &SignalHandlerRegistry::getSignalStats,
             py::arg("signal"), py::return_value_policy::reference,
             R"(Get statistics for a specific signal

Args:
    signal: The signal to get stats for

Returns:
    Reference to the stats for the signal

Examples:
    >>> stats = registry.get_signal_stats(2)  # Stats for SIGINT
    >>> print(f"Received: {stats.received}, Processed: {stats.processed}")
)")
        .def("reset_stats", &SignalHandlerRegistry::resetStats,
             py::arg("signal") = -1,
             R"(Reset statistics for all or a specific signal

Args:
    signal: The signal to reset (default -1 means all signals)

Examples:
    >>> registry.reset_stats()  # Reset all stats
    >>> registry.reset_stats(2)  # Reset stats for SIGINT only
)")
        .def("set_handler_timeout", &SignalHandlerRegistry::setHandlerTimeout,
             py::arg("timeout"),
             R"(Set the timeout for signal handlers

Args:
    timeout: Maximum time a handler can run before being considered hanging

Examples:
    >>> # Set a 2 second timeout for handlers
    >>> registry.set_handler_timeout(2000)
)")
        .def("execute_handler_with_timeout",
             &SignalHandlerRegistry::executeHandlerWithTimeout,
             py::arg("handler"), py::arg("signal"),
             R"(Execute a handler with timeout protection

Args:
    handler: The handler to execute
    signal: The signal to pass to the handler

Returns:
    True if handler completed successfully, False if it timed out

Examples:
    >>> def long_running_handler(sig_id):
    ...     # Potentially long operation
    ...     import time
    ...     time.sleep(0.5)
    ...
    >>> success = registry.execute_handler_with_timeout(long_running_handler, 2)
)");

    // Define SafeSignalManager class
    py::class_<SafeSignalManager>(
        m, "SafeSignalManager",
        R"(Class to safely manage and dispatch signals with separate thread handling.

This class allows adding and removing signal handlers and dispatching signals
in a separate thread to ensure thread safety and avoid blocking signal handling.

Args:
    thread_count: Number of worker threads to handle signals (default: 1)
    queue_size: Maximum size of the signal queue (default: 1000)

Examples:
    >>> from atom.system import signal
    >>> manager = signal.SafeSignalManager.get_instance()
    >>>
    >>> # Define a signal handler function
    >>> def handle_signal(sig_id):
    ...     print(f"Handled signal {sig_id} safely in separate thread")
    ...
    >>> # Register the handler
    >>> handler_id = manager.add_safe_signal_handler(2, handle_signal)
)")
        .def(py::init<size_t, size_t>(), py::arg("thread_count") = 1,
             py::arg("queue_size") = 1000,
             "Constructs a SafeSignalManager and starts the signal processing "
             "thread.")
        .def_static("get_instance", &SafeSignalManager::getInstance,
                    py::return_value_policy::reference,
                    R"(Get the singleton instance of SafeSignalManager.

Returns:
    A reference to the singleton SafeSignalManager instance.
)")
        .def(
            "add_safe_signal_handler", &SafeSignalManager::addSafeSignalHandler,
            py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
            py::arg("handler_name") = "",
            R"(Add a signal handler for a specific signal with an optional priority.

Args:
    signal: The signal ID to handle.
    handler: The handler function to execute.
    priority: The priority of the handler. Default is 0.
    handler_name: Optional name for the handler for debugging purposes.

Returns:
    A unique identifier for this handler registration.

Examples:
    >>> def safe_handler(sig_id):
    ...     print(f"Safe handling of signal {sig_id}")
    ...
    >>> handler_id = manager.add_safe_signal_handler(15, safe_handler)
)")
        .def("remove_safe_signal_handler_by_id",
             &SafeSignalManager::removeSafeSignalHandlerById,
             py::arg("handler_id"),
             R"(Remove a specific signal handler by its identifier.

Args:
    handler_id: The identifier returned by add_safe_signal_handler

Returns:
    True if handler was successfully removed, False otherwise

Examples:
    >>> success = manager.remove_safe_signal_handler_by_id(handler_id)
)")
        .def("remove_safe_signal_handler",
             &SafeSignalManager::removeSafeSignalHandler, py::arg("signal"),
             py::arg("handler"),
             R"(Remove a specific signal handler for a signal.

Args:
    signal: The signal ID to stop handling.
    handler: The handler function to remove.

Returns:
    True if handler was successfully removed, False otherwise
)")
        .def("clear_signal_queue", &SafeSignalManager::clearSignalQueue,
             R"(Clear the signal queue

Returns:
    Number of signals cleared from the queue

Examples:
    >>> cleared = manager.clear_signal_queue()
    >>> print(f"Cleared {cleared} pending signals")
)")
        .def("queue_signal", &SafeSignalManager::queueSignal, py::arg("signal"),
             R"(Manually queue a signal for processing

Args:
    signal: The signal to queue

Returns:
    True if signal was queued, False if queue is full

Examples:
    >>> # Manually queue SIGTERM
    >>> success = manager.queue_signal(15)
)")
        .def("get_queue_size", &SafeSignalManager::getQueueSize,
             R"(Get current queue size

Returns:
    Current number of signals in the queue

Examples:
    >>> size = manager.get_queue_size()
    >>> print(f"There are {size} signals waiting to be processed")
)")
        .def("get_signal_stats", &SafeSignalManager::getSignalStats,
             py::arg("signal"), py::return_value_policy::reference,
             R"(Get statistics for a specific signal

Args:
    signal: The signal to get stats for

Returns:
    Reference to the stats for the signal

Examples:
    >>> stats = manager.get_signal_stats(2)  # Stats for SIGINT
    >>> print(f"Received: {stats.received}, Processed: {stats.processed}")
)")
        .def("reset_stats", &SafeSignalManager::resetStats,
             py::arg("signal") = -1,
             R"(Reset statistics for all or a specific signal

Args:
    signal: The signal to reset (default -1 means all signals)

Examples:
    >>> manager.reset_stats()  # Reset all stats
    >>> manager.reset_stats(2)  # Reset stats for SIGINT only
)")
        .def("set_worker_thread_count",
             &SafeSignalManager::setWorkerThreadCount, py::arg("thread_count"),
             R"(Configure the number of worker threads

Args:
    thread_count: New number of worker threads

Returns:
    True if change was successful, False otherwise

Examples:
    >>> # Use 4 worker threads for parallel signal handling
    >>> success = manager.set_worker_thread_count(4)
)")
        .def("set_max_queue_size", &SafeSignalManager::setMaxQueueSize,
             py::arg("size"),
             R"(Set the maximum queue size

Args:
    size: New maximum queue size

Examples:
    >>> manager.set_max_queue_size(5000)  # Increase queue capacity
)");

    // Define module-level functions
    m.def("install_platform_specific_handlers",
          &installPlatformSpecificHandlers,
          R"(Register signal handlers for platform-specific signals

Examples:
    >>> from atom.system import signal
    >>> signal.install_platform_specific_handlers()
)");

    m.def("initialize_signal_system", &initializeSignalSystem,
          py::arg("worker_thread_count") = 1, py::arg("queue_size") = 1000,
          R"(Initialize the signal handling system with reasonable defaults

Args:
    worker_thread_count: Number of worker threads for SafeSignalManager
    queue_size: Size of the signal queue

Examples:
    >>> from atom.system import signal
    >>> # Initialize with 2 worker threads and a larger queue
    >>> signal.initialize_signal_system(2, 2000)
)");

    // Define convenient constants for common signal values
    m.attr("SIGINT") = py::int_(SIGINT);
    m.attr("SIGTERM") = py::int_(SIGTERM);
    m.attr("SIGSEGV") = py::int_(SIGSEGV);
    m.attr("SIGABRT") = py::int_(SIGABRT);
    m.attr("SIGFPE") = py::int_(SIGFPE);
    m.attr("SIGILL") = py::int_(SIGILL);

#ifdef SIGQUIT
    m.attr("SIGQUIT") = py::int_(SIGQUIT);
#endif
#ifdef SIGHUP
    m.attr("SIGHUP") = py::int_(SIGHUP);
#endif
#ifdef SIGKILL
    m.attr("SIGKILL") = py::int_(SIGKILL);
#endif
#ifdef SIGUSR1
    m.attr("SIGUSR1") = py::int_(SIGUSR1);
#endif
#ifdef SIGUSR2
    m.attr("SIGUSR2") = py::int_(SIGUSR2);
#endif

    // Add utility functions
    m.def(
        "create_simple_handler",
        [](const std::string& message) {
            return [message](SignalID sig) {
                py::gil_scoped_acquire acquire;
                py::print(message, "Signal:", sig);
            };
        },
        py::arg("message"),
        R"(Create a simple handler that prints a message when a signal is received.

Args:
    message: The message to print when signal is received

Returns:
    A handler function that can be registered with SignalHandlerRegistry

Examples:
    >>> from atom.system import signal
    >>> # Create a simple handler
    >>> handler = signal.create_simple_handler("Received signal:")
    >>> # Register it
    >>> registry = signal.SignalHandlerRegistry.get_instance()
    >>> registry.set_signal_handler(signal.SIGINT, handler)
)");

    m.def(
        "create_logging_handler",
        [](const std::string& log_format, bool include_timestamp) {
            return [log_format, include_timestamp](SignalID sig) {
                py::gil_scoped_acquire acquire;
                std::string message = log_format;
                if (include_timestamp) {
                    auto now = std::chrono::system_clock::now();
                    auto now_time_t = std::chrono::system_clock::to_time_t(now);
                    char time_str[100];
                    std::strftime(time_str, sizeof(time_str),
                                  "%Y-%m-%d %H:%M:%S",
                                  std::localtime(&now_time_t));
                    message = std::string(time_str) + " " + message;
                }
                py::print(message, sig);
            };
        },
        py::arg("log_format") = "Received signal:",
        py::arg("include_timestamp") = true,
        R"(Create a handler that logs signals with optional timestamp.

Args:
    log_format: The format string for the log message
    include_timestamp: Whether to include a timestamp in the log

Returns:
    A handler function that can be registered with SignalHandlerRegistry

Examples:
    >>> from atom.system import signal
    >>> # Create a logging handler
    >>> handler = signal.create_logging_handler("SIGNAL RECEIVED:", True)
    >>> # Register it for SIGTERM
    >>> registry = signal.SignalHandlerRegistry.get_instance()
    >>> registry.set_signal_handler(signal.SIGTERM, handler)
)");
}
