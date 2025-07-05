#include "atom/sysinfo/os.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(os, m) {
    m.doc() = "Operating system information module for the atom package";

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

    // OperatingSystemInfo structure binding
    py::class_<OperatingSystemInfo>(m, "OperatingSystemInfo",
                                    R"(Information about the operating system.

This class provides detailed information about the operating system, including
name, version, kernel version, architecture, and more.

Examples:
    >>> from atom.sysinfo import os
    >>> # Get OS information
    >>> os_info = os.get_operating_system_info()
    >>> print(f"OS: {os_info.os_name} {os_info.os_version}")
    >>> print(f"Kernel: {os_info.kernel_version}")
    >>> print(f"Architecture: {os_info.architecture}")
    >>> print(f"Computer name: {os_info.computer_name}")
    >>> print(f"Boot time: {os_info.boot_time}")
)")
        .def(py::init<>(), "Constructs a new OperatingSystemInfo object.")
        .def_readwrite("os_name", &OperatingSystemInfo::osName,
                       "Name of the operating system")
        .def_readwrite("os_version", &OperatingSystemInfo::osVersion,
                       "Version of the operating system")
        .def_readwrite("kernel_version", &OperatingSystemInfo::kernelVersion,
                       "Version of the kernel")
        .def_readwrite("architecture", &OperatingSystemInfo::architecture,
                       "Architecture of the operating system")
        .def_readwrite("compiler", &OperatingSystemInfo::compiler,
                       "Compiler used to compile the operating system")
        .def_readwrite("computer_name", &OperatingSystemInfo::computerName,
                       "Name of the computer")
        .def_readwrite("boot_time", &OperatingSystemInfo::bootTime,
                       "System boot time")
        .def_readwrite("install_date", &OperatingSystemInfo::installDate,
                       "OS installation date")
        .def_readwrite("last_update", &OperatingSystemInfo::lastUpdate,
                       "Last system update time")
        .def_readwrite("time_zone", &OperatingSystemInfo::timeZone,
                       "System timezone")
        .def_readwrite("char_set", &OperatingSystemInfo::charSet,
                       "System character set")
        .def_readwrite("is_server", &OperatingSystemInfo::isServer,
                       "Whether the OS is server version")
        .def_readwrite("installed_updates",
                       &OperatingSystemInfo::installedUpdates,
                       "List of installed updates")
        .def("to_json", &OperatingSystemInfo::toJson,
             "Returns OS information as a JSON string")
        .def("to_detailed_string", &OperatingSystemInfo::toDetailedString,
             "Returns formatted detailed OS information")
        .def("to_json_string", &OperatingSystemInfo::toJsonString,
             "Returns OS information as a formatted JSON string")
        .def("__repr__", [](const OperatingSystemInfo& info) {
            return "<OperatingSystemInfo " + info.osName + " " +
                   info.osVersion + " (" + info.architecture + ")>";
        });

    // Function bindings
    m.def("get_operating_system_info", &getOperatingSystemInfo,
          R"(Retrieves the information about the operating system.

Returns:
    OperatingSystemInfo object containing operating system information

Examples:
    >>> from atom.sysinfo import os
    >>> os_info = os.get_operating_system_info()
    >>> print(f"OS: {os_info.os_name} {os_info.os_version}")
    >>> print(f"Architecture: {os_info.architecture}")
)");

    m.def(
        "is_wsl", &isWsl,
        R"(Checks if the operating system is running in a Windows Subsystem for Linux (WSL) environment.

Returns:
    Boolean indicating whether the OS is running in WSL

Examples:
    >>> from atom.sysinfo import os
    >>> if os.is_wsl():
    ...     print("Running in Windows Subsystem for Linux")
    ... else:
    ...     print("Not running in WSL")
)");

    m.def("get_system_uptime", &getSystemUptime,
          R"(Retrieves the system uptime.

Returns:
    System uptime as a timedelta object

Examples:
    >>> from atom.sysinfo import os
    >>> uptime = os.get_system_uptime()
    >>> print(f"System uptime: {uptime.total_seconds() // 3600} hours, {(uptime.total_seconds() % 3600) // 60} minutes")
)");

    m.def("get_last_boot_time", &getLastBootTime,
          R"(Retrieves the last boot time of the system.

Returns:
    String representing the last boot time

Examples:
    >>> from atom.sysinfo import os
    >>> boot_time = os.get_last_boot_time()
    >>> print(f"Last boot time: {boot_time}")
)");

    m.def("get_system_time_zone", &getSystemTimeZone,
          R"(Retrieves the system timezone.

Returns:
    String representing the system timezone

Examples:
    >>> from atom.sysinfo import os
    >>> timezone = os.get_system_time_zone()
    >>> print(f"System timezone: {timezone}")
)");

    m.def("get_installed_updates", &getInstalledUpdates,
          R"(Retrieves the list of installed updates.

Returns:
    List of strings representing installed updates

Examples:
    >>> from atom.sysinfo import os
    >>> updates = os.get_installed_updates()
    >>> print(f"Number of installed updates: {len(updates)}")
    >>> for update in updates[:5]:  # Show first 5 updates
    ...     print(f"- {update}")
)");

    m.def("check_for_updates", &checkForUpdates,
          R"(Checks for available updates.

Returns:
    List of strings representing available updates

Examples:
    >>> from atom.sysinfo import os
    >>> available_updates = os.check_for_updates()
    >>> if available_updates:
    ...     print(f"{len(available_updates)} updates available:")
    ...     for update in available_updates[:5]:  # Show first 5 updates
    ...         print(f"- {update}")
    ... else:
    ...     print("No updates available")
)");

    m.def("get_system_language", &getSystemLanguage,
          R"(Retrieves the system language.

Returns:
    String representing the system language

Examples:
    >>> from atom.sysinfo import os
    >>> language = os.get_system_language()
    >>> print(f"System language: {language}")
)");

    m.def("get_system_encoding", &getSystemEncoding,
          R"(Retrieves the system encoding.

Returns:
    String representing the system encoding

Examples:
    >>> from atom.sysinfo import os
    >>> encoding = os.get_system_encoding()
    >>> print(f"System encoding: {encoding}")
)");

    m.def("is_server_edition", &isServerEdition,
          R"(Checks if the operating system is a server edition.

Returns:
    Boolean indicating whether the OS is a server edition

Examples:
    >>> from atom.sysinfo import os
    >>> if os.is_server_edition():
    ...     print("Running on a server edition OS")
    ... else:
    ...     print("Running on a client edition OS")
)");

    // Additional utility functions
    m.def(
        "format_uptime",
        [](const std::chrono::seconds& uptime) {
            long total_seconds = uptime.count();
            long days = total_seconds / 86400;
            long hours = (total_seconds % 86400) / 3600;
            long minutes = (total_seconds % 3600) / 60;
            long seconds = total_seconds % 60;

            std::string result;
            if (days > 0) {
                result += std::to_string(days) + " days, ";
            }
            if (hours > 0 || days > 0) {
                result += std::to_string(hours) + " hours, ";
            }
            if (minutes > 0 || hours > 0 || days > 0) {
                result += std::to_string(minutes) + " minutes, ";
            }
            result += std::to_string(seconds) + " seconds";

            return result;
        },
        py::arg("uptime"),
        R"(Format system uptime as a human-readable string.

Args:
    uptime: System uptime as a timedelta object

Returns:
    Formatted string representing uptime

Examples:
    >>> from atom.sysinfo import os
    >>> uptime = os.get_system_uptime()
    >>> formatted = os.format_uptime(uptime)
    >>> print(f"System uptime: {formatted}")
)");

    m.def(
        "get_os_summary",
        []() {
            OperatingSystemInfo info = getOperatingSystemInfo();
            std::chrono::seconds uptime = getSystemUptime();

            py::dict summary;
            summary["name"] = info.osName;
            summary["version"] = info.osVersion;
            summary["kernel"] = info.kernelVersion;
            summary["architecture"] = info.architecture;
            summary["computer_name"] = info.computerName;
            summary["boot_time"] = info.bootTime;
            summary["uptime_seconds"] = uptime.count();
            summary["time_zone"] = info.timeZone;
            summary["is_server"] = info.isServer;
            summary["is_wsl"] = isWsl();
            summary["language"] = getSystemLanguage();
            summary["encoding"] = getSystemEncoding();

            return summary;
        },
        R"(Get a comprehensive OS summary in an easy-to-use dictionary format.

Returns:
    Dictionary containing OS information and system status

Examples:
    >>> from atom.sysinfo import os
    >>> # Get OS summary
    >>> summary = os.get_os_summary()
    >>> print(f"OS: {summary['name']} {summary['version']}")
    >>> print(f"Kernel: {summary['kernel']}")
    >>> print(f"Architecture: {summary['architecture']}")
    >>> print(f"Uptime: {summary['uptime_seconds'] // 3600} hours")
)");

    m.def(
        "is_windows",
        []() {
            OperatingSystemInfo info = getOperatingSystemInfo();
            return info.osName.find("Windows") != std::string::npos;
        },
        R"(Check if the current operating system is Windows.

Returns:
    Boolean indicating whether the current OS is Windows

Examples:
    >>> from atom.sysinfo import os
    >>> if os.is_windows():
    ...     print("Running on Windows")
    ... else:
    ...     print("Not running on Windows")
)");

    m.def(
        "is_linux",
        []() {
            OperatingSystemInfo info = getOperatingSystemInfo();
            return info.osName.find("Linux") != std::string::npos;
        },
        R"(Check if the current operating system is Linux.

Returns:
    Boolean indicating whether the current OS is Linux

Examples:
    >>> from atom.sysinfo import os
    >>> if os.is_linux():
    ...     print("Running on Linux")
    ... else:
    ...     print("Not running on Linux")
)");

    m.def(
        "is_macos",
        []() {
            OperatingSystemInfo info = getOperatingSystemInfo();
            return info.osName.find("macOS") != std::string::npos ||
                   info.osName.find("Mac OS") != std::string::npos;
        },
        R"(Check if the current operating system is macOS.

Returns:
    Boolean indicating whether the current OS is macOS

Examples:
    >>> from atom.sysinfo import os
    >>> if os.is_macos():
    ...     print("Running on macOS")
    ... else:
    ...     print("Not running on macOS")
)");

    // Context manager for OS update checks
    py::class_<py::object>(m, "UpdateCheckContext")
        .def(py::init([]() {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             "Create a context manager for checking OS updates")
        .def("__enter__",
             [](py::object& self) {
                 // Get updates at entry
                 self.attr("updates") = py::cast(checkForUpdates());
                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 return py::bool_(false);  // Don't suppress exceptions
             })
        .def_property_readonly(
            "available_updates",
            [](py::object& self) { return self.attr("updates"); },
            "List of available updates")
        .def_property_readonly(
            "has_updates",
            [](py::object& self) {
                py::list updates = self.attr("updates");
                return !updates.empty();
            },
            "Whether updates are available");

    // Factory function for update check context
    m.def(
        "check_updates", [&m]() { return m.attr("UpdateCheckContext")(); },
        R"(Create a context manager for checking OS updates.

Returns:
    A context manager that provides information about available updates

Examples:
    >>> from atom.sysinfo import os
    >>> # Use as a context manager
    >>> with os.check_updates() as ctx:
    ...     if ctx.has_updates:
    ...         print(f"Found {len(ctx.available_updates)} available updates:")
    ...         for update in ctx.available_updates[:3]:  # Show first 3
    ...             print(f"- {update}")
    ...     else:
    ...         print("No updates available")
)");

    // OS information comparison
    m.def(
        "compare_os_versions",
        [](const std::string& version1, const std::string& version2) {
            // Simple version comparison (this is a basic implementation)
            // In a real implementation, you'd want to handle complex version
            // strings correctly
            auto split_version =
                [](const std::string& version) -> std::vector<int> {
                std::vector<int> parts;
                std::string current_part;

                for (char c : version) {
                    if (c == '.' || c == '-') {
                        if (!current_part.empty()) {
                            try {
                                parts.push_back(std::stoi(current_part));
                            } catch (...) {
                                parts.push_back(0);
                            }
                            current_part.clear();
                        }
                    } else if (std::isdigit(c)) {
                        current_part += c;
                    }
                }

                if (!current_part.empty()) {
                    try {
                        parts.push_back(std::stoi(current_part));
                    } catch (...) {
                        parts.push_back(0);
                    }
                }

                return parts;
            };

            auto v1_parts = split_version(version1);
            auto v2_parts = split_version(version2);

            size_t max_parts = std::max(v1_parts.size(), v2_parts.size());
            v1_parts.resize(max_parts, 0);
            v2_parts.resize(max_parts, 0);

            for (size_t i = 0; i < max_parts; ++i) {
                if (v1_parts[i] < v2_parts[i])
                    return -1;
                if (v1_parts[i] > v2_parts[i])
                    return 1;
            }

            return 0;
        },
        py::arg("version1"), py::arg("version2"),
        R"(Compare two OS version strings.

Args:
    version1: First version string
    version2: Second version string

Returns:
    -1 if version1 < version2, 0 if equal, 1 if version1 > version2

Examples:
    >>> from atom.sysinfo import os
    >>> comparison = os.compare_os_versions("10.0.19042", "10.0.19043")
    >>> if comparison < 0:
    ...     print("First version is older")
    >>> elif comparison > 0:
    ...     print("First version is newer")
    >>> else:
    ...     print("Versions are the same")
)");

    // Get architecture bit width
    m.def(
        "get_architecture_bits",
        []() {
            OperatingSystemInfo info = getOperatingSystemInfo();
            if (info.architecture.find("64") != std::string::npos) {
                return 64;
            } else if (info.architecture.find("32") != std::string::npos) {
                return 32;
            } else {
                return 0;  // Unknown
            }
        },
        R"(Get the bit width of the system architecture.

Returns:
    Integer representing the architecture bit width (64, 32, or 0 if unknown)

Examples:
    >>> from atom.sysinfo import os
    >>> bits = os.get_architecture_bits()
    >>> print(f"Running on a {bits}-bit architecture")
)");
}
