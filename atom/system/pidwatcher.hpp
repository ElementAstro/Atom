/*
 * pidwatcher.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-17

Description: PID Watcher

**************************************************/

#ifndef ATOM_SYSTEM_PIDWATCHER_HPP
#define ATOM_SYSTEM_PIDWATCHER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

namespace atom::system {

/**
 * @brief Process information structure
 */
struct ProcessInfo {
    pid_t pid;            ///< Process ID
    std::string name;     ///< Process name
    bool running;         ///< Process running status
    double cpu_usage;     ///< CPU usage percentage
    size_t memory_usage;  ///< Memory usage in KB
    std::chrono::system_clock::time_point start_time;  ///< Process start time

    ProcessInfo() : pid(0), running(false), cpu_usage(0.0), memory_usage(0) {}
};

/**
 * @brief A class for monitoring processes by their PID.
 *
 * This class allows monitoring of processes by their PID. It provides
 * functionality to set callbacks on process exit, set a monitor function to
 * run at intervals, get PID by process name, start monitoring a specific
 * process, stop monitoring, and switch the target process.
 */
class PidWatcher {
public:
    using ProcessCallback = std::function<void(const ProcessInfo &)>;
    using MultiProcessCallback =
        std::function<void(const std::vector<ProcessInfo> &)>;
    using ErrorCallback = std::function<void(const std::string &, int)>;

    /**
     * @brief Constructs a PidWatcher object.
     */
    PidWatcher();

    /**
     * @brief Destroys the PidWatcher object.
     */
    ~PidWatcher();

    /**
     * @brief Sets the callback function to be executed on process exit.
     *
     * @param callback The callback function to set.
     */
    void setExitCallback(ProcessCallback callback);

    /**
     * @brief Sets the monitor function to be executed at specified intervals.
     *
     * @param callback The monitor function to set.
     * @param interval The interval at which the monitor function should run.
     */
    void setMonitorFunction(ProcessCallback callback,
                            std::chrono::milliseconds interval);

    /**
     * @brief Sets the callback for monitoring multiple processes.
     *
     * @param callback The callback function to set.
     */
    void setMultiProcessCallback(MultiProcessCallback callback);

    /**
     * @brief Sets the error callback function.
     *
     * @param callback The callback function to set.
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Retrieves the PID of a process by its name.
     *
     * @param name The name of the process.
     * @return The PID of the process.
     */
    [[nodiscard]] auto getPidByName(const std::string &name) const -> pid_t;

    /**
     * @brief Get information about a process.
     *
     * @param pid The process ID.
     * @return Process information or nullopt if not found.
     */
    [[nodiscard]] auto getProcessInfo(pid_t pid) const
        -> std::optional<ProcessInfo>;

    /**
     * @brief Starts monitoring the specified process by name.
     *
     * @param name The name of the process to monitor.
     * @return True if monitoring started successfully, false otherwise.
     */
    auto start(const std::string &name) -> bool;

    /**
     * @brief Starts monitoring multiple processes at once.
     *
     * @param process_names Vector of process names to monitor.
     * @return Number of successfully started monitors.
     */
    auto startMultiple(const std::vector<std::string> &process_names) -> size_t;

    /**
     * @brief Stops monitoring the currently monitored process.
     */
    void stop();

    /**
     * @brief Stops monitoring a specific process.
     *
     * @param pid The process ID to stop monitoring.
     * @return True if the process was being monitored and stopped.
     */
    bool stopProcess(pid_t pid);

    /**
     * @brief Switches the target process to monitor.
     *
     * @param name The name of the process to switch to.
     * @return True if the process was successfully switched, false otherwise.
     */
    auto Switch(const std::string &name) -> bool;

    /**
     * @brief Checks if a specific process is being monitored.
     *
     * @param pid The process ID to check.
     * @return True if the process is being monitored.
     */
    bool isMonitoring(pid_t pid) const;

    /**
     * @brief Checks if a specific process is running.
     *
     * @param pid The process ID to check.
     * @return True if the process is running.
     */
    bool isProcessRunning(pid_t pid) const;

    /**
     * @brief Gets CPU usage of a process.
     *
     * @param pid The process ID.
     * @return CPU usage as percentage or -1.0 on error.
     */
    double getProcessCpuUsage(pid_t pid) const;

    /**
     * @brief Gets memory usage of a process.
     *
     * @param pid The process ID.
     * @return Memory usage in KB or 0 on error.
     */
    size_t getProcessMemoryUsage(pid_t pid) const;

private:
    /**
     * @brief The thread function for monitoring the process.
     */
    void monitorThread();

    /**
     * @brief The thread function for handling process exit.
     */
    void exitThread();

    /**
     * @brief The thread function for monitoring multiple processes.
     */
    void multiMonitorThread();

    /**
     * @brief Update process information.
     *
     * @param pid The process ID to update.
     * @return Updated process information.
     */
    ProcessInfo updateProcessInfo(pid_t pid);

    pid_t pid_;  ///< The PID of the currently monitored process.
    std::atomic<bool>
        running_;  ///< Flag indicating if the monitoring is running.
    std::atomic<bool>
        monitoring_;  ///< Flag indicating if a process is being monitored.

    ProcessCallback
        exit_callback_;  ///< Callback function to execute on process exit.
    ProcessCallback
        monitor_callback_;  ///< Monitor function to execute at intervals.
    MultiProcessCallback
        multi_process_callback_;    ///< Callback for multiple processes.
    ErrorCallback error_callback_;  ///< Callback for error handling.

    std::chrono::milliseconds
        monitor_interval_;  ///< Interval for monitor function execution.

    std::unordered_map<pid_t, ProcessInfo>
        monitored_processes_;  ///< Map of monitored processes

#if __cplusplus >= 202002L
    std::jthread monitor_thread_;  ///< Thread for monitoring the process.
    std::jthread exit_thread_;     ///< Thread for handling process exit.
    std::jthread
        multi_monitor_thread_;  ///< Thread for monitoring multiple processes.
#else
    std::thread monitor_thread_;  ///< Thread for monitoring the process.
    std::thread exit_thread_;     ///< Thread for handling process exit.
    std::thread
        multi_monitor_thread_;  ///< Thread for monitoring multiple processes.
#endif

    mutable std::mutex mutex_;  ///< Mutex for thread synchronization.
    std::condition_variable
        monitor_cv_;                   ///< Condition variable for monitoring.
    std::condition_variable exit_cv_;  ///< Condition variable for process exit.
    std::condition_variable multi_monitor_cv_;  ///< Condition variable for
                                                ///< multi-process monitoring.

    // CPU usage tracking
    struct CPUUsageData {
        unsigned long long lastTotalUser;
        unsigned long long lastTotalUserLow;
        unsigned long long lastTotalSys;
        unsigned long long lastTotalIdle;
    };

    mutable std::unordered_map<pid_t, CPUUsageData>
        cpu_usage_data_;  ///< CPU usage tracking data
};

}  // namespace atom::system

#endif
