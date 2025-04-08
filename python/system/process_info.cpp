#include "atom/system/process_info.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(process_info, m) {
    m.doc() = "Process information structures for the atom system module";

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

    // ProcessPriority enum binding
    py::enum_<atom::system::ProcessPriority>(m, "ProcessPriority",
                                             "Process priority levels")
        .value("IDLE", atom::system::ProcessPriority::IDLE,
               "Idle priority (lowest)")
        .value("LOW", atom::system::ProcessPriority::LOW, "Low priority")
        .value("NORMAL", atom::system::ProcessPriority::NORMAL,
               "Normal priority")
        .value("HIGH", atom::system::ProcessPriority::HIGH, "High priority")
        .value("REALTIME", atom::system::ProcessPriority::REALTIME,
               "Real-time priority (highest)")
        .export_values();

    // ProcessResource struct binding
    py::class_<atom::system::ProcessResource>(
        m, "ProcessResource",
        R"(Represents resource information used by a process.

This structure contains information about various resources used by a process,
including CPU usage, memory usage, I/O operations, and thread count.

Examples:
    >>> from atom.system import process_info
    >>> # Create a ProcessResource instance
    >>> resource = process_info.ProcessResource()
    >>> resource.cpu_usage = 5.2
    >>> resource.mem_usage = 10485760  # 10 MB
    >>> print(f"CPU: {resource.cpu_usage}%, Memory: {resource.mem_usage / 1024 / 1024} MB")
)")
        .def(py::init<>())
        .def_readwrite("cpu_usage", &atom::system::ProcessResource::cpuUsage,
                       "CPU usage percentage")
        .def_readwrite("mem_usage", &atom::system::ProcessResource::memUsage,
                       "Memory usage in bytes")
        .def_readwrite("vm_usage", &atom::system::ProcessResource::vmUsage,
                       "Virtual memory usage in bytes")
        .def_readwrite("io_read", &atom::system::ProcessResource::ioRead,
                       "I/O read bytes")
        .def_readwrite("io_write", &atom::system::ProcessResource::ioWrite,
                       "I/O write bytes")
        .def_readwrite("thread_count",
                       &atom::system::ProcessResource::threadCount,
                       "Number of threads")
        .def_readwrite("open_files", &atom::system::ProcessResource::openFiles,
                       "Number of open files")
        .def("__repr__", [](const atom::system::ProcessResource& res) {
            return "<ProcessResource cpu_usage=" +
                   std::to_string(res.cpuUsage) +
                   "% mem=" + std::to_string(res.memUsage / (1024 * 1024)) +
                   "MB" + " threads=" + std::to_string(res.threadCount) + ">";
        });

    // PrivilegesInfo struct binding
    py::class_<atom::system::PrivilegesInfo>(
        m, "PrivilegesInfo",
        R"(Contains user privileges information.

This structure contains information about a user's privileges, including
username, group name, list of privileges, and administrator status.

Examples:
    >>> from atom.system import process_info
    >>> # Create a PrivilegesInfo instance
    >>> privs = process_info.PrivilegesInfo()
    >>> privs.username = "admin"
    >>> privs.is_admin = True
    >>> privs.privileges = ["READ", "WRITE", "EXECUTE"]
    >>> print(f"User {privs.username} has admin privileges: {privs.is_admin}")
)")
        .def(py::init<>())
        .def_readwrite("username", &atom::system::PrivilegesInfo::username,
                       "Username")
        .def_readwrite("groupname", &atom::system::PrivilegesInfo::groupname,
                       "Group name")
        .def_readwrite("privileges", &atom::system::PrivilegesInfo::privileges,
                       "List of privileges")
        .def_readwrite("is_admin", &atom::system::PrivilegesInfo::isAdmin,
                       "Whether the user is an administrator")
        .def_readwrite("groups", &atom::system::PrivilegesInfo::groups,
                       "List of groups the user belongs to")
        .def("__repr__", [](const atom::system::PrivilegesInfo& info) {
            return "<PrivilegesInfo username=\"" + info.username +
                   "\" is_admin=" + (info.isAdmin ? "True" : "False") +
                   " groups=" + std::to_string(info.groups.size()) + ">";
        });

    // NetworkConnection struct binding
    py::class_<atom::system::NetworkConnection>(
        m, "NetworkConnection",
        R"(Represents a network connection information for a process.

This structure contains information about network connections established by a process,
including protocol, local and remote addresses, ports, and connection status.

Examples:
    >>> from atom.system import process_info
    >>> # Create a NetworkConnection instance
    >>> conn = process_info.NetworkConnection()
    >>> conn.protocol = "TCP"
    >>> conn.local_address = "127.0.0.1"
    >>> conn.local_port = 8080
    >>> conn.remote_address = "192.168.1.100"
    >>> conn.remote_port = 49152
    >>> conn.status = "ESTABLISHED"
    >>> print(f"{conn.protocol} {conn.local_address}:{conn.local_port} -> {conn.remote_address}:{conn.remote_port}")
)")
        .def(py::init<>())
        .def_readwrite("protocol", &atom::system::NetworkConnection::protocol,
                       "Protocol (TCP/UDP)")
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

    // FileDescriptor struct binding
    py::class_<atom::system::FileDescriptor>(m, "FileDescriptor", 
        R"(Represents a file descriptor or handle used by a process.

This structure contains information about file descriptors opened by a process,
including file descriptor ID, file path, type, and access mode.

Examples:
    >>> from atom.system import process_info
    >>> # Create a FileDescriptor instance
    >>> fd = process_info.FileDescriptor()
    >>> fd.fd = 3
    >>> fd.path = "/etc/hosts"
    >>> fd.type = "regular"
    >>> fd.mode = "r"
    >>> print(f"FD {fd.fd}: {fd.path} ({fd.type}, {fd.mode})")
)")
        .def(py::init<>())
        .def_readwrite("fd", &atom::system::FileDescriptor::fd, 
                      "File descriptor/handle ID")
        .def_readwrite("path", &atom::system::FileDescriptor::path, 
                      "File path")
        .def_readwrite("type", &atom::system::FileDescriptor::type, 
                      "File type (regular, socket, pipe, etc.)")
        .def_readwrite("mode", &atom::system::FileDescriptor::mode, 
                      "Access mode (r, w, rw, etc.)")
        .def("__repr__", [](const atom::system::FileDescriptor& fd) {
        return "<FileDescriptor fd=" + std::to_string(fd.fd) + " path=\"" +
               fd.path + "\" type=\"" + fd.type + "\" mode=\"" + fd.mode +
               "\">";
        });

    // PerformanceDataPoint struct binding
    py::class_<atom::system::PerformanceDataPoint>(
        m, "PerformanceDataPoint",
        R"(Represents performance data at a specific point in time.

This structure contains performance metrics for a process at a specific timestamp,
including CPU usage, memory usage, and I/O operations.

Examples:
    >>> from atom.system import process_info
    >>> import datetime
    >>> # Create a PerformanceDataPoint instance
    >>> data_point = process_info.PerformanceDataPoint()
    >>> data_point.timestamp = datetime.datetime.now()
    >>> data_point.cpu_usage = 4.2
    >>> data_point.memory_usage = 52428800  # 50 MB
    >>> print(f"{data_point.timestamp}: CPU {data_point.cpu_usage}%, Memory {data_point.memory_usage / 1024 / 1024} MB")
)")
        .def(py::init<>())
        .def_readwrite("timestamp",
                       &atom::system::PerformanceDataPoint::timestamp,
                       "Time at which the data was captured")
        .def_readwrite("cpu_usage",
                       &atom::system::PerformanceDataPoint::cpuUsage,
                       "CPU usage percentage")
        .def_readwrite("memory_usage",
                       &atom::system::PerformanceDataPoint::memoryUsage,
                       "Memory usage in bytes")
        .def_readwrite("io_read_bytes",
                       &atom::system::PerformanceDataPoint::ioReadBytes,
                       "I/O read bytes")
        .def_readwrite("io_write_bytes",
                       &atom::system::PerformanceDataPoint::ioWriteBytes,
                       "I/O write bytes")
        .def("__repr__", [](const atom::system::PerformanceDataPoint& dp) {
            auto time_t = std::chrono::system_clock::to_time_t(dp.timestamp);
            std::tm tm;
#ifdef _WIN32
            localtime_s(&tm, &time_t);
#else
            localtime_r(&time_t, &tm);
#endif
            char time_str[20];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

            return "<PerformanceDataPoint timestamp=\"" +
                   std::string(time_str) +
                   "\" cpu_usage=" + std::to_string(dp.cpuUsage) + "% memory=" +
                   std::to_string(dp.memoryUsage / (1024 * 1024)) + "MB>";
        });

    // PerformanceHistory struct binding
    py::class_<atom::system::PerformanceHistory>(
        m, "PerformanceHistory",
        R"(Represents performance history data for a process.

This structure contains a collection of performance data points for a process
over a period of time.

Examples:
    >>> from atom.system import process_info
    >>> # Create a PerformanceHistory instance
    >>> history = process_info.PerformanceHistory()
    >>> history.pid = 1234
    >>> # We would typically add data points over time
    >>> if len(history.data_points) > 0:
    ...     latest = history.data_points[-1]
    ...     print(f"Latest CPU usage: {latest.cpu_usage}%")
)")
        .def(py::init<>())
        .def_readwrite("pid", &atom::system::PerformanceHistory::pid,
                       "Process ID")
        .def_readwrite("data_points",
                       &atom::system::PerformanceHistory::dataPoints,
                       "List of performance data points")
        .def("__repr__",
             [](const atom::system::PerformanceHistory& history) {
                 return "<PerformanceHistory pid=" +
                        std::to_string(history.pid) + " data_points=" +
                        std::to_string(history.dataPoints.size()) + ">";
             })
        .def(
            "add_data_point",
            [](atom::system::PerformanceHistory& self,
               const atom::system::PerformanceDataPoint& point) {
                self.dataPoints.push_back(point);
            },
            py::arg("data_point"),
            R"(Add a performance data point to the history.

Args:
    data_point: The PerformanceDataPoint to add.

Examples:
    >>> from atom.system import process_info
    >>> import datetime
    >>> history = process_info.PerformanceHistory()
    >>> history.pid = 1234
    >>> # Create and add a data point
    >>> point = process_info.PerformanceDataPoint()
    >>> point.timestamp = datetime.datetime.now()
    >>> point.cpu_usage = 2.5
    >>> point.memory_usage = 104857600  # 100 MB
    >>> history.add_data_point(point)
)")
        .def(
            "clear",
            [](atom::system::PerformanceHistory& self) {
                self.dataPoints.clear();
            },
            R"(Clear all data points from the history.

Examples:
    >>> from atom.system import process_info
    >>> history = process_info.PerformanceHistory()
    >>> # ... after adding data points
    >>> history.clear()
    >>> assert len(history.data_points) == 0
)")
        .def(
            "get_average_cpu",
            [](const atom::system::PerformanceHistory& self) -> double {
                if (self.dataPoints.empty())
                    return 0.0;
                double sum = 0.0;
                for (const auto& point : self.dataPoints) {
                    sum += point.cpuUsage;
                }
                return sum / self.dataPoints.size();
            },
            R"(Calculate the average CPU usage across all data points.

Returns:
    Average CPU usage percentage.

Examples:
    >>> from atom.system import process_info
    >>> history = process_info.PerformanceHistory()
    >>> # ... after adding data points
    >>> avg_cpu = history.get_average_cpu()
    >>> print(f"Average CPU usage: {avg_cpu:.2f}%")
)")
        .def(
            "get_average_memory",
            [](const atom::system::PerformanceHistory& self) -> double {
                if (self.dataPoints.empty())
                    return 0.0;
                double sum = 0.0;
                for (const auto& point : self.dataPoints) {
                    sum += point.memoryUsage;
                }
                return sum / self.dataPoints.size();
            },
            R"(Calculate the average memory usage across all data points.

Returns:
    Average memory usage in bytes.

Examples:
    >>> from atom.system import process_info
    >>> history = process_info.PerformanceHistory()
    >>> # ... after adding data points
    >>> avg_mem = history.get_average_memory()
    >>> print(f"Average memory usage: {avg_mem / 1024 / 1024:.2f} MB")
)");

    // Process struct binding
    py::class_<atom::system::Process>(m, "Process", 
        R"(Represents a system process with detailed information.

This structure contains comprehensive information about a system process,
including ID, name, command, path, status, resource usage, etc.

Examples:
    >>> from atom.system import process_info
    >>> # Create a Process instance
    >>> proc = process_info.Process()
    >>> proc.pid = 1234
    >>> proc.name = "example"
    >>> proc.command = "./example --arg1=value"
    >>> print(f"Process {proc.name} (PID: {proc.pid})")
)")
        .def(py::init<>())
        .def_readwrite("pid", &atom::system::Process::pid, 
                      "Process ID")
        .def_readwrite("ppid", &atom::system::Process::ppid, 
                      "Parent process ID")
        .def_readwrite("name", &atom::system::Process::name, 
                      "Process name")
        .def_readwrite("command", &atom::system::Process::command, 
                      "Command used to start the process")
        .def_readwrite("output", &atom::system::Process::output, 
                      "Process output")
        .def_readwrite("path", &atom::system::Process::path, 
                      "Path to the process executable")
        .def_readwrite("status", &atom::system::Process::status, 
                      "Process status")
        .def_readwrite("username", &atom::system::Process::username, 
                      "Username of the process owner")
        .def_readwrite("priority", &atom::system::Process::priority, 
                      "Process priority")
        .def_readwrite("start_time", &atom::system::Process::startTime, 
                      "Process start time")
        .def_readwrite("resources", &atom::system::Process::resources, 
                      "Process resource usage information")
        .def_readwrite("environment", &atom::system::Process::environment, 
                      "Process environment variables")
        .def_readwrite("is_background", &atom::system::Process::isBackground, 
                      "Whether the process is running in the background")
#ifdef _WIN32
        .def_property("handle",
                      [](const atom::system::Process& proc) {
        return reinterpret_cast<intptr_t>(proc.handle);
                      },
                      [](atom::system::Process& proc, intptr_t handle) {
        proc.handle = reinterpret_cast<void*>(handle);
                      },
                      "Handle to the process (Windows only)")
        .def_readwrite("modules", &atom::system::Process::modules, 
                       "List of loaded modules")
#endif
        .def("__repr__", [](const atom::system::Process& proc) {
        return "<Process pid=" + std::to_string(proc.pid) + " name=\"" +
               proc.name + "\" status=\"" + proc.status + "\" username=\"" +
               proc.username + "\">";
        });

    // Helper functions
    m.def(
        "format_process_status",
        [](const std::string& status) {
            std::unordered_map<std::string, std::string> status_descriptions = {
                {"R", "Running"},
                {"S", "Sleeping (interruptible)"},
                {"D", "Sleeping (uninterruptible)"},
                {"Z", "Zombie"},
                {"T", "Stopped (on signal)"},
                {"t", "Tracing stop"},
                {"W", "Paging"},
                {"X", "Dead"},
                {"K", "Wakekill"},
                {"P", "Parked"}};

            auto it = status_descriptions.find(status);
            if (it != status_descriptions.end()) {
                return it->second;
            }
            return status;
        },
        py::arg("status"),
        R"(Format a process status code into a human-readable description.

Args:
    status: Process status code (typically a single letter like "R", "S", etc.)

Returns:
    Human-readable description of the process status.

Examples:
    >>> from atom.system import process_info
    >>> description = process_info.format_process_status("R")
    >>> print(description)  # "Running"
)");

    m.def(
         "format_cpu_usage",
         [](double cpu_usage) {
             std::string result;
             if (cpu_usage < 0.1) {
                 result = "Idle";
             } else if (cpu_usage < 5.0) {
                 result = "Low";
             } else if (cpu_usage < 30.0) {
                 result = "Moderate";
             } else if (cpu_usage < 70.0) {
                 result = "High";
             } else {
                 result = "Very High";
             }

             char buffer[32];
             snprintf(buffer, sizeof(buffer), " (%.1f%%)", cpu_usage);
             result += buffer;

             return result;
         },
         py::arg("cpu_usage"),
         R"(Format CPU usage percentage into a descriptive category.

Args:
    cpu_usage: CPU usage percentage (0-100)

Returns:
    Descriptive category with percentage.

Examples:
    >>> from atom.system import process_info
    >>> description = process_info.format_cpu_usage(45.2)
    >>> print(description)  # "High (45.2%)") ");

        m.def(
            "format_memory_usage",
            [](std::size_t memory_bytes) {
                const char* units[] = {"B", "KB", "MB", "GB", "TB"};
                int unit_index = 0;
                double size = static_cast<double>(memory_bytes);

                while (size >= 1024.0 && unit_index < 4) {
                    size /= 1024.0;
                    unit_index++;
                }

                char buffer[32];
                if (unit_index == 0) {
                    snprintf(buffer, sizeof(buffer), "%zu %s",
                             static_cast<std::size_t>(size), units[unit_index]);
                } else {
                    snprintf(buffer, sizeof(buffer), "%.2f %s", size,
                             units[unit_index]);
                }

                return std::string(buffer);
            },
            py::arg("memory_bytes"),
            R"(Format memory usage in bytes to a human-readable string.

Args:
    memory_bytes: Memory usage in bytes

Returns:
    Human-readable string (e.g., "15.24 MB")

Examples:
    >>> from atom.system import process_info
    >>> # Format 15,728,640 bytes (15 MB)
    >>> formatted = process_info.format_memory_usage(15728640)
    >>> print(formatted)  # "15.00 MB"
)");

    m.def(
        "create_performance_data_point",
        [](double cpu_usage, std::size_t memory_usage, std::size_t io_read,
           std::size_t io_write) {
            atom::system::PerformanceDataPoint point;
            point.timestamp = std::chrono::system_clock::now();
            point.cpuUsage = cpu_usage;
            point.memoryUsage = memory_usage;
            point.ioReadBytes = io_read;
            point.ioWriteBytes = io_write;
            return point;
        },
        py::arg("cpu_usage"), py::arg("memory_usage"), py::arg("io_read") = 0,
        py::arg("io_write") = 0,
        R"(Create a new PerformanceDataPoint with current timestamp.

Args:
    cpu_usage: CPU usage percentage
    memory_usage: Memory usage in bytes
    io_read: I/O read bytes (default: 0)
    io_write: I/O write bytes (default: 0)

Returns:
    New PerformanceDataPoint instance with current timestamp

Examples:
    >>> from atom.system import process_info
    >>> # Create a data point with 5% CPU and 100MB memory usage
    >>> point = process_info.create_performance_data_point(5.0, 104857600)
    >>> print(f"CPU: {point.cpu_usage}%, Memory: {point.memory_usage / 1024 / 1024} MB")
)");

    // Create and return a Process object filled with sample data
    m.def("create_sample_process", []() {
        atom::system::Process proc;
        proc.pid = 12345;
        proc.ppid = 1;
        proc.name = "sample_process";
        proc.command = "./sample_process --arg=value";
        proc.output = "Sample output";
        proc.path = fs::path("/usr/bin/sample_process");
        proc.status = "Running";
        proc.username = "user";
        proc.priority = 0;
        proc.startTime =
            std::chrono::system_clock::now() - std::chrono::hours(1);

        proc.resources.cpuUsage = 5.2;
        proc.resources.memUsage = 104857600;  // 100 MB
        proc.resources.vmUsage = 209715200;   // 200 MB
        proc.resources.ioRead = 1048576;      // 1 MB
        proc.resources.ioWrite = 524288;      // 512 KB
        proc.resources.threadCount = 4;
        proc.resources.openFiles = 12;

        proc.environment["PATH"] = "/usr/bin:/usr/local/bin";
        proc.environment["HOME"] = "/home/user";

        proc.isBackground = false;

        return proc;
    },
    R"(Create a sample Process object with predefined values for testing.

Returns:
    A Process object filled with sample data.

Examples:
    >>> from atom.system import process_info
    >>> # Get a sample process for testing
    >>> sample = process_info.create_sample_process()
    >>> print(f"Sample process: {sample.name} (PID: {sample.pid})")
    >>> print(f"CPU: {sample.resources.cpu_usage}%, Memory: {sample.resources.mem_usage / 1024 / 1024} MB")
)");
}