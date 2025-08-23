#include "atom/system/priority.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(priority, m) {
    m.doc() =
        "Process and thread priority management module for the atom package";

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

    // Enum bindings for PriorityLevel
    py::enum_<atom::system::PriorityManager::PriorityLevel>(
        m, "PriorityLevel",
        R"(Priority levels for processes and threads.

This enum defines various priority levels that can be assigned to processes and threads.
Higher priority levels generally result in more CPU time being allocated to the process or thread.

Examples:
    >>> from atom.system import priority
    >>> # Get the highest priority level
    >>> high_priority = priority.PriorityLevel.HIGHEST
    >>> # Set process priority to highest
    >>> priority.set_process_priority(high_priority)
)")
        .value("LOWEST", atom::system::PriorityManager::PriorityLevel::LOWEST,
               "Lowest priority level")
        .value("BELOW_NORMAL",
               atom::system::PriorityManager::PriorityLevel::BELOW_NORMAL,
               "Below normal priority level")
        .value("NORMAL", atom::system::PriorityManager::PriorityLevel::NORMAL,
               "Normal priority level")
        .value("ABOVE_NORMAL",
               atom::system::PriorityManager::PriorityLevel::ABOVE_NORMAL,
               "Above normal priority level")
        .value("HIGHEST", atom::system::PriorityManager::PriorityLevel::HIGHEST,
               "Highest priority level")
        .value("REALTIME",
               atom::system::PriorityManager::PriorityLevel::REALTIME,
               "Realtime priority level (use with caution)")
        .export_values();

    // Enum bindings for SchedulingPolicy
    py::enum_<atom::system::PriorityManager::SchedulingPolicy>(
        m, "SchedulingPolicy",
        R"(Scheduling policies for threads.

This enum defines various scheduling policies that can be used for thread scheduling.
Different policies provide different behaviors for thread execution.

Examples:
    >>> from atom.system import priority
    >>> # Get FIFO scheduling policy
    >>> fifo_policy = priority.SchedulingPolicy.FIFO
    >>> # Set thread scheduling policy to FIFO
    >>> priority.set_thread_scheduling_policy(fifo_policy)
)")
        .value("NORMAL",
               atom::system::PriorityManager::SchedulingPolicy::NORMAL,
               "Normal scheduling policy")
        .value("FIFO", atom::system::PriorityManager::SchedulingPolicy::FIFO,
               "First In First Out scheduling policy")
        .value("ROUND_ROBIN",
               atom::system::PriorityManager::SchedulingPolicy::ROUND_ROBIN,
               "Round Robin scheduling policy")
        .export_values();

    // Process priority functions
    m.def("set_process_priority",
          &atom::system::PriorityManager::setProcessPriority, py::arg("level"),
          py::arg("pid") = 0,
          R"(Sets the priority of a process.

Args:
    level: The priority level to set.
    pid: The process ID. Defaults to 0, which means the current process.

Examples:
    >>> from atom.system import priority
    >>> # Set current process to above normal priority
    >>> priority.set_process_priority(priority.PriorityLevel.ABOVE_NORMAL)
    >>> # Set a specific process to high priority
    >>> priority.set_process_priority(priority.PriorityLevel.HIGHEST, 1234)
)");

    m.def("get_process_priority",
          &atom::system::PriorityManager::getProcessPriority,
          py::arg("pid") = 0,
          R"(Gets the priority of a process.

Args:
    pid: The process ID. Defaults to 0, which means the current process.

Returns:
    The current priority level of the process.

Examples:
    >>> from atom.system import priority
    >>> # Get current process priority
    >>> current_priority = priority.get_process_priority()
    >>> print(f"Current process priority: {current_priority}")
    >>> # Get priority of a specific process
    >>> other_priority = priority.get_process_priority(1234)
    >>> print(f"Process 1234 priority: {other_priority}")
)");

    // Thread priority functions
    m.def("set_thread_priority",
          &atom::system::PriorityManager::setThreadPriority, py::arg("level"),
          py::arg("thread") = 0,
          R"(Sets the priority of a thread.

Args:
    level: The priority level to set.
    thread: The native handle of the thread. Defaults to 0, which means the current thread.

Examples:
    >>> from atom.system import priority
    >>> import threading
    >>> # Set current thread to high priority
    >>> priority.set_thread_priority(priority.PriorityLevel.HIGHEST)
    >>>
    >>> # Create thread and set its priority (using native handle)
    >>> def worker():
    ...     # Get native handle and set priority (platform-specific code)
    ...     thread_handle = threading.get_native_id()  # This is simplified
    ...     priority.set_thread_priority(priority.PriorityLevel.ABOVE_NORMAL, thread_handle)
    ...     # Thread work...
    ...
    >>> t = threading.Thread(target=worker)
    >>> t.start()
)");

    m.def("get_thread_priority",
          &atom::system::PriorityManager::getThreadPriority,
          py::arg("thread") = 0,
          R"(Gets the priority of a thread.

Args:
    thread: The native handle of the thread. Defaults to 0, which means the current thread.

Returns:
    The current priority level of the thread.

Examples:
    >>> from atom.system import priority
    >>> # Get current thread priority
    >>> current_priority = priority.get_thread_priority()
    >>> print(f"Current thread priority: {current_priority}")
)");

    // Thread scheduling policy
    m.def("set_thread_scheduling_policy",
          &atom::system::PriorityManager::setThreadSchedulingPolicy,
          py::arg("policy"), py::arg("thread") = 0,
          R"(Sets the scheduling policy of a thread.

Args:
    policy: The scheduling policy to set.
    thread: The native handle of the thread. Defaults to 0, which means the current thread.

Examples:
    >>> from atom.system import priority
    >>> # Set current thread to FIFO scheduling
    >>> priority.set_thread_scheduling_policy(priority.SchedulingPolicy.FIFO)
)");

    // Process affinity functions
    m.def("set_process_affinity",
          &atom::system::PriorityManager::setProcessAffinity, py::arg("cpus"),
          py::arg("pid") = 0,
          R"(Sets the CPU affinity of a process.

Args:
    cpus: A list of CPU indices to set the affinity to.
    pid: The process ID. Defaults to 0, which means the current process.

Examples:
    >>> from atom.system import priority
    >>> # Pin current process to CPUs 0 and 1
    >>> priority.set_process_affinity([0, 1])
    >>> # Pin another process to CPU 2
    >>> priority.set_process_affinity([2], 1234)
)");

    m.def("get_process_affinity",
          &atom::system::PriorityManager::getProcessAffinity,
          py::arg("pid") = 0,
          R"(Gets the CPU affinity of a process.

Args:
    pid: The process ID. Defaults to 0, which means the current process.

Returns:
    A list of CPU indices the process is affinitized to.

Examples:
    >>> from atom.system import priority
    >>> # Get CPU affinity of current process
    >>> cpus = priority.get_process_affinity()
    >>> print(f"Current process is pinned to CPUs: {cpus}")
)");

    // Priority monitoring
    m.def("start_priority_monitor",
          &atom::system::PriorityManager::startPriorityMonitor, py::arg("pid"),
          py::arg("callback"), py::arg("interval") = std::chrono::seconds(1),
          R"(Starts monitoring the priority of a process.

Args:
    pid: The process ID to monitor.
    callback: The callback function to call when the priority changes.
    interval: The interval at which to check the priority. Defaults to 1 second.

Examples:
    >>> from atom.system import priority
    >>> import time
    >>>
    >>> # Callback function for priority changes
    >>> def on_priority_change(level):
    ...     print(f"Process priority changed to: {level}")
    ...
    >>> # Monitor process 1234 for priority changes
    >>> priority.start_priority_monitor(1234, on_priority_change)
    >>>
    >>> # Keep the program running to receive callbacks
    >>> try:
    ...     while True:
    ...         time.sleep(1)
    ... except KeyboardInterrupt:
    ...     print("Monitoring stopped")
)");

    // Helper methods
    m.def(
        "is_elevated",
        []() -> bool {
#ifdef _WIN32
            BOOL is_elevated = FALSE;
            HANDLE token_handle = NULL;

            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY,
                                 &token_handle)) {
                TOKEN_ELEVATION elevation;
                DWORD size = sizeof(TOKEN_ELEVATION);

                if (GetTokenInformation(token_handle, TokenElevation,
                                        &elevation, sizeof(elevation), &size)) {
                    is_elevated = elevation.TokenIsElevated;
                }

                CloseHandle(token_handle);
            }

            return is_elevated != FALSE;
#else
            // On Unix systems, check if effective user ID is 0 (root)
            return geteuid() == 0;
#endif
        },
        R"(Checks if the current process has elevated privileges.

Returns:
    True if the process is running with administrator/root privileges, False otherwise.

Examples:
    >>> from atom.system import priority
    >>> if priority.is_elevated():
    ...     print("Running with elevated privileges")
    ...     # Can set realtime priority
    ...     priority.set_process_priority(priority.PriorityLevel.REALTIME)
    ... else:
    ...     print("Running without elevated privileges")
    ...     # Stick to safer priority levels
    ...     priority.set_process_priority(priority.PriorityLevel.ABOVE_NORMAL)
)");

    m.def(
        "get_available_cpu_count",
        []() -> int {
#ifdef _WIN32
            SYSTEM_INFO sysinfo;
            GetSystemInfo(&sysinfo);
            return static_cast<int>(sysinfo.dwNumberOfProcessors);
#else
            return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#endif
        },
        R"(Gets the number of available CPU cores.

Returns:
    The number of available CPU cores on the system.

Examples:
    >>> from atom.system import priority
    >>> cpu_count = priority.get_available_cpu_count()
    >>> print(f"This system has {cpu_count} CPU cores")
    >>>
    >>> # Pin process to first half of available cores
    >>> first_half = list(range(cpu_count // 2))
    >>> priority.set_process_affinity(first_half)
)");

    m.def(
        "priority_level_to_string",
        [](atom::system::PriorityManager::PriorityLevel level) -> std::string {
            switch (level) {
                case atom::system::PriorityManager::PriorityLevel::LOWEST:
                    return "Lowest";
                case atom::system::PriorityManager::PriorityLevel::BELOW_NORMAL:
                    return "Below Normal";
                case atom::system::PriorityManager::PriorityLevel::NORMAL:
                    return "Normal";
                case atom::system::PriorityManager::PriorityLevel::ABOVE_NORMAL:
                    return "Above Normal";
                case atom::system::PriorityManager::PriorityLevel::HIGHEST:
                    return "Highest";
                case atom::system::PriorityManager::PriorityLevel::REALTIME:
                    return "Realtime";
                default:
                    return "Unknown";
            }
        },
        py::arg("level"),
        R"(Converts a PriorityLevel enum value to a human-readable string.

Args:
    level: The PriorityLevel enum value.

Returns:
    A human-readable string representation of the priority level.

Examples:
    >>> from atom.system import priority
    >>> level = priority.get_process_priority()
    >>> level_str = priority.priority_level_to_string(level)
    >>> print(f"Current priority level: {level_str}")
)");

    // Convenience class for thread priority management
    py::class_<py::object>(m, "ThreadPriorityContext",
                           "Context manager for thread priority")
        .def(py::init([](atom::system::PriorityManager::PriorityLevel level) {
                 return py::object();  // Actual implementation in __enter__
             }),
             py::arg("level"), "Initialize thread priority context manager")
        .def("__enter__",
             [](py::object& self,
                atom::system::PriorityManager::PriorityLevel level) {
                 auto original_priority =
                     atom::system::PriorityManager::getThreadPriority();
                 self.attr("original_priority") = py::cast(original_priority);
                 atom::system::PriorityManager::setThreadPriority(level);
                 return self;
             })
        .def("__exit__", [](py::object& self, py::object, py::object,
                            py::object) {
            auto original_priority =
                py::cast<atom::system::PriorityManager::PriorityLevel>(
                    self.attr("original_priority"));
            atom::system::PriorityManager::setThreadPriority(original_priority);
            return false;  // Don't suppress exceptions
        });

    // Factory function for thread priority context
    m.def(
        "thread_priority",
        [&m](atom::system::PriorityManager::PriorityLevel level) {
            return m.attr("ThreadPriorityContext")(level);
        },
        py::arg("level"),
        R"(Creates a context manager for thread priority.

This function returns a context manager that temporarily sets the current thread's
priority and restores it when the context is exited.

Args:
    level: The priority level to set during the context.

Returns:
    A context manager for thread priority.

Examples:
    >>> from atom.system import priority
    >>> import time
    >>>
    >>> # Temporarily run with high priority
    >>> with priority.thread_priority(priority.PriorityLevel.HIGHEST):
    ...     # This code runs with high priority
    ...     for i in range(10):
    ...         time.sleep(0.1)
    ...         print(f"Processing with high priority: {i}")
    ... # Priority is automatically restored to original value
    >>> print("Back to normal priority")
)");

    // Convenience class for process priority management
    py::class_<py::object>(m, "ProcessPriorityContext",
                           "Context manager for process priority")
        .def(py::init([](atom::system::PriorityManager::PriorityLevel level,
                         int pid) {
                 return py::object();  // Actual implementation in __enter__
             }),
             py::arg("level"), py::arg("pid") = 0,
             "Initialize process priority context manager")
        .def("__enter__",
             [](py::object& self,
                atom::system::PriorityManager::PriorityLevel level, int pid) {
                 auto original_priority =
                     atom::system::PriorityManager::getProcessPriority(pid);
                 self.attr("original_priority") = py::cast(original_priority);
                 self.attr("pid") = py::cast(pid);
                 atom::system::PriorityManager::setProcessPriority(level, pid);
                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 auto original_priority =
                     py::cast<atom::system::PriorityManager::PriorityLevel>(
                         self.attr("original_priority"));
                 int pid = py::cast<int>(self.attr("pid"));
                 atom::system::PriorityManager::setProcessPriority(
                     original_priority, pid);
                 return false;  // Don't suppress exceptions
             });

    // Factory function for process priority context
    m.def(
        "process_priority",
        [&m](atom::system::PriorityManager::PriorityLevel level, int pid) {
            return m.attr("ProcessPriorityContext")(level, pid);
        },
        py::arg("level"), py::arg("pid") = 0,
        R"(Creates a context manager for process priority.

This function returns a context manager that temporarily sets a process's
priority and restores it when the context is exited.

Args:
    level: The priority level to set during the context.
    pid: The process ID. Defaults to 0, which means the current process.

Returns:
    A context manager for process priority.

Examples:
    >>> from atom.system import priority
    >>> import time
    >>>
    >>> # Temporarily run with high priority
    >>> with priority.process_priority(priority.PriorityLevel.HIGHEST):
    ...     # This code runs with high priority
    ...     for i in range(10):
    ...         time.sleep(0.1)
    ...         print(f"Processing with high priority: {i}")
    ... # Priority is automatically restored to original value
    >>> print("Back to normal priority")
)");

    // Function to run a callback with a specific priority
    m.def(
        "run_with_priority",
        [](atom::system::PriorityManager::PriorityLevel level,
           py::function callback) {
            auto original_priority =
                atom::system::PriorityManager::getProcessPriority();

            try {
                atom::system::PriorityManager::setProcessPriority(level);
                auto result = callback();
                atom::system::PriorityManager::setProcessPriority(
                    original_priority);
                return result;
            } catch (const std::exception& e) {
                atom::system::PriorityManager::setProcessPriority(
                    original_priority);
                throw;
            }
        },
        py::arg("level"), py::arg("callback"),
        R"(Runs a callback function with a specific process priority.

Args:
    level: The priority level to use during execution.
    callback: The function to execute with the specified priority.

Returns:
    The result of the callback function.

Examples:
    >>> from atom.system import priority
    >>>
    >>> def compute_something():
    ...     result = 0
    ...     for i in range(10000000):
    ...         result += i
    ...     return result
    ...
    >>> # Run with high priority
    >>> result = priority.run_with_priority(
    ...     priority.PriorityLevel.HIGHEST,
    ...     compute_something
    ... )
    >>> print(f"Result: {result}")
)");

    // Function to pin a thread to specified CPUs
    m.def(
        "pin_thread_to_cpus",
        [](const std::vector<int>& cpu_ids) {
#ifdef _WIN32
            DWORD_PTR mask = 0;
            for (int cpu : cpu_ids) {
                if (cpu >= 0 &&
                    cpu < 64) {  // Windows supports up to 64 CPUs in one mask
                    mask |= (static_cast<DWORD_PTR>(1) << cpu);
                }
            }

            if (mask == 0) {
                throw std::invalid_argument("Invalid CPU IDs or empty list");
            }

            HANDLE thread = GetCurrentThread();
            DWORD_PTR result = SetThreadAffinityMask(thread, mask);

            if (result == 0) {
                throw std::runtime_error("Failed to set thread affinity mask");
            }
#else
            cpu_set_t cpu_set;
            CPU_ZERO(&cpu_set);

            for (int cpu : cpu_ids) {
                if (cpu >= 0 && cpu < CPU_SETSIZE) {
                    CPU_SET(cpu, &cpu_set);
                }
            }

            int result = pthread_setaffinity_np(pthread_self(),
                                                sizeof(cpu_set_t), &cpu_set);

            if (result != 0) {
                throw std::runtime_error("Failed to set thread affinity: " +
                                         std::to_string(result));
            }
#endif

            return true;
        },
        py::arg("cpu_ids"),
        R"(Pins the current thread to specific CPU cores.

Args:
    cpu_ids: A list of CPU IDs to pin the thread to.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the CPU IDs are invalid or the list is empty.
    RuntimeError: If setting the thread affinity failed.

Examples:
    >>> from atom.system import priority
    >>> import threading
    >>>
    >>> def worker(cpu_id):
    ...     # Pin this thread to the specified CPU
    ...     priority.pin_thread_to_cpus([cpu_id])
    ...     # Now this thread will only run on the specified CPU
    ...     for i in range(10):
    ...         print(f"Thread on CPU {cpu_id}: {i}")
    ...
    >>> # Create threads and pin each to a different CPU
    >>> threads = []
    >>> for i in range(4):  # Create 4 threads
    ...     t = threading.Thread(target=worker, args=(i,))
    ...     threads.append(t)
    ...     t.start()
    ...
    >>> # Wait for all threads to complete
    >>> for t in threads:
    ...     t.join()
)");

    // Function to get affinity mask
    m.def(
        "get_thread_affinity",
        []() -> std::vector<int> {
            std::vector<int> cpu_ids;

#ifdef _WIN32
            DWORD_PTR process_mask;
            DWORD_PTR system_mask;

            if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask,
                                       &system_mask)) {
                for (int i = 0; i < 64; i++) {  // Check up to 64 CPUs
                    if ((process_mask >> i) & 1) {
                        cpu_ids.push_back(i);
                    }
                }
            }
#else
            cpu_set_t cpu_set;
            CPU_ZERO(&cpu_set);

            if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set),
                                       &cpu_set) == 0) {
                for (int i = 0; i < CPU_SETSIZE; i++) {
                    if (CPU_ISSET(i, &cpu_set)) {
                        cpu_ids.push_back(i);
                    }
                }
            }
#endif

            return cpu_ids;
        },
        R"(Gets the CPU affinity of the current thread.

Returns:
    A list of CPU IDs that the thread is allowed to run on.

Examples:
    >>> from atom.system import priority
    >>> # Get current thread affinity
    >>> cpu_ids = priority.get_thread_affinity()
    >>> print(f"Current thread can run on these CPUs: {cpu_ids}")
)");
}
