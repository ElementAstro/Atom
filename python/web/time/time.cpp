#include "atom/web/time.hpp"
#include "atom/web/time/time_error.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void init_time(py::module_& m) {
    m.attr("__doc__") = R"pbdoc(
        Time Management Module
        ---------------------

        This module provides comprehensive time management functionality including system time
        operations, timezone management, NTP synchronization, and RTC operations. It offers
        both high-level and low-level interfaces for time-related operations across different
        platforms.

        Key Features:
        - System time retrieval and modification
        - Timezone configuration and management
        - Network Time Protocol (NTP) synchronization
        - Real-Time Clock (RTC) synchronization
        - Administrative privilege checking
        - Cross-platform time operations
        - High-precision time handling

        Security Considerations:
        - Many operations require administrative privileges
        - System time changes affect the entire system
        - NTP operations require network access
        - RTC operations may require hardware access

        Examples:
            >>> from atom.web.time import TimeManager, TimeError
            >>>
            >>> # Get current system time
            >>> tm = TimeManager()
            >>> current_time = tm.get_system_time()
            >>> print(f"Current time: {current_time}")
            >>>
            >>> # Check if we have admin privileges
            >>> if tm.has_admin_privileges():
            ...     # Set system time (requires admin)
            ...     error = tm.set_system_time(2024, 12, 25, 12, 0, 0)
            ...     if error == TimeError.None:
            ...         print("Time set successfully")
            ... else:
            ...     print("Admin privileges required for time modification")
            >>>
            >>> # Get NTP time
            >>> ntp_time = tm.get_ntp_time("pool.ntp.org")
            >>> if ntp_time:
            ...     print(f"NTP time: {ntp_time}")
            ... else:
            ...     print("Failed to get NTP time")
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
        } catch (const std::system_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // TimeError enum binding
    py::enum_<atom::web::TimeError>(m, "TimeError",
                                    R"(Error codes for time operations.

This enumeration defines the possible error conditions that can occur during
time management operations.

Examples:
    >>> from atom.web.time import TimeError
    >>> error = TimeError.None
    >>> if error == TimeError.PermissionDenied:
    ...     print("Need administrator privileges")
)")
        .value("None", atom::web::TimeError::None, "No error occurred")
        .value("InvalidParameter", atom::web::TimeError::InvalidParameter,
               "Invalid parameter provided")
        .value("PermissionDenied", atom::web::TimeError::PermissionDenied,
               "Insufficient permissions for operation")
        .value("NetworkError", atom::web::TimeError::NetworkError,
               "Network-related error occurred")
        .value("SystemError", atom::web::TimeError::SystemError,
               "System-level error occurred")
        .value("TimeoutError", atom::web::TimeError::TimeoutError,
               "Operation timed out")
        .value("NotSupported", atom::web::TimeError::NotSupported,
               "Operation not supported on this platform")
        .export_values();

    // TimeManager class binding
    py::class_<atom::web::TimeManager>(
        m, "TimeManager",
        R"(A class for managing system time and synchronization.

This class provides a high-level interface for time management operations
including system time retrieval/setting, timezone management, RTC
synchronization, and NTP time fetching.

Note:
    Many operations require administrative privileges. Use has_admin_privileges()
    to check before attempting privileged operations.

Examples:
    >>> from atom.web.time import TimeManager
    >>> tm = TimeManager()
    >>> current_time = tm.get_system_time()
)")
        .def(py::init<>(),
             R"(Constructs a TimeManager.

Initializes the time manager with default implementation.

Raises:
    RuntimeError: If initialization fails.
)")

        .def("get_system_time", &atom::web::TimeManager::getSystemTime,
             R"(Gets the current system time.

Returns:
    int: The current system time as a Unix timestamp (seconds since epoch).

Raises:
    OSError: If system call fails.

Examples:
    >>> tm = TimeManager()
    >>> timestamp = tm.get_system_time()
    >>> print(f"Current timestamp: {timestamp}")
)")

        .def("get_system_time_point",
             &atom::web::TimeManager::getSystemTimePoint,
             R"(Gets the current system time with higher precision.

Returns:
    datetime: The current system time as a high-precision time point.

Raises:
    OSError: If system call fails.

Examples:
    >>> tm = TimeManager()
    >>> time_point = tm.get_system_time_point()
    >>> print(f"High precision time: {time_point}")
)")

        .def(
            "set_system_time",
            [](atom::web::TimeManager& self, int year, int month, int day,
               int hour, int minute, int second) {
                auto error_code =
                    self.setSystemTime(year, month, day, hour, minute, second);
                if (!error_code) {
                    return atom::web::TimeError::None;
                }
                // Map std::error_code to TimeError enum
                if (error_code == std::errc::invalid_argument) {
                    return atom::web::TimeError::InvalidParameter;
                } else if (error_code == std::errc::permission_denied) {
                    return atom::web::TimeError::PermissionDenied;
                } else if (error_code == std::errc::not_supported) {
                    return atom::web::TimeError::NotSupported;
                } else {
                    return atom::web::TimeError::SystemError;
                }
            },
            py::arg("year"), py::arg("month"), py::arg("day"), py::arg("hour"),
            py::arg("minute"), py::arg("second"),
            R"(Sets the system time.

Args:
    year: The year to set (1970-2038).
    month: The month to set (1-12).
    day: The day to set (1-31).
    hour: The hour to set (0-23).
    minute: The minute to set (0-59).
    second: The second to set (0-59).

Returns:
    TimeError: Error code indicating success or failure.

Note:
    Requires administrative privileges on most systems.

Examples:
    >>> tm = TimeManager()
    >>> if tm.has_admin_privileges():
    ...     error = tm.set_system_time(2024, 12, 25, 12, 0, 0)
    ...     if error == TimeError.None:
    ...         print("Time set successfully")
    ...     else:
    ...         print(f"Failed to set time: {error}")
)")

        .def(
            "set_system_timezone",
            [](atom::web::TimeManager& self, const std::string& timezone) {
                auto error_code = self.setSystemTimezone(timezone);
                if (!error_code) {
                    return atom::web::TimeError::None;
                }
                // Map std::error_code to TimeError enum
                if (error_code == std::errc::invalid_argument) {
                    return atom::web::TimeError::InvalidParameter;
                } else if (error_code == std::errc::permission_denied) {
                    return atom::web::TimeError::PermissionDenied;
                } else if (error_code == std::errc::not_supported) {
                    return atom::web::TimeError::NotSupported;
                } else {
                    return atom::web::TimeError::SystemError;
                }
            },
            py::arg("timezone"),
            R"(Sets the system timezone.

Args:
    timezone: The timezone to set (e.g., "UTC", "America/New_York").

Returns:
    TimeError: Error code indicating success or failure.

Note:
    Requires administrative privileges on most systems.

Examples:
    >>> tm = TimeManager()
    >>> if tm.has_admin_privileges():
    ...     error = tm.set_system_timezone("UTC")
    ...     if error == TimeError.None:
    ...         print("Timezone set to UTC")
)")

        .def(
            "sync_time_from_rtc",
            [](atom::web::TimeManager& self) {
                auto error_code = self.syncTimeFromRTC();
                if (!error_code) {
                    return atom::web::TimeError::None;
                }
                // Map std::error_code to TimeError enum
                if (error_code == std::errc::invalid_argument) {
                    return atom::web::TimeError::InvalidParameter;
                } else if (error_code == std::errc::permission_denied) {
                    return atom::web::TimeError::PermissionDenied;
                } else if (error_code == std::errc::not_supported) {
                    return atom::web::TimeError::NotSupported;
                } else {
                    return atom::web::TimeError::SystemError;
                }
            },
            R"(Synchronizes the system time from the Real-Time Clock (RTC).

Returns:
    TimeError: Error code indicating success or failure.

Note:
    Requires administrative privileges on most systems.

Examples:
    >>> tm = TimeManager()
    >>> if tm.has_admin_privileges():
    ...     error = tm.sync_time_from_rtc()
    ...     if error == TimeError.None:
    ...         print("Time synchronized from RTC")
)")

        .def(
            "get_ntp_time", &atom::web::TimeManager::getNtpTime,
            py::arg("hostname"), py::arg("timeout") = std::chrono::seconds(5),
            R"(Gets the Network Time Protocol (NTP) time from a specified hostname.

Args:
    hostname: The NTP server hostname (e.g., "pool.ntp.org").
    timeout: Timeout for the NTP request in seconds (default: 5).

Returns:
    Optional[int]: The NTP time as a Unix timestamp, or None on error.

Note:
    Results are cached to improve performance.

Examples:
    >>> tm = TimeManager()
    >>> ntp_time = tm.get_ntp_time("pool.ntp.org")
    >>> if ntp_time:
    ...     print(f"NTP time: {ntp_time}")
    ... else:
    ...     print("Failed to get NTP time")
    >>>
    >>> # With custom timeout
    >>> ntp_time = tm.get_ntp_time("time.google.com", timeout=10)
)")

        .def(
            "has_admin_privileges", &atom::web::TimeManager::hasAdminPrivileges,
            R"(Checks if the current process has administrative/root privileges.

Returns:
    bool: True if the process has admin/root privileges, False otherwise.

Examples:
    >>> tm = TimeManager()
    >>> if tm.has_admin_privileges():
    ...     print("Running with admin privileges")
    ...     # Can perform privileged operations
    ... else:
    ...     print("Running without admin privileges")
    ...     # Limited to read-only operations
)");

    // Convenience functions
    m.def(
        "get_current_timestamp",
        []() {
            atom::web::TimeManager tm;
            return tm.getSystemTime();
        },
        R"(Convenience function to get the current system timestamp.

Returns:
    int: Current Unix timestamp.

Examples:
    >>> from atom.web.time import get_current_timestamp
    >>> timestamp = get_current_timestamp()
    >>> print(f"Current time: {timestamp}")
)");

    m.def(
        "check_ntp_server",
        [](const std::string& hostname, int timeout_seconds = 5) {
            atom::web::TimeManager tm;
            auto ntp_time =
                tm.getNtpTime(hostname, std::chrono::seconds(timeout_seconds));
            return ntp_time.has_value();
        },
        py::arg("hostname"), py::arg("timeout_seconds") = 5,
        R"(Convenience function to check if an NTP server is reachable.

Args:
    hostname: The NTP server hostname to test.
    timeout_seconds: Timeout in seconds (default: 5).

Returns:
    bool: True if the NTP server is reachable, False otherwise.

Examples:
    >>> from atom.web.time import check_ntp_server
    >>> if check_ntp_server("pool.ntp.org"):
    ...     print("NTP server is reachable")
    ... else:
    ...     print("NTP server is not reachable")
)");

    m.def(
        "requires_admin",
        []() {
            atom::web::TimeManager tm;
            return !tm.hasAdminPrivileges();
        },
        R"(Convenience function to check if admin privileges are required.

Returns:
    bool: True if admin privileges are needed for time operations, False otherwise.

Examples:
    >>> from atom.web.time import requires_admin
    >>> if requires_admin():
    ...     print("Please run as administrator for time modification")
)");
}
