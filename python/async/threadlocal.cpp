#include "atom/async/threadlocal.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(async, m) {
    m.doc() =
        "Asynchronous utilities implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ThreadLocal class binding
    py::class_<atom::async::ThreadLocal<py::object>>(
        m, "ThreadLocal",
        R"(An enhanced thread-local storage class for Python objects.

This class allows each thread to maintain its own independent instance of an object,
with various initialization options (default, conditional, thread-id based),
automatic cleanup, and a rich set of access and manipulation methods.

Examples:
    >>> from atom.async import ThreadLocal, get_thread_id
    >>> # Create with a simple initializer
    >>> tls = ThreadLocal(lambda: "default value")
    >>> print(tls.get())
    default value

    >>> # Create with a default value
    >>> tls_default = ThreadLocal("initial")
    >>> print(tls_default.get())
    initial

    >>> # Create with a conditional initializer
    >>> def cond_init():
    ...     # Initialize only for some condition, e.g., specific thread
    ...     # For demonstration, let's say it returns None sometimes
    ...     import random
    ...     return "conditional_value" if random.choice([True, False]) else None
    >>> tls_cond = ThreadLocal(conditional_initializer=cond_init)
    >>> try:
    ...   val = tls_cond.get() # Might raise error if cond_init returns None
    ...   print(val)
    ... except RuntimeError as e:
    ...   print(f"Error getting value: {e}")

    >>> # Create with a thread_id based initializer
    >>> tls_tid = ThreadLocal(thread_id_initializer=lambda tid: f"Value for thread {tid}")
    >>> print(tls_tid.get()) # Will show value based on current thread's ID
)")
        .def(py::init<>(),
             "Constructs a new ThreadLocal object without an initializer.")
        .def(py::init<std::function<py::object()>>(), py::arg("initializer"),
             "Constructs a ThreadLocal object with an initializer function "
             "(func() -> value).")
        .def(py::init([](py::function py_conditional_init) {
                 std::function<std::optional<py::object>()>
                     cpp_conditional_init =
                         [py_conditional_init]() -> std::optional<py::object> {
                     py::gil_scoped_acquire acquire;
                     py::object result = py_conditional_init();
                     if (result.is_none()) {
                         return std::nullopt;
                     }
                     return result.cast<py::object>();
                 };
                 return std::make_unique<atom::async::ThreadLocal<py::object>>(
                     cpp_conditional_init);
             }),
             py::arg("conditional_initializer"),
             "Constructs with a conditional initializer (Python func() -> "
             "value or None).")
        .def(py::init([](py::function py_tid_init) {
                 std::function<py::object(std::thread::id)> cpp_tid_init =
                     [py_tid_init](std::thread::id tid) -> py::object {
                     py::gil_scoped_acquire acquire;
                     return py_tid_init(py::cast(tid)).cast<py::object>();
                 };
                 return std::make_unique<atom::async::ThreadLocal<py::object>>(
                     cpp_tid_init);
             }),
             py::arg("thread_id_initializer"),
             "Constructs with a thread ID-based initializer (Python func(tid) "
             "-> value).")
        .def(py::init<py::object>(), py::arg("default_value"),
             "Constructs with a default value for all threads.")
        .def(
            "get",
            [](atom::async::ThreadLocal<py::object>& self) {
                try {
                    return self.get();
                } catch (const atom::async::ThreadLocalException& e) {
                    PyErr_SetString(PyExc_RuntimeError, e.what());
                    throw py::error_already_set();
                } catch (const std::exception& e) {  // Fallback
                    PyErr_SetString(PyExc_ValueError, e.what()); // Consistent with existing translator for std::invalid_argument or use PyExc_RuntimeError
                    throw py::error_already_set();
                }
            },
            R"(Retrieves the thread-local value.

If the value is not yet initialized, the initializer function is called.

Returns:
    The thread-local value for the current thread.

Raises:
    RuntimeError: If initialization fails or no initializer is available and the value hasn't been set.
)")
        .def(
            "try_get",
            [](atom::async::ThreadLocal<py::object>& self) -> py::object {
                auto result = self.tryGet();
                if (result.has_value()) {
                    return result.value().get();
                }
                return py::none();
            },
            R"(Tries to get the value for the current thread.

Unlike get(), this method does not throw an exception if the value
is not found or cannot be initialized (if using a non-throwing initializer).

Returns:
    The thread-local value or None if it doesn't exist or couldn't be initialized.
)")
        .def(
            "get_or_create",
            [](atom::async::ThreadLocal<py::object>& self,
               py::function factory) -> py::object {
                try {
                    return self.getOrCreate([factory]() {
                        py::gil_scoped_acquire acquire;
                        return factory().cast<py::object>();
                    });
                } catch (const atom::async::ThreadLocalException& e) {
                    PyErr_SetString(PyExc_RuntimeError, e.what());
                    throw py::error_already_set();
                }
            },
            py::arg("factory"),
            R"(Gets or creates the value for the current thread.

If the value does not exist, it is created using the provided factory function.

Args:
    factory: A Python function that takes no arguments and returns the value to be created.

Returns:
    The thread-local value.

Raises:
    RuntimeError: If the factory function fails.
)")
        .def(
            "reset",
            [](atom::async::ThreadLocal<py::object>& self, py::object value) {
                self.reset(std::move(value));
            },
            py::arg("value") = py::none(),  // Added default value
            R"(Resets the thread-local value.

Args:
    value: The new value to set for the current thread. Defaults to None.
)")
        .def(
            "has_value",
            [](const atom::async::ThreadLocal<py::object>& self) {
                return self.hasValue();
            },
            R"(Checks if the current thread has a value.

Returns:
    True if the current thread has an initialized value, otherwise False.
)")
        .def("compare_and_update",
             &atom::async::ThreadLocal<py::object>::compareAndUpdate,
             py::arg("expected"), py::arg("desired"),
             R"(Atomically compares and updates the thread-local value.

Updates to 'desired' only if the current value equals 'expected'.

Args:
    expected: The expected current value.
    desired: The new value to set.

Returns:
    True if the update was successful, False otherwise.
)")
        .def(
            "update",
            [](atom::async::ThreadLocal<py::object>& self, py::function func) {
                return self.update([func](py::object& current_value) {
                    py::gil_scoped_acquire acquire;
                    return func(current_value).cast<py::object>();
                });
            },
            py::arg("func"),
            R"(Updates the thread-local value using the provided transformation function.

The function receives the current value and should return the new value.

Args:
    func: A Python function (current_value) -> new_value.

Returns:
    True if successfully updated (i.e., a value existed to update), False otherwise.
)")
        .def(
            "for_each",
            [](atom::async::ThreadLocal<py::object>& self, py::function func) {
                self.forEach([func](py::object& value) {
                    py::gil_scoped_acquire acquire;
                    try {
                        func(value);
                    } catch (const py::error_already_set& e) {
                        // Restore and print error, but don't let it propagate
                        // as forEach in C++ is designed not to throw from the
                        // loop.
                        const_cast<py::error_already_set&>(e).restore();
                        PyErr_Print();
                    }
                });
            },
            py::arg("func"),
            R"(Executes a function for each stored thread-local value.

Args:
    func: The function (value) -> None to execute for each value.
          Exceptions from this function are caught and printed.
)")
        .def(
            "for_each_with_id",
            [](atom::async::ThreadLocal<py::object>& self, py::function func) {
                self.forEachWithId(
                    [func](py::object& value, std::thread::id tid) {
                        py::gil_scoped_acquire acquire;
                        try {
                            func(value, py::cast(tid));
                        } catch (const py::error_already_set& e) {
                            const_cast<py::error_already_set&>(e).restore();
                            PyErr_Print();
                        }
                    });
            },
            py::arg("func"),
            R"(Executes a function for each thread-local value with its thread ID.

Args:
    func: The function (value, thread_id) -> None to execute.
          Exceptions from this function are caught and printed.
)")
        .def(
            "find_if",
            [](atom::async::ThreadLocal<py::object>& self,
               py::function pred) -> py::object {
                auto result = self.findIf([pred](py::object& value) {
                    py::gil_scoped_acquire acquire;
                    return pred(value).cast<bool>();
                });
                if (result.has_value()) {
                    return result.value().get();
                }
                return py::none();
            },
            py::arg("pred"),
            R"(Finds the first thread value that satisfies the given predicate.

Args:
    pred: A Python function (value) -> bool used to test values.

Returns:
    The found value, or None if not found.
)")
        .def(
            "clear",
            [](atom::async::ThreadLocal<py::object>& self) { self.clear(); },
            R"(Clears thread-local storage for all threads.
Calls cleanup function if set.)")
        .def(
            "clear_current_thread",
            [](atom::async::ThreadLocal<py::object>& self) {
                self.clearCurrentThread();
            },
            R"(Clears the thread-local storage for the current thread.
Calls cleanup function if set.)")
        .def(
            "remove_if",
            [](atom::async::ThreadLocal<py::object>& self, py::function pred) {
                return self.removeIf([pred](py::object& value) {
                    py::gil_scoped_acquire acquire;
                    return pred(value).cast<bool>();
                });
            },
            py::arg("pred"),
            R"(Removes all thread values that satisfy the given predicate.
Calls cleanup function for removed values if set.

Args:
    pred: A Python function (value) -> bool used to test values.

Returns:
    The number of values removed.
)")
        .def(
            "size",
            [](const atom::async::ThreadLocal<py::object>& self) {
                return self.size();
            },
            R"(Gets the number of threads with values stored.

Returns:
    The count of thread values currently stored.
)")
        .def("empty", &atom::async::ThreadLocal<py::object>::empty,
             R"(Checks if the storage is empty (no threads have values stored).

Returns:
    True if there are no stored thread values, False otherwise.
)")
        .def(
            "set_cleanup_function",
            [](atom::async::ThreadLocal<py::object>& self,
               py::object py_cleanup_func) {
                if (py_cleanup_func.is_none()) {
                    self.setCleanupFunction(nullptr);
                } else {
                    py::function casted_cleanup_func =
                        py_cleanup_func.cast<py::function>();
                    self.setCleanupFunction(
                        [casted_cleanup_func](py::object& value) {
                            py::gil_scoped_acquire acquire;
                            try {
                                casted_cleanup_func(value);
                            } catch (const py::error_already_set& e) {
                                const_cast<py::error_already_set&>(e).restore();
                                PyErr_Print();  // Print and clear the error
                            } catch (...) {
                                // Non-Python exception
                                fprintf(stderr,
                                        "Unknown C++ exception in cleanup "
                                        "callback wrapper\n");
                            }
                        });
                }
            },
            py::arg("cleanup_func"),
            R"(Sets or updates the cleanup function.

The cleanup function is called when a value is removed (e.g., by reset, clear, remove_if)
or when the ThreadLocal object is destroyed. Pass None to remove the cleanup function.

Args:
    cleanup_func: A Python function (value) -> None, or None.
                  Exceptions from this function are caught and printed.
)")
        .def("has_value_for_thread",
             &atom::async::ThreadLocal<py::object>::hasValueForThread,
             py::arg("thread_id"),
             R"(Checks if the specified thread has a value.

Args:
    thread_id: The ID of the thread to check (as returned by get_thread_id()).

Returns:
    True if the specified thread has an initialized value, False otherwise.
)")
        .def(
            "__call__",
            [](atom::async::ThreadLocal<py::object>& self) {
                try {
                    return self.get();
                } catch (const atom::async::ThreadLocalException& e) {
                    PyErr_SetString(PyExc_RuntimeError, e.what());
                    throw py::error_already_set();
                } catch (const std::exception& e) {  // Fallback
                    PyErr_SetString(PyExc_ValueError, e.what()); // Consistent with existing translator or use PyExc_RuntimeError
                    throw py::error_already_set();
                }
            },
            R"(Allows calling the ThreadLocal object as a function to get the value.

Equivalent to calling .get().

Returns:
    The thread-local value for the current thread.

Raises:
    RuntimeError: If initialization fails or no initializer is available.
)")
        .def(
            "__bool__",
            [](const atom::async::ThreadLocal<py::object>& self) {
                return self.hasValue();
            },
            "Support for boolean evaluation (True if current thread has a "
            "value).");
}