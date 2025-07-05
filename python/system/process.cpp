#include "atom/system/process.hpp"
#include "atom/system/process_info.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(process, m) {
    m.doc() = "Process management and monitoring module for the atom package";

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

    // ProcessPriority enum binding - 修正枚举值名称
    py::enum_<atom::system::ProcessPriority>(m, "ProcessPriority",
                                             "Process priority levels")
        .value("IDLE", atom::system::ProcessPriority::IDLE,
               "Idle priority (lowest)")
        .value("BELOW_NORMAL", atom::system::ProcessPriority::LOW,
               "Below normal priority")
        .value("NORMAL", atom::system::ProcessPriority::NORMAL,
               "Normal priority")
        .value("ABOVE_NORMAL", atom::system::ProcessPriority::HIGH,
               "Above normal priority")
        .value("HIGH", atom::system::ProcessPriority::HIGH, "High priority")
        .value("REALTIME", atom::system::ProcessPriority::REALTIME,
               "Realtime priority (highest)")
        .export_values();

    // FileDescriptor struct binding
    py::class_<atom::system::FileDescriptor>(
        m, "FileDescriptor",
        "Information about a file descriptor used by a process")
        .def(py::init<>())
        .def_readwrite("fd", &atom::system::FileDescriptor::fd,
                       "File descriptor or handle number")
        .def_readwrite("path", &atom::system::FileDescriptor::path,
                       "File path (if available)")
        .def_readwrite("type", &atom::system::FileDescriptor::type,
                       "Type of descriptor (file, socket, pipe, etc.)")
        .def_readwrite("mode", &atom::system::FileDescriptor::mode,
                       "Access mode (read, write, etc.)")
        .def("__repr__", [](const atom::system::FileDescriptor& fd) {
            return "<FileDescriptor fd=" + std::to_string(fd.fd) + " path='" +
                   fd.path + "' type='" + fd.type + "'>";
        });

    // NetworkConnection struct binding - 修正 state -> status
    py::class_<atom::system::NetworkConnection>(
        m, "NetworkConnection", "Information about a network connection")
        .def(py::init<>())
        .def_readwrite("protocol", &atom::system::NetworkConnection::protocol,
                       "Protocol (TCP, UDP, etc.)")
        .def_readwrite("local_address",
                       &atom::system::NetworkConnection::localAddress,
                       "Local IP address")
        .def_readwrite("local_port",
                       &atom::system::NetworkConnection::localPort,
                       "Local port")
        .def_readwrite("remote_address",
                       &atom::system::NetworkConnection::remoteAddress,
                       "Remote IP address")
        .def_readwrite("remote_port",
                       &atom::system::NetworkConnection::remotePort,
                       "Remote port")
        .def_readwrite("status", &atom::system::NetworkConnection::status,
                       "Connection status")
        .def("__repr__", [](const atom::system::NetworkConnection& conn) {
            return "<NetworkConnection " + conn.protocol + " " +
                   conn.localAddress + ":" + std::to_string(conn.localPort) +
                   " -> " + conn.remoteAddress + ":" +
                   std::to_string(conn.remotePort) + " " + conn.status + ">";
        });

    // PerformanceHistory struct binding - 修正成员变量名称
    py::class_<atom::system::PerformanceHistory>(
        m, "PerformanceHistory", "Process performance history data")
        .def(py::init<>())
        .def_readwrite("pid", &atom::system::PerformanceHistory::pid,
                       "Process ID")
        .def_readwrite("data_points",
                       &atom::system::PerformanceHistory::dataPoints,
                       "Performance data points over time");

    // 添加PerformanceDataPoint结构体的绑定
    py::class_<atom::system::PerformanceDataPoint>(
        m, "PerformanceDataPoint", "Point-in-time performance data")
        .def(py::init<>())
        .def_readwrite("timestamp",
                       &atom::system::PerformanceDataPoint::timestamp,
                       "Timestamp for this data point")
        .def_readwrite("cpu_usage",
                       &atom::system::PerformanceDataPoint::cpuUsage,
                       "CPU usage percentage")
        .def_readwrite("memory_usage",
                       &atom::system::PerformanceDataPoint::memoryUsage,
                       "Memory usage in bytes")
        .def_readwrite("io_read_bytes",
                       &atom::system::PerformanceDataPoint::ioReadBytes,
                       "IO read bytes")
        .def_readwrite("io_write_bytes",
                       &atom::system::PerformanceDataPoint::ioWriteBytes,
                       "IO write bytes");

// PrivilegesInfo struct binding for Windows
#ifdef _WIN32
    py::class_<atom::system::PrivilegesInfo>(
        m, "PrivilegesInfo", "Windows process privileges information")
        .def(py::init<>())
        .def_readwrite("privileges", &atom::system::PrivilegesInfo::privileges,
                       "List of privilege names")
        // Removed .def_readwrite for "enabled" as it's not in PrivilegesInfo
        // .def_readwrite("enabled", &atom::system::PrivilegesInfo::enabled,
        //                "List of whether each privilege is enabled")
        .def("__repr__", [](const atom::system::PrivilegesInfo& info) {
            std::string repr = "<PrivilegesInfo [";
            for (size_t i = 0; i < info.privileges.size();
                 ++i) {  // Iterate only based on privileges size
                if (i > 0)
                    repr += ", ";
                repr += info.privileges[i];  // Display only the privilege name
                // Removed display of enabled status: (info.enabled[i] ?
                // "enabled" : "disabled")
                if (i >= 2 && info.privileges.size() > 5) {
                    repr += ", ... (" +
                            std::to_string(
                                info.privileges.size() -
                                (i + 1)) +  // Correctly calculate remaining
                            " more)";
                    break;
                }
            }
            repr += "]>";
            return repr;
        });
#endif

    // ProcessResource struct binding - 修正成员变量名称
    py::class_<atom::system::ProcessResource>(
        m, "ProcessResource", "Process resource usage information")
        .def(py::init<>())
        .def_readwrite("cpu_usage", &atom::system::ProcessResource::cpuUsage,
                       "CPU usage percentage")
        .def_readwrite("memory_usage", &atom::system::ProcessResource::memUsage,
                       "Memory usage in bytes")
        .def_readwrite("vm_usage", &atom::system::ProcessResource::vmUsage,
                       "Virtual memory usage in bytes")
        .def_readwrite("io_read", &atom::system::ProcessResource::ioRead,
                       "Total bytes read from storage")
        .def_readwrite("io_write", &atom::system::ProcessResource::ioWrite,
                       "Total bytes written to storage")
        .def_readwrite("thread_count",
                       &atom::system::ProcessResource::threadCount,
                       "Number of threads")
        .def_readwrite("open_files", &atom::system::ProcessResource::openFiles,
                       "Number of open files")
        .def("__repr__", [](const atom::system::ProcessResource& res) {
            return "<ProcessResource cpu_usage=" +
                   std::to_string(res.cpuUsage) +
                   " memory=" + std::to_string(res.memUsage / 1024) + "KB" +
                   " threads=" + std::to_string(res.threadCount) + ">";
        });

    // Process struct binding
    py::class_<atom::system::Process>(m, "Process",
                                      "Information about a process")
        .def(py::init<>())
        .def_readwrite("pid", &atom::system::Process::pid, "Process ID")
        .def_readwrite("ppid", &atom::system::Process::ppid,
                       "Parent process ID")
        .def_readwrite("name", &atom::system::Process::name, "Process name")
        .def_readwrite("path", &atom::system::Process::path,
                       "Full path to executable")
        .def_readwrite("command", &atom::system::Process::command,
                       "Full command line")
        .def_readwrite("status", &atom::system::Process::status,
                       "Process status")
        .def_readwrite("username", &atom::system::Process::username,
                       "Username that owns the process")
        .def_readwrite("start_time", &atom::system::Process::startTime,
                       "Process start time")
        .def_readwrite("resources", &atom::system::Process::resources,
                       "Resource usage information")
        .def_readwrite("priority", &atom::system::Process::priority,
                       "Process priority")
        .def("__repr__", [](const atom::system::Process& proc) {
            return "<Process pid=" + std::to_string(proc.pid) + " name='" +
                   proc.name +
                   "' cpu=" + std::to_string(proc.resources.cpuUsage) +
                   "% mem=" +
                   std::to_string(proc.resources.memUsage / (1024 * 1024)) +
                   "MB>";
        });

    // Process management functions
    m.def("get_all_processes", &atom::system::getAllProcesses,
          R"(Gets information about all processes.

Returns:
    A list of tuples containing process IDs and names.

Examples:
    >>> from atom.system import process
    >>> all_procs = process.get_all_processes()
    >>> for pid, name in all_procs:
    ...     print(f"Process: {name} (PID: {pid}) ")
)");

    m.def("get_process_info_by_pid", &atom::system::getProcessInfoByPid,
          py::arg("pid"),
          R"(Gets information about a process by its PID.

Args:
    pid: The process ID.

Returns:
    A Process object containing information about the process.

Examples:
    >>> from atom.system import process
    >>> proc_info = process.get_process_info_by_pid(1234)
    >>> print(f"Process: {proc_info.name}, CPU: {proc_info.resources.cpu_usage}%")
)");

    m.def("get_self_process_info", &atom::system::getSelfProcessInfo,
          R"(Gets information about the current process.

Returns:
    A Process object containing information about the current process.

Examples:
    >>> from atom.system import process
    >>> my_proc = process.get_self_process_info()
    >>> print(f"Current process: {my_proc.name} (PID: {my_proc.pid}) ")
)");

    m.def("ctermid", &atom::system::ctermid,
          R"(Returns the name of the controlling terminal.

This function returns the name of the controlling terminal associated with
the current process.

Returns:
    The name of the controlling terminal.

Examples:
    >>> from atom.system import process
    >>> term = process.ctermid()
    >>> print(f"Terminal: {term}")
)");

    m.def("is_process_running", &atom::system::isProcessRunning,
          py::arg("process_name"),
          R"(Checks if a process is running by its name.

Args:
    process_name: The name of the process to check.

Returns:
    True if the process is running, otherwise False.

Examples:
    >>> from atom.system import process
    >>> if process.is_process_running("chrome"):
    ...     print("Chrome is running")
    ... else:
    ...     print("Chrome is not running")
)");

    m.def("get_parent_process_id", &atom::system::getParentProcessId,
          py::arg("process_id"),
          R"(Returns the parent process ID of a given process.

Args:
    process_id: The process ID of the target process.

Returns:
    The parent process ID if found, otherwise -1.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> ppid = process.get_parent_process_id(pid)
    >>> print(f"Parent of process {pid} is {ppid}")
)");

    m.def("create_process_as_user", &atom::system::createProcessAsUser,
          py::arg("command"), py::arg("username"), py::arg("domain"),
          py::arg("password"),
          R"(Creates a process as a specified user.

This function creates a new process using the specified user credentials.
It logs in the user, duplicates the user token, and creates a new process
with the specified command. This function is only available on Windows.

Args:
    command: The command to be executed by the new process.
    username: The username of the user account.
    domain: The domain of the user account.
    password: The password of the user account.

Returns:
    True if the process is created successfully, otherwise False.

Examples:
    >>> from atom.system import process
    >>> # Windows example
    >>> success = process.create_process_as_user(
    ...     "notepad.exe", "username", "domain", "password"
    ... )
    >>> if success:
    ...     print("Process created successfully")
)");

    m.def("get_process_id_by_name", &atom::system::getProcessIdByName,
          py::arg("process_name"),
          R"(Gets the process IDs of processes with the specified name.

Args:
    process_name: The name of the process.

Returns:
    A list of process IDs.

Examples:
    >>> from atom.system import process
    >>> chrome_pids = process.get_process_id_by_name("chrome")
    >>> print(f"Found {len(chrome_pids)} Chrome processes")
)");

    m.def("get_process_cpu_usage", &atom::system::getProcessCpuUsage,
          py::arg("pid"),
          R"(Gets the CPU usage percentage of a process.

Args:
    pid: Process ID.

Returns:
    CPU usage percentage, or -1 if process doesn't exist.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> cpu_usage = process.get_process_cpu_usage(pid)
    >>> print(f"Process {pid} CPU usage: {cpu_usage:.2f}%")
)");

    m.def("get_process_memory_usage", &atom::system::getProcessMemoryUsage,
          py::arg("pid"),
          R"(Gets the memory usage of a process in bytes.

Args:
    pid: Process ID.

Returns:
    Memory usage in bytes, or 0 if process doesn't exist.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> mem_bytes = process.get_process_memory_usage(pid)
    >>> print(f"Process {pid} memory usage: {mem_bytes / (1024*1024):.2f} MB")
)");

    m.def("set_process_priority", &atom::system::setProcessPriority,
          py::arg("pid"), py::arg("priority"),
          R"(Sets the priority of a process.

Args:
    pid: Process ID.
    priority: Priority level from ProcessPriority enum.

Returns:
    True if priority was set successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> # Set process to high priority
    >>> success = process.set_process_priority(pid, process.ProcessPriority.HIGH)
    >>> if success:
    ...     print(f"Set process {pid} to high priority")
)");

    m.def("get_process_priority", &atom::system::getProcessPriority,
          py::arg("pid"),
          R"(Gets the priority of a process.

Args:
    pid: Process ID.

Returns:
    ProcessPriority enum value, or None if process doesn't exist.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> priority = process.get_process_priority(pid)
    >>> if priority is not None:
    ...     print(f"Process {pid} priority: {priority}")
)");

    m.def("get_child_processes", &atom::system::getChildProcesses,
          py::arg("pid"),
          R"(Gets the child processes of a parent process.

Args:
    pid: Parent process ID.

Returns:
    List of child process IDs.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> children = process.get_child_processes(pid)
    >>> print(f"Process {pid} has {len(children)} child processes")
)");

    m.def("get_process_start_time", &atom::system::getProcessStartTime,
          py::arg("pid"),
          R"(Gets the start time of a process.

Args:
    pid: Process ID.

Returns:
    Process start time as a datetime object, or None if process doesn't exist.

Examples:
    >>> from atom.system import process
    >>> import datetime
    >>> pid = 1234
    >>> start_time = process.get_process_start_time(pid)
    >>> if start_time is not None:
    ...     now = datetime.datetime.now()
    ...     uptime = now - start_time
    ...     print(f"Process {pid} has been running for {uptime}")
)");

    m.def("get_process_running_time", &atom::system::getProcessRunningTime,
          py::arg("pid"),
          R"(Gets the running time of a process in seconds.

Args:
    pid: Process ID.

Returns:
    Process running time in seconds, or -1 if process doesn't exist.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> uptime_seconds = process.get_process_running_time(pid)
    >>> if uptime_seconds >= 0:
    ...     hours = uptime_seconds // 3600
    ...     minutes = (uptime_seconds % 3600) // 60
    ...     seconds = uptime_seconds % 60
    ...     print(f"Process uptime: {hours}h {minutes}m {seconds}s")
)");

    m.def("monitor_process", &atom::system::monitorProcess, py::arg("pid"),
          py::arg("callback"), py::arg("interval_ms") = 1000,
          R"(Monitors a process and executes a callback when its status changes.

Args:
    pid: Process ID.
    callback: Function to call when process status changes, takes (pid, status_message).
    interval_ms: Monitoring interval in milliseconds (default: 1000).

Returns:
    Monitor ID that can be used to stop monitoring.

Examples:
    >>> from atom.system import process
    >>> def status_changed(pid, status):
    ...     print(f"Process {pid} status changed: {status}")
    ...
    >>> pid = 1234
    >>> monitor_id = process.monitor_process(pid, status_changed, 2000)
    >>> # Later, to stop monitoring:
    >>> # process.stop_monitoring(monitor_id)
)");

    m.def("stop_monitoring", &atom::system::stopMonitoring,
          py::arg("monitor_id"),
          R"(Stops monitoring a process.

Args:
    monitor_id: Monitor ID returned by monitor_process.

Returns:
    True if monitoring was stopped successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> # Assuming we have a monitor_id from a previous call to monitor_process
    >>> monitor_id = 42
    >>> if process.stop_monitoring(monitor_id):
    ...     print("Stopped monitoring process")
)");

    m.def("get_process_command_line", &atom::system::getProcessCommandLine,
          py::arg("pid"),
          R"(Gets the command line arguments of a process.

Args:
    pid: Process ID.

Returns:
    List of command line arguments.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> cmd_args = process.get_process_command_line(pid)
    >>> print(f"Process {pid} command line: {' '.join(cmd_args)}")
)");

    m.def("get_process_environment", &atom::system::getProcessEnvironment,
          py::arg("pid"),
          R"(Gets the environment variables of a process.

Args:
    pid: Process ID.

Returns:
    Dictionary of environment variable names to values.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> env = process.get_process_environment(pid)
    >>> print(f"Process {pid} has {len(env)} environment variables")
    >>> if 'PATH' in env:
    ...     print(f"PATH: {env['PATH']}")
)");

    m.def("get_process_resources", &atom::system::getProcessResources,
          py::arg("pid"),
          R"(Gets detailed resource usage information for a process.

Args:
    pid: Process ID.

Returns:
    ProcessResource object with resource usage details.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> resources = process.get_process_resources(pid)
    >>> print(f"Process {pid} CPU usage: {resources.cpu_usage}%")
    >>> print(f"Memory usage: {resources.memory_usage / (1024*1024):.2f} MB")
)");

#ifdef _WIN32
    m.def("get_windows_privileges", &atom::system::getWindowsPrivileges,
          py::arg("pid"),
          R"(Gets Windows privileges information for a process.

Args:
    pid: Process ID.

Returns:
    PrivilegesInfo object with privilege names and enabled status.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> privileges = process.get_windows_privileges(pid)
    >>> for i, priv in enumerate(privileges.privileges):
    ...     status = "enabled" if privileges.enabled[i] else "disabled"
    ...     print(f"{priv}: {status}")
)");

    m.def("get_process_modules", &atom::system::getProcessModules,
          py::arg("pid"),
          R"(Gets the list of loaded modules (DLLs) for a Windows process.

Args:
    pid: Process ID.

Returns:
    List of module paths.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> modules = process.get_process_modules(pid)
    >>> print(f"Process {pid} has {len(modules)} loaded modules")
    >>> for i, module in enumerate(modules[:5]):
    ...     print(f"{i+1}. {module}")
    >>> if len(modules) > 5:
    ...     print(f"... and {len(modules) - 5} more")
)");
#endif

#ifdef __linux__
    m.def("get_process_capabilities", &atom::system::getProcessCapabilities,
          py::arg("pid"),
          R"(Gets Linux capabilities information for a process.

Args:
    pid: Process ID.

Returns:
    List of capability names.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> caps = process.get_process_capabilities(pid)
    >>> print(f"Process {pid} has {len(caps)} capabilities")
    >>> for cap in caps:
    ...     print(f"- {cap}")
)");
#endif

    m.def("suspend_process", &atom::system::suspendProcess, py::arg("pid"),
          R"(Suspends (pauses) a running process.

Args:
    pid: Process ID.

Returns:
    True if process was suspended successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> if process.suspend_process(pid):
    ...     print(f"Process {pid} suspended")
)");

    m.def("resume_process", &atom::system::resumeProcess, py::arg("pid"),
          R"(Resumes a suspended process.

Args:
    pid: Process ID.

Returns:
    True if process was resumed successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> if process.resume_process(pid):
    ...     print(f"Process {pid} resumed")
)");

    m.def("set_process_affinity", &atom::system::setProcessAffinity,
          py::arg("pid"), py::arg("cpu_indices"),
          R"(Sets CPU affinity (binds process to specific CPU cores).

Args:
    pid: Process ID.
    cpu_indices: List of CPU core indices to bind to.

Returns:
    True if affinity was set successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> # Bind process to CPU cores 0 and 1
    >>> if process.set_process_affinity(pid, [0, 1]):
    ...     print(f"Process {pid} bound to CPU cores 0 and 1")
)");

    m.def("get_process_affinity", &atom::system::getProcessAffinity,
          py::arg("pid"),
          R"(Gets CPU affinity (CPU cores a process is bound to).

Args:
    pid: Process ID.

Returns:
    List of CPU core indices, or empty list on failure.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> cpu_cores = process.get_process_affinity(pid)
    >>> if cpu_cores:
    ...     print(f"Process {pid} is bound to CPU cores: {', '.join(map(str, cpu_cores))}")
)");

    m.def("set_process_memory_limit", &atom::system::setProcessMemoryLimit,
          py::arg("pid"), py::arg("limit_bytes"),
          R"(Sets memory usage limit for a process.

Args:
    pid: Process ID.
    limit_bytes: Maximum memory usage in bytes.

Returns:
    True if limit was set successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> # Limit process to 1GB of memory
    >>> limit_bytes = 1 * 1024 * 1024 * 1024
    >>> if process.set_process_memory_limit(pid, limit_bytes):
    ...     print(f"Process {pid} memory limited to 1GB")
)");

    m.def("get_process_path", &atom::system::getProcessPath, py::arg("pid"),
          R"(Gets the full executable path of a process.

Args:
    pid: Process ID.

Returns:
    Full path to the process executable.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> exe_path = process.get_process_path(pid)
    >>> print(f"Process {pid} executable: {exe_path}")
)");

    m.def("monitor_process_resource", &atom::system::monitorProcessResource,
          py::arg("pid"), py::arg("resource_type"), py::arg("threshold"),
          py::arg("callback"), py::arg("interval_ms") = 1000,
          R"(Monitors a specific resource of a process.

Args:
    pid: Process ID.
    resource_type: Resource type to monitor ("cpu", "memory", "disk", "network").
    threshold: Threshold value (CPU percentage or bytes for memory).
    callback: Function to call when threshold is exceeded, takes (pid, resource_type, value).
    interval_ms: Monitoring interval in milliseconds (default: 1000).

Returns:
    Monitor ID that can be used to stop monitoring.

Examples:
    >>> from atom.system import process
    >>> def threshold_exceeded(pid, resource, value):
    ...     print(f"Process {pid} {resource} usage exceeded: {value}")
    ...
    >>> pid = 1234
    >>> # Monitor CPU usage, alert when > 50%
    >>> monitor_id = process.monitor_process_resource(
    ...     pid, "cpu", 50.0, threshold_exceeded
    ... )
    >>> # Later, to stop monitoring:
    >>> # process.stop_monitoring(monitor_id)
)");

    m.def("get_process_syscalls", &atom::system::getProcessSyscalls,
          py::arg("pid"),
          R"(Gets system call statistics for a process.

Args:
    pid: Process ID.

Returns:
    Dictionary mapping system call names to call counts.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> syscalls = process.get_process_syscalls(pid)
    >>> print(f"Process {pid} made {len(syscalls)} different system calls")
    >>> # Find the most common syscalls
    >>> top_syscalls = sorted(syscalls.items(), key=lambda x: x[1], reverse=True)[:5]
    >>> for syscall, count in top_syscalls:
    ...     print(f"{syscall}: {count} calls")
)");

    m.def("get_process_network_connections",
          &atom::system::getProcessNetworkConnections, py::arg("pid"),
          R"(Gets network connection information for a process.

Args:
    pid: Process ID.

Returns:
    List of NetworkConnection objects.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> connections = process.get_process_network_connections(pid)
    >>> print(f"Process {pid} has {len(connections)} network connections")
    >>> for conn in connections:
    ...     print(f"{conn.protocol}: {conn.local_address}:{conn.local_port} -> "
    ...           f"{conn.remote_address}:{conn.remote_port} ({conn.status}) ")
)");

    m.def("get_process_file_descriptors",
          &atom::system::getProcessFileDescriptors, py::arg("pid"),
          R"(Gets file descriptor information for a process.

Args:
    pid: Process ID.

Returns:
    List of FileDescriptor objects.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> fds = process.get_process_file_descriptors(pid)
    >>> print(f"Process {pid} has {len(fds)} open file descriptors")
    >>> for fd in fds:
    ...     print(f"{fd.fd}: {fd.path} ({fd.type}, {fd.mode}) ")
)");

    m.def("get_process_performance_history",
          &atom::system::getProcessPerformanceHistory, py::arg("pid"),
          py::arg("duration"), py::arg("interval_ms") = 1000,
          R"(Gets performance history data for a process over a time period.

Args:
    pid: Process ID.
    duration: Time duration to collect data for.
    interval_ms: Sampling interval in milliseconds (default: 1000).

Returns:
    PerformanceHistory object with performance data over time.

Examples:
    >>> from atom.system import process
    >>> import datetime
    >>> pid = 1234
    >>> # Collect 1 minute of data
    >>> duration = datetime.timedelta(minutes=1)
    >>> history = process.get_process_performance_history(pid, duration, 500)
    >>> print(f"Collected {len(history.data_points)} data points")
    >>> # Calculate average CPU usage
    >>> avg_cpu = sum(dp.cpu_usage for dp in history.data_points) / len(history.data_points) if history.data_points else 0
    >>> print(f"Average CPU usage: {avg_cpu:.2f}%")
)");

    m.def("set_process_io_priority", &atom::system::setProcessIOPriority,
          py::arg("pid"), py::arg("priority"),
          R"(Sets IO priority for a process.

Args:
    pid: Process ID.
    priority: IO priority (0-7, lower is higher priority).

Returns:
    True if priority was set successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> # Set high IO priority (value 1)
    >>> if process.set_process_io_priority(pid, 1):
    ...     print(f"Set process {pid} to high IO priority")
)");

    m.def("get_process_io_priority", &atom::system::getProcessIOPriority,
          py::arg("pid"),
          R"(Gets IO priority for a process.

Args:
    pid: Process ID.

Returns:
    IO priority value (0-7), or -1 on failure.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> io_priority = process.get_process_io_priority(pid)
    >>> if io_priority >= 0:
    ...     print(f"Process {pid} IO priority: {io_priority}")
)");

    m.def("send_signal_to_process", &atom::system::sendSignalToProcess,
          py::arg("pid"), py::arg("signal"),
          R"(Sends a signal to a process.

Args:
    pid: Process ID.
    signal: Signal number to send.

Returns:
    True if signal was sent successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> import signal
    >>> pid = 1234
    >>> # Send SIGTERM (15)
    >>> if process.send_signal_to_process(pid, signal.SIGTERM):
    ...     print(f"Sent SIGTERM to process {pid}")
)");

    m.def("find_processes", &atom::system::findProcesses, py::arg("predicate"),
          R"(Finds processes that match a specific condition.

Args:
    predicate: Function that takes a Process object and returns True/False.

Returns:
    List of process IDs that match the condition.

Examples:
    >>> from atom.system import process
    >>> # Find all processes using more than 100MB of memory
    >>> def high_memory(proc):
    ...     return proc.resources.mem_usage > 100 * 1024 * 1024
    ...
    >>> high_mem_pids = process.find_processes(high_memory)
    >>> print(f"Found {len(high_mem_pids)} processes using >100MB memory")
)");

    // Add utility functions
    m.def(
        "find_by_name",
        [](const std::string& name_pattern) {
            return atom::system::findProcesses(
                [name_pattern](const atom::system::Process& proc) {
                    // Simple substring match
                    return proc.name.find(name_pattern) != std::string::npos;
                });
        },
        py::arg("name_pattern"),
        R"(Find processes by name pattern (substring match).

Args:
    name_pattern: String pattern to search for in process names.

Returns:
    List of matching process IDs.

Examples:
    >>> from atom.system import process
    >>> chrome_pids = process.find_by_name("chrome")
    >>> print(f"Found {len(chrome_pids)} Chrome-related processes")
)");

    m.def(
        "kill_process",
        [](int pid, bool force = false) {
            int signal = force ? 9 : 15;  // SIGKILL or SIGTERM
            return atom::system::sendSignalToProcess(pid, signal);
        },
        py::arg("pid"), py::arg("force") = false,
        R"(Kill a process.

Args:
    pid: Process ID to kill.
    force: Whether to force kill (SIGKILL) or request termination (SIGTERM).

Returns:
    True if kill signal was sent successfully, False otherwise.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> # Ask process to terminate gracefully
    >>> process.kill_process(pid)
    >>> # Force kill if needed
    >>> process.kill_process(pid, True)
)");

    m.def(
        "list_top_cpu_processes",
        [](int limit = 5) {
            std::vector<atom::system::Process> processes;
            auto all_pids = atom::system::getAllProcesses();

            for (const auto& [pid, name] : all_pids) {
                try {
                    auto proc_info = atom::system::getProcessInfoByPid(pid);
                    processes.push_back(proc_info);
                } catch (...) {
                    // Skip processes we can't access
                }
            }

            // Sort by CPU usage (descending)
            std::sort(processes.begin(), processes.end(),
                      [](const atom::system::Process& a,
                         const atom::system::Process& b) {
                          return a.resources.cpuUsage > b.resources.cpuUsage;
                      });

            // Take top N
            if (limit > 0 && limit < static_cast<int>(processes.size())) {
                processes.resize(limit);
            }

            return processes;
        },
        py::arg("limit") = 5,
        R"(List top CPU-consuming processes.

Args:
    limit: Maximum number of processes to return (default: 5).

Returns:
    List of Process objects sorted by CPU usage (highest first).

Examples:
    >>> from atom.system import process
    >>> top_cpu = process.list_top_cpu_processes(10)
    >>> for i, proc in enumerate(top_cpu):
    ...     print(f"{i+1}. {proc.name} (PID: {proc.pid}): {proc.resources.cpu_usage:.2f}%")
)");

    m.def(
        "list_top_memory_processes",
        [](int limit = 5) {
            std::vector<atom::system::Process> processes;
            auto all_pids = atom::system::getAllProcesses();

            for (const auto& [pid, name] : all_pids) {
                try {
                    auto proc_info = atom::system::getProcessInfoByPid(pid);
                    processes.push_back(proc_info);
                } catch (...) {
                    // Skip processes we can't access
                }
            }

            // Sort by memory usage (descending)
            std::sort(processes.begin(), processes.end(),
                      [](const atom::system::Process& a,
                         const atom::system::Process& b) {
                          return a.resources.memUsage > b.resources.memUsage;
                      });

            // Take top N
            if (limit > 0 && limit < static_cast<int>(processes.size())) {
                processes.resize(limit);
            }

            return processes;
        },
        py::arg("limit") = 5,
        R"(List top memory-consuming processes.

Args:
    limit: Maximum number of processes to return (default: 5).

Returns:
    List of Process objects sorted by memory usage (highest first).

Examples:
    >>> from atom.system import process
    >>> top_mem = process.list_top_memory_processes(10)
    >>> for i, proc in enumerate(top_mem):
    ...     mem_mb = proc.resources.mem_usage / (1024 * 1024)
    ...     print(f"{i+1}. {proc.name} (PID: {proc.pid}): {mem_mb:.2f} MB")
)");

    m.def(
        "get_process_tree",
        [](int pid) {
            std::map<int, std::vector<int>> tree;
            std::vector<int> to_process = {pid};
            std::set<int> processed;

            while (!to_process.empty()) {
                int current_pid = to_process.back();
                to_process.pop_back();

                if (processed.count(current_pid) > 0) {
                    continue;
                }

                processed.insert(current_pid);

                auto children = atom::system::getChildProcesses(current_pid);
                if (!children.empty()) {
                    tree[current_pid] = children;
                    to_process.insert(to_process.end(), children.begin(),
                                      children.end());
                }
            }

            return tree;
        },
        py::arg("pid"),
        R"(Get the process tree starting from a specific process.

Args:
    pid: Root process ID.

Returns:
    Dictionary mapping each process ID to its list of child process IDs.

Examples:
    >>> from atom.system import process
    >>> pid = 1234
    >>> tree = process.get_process_tree(pid)
    >>> def print_tree(pid, tree, indent=0):
    ...     proc = process.get_process_info_by_pid(pid)
    ...     print(" " * indent + f"{proc.name} (PID: {pid}) ")
    ...     for child in tree.get(pid, []):
    ...         print_tree(child, tree, indent + 2)
    ...
    >>> print_tree(pid, tree)
)");

    // Define a helper class for process monitoring context manager
    py::class_<py::object>(m, "ProcessMonitor",
                           "Process monitoring context manager")
        .def(py::init([](int pid,
                         std::function<void(int, const std::string&)> callback,
                         unsigned int interval_ms = 1000) {
                 return py::object();  // Placeholder, actual impl in __enter__
             }),
             py::arg("pid"), py::arg("callback"), py::arg("interval_ms") = 1000,
             "Initialize a process monitor context manager")
        .def("__enter__",
             [](py::object& self, py::object exc_type, py::object exc_value,
                py::object traceback) {
                 // Here we capture self and extract the monitor parameters
                 if (py::hasattr(self, "pid") &&
                     py::hasattr(self, "callback") &&
                     py::hasattr(self, "interval_ms")) {
                     int pid = py::cast<int>(self.attr("pid"));
                     auto callback =
                         py::cast<std::function<void(int, const std::string&)>>(
                             self.attr("callback"));
                     unsigned int interval_ms =
                         py::cast<unsigned int>(self.attr("interval_ms"));

                     int monitor_id = atom::system::monitorProcess(
                         pid, callback, interval_ms);
                     self.attr("monitor_id") = py::int_(monitor_id);
                 }
                 return self;
             })
        .def("__exit__", [](py::object& self, py::object exc_type,
                            py::object exc_value, py::object traceback) {
            if (py::hasattr(self, "monitor_id")) {
                int monitor_id = py::cast<int>(self.attr("monitor_id"));
                atom::system::stopMonitoring(monitor_id);
            }
            return false;  // Don't suppress exceptions
        });

    // Context manager factory function
    m.def(
        "monitor",
        [&m](int pid, std::function<void(int, const std::string&)> callback,
             unsigned int interval_ms = 1000) {
            auto obj = m.attr("ProcessMonitor")(pid, callback, interval_ms);
            obj.attr("pid") = py::int_(pid);
            obj.attr("callback") = py::cast(callback);
            obj.attr("interval_ms") = py::int_(interval_ms);
            return obj;
        },
        py::arg("pid"), py::arg("callback"), py::arg("interval_ms") = 1000,
        R"(Create a context manager for process monitoring.

Args:
    pid: Process ID to monitor.
    callback: Function to call when process status changes.
    interval_ms: Monitoring interval in milliseconds (default: 1000).

Returns:
    Context manager object that starts monitoring on enter and stops on exit.

Examples:
    >>> from atom.system import process
    >>> def on_status_change(pid, status):
    ...     print(f"Process {pid}: {status}")
    ...
    >>> pid = 1234
    >>> with process.monitor(pid, on_status_change):
    ...     # Monitoring is active in this block
    ...     import time
    ...     time.sleep(10)  # Wait for events
    ... # Monitoring stops automatically when leaving the block
)");
}
