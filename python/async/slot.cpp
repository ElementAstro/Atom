#include "atom/async/slot.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <memory>


namespace py = pybind11;

PYBIND11_MODULE(slot, m) {
    m.doc() = "Signal-slot implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::SlotConnectionError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::async::SlotEmissionError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register custom exceptions
    py::register_exception<atom::async::SlotConnectionError>(
        m, "SlotConnectionError");
    py::register_exception<atom::async::SlotEmissionError>(m,
                                                           "SlotEmissionError");

    // Basic Signal class binding
    py::class_<atom::async::Signal<py::object>>(
        m, "Signal",
        R"(A signal class that allows connecting, disconnecting, and emitting slots.

This class provides a mechanism for implementing the observer pattern where functions
(slots) can be connected to a signal and will be called when the signal is emitted.

Examples:
    >>> from atom.async import Signal
    >>> def handler(data):
    ...     print(f"Received: {data}")
    >>> signal = Signal()
    >>> signal.connect(handler)
    >>> signal.emit("Hello, World!")
    Received: Hello, World!
)")
        .def(py::init<>(), "Constructs a new Signal object.")
        .def("connect", &atom::async::Signal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.

Examples:
    >>> signal.connect(lambda data: print(f"Data: {data}"))
)")
        .def("disconnect", &atom::async::Signal<py::object>::disconnect,
             py::arg("slot"),
             R"(Disconnect a slot from the signal.

Args:
    slot: The function to disconnect.

Examples:
    >>> def handler(data):
    ...     print(data)
    >>> signal.connect(handler)
    >>> signal.disconnect(handler)
)")
        .def("emit", &atom::async::Signal<py::object>::emit, py::arg("args"),
             R"(Emit the signal, calling all connected slots.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.

Examples:
    >>> signal.emit("Data to send")
)")
        .def("clear", &atom::async::Signal<py::object>::clear,
             R"(Clear all slots connected to this signal.

Examples:
    >>> signal.clear()  # Disconnects all slots
)")
        .def("size", &atom::async::Signal<py::object>::size,
             R"(Get the number of connected slots.

Returns:
    The number of slots.

Examples:
    >>> count = signal.size()
    >>> print(f"Signal has {count} connected slots")
)")
        .def("empty", &atom::async::Signal<py::object>::empty,
             R"(Check if the signal has no connected slots.

Returns:
    True if the signal has no slots, False otherwise.

Examples:
    >>> if signal.empty():
    ...     print("No slots connected")
)")
        .def("__len__", &atom::async::Signal<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::Signal<py::object>& s) { return !s.empty(); },
            "Support for boolean evaluation.");

    // AsyncSignal class binding
    py::class_<atom::async::AsyncSignal<py::object>>(
        m, "AsyncSignal",
        R"(A signal class that allows asynchronous slot execution.

This class provides a mechanism for implementing the observer pattern where functions
(slots) can be connected to a signal and will be called asynchronously when the
signal is emitted.

Examples:
    >>> from atom.async import AsyncSignal
    >>> def handler(data):
    ...     print(f"Received asynchronously: {data}")
    >>> signal = AsyncSignal()
    >>> signal.connect(handler)
    >>> signal.emit("Hello, World!")  # Handler runs in a separate thread
)")
        .def(py::init<>(), "Constructs a new AsyncSignal object.")
        .def("connect", &atom::async::AsyncSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def("disconnect", &atom::async::AsyncSignal<py::object>::disconnect,
             py::arg("slot"),
             R"(Disconnect a slot from the signal.

Args:
    slot: The function to disconnect.
)")
        .def("emit", &atom::async::AsyncSignal<py::object>::emit,
             py::arg("args"),
             R"(Emit the signal asynchronously, calling all connected slots.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any asynchronous execution fails.

Notes:
    This method will wait for all slots to complete execution.
)")
        .def("wait_for_completion",
             &atom::async::AsyncSignal<py::object>::waitForCompletion,
             R"(Wait for all slots to finish execution.)")
        .def("clear", &atom::async::AsyncSignal<py::object>::clear,
             R"(Clear all slots connected to this signal.)");

    // AutoDisconnectSignal class binding
    py::class_<atom::async::AutoDisconnectSignal<py::object>>(
        m, "AutoDisconnectSignal",
        R"(A signal class that allows automatic disconnection of slots.

This class provides a mechanism for implementing the observer pattern with
uniquely identifiable connections that can be easily disconnected by ID.

Examples:
    >>> from atom.async import AutoDisconnectSignal
    >>> def handler(data):
    ...     print(f"Received: {data}")
    >>> signal = AutoDisconnectSignal()
    >>> connection_id = signal.connect(handler)
    >>> signal.emit("Hello, World!")
    Received: Hello, World!
    >>> signal.disconnect(connection_id)  # Disconnect using ID
)")
        .def(py::init<>(), "Constructs a new AutoDisconnectSignal object.")
        .def("connect", &atom::async::AutoDisconnectSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal and return its unique ID.

Args:
    slot: The function to be called when the signal is emitted.

Returns:
    The unique ID of the connected slot.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def("disconnect",
             &atom::async::AutoDisconnectSignal<py::object>::disconnect,
             py::arg("id"),
             R"(Disconnect a slot from the signal using its unique ID.

Args:
    id: The unique ID of the slot to disconnect.

Returns:
    True if the slot was disconnected, False if it wasn't found.
)")
        .def("emit", &atom::async::AutoDisconnectSignal<py::object>::emit,
             py::arg("args"),
             R"(Emit the signal, calling all connected slots.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.
)")
        .def("clear", &atom::async::AutoDisconnectSignal<py::object>::clear,
             R"(Clear all slots connected to this signal.)")
        .def("size", &atom::async::AutoDisconnectSignal<py::object>::size,
             R"(Get the number of connected slots.

Returns:
    The number of slots.
)")
        .def("__len__", &atom::async::AutoDisconnectSignal<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::AutoDisconnectSignal<py::object>& s) {
                return s.size() > 0;
            },
            "Support for boolean evaluation.");

    // ChainedSignal class binding
    py::class_<atom::async::ChainedSignal<py::object>,
               std::shared_ptr<atom::async::ChainedSignal<py::object>>>(
        m, "ChainedSignal",
        R"(A signal class that allows chaining of signals.

This class provides a mechanism for implementing signal chains where emitting
one signal will trigger others connected in a chain.

Examples:
    >>> from atom.async import ChainedSignal
    >>> signal1 = ChainedSignal()
    >>> signal2 = ChainedSignal()
    >>> signal1.add_chain(signal2)
    >>> signal1.connect(lambda data: print(f"Signal1: {data}"))
    >>> signal2.connect(lambda data: print(f"Signal2: {data}"))
    >>> signal1.emit("Hello")  # Both handlers will be called
    Signal1: Hello
    Signal2: Hello
)")
        .def(py::init<>(), "Constructs a new ChainedSignal object.")
        .def("connect", &atom::async::ChainedSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def(
            "add_chain",
            [](atom::async::ChainedSignal<py::object>& self,
               std::shared_ptr<atom::async::ChainedSignal<py::object>>
                   next_signal) { self.addChain(next_signal); },
            py::arg("next_signal"),
            R"(Add a chained signal to be emitted after this signal.

Args:
    next_signal: The next signal to chain.
)")
        .def(
            "emit", &atom::async::ChainedSignal<py::object>::emit,
            py::arg("args"),
            R"(Emit the signal, calling all connected slots and chained signals.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.
)")
        .def("clear", &atom::async::ChainedSignal<py::object>::clear,
             R"(Clear all slots and chains connected to this signal.)");

    // ThreadSafeSignal class binding
    py::class_<atom::async::ThreadSafeSignal<py::object>>(
        m, "ThreadSafeSignal",
        R"(A signal class with advanced thread-safety for readers and writers.

This class provides a mechanism for implementing the observer pattern with
advanced thread-safety features using shared mutexes for efficient read access.

Examples:
    >>> from atom.async import ThreadSafeSignal
    >>> signal = ThreadSafeSignal()
    >>> signal.connect(lambda data: print(f"Received: {data}"))
    >>> signal.emit("Hello from thread")
)")
        .def(py::init<>(), "Constructs a new ThreadSafeSignal object.")
        .def("connect", &atom::async::ThreadSafeSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def("disconnect",
             &atom::async::ThreadSafeSignal<py::object>::disconnect,
             py::arg("slot"),
             R"(Disconnect a slot from the signal.

Args:
    slot: The function to disconnect.
)")
        .def(
            "emit", &atom::async::ThreadSafeSignal<py::object>::emit,
            py::arg("args"),
            R"(Emit the signal using a strand execution policy for parallel execution.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.

Notes:
    When there are more than 4 slots, they will be executed in parallel.
)")
        .def("size", &atom::async::ThreadSafeSignal<py::object>::size,
             R"(Get the number of connected slots.

Returns:
    The number of slots.
)")
        .def("clear", &atom::async::ThreadSafeSignal<py::object>::clear,
             R"(Clear all slots connected to this signal.)")
        .def("__len__", &atom::async::ThreadSafeSignal<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::ThreadSafeSignal<py::object>& s) {
                return s.size() > 0;
            },
            "Support for boolean evaluation.");

    // LimitedSignal class binding
    py::class_<atom::async::LimitedSignal<py::object>>(
        m, "LimitedSignal",
        R"(A signal class that limits the number of times it can be emitted.

This class provides a mechanism for implementing the observer pattern with
a limit on the number of emissions.

Examples:
    >>> from atom.async import LimitedSignal
    >>> signal = LimitedSignal(3)  # Can only be emitted 3 times
    >>> signal.connect(lambda data: print(f"Received: {data}"))
    >>> signal.emit("First")   # Returns True
    Received: First
    >>> signal.emit("Second")  # Returns True
    Received: Second
    >>> signal.emit("Third")   # Returns True
    Received: Third
    >>> signal.emit("Fourth")  # Returns False (limit reached)
)")
        .def(py::init<size_t>(), py::arg("max_calls"),
             R"(Construct a new Limited Signal object.

Args:
    max_calls: The maximum number of times the signal can be emitted.

Raises:
    ValueError: If max_calls is zero.
)")
        .def("connect", &atom::async::LimitedSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def("disconnect", &atom::async::LimitedSignal<py::object>::disconnect,
             py::arg("slot"),
             R"(Disconnect a slot from the signal.

Args:
    slot: The function to disconnect.
)")
        .def(
            "emit", &atom::async::LimitedSignal<py::object>::emit,
            py::arg("args"),
            R"(Emit the signal, calling all connected slots up to the maximum number of calls.

Args:
    args: The arguments to pass to the slots.

Returns:
    True if the signal was emitted, False if the call limit was reached.

Raises:
    SlotEmissionError: If any slot execution fails.
)")
        .def("is_exhausted",
             &atom::async::LimitedSignal<py::object>::isExhausted,
             R"(Check if the signal has reached its call limit.

Returns:
    True if the call limit has been reached.
)")
        .def("remaining_calls",
             &atom::async::LimitedSignal<py::object>::remainingCalls,
             R"(Get remaining call count before limit is reached.

Returns:
    Number of remaining emissions.
)")
        .def("reset", &atom::async::LimitedSignal<py::object>::reset,
             R"(Reset the call counter.

Examples:
    >>> signal.reset()  # Allows the signal to be emitted max_calls times again
)");

    // CoroutineSignal class binding (simplified for Python)
    py::class_<atom::async::CoroutineSignal<py::object>>(
        m, "CoroutineSignal",
        R"(A signal class that uses C++20 coroutines for asynchronous slot execution.

This class provides a mechanism for implementing the observer pattern with
cooperative multitasking using coroutines.

Examples:
    >>> from atom.async import CoroutineSignal
    >>> signal = CoroutineSignal()
    >>> signal.connect(lambda data: print(f"Received: {data}"))
    >>> signal.emit("Hello, World!")  # Slots execute as coroutines
)")
        .def(py::init<>(), "Constructs a new CoroutineSignal object.")
        .def("connect", &atom::async::CoroutineSignal<py::object>::connect,
             py::arg("slot"),
             R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the slot is invalid.
)")
        .def("disconnect",
             &atom::async::CoroutineSignal<py::object>::disconnect,
             py::arg("slot"),
             R"(Disconnect a slot from the signal.

Args:
    slot: The function to disconnect.
)")
        .def(
            "emit",
            [](atom::async::CoroutineSignal<py::object>& self,
               py::object args) {
                // In Python, we don't expose the coroutine task
                // but we execute it synchronously
                self.emit(args);
            },
            py::arg("args"),
            R"(Emit the signal using coroutines to execute the slots.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.
)");

    // ScopedSignal class binding
    py::class_<atom::async::ScopedSignal<py::object>>(
        m, "ScopedSignal",
        R"(A signal class that uses shared_ptr for scoped slot management.

This class provides a mechanism for implementing the observer pattern with
automatic cleanup of slots when they are no longer referenced.

Examples:
    >>> from atom.async import ScopedSignal
    >>> signal = ScopedSignal()
    >>> def handler(data):
    ...     print(f"Received: {data}")
    >>> signal.connect(handler)
    >>> signal.emit("Hello, World!")
    Received: Hello, World!
)")
        .def(py::init<>(), "Constructs a new ScopedSignal object.")
        .def(
            "connect",
            [](atom::async::ScopedSignal<py::object>& self, py::function slot) {
                // Create a callable that will invoke the Python function
                self.connect(slot);
            },
            py::arg("slot"),
            R"(Connect a slot to the signal.

Args:
    slot: The function to be called when the signal is emitted.

Raises:
    SlotConnectionError: If the callable cannot be converted to a slot.
)")
        .def("emit", &atom::async::ScopedSignal<py::object>::emit,
             py::arg("args"),
             R"(Emit the signal, calling all connected slots.

Args:
    args: The arguments to pass to the slots.

Raises:
    SlotEmissionError: If any slot execution fails.
)")
        .def("clear", &atom::async::ScopedSignal<py::object>::clear,
             R"(Clear all slots connected to this signal.)")
        .def("size", &atom::async::ScopedSignal<py::object>::size,
             R"(Get the number of connected slots.

Returns:
    The number of valid slots.
)")
        .def("__len__", &atom::async::ScopedSignal<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::ScopedSignal<py::object>& s) {
                return s.size() > 0;
            },
            "Support for boolean evaluation.");

    // Factory functions
    m.def(
        "create_signal",
        []() { return std::make_unique<atom::async::Signal<py::object>>(); },
        R"(Create a new Signal object.

Returns:
    A new Signal instance.

Examples:
    >>> from atom.async import create_signal
    >>> signal = create_signal()
)");

    m.def(
        "create_async_signal",
        []() {
            return std::make_unique<atom::async::AsyncSignal<py::object>>();
        },
        R"(Create a new AsyncSignal object.

Returns:
    A new AsyncSignal instance.

Examples:
    >>> from atom.async import create_async_signal
    >>> signal = create_async_signal()
)");

    m.def(
        "create_auto_disconnect_signal",
        []() {
            return std::make_unique<
                atom::async::AutoDisconnectSignal<py::object>>();
        },
        R"(Create a new AutoDisconnectSignal object.

Returns:
    A new AutoDisconnectSignal instance.

Examples:
    >>> from atom.async import create_auto_disconnect_signal
    >>> signal = create_auto_disconnect_signal()
)");

    m.def(
        "create_chained_signal",
        []() {
            return std::make_shared<atom::async::ChainedSignal<py::object>>();
        },
        R"(Create a new ChainedSignal object.

Returns:
    A new ChainedSignal instance.

Examples:
    >>> from atom.async import create_chained_signal
    >>> signal1 = create_chained_signal()
    >>> signal2 = create_chained_signal()
    >>> signal1.add_chain(signal2)
)");

    m.def(
        "create_thread_safe_signal",
        []() {
            return std::make_unique<
                atom::async::ThreadSafeSignal<py::object>>();
        },
        R"(Create a new ThreadSafeSignal object.

Returns:
    A new ThreadSafeSignal instance.

Examples:
    >>> from atom.async import create_thread_safe_signal
    >>> signal = create_thread_safe_signal()
)");

    m.def(
        "create_limited_signal",
        [](size_t max_calls) {
            return std::make_unique<atom::async::LimitedSignal<py::object>>(
                max_calls);
        },
        py::arg("max_calls"),
        R"(Create a new LimitedSignal object.

Args:
    max_calls: The maximum number of times the signal can be emitted.

Returns:
    A new LimitedSignal instance.

Examples:
    >>> from atom.async import create_limited_signal
    >>> signal = create_limited_signal(5)  # Can emit 5 times
)");

    m.def(
        "create_coroutine_signal",
        []() {
            return std::make_unique<atom::async::CoroutineSignal<py::object>>();
        },
        R"(Create a new CoroutineSignal object.

Returns:
    A new CoroutineSignal instance.

Examples:
    >>> from atom.async import create_coroutine_signal
    >>> signal = create_coroutine_signal()
)");

    m.def(
        "create_scoped_signal",
        []() {
            return std::make_unique<atom::async::ScopedSignal<py::object>>();
        },
        R"(Create a new ScopedSignal object.

Returns:
    A new ScopedSignal instance.

Examples:
    >>> from atom.async import create_scoped_signal
    >>> signal = create_scoped_signal()
)");
}
