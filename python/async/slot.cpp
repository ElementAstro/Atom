#include "atom/async/slot.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <memory>


namespace py = pybind11;

PYBIND11_MODULE(slot, m) {
    m.doc() = R"pbdoc(
        Signal-Slot Implementation Module
        --------------------------------

        This module provides a comprehensive signal-slot system for implementing
        the observer pattern with various execution strategies and advanced features.

        Features:
          - Synchronous and asynchronous signal emission
          - Thread-safe signal implementations with shared mutex optimization
          - Automatic connection management with unique IDs
          - Signal chaining for complex event propagation
          - Limited signals with emission count restrictions
          - Coroutine-based signals for cooperative multitasking
          - Scoped signals with automatic cleanup

        Signal Types:
          - Signal: Basic synchronous signal-slot implementation
          - AsyncSignal: Asynchronous signal execution with futures
          - AutoDisconnectSignal: Signals with unique connection IDs
          - ChainedSignal: Signals that can trigger other signals
          - ThreadSafeSignal: Thread-safe signals with parallel execution
          - LimitedSignal: Signals with emission count limits
          - CoroutineSignal: Coroutine-based asynchronous signals
          - ScopedSignal: Signals with automatic slot cleanup

        Example:
            >>> from atom.async.slot import Signal, AsyncSignal
            >>>
            >>> # Basic signal usage
            >>> signal = Signal()
            >>> signal.connect(lambda data: print(f"Received: {data}"))
            >>> signal.emit("Hello, World!")
            >>>
            >>> # Asynchronous signal usage
            >>> async_signal = AsyncSignal()
            >>> async_signal.connect(lambda data: print(f"Async: {data}"))
            >>> async_signal.emit("Hello, Async World!")
    )pbdoc";

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

    // Utility functions for signal management and testing
    m.def(
        "benchmark_signal_performance",
        [](size_t num_slots, size_t num_emissions) -> py::dict {
            using namespace std::chrono;

            py::dict results;
            atom::async::Signal<py::object> signal;

            // Connect slots
            for (size_t i = 0; i < num_slots; ++i) {
                signal.connect([i](const py::object& /*data*/) {
                    // Simulate some work
                    volatile int dummy = 0;
                    for (int j = 0; j < 100; ++j) {
                        dummy += j;
                    }
                });
            }

            // Benchmark signal emission
            auto start = high_resolution_clock::now();

            for (size_t i = 0; i < num_emissions; ++i) {
                signal.emit(py::cast(i));
            }

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            // Calculate statistics
            double total_time_us = duration.count();
            double emissions_per_second = (num_emissions * 1000000.0) / total_time_us;
            double total_slot_calls = num_emissions * num_slots;
            double slot_calls_per_second = (total_slot_calls * 1000000.0) / total_time_us;

            results[py::str("num_slots")] = num_slots;
            results[py::str("num_emissions")] = num_emissions;
            results[py::str("total_time_us")] = total_time_us;
            results[py::str("emissions_per_second")] = emissions_per_second;
            results[py::str("total_slot_calls")] = total_slot_calls;
            results[py::str("slot_calls_per_second")] = slot_calls_per_second;
            results[py::str("avg_time_per_emission_us")] = total_time_us / num_emissions;

            return results;
        },
        py::arg("num_slots") = 10, py::arg("num_emissions") = 1000,
        R"pbdoc(
        Benchmark signal performance with multiple slots and emissions.

        Args:
            num_slots: Number of slots to connect (default: 10)
            num_emissions: Number of signal emissions to perform (default: 1000)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_signal_performance(5, 500)
            >>> print(f"Emissions per second: {results['emissions_per_second']:.2f}")
            >>> print(f"Slot calls per second: {results['slot_calls_per_second']:.2f}")
        )pbdoc")

    .def(
        "benchmark_async_signal_performance",
        [](size_t num_slots, size_t num_emissions) -> py::dict {
            using namespace std::chrono;

            py::dict results;
            atom::async::AsyncSignal<py::object> signal;

            // Connect slots
            for (size_t i = 0; i < num_slots; ++i) {
                signal.connect([i](const py::object& /*data*/) {
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                });
            }

            // Benchmark async signal emission
            auto start = high_resolution_clock::now();

            for (size_t i = 0; i < num_emissions; ++i) {
                signal.emit(py::cast(i));
            }

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            // Calculate statistics
            double total_time_us = duration.count();
            double emissions_per_second = (num_emissions * 1000000.0) / total_time_us;
            double total_slot_calls = num_emissions * num_slots;
            double slot_calls_per_second = (total_slot_calls * 1000000.0) / total_time_us;

            results[py::str("num_slots")] = num_slots;
            results[py::str("num_emissions")] = num_emissions;
            results[py::str("total_time_us")] = total_time_us;
            results[py::str("emissions_per_second")] = emissions_per_second;
            results[py::str("total_slot_calls")] = total_slot_calls;
            results[py::str("slot_calls_per_second")] = slot_calls_per_second;
            results[py::str("avg_time_per_emission_us")] = total_time_us / num_emissions;

            return results;
        },
        py::arg("num_slots") = 5, py::arg("num_emissions") = 100,
        R"pbdoc(
        Benchmark async signal performance with multiple slots and emissions.

        Args:
            num_slots: Number of slots to connect (default: 5)
            num_emissions: Number of signal emissions to perform (default: 100)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_async_signal_performance(3, 50)
            >>> print(f"Async emissions per second: {results['emissions_per_second']:.2f}")
        )pbdoc")

    .def(
        "create_signal_chain",
        [](py::list signal_names) -> py::dict {
            py::dict chain;
            std::vector<std::shared_ptr<atom::async::ChainedSignal<py::object>>> signals;

            // Create signals
            for (auto name : signal_names) {
                std::string signal_name = name.cast<std::string>();
                auto signal = std::make_shared<atom::async::ChainedSignal<py::object>>();
                signals.push_back(signal);
                chain[py::str(signal_name)] = signal;
            }

            // Chain signals together
            for (size_t i = 0; i < signals.size() - 1; ++i) {
                signals[i]->addChain(signals[i + 1]);
            }

            return chain;
        },
        py::arg("signal_names"),
        R"pbdoc(
        Create a chain of connected signals.

        Args:
            signal_names: List of signal names for the chain

        Returns:
            dict: Dictionary mapping signal names to ChainedSignal instances

        Examples:
            >>> chain = create_signal_chain(["start", "process", "end"])
            >>> chain["start"].connect(lambda data: print(f"Start: {data}"))
            >>> chain["process"].connect(lambda data: print(f"Process: {data}"))
            >>> chain["end"].connect(lambda data: print(f"End: {data}"))
            >>> chain["start"].emit("data")  # Triggers all three signals
        )pbdoc")

    .def(
        "create_signal_hub",
        [](py::dict signal_configs) -> py::dict {
            py::dict hub;

            for (auto item : signal_configs) {
                std::string signal_name = item.first.cast<std::string>();
                std::string signal_type = item.second.cast<std::string>();

                if (signal_type == "basic") {
                    hub[py::str(signal_name)] = std::make_unique<atom::async::Signal<py::object>>();
                } else if (signal_type == "async") {
                    hub[py::str(signal_name)] = std::make_unique<atom::async::AsyncSignal<py::object>>();
                } else if (signal_type == "thread_safe") {
                    hub[py::str(signal_name)] = std::make_unique<atom::async::ThreadSafeSignal<py::object>>();
                } else if (signal_type == "auto_disconnect") {
                    hub[py::str(signal_name)] = std::make_unique<atom::async::AutoDisconnectSignal<py::object>>();
                } else {
                    // Default to basic signal
                    hub[py::str(signal_name)] = std::make_unique<atom::async::Signal<py::object>>();
                }
            }

            return hub;
        },
        py::arg("signal_configs"),
        R"pbdoc(
        Create a hub of different signal types.

        Args:
            signal_configs: Dictionary mapping signal names to signal types

        Returns:
            dict: Dictionary mapping signal names to signal instances

        Examples:
            >>> configs = {
            ...     "user_action": "basic",
            ...     "data_update": "async",
            ...     "system_event": "thread_safe"
            ... }
            >>> hub = create_signal_hub(configs)
            >>> hub["user_action"].connect(lambda data: print(f"User: {data}"))
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

    // Feature detection
    m.attr("HAS_ASYNC_SUPPORT") = true;
    m.attr("HAS_COROUTINE_SUPPORT") = true;
    m.attr("HAS_THREAD_SAFETY") = true;

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

    // Platform information
#ifdef ATOM_PLATFORM_WINDOWS
    m.attr("PLATFORM") = "Windows";
#elif defined(ATOM_PLATFORM_APPLE)
    m.attr("PLATFORM") = "macOS";
#elif defined(ATOM_PLATFORM_LINUX)
    m.attr("PLATFORM") = "Linux";
#else
    m.attr("PLATFORM") = "Unknown";
#endif
}
