#include "atom/sysinfo/memory.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(memory, m) {
    m.doc() = "Memory information and monitoring module for the atom package";

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

    // MemorySlot structure binding
    py::class_<MemoryInfo::MemorySlot>(
        m, "MemorySlot",
        R"(Information about a physical memory module/slot.

This class provides details about a specific physical memory module
including capacity, speed, and memory type.

Examples:
    >>> from atom.sysinfo import memory
    >>> # Get information about physical memory modules
    >>> mem_slot = memory.get_physical_memory_info()
    >>> print(f"Memory capacity: {mem_slot.capacity}")
    >>> print(f"Memory clock speed: {mem_slot.clock_speed}")
    >>> print(f"Memory type: {mem_slot.type}")
)")
        .def(py::init<>(), "Constructs a new MemorySlot object.")
        .def(py::init<std::string, std::string, std::string>(),
             py::arg("capacity"), py::arg("clock_speed"), py::arg("type"),
             "Constructs a new MemorySlot with specified parameters.")
        .def_readwrite("capacity", &MemoryInfo::MemorySlot::capacity,
                       "Memory module capacity (e.g., \"8GB\")")
        .def_readwrite("clock_speed", &MemoryInfo::MemorySlot::clockSpeed,
                       "Memory clock speed (e.g., \"3200MHz\")")
        .def_readwrite("type", &MemoryInfo::MemorySlot::type,
                       "Memory type (e.g., \"DDR4\", \"DDR5\")")
        .def("__repr__", [](const MemoryInfo::MemorySlot& slot) {
            return "<MemorySlot capacity='" + slot.capacity +
                   "' clock_speed='" + slot.clockSpeed + "' type='" +
                   slot.type + "'>";
        });

    // MemoryInfo structure binding
    py::class_<MemoryInfo>(m, "MemoryInfo",
                           R"(Comprehensive information about system memory.

Contains detailed information about physical memory slots,
virtual memory, swap memory, and process-specific memory metrics.

Examples:
    >>> from atom.sysinfo import memory
    >>> # Get detailed memory statistics
    >>> mem_info = memory.get_detailed_memory_stats()
    >>> print(f"Memory load: {mem_info.memory_load_percentage:.1f}%")
    >>> print(f"Total physical memory: {mem_info.total_physical_memory / (1024**3):.2f} GB")
    >>> print(f"Available physical memory: {mem_info.available_physical_memory / (1024**3):.2f} GB")
    >>> print(f"Swap used: {mem_info.swap_memory_used / (1024**3):.2f} GB of {mem_info.swap_memory_total / (1024**3):.2f} GB")
)")
        .def(py::init<>(), "Constructs a new MemoryInfo object.")
        .def_readwrite("slots", &MemoryInfo::slots,
                       "Collection of physical memory slots")
        .def_readwrite("virtual_memory_max", &MemoryInfo::virtualMemoryMax,
                       "Maximum virtual memory size in bytes")
        .def_readwrite("virtual_memory_used", &MemoryInfo::virtualMemoryUsed,
                       "Used virtual memory in bytes")
        .def_readwrite("swap_memory_total", &MemoryInfo::swapMemoryTotal,
                       "Total swap memory in bytes")
        .def_readwrite("swap_memory_used", &MemoryInfo::swapMemoryUsed,
                       "Used swap memory in bytes")
        .def_readwrite("memory_load_percentage",
                       &MemoryInfo::memoryLoadPercentage,
                       "Current memory usage percentage")
        .def_readwrite("total_physical_memory",
                       &MemoryInfo::totalPhysicalMemory,
                       "Total physical RAM in bytes")
        .def_readwrite("available_physical_memory",
                       &MemoryInfo::availablePhysicalMemory,
                       "Available physical RAM in bytes")
        .def_readwrite("page_fault_count", &MemoryInfo::pageFaultCount,
                       "Number of page faults")
        .def_readwrite("peak_working_set_size", &MemoryInfo::peakWorkingSetSize,
                       "Peak working set size in bytes")
        .def_readwrite("working_set_size", &MemoryInfo::workingSetSize,
                       "Current working set size in bytes")
        .def_readwrite("quota_peak_paged_pool_usage",
                       &MemoryInfo::quotaPeakPagedPoolUsage,
                       "Peak paged pool usage in bytes")
        .def_readwrite("quota_paged_pool_usage",
                       &MemoryInfo::quotaPagedPoolUsage,
                       "Current paged pool usage in bytes")
        .def_property_readonly(
            "used_physical_memory",
            [](const MemoryInfo& info) {
                return info.totalPhysicalMemory - info.availablePhysicalMemory;
            },
            "Used physical memory in bytes (calculated as total - available)")
        .def_property_readonly(
            "physical_memory_usage_percent",
            [](const MemoryInfo& info) {
                if (info.totalPhysicalMemory == 0)
                    return 0.0;
                return 100.0 *
                       (static_cast<double>(info.totalPhysicalMemory -
                                            info.availablePhysicalMemory) /
                        static_cast<double>(info.totalPhysicalMemory));
            },
            "Physical memory usage as percentage (calculated)")
        .def_property_readonly(
            "swap_memory_usage_percent",
            [](const MemoryInfo& info) {
                if (info.swapMemoryTotal == 0)
                    return 0.0;
                return 100.0 * (static_cast<double>(info.swapMemoryUsed) /
                                static_cast<double>(info.swapMemoryTotal));
            },
            "Swap memory usage as percentage (calculated)")
        .def("__repr__", [](const MemoryInfo& info) {
            return "<MemoryInfo total=" +
                   std::to_string(info.totalPhysicalMemory /
                                  (1024 * 1024 * 1024)) +
                   "GB" + " available=" +
                   std::to_string(info.availablePhysicalMemory /
                                  (1024 * 1024 * 1024)) +
                   "GB" +
                   " usage=" + std::to_string(info.memoryLoadPercentage) + "%>";
        });

    // MemoryPerformance structure binding
    py::class_<MemoryPerformance>(m, "MemoryPerformance",
                                  R"(Memory performance monitoring structure.

Contains metrics for memory read/write speeds, bandwidth usage,
latency, and historical latency data.

Examples:
    >>> from atom.sysinfo import memory
    >>> # Get memory performance metrics
    >>> perf = memory.get_memory_performance()
    >>> print(f"Read speed: {perf.read_speed:.2f} MB/s")
    >>> print(f"Write speed: {perf.write_speed:.2f} MB/s")
    >>> print(f"Memory latency: {perf.latency:.2f} ns")
    >>> print(f"Bandwidth usage: {perf.bandwidth_usage:.2f}%")
)")
        .def(py::init<>(), "Constructs a new MemoryPerformance object.")
        .def_readwrite("read_speed", &MemoryPerformance::readSpeed,
                       "Memory read speed in MB/s")
        .def_readwrite("write_speed", &MemoryPerformance::writeSpeed,
                       "Memory write speed in MB/s")
        .def_readwrite("bandwidth_usage", &MemoryPerformance::bandwidthUsage,
                       "Memory bandwidth usage percentage")
        .def_readwrite("latency", &MemoryPerformance::latency,
                       "Memory latency in nanoseconds")
        .def_readwrite("latency_history", &MemoryPerformance::latencyHistory,
                       "Historical latency data")
        .def_property_readonly(
            "average_latency",
            [](const MemoryPerformance& perf) {
                if (perf.latencyHistory.empty())
                    return 0.0;
                double sum = 0.0;
                for (double val : perf.latencyHistory) {
                    sum += val;
                }
                return sum / perf.latencyHistory.size();
            },
            "Average latency from historical data (calculated)")
        .def("__repr__", [](const MemoryPerformance& perf) {
            return "<MemoryPerformance read=" + std::to_string(perf.readSpeed) +
                   "MB/s" + " write=" + std::to_string(perf.writeSpeed) +
                   "MB/s" + " latency=" + std::to_string(perf.latency) + "ns" +
                   " bandwidth=" + std::to_string(perf.bandwidthUsage) + "%>";
        });

    // Function bindings
    m.def("get_memory_usage", &getMemoryUsage,
          R"(Get the memory usage percentage.

Returns:
    Float value representing memory usage percentage (0-100)

Examples:
    >>> from atom.sysinfo import memory
    >>> usage = memory.get_memory_usage()
    >>> print(f"Memory usage: {usage:.1f}%")
)");

    m.def("get_total_memory_size", &getTotalMemorySize,
          R"(Get the total physical memory size.

Returns:
    Total physical memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> total = memory.get_total_memory_size()
    >>> print(f"Total memory: {total / (1024**3):.2f} GB")
)");

    m.def("get_available_memory_size", &getAvailableMemorySize,
          R"(Get the available physical memory size.

Returns:
    Available physical memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> available = memory.get_available_memory_size()
    >>> print(f"Available memory: {available / (1024**3):.2f} GB")
)");

    m.def("get_physical_memory_info", &getPhysicalMemoryInfo,
          R"(Get information about physical memory modules.

Returns:
    MemorySlot object containing information about the memory modules

Examples:
    >>> from atom.sysinfo import memory
    >>> slot = memory.get_physical_memory_info()
    >>> print(f"RAM type: {slot.type}, Capacity: {slot.capacity}, Speed: {slot.clock_speed}")
)");

    m.def("get_virtual_memory_max", &getVirtualMemoryMax,
          R"(Get the maximum virtual memory size.

Returns:
    Maximum virtual memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> max_vm = memory.get_virtual_memory_max()
    >>> print(f"Maximum virtual memory: {max_vm / (1024**3):.2f} GB")
)");

    m.def("get_virtual_memory_used", &getVirtualMemoryUsed,
          R"(Get the currently used virtual memory.

Returns:
    Used virtual memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> used_vm = memory.get_virtual_memory_used()
    >>> print(f"Used virtual memory: {used_vm / (1024**3):.2f} GB")
)");

    m.def("get_swap_memory_total", &getSwapMemoryTotal,
          R"(Get the total swap/page file size.

Returns:
    Total swap memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> total_swap = memory.get_swap_memory_total()
    >>> print(f"Total swap memory: {total_swap / (1024**3):.2f} GB")
)");

    m.def("get_swap_memory_used", &getSwapMemoryUsed,
          R"(Get the used swap/page file size.

Returns:
    Used swap memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> used_swap = memory.get_swap_memory_used()
    >>> print(f"Used swap memory: {used_swap / (1024**3):.2f} GB")
)");

    m.def("get_committed_memory", &getCommittedMemory,
          R"(Get the committed memory size.

Returns:
    Committed memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> committed = memory.get_committed_memory()
    >>> print(f"Committed memory: {committed / (1024**3):.2f} GB")
)");

    m.def("get_uncommitted_memory", &getUncommittedMemory,
          R"(Get the uncommitted memory size.

Returns:
    Uncommitted memory size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> uncommitted = memory.get_uncommitted_memory()
    >>> print(f"Uncommitted memory: {uncommitted / (1024**3):.2f} GB")
)");

    m.def("get_detailed_memory_stats", &getDetailedMemoryStats,
          R"(Get comprehensive memory statistics.

Returns:
    MemoryInfo structure containing comprehensive memory statistics

Examples:
    >>> from atom.sysinfo import memory
    >>> stats = memory.get_detailed_memory_stats()
    >>> print(f"Memory load: {stats.memory_load_percentage:.1f}%")
    >>> print(f"Total memory: {stats.total_physical_memory / (1024**3):.2f} GB")
    >>> print(f"Available memory: {stats.available_physical_memory / (1024**3):.2f} GB")
)");

    m.def("get_peak_working_set_size", &getPeakWorkingSetSize,
          R"(Get the peak working set size of the current process.

Returns:
    Peak working set size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> peak_wss = memory.get_peak_working_set_size()
    >>> print(f"Peak working set size: {peak_wss / (1024**2):.2f} MB")
)");

    m.def("get_current_working_set_size", &getCurrentWorkingSetSize,
          R"(Get the current working set size of the process.

Returns:
    Current working set size in bytes

Examples:
    >>> from atom.sysinfo import memory
    >>> current_wss = memory.get_current_working_set_size()
    >>> print(f"Current working set size: {current_wss / (1024**2):.2f} MB")
)");

    m.def("get_page_fault_count", &getPageFaultCount,
          R"(Get the page fault count.

Returns:
    Number of page faults

Examples:
    >>> from atom.sysinfo import memory
    >>> faults = memory.get_page_fault_count()
    >>> print(f"Page fault count: {faults}")
)");

    m.def("get_memory_load_percentage", &getMemoryLoadPercentage,
          R"(Get memory load percentage.

Returns:
    Memory load as a percentage (0-100)

Examples:
    >>> from atom.sysinfo import memory
    >>> load = memory.get_memory_load_percentage()
    >>> print(f"Memory load: {load:.1f}%")
)");

    m.def("get_memory_performance", &getMemoryPerformance,
          R"(Get memory performance metrics.

Returns:
    MemoryPerformance structure containing performance metrics

Examples:
    >>> from atom.sysinfo import memory
    >>> perf = memory.get_memory_performance()
    >>> print(f"Read speed: {perf.read_speed:.2f} MB/s")
    >>> print(f"Write speed: {perf.write_speed:.2f} MB/s")
    >>> print(f"Memory latency: {perf.latency:.2f} ns")
)");

    m.def(
        "start_memory_monitoring",
        [](py::function callback) {
            startMemoryMonitoring([callback](const MemoryInfo& info) {
                py::gil_scoped_acquire acquire;
                try {
                    callback(info);
                } catch (py::error_already_set& e) {
                    PyErr_Print();
                }
            });
        },
        py::arg("callback"),
        R"(Start memory monitoring.

Initiates memory monitoring and invokes the provided callback function
with updated memory information.

Args:
    callback: Function to be called with memory information updates.
              The callback will receive a MemoryInfo object as its argument.

Examples:
    >>> from atom.sysinfo import memory
    >>> import time
    >>> 
    >>> # Define a callback function
    >>> def on_memory_update(info):
    ...     print(f"Memory usage: {info.memory_load_percentage:.1f}%")
    ...     print(f"Available: {info.available_physical_memory / (1024**3):.2f} GB")
    ... 
    >>> # Start monitoring
    >>> memory.start_memory_monitoring(on_memory_update)
    >>> 
    >>> # Let it run for a while
    >>> time.sleep(10)
    >>> 
    >>> # Stop monitoring
    >>> memory.stop_memory_monitoring()
)");

    m.def("stop_memory_monitoring", &stopMemoryMonitoring,
          R"(Stop memory monitoring.

Stops the ongoing memory monitoring process.

Examples:
    >>> from atom.sysinfo import memory
    >>> # After starting monitoring with start_memory_monitoring()
    >>> memory.stop_memory_monitoring()
)");

    m.def("get_memory_timeline", &getMemoryTimeline, py::arg("duration"),
          R"(Get memory timeline.

Retrieves a timeline of memory statistics over a specified duration.

Args:
    duration: Duration for which memory statistics are collected

Returns:
    List of MemoryInfo objects representing the memory timeline

Examples:
    >>> from atom.sysinfo import memory
    >>> import datetime
    >>> 
    >>> # Get memory timeline for 1 minute
    >>> timeline = memory.get_memory_timeline(datetime.timedelta(minutes=1))
    >>> print(f"Collected {len(timeline)} memory snapshots")
    >>> 
    >>> # Analyze the data
    >>> for i, snapshot in enumerate(timeline):
    ...     print(f"Snapshot {i}: {snapshot.memory_load_percentage:.1f}% used")
)");

    m.def("detect_memory_leaks", &detectMemoryLeaks,
          R"(Detect memory leaks.

Analyzes the system for potential memory leaks and returns a list of
detected issues.

Returns:
    List of strings describing detected memory leaks

Examples:
    >>> from atom.sysinfo import memory
    >>> # Check for memory leaks
    >>> leaks = memory.detect_memory_leaks()
    >>> if leaks:
    ...     print("Potential memory leaks detected:")
    ...     for leak in leaks:
    ...         print(f"- {leak}")
    ... else:
    ...     print("No memory leaks detected")
)");

    m.def("get_memory_fragmentation", &getMemoryFragmentation,
          R"(Get memory fragmentation percentage.

Calculates the percentage of memory fragmentation in the system.

Returns:
    Memory fragmentation percentage

Examples:
    >>> from atom.sysinfo import memory
    >>> # Check memory fragmentation
    >>> frag = memory.get_memory_fragmentation()
    >>> print(f"Memory fragmentation: {frag:.1f}%")
    >>> if frag > 30:
    ...     print("High memory fragmentation detected!")
)");

    m.def("optimize_memory_usage", &optimizeMemoryUsage,
          R"(Optimize memory usage.

Attempts to optimize memory usage by defragmenting and reallocating resources.

Returns:
    Boolean indicating success or failure of optimization

Examples:
    >>> from atom.sysinfo import memory
    >>> # Try to optimize memory usage
    >>> if memory.optimize_memory_usage():
    ...     print("Memory optimization successful")
    ... else:
    ...     print("Memory optimization failed or not needed")
)");

    m.def("analyze_memory_bottlenecks", &analyzeMemoryBottlenecks,
          R"(Analyze memory bottlenecks.

Identifies potential bottlenecks in memory usage and provides suggestions
for improvement.

Returns:
    List of strings describing memory bottlenecks

Examples:
    >>> from atom.sysinfo import memory
    >>> # Analyze memory bottlenecks
    >>> bottlenecks = memory.analyze_memory_bottlenecks()
    >>> if bottlenecks:
    ...     print("Memory bottlenecks detected:")
    ...     for bottleneck in bottlenecks:
    ...         print(f"- {bottleneck}")
    ... else:
    ...     print("No memory bottlenecks detected")
)");

    // Context manager for memory monitoring
    py::class_<py::object>(m, "MemoryMonitorContext")
        .def(py::init([](py::function callback) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("callback"),
             "Create a context manager for memory monitoring")
        .def("__enter__",
             [](py::object& self, py::function callback) {
                 // Store the callback and start memory monitoring
                 self.attr("callback") = callback;

                 startMemoryMonitoring([callback](const MemoryInfo& info) {
                     py::gil_scoped_acquire acquire;
                     try {
                         callback(info);
                     } catch (py::error_already_set& e) {
                         PyErr_Print();
                     }
                 });

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 stopMemoryMonitoring();
                 return py::bool_(false);  // Don't suppress exceptions
             });

    // Factory function for memory monitor context
    m.def(
        "monitor_memory",
        [&m](py::function callback) {
            return m.attr("MemoryMonitorContext")(callback);
        },
        py::arg("callback"),
        R"(Create a context manager for memory monitoring.

This function returns a context manager that monitors memory usage and calls
the provided callback with memory information updates.

Args:
    callback: Function to call with memory updates
              The callback receives a MemoryInfo object as its argument

Returns:
    A context manager for memory monitoring

Examples:
    >>> from atom.sysinfo import memory
    >>> import time
    >>> 
    >>> # Define a callback function
    >>> def on_memory_update(info):
    ...     print(f"Memory usage: {info.memory_load_percentage:.1f}%")
    ...     print(f"Available: {info.available_physical_memory / (1024**3):.2f} GB")
    ... 
    >>> # Use as a context manager
    >>> with memory.monitor_memory(on_memory_update):
    ...     print("Monitoring memory for 5 seconds...")
    ...     time.sleep(5)
    ... 
    >>> print("Monitoring stopped")
)");

    // Format size function
    m.def(
        "format_size",
        [](unsigned long long size_bytes) {
            const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
            int unit_index = 0;
            double size = static_cast<double>(size_bytes);

            while (size >= 1024 && unit_index < 5) {
                size /= 1024;
                unit_index++;
            }

            char buffer[64];
            if (unit_index == 0) {
                snprintf(buffer, sizeof(buffer), "%.0f %s", size,
                         units[unit_index]);
            } else {
                snprintf(buffer, sizeof(buffer), "%.2f %s", size,
                         units[unit_index]);
            }

            return std::string(buffer);
        },
        py::arg("size_bytes"),
        R"(Format a size in bytes to a human-readable string.

Args:
    size_bytes: Size in bytes

Returns:
    String representation with appropriate unit (B, KB, MB, GB, TB, PB)

Examples:
    >>> from atom.sysinfo import memory
    >>> # Format memory sizes
    >>> total = memory.get_total_memory_size()
    >>> available = memory.get_available_memory_size()
    >>> print(f"Total memory: {memory.format_size(total)}")
    >>> print(f"Available memory: {memory.format_size(available)}")
)");

    // Get memory summary function
    m.def(
        "get_memory_summary",
        []() {
            auto stats = getDetailedMemoryStats();

            py::dict summary;
            summary["total_gb"] =
                static_cast<double>(stats.totalPhysicalMemory) /
                (1024 * 1024 * 1024);
            summary["available_gb"] =
                static_cast<double>(stats.availablePhysicalMemory) /
                (1024 * 1024 * 1024);
            summary["used_gb"] =
                static_cast<double>(stats.totalPhysicalMemory -
                                    stats.availablePhysicalMemory) /
                (1024 * 1024 * 1024);
            summary["usage_percent"] = stats.memoryLoadPercentage;
            summary["swap_total_gb"] =
                static_cast<double>(stats.swapMemoryTotal) /
                (1024 * 1024 * 1024);
            summary["swap_used_gb"] =
                static_cast<double>(stats.swapMemoryUsed) /
                (1024 * 1024 * 1024);
            summary["swap_usage_percent"] =
                (stats.swapMemoryTotal > 0)
                    ? (100.0 * stats.swapMemoryUsed / stats.swapMemoryTotal)
                    : 0.0;
            summary["page_faults"] = stats.pageFaultCount;

            return summary;
        },
        R"(Get a summary of memory information in an easy-to-use format.

Returns:
    Dictionary containing memory information with pre-calculated values in GB

Examples:
    >>> from atom.sysinfo import memory
    >>> # Get memory summary
    >>> summary = memory.get_memory_summary()
    >>> print(f"RAM: {summary['used_gb']:.1f} GB used of {summary['total_gb']:.1f} GB ({summary['usage_percent']:.1f}%) ")
    >>> print(f"Swap: {summary['swap_used_gb']:.1f} GB used of {summary['swap_total_gb']:.1f} GB ({summary['swap_usage_percent']:.1f}%) ")
)");

    // Check if memory is low
    m.def(
        "is_memory_low",
        [](float threshold_percent) {
            float usage = getMemoryUsage();
            return usage > (100.0f - threshold_percent);
        },
        py::arg("threshold_percent") = 10.0f,
        R"(Check if available memory is below a certain threshold.

Args:
    threshold_percent: Available memory threshold percentage (default: 10.0)

Returns:
    Boolean indicating whether available memory is below the threshold

Examples:
    >>> from atom.sysinfo import memory
    >>> # Check if less than 15% memory is available
    >>> if memory.is_memory_low(15.0):
    ...     print("Warning: System is running low on memory!")
)");

    // Get memory usage history
    m.def(
        "get_memory_usage_history",
        [](int samples, std::chrono::seconds interval) {
            std::vector<float> history;
            history.reserve(samples);

            for (int i = 0; i < samples; i++) {
                history.push_back(getMemoryUsage());
                if (i < samples - 1) {
                    std::this_thread::sleep_for(interval);
                }
            }

            return history;
        },
        py::arg("samples") = 10, py::arg("interval") = std::chrono::seconds(1),
        R"(Collect memory usage data over time.

Args:
    samples: Number of samples to collect (default: 10)
    interval: Time between samples (default: 1 second)

Returns:
    List of memory usage percentages

Examples:
    >>> from atom.sysinfo import memory
    >>> import time
    >>> # Collect 5 samples of memory usage, 2 seconds apart
    >>> history = memory.get_memory_usage_history(5, time.timedelta(seconds=2))
    >>> print("Memory usage history (%):")
    >>> for i, usage in enumerate(history):
    ...     print(f"Sample {i+1}: {usage:.1f}%")
)");
}