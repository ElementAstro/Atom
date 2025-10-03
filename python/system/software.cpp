#include "atom/system/info/software.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(software, m) {
    m.doc() = R"pbdoc(
        Software Information and Management Module
        ------------------------------------------

        This module provides comprehensive software management capabilities including
        checking installation status, retrieving version information, managing
        software processes, and monitoring software usage.

        Examples:
            >>> from atom.system import software
            >>> 
            >>> # Check if software is installed
            >>> if software.check_software_installed("python"):
            ...     print("Python is installed")
            ...     path = software.get_app_path("python")
            ...     version = software.get_app_version(path)
            ...     print(f"Python version: {version}")
            >>> 
            >>> # Launch software
            >>> if software.launch_software("/usr/bin/python3", ["-c", "print('Hello')"]):
            ...     print("Python launched successfully")
            >>> 
            >>> # Monitor software usage
            >>> def usage_callback(info):
            ...     print(f"CPU: {info.get('cpu_usage', 'N/A')}%")
            ...     print(f"Memory: {info.get('memory_usage', 'N/A')} MB")
            >>> 
            >>> monitor_id = software.monitor_software_usage("python", usage_callback)
            >>> # Later: software.stop_monitoring(monitor_id)
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

    // Software installation and information functions
    m.def("check_software_installed", &atom::system::checkSoftwareInstalled,
          py::arg("software_name"),
          R"(Check whether the specified software is installed.

Args:
    software_name: The name of the software to check.

Returns:
    True if the software is installed, False otherwise.

Examples:
    >>> if software.check_software_installed("python"):
    ...     print("Python is installed on this system")
    >>> 
    >>> # Check multiple software packages
    >>> packages = ["git", "node", "docker"]
    >>> for pkg in packages:
    ...     status = "installed" if software.check_software_installed(pkg) else "not found"
    ...     print(f"{pkg}: {status}")

Note:
    The detection method varies by platform:
    - Windows: Checks registry and common installation paths
    - Linux: Checks package managers and PATH
    - macOS: Checks Applications folder and PATH
)");

    m.def("get_app_version", &atom::system::getAppVersion,
          py::arg("app_path"),
          R"(Get the version of the specified application.

Args:
    app_path: The path to the application executable.

Returns:
    The version string of the application.

Raises:
    RuntimeError: If the version cannot be determined.

Examples:
    >>> import pathlib
    >>> python_path = software.get_app_path("python")
    >>> if python_path:
    ...     version = software.get_app_version(python_path)
    ...     print(f"Python version: {version}")
    >>> 
    >>> # Get version of a specific executable
    >>> version = software.get_app_version(pathlib.Path("/usr/bin/git"))
    >>> print(f"Git version: {version}")

Note:
    Version detection methods include:
    - Reading version resources (Windows)
    - Executing with --version flag
    - Parsing executable metadata
    - Reading version files
)");

    m.def("get_app_path", &atom::system::getAppPath,
          py::arg("software_name"),
          R"(Get the path to the specified application.

Args:
    software_name: The name of the software.

Returns:
    The filesystem path to the application.

Raises:
    RuntimeError: If the application cannot be found.

Examples:
    >>> python_path = software.get_app_path("python")
    >>> print(f"Python is located at: {python_path}")
    >>> 
    >>> # Find multiple applications
    >>> apps = ["git", "node", "code"]
    >>> for app in apps:
    ...     try:
    ...         path = software.get_app_path(app)
    ...         print(f"{app}: {path}")
    ...     except RuntimeError:
    ...         print(f"{app}: not found")

Note:
    Search locations include:
    - System PATH directories
    - Common installation directories
    - Application-specific locations
    - Registry entries (Windows)
)");

    m.def("get_app_permissions", &atom::system::getAppPermissions,
          py::arg("app_path"),
          R"(Get the permissions of the specified application.

Args:
    app_path: The path to the application.

Returns:
    List of permission strings describing the application's access rights.

Examples:
    >>> app_path = software.get_app_path("python")
    >>> permissions = software.get_app_permissions(app_path)
    >>> print("Application permissions:")
    >>> for perm in permissions:
    ...     print(f"  - {perm}")

Note:
    Permission information may include:
    - File system access rights
    - Network permissions
    - System resource access
    - Security context information
)");

    m.def("get_process_info", &atom::system::getProcessInfo,
          py::arg("software_name"),
          R"(Get process information for a running software.

Args:
    software_name: The name of the software.

Returns:
    Dictionary containing process information (pid, cpu usage, memory usage, etc).

Raises:
    RuntimeError: If the process is not found or information cannot be retrieved.

Examples:
    >>> try:
    ...     info = software.get_process_info("python")
    ...     print(f"PID: {info['pid']}")
    ...     print(f"CPU Usage: {info['cpu_usage']}%")
    ...     print(f"Memory Usage: {info['memory_usage']} MB")
    ...     print(f"Start Time: {info['start_time']}")
    ... except RuntimeError:
    ...     print("Python process not found")

Note:
    Available information may include:
    - Process ID (pid)
    - CPU usage percentage
    - Memory usage in MB
    - Start time
    - Command line arguments
    - Working directory
)");

    // Software lifecycle management
    m.def("launch_software", &atom::system::launchSoftware,
          py::arg("software_path"), py::arg("args") = std::vector<std::string>{},
          R"(Launch a software application.

Args:
    software_path: The path to the software executable.
    args: Command line arguments to pass to the software. Default is empty list.

Returns:
    True if the software was launched successfully, False otherwise.

Examples:
    >>> # Launch without arguments
    >>> success = software.launch_software("/usr/bin/python3")
    >>> 
    >>> # Launch with arguments
    >>> args = ["-c", "print('Hello from Python')"]
    >>> success = software.launch_software("/usr/bin/python3", args)
    >>> 
    >>> # Launch a GUI application
    >>> success = software.launch_software("/usr/bin/gedit", ["myfile.txt"])
    >>> if success:
    ...     print("Application launched successfully")

Note:
    - The function returns immediately after launching
    - Use get_process_info() to monitor the launched process
    - Some applications may require specific working directories
    - GUI applications may need display environment variables
)");

    m.def("terminate_software", &atom::system::terminateSoftware,
          py::arg("software_name"),
          R"(Terminate a running software application.

Args:
    software_name: The name of the software to terminate.

Returns:
    True if the software was terminated successfully, False otherwise.

Examples:
    >>> # Terminate by name
    >>> if software.terminate_software("notepad"):
    ...     print("Notepad terminated")
    >>> 
    >>> # Graceful shutdown attempt
    >>> if not software.terminate_software("myapp"):
    ...     print("Failed to terminate myapp - may require force kill")

Warning:
    This function attempts graceful termination first, but may force-kill
    the process if necessary. Unsaved data may be lost.

Note:
    - Termination methods vary by platform
    - Some processes may resist termination
    - System processes typically cannot be terminated
    - May require elevated privileges for some applications
)");

    // Software monitoring
    m.def("monitor_software_usage", &atom::system::monitorSoftwareUsage,
          py::arg("software_name"), py::arg("callback"), py::arg("interval_ms") = 1000,
          R"(Monitor software usage with a callback.

Args:
    software_name: The name of the software to monitor.
    callback: Function to call when usage data is available.
              Receives a dictionary with usage information.
    interval_ms: The monitoring interval in milliseconds. Default is 1000ms.

Returns:
    A monitoring ID that can be used to stop monitoring.

Examples:
    >>> def usage_callback(info):
    ...     print(f"CPU: {info.get('cpu_usage', 0):.1f}%")
    ...     print(f"Memory: {info.get('memory_usage', 0):.1f} MB")
    ...     print(f"Threads: {info.get('thread_count', 0)}")
    >>> 
    >>> # Start monitoring Python processes
    >>> monitor_id = software.monitor_software_usage("python", usage_callback, 2000)
    >>> 
    >>> # Monitor for 30 seconds, then stop
    >>> import time
    >>> time.sleep(30)
    >>> software.stop_monitoring(monitor_id)

Note:
    - Monitoring runs in a background thread
    - Callback is called at the specified interval
    - Multiple monitors can run simultaneously
    - Always call stop_monitoring() to clean up resources
)");

    m.def("stop_monitoring", &atom::system::stopMonitoring,
          py::arg("monitor_id"),
          R"(Stop monitoring software usage.

Args:
    monitor_id: The monitoring ID returned by monitor_software_usage.

Returns:
    True if monitoring was stopped successfully, False otherwise.

Examples:
    >>> # Start monitoring
    >>> monitor_id = software.monitor_software_usage("myapp", callback)
    >>> 
    >>> # Stop monitoring after some time
    >>> if software.stop_monitoring(monitor_id):
    ...     print("Monitoring stopped successfully")
    >>> else:
    ...     print("Failed to stop monitoring - ID may be invalid")

Note:
    - Invalid monitor IDs will return False
    - Stopping monitoring cleans up background threads
    - It's safe to call this multiple times with the same ID
)");

    // Software update checking
    m.def("check_software_updates", &atom::system::checkSoftwareUpdates,
          py::arg("software_name"), py::arg("current_version"),
          R"(Check if a software has updates available.

Args:
    software_name: The name of the software.
    current_version: The current version of the software.

Returns:
    String containing the latest version if updates are available,
    empty string otherwise.

Examples:
    >>> current_ver = "3.9.0"
    >>> latest_ver = software.check_software_updates("python", current_ver)
    >>> if latest_ver:
    ...     print(f"Update available: {current_ver} -> {latest_ver}")
    >>> else:
    ...     print("No updates available")
    >>> 
    >>> # Check updates for multiple software
    >>> software_list = [
    ...     ("git", "2.30.0"),
    ...     ("node", "14.15.0"),
    ...     ("docker", "20.10.0")
    ... ]
    >>> for name, version in software_list:
    ...     latest = software.check_software_updates(name, version)
    ...     if latest:
    ...         print(f"{name}: {version} -> {latest}")

Note:
    - Update checking may require internet connectivity
    - Some software may not support automatic update checking
    - Version comparison follows semantic versioning when possible
    - Results may be cached for performance
)");

    // Utility functions
    m.def("get_installed_software_list", []() -> py::list {
        py::list result;
        
        // Common software to check
        std::vector<std::string> common_software = {
            "python", "git", "node", "npm", "docker", "java", "gcc", "clang",
            "cmake", "make", "vim", "emacs", "code", "firefox", "chrome"
        };
        
        for (const auto& software : common_software) {
            if (atom::system::checkSoftwareInstalled(software)) {
                py::dict info;
                info["name"] = software;
                try {
                    auto path = atom::system::getAppPath(software);
                    info["path"] = path.string();
                    try {
                        auto version = atom::system::getAppVersion(path);
                        info["version"] = version;
                    } catch (...) {
                        info["version"] = "unknown";
                    }
                } catch (...) {
                    info["path"] = "unknown";
                    info["version"] = "unknown";
                }
                result.append(info);
            }
        }
        
        return result;
    }, R"(Get a list of commonly installed software.

Returns:
    List of dictionaries containing software information.

Examples:
    >>> installed = software.get_installed_software_list()
    >>> print("Installed software:")
    >>> for app in installed:
    ...     print(f"  {app['name']}: {app['version']} at {app['path']}")

Note:
    This function checks for commonly used development tools and applications.
    It may not include all installed software on the system.
)");

    m.def("get_software_info", [](const std::string& software_name) -> py::dict {
        py::dict info;
        info["name"] = software_name;
        info["installed"] = atom::system::checkSoftwareInstalled(software_name);
        
        if (info["installed"].cast<bool>()) {
            try {
                auto path = atom::system::getAppPath(software_name);
                info["path"] = path.string();
                
                try {
                    auto version = atom::system::getAppVersion(path);
                    info["version"] = version;
                } catch (...) {
                    info["version"] = "unknown";
                }
                
                try {
                    auto permissions = atom::system::getAppPermissions(path);
                    info["permissions"] = permissions;
                } catch (...) {
                    info["permissions"] = py::list();
                }
                
                try {
                    auto process_info = atom::system::getProcessInfo(software_name);
                    info["running"] = true;
                    info["process_info"] = process_info;
                } catch (...) {
                    info["running"] = false;
                }
            } catch (...) {
                info["path"] = "unknown";
                info["version"] = "unknown";
                info["permissions"] = py::list();
                info["running"] = false;
            }
        }
        
        return info;
    }, py::arg("software_name"),
          R"(Get comprehensive information about a software.

Args:
    software_name: The name of the software.

Returns:
    Dictionary containing comprehensive software information.

Examples:
    >>> info = software.get_software_info("python")
    >>> print(f"Name: {info['name']}")
    >>> print(f"Installed: {info['installed']}")
    >>> if info['installed']:
    ...     print(f"Path: {info['path']}")
    ...     print(f"Version: {info['version']}")
    ...     print(f"Running: {info['running']}")
    ...     if info['permissions']:
    ...         print(f"Permissions: {info['permissions']}")
)");
}
