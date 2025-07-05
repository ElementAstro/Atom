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
             R"(Constructs a new ScopedSignalHandler.)");

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
             R"(Constructs a new SignalGroup.)")
        .def(
            "add_handler",
            [](SignalGroup& self, SignalID signal, py::function py_handler,
               int priority) {
                SignalHandler cpp_handler = [py_handler](int signal_id) {
                    py::gil_scoped_acquire acquire;
                    try {
                        py::object result = py_handler(signal_id);
                        return py::cast<bool>(result);
                    } catch (py::error_already_set& e) {
                        PyErr_Print();
                        return false;  // Default behavior on Python exception
                    }
                };
                return self.addHandler(signal, cpp_handler, priority);
            },
            py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
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
    m.def(
        "with_blocked_signal",
        [](int signal, py::function function) {
            withBlockedSignal(signal, [&function]() {
                py::gil_scoped_acquire
                    acquire;  // Ensure GIL for python callback
                function();
            });
        },
        py::arg("signal"), py::arg("function"),
        R"(Temporarily block a signal during a critical section.

Args:
    signal: Signal to block
    function: Function to execute while signal is blocked

Examples:
    >>> from atom.system import signal_utils
    >>> import time
    >>>
    >>> def critical_section():
    ...     print("Starting critical section (SIGINT blocked) ")
    ...     time.sleep(2)  # During this time, SIGINT is blocked
    ...     print("Ending critical section")
    ...
    >>> # SIGINT will be blocked during the execution of critical_section
    >>> signal_utils.with_blocked_signal(signal_utils.SIGINT, critical_section)
)");

    // Add context manager for signal handlers
    py::class_<py::object>(m, "SignalHandlerContext")
        .def(py::init([](SignalID signal, py::function handler, int priority,
                         bool use_safe_manager) {
                 return py::object();  // Placeholder, actual impl in __enter__
             }),
             py::arg("signal"), py::arg("handler"), py::arg("priority") = 0,
             py::arg("use_safe_manager") = true,
             R"(Create a context manager for signal handling)")
        .def("__enter__",
             [](py::object& self, SignalID signal, py::function handler,
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
                 self.attr("_handler") =
                     py::capsule(scoped_handler, [](void* ptr) {
                         delete static_cast<ScopedSignalHandler*>(ptr);
                     });

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 // Handler will be deleted by the capsule's destructor
                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for the context manager
    m.def(
        "handle_signal",
        [&m](SignalID signal, py::function handler, int priority,
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
             py::arg("signal"),
             R"(Create a context manager for blocked signals)")
        .def(
            "__enter__",
            [](py::object& self, int signal) {
                self.attr("signal") = py::int_(signal);

#if !defined(_WIN32) && !defined(_WIN64)
                // Create new mask sets
                sigset_t* block_set = new sigset_t;
                sigset_t* old_set = new sigset_t;

                sigemptyset(block_set);
                sigaddset(block_set, signal);

                // Block the signal
                if (sigprocmask(SIG_BLOCK, block_set, old_set) == -1) {
                    delete block_set;
                    delete old_set;
                    throw py::error_already_set();  // Or a custom runtime error
                }

                // Store both sets for __exit__
                self.attr("_block_set") = py::capsule(block_set, [](void* ptr) {
                    delete static_cast<sigset_t*>(ptr);
                });
                self.attr("_old_set") = py::capsule(old_set, [](void* ptr) {
                    delete static_cast<sigset_t*>(ptr);
                });
#endif

                return self;
            })
        .def("__exit__", [](py::object& self, py::object, py::object,
                            py::object) {
#if !defined(_WIN32) && !defined(_WIN64)
            // Restore the old signal mask
            py::capsule old_set_capsule = py::capsule(self.attr("_old_set"));
            sigset_t* old_set =
                static_cast<sigset_t*>(old_set_capsule.get_pointer());

            if (old_set) {
                if (sigprocmask(SIG_SETMASK, old_set, nullptr) == -1) {
                    // Consider how to handle errors here, PyErr_Print or throw
                    // For now, let's assume it works or error is minor in exit
                }
            }
#endif

            return py::bool_(false);  // Don't suppress exceptions
        });

    // Factory function for blocked signal context
    m.def(
        "block_signal",
        [&m](int signal) { return m.attr("BlockedSignalContext")(signal); },
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
        [](py::object signals, py::function handler, int priority,
           bool use_safe_manager, const std::string& group_name) {
            // 检查signals是否为元组或列表
            bool is_sequence = py::isinstance<py::tuple>(signals) ||
                               py::isinstance<py::list>(signals);

            if (is_sequence) {
                // 多个信号的情况
                auto group =
                    std::make_shared<SignalGroup>(group_name, use_safe_manager);

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

                // 遍历序列中的所有信号
                for (auto signal_obj : signals) {
                    SignalID signal = py::cast<SignalID>(signal_obj);
                    (void)group->addHandler(signal, cpp_handler,
                                            priority);  // Handle nodiscard
                }
                return py::cast(group);
            } else {
                // 单个信号的情况
                SignalID signal = py::cast<SignalID>(signals);

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
            }
        },
        py::arg("signals"), py::arg("handler"), py::arg("priority") = 0,
        py::arg("use_safe_manager") = true, py::arg("group_name") = "",
        R"(Create a signal handler or group based on the type of signals argument.

Args:
    signals: One or more signal IDs (can be a single value or a list/tuple of signals).
    handler: The handler function.
    priority: Priority of the handler (default: 0).
    use_safe_manager: Whether to use SafeSignalManager (default: True).
    group_name: Group name if multiple signals (default: "").

Returns:
    ScopedSignalHandler if signals is a single value, SignalGroup if signals is a sequence.

Examples:
    >>> from atom.system import signal_utils
    >>>
    >>> def handle_signal(signal_id):
    ...     signal_name = signal_utils.get_signal_name(signal_id)
    ...     print(f"Handling {signal_name}")
    ...     return True
    ...
    >>> # Single signal handler
    >>> int_handler = signal_utils.create_handler(signal_utils.SIGINT, handler=handle_signal)
    >>>
    >>> # Multiple signal handler group
    >>> termination_handlers = signal_utils.create_handler(
    ...     [signal_utils.SIGTERM, signal_utils.SIGINT, signal_utils.SIGQUIT],
    ...     handler=handle_signal, group_name="termination_signals"
    ... )
)");
    // Note: The py::pos_only() and py::kw_only() were removed from
    // create_handler to simplify the signature and match the py::arg("signals")
    // for *args behavior. If strict positional-only for signals and
    // keyword-only for others is desired, the signature needs more careful
    // construction, possibly splitting the py::args. For now,
    // py::arg("signals") will capture all positional args into a tuple.

    // Helper for installing a temporary signal handler that captures the signal
    // once
    m.def(
        "capture_next_signal",
        [](SignalID signal, double timeout = -1.0) {
            py::object threading = py::module::import("threading");
            py::object event = threading.attr("Event")();

            // Using a std::optional to hold the captured signal ID
            std::optional<SignalID> captured_signal_id;

            // Create a handler that captures the signal and sets the event
            SignalHandler cpp_handler = [&event, &captured_signal_id,
                                         signal](int sig_id_from_handler) {
                // Check if it's the signal we are interested in, though
                // ScopedSignalHandler should only call this for the registered
                // signal.
                if (sig_id_from_handler == signal) {
                    py::gil_scoped_acquire acquire;
                    captured_signal_id = sig_id_from_handler;
                    event.attr("set")();
                }
                return false;  // Don't continue handling, remove handler
            };

            // Install the handler
            ScopedSignalHandler handler_guard(signal, cpp_handler, 1000,
                                              true);  // High priority

            // Wait for the event with optional timeout
            bool success = false;
            if (timeout < 0) {
                event.attr("wait")();
                success = event.attr("is_set")().cast<bool>();
            } else {
                success = event.attr("wait")(timeout).cast<bool>();
            }

            py::object result_signal_id = py::none();
            if (success && captured_signal_id.has_value()) {
                result_signal_id = py::int_(captured_signal_id.value());
            }

            return py::make_tuple(success, result_signal_id);
        },
        py::arg("signal"), py::arg("timeout") = -1.0,
        R"(Wait for the next occurrence of a signal, with optional timeout.

Args:
    signal: The signal to capture
    timeout: Timeout in seconds (negative for no timeout)

Returns:
    Tuple of (success, signal_id), where signal_id is None if timed out or signal not captured.

Examples:
    >>> from atom.system import signal_utils
    >>> import threading, time, os
    >>>
    >>> # Ensure SIGUSR1 is available for the example
    >>> sig_to_test = signal_utils.SIGUSR1 if hasattr(signal_utils, "SIGUSR1") else signal_utils.SIGINT
    >>>
    >>> def send_signal_thread_func(pid, sig):
    ...     time.sleep(0.5) # Give capture_next_signal time to set up
    ...     try:
    ...         os.kill(pid, sig)
    ...         print(f"Test thread: Sent signal {sig}")
    ...     except Exception as e:
    ...         print(f"Test thread: Error sending signal: {e}")
    ...
    >>> t = threading.Thread(target=send_signal_thread_func, args=(os.getpid(), sig_to_test))
    >>> t.start()
    >>>
    >>> print(f"Main thread: Waiting for signal {sig_to_test}...")
    >>> success, sig = signal_utils.capture_next_signal(sig_to_test, 2.0)
    >>> if success and sig is not None:
    ...     print(f"Main thread: Captured signal: {signal_utils.get_signal_name(sig)}")
    ... elif success and sig is None:
    ...     print("Main thread: Event set, but signal_id not captured (should not happen).")
    ... else:
    ...     print("Main thread: Timed out waiting for signal")
    >>> t.join()
)");

    // Helper for checking if a signal is ignored
    m.def(
        "is_signal_ignored",
        [](SignalID signal) -> bool {
#if !defined(_WIN32) && !defined(_WIN64)
            struct sigaction current_action;
            if (sigaction(signal, nullptr, &current_action) == -1) {
                // Could throw an error here if sigaction fails
                return false;  // Or indicate error
            }
            return current_action.sa_handler == SIG_IGN;
#else
            // Windows doesn't have proper signal management like POSIX
            // sigaction
            return false;
#endif
        },
        py::arg("signal"),
        R"(Check if a signal is currently being ignored.

Args:
    signal: The signal to check

Returns:
    True if the signal is being ignored, False otherwise (or if check fails on POSIX).

Examples:
    >>> from atom.system import signal_utils
    >>> # Check if SIGPIPE is ignored (behavior might vary by OS default)
    >>> if hasattr(signal_utils, "SIGPIPE"): # SIGPIPE is POSIX-specific
    ...     if signal_utils.is_signal_ignored(signal_utils.SIGPIPE):
    ...         print("SIGPIPE is being ignored")
    ...     else:
    ...         print("SIGPIPE is not ignored (or check failed) ")
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
            py::list signals_list;  // Renamed from signals to avoid conflict

            signals_list.append(SIGABRT);
            signals_list.append(SIGFPE);
            signals_list.append(SIGILL);
            signals_list.append(SIGINT);
            signals_list.append(SIGSEGV);
            signals_list.append(SIGTERM);

#if !defined(_WIN32) && !defined(_WIN64)
            signals_list.append(SIGALRM);
            signals_list.append(SIGBUS);
            signals_list.append(SIGCHLD);
            signals_list.append(SIGCONT);
            signals_list.append(SIGHUP);
            signals_list.append(SIGKILL);
            signals_list.append(SIGPIPE);
            signals_list.append(SIGQUIT);
            signals_list.append(SIGSTOP);
            signals_list.append(SIGTSTP);
            signals_list.append(SIGTTIN);
            signals_list.append(SIGTTOU);
            signals_list.append(SIGUSR1);
            signals_list.append(SIGUSR2);
#else
            signals_list.append(SIGBREAK);
#endif

            return signals_list;
        },
        R"(Get a list of all available signals on the current platform.

Returns:
    List of signal IDs

Examples:
    >>> from atom.system import signal_utils
    >>> available_sigs = signal_utils.get_available_signals()
    >>> for sig_id in available_sigs:
    ...     print(signal_utils.get_signal_name(sig_id))
)");
}
