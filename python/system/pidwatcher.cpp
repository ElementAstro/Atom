#include "atom/system/pidwatcher.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(pidwatcher, m) {
    m.doc() = "Process monitoring and management module for the atom package";

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

    // Process Status Enum
    py::enum_<atom::system::ProcessStatus>(m, "ProcessStatus",
                                           "Process status enumeration")
        .value("UNKNOWN", atom::system::ProcessStatus::UNKNOWN,
               "Unknown status")
        .value("RUNNING", atom::system::ProcessStatus::RUNNING,
               "Process is running")
        .value("SLEEPING", atom::system::ProcessStatus::SLEEPING,
               "Process is sleeping")
        .value("WAITING", atom::system::ProcessStatus::WAITING,
               "Process is waiting")
        .value("STOPPED", atom::system::ProcessStatus::STOPPED,
               "Process is stopped")
        .value("ZOMBIE", atom::system::ProcessStatus::ZOMBIE,
               "Process is zombie")
        .value("DEAD", atom::system::ProcessStatus::DEAD, "Process is dead")
        .export_values();

    // Process I/O Stats struct
    py::class_<atom::system::ProcessIOStats>(m, "ProcessIOStats",
                                             "Process I/O statistics structure")
        .def(py::init<>())
        .def_readwrite("read_bytes", &atom::system::ProcessIOStats::read_bytes,
                       "Total bytes read")
        .def_readwrite("write_bytes",
                       &atom::system::ProcessIOStats::write_bytes,
                       "Total bytes written")
        .def_readwrite("read_rate", &atom::system::ProcessIOStats::read_rate,
                       "Current read rate (bytes/sec)")
        .def_readwrite("write_rate", &atom::system::ProcessIOStats::write_rate,
                       "Current write rate (bytes/sec)");

    // Process Information struct
    py::class_<atom::system::ProcessInfo>(m, "ProcessInfo",
                                          "Process information structure")
        .def(py::init<>())
        .def_readwrite("pid", &atom::system::ProcessInfo::pid, "Process ID")
        .def_readwrite("parent_pid", &atom::system::ProcessInfo::parent_pid,
                       "Parent process ID")
        .def_readwrite("name", &atom::system::ProcessInfo::name, "Process name")
        .def_readwrite("command_line", &atom::system::ProcessInfo::command_line,
                       "Full command line")
        .def_readwrite("username", &atom::system::ProcessInfo::username,
                       "Owner username")
        .def_readwrite("status", &atom::system::ProcessInfo::status,
                       "Process status")
        .def_readwrite("running", &atom::system::ProcessInfo::running,
                       "Process running status")
        .def_readwrite("cpu_usage", &atom::system::ProcessInfo::cpu_usage,
                       "CPU usage percentage")
        .def_readwrite("memory_usage", &atom::system::ProcessInfo::memory_usage,
                       "Memory usage in KB")
        .def_readwrite("virtual_memory",
                       &atom::system::ProcessInfo::virtual_memory,
                       "Virtual memory in KB")
        .def_readwrite("shared_memory",
                       &atom::system::ProcessInfo::shared_memory,
                       "Shared memory in KB")
        .def_readwrite("priority", &atom::system::ProcessInfo::priority,
                       "Process priority/nice value")
        .def_readwrite("thread_count", &atom::system::ProcessInfo::thread_count,
                       "Number of threads")
        .def_readwrite("io_stats", &atom::system::ProcessInfo::io_stats,
                       "I/O statistics")
        .def_readwrite("start_time", &atom::system::ProcessInfo::start_time,
                       "Process start time")
        .def_readwrite("uptime", &atom::system::ProcessInfo::uptime,
                       "Process uptime")
        .def_readwrite("child_processes",
                       &atom::system::ProcessInfo::child_processes,
                       "Child process IDs")
        // String representation
        .def("__repr__", [](const atom::system::ProcessInfo& info) {
            return "<ProcessInfo pid=" + std::to_string(info.pid) + " name='" +
                   info.name + "' cpu=" + std::to_string(info.cpu_usage) +
                   "% mem=" + std::to_string(info.memory_usage) + "KB>";
        });

    // Resource Limits struct
    py::class_<atom::system::ResourceLimits>(m, "ResourceLimits",
                                             "Resource limit configuration")
        .def(py::init<>())
        .def_readwrite("max_cpu_percent",
                       &atom::system::ResourceLimits::max_cpu_percent,
                       "Maximum CPU usage percentage")
        .def_readwrite("max_memory_kb",
                       &atom::system::ResourceLimits::max_memory_kb,
                       "Maximum memory usage in KB")
        // Constructor with parameters
        .def(py::init([](double cpu, size_t mem) {
                 atom::system::ResourceLimits limits;
                 limits.max_cpu_percent = cpu;
                 limits.max_memory_kb = mem;
                 return limits;
             }),
             py::arg("max_cpu_percent") = 0.0, py::arg("max_memory_kb") = 0,
             "Create resource limits with specific values");

    // Monitor Configuration struct
    py::class_<atom::system::MonitorConfig>(
        m, "MonitorConfig", "Configuration for process monitoring")
        .def(py::init<>())
        .def_readwrite("update_interval",
                       &atom::system::MonitorConfig::update_interval,
                       "Update interval")
        .def_readwrite("monitor_children",
                       &atom::system::MonitorConfig::monitor_children,
                       "Whether to monitor child processes")
        .def_readwrite("auto_restart",
                       &atom::system::MonitorConfig::auto_restart,
                       "Whether to restart process on exit")
        .def_readwrite("max_restart_attempts",
                       &atom::system::MonitorConfig::max_restart_attempts,
                       "Maximum restart attempts")
        .def_readwrite("resource_limits",
                       &atom::system::MonitorConfig::resource_limits,
                       "Resource limits")
        // Constructor with parameters
        .def(py::init([](int interval_ms, bool monitor_children,
                         bool auto_restart, int max_restart) {
                 atom::system::MonitorConfig config;
                 config.update_interval =
                     std::chrono::milliseconds(interval_ms);
                 config.monitor_children = monitor_children;
                 config.auto_restart = auto_restart;
                 config.max_restart_attempts = max_restart;
                 return config;
             }),
             py::arg("update_interval_ms") = 1000,
             py::arg("monitor_children") = false,
             py::arg("auto_restart") = false,
             py::arg("max_restart_attempts") = 3,
             "Create monitor config with specific values");

    // PidWatcher class
    py::class_<atom::system::PidWatcher>(
        m, "PidWatcher",
        R"(A class for monitoring processes by their PID.

This class allows monitoring of processes by their PID. It provides
functionality to set callbacks on process exit, set a monitor function to
run at intervals, get PID by process name, start monitoring a specific
process, stop monitoring, and switch the target process.

Args:
    config: Optional monitoring configuration.

Examples:
    >>> from atom.system import pidwatcher
    >>> # Create a watcher
    >>> watcher = pidwatcher.PidWatcher()
    >>> # Start monitoring a process by name
    >>> watcher.start("firefox")
    >>> # Get info about the process
    >>> info = watcher.get_process_info(watcher.get_pid_by_name("firefox"))
    >>> print(f"Firefox is using {info.memory_usage} KB of memory")
)")
        .def(py::init<>(), "Constructs a default PidWatcher object.")
        .def(py::init<const atom::system::MonitorConfig&>(), py::arg("config"),
             "Constructs a PidWatcher object with specific configuration.")

        // Callback setters (method chaining)
        .def("set_exit_callback", &atom::system::PidWatcher::setExitCallback,
             py::arg("callback"),
             R"(Sets the callback function to be executed on process exit.

Args:
    callback: The callback function to set, taking a ProcessInfo parameter.

Returns:
    Reference to this object for method chaining.

Examples:
    >>> def on_exit(process_info):
    ...     print(f"Process {process_info.name} (PID: {process_info.pid}) exited.")
    >>> watcher.set_exit_callback(on_exit)
)")
        .def("set_monitor_function",
             &atom::system::PidWatcher::setMonitorFunction, py::arg("callback"),
             py::arg("interval"),
             R"(Sets the monitor function to be executed at specified intervals.

Args:
    callback: The monitor function to set, taking a ProcessInfo parameter.
    interval: The interval at which the monitor function should run.

Returns:
    Reference to this object for method chaining.

Examples:
    >>> def monitor_func(process_info):
    ...     print(f"CPU: {process_info.cpu_usage}%, Memory: {process_info.memory_usage}KB")
    >>> watcher.set_monitor_function(monitor_func, datetime.timedelta(seconds=5))
)")
        .def("set_multi_process_callback",
             &atom::system::PidWatcher::setMultiProcessCallback,
             py::arg("callback"),
             R"(Sets the callback for monitoring multiple processes.

Args:
    callback: The callback function to set, taking a list of ProcessInfo objects.

Returns:
    Reference to this object for method chaining.
)")
        .def("set_error_callback", &atom::system::PidWatcher::setErrorCallback,
             py::arg("callback"),
             R"(Sets the error callback function.

Args:
    callback: The callback function to set, taking an error message and error code.

Returns:
    Reference to this object for method chaining.

Examples:
    >>> def on_error(message, code):
    ...     print(f"Error {code}: {message}")
    >>> watcher.set_error_callback(on_error)
)")
        .def("set_resource_limit_callback",
             &atom::system::PidWatcher::setResourceLimitCallback,
             py::arg("callback"),
             R"(Sets the callback for resource limit violations.

Args:
    callback: The callback function to set, taking ProcessInfo and ResourceLimits parameters.

Returns:
    Reference to this object for method chaining.
)")
        .def("set_process_create_callback",
             &atom::system::PidWatcher::setProcessCreateCallback,
             py::arg("callback"),
             R"(Sets the callback for process creation events.

Args:
    callback: The callback function to set, taking a pid and process name.

Returns:
    Reference to this object for method chaining.
)")
        .def("set_process_filter", &atom::system::PidWatcher::setProcessFilter,
             py::arg("filter"),
             R"(Sets a filter for processes to monitor.

Args:
    filter: Function that returns true for processes to monitor.

Returns:
    Reference to this object for method chaining.

Examples:
    >>> def only_high_memory(process_info):
    ...     return process_info.memory_usage > 100000  # Only monitor high-memory processes
    >>> watcher.set_process_filter(only_high_memory)
)")

        // Process information methods
        .def("get_pid_by_name", &atom::system::PidWatcher::getPidByName,
             py::arg("name"),
             R"(Retrieves the PID of a process by its name.

Args:
    name: The name of the process.

Returns:
    The PID of the process or 0 if not found.

Examples:
    >>> pid = watcher.get_pid_by_name("firefox")
    >>> print(f"Firefox PID: {pid}")
)")
        .def(
            "get_pids_by_name", &atom::system::PidWatcher::getPidsByName,
            py::arg("name"),
            R"(Retrieves multiple PIDs by process name (for multiple instances).

Args:
    name: The name of the process.

Returns:
    Vector of PIDs matching the name.

Examples:
    >>> pids = watcher.get_pids_by_name("chrome")
    >>> print(f"Found {len(pids)} Chrome processes")
)")
        .def(
            "get_process_info",
            [](atom::system::PidWatcher& self, pid_t pid) -> py::object {
                auto info = self.getProcessInfo(pid);
                if (!info.has_value()) {
                    return py::none();
                }
                return py::cast(info.value());
            },
            py::arg("pid"),
            R"(Get information about a process.

Args:
    pid: The process ID.

Returns:
    ProcessInfo object or None if not found.

Examples:
    >>> info = watcher.get_process_info(12345)
    >>> if info:
    ...     print(f"Process {info.name} is using {info.cpu_usage}% CPU")
)")
        .def("get_all_processes", &atom::system::PidWatcher::getAllProcesses,
             R"(Get a list of all currently running processes.

Returns:
    List of ProcessInfo for all running processes.

Examples:
    >>> processes = watcher.get_all_processes()
    >>> print(f"Total running processes: {len(processes)}")
)")
        .def("get_child_processes",
             &atom::system::PidWatcher::getChildProcesses, py::arg("pid"),
             R"(Get child processes of a specific process.

Args:
    pid: The parent process ID.

Returns:
    List of child process IDs.

Examples:
    >>> children = watcher.get_child_processes(12345)
    >>> print(f"Process has {len(children)} child processes")
)")

        // Start/stop monitoring
        .def("start", &atom::system::PidWatcher::start, py::arg("name"),
             py::arg("config") = nullptr,
             R"(Starts monitoring the specified process by name.

Args:
    name: The name of the process to monitor.
    config: Optional specific configuration for this process.

Returns:
    True if monitoring started successfully, false otherwise.

Examples:
    >>> watcher.start("firefox")
)")
        .def("start_by_pid", &atom::system::PidWatcher::startByPid,
             py::arg("pid"), py::arg("config") = nullptr,
             R"(Starts monitoring a process by its PID.

Args:
    pid: The process ID to monitor.
    config: Optional specific configuration for this process.

Returns:
    True if monitoring started successfully, false otherwise.

Examples:
    >>> watcher.start_by_pid(12345)
)")
        .def("start_multiple", &atom::system::PidWatcher::startMultiple,
             py::arg("process_names"), py::arg("config") = nullptr,
             R"(Starts monitoring multiple processes at once.

Args:
    process_names: List of process names to monitor.
    config: Optional specific configuration for these processes.

Returns:
    Number of successfully started monitors.

Examples:
    >>> count = watcher.start_multiple(["firefox", "chrome", "code"])
    >>> print(f"Started monitoring {count} processes")
)")
        .def("stop", &atom::system::PidWatcher::stop,
             R"(Stops monitoring the currently monitored process.

Examples:
    >>> watcher.stop()
)")
        .def("stop_process", &atom::system::PidWatcher::stopProcess,
             py::arg("pid"),
             R"(Stops monitoring a specific process.

Args:
    pid: The process ID to stop monitoring.

Returns:
    True if the process was being monitored and stopped.

Examples:
    >>> watcher.stop_process(12345)
)")
        .def("switch_to_process", &atom::system::PidWatcher::switchToProcess,
             py::arg("name"),
             R"(Switches the target process to monitor.

Args:
    name: The name of the process to switch to.

Returns:
    True if the process was successfully switched, false otherwise.

Examples:
    >>> watcher.switch_to_process("code")
)")
        .def("switch_to_process_by_id",
             &atom::system::PidWatcher::switchToProcessById, py::arg("pid"),
             R"(Switches the target process to monitor by PID.

Args:
    pid: The process ID to switch to.

Returns:
    True if the process was successfully switched, false otherwise.

Examples:
    >>> watcher.switch_to_process_by_id(12345)
)")

        // Status checking methods
        .def("is_active", &atom::system::PidWatcher::isActive,
             R"(Check if the watcher is actively monitoring any processes.

Returns:
    True if at least one process is being monitored.

Examples:
    >>> if watcher.is_active():
    ...     print("Watcher is monitoring processes")
)")
        .def("is_monitoring", &atom::system::PidWatcher::isMonitoring,
             py::arg("pid"),
             R"(Checks if a specific process is being monitored.

Args:
    pid: The process ID to check.

Returns:
    True if the process is being monitored.

Examples:
    >>> if watcher.is_monitoring(12345):
    ...     print("Process 12345 is being monitored")
)")
        .def("is_process_running", &atom::system::PidWatcher::isProcessRunning,
             py::arg("pid"),
             R"(Checks if a specific process is running.

Args:
    pid: The process ID to check.

Returns:
    True if the process is running.

Examples:
    >>> if watcher.is_process_running(12345):
    ...     print("Process 12345 is running")
)")

        // Resource usage methods
        .def("get_process_cpu_usage",
             &atom::system::PidWatcher::getProcessCpuUsage, py::arg("pid"),
             R"(Gets CPU usage of a process.

Args:
    pid: The process ID.

Returns:
    CPU usage as percentage or -1.0 on error.

Examples:
    >>> cpu = watcher.get_process_cpu_usage(12345)
    >>> print(f"CPU usage: {cpu}%")
)")
        .def("get_process_memory_usage",
             &atom::system::PidWatcher::getProcessMemoryUsage, py::arg("pid"),
             R"(Gets memory usage of a process.

Args:
    pid: The process ID.

Returns:
    Memory usage in KB or 0 on error.

Examples:
    >>> mem = watcher.get_process_memory_usage(12345)
    >>> print(f"Memory usage: {mem} KB")
)")
        .def("get_process_thread_count",
             &atom::system::PidWatcher::getProcessThreadCount, py::arg("pid"),
             R"(Gets thread count of a process.

Args:
    pid: The process ID.

Returns:
    Number of threads or 0 on error.

Examples:
    >>> threads = watcher.get_process_thread_count(12345)
    >>> print(f"Thread count: {threads}")
)")
        .def("get_process_io_stats",
             &atom::system::PidWatcher::getProcessIOStats, py::arg("pid"),
             R"(Gets I/O statistics for a process.

Args:
    pid: The process ID.

Returns:
    ProcessIOStats structure with I/O information.

Examples:
    >>> io = watcher.get_process_io_stats(12345)
    >>> print(f"Read: {io.read_bytes} bytes, Write: {io.write_bytes} bytes")
)")
        .def("get_process_status", &atom::system::PidWatcher::getProcessStatus,
             py::arg("pid"),
             R"(Gets the process status.

Args:
    pid: The process ID.

Returns:
    Process status enum.

Examples:
    >>> status = watcher.get_process_status(12345)
    >>> if status == pidwatcher.ProcessStatus.RUNNING:
    ...     print("Process is running")
)")
        .def("get_process_uptime", &atom::system::PidWatcher::getProcessUptime,
             py::arg("pid"),
             R"(Gets the uptime of a process.

Args:
    pid: The process ID.

Returns:
    Process uptime in milliseconds.

Examples:
    >>> uptime = watcher.get_process_uptime(12345)
    >>> print(f"Process uptime: {uptime.total_seconds()} seconds")
)")

        // Process management methods
        .def("launch_process", &atom::system::PidWatcher::launchProcess,
             py::arg("command"), py::arg("args") = std::vector<std::string>{},
             py::arg("auto_monitor") = true,
             R"(Launch a new process.

Args:
    command: The command to execute.
    args: Vector of command arguments.
    auto_monitor: Whether to automatically start monitoring the new process.

Returns:
    PID of the new process or 0 on failure.

Examples:
    >>> pid = watcher.launch_process("firefox", ["--private-window"], True)
    >>> print(f"Launched firefox with PID: {pid}")
)")
        .def("terminate_process", &atom::system::PidWatcher::terminateProcess,
             py::arg("pid"), py::arg("force") = false,
             R"(Terminate a process.

Args:
    pid: The process ID to terminate.
    force: Whether to force termination (SIGKILL vs SIGTERM).

Returns:
    True if termination signal was sent successfully.

Examples:
    >>> watcher.terminate_process(12345)  # Graceful termination
    >>> watcher.terminate_process(12345, True)  # Force kill
)")
        .def("set_resource_limits",
             &atom::system::PidWatcher::setResourceLimits, py::arg("pid"),
             py::arg("limits"),
             R"(Configure process resource limits.

Args:
    pid: The process ID.
    limits: The resource limits to apply.

Returns:
    True if limits were set successfully.

Examples:
    >>> limits = pidwatcher.ResourceLimits()
    >>> limits.max_cpu_percent = 50.0
    >>> limits.max_memory_kb = 1024 * 1024  # 1 GB
    >>> watcher.set_resource_limits(12345, limits)
)")
        .def("set_process_priority",
             &atom::system::PidWatcher::setProcessPriority, py::arg("pid"),
             py::arg("priority"),
             R"(Change process priority.

Args:
    pid: The process ID.
    priority: New priority value (nice).

Returns:
    True if priority was changed successfully.

Examples:
    >>> watcher.set_process_priority(12345, 10)  # Lower priority
    >>> watcher.set_process_priority(12345, -10)  # Higher priority (requires privileges)
)")
        .def("configure_auto_restart",
             &atom::system::PidWatcher::configureAutoRestart, py::arg("pid"),
             py::arg("enable"), py::arg("max_attempts") = 3,
             R"(Configure process auto-restart behavior.

Args:
    pid: The process ID.
    enable: Whether to enable auto-restart.
    max_attempts: Maximum number of restart attempts.

Returns:
    True if configuration was applied successfully.

Examples:
    >>> watcher.configure_auto_restart(12345, True, 5)  # Restart up to 5 times
)")
        .def("restart_process", &atom::system::PidWatcher::restartProcess,
             py::arg("pid"),
             R"(Restart a process.

Args:
    pid: The process ID to restart.

Returns:
    PID of the new process or 0 on failure.

Examples:
    >>> new_pid = watcher.restart_process(12345)
    >>> print(f"Process restarted with new PID: {new_pid}")
)")
        .def("dump_process_info", &atom::system::PidWatcher::dumpProcessInfo,
             py::arg("pid"), py::arg("detailed") = false,
             py::arg("output_file") = "",
             R"(Dump process information to a log or file.

Args:
    pid: The process ID.
    detailed: Whether to include detailed information.
    output_file: Optional file to write to (default: log).

Returns:
    True if dump was successful.

Examples:
    >>> watcher.dump_process_info(12345, True, "process_info.log")
)")
        .def("get_monitoring_stats",
             &atom::system::PidWatcher::getMonitoringStats,
             R"(Get monitoring statistics.

Returns:
    Dictionary of monitoring statistics by process ID.

Examples:
    >>> stats = watcher.get_monitoring_stats()
    >>> for pid, proc_stats in stats.items():
    ...     print(f"PID {pid}: {proc_stats}")
)")
        .def("set_rate_limiting", &atom::system::PidWatcher::setRateLimiting,
             py::arg("max_updates_per_second"),
             R"(Set rate limiting for monitoring to prevent high CPU usage.

Args:
    max_updates_per_second: Maximum update operations per second.

Returns:
    Reference to this object for method chaining.

Examples:
    >>> watcher.set_rate_limiting(20)  # Limit to 20 updates per second
)");

    // Utility functions
    m.def(
        "status_to_string",
        [](atom::system::ProcessStatus status) {
            switch (status) {
                case atom::system::ProcessStatus::UNKNOWN:
                    return "unknown";
                case atom::system::ProcessStatus::RUNNING:
                    return "running";
                case atom::system::ProcessStatus::SLEEPING:
                    return "sleeping";
                case atom::system::ProcessStatus::WAITING:
                    return "waiting";
                case atom::system::ProcessStatus::STOPPED:
                    return "stopped";
                case atom::system::ProcessStatus::ZOMBIE:
                    return "zombie";
                case atom::system::ProcessStatus::DEAD:
                    return "dead";
                default:
                    return "unknown";
            }
        },
        py::arg("status"),
        R"(Convert a ProcessStatus enum to string representation.

Args:
    status: The ProcessStatus enum value

Returns:
    String representation of the status

Examples:
    >>> status = watcher.get_process_status(12345)
    >>> status_str = pidwatcher.status_to_string(status)
    >>> print(f"Process status: {status_str}")
)");

    m.def(
        "find_process_by_name",
        [](const std::string& name) {
            atom::system::PidWatcher watcher;
            return watcher.getPidByName(name);
        },
        py::arg("name"),
        R"(Utility function to find a process ID by name without creating a watcher instance.

Args:
    name: The process name to search for

Returns:
    The process ID or 0 if not found

Examples:
    >>> pid = pidwatcher.find_process_by_name("firefox")
    >>> print(f"Firefox PID: {pid}")
)");

    m.def(
        "list_high_cpu_processes",
        [](double threshold) {
            atom::system::PidWatcher watcher;
            auto all_processes = watcher.getAllProcesses();

            std::vector<atom::system::ProcessInfo> high_cpu_processes;
            for (const auto& proc : all_processes) {
                if (proc.cpu_usage > threshold) {
                    high_cpu_processes.push_back(proc);
                }
            }

            // Sort by CPU usage (highest first)
            std::sort(high_cpu_processes.begin(), high_cpu_processes.end(),
                      [](const atom::system::ProcessInfo& a,
                         const atom::system::ProcessInfo& b) {
                          return a.cpu_usage > b.cpu_usage;
                      });

            return high_cpu_processes;
        },
        py::arg("threshold") = 5.0,
        R"(Find processes using more than the specified CPU percentage.

Args:
    threshold: CPU usage threshold percentage (default: 5.0)

Returns:
    List of ProcessInfo objects for high-CPU processes, sorted by CPU usage

Examples:
    >>> high_cpu = pidwatcher.list_high_cpu_processes(10.0)
    >>> for proc in high_cpu:
    ...     print(f"{proc.name}: {proc.cpu_usage}% CPU")
)");

    m.def(
        "list_high_memory_processes",
        [](size_t threshold_mb) {
            atom::system::PidWatcher watcher;
            auto all_processes = watcher.getAllProcesses();

            std::vector<atom::system::ProcessInfo> high_mem_processes;
            size_t threshold_kb = threshold_mb * 1024;

            for (const auto& proc : all_processes) {
                if (proc.memory_usage > threshold_kb) {
                    high_mem_processes.push_back(proc);
                }
            }

            // Sort by memory usage (highest first)
            std::sort(high_mem_processes.begin(), high_mem_processes.end(),
                      [](const atom::system::ProcessInfo& a,
                         const atom::system::ProcessInfo& b) {
                          return a.memory_usage > b.memory_usage;
                      });

            return high_mem_processes;
        },
        py::arg("threshold_mb") = 100,
        R"(Find processes using more than the specified amount of memory.

Args:
    threshold_mb: Memory usage threshold in megabytes (default: 100)

Returns:
    List of ProcessInfo objects for high-memory processes, sorted by memory usage

Examples:
    >>> high_mem = pidwatcher.list_high_memory_processes(500)
    >>> for proc in high_mem:
    ...     print(f"{proc.name}: {proc.memory_usage/1024:.1f} MB")
)");
}
