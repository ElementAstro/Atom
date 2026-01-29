#include "atom/system/debug/crash.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(crash, m) {
    m.doc() = R"pbdoc(
        Crash Report Module
        -------------------

        This module provides functionality for saving crash logs and gathering system
        information for debugging purposes. It includes utilities to capture detailed
        system state when an application crashes, including system information, stack
        traces, environment variables, and crash dump files (on Windows).

        The crash module automatically collects comprehensive diagnostic information
        to aid in debugging and post-mortem analysis of application failures.

        Examples:
            >>> from atom.system import crash
            >>>
            >>> # Get current system information
            >>> sys_info = crash.get_system_info()
            >>> print(sys_info)
            >>>
            >>> # Save a crash log with error details
            >>> try:
            ...     # Some operation that might crash
            ...     risky_operation()
            ... except Exception as e:
            ...     crash.save_crash_log(f"Application crashed: {str(e)}")

        Functions:
            save_crash_log: Save detailed crash information to a log file
            get_system_info: Get comprehensive system information for diagnostics

        Note:
            On Windows, crash logs will include minidump files for advanced debugging.
            Ensure the crash log directory has appropriate write permissions.
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

    m.def("save_crash_log", &atom::system::saveCrashLog, py::arg("error_msg"),
          R"(Save crash log with detailed system information.

This function saves comprehensive crash information including:
- Error message details
- System information (OS, CPU, memory)
- Stack trace (where available)
- Environment variables
- Crash dump files (Windows only)

The crash log is saved to a timestamped file in the crash logs directory,
making it easy to correlate crashes with specific incidents.

Args:
    error_msg: Detailed information about the crash. This should include
               the error message, relevant context, and any other information
               that will help in debugging the issue.

Raises:
    RuntimeError: If the crash log directory is not writable or if there's
                  an error creating the crash dump file.

Examples:
    >>> from atom.system import crash
    >>>
    >>> # Basic crash logging
    >>> crash.save_crash_log("Unexpected null pointer dereference in module X")
    >>>
    >>> # Crash logging with exception details
    >>> try:
    ...     result = 1 / 0
    ... except Exception as e:
    ...     crash.save_crash_log(f"Division error: {type(e).__name__}: {str(e)}")
    >>>
    >>> # Crash logging with additional context
    >>> def process_data(data):
    ...     try:
    ...         # Processing logic
    ...         process(data)
    ...     except Exception as e:
    ...         context = f"Failed processing data: {data[:100]}"
    ...         crash.save_crash_log(f"{context}\nError: {str(e)}")
    ...         raise

Note:
    - Crash logs are automatically timestamped
    - On Windows, a minidump file (.dmp) is created alongside the log
    - The log includes a full system snapshot at the time of the crash
    - Ensure adequate disk space for crash dumps
)");

    m.def("get_system_info", &atom::system::getSystemInfo,
          R"(Get comprehensive system information for crash reports.

This function collects detailed information about the system, which is useful
for crash analysis and debugging. The information includes:

- Operating System: Name, version, and build number
- CPU: Model, core count, architecture, and current usage
- Memory: Total RAM, available RAM, and memory usage percentage
- Disk: Total space, free space, and usage for system drive
- Additional platform-specific details

The returned information is formatted as a human-readable string that can be
included in crash reports, diagnostic logs, or system monitoring tools.

Returns:
    str: A formatted string containing comprehensive system information.
         Each piece of information is on a separate line with clear labels.

Examples:
    >>> from atom.system import crash
    >>>
    >>> # Get and print system information
    >>> info = crash.get_system_info()
    >>> print(info)
    Operating System: Windows 10 Pro 64-bit (Build 19045)
    CPU: Intel Core i7-9700K @ 3.60GHz (8 cores)
    CPU Usage: 23.5%
    Memory: 16.0 GB total, 8.2 GB available (48.8% used)
    Disk: C:\ - 512.0 GB total, 128.5 GB free (74.9% used)
    >>>
    >>> # Log system info to a file
    >>> with open("system_info.txt", "w") as f:
    ...     f.write(crash.get_system_info())
    >>>
    >>> # Include in error reports
    >>> def report_error(error_msg):
    ...     report = f"Error: {error_msg}\n\n"
    ...     report += "System Information:\n"
    ...     report += crash.get_system_info()
    ...     send_error_report(report)

Note:
    - This function may take a moment to collect all information
    - Some information may not be available on all platforms
    - The format may vary slightly between operating systems
    - Consider caching this information if called frequently
)");
}
