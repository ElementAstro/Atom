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
    m.doc() = "Event trigger implementation module for the atom package";

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
        .def(
            "has_callbacks", &atom::async::Trigger<py::object>::hasCallbacks,
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
)");

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
)");
}
