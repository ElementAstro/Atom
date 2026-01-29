#include "atom/async/trigger.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <future>
#include <string>

namespace py = pybind11;

// Helper function to convert std::future<std::size_t> to Python
auto future_to_py_object(std::future<std::size_t>& future) {
    try {
        return py::cast(future.get());
    } catch (const std::exception& e) {
        throw py::error_already_set();
    }
}

PYBIND11_MODULE(trigger, m) {
    m.doc() = R"pbdoc(
        Event Trigger Implementation Module
        ----------------------------------

        This module provides a high-performance event trigger system for managing
        callbacks with advanced features for event-driven programming.

        Features:
          - Event-driven callback registration and management
          - Priority-based callback execution (High, Normal, Low)
          - Delayed and asynchronous trigger scheduling
          - Thread-safe operations with shared mutex support
          - Lock-free trigger queues for high-throughput scenarios
          - Comprehensive callback lifecycle management

        The module includes:
          - Trigger: Main event trigger system
          - CallbackPriority: Priority levels for callback execution
          - TriggerException: Exception for trigger-related errors
          - Utility functions for duration handling and trigger creation

        Example:
            >>> from atom.async.trigger import Trigger, CallbackPriority
            >>>
            >>> # Create trigger system
            >>> trigger = Trigger()
            >>>
            >>> # Register callbacks with different priorities
            >>> high_id = trigger.register_callback("event", lambda data: print(f"High: {data}"), CallbackPriority.HIGH)
            >>> normal_id = trigger.register_callback("event", lambda data: print(f"Normal: {data}"), CallbackPriority.NORMAL)
            >>>
            >>> # Trigger event (high priority executes first)
            >>> count = trigger.trigger("event", "test data")
            >>> print(f"Executed {count} callbacks")
            >>>
            >>> # Schedule delayed trigger
            >>> cancel_flag = trigger.schedule_trigger("delayed", "delayed data", milliseconds(1000))
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::TriggerException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Bind the TriggerException
    py::register_exception<atom::async::TriggerException>(m,
                                                          "TriggerException");

    // Define the CallbackPriority enum
    py::enum_<atom::async::Trigger<py::object>::CallbackPriority>(
        m, "CallbackPriority",
        R"(Priority levels for trigger callbacks.

Determines the order in which callbacks are executed when an event is triggered.
)")
        .value("HIGH", atom::async::Trigger<py::object>::CallbackPriority::High,
               "High priority callbacks are executed first.")
        .value(
            "NORMAL",
            atom::async::Trigger<py::object>::CallbackPriority::Normal,
            "Normal priority callbacks are executed after high priority ones.")
        .value("LOW", atom::async::Trigger<py::object>::CallbackPriority::Low,
               "Low priority callbacks are executed last.")
        .export_values();

    // Trigger class binding
    py::class_<atom::async::Trigger<py::object>>(
        m, "Trigger",
        R"(Event trigger system for managing callbacks.

This class provides an event system to register, unregister, and trigger callbacks
for different events with support for priorities and delayed execution.

Examples:
    >>> from atom.async import Trigger, CallbackPriority
    >>> trigger = Trigger()
    >>> def on_event(data):
    ...     print(f"Event received: {data}")
    >>> callback_id = trigger.register_callback("my_event", on_event)
    >>> trigger.trigger("my_event", "Hello, World!")
    Event received: Hello, World!
    >>> trigger.unregister_callback("my_event", callback_id)
)")
        .def(py::init<>(), "Constructs a new Trigger object.")
        .def("register_callback",
             &atom::async::Trigger<py::object>::registerCallback,
             py::arg("event"), py::arg("callback"),
             py::arg("priority") =
                 atom::async::Trigger<py::object>::CallbackPriority::Normal,
             R"(Registers a callback function for a specified event.

Args:
    event: The name of the event for which the callback is registered.
    callback: The function to be called when the event is triggered.
    priority: Priority level for the callback (default: NORMAL).

Returns:
    An identifier that can be used to unregister the callback.

Raises:
    TriggerException: If the event name is empty or the callback is invalid.

Examples:
    >>> callback_id = trigger.register_callback("data_received",
    ...                                         lambda data: print(f"Got: {data}"),
    ...                                         CallbackPriority.HIGH)
)")
        .def("unregister_callback",
             &atom::async::Trigger<py::object>::unregisterCallback,
             py::arg("event"), py::arg("callback_id"),
             R"(Unregisters a callback for a specified event.

Args:
    event: The name of the event from which to unregister the callback.
    callback_id: The identifier of the callback to unregister.

Returns:
    True if the callback was found and removed, False otherwise.

Examples:
    >>> trigger.unregister_callback("data_received", callback_id)
)")
        .def("unregister_all_callbacks",
             &atom::async::Trigger<py::object>::unregisterAllCallbacks,
             py::arg("event"),
             R"(Unregisters all callbacks for a specified event.

Args:
    event: The name of the event from which to unregister all callbacks.

Returns:
    The number of callbacks that were unregistered.

Examples:
    >>> count = trigger.unregister_all_callbacks("data_received")
    >>> print(f"Removed {count} callbacks")
)")
        .def("trigger", &atom::async::Trigger<py::object>::trigger,
             py::arg("event"), py::arg("param"),
             R"(Triggers the callbacks associated with a specified event.

Args:
    event: The name of the event to trigger.
    param: The parameter to be passed to the callbacks.

Returns:
    The number of callbacks that were executed.

Examples:
    >>> count = trigger.trigger("data_received", {"id": 123, "value": "test"})
    >>> print(f"Executed {count} callbacks")
)")
        .def("schedule_trigger",
             &atom::async::Trigger<py::object>::scheduleTrigger,
             py::arg("event"), py::arg("param"), py::arg("delay"),
             R"(Schedules a trigger for a specified event after a delay.

Args:
    event: The name of the event to trigger.
    param: The parameter to be passed to the callbacks.
    delay: The delay after which to trigger the event, in milliseconds.

Returns:
    A cancel flag that can be used to cancel the scheduled trigger.

Raises:
    TriggerException: If the event name is empty or delay is negative.

Examples:
    >>> import time
    >>> from atom.async import milliseconds
    >>> # Schedule trigger to run after 1 second
    >>> cancel_flag = trigger.schedule_trigger("delayed_event", "delayed data", milliseconds(1000))
    >>> # To cancel the scheduled trigger:
    >>> # cancel_flag._set()  # Not directly accessible in Python
)")
        .def(
            "schedule_async_trigger",
            [](atom::async::Trigger<py::object>& self, std::string event,
               py::object param) {
                auto future =
                    self.scheduleAsyncTrigger(std::move(event), param);
                return future_to_py_object(future);
            },
            py::arg("event"), py::arg("param"),
            R"(Schedules an asynchronous trigger for a specified event.

Args:
    event: The name of the event to trigger.
    param: The parameter to be passed to the callbacks.

Returns:
    The number of callbacks that were executed.

Raises:
    TriggerException: If the event name is empty.

Examples:
    >>> # Trigger event asynchronously
    >>> count = trigger.schedule_async_trigger("async_event", "async data")
    >>> print(f"Executed {count} callbacks")
)")
        .def("cancel_trigger", &atom::async::Trigger<py::object>::cancelTrigger,
             py::arg("event"),
             R"(Cancels the scheduled trigger for a specified event.

Args:
    event: The name of the event for which to cancel the trigger.

Returns:
    The number of pending triggers that were canceled.

Examples:
    >>> count = trigger.cancel_trigger("delayed_event")
    >>> print(f"Canceled {count} pending triggers")
)")
        .def("cancel_all_triggers",
             &atom::async::Trigger<py::object>::cancelAllTriggers,
             R"(Cancels all scheduled triggers.

Returns:
    The number of pending triggers that were canceled.

Examples:
    >>> count = trigger.cancel_all_triggers()
    >>> print(f"Canceled {count} total pending triggers")
)")
        .def("has_callbacks", &atom::async::Trigger<py::object>::hasCallbacks,
             py::arg("event"),
             R"(Checks if the trigger has any registered callbacks for an event.

Args:
    event: The name of the event to check.

Returns:
    True if there are callbacks registered for the event, False otherwise.

Examples:
    >>> if trigger.has_callbacks("data_received"):
    ...     print("Event has listeners")
)")
        .def("callback_count", &atom::async::Trigger<py::object>::callbackCount,
             py::arg("event"),
             R"(Gets the number of registered callbacks for an event.

Args:
    event: The name of the event to check.

Returns:
    The number of callbacks registered for the event.

Examples:
    >>> count = trigger.callback_count("data_received")
    >>> print(f"Event has {count} listeners")
)")

#ifdef ATOM_USE_BOOST_LOCKFREE
        .def_static(
            "create_lockfree_trigger_queue",
            [](std::size_t queue_size) {
                return atom::async::Trigger<
                    py::object>::createLockFreeTriggerQueue(queue_size);
            },
            py::arg("queue_size") = 1024,
            R"pbdoc(
            Create a lock-free trigger queue for high-throughput event handling.

            This creates an optimized version of the trigger system for scenarios
            requiring high-throughput event processing with minimal contention.

            Args:
                queue_size: The size of the internal lock-free queue (default: 1024)

            Returns:
                A unique pointer to the created trigger queue

            Note:
                This feature requires Boost lockfree support to be enabled

            Examples:
                >>> queue = Trigger.create_lockfree_trigger_queue(2048)
                >>> # Use queue for high-performance event processing
            )pbdoc")
        .def(
            "process_lockfree_triggers",
            [](atom::async::Trigger<py::object>& self, py::object queue,
               std::size_t max_events) {
                // Note: This would require proper queue binding
                // For now, we'll provide a placeholder
                py::print(
                    "Lock-free trigger processing not fully implemented in "
                    "Python bindings");
                return std::size_t(0);
            },
            py::arg("queue"), py::arg("max_events") = 0,
            R"pbdoc(
            Process events from a lock-free trigger queue.

            Args:
                queue: The lock-free trigger queue to process
                max_events: Maximum number of events to process in one call (0 for all available)

            Returns:
                Number of events processed

            Note:
                This feature requires Boost lockfree support to be enabled

            Examples:
                >>> processed = trigger.process_lockfree_triggers(queue, 100)
                >>> print(f"Processed {processed} events")
            )pbdoc")
#endif
        ;

    // Helper functions and constants
    m.def(
        "milliseconds",
        [](long long ms) { return std::chrono::milliseconds(ms); },
        py::arg("ms"),
        R"(Creates a milliseconds duration object.

Args:
    ms: Number of milliseconds.

Returns:
    A duration object representing the specified number of milliseconds.

Examples:
    >>> from atom.async import milliseconds
    >>> delay = milliseconds(500)  # 500 milliseconds
)");

    // Factory function for creating Trigger objects
    m.def(
         "create_trigger",
         []() { return std::make_unique<atom::async::Trigger<py::object>>(); },
         R"(Creates a new Trigger object.

Returns:
    A new Trigger instance.

Examples:
    >>> from atom.async import create_trigger
    >>> trigger = create_trigger()
)")

        .def(
            "seconds",
            [](long long s) { return std::chrono::milliseconds(s * 1000); },
            py::arg("s"),
            R"pbdoc(
        Creates a duration object from seconds.

        Args:
            s: Number of seconds

        Returns:
            A duration object representing the specified number of seconds

        Examples:
            >>> from atom.async.trigger import seconds
            >>> delay = seconds(5)  # 5 seconds
        )pbdoc")

        .def(
            "minutes",
            [](long long m) {
                return std::chrono::milliseconds(m * 60 * 1000);
            },
            py::arg("m"),
            R"pbdoc(
        Creates a duration object from minutes.

        Args:
            m: Number of minutes

        Returns:
            A duration object representing the specified number of minutes

        Examples:
            >>> from atom.async.trigger import minutes
            >>> delay = minutes(2)  # 2 minutes
        )pbdoc")

        .def(
            "benchmark_trigger_performance",
            [](std::size_t num_events, std::size_t num_callbacks) -> py::dict {
                using namespace std::chrono;

                py::dict results;
                atom::async::Trigger<py::object> trigger;

                // Register callbacks
                std::vector<std::size_t> callback_ids;
                for (std::size_t i = 0; i < num_callbacks; ++i) {
                    auto callback = [i](const py::object& /*data*/) {
                        // Simulate some work
                        volatile int dummy = 0;
                        for (int j = 0; j < 100; ++j) {
                            dummy += j;
                        }
                    };
                    auto id =
                        trigger.registerCallback("benchmark_event", callback);
                    callback_ids.push_back(id);
                }

                // Benchmark trigger performance
                auto start = high_resolution_clock::now();

                for (std::size_t i = 0; i < num_events; ++i) {
                    trigger.trigger("benchmark_event", py::cast(i));
                }

                auto end = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(end - start);

                // Calculate statistics
                double total_time_us = duration.count();
                double avg_time_per_event = total_time_us / num_events;
                double events_per_second =
                    (num_events * 1000000.0) / total_time_us;
                double total_callbacks_executed = num_events * num_callbacks;
                double callbacks_per_second =
                    (total_callbacks_executed * 1000000.0) / total_time_us;

                results[py::str("num_events")] = num_events;
                results[py::str("num_callbacks")] = num_callbacks;
                results[py::str("total_time_us")] = total_time_us;
                results[py::str("avg_time_per_event_us")] = avg_time_per_event;
                results[py::str("events_per_second")] = events_per_second;
                results[py::str("total_callbacks_executed")] =
                    total_callbacks_executed;
                results[py::str("callbacks_per_second")] = callbacks_per_second;

                return results;
            },
            py::arg("num_events") = 10000, py::arg("num_callbacks") = 10,
            R"pbdoc(
        Benchmark trigger system performance.

        Args:
            num_events: Number of events to trigger (default: 10,000)
            num_callbacks: Number of callbacks per event (default: 10)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_trigger_performance(5000, 5)
            >>> print(f"Events per second: {results['events_per_second']:.2f}")
            >>> print(f"Callbacks per second: {results['callbacks_per_second']:.2f}")
        )pbdoc")

        .def(
            "create_event_chain",
            [](py::list event_names, py::function callback) -> py::object {
                if (event_names.empty()) {
                    throw std::invalid_argument(
                        "Event names list cannot be empty");
                }

                auto trigger =
                    std::make_unique<atom::async::Trigger<py::object>>();
                std::vector<std::size_t> callback_ids;

                // Register the same callback for all events
                for (auto event_name : event_names) {
                    std::string event_str = event_name.cast<std::string>();
                    auto id = trigger->registerCallback(
                        event_str, [callback](const py::object& data) {
                            py::gil_scoped_acquire acquire;
                            try {
                                callback(data);
                            } catch (const py::error_already_set& e) {
                                py::print("Callback error:", e.what());
                            }
                        });
                    callback_ids.push_back(id);
                }

                // Return a tuple of (trigger, callback_ids)
                return py::make_tuple(std::move(trigger), callback_ids);
            },
            py::arg("event_names"), py::arg("callback"),
            R"pbdoc(
        Create an event chain with the same callback for multiple events.

        Args:
            event_names: List of event names to register the callback for
            callback: Function to call for all events

        Returns:
            tuple: (trigger_instance, list_of_callback_ids)

        Examples:
            >>> events = ["start", "process", "end"]
            >>> trigger, ids = create_event_chain(events, lambda data: print(f"Event: {data}"))
            >>> trigger.trigger("start", "beginning")
            >>> trigger.trigger("process", "working")
            >>> trigger.trigger("end", "finished")
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

#ifdef ATOM_USE_BOOST_LOCKS
    m.attr("HAS_BOOST_LOCKS") = true;
#else
    m.attr("HAS_BOOST_LOCKS") = false;
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

    // Threading backend information
#ifdef ATOM_USE_BOOST_LOCKS
    m.attr("THREADING_BACKEND") = "Boost";
#else
    m.attr("THREADING_BACKEND") = "Standard";
#endif
}
