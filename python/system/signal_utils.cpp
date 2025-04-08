#include "atom/system/signal_utils.hpp"
#include "atom/system/signal.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(signal_utils, m) {
    m.doc() = "Signal utilities module for advanced signal handling";

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

    // Define common signal constants
    m.attr("SIGABRT") = py::int_(SIGABRT);
    m.attr("SIGFPE") = py::int_(SIGFPE);
    m.attr("SIGILL") = py::int_(SIGILL);
    m.attr("SIGINT") = py::int_(SIGINT);
    m.attr("SIGSEGV") = py::int_(SIGSEGV);
    m.attr("SIGTERM") = py::int_(SIGTERM);

#if !defined(_WIN32) && !defined(_WIN64)
    m.attr("SIGALRM") = py::int_(SIGALRM);
    m.attr("SIGBUS") = py::int_(SIGBUS);
    m.attr("SIGCHLD") = py::int_(SIGCHLD);
    m.attr("SIGCONT") = py::int_(SIGCONT);
    m.attr("SIGHUP") = py::int_(SIGHUP);
    m.attr("SIGKILL") = py::int_(SIGKILL);
    m.attr("SIGPIPE") = py::int_(SIGPIPE);
    m.attr("SIGQUIT") = py::int_(SIGQUIT);
    m.attr("SIGSTOP") = py::int_(SIGSTOP);
    m.attr("SIGTSTP") = py::int_(SIGTSTP);
    m.attr("SIGTTIN") = py::int_(SIGTTIN);
    m.attr("SIGTTOU") = py::int_(SIGTTOU);
    m.attr("SIGUSR1") = py::int_(SIGUSR1);
    m.attr("SIGUSR2") = py::int_(SIGUSR2);
#else
    m.attr("SIGBREAK") = py::int_(SIGBREAK);
#endif

    // ScopedSignalHandler binding
    py::class_<ScopedSignalHandler>(
        m, "ScopedSignalHandler",
        R"(A scoped signal handler that automatically removes itself when destroyed.

This class provides RAII-style management of signal handlers to ensure
they're properly cleaned up when the object goes out of scope.

Args:
    signal: The signal to handle
    handler: The handler function
    priority: Priority of the handler (higher values = higher priority)
    use_safe_manager: Whether to use the SafeSignalManager (True) or direct registry (False)

Examples:
    >>> from atom.system import signal_utils
    >>> def handle_sigint(signal_id):
    ...     print(f"Caught signal {signal_utils.get_signal_name(signal_id)}")
    ...     return True  # Continue handling
    ... 
    >>> # Create a scoped handler for SIGINT
    >>> handler = signal_utils.ScopedSignalHandler(signal_utils.SIGINT, handle_sigint)
    >>> # The handler will be automatically removed when it goes out of scope
)")
        .def(py::init<SignalID, const SignalHandler&, int, bool>(),
             py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
             py::arg("use_safe_manager") = true,
             "Constructs a new ScopedSignalHandler.");

    // SignalGroup binding
    py::class_<SignalGroup, std::shared_ptr<SignalGroup>>(
        m, "SignalGroup",
        R"(A signal group that manages multiple related signal handlers.

This class allows you to group related signal handlers together for easier management.
When the group is destroyed, all its handlers are automatically removed.

Args:
    group_name: Name of the group (for logging)
    use_safe_manager: Whether to use the SafeSignalManager (True) or direct registry (False)

Examples:
    >>> from atom.system import signal_utils
    >>> # Create a signal group
    >>> group = signal_utils.SignalGroup("app_signals")
    >>> 
    >>> def handle_int(signal_id):
    ...     print("Handling SIGINT")
    ...     return True
    ... 
    >>> def handle_term(signal_id):
    ...     print("Handling SIGTERM")
    ...     return True
    ... 
    >>> # Add handlers to the group
    >>> group.add_handler(signal_utils.SIGINT, handle_int)
    >>> group.add_handler(signal_utils.SIGTERM, handle_term)
    >>> 
    >>> # All handlers will be removed when group is destroyed
)")
        .def(py::init<const std::string&, bool>(), py::arg("group_name") = "",
             py::arg("use_safe_manager") = true,
             "Constructs a new SignalGroup.")
        .def("add_handler", &SignalGroup::addHandler, py::arg("signal"),
             py::arg("handler"), py::arg("priority") = 0,
             R"(Add a handler to the group.

Args:
    signal: The signal to handle
    handler: The handler function
    priority: Priority of the handler (higher values = higher priority)

Returns:
    ID of the registered handler

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup("app_signals")
    >>> 
    >>> def handle_signal(signal_id):
    ...     print(f"Handling signal {signal_id}")
    ...     return True
    ... 
    >>> handler_id = group.add_handler(signal_utils.SIGINT, handle_signal)
    >>> print(f"Registered handler with ID: {handler_id}")
)")
        .def("remove_handler", &SignalGroup::removeHandler,
             py::arg("handler_id"),
             R"(Remove a specific handler by ID.

Args:
    handler_id: ID of the handler to remove

Returns:
    True if handler was successfully removed, False otherwise

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup()
    >>> 
    >>> def handle_signal(signal_id):
    ...     print(f"Handling signal {signal_id}")
    ...     return True
    ... 
    >>> handler_id = group.add_handler(signal_utils.SIGINT, handle_signal)
    >>> # Later, when we want to remove just this handler:
    >>> success = group.remove_handler(handler_id)
    >>> print(f"Handler removed: {success}")
)")
        .def("remove_signal_handlers", &SignalGroup::removeSignalHandlers,
             py::arg("signal"),
             R"(Remove all handlers for a specific signal.

Args:
    signal: Signal to remove handlers for

Returns:
    Number of handlers removed

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup()
    >>> 
    >>> # Add multiple handlers for SIGINT
    >>> group.add_handler(signal_utils.SIGINT, lambda sig: True)
    >>> group.add_handler(signal_utils.SIGINT, lambda sig: True)
    >>> 
    >>> # Remove all SIGINT handlers
    >>> removed = group.remove_signal_handlers(signal_utils.SIGINT)
    >>> print(f"Removed {removed} handlers")
)")
        .def("remove_all", &SignalGroup::removeAll,
             R"(Remove all handlers in this group.

Returns:
    Number of handlers removed

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup()
    >>> 
    >>> # Add handlers for different signals
    >>> group.add_handler(signal_utils.SIGINT, lambda sig: True)
    >>> group.add_handler(signal_utils.SIGTERM, lambda sig: True)
    >>> 
    >>> # Later, remove all handlers
    >>> removed = group.remove_all()
    >>> print(f"Removed {removed} handlers")
)")
        .def("get_handler_ids", &SignalGroup::getHandlerIds,
             R"(Get all registered handler IDs.

Returns:
    Dictionary mapping signal IDs to lists of handler IDs

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup()
    >>> 
    >>> group.add_handler(signal_utils.SIGINT, lambda sig: True)
    >>> group.add_handler(signal_utils.SIGTERM, lambda sig: True)
    >>> 
    >>> handler_ids = group.get_handler_ids()
    >>> for signal, ids in handler_ids.items():
    ...     signal_name = signal_utils.get_signal_name(signal)
    ...     print(f"{signal_name}: {len(ids)} handlers")
)")
        .def("get_group_name", &SignalGroup::getGroupName,
             R"(Get the group name.

Returns:
    Group name

Examples:
    >>> from atom.system import signal_utils
    >>> group = signal_utils.SignalGroup("application_signals")
    >>> name = group.get_group_name()
    >>> print(f"Group name: {name}")
)");

    // make_signal_group factory function
    m.def("make_signal_group", &makeSignalGroup, py::arg("group_name") = "",
          py::arg("use_safe_manager") = true,
          R"(Create a smart pointer to a SignalGroup.

Args:
    group_name: Name of the group (for logging)
    use_safe_manager: Whether to use the SafeSignalManager (True) or direct registry (False)

Returns:
    Shared pointer to the new group

Examples:
    >>> from atom.system import signal_utils
    >>> # Create a signal group
    >>> group = signal_utils.make_signal_group("app_signals")
    >>> 
    >>> def handle_signal(signal_id):
    ...     print(f"Handling signal {signal_id}")
    ...     return True
    ... 
    >>> group.add_handler(signal_utils.SIGINT, handle_signal)
    >>> # The group will be automatically cleaned up when the reference is lost
)",
          py::return_value_policy::take_ownership);

    // get_signal_name function
    m.def("get_signal_name", &getSignalName, py::arg("signal"),
          R"(Get the signal name as a string.

Args:
    signal: Signal ID

Returns:
    Name of the signal (e.g., "SIGINT", "SIGTERM")

Examples:
    >>> from atom.system import signal_utils
    >>> name = signal_utils.get_signal_name(signal_utils.SIGINT)
    >>> print(f"Signal name: {name}")  # Prints: Signal name: SIGINT
)");

    // Wrapper for withBlockedSignal template function
    m.def("with_blocked_signal", [](int signal, py::function function) {
        withBlockedSignal(signal, [&function]() { function(); });
    }, py::arg("signal"), py::arg("function"),
    R"(Temporarily block a signal during a critical section.

Args:
    signal: Signal to block
    function: Function to execute while signal is blocked

Examples:
    >>> from atom.system import signal_utils
    >>> import time
    >>> 
    >>> def critical_section():
    ...     print("Starting critical section (SIGINT blocked)")
    ...     time.sleep(2)  # During this time, SIGINT is blocked
    ...     print("Ending critical section")
    ... 
    >>> # SIGINT will be blocked during the execution of critical_section
    >>> signal_utils.with_blocked_signal(signal_utils.SIGINT, critical_section)
)");

    // Add context manager for signal handlers
    py::class_<py::object>(m, "SignalHandlerContext")
        .def(py::init([](SignalID signal, py::function handler, int priority, bool use_safe_manager) {
        return py::object();  // Placeholder, actual impl in __enter__
        }), py::arg("signal"), py::arg("handler"), py::arg("priority") = 0, 
           py::arg("use_safe_manager") = true,
           "Create a context manager for signal handling")
        .def("__enter__", [](py::object& self, SignalID signal, py::function handler, 
                           int priority, bool use_safe_manager) {
        // Wrap Python function in a C++ lambda
        SignalHandler cpp_handler = [handler](int signal_id) {
            py::gil_scoped_acquire acquire;
            try {
                py::object result = handler(signal_id);
                return py::cast<bool>(result);
            } catch (py::error_already_set& e) {
                // Log the error but don't propagate it
                PyErr_Print();
                return false;  // Don't continue handling
            }
        };

        // Create the ScopedSignalHandler
        auto scoped_handler = new ScopedSignalHandler(
            signal, cpp_handler, priority, use_safe_manager);
        self.attr("_handler") = py::capsule(scoped_handler, [](void* ptr) {
            delete static_cast<ScopedSignalHandler*>(ptr);
        });

        return self;
        })
        .def("__exit__", [](py::object& self, py::object, py::object, py::object) {
        // Handler will be deleted by the capsule's destructor
        return false;  // Don't suppress exceptions
        });

    // Factory function for the context manager
    m.def(
        "handle_signal",
        [](SignalID signal, py::function handler, int priority,
           bool use_safe_manager) {
            return m.attr("SignalHandlerContext")(signal, handler, priority,
                                                  use_safe_manager);
        },
        py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
        py::arg("use_safe_manager") = true,
        R"(Create a context manager for temporary signal handling.

Args:
    signal: The signal to handle
    handler: The handler function
    priority: Priority of the handler (higher values = higher priority)
    use_safe_manager: Whether to use the SafeSignalManager (True) or direct registry (False)

Returns:
    A context manager that sets up and tears down the signal handler

Examples:
    >>> from atom.system import signal_utils
    >>> import time
    >>> 
    >>> def handle_int(signal_id):
    ...     print("Got SIGINT, but continuing execution")
    ...     return True
    ... 
    >>> # Use as a context manager
    >>> with signal_utils.handle_signal(signal_utils.SIGINT, handle_int):
    ...     print("SIGINT will be handled specially in this block")
    ...     time.sleep(5)  # Try pressing Ctrl+C during this time
    ... 
    >>> print("Back to normal signal handling")
)");

    // Helper function for Python with-statement for blocked signals
    py::class_<py::object>(m, "BlockedSignalContext")
        .def(py::init([](int signal) {
                 return py::object();  // Placeholder, actual impl in __enter__
             }),
             py::arg("signal"), "Create a context manager for blocked signals")
        .def("__enter__",
             [](py::object& self, int signal) {
                 self.attr("signal") = py::int_(signal);

#if !defined(_WIN32) && !defined(_WIN64)
                 // Create new mask sets
                 sigset_t* block_set = new sigset_t;
                 sigset_t* old_set = new sigset_t;

                 sigemptyset(block_set);
                 sigaddset(block_set, signal);

                 // Block the signal
                 sigprocmask(SIG_BLOCK, block_set, old_set);

                 // Store both sets for __exit__
                 self.attr("_block_set") = py::capsule(
                     block_set,
                     [](void* ptr) { delete static_cast<sigset_t*>(ptr); });
                 self.attr("_old_set") = py::capsule(old_set, [](void* ptr) {
                     delete static_cast<sigset_t*>(ptr);
                 });
#endif

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
#if !defined(_WIN32) && !defined(_WIN64)
                 // Restore the old signal mask
                 sigset_t* old_set = static_cast<sigset_t*>(
                     PyDescr_NAME(self.attr("_old_set").ptr()));

                 sigprocmask(SIG_SETMASK, old_set, nullptr);
#endif

                 return false;  // Don't suppress exceptions
             });

    // Factory function for blocked signal context
    m.def(
        "block_signal",
        [](int signal) { return m.attr("BlockedSignalContext")(signal); },
        py::arg("signal"),
        R"(Create a context manager for temporarily blocking a signal.

Args:
    signal: The signal to block

Returns:
    A context manager that blocks and unblocks the signal

Examples:
    >>> from atom.system import signal_utils
    >>> import time
    >>> 
    >>> # Use as a context manager to block SIGINT
    >>> with signal_utils.block_signal(signal_utils.SIGINT):
    ...     print("SIGINT is blocked in this block")
    ...     print("Try pressing Ctrl+C, it won't interrupt until after the block")
    ...     time.sleep(5)
    ... 
    >>> print("SIGINT is now unblocked")
)");

    // Constructor function that returns a handler or group based on need
    m.def(
        "create_handler",
        [](py::args signals, py::function handler, int priority,
           bool use_safe_manager, const std::string& group_name) {
            if (signals.size() == 0) {
                throw py::value_error("At least one signal must be specified");
            }

            if (signals.size() == 1) {
                // Single signal, return a ScopedSignalHandler
                SignalID signal = py::cast<SignalID>(signals[0]);

                // Wrap Python function in a C++ lambda
                SignalHandler cpp_handler = [handler](int signal_id) {
                    py::gil_scoped_acquire acquire;
                    try {
                        py::object result = handler(signal_id);
                        return py::cast<bool>(result);
                    } catch (py::error_already_set& e) {
                        PyErr_Print();
                        return false;
                    }
                };

                return py::cast(
                    new ScopedSignalHandler(signal, cpp_handler, priority,
                                            use_safe_manager),
                    py::return_value_policy::take_ownership);
            } else {
                // Multiple signals, return a SignalGroup
                auto group =
                    std::make_shared<SignalGroup>(group_name, use_safe_manager);

                // Wrap Python function in a C++ lambda
                SignalHandler cpp_handler = [handler](int signal_id) {
                    py::gil_scoped_acquire acquire;
                    try {
                        py::object result = handler(signal_id);
                        return py::cast<bool>(result);
                    } catch (py::error_already_set& e) {
                        PyErr_Print();
                        return false;
                    }
                };

                // Add all signals to the group
                for (auto signal_arg : signals) {
                    SignalID signal = py::cast<SignalID>(signal_arg);
                    group->addHandler(signal, cpp_handler, priority);
                }

                return py::cast(group);
            }
        },
        py::pos_only(), py::arg("signals"), py::arg("handler"), py::kw_only(),
        py::arg("priority") = 0, py::arg("use_safe_manager") = true,
        py::arg("group_name") = "",
        R"(Create a signal handler or group based on the number of signals.

Args:
    *signals: One or more signal IDs
    handler: The handler function
    priority: Priority of the handler (keyword-only, default: 0)
    use_safe_manager: Whether to use SafeSignalManager (keyword-only, default: True)
    group_name: Group name if multiple signals (keyword-only, default: "")

Returns:
    ScopedSignalHandler if one signal, SignalGroup if multiple signals

Examples:
    >>> from atom.system import signal_utils
    >>> 
    >>> def handle_signal(signal_id):
    ...     signal_name = signal_utils.get_signal_name(signal_id)
    ...     print(f"Handling {signal_name}")
    ...     return True
    ... 
    >>> # Single signal handler
    >>> int_handler = signal_utils.create_handler(signal_utils.SIGINT, handle_signal)
    >>> 
    >>> # Multiple signal handler group
    >>> termination_handlers = signal_utils.create_handler(
    ...     signal_utils.SIGTERM, signal_utils.SIGINT, signal_utils.SIGQUIT,
    ...     handle_signal, group_name="termination_signals"
    ... )
)");

    // Helper for installing a temporary signal handler that captures the signal
    // once
    m.def(
        "capture_next_signal",
        [](SignalID signal, double timeout = -1.0) {
            py::object threading = py::module::import("threading");
            py::object event = threading.attr("Event")();

            py::dict result;
            result["signal_id"] = py::none();

            // Create a handler that captures the signal and sets the event
            SignalHandler cpp_handler = [&event, &result](int signal_id) {
                py::gil_scoped_acquire acquire;
                result["signal_id"] = py::int_(signal_id);
                event.attr("set")();
                return false;  // Don't continue handling
            };

            // Install the handler
            ScopedSignalHandler handler(signal, cpp_handler, 100,
                                        true);  // High priority

            // Wait for the event with optional timeout
            bool success = false;
            if (timeout < 0) {
                event.attr("wait")();
                success = true;
            } else {
                success = py::cast<bool>(event.attr("wait")(timeout));
            }

            return py::make_tuple(success, result["signal_id"]);
        },
        py::arg("signal"), py::arg("timeout") = -1.0,
        R"(Wait for the next occurrence of a signal, with optional timeout.

Args:
    signal: The signal to capture
    timeout: Timeout in seconds (negative for no timeout)

Returns:
    Tuple of (success, signal_id), where signal_id is None if timed out

Examples:
    >>> from atom.system import signal_utils
    >>> import threading, time
    >>> 
    >>> # Set up a thread to send a signal after a delay
    >>> def send_signal():
    ...     time.sleep(1)
    ...     import os, signal
    ...     os.kill(os.getpid(), signal.SIGUSR1)
    ... 
    >>> t = threading.Thread(target=send_signal)
    >>> t.start()
    >>> 
    >>> # Wait for the signal
    >>> success, sig = signal_utils.capture_next_signal(signal_utils.SIGUSR1, 2.0)
    >>> if success:
    ...     print(f"Captured signal: {signal_utils.get_signal_name(sig)}")
    ... else:
    ...     print("Timed out waiting for signal")
)");

    // Helper for checking if a signal is ignored
    m.def(
        "is_signal_ignored",
        [](SignalID signal) -> bool {
#if !defined(_WIN32) && !defined(_WIN64)
            struct sigaction current_action;
            sigaction(signal, nullptr, &current_action);
            return current_action.sa_handler == SIG_IGN;
#else
            // Windows doesn't have proper signal management
            return false;
#endif
        },
        py::arg("signal"),
        R"(Check if a signal is currently being ignored.

Args:
    signal: The signal to check

Returns:
    True if the signal is being ignored, False otherwise

Examples:
    >>> from atom.system import signal_utils
    >>> # Check if SIGPIPE is ignored
    >>> if signal_utils.is_signal_ignored(signal_utils.SIGPIPE):
    ...     print("SIGPIPE is being ignored")
    ... else:
    ...     print("SIGPIPE is not ignored")
)");

    // Create a dictionary of all signal names to values
    py::dict signal_dict;
    signal_dict["SIGABRT"] = py::int_(SIGABRT);
    signal_dict["SIGFPE"] = py::int_(SIGFPE);
    signal_dict["SIGILL"] = py::int_(SIGILL);
    signal_dict["SIGINT"] = py::int_(SIGINT);
    signal_dict["SIGSEGV"] = py::int_(SIGSEGV);
    signal_dict["SIGTERM"] = py::int_(SIGTERM);

#if !defined(_WIN32) && !defined(_WIN64)
    signal_dict["SIGALRM"] = py::int_(SIGALRM);
    signal_dict["SIGBUS"] = py::int_(SIGBUS);
    signal_dict["SIGCHLD"] = py::int_(SIGCHLD);
    signal_dict["SIGCONT"] = py::int_(SIGCONT);
    signal_dict["SIGHUP"] = py::int_(SIGHUP);
    signal_dict["SIGKILL"] = py::int_(SIGKILL);
    signal_dict["SIGPIPE"] = py::int_(SIGPIPE);
    signal_dict["SIGQUIT"] = py::int_(SIGQUIT);
    signal_dict["SIGSTOP"] = py::int_(SIGSTOP);
    signal_dict["SIGTSTP"] = py::int_(SIGTSTP);
    signal_dict["SIGTTIN"] = py::int_(SIGTTIN);
    signal_dict["SIGTTOU"] = py::int_(SIGTTOU);
    signal_dict["SIGUSR1"] = py::int_(SIGUSR1);
    signal_dict["SIGUSR2"] = py::int_(SIGUSR2);
#else
    signal_dict["SIGBREAK"] = py::int_(SIGBREAK);
#endif

    m.attr("SIGNALS") = signal_dict;

    // Add utility function to get all available signals
    m.def(
        "get_available_signals",
        []() {
            py::list signals;

            signals.append(SIGABRT);
            signals.append(SIGFPE);
            signals.append(SIGILL);
            signals.append(SIGINT);
            signals.append(SIGSEGV);
            signals.append(SIGTERM);

#if !defined(_WIN32) && !defined(_WIN64)
            signals.append(SIGALRM);
            signals.append(SIGBUS);
            signals.append(SIGCHLD);
            signals.append(SIGCONT);
            signals.append(SIGHUP);
            signals.append(SIGKILL);
            signals.append(SIGPIPE);
            signals.append(SIGQUIT);
            signals.append(SIGSTOP);
            signals.append(SIGTSTP);
            signals.append(SIGTTIN);
            signals.append(SIGTTOU);
            signals.append(SIGUSR1);
            signals.append(SIGUSR2);
#else
            signals.append(SIGBREAK);
#endif

            return signals;
        },
        R"(Get a list of all available signals on the current platform.

Returns:
    List of signal IDs

Examples:
    >>> from atom.system import signal_utils
    >>> signals = signal_utils.get_available_signals()
    >>> for sig in signals:
    ...     print(signal_utils.get_signal_name(sig))
)");
}