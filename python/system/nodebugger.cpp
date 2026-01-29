#include "atom/system/debug/nodebugger.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(nodebugger, m) {
    m.doc() = R"pbdoc(
        Anti-Debugging Module
        ---------------------

        This module provides advanced anti-debugging functionality to detect and
        respond to debugging attempts. It offers multiple detection methods and
        configurable actions to protect software from reverse engineering and
        tampering.

        The nodebugger module includes various techniques to detect debuggers:
        - Basic debugger checks
        - Timing-based detection
        - Exception-based detection
        - Hardware and memory breakpoint detection
        - Process environment analysis
        - Thread context examination

        It also provides anti-tampering features such as memory protection,
        integrity checks, and dump prevention.

        Examples:
            >>> from atom.system import nodebugger
            >>>
            >>> # Simple debugger check and exit if detected
            >>> nodebugger.check_debugger_and_exit()
            >>>
            >>> # Check if debugger is attached with specific method
            >>> if nodebugger.is_debugger_attached(
            ...     nodebugger.DebuggerDetectionMethod.TIMING_CHECK):
            ...     print("Debugger detected!")
            >>>
            >>> # Configure anti-debug behavior
            >>> config = nodebugger.AntiDebugConfig()
            >>> config.enabled = True
            >>> config.method = nodebugger.DebuggerDetectionMethod.ALL_METHODS
            >>> config.action = nodebugger.AntiDebugAction.MISLEAD
            >>> config.continuous_monitoring = True
            >>> config.check_interval = 1000  # Check every second
            >>> nodebugger.start_anti_debug_monitoring(config)

        Warning:
            Anti-debugging techniques can interfere with legitimate debugging tools.
            Use with caution during development and testing. Some features may
            affect application stability or performance.

        Note:
            Some features are platform-specific. Windows-specific functions will
            have no effect on other platforms.
    )pbdoc";

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

    // DebuggerDetectionMethod enum
    py::enum_<atom::system::DebuggerDetectionMethod>(
        m, "DebuggerDetectionMethod",
        R"(Methods for detecting debugger presence.

This enum defines various techniques for detecting if a debugger is attached
to the process. Different methods have different performance characteristics
and detection rates.

Values:
    BASIC_CHECK: Simple, fast check for debugger presence.
    TIMING_CHECK: Detects debuggers based on execution timing anomalies.
    EXCEPTION_BASED: Uses exceptions to detect debugging.
    HARDWARE_BREAKPOINTS: Checks for hardware breakpoint registers.
    MEMORY_BREAKPOINTS: Detects memory breakpoints.
    PROCESS_ENVIRONMENT: Examines process environment for debugger signs.
    PARENT_PROCESS: Checks if parent process is a known debugger.
    THREAD_CONTEXT: Analyzes thread context for debugging indicators.
    ALL_METHODS: Uses all available detection methods (most thorough).

Examples:
    >>> # Use basic check for performance
    >>> if nodebugger.is_debugger_attached(
    ...     nodebugger.DebuggerDetectionMethod.BASIC_CHECK):
    ...     handle_debugger()
    >>>
    >>> # Use comprehensive check for security
    >>> if nodebugger.is_debugger_attached(
    ...     nodebugger.DebuggerDetectionMethod.ALL_METHODS):
    ...     exit(1)
)")
        .value("BASIC_CHECK",
               atom::system::DebuggerDetectionMethod::BASIC_CHECK,
               "Simple debugger presence check")
        .value("TIMING_CHECK",
               atom::system::DebuggerDetectionMethod::TIMING_CHECK,
               "Timing-based detection")
        .value("EXCEPTION_BASED",
               atom::system::DebuggerDetectionMethod::EXCEPTION_BASED,
               "Exception-based detection")
        .value("HARDWARE_BREAKPOINTS",
               atom::system::DebuggerDetectionMethod::HARDWARE_BREAKPOINTS,
               "Hardware breakpoint detection")
        .value("MEMORY_BREAKPOINTS",
               atom::system::DebuggerDetectionMethod::MEMORY_BREAKPOINTS,
               "Memory breakpoint detection")
        .value("PROCESS_ENVIRONMENT",
               atom::system::DebuggerDetectionMethod::PROCESS_ENVIRONMENT,
               "Process environment analysis")
        .value("PARENT_PROCESS",
               atom::system::DebuggerDetectionMethod::PARENT_PROCESS,
               "Parent process check")
        .value("THREAD_CONTEXT",
               atom::system::DebuggerDetectionMethod::THREAD_CONTEXT,
               "Thread context examination")
        .value("ALL_METHODS",
               atom::system::DebuggerDetectionMethod::ALL_METHODS,
               "All detection methods combined")
        .export_values();

    // AntiDebugAction enum
    py::enum_<atom::system::AntiDebugAction>(
        m, "AntiDebugAction",
        R"(Actions to take when debugger is detected.

This enum defines the actions that can be taken when a debugger is detected.
Different actions provide different levels of protection and application behavior.

Values:
    EXIT: Terminate the application immediately.
    CRASH: Cause the application to crash.
    MISLEAD: Provide misleading information to the debugger.
    CORRUPT_MEMORY: Corrupt memory to prevent analysis.
    CUSTOM: Execute a custom user-defined action.

Examples:
    >>> config = nodebugger.AntiDebugConfig()
    >>> config.action = nodebugger.AntiDebugAction.MISLEAD  # Confuse debugger
    >>> nodebugger.handle_debugger_detection(config)

Warning:
    CRASH and CORRUPT_MEMORY actions may leave the system in an unstable state.
    Use with extreme caution.
)")
        .value("EXIT", atom::system::AntiDebugAction::EXIT,
               "Exit the application")
        .value("CRASH", atom::system::AntiDebugAction::CRASH,
               "Crash the application")
        .value("MISLEAD", atom::system::AntiDebugAction::MISLEAD,
               "Mislead the debugger")
        .value("CORRUPT_MEMORY", atom::system::AntiDebugAction::CORRUPT_MEMORY,
               "Corrupt memory regions")
        .value("CUSTOM", atom::system::AntiDebugAction::CUSTOM,
               "Execute custom callback")
        .export_values();

    // AntiDebugConfig structure
    py::class_<atom::system::AntiDebugConfig>(
        m, "AntiDebugConfig",
        R"(Configuration for anti-debugging behavior.

This class holds configuration options for anti-debugging features, including
detection methods, actions to take, and monitoring settings.

Attributes:
    enabled: Whether anti-debugging is enabled.
    method: The detection method to use.
    action: The action to take when debugger is detected.
    custom_action: Custom callback function for CUSTOM action.
    timing_threshold: Threshold in microseconds for timing checks.
    continuous_monitoring: Whether to continuously monitor for debuggers.
    check_interval: Milliseconds between checks if continuous monitoring is enabled.

Examples:
    >>> config = nodebugger.AntiDebugConfig()
    >>> config.enabled = True
    >>> config.method = nodebugger.DebuggerDetectionMethod.ALL_METHODS
    >>> config.action = nodebugger.AntiDebugAction.EXIT
    >>> config.continuous_monitoring = True
    >>> config.check_interval = 500
    >>> nodebugger.start_anti_debug_monitoring(config)
)")
        .def(py::init<>(),
             "Constructs a new AntiDebugConfig with default values.")
        .def_readwrite("enabled", &atom::system::AntiDebugConfig::enabled,
                       "Enable or disable anti-debugging")
        .def_readwrite("method", &atom::system::AntiDebugConfig::method,
                       "Detection method to use")
        .def_readwrite("action", &atom::system::AntiDebugConfig::action,
                       "Action to take when debugger detected")
        .def_readwrite("custom_action",
                       &atom::system::AntiDebugConfig::customAction,
                       "Custom action callback (for CUSTOM action)")
        .def_readwrite("timing_threshold",
                       &atom::system::AntiDebugConfig::timingThreshold,
                       "Timing threshold in microseconds")
        .def_readwrite("continuous_monitoring",
                       &atom::system::AntiDebugConfig::continuousMonitoring,
                       "Enable continuous monitoring")
        .def_readwrite("check_interval",
                       &atom::system::AntiDebugConfig::checkInterval,
                       "Check interval in milliseconds");

    // Basic API functions
    m.def("check_debugger_and_exit", &atom::system::checkDebuggerAndExit,
          R"(Check for debugger and exit if detected.

This is a simple convenience function that performs a basic debugger check
and immediately exits the application if a debugger is detected.

This function is backward compatible with older versions and provides the
simplest way to add basic anti-debugging protection.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Add at application startup
    >>> def main():
    ...     nodebugger.check_debugger_and_exit()
    ...     # Rest of application code
    ...     run_application()

Warning:
    This will immediately terminate your application if a debugger is detected.
    Not recommended for development builds.
)");

    m.def(
        "is_debugger_attached", &atom::system::isDebuggerAttached,
        py::arg("method") = atom::system::DebuggerDetectionMethod::BASIC_CHECK,
        R"(Check if a debugger is attached to the process.

This function checks for the presence of a debugger using the specified
detection method. Different methods have different performance and accuracy
characteristics.

Args:
    method: The detection method to use. Defaults to BASIC_CHECK for
            performance. Use ALL_METHODS for comprehensive detection.

Returns:
    bool: True if a debugger is detected, False otherwise.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Quick check
    >>> if nodebugger.is_debugger_attached():
    ...     print("Debugger detected!")
    >>>
    >>> # Thorough check
    >>> if nodebugger.is_debugger_attached(
    ...     nodebugger.DebuggerDetectionMethod.ALL_METHODS):
    ...     print("Debugger definitely present!")
    >>>
    >>> # Use specific method
    >>> if nodebugger.is_debugger_attached(
    ...     nodebugger.DebuggerDetectionMethod.TIMING_CHECK):
    ...     print("Timing anomaly detected!")

Note:
    - BASIC_CHECK is fast but may miss sophisticated debuggers
    - ALL_METHODS is more accurate but slower
    - Some methods may have false positives in certain environments
)");

    m.def("handle_debugger_detection", &atom::system::handleDebuggerDetection,
          py::arg("config") = atom::system::AntiDebugConfig{},
          R"(Handle debugger detection with specified configuration.

This function checks for a debugger and takes the configured action if one
is detected. It provides fine-grained control over anti-debugging behavior.

Args:
    config: Configuration specifying detection method and action.
            Defaults to basic check with exit action.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Use with default config (basic check, exit on detect)
    >>> nodebugger.handle_debugger_detection()
    >>>
    >>> # Custom configuration
    >>> config = nodebugger.AntiDebugConfig()
    >>> config.method = nodebugger.DebuggerDetectionMethod.TIMING_CHECK
    >>> config.action = nodebugger.AntiDebugAction.MISLEAD
    >>> nodebugger.handle_debugger_detection(config)
    >>>
    >>> # With custom action
    >>> def on_debugger_detected():
    ...     log_security_event("Debugger detected")
    ...     send_alert()
    >>> config.action = nodebugger.AntiDebugAction.CUSTOM
    >>> config.custom_action = on_debugger_detected
    >>> nodebugger.handle_debugger_detection(config)
)");

    m.def("start_anti_debug_monitoring",
          &atom::system::startAntiDebugMonitoring,
          py::arg("config") = atom::system::AntiDebugConfig{},
          R"(Start continuous anti-debugging monitoring.

This function starts a background monitoring thread that continuously checks
for debuggers at the specified interval. This provides ongoing protection
against debuggers being attached after application startup.

Args:
    config: Configuration for monitoring behavior, including detection method,
            action, and check interval.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Start monitoring with defaults
    >>> nodebugger.start_anti_debug_monitoring()
    >>>
    >>> # Configure monitoring
    >>> config = nodebugger.AntiDebugConfig()
    >>> config.enabled = True
    >>> config.continuous_monitoring = True
    >>> config.check_interval = 1000  # Check every second
    >>> config.method = nodebugger.DebuggerDetectionMethod.ALL_METHODS
    >>> config.action = nodebugger.AntiDebugAction.EXIT
    >>> nodebugger.start_anti_debug_monitoring(config)
    >>>
    >>> # ... application runs ...
    >>>
    >>> # Stop monitoring when done
    >>> nodebugger.stop_anti_debug_monitoring()

Warning:
    - Continuous monitoring adds runtime overhead
    - Remember to call stop_anti_debug_monitoring() before application exit
    - The monitoring thread runs in the background

Note:
    If continuous_monitoring is False in config, this function performs
    a single check instead of starting a monitoring thread.
)");

    m.def("stop_anti_debug_monitoring", &atom::system::stopAntiDebugMonitoring,
          R"(Stop continuous anti-debugging monitoring.

This function stops the background monitoring thread started by
start_anti_debug_monitoring(). It should be called before application
exit to ensure clean shutdown.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Start monitoring
    >>> nodebugger.start_anti_debug_monitoring()
    >>>
    >>> # ... application runs ...
    >>>
    >>> # Stop monitoring
    >>> nodebugger.stop_anti_debug_monitoring()

Note:
    It's safe to call this even if monitoring was never started.
    The function will simply return without error.
)");

    // Anti-tampering functions
    m.def("protect_memory_region", &atom::system::protectMemoryRegion,
          py::arg("address"), py::arg("size"),
          R"(Protect a memory region from tampering.

This function marks a memory region as protected, making it more difficult
for debuggers and malware to modify. The protection mechanism is
platform-specific and may not be available on all systems.

Args:
    address: Starting address of the memory region (as integer).
    size: Size of the memory region in bytes.

Examples:
    >>> from atom.system import nodebugger
    >>> import ctypes
    >>>
    >>> # Protect a code region
    >>> code_start = ctypes.addressof(my_function)
    >>> code_size = 1024  # Approximate size
    >>> nodebugger.protect_memory_region(code_start, code_size)

Warning:
    Incorrect use can cause application crashes. Ensure the address range
    is valid and owned by your application.

Note:
    - Protection is not absolute - determined attackers can bypass it
    - May interfere with legitimate dynamic code generation
    - Platform-specific behavior
)");

    m.def("install_integrity_checks", &atom::system::installIntegrityChecks,
          py::arg("code_start"), py::arg("code_size"), py::arg("hash"),
          R"(Install integrity checks for code regions.

This function sets up periodic integrity checks to detect if code has been
modified at runtime. It compares the current state of the code against a
precomputed hash.

Args:
    code_start: Starting address of the code region (as integer).
    code_size: Size of the code region in bytes.
    hash: Expected hash value as bytes. Must be 32 bytes for SHA-256.

Examples:
    >>> from atom.system import nodebugger
    >>> import hashlib
    >>> import ctypes
    >>>
    >>> # Calculate hash of code region
    >>> code_start = ctypes.addressof(my_function)
    >>> code_size = 1024
    >>> code_bytes = ctypes.string_at(code_start, code_size)
    >>> code_hash = hashlib.sha256(code_bytes).digest()
    >>>
    >>> # Install integrity check
    >>> nodebugger.install_integrity_checks(code_start, code_size, code_hash)

Warning:
    Self-modifying code will trigger integrity check failures.
    Not suitable for JIT-compiled code regions.

Note:
    - Adds runtime overhead for integrity checking
    - Hash algorithm is typically SHA-256
    - Detects tampering, does not prevent it
)");

    m.def("prevent_dumping", &atom::system::preventDumping,
          R"(Prevent process memory dumping.

This function applies various protections to make it more difficult to dump
the process memory. This includes preventing debuggers from creating memory
dumps and protecting against memory scanning tools.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Apply dump prevention at startup
    >>> def main():
    ...     nodebugger.prevent_dumping()
    ...     run_application()

Warning:
    May interfere with legitimate crash dump generation.
    Consider disabling in development builds.

Note:
    - Protection is not absolute
    - May affect crash reporting tools
    - Platform-specific implementation
    - Some antivirus software may flag this as suspicious
)");

    // Windows-specific functions
#ifdef _WIN32
    m.def(
        "hide_peb_debugging_flags", &atom::system::hidePEBDebuggingFlags,
        R"(Hide debugging flags in the Process Environment Block (Windows only).

This function modifies the PEB to hide debugging indicators that debuggers
use to determine if they are attached. This is a Windows-specific
anti-debugging technique.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Hide PEB flags on Windows
    >>> if hasattr(nodebugger, 'hide_peb_debugging_flags'):
    ...     nodebugger.hide_peb_debugging_flags()

Note:
    - Windows only - this function is not available on other platforms
    - Effective against many common debuggers
    - Sophisticated debuggers may detect this technique
)");

    m.def("detect_remote_threads", &atom::system::detectRemoteThreads,
          R"(Detect remote threads injected by debuggers (Windows only).

This function checks for threads that were created by external processes,
which is a common technique used by debuggers and malware. This is a
Windows-specific detection method.

Examples:
    >>> from atom.system import nodebugger
    >>>
    >>> # Check for remote threads on Windows
    >>> if hasattr(nodebugger, 'detect_remote_threads'):
    ...     nodebugger.detect_remote_threads()

Note:
    - Windows only - this function is not available on other platforms
    - May have false positives with legitimate system processes
    - Useful for detecting DLL injection and debugging tools
)");

    m.def("enable_self_modifying_code", &atom::system::enableSelfModifyingCode,
          py::arg("code_address"), py::arg("code_size"),
          R"(Enable self-modifying code protection (Windows only).

This function enables a code region to modify itself, which can confuse
static analysis tools and debuggers. This is a Windows-specific
anti-analysis technique.

Args:
    code_address: Starting address of the code region (as integer).
    code_size: Size of the code region in bytes.

Examples:
    >>> from atom.system import nodebugger
    >>> import ctypes
    >>>
    >>> # Enable self-modification on Windows
    >>> if hasattr(nodebugger, 'enable_self_modifying_code'):
    ...     code_addr = ctypes.addressof(my_function)
    ...     nodebugger.enable_self_modifying_code(code_addr, 1024)

Warning:
    Self-modifying code is difficult to debug and maintain.
    Use only when necessary for protection.

Note:
    - Windows only - this function is not available on other platforms
    - May trigger antivirus warnings
    - Requires careful memory management
    - Can cause crashes if used incorrectly
)");
#endif
}
