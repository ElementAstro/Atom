#include "atom/system/signal_monitor.hpp"
#include "atom/system/signal.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(signal_monitor, m) {
    m.doc() =
        "Signal monitoring module for collecting signal statistics and "
        "activity";

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

    // First bind the SignalStats struct to make it available
    py::class_<SignalStats>(m, "SignalStats",
                            R"(Statistics for a specific signal.

This structure contains information about signal activity, including counts of
received, processed, and dropped signals, as well as timestamps of the most recent events.

Examples:
    >>> from atom.system import signal_monitor
    >>> # Get statistics for a monitored signal
    >>> monitor = signal_monitor.get_instance()
    >>> stats = monitor.get_stat_snapshot()
    >>> for signal_id, signal_stats in stats.items():
    ...     print(f"Signal {signal_id}: Received {signal_stats.received}")
)")
        .def(py::init<>(), "Constructs a new SignalStats object.")
        .def_readonly("received", &SignalStats::received,
                      "Number of times the signal has been received")
        .def_readonly("processed", &SignalStats::processed,
                      "Number of times the signal has been processed")
        .def_readonly("dropped", &SignalStats::dropped,
                      "Number of times the signal has been dropped")
        .def_readonly("handler_errors", &SignalStats::handlerErrors,
                      "Number of errors occurred during signal handling")
        .def_readonly(
            "last_received", &SignalStats::lastReceived,
            "Timestamp of when the signal was last received (steady_clock)")
        .def_readonly(
            "last_processed", &SignalStats::lastProcessed,
            "Timestamp of when the signal was last processed (steady_clock)")
        .def("__repr__", [](const SignalStats& stats) {
            // Create string representing the timestamp
            std::string last_received_str = "N/A";
            std::string last_processed_str = "N/A";

            // Note: Converting steady_clock to a calendar time is generally
            // problematic as its epoch is arbitrary. This conversion is for
            // representation purposes.
            if (stats.lastReceived.time_since_epoch().count() > 0) {
                auto steady_duration = stats.lastReceived.time_since_epoch();
                std::chrono::system_clock::time_point system_time_point(
                    std::chrono::duration_cast<
                        std::chrono::system_clock::duration>(steady_duration));
                auto received_time_t =
                    std::chrono::system_clock::to_time_t(system_time_point);
                std::tm tm_received;
#ifdef _WIN32
                localtime_s(&tm_received, &received_time_t);
#else
                localtime_r(&received_time_t, &tm_received);
#endif
                char time_str_buf[100];
                std::strftime(time_str_buf, sizeof(time_str_buf),
                              "%Y-%m-%d %H:%M:%S", &tm_received);
                last_received_str = time_str_buf;
            }

            if (stats.lastProcessed.time_since_epoch().count() > 0) {
                auto steady_duration = stats.lastProcessed.time_since_epoch();
                std::chrono::system_clock::time_point system_time_point(
                    std::chrono::duration_cast<
                        std::chrono::system_clock::duration>(steady_duration));
                auto processed_time_t =
                    std::chrono::system_clock::to_time_t(system_time_point);
                std::tm tm_processed;
#ifdef _WIN32
                localtime_s(&tm_processed, &processed_time_t);
#else
                localtime_r(&processed_time_t, &tm_processed);
#endif
                char time_str_buf[100];
                std::strftime(time_str_buf, sizeof(time_str_buf),
                              "%Y-%m-%d %H:%M:%S", &tm_processed);
                last_processed_str = time_str_buf;
            }

            return "<SignalStats received=" +
                   std::to_string(stats.received.load()) +
                   " processed=" + std::to_string(stats.processed.load()) +
                   " dropped=" + std::to_string(stats.dropped.load()) +
                   " errors=" + std::to_string(stats.handlerErrors.load()) +
                   " last_received=\"" + last_received_str + "\"" +
                   " last_processed=\"" + last_processed_str + "\">";
        });

    // Define the callback wrapper type
    using PySignalMonitorCallback =
        std::function<void(int, const SignalStats&)>;

    // SignalMonitor class binding - as a singleton
    py::class_<SignalMonitor, std::unique_ptr<SignalMonitor, py::nodelete>>(
        m, "SignalMonitor",
        R"(Class to monitor signal activity and collect statistics.

This singleton class provides methods to monitor signals, collect statistics,
and register callbacks for various signal events.

Examples:
    >>> from atom.system import signal_monitor
    >>> import time
    >>>
    >>> # Get the singleton instance
    >>> monitor = signal_monitor.get_instance()
    >>>
    >>> # Start monitoring all signals
    >>> monitor.start()
    >>>
    >>> # Wait a bit to collect stats
    >>> time.sleep(5)
    >>>
    >>> # Get a snapshot of signal statistics
    >>> stats = monitor.get_stat_snapshot()
    >>> for signal_id, signal_stats in stats.items():
    ...     print(f"Signal {signal_id}: Received {signal_stats.received}")
    >>>
    >>> # Stop monitoring
    >>> monitor.stop()
)")
        .def(
            "start",
            [](SignalMonitor& self, std::chrono::milliseconds monitorInterval,
               const std::vector<SignalID>& signalsToMonitor) {
                py::gil_scoped_release release;
                self.start(monitorInterval, signalsToMonitor);
            },
            py::arg("monitor_interval") = std::chrono::milliseconds(1000),
            py::arg("signals_to_monitor") = std::vector<SignalID>{},
            R"(Start monitoring signals.

Args:
    monitor_interval: How often to check signal statistics (milliseconds, default: 1000)
    signals_to_monitor: List of signals to monitor (empty list = all signals)

Examples:
    >>> from atom.system import signal_monitor
    >>> # Start monitoring all signals, checking every 500ms
    >>> monitor = signal_monitor.get_instance()
    >>> monitor.start(500)
    >>>
    >>> # Or monitor specific signals
    >>> import signal
    >>> monitor.start(1000, [signal.SIGINT, signal.SIGTERM])
)")
        .def(
            "stop",
            [](SignalMonitor& self) {
                py::gil_scoped_release release;
                self.stop();
            },
            R"(Stop monitoring signals.

Examples:
    >>> from atom.system import signal_monitor
    >>> monitor = signal_monitor.get_instance()
    >>> monitor.start()
    >>> # Later...
    >>> monitor.stop()
)")
        .def(
            "add_threshold_callback",
            [](SignalMonitor& self, SignalID signal, uint64_t receivedThreshold,
               uint64_t errorThreshold, PySignalMonitorCallback callback) {
                // Wrap the Python callback to ensure GIL acquisition
                SignalMonitorCallback cpp_callback =
                    [callback](SignalID sig, const SignalStats& stats) {
                        py::gil_scoped_acquire acquire;
                        try {
                            callback(sig, stats);
                        } catch (py::error_already_set& e) {
                            PyErr_Print();
                        }
                    };

                return self.addThresholdCallback(signal, receivedThreshold,
                                                 errorThreshold, cpp_callback);
            },
            py::arg("signal"), py::arg("received_threshold"),
            py::arg("error_threshold"), py::arg("callback"),
            R"(Add a callback for when a signal exceeds a threshold.

Args:
    signal: The signal to monitor
    received_threshold: Callback triggered when received count exceeds this value
    error_threshold: Callback triggered when error count exceeds this value
    callback: The callback function to execute (takes signal_id and signal_stats)

Returns:
    ID of the registered callback

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>>
    >>> # Define a callback function
    >>> def on_signal_threshold(signal_id, stats):
    ...     print(f"Signal {signal_id} threshold exceeded!")
    ...     print(f"Received: {stats.received}, Errors: {stats.handler_errors}")
    ...
    >>> # Register callback for SIGINT - triggered after 5 occurrences
    >>> monitor = signal_monitor.get_instance()
    >>> callback_id = monitor.add_threshold_callback(
    ...     signal.SIGINT, 5, 1, on_signal_threshold
    ... )
    >>> print(f"Registered callback with ID: {callback_id}")
)")
        .def(
            "add_inactivity_callback",
            [](SignalMonitor& self, SignalID signal,
               std::chrono::milliseconds inactivityPeriod,
               PySignalMonitorCallback callback) {
                // Wrap the Python callback to ensure GIL acquisition
                SignalMonitorCallback cpp_callback =
                    [callback](SignalID sig, const SignalStats& stats) {
                        py::gil_scoped_acquire acquire;
                        try {
                            callback(sig, stats);
                        } catch (py::error_already_set& e) {
                            PyErr_Print();
                        }
                    };

                return self.addInactivityCallback(signal, inactivityPeriod,
                                                  cpp_callback);
            },
            py::arg("signal"), py::arg("inactivity_period"),
            py::arg("callback"),
            R"(Add a callback for when a signal has been inactive for a period.

Args:
    signal: The signal to monitor
    inactivity_period: Time without activity to trigger callback (milliseconds)
    callback: The callback function to execute (takes signal_id and signal_stats)

Returns:
    ID of the registered callback

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>> import time
    >>>
    >>> # Define a callback function
    >>> def on_signal_inactivity(signal_id, stats):
    ...     print(f"Signal {signal_id} has been inactive for too long!")
    ...     print(f"Last received: {stats.last_received}")
    ...
    >>> # Register callback for SIGTERM - triggered after 30 seconds of inactivity
    >>> monitor = signal_monitor.get_instance()
    >>> callback_id = monitor.add_inactivity_callback(
    ...     signal.SIGTERM, 30000, on_signal_inactivity
    ... )
    >>> print(f"Registered callback with ID: {callback_id}")
)")
        .def("remove_callback", &SignalMonitor::removeCallback,
             py::arg("callback_id"),
             R"(Remove a callback by ID.

Args:
    callback_id: ID of the callback to remove

Returns:
    True if callback was successfully removed, False otherwise

Examples:
    >>> from atom.system import signal_monitor
    >>> monitor = signal_monitor.get_instance()
    >>>
    >>> # Add a callback
    >>> def callback(signal_id, stats):
    ...     print(f"Signal {signal_id} event")
    ...
    >>> callback_id = monitor.add_threshold_callback(
    ...     signal.SIGINT, 5, 0, callback
    ... )
    >>>
    >>> # Later, remove the callback
    >>> success = monitor.remove_callback(callback_id)
    >>> print(f"Callback removed: {success}")
)")
        .def("get_stat_snapshot", &SignalMonitor::getStatSnapshot,
             R"(Get a snapshot of signal statistics.

Returns:
    Dictionary mapping signal IDs to their statistics

Examples:
    >>> from atom.system import signal_monitor
    >>> monitor = signal_monitor.get_instance()
    >>>
    >>> # Get stats for all monitored signals
    >>> stats = monitor.get_stat_snapshot()
    >>> for signal_id, signal_stats in stats.items():
    ...     print(f"Signal {signal_id}:")
    ...     print(f"  Received: {signal_stats.received}")
    ...     print(f"  Processed: {signal_stats.processed}")
    ...     print(f"  Errors: {signal_stats.handler_errors}")
)")
        .def("get_monitored_signals", &SignalMonitor::getMonitoredSignals,
             R"(Get a list of all monitored signals.

Returns:
    List of monitored signal IDs

Examples:
    >>> from atom.system import signal_monitor
    >>> monitor = signal_monitor.get_instance()
    >>>
    >>> # Get list of monitored signals
    >>> signals = monitor.get_monitored_signals()
    >>> print(f"Monitoring {len(signals)} signals: {signals}")
)")
        .def("reset_all_stats", &SignalMonitor::resetAllStats,
             R"(Reset all monitoring statistics.

Examples:
    >>> from atom.system import signal_monitor
    >>> monitor = signal_monitor.get_instance()
    >>>
    >>> # Reset all stats to zero
    >>> monitor.reset_all_stats()
    >>> print("All signal statistics have been reset")
)");

    // Add singleton accessor function
    m.def(
        "get_instance",
        []() {
            return std::unique_ptr<SignalMonitor, py::nodelete>(
                &SignalMonitor::getInstance());
        },
        R"(Get the singleton instance of SignalMonitor.

Returns:
    The SignalMonitor singleton instance

Examples:
    >>> from atom.system import signal_monitor
    >>> # Get the singleton instance
    >>> monitor = signal_monitor.get_instance()
)",
        py::return_value_policy::reference);

    // Add convenience functions for common operations
    m.def(
        "start_monitoring",
        [](std::chrono::milliseconds interval,
           const std::vector<SignalID>& signals) {
            SignalMonitor::getInstance().start(interval, signals);
        },
        py::arg("interval") = std::chrono::milliseconds(1000),
        py::arg("signals") = std::vector<SignalID>{},
        R"(Start signal monitoring with the given parameters.

This is a convenience function to get the SignalMonitor instance and start it.

Args:
    interval: How often to check signal statistics (milliseconds, default: 1000)
    signals: List of signals to monitor (empty list = all signals)

Examples:
    >>> from atom.system import signal_monitor
    >>> # Start monitoring all signals
    >>> signal_monitor.start_monitoring()
    >>>
    >>> # Or monitor specific signals with custom interval
    >>> import signal
    >>> signal_monitor.start_monitoring(500, [signal.SIGINT, signal.SIGTERM])
)");

    m.def(
        "stop_monitoring", []() { SignalMonitor::getInstance().stop(); },
        R"(Stop signal monitoring.

This is a convenience function to get the SignalMonitor instance and stop it.

Examples:
    >>> from atom.system import signal_monitor
    >>> # Start monitoring
    >>> signal_monitor.start_monitoring()
    >>> # Later...
    >>> signal_monitor.stop_monitoring()
)");

    m.def(
        "get_signal_stats",
        [](SignalID signal) {
            auto stats = SignalMonitor::getInstance().getStatSnapshot();
            auto it = stats.find(signal);
            if (it != stats.end()) {
                return it->second;
            }
            return SignalStats();
        },
        py::arg("signal"),
        R"(Get statistics for a specific signal.

Args:
    signal: The signal ID to get statistics for

Returns:
    SignalStats object with the signal's statistics

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>> # Get stats for SIGINT
    >>> stats = signal_monitor.get_signal_stats(signal.SIGINT)
    >>> print(f"SIGINT received {stats.received} times")
)");

    // Helper for monitoring multiple signals with a single callback
    py::class_<py::object>(m, "SignalMonitorGroup")
        .def(py::init([](const std::vector<SignalID>& signals,
                         py::function callback,
                         std::chrono::milliseconds interval) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("signals"), py::arg("callback"),
             py::arg("interval") = std::chrono::milliseconds(1000),
             "Create a signal monitor group for multiple signals")
        .def("__enter__",
             [](py::object& self, const std::vector<SignalID>& signals,
                py::function callback, std::chrono::milliseconds interval) {
                 // Store signals and start monitoring
                 self.attr("signals") = py::cast(signals);
                 self.attr("callback_ids") = py::list();

                 // Start monitoring if not already running
                 SignalMonitor& monitor = SignalMonitor::getInstance();

                 // Create C++ callback that will call the Python function
                 SignalMonitorCallback cpp_callback =
                     [callback](SignalID sig, const SignalStats& stats) {
                         py::gil_scoped_acquire acquire;
                         try {
                             callback(sig, stats);
                         } catch (py::error_already_set& e) {
                             PyErr_Print();
                         }
                     };

                 // Add inactivity callbacks for each signal
                 for (SignalID signal : signals) {
                     int id = monitor.addInactivityCallback(signal, interval,
                                                            cpp_callback);
                     self.attr("callback_ids").attr("append")(id);
                 }

                 // Start monitoring if not already started
                 monitor.start(interval, signals);

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 py::list callback_ids = self.attr("callback_ids");
                 SignalMonitor& monitor = SignalMonitor::getInstance();

                 // Remove all callbacks
                 for (auto id : callback_ids) {
                     monitor.removeCallback(py::cast<int>(id));
                 }

                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for signal monitor group
    m.def(
        "monitor_signals",
        [&m](const std::vector<SignalID>& signals, py::function callback,
             std::chrono::milliseconds interval) {
            return m.attr("SignalMonitorGroup")(signals, callback, interval);
        },
        py::arg("signals"), py::arg("callback"),
        py::arg("interval") = std::chrono::milliseconds(1000),
        R"(Create a context manager for monitoring multiple signals.

This function returns a context manager that sets up monitoring for multiple signals
and removes the monitoring when the context is exited.

Args:
    signals: List of signals to monitor
    callback: Function to call when signal activity is detected
    interval: Monitoring interval in milliseconds (default: 1000)

Returns:
    A context manager for signal monitoring

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>>
    >>> def on_signal_event(signal_id, stats):
    ...     print(f"Signal {signal_id} event detected!")
    ...
    >>> # Use as a context manager to monitor signals
    >>> with signal_monitor.monitor_signals(
    ...     [signal.SIGINT, signal.SIGTERM], on_signal_event, 500
    ... ):
    ...     print("Monitoring signals in this block...")
    ...     # Your code here
    ...
    >>> print("Signal monitoring stopped")
)");

    // Add utility for waiting for a signal
    m.def(
        "wait_for_signal",
        [](SignalID signal, std::chrono::milliseconds timeout) {
            SignalMonitor& monitor = SignalMonitor::getInstance();

            // Create an event to wait on
            py::object threading = py::module::import("threading");
            py::object event = threading.attr("Event")();

            // Get initial stats to compare against
            auto initial_stats_map = monitor.getStatSnapshot();
            uint64_t initial_count = 0;

            auto it_initial = initial_stats_map.find(signal);
            if (it_initial != initial_stats_map.end()) {
                initial_count = it_initial->second.received.load();
            }

            // Add a threshold callback that will be triggered on the next
            // occurrence
            SignalMonitorCallback callback = [&event, initial_count, signal](
                                                 SignalID sig,
                                                 const SignalStats& stats) {
                if (sig == signal && stats.received.load() > initial_count) {
                    py::gil_scoped_acquire acquire;
                    event.attr("set")();
                }
            };

            // Register the callback and start monitoring
            int callback_id = monitor.addThresholdCallback(
                signal, initial_count + 1, 0,
                callback);  // Trigger when received > initial_count
            monitor.start(std::chrono::milliseconds(50), {signal});

            // Wait for the event with timeout
            bool result;
            if (timeout.count() <=
                0) {  // Negative or zero timeout means wait indefinitely
                event.attr("wait")();
                result = true;
            } else {
                double timeout_seconds =
                    static_cast<double>(timeout.count()) / 1000.0;
                result = py::cast<bool>(event.attr("wait")(timeout_seconds));
            }

            // Clean up
            monitor.removeCallback(callback_id);

            return result;
        },
        py::arg("signal"), py::arg("timeout") = std::chrono::milliseconds(-1),
        R"(Wait for a specific signal to occur.

Args:
    signal: The signal ID to wait for
    timeout: Maximum time to wait in milliseconds (-1 for no timeout)

Returns:
    True if the signal was received, False if timed out

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>> import threading
    >>> import os
    >>> import time
    >>>
    >>> # Set up a thread to send a signal after 1 second
    >>> def send_test_signal(pid, sig_to_send):
    ...     time.sleep(1)
    ...     os.kill(pid, sig_to_send)
    ...
    >>> # Note: SIGUSR1 might not be available on Windows without specific setup.
    >>> # Using SIGINT for a more portable example, though be careful with terminal interruption.
    >>> # For a real test, use a signal like SIGUSR1 if available and handled.
    >>> test_signal = signal.SIGUSR1 if hasattr(signal, "SIGUSR1") else signal.SIGINT
    >>> pid = os.getpid()
    >>> threading.Thread(target=send_test_signal, args=(pid, test_signal)).start()
    >>>
    >>> # Wait for the signal with 2 second timeout
    >>> print(f"Waiting for signal {test_signal}...")
    >>> if signal_monitor.wait_for_signal(test_signal, 2000):
    ...     print(f"Received signal {test_signal} as expected")
    ... else:
    ...     print("Timed out waiting for signal")
)");

    // Helper functions for common signal monitoring operations
    m.def(
        "is_signal_active",
        [](SignalID signal, std::chrono::milliseconds within) {
            SignalMonitor& monitor = SignalMonitor::getInstance();
            auto stats_map = monitor.getStatSnapshot();

            auto it = stats_map.find(signal);
            if (it == stats_map.end()) {
                return false;  // Signal not found or not monitored
            }

            // Compare with steady_clock::now() as lastReceived is steady_clock
            auto now_steady = std::chrono::steady_clock::now();
            auto last_received_steady = it->second.lastReceived;

            // Ensure last_received is not its default epoch value if it hasn't
            // occurred
            if (last_received_steady.time_since_epoch().count() == 0) {
                return false;
            }

            return (now_steady - last_received_steady) < within;
        },
        py::arg("signal"), py::arg("within") = std::chrono::seconds(10),
        R"(Check if a signal has been active recently.

Args:
    signal: The signal ID to check
    within: Time period to consider (milliseconds, default: 10000)

Returns:
    True if the signal was received within the specified period

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>> # Check if SIGINT was received in the last minute
    >>> if signal_monitor.is_signal_active(signal.SIGINT, 60000):
    ...     print("SIGINT was active recently")
    ... else:
    ...     print("No recent SIGINT activity")
)");

    // Add class for tracking signal rate
    py::class_<py::object>(m, "SignalRateTracker")
        .def(py::init(
                 [](SignalID signal, std::chrono::milliseconds window_size) {
                     return py::object();  // Placeholder, actual implementation
                                           // in __enter__
                 }),
             py::arg("signal"),
             py::arg("window_size") = std::chrono::seconds(
                 10),  // Default window_size, not used in __init__ directly
             "Create a signal rate tracker to measure signal frequency")
        .def("__enter__",
             [](py::object& self, SignalID signal,
                std::chrono::milliseconds
                    window_size) {  // window_size is from constructor args
                 self.attr("signal_id_attr") = py::int_(
                     signal);  // Use a different name to avoid conflict
                 self.attr("window_size_attr") =
                     py::cast(window_size);  // Store for potential use
                 self.attr("start_time_attr") = py::cast(
                     std::chrono::steady_clock::now());  // Use steady_clock for
                                                         // rate calculation

                 // Get initial stats
                 SignalMonitor& monitor = SignalMonitor::getInstance();
                 auto stats_map = monitor.getStatSnapshot();

                 uint64_t initial_count = 0;
                 auto it_initial = stats_map.find(signal);
                 if (it_initial != stats_map.end()) {
                     initial_count = it_initial->second.received.load();
                 }

                 self.attr("initial_count_attr") = py::int_(initial_count);

                 // Start monitoring if not already started (minimal interval
                 // for responsiveness)
                 monitor.start(std::chrono::milliseconds(100), {signal});

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 return py::bool_(false);  // Don't suppress exceptions
             })
        .def(
            "get_rate",
            [](py::object& self) {
                SignalID signal =
                    py::cast<SignalID>(self.attr("signal_id_attr"));
                uint64_t initial_count =
                    py::cast<uint64_t>(self.attr("initial_count_attr"));
                auto start_time = py::cast<
                    std::chrono::steady_clock::time_point>(  // Expect
                                                             // steady_clock
                    self.attr("start_time_attr"));

                // Get current stats
                SignalMonitor& monitor = SignalMonitor::getInstance();
                auto stats_map = monitor.getStatSnapshot();

                uint64_t current_count =
                    initial_count;  // Default to initial if not found
                auto it_current = stats_map.find(signal);
                if (it_current != stats_map.end()) {
                    current_count = it_current->second.received.load();
                }

                // Calculate elapsed time in seconds using steady_clock
                auto now_steady = std::chrono::steady_clock::now();
                double elapsed_seconds =
                    std::chrono::duration<double>(now_steady - start_time)
                        .count();

                if (elapsed_seconds <= 0) {
                    return 0.0;
                }

                // Calculate rate (signals per second)
                return static_cast<double>(current_count - initial_count) /
                       elapsed_seconds;
            },
            "Get the current signal rate in signals per second");

    // Factory function for signal rate tracker
    m.def(
        "track_signal_rate",
        [&m](SignalID signal, std::chrono::milliseconds window_size) {
            return m.attr("SignalRateTracker")(signal, window_size);
        },
        py::arg("signal"), py::arg("window_size") = std::chrono::seconds(10),
        R"(Create a context manager for tracking signal rate.

This function returns a context manager that measures the rate at which
a signal is being received. The rate is calculated from the moment the
context is entered until get_rate() is called.

Args:
    signal: The signal ID to track
    window_size: This argument is passed to the tracker but the rate calculation
                 is based on elapsed time since context entry, not a fixed window.
                 (Default: 10000ms)


Returns:
    A context manager for signal rate tracking

Examples:
    >>> from atom.system import signal_monitor
    >>> import signal
    >>> import time
    >>> import os
    >>>
    >>> # Note: SIGUSR1 might not be available on Windows.
    >>> test_signal = signal.SIGUSR1 if hasattr(signal, "SIGUSR1") else signal.SIGINT
    >>> pid = os.getpid()
    >>>
    >>> # Use as a context manager to track signal rate
    >>> with signal_monitor.track_signal_rate(test_signal) as tracker:
    ...     # Generate some signals
    ...     def send_signals_thread():
    ...         for _ in range(5):
    ...             try:
    ...                 os.kill(pid, test_signal)
    ...             except Exception as e:
    ...                 print(f"Error sending signal: {e}") // Handle case where signal can't be sent
    ...             time.sleep(0.1)
    ...
    ...     import threading
    ...     sig_thread = threading.Thread(target=send_signals_thread)
    ...     sig_thread.start()
    ...     time.sleep(0.6) // Allow signals to be sent and processed
    ...     sig_thread.join()
    ...
    ...     // Get the rate
    ...     rate = tracker.get_rate()
    ...     print(f"Signal rate for {test_signal}: {rate:.2f} signals per second")
)");
}
