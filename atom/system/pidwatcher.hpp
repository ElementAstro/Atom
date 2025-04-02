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
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

namespace atom::system {

/**
 * @brief Process status enum
 */
enum class ProcessStatus {
    UNKNOWN,
    RUNNING,
    SLEEPING,
    WAITING,
    STOPPED,
    ZOMBIE,
    DEAD
};

/**
 * @brief Process I/O statistics structure
 */
struct ProcessIOStats {
    uint64_t read_bytes;   ///< Total bytes read
    uint64_t write_bytes;  ///< Total bytes written
    double read_rate;      ///< Current read rate (bytes/sec)
    double write_rate;     ///< Current write rate (bytes/sec)

    ProcessIOStats()
        : read_bytes(0), write_bytes(0), read_rate(0.0), write_rate(0.0) {}
};

/**
 * @brief Process information structure
 */
struct ProcessInfo {
    pid_t pid;                  ///< Process ID
    pid_t parent_pid;           ///< Parent process ID
    std::string name;           ///< Process name
    std::string command_line;   ///< Full command line
    std::string username;       ///< Owner username
    ProcessStatus status;       ///< Process status
    bool running;               ///< Process running status
    double cpu_usage;           ///< CPU usage percentage
    size_t memory_usage;        ///< Memory usage in KB
    size_t virtual_memory;      ///< Virtual memory in KB
    size_t shared_memory;       ///< Shared memory in KB
    int priority;               ///< Process priority/nice value
    unsigned int thread_count;  ///< Number of threads
    ProcessIOStats io_stats;    ///< I/O statistics
    std::chrono::system_clock::time_point start_time;  ///< Process start time
    std::chrono::milliseconds uptime;                  ///< Process uptime
    std::vector<pid_t> child_processes;                ///< Child process IDs

    ProcessInfo()
        : pid(0),
          parent_pid(0),
          status(ProcessStatus::UNKNOWN),
          running(false),
          cpu_usage(0.0),
          memory_usage(0),
          virtual_memory(0),
          shared_memory(0),
          priority(0),
          thread_count(0) {}
};

/**
 * @brief Resource limit configuration
 */
struct ResourceLimits {
    double max_cpu_percent;  ///< Maximum CPU usage percentage
    size_t max_memory_kb;    ///< Maximum memory usage in KB

    ResourceLimits() : max_cpu_percent(0.0), max_memory_kb(0) {}
};

/**
 * @brief Configuration for process monitoring
 */
struct MonitorConfig {
    std::chrono::milliseconds update_interval;  ///< Update interval
    bool monitor_children;           ///< Whether to monitor child processes
    bool auto_restart;               ///< Whether to restart process on exit
    int max_restart_attempts;        ///< Maximum restart attempts
    ResourceLimits resource_limits;  ///< Resource limits

    MonitorConfig()
        : update_interval(std::chrono::milliseconds(1000)),
          monitor_children(false),
          auto_restart(false),
          max_restart_attempts(3) {}
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
    using ProcessCallback = std::function<void(const ProcessInfo&)>;
    using MultiProcessCallback =
        std::function<void(const std::vector<ProcessInfo>&)>;
    using ErrorCallback = std::function<void(const std::string&, int)>;
    using ResourceLimitCallback =
        std::function<void(const ProcessInfo&, const ResourceLimits&)>;
    using ProcessCreateCallback =
        std::function<void(pid_t, const std::string&)>;
    using ProcessFilter = std::function<bool(const ProcessInfo&)>;

    /**
     * @brief Constructs a PidWatcher object.
     */
    PidWatcher();

    /**
     * @brief Constructs a PidWatcher object with specific configuration.
     *
     * @param config The monitoring configuration
     */
    explicit PidWatcher(const MonitorConfig& config);

    /**
     * @brief Destroys the PidWatcher object.
     */
    ~PidWatcher();

    /**
     * @brief Sets the callback function to be executed on process exit.
     *
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setExitCallback(ProcessCallback callback);

    /**
     * @brief Sets the monitor function to be executed at specified intervals.
     *
     * @param callback The monitor function to set.
     * @param interval The interval at which the monitor function should run.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setMonitorFunction(ProcessCallback callback,
                                   std::chrono::milliseconds interval);

    /**
     * @brief Sets the callback for monitoring multiple processes.
     *
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setMultiProcessCallback(MultiProcessCallback callback);

    /**
     * @brief Sets the error callback function.
     *
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setErrorCallback(ErrorCallback callback);

    /**
     * @brief Sets the callback for resource limit violations.
     *
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setResourceLimitCallback(ResourceLimitCallback callback);

    /**
     * @brief Sets the callback for process creation events.
     *
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setProcessCreateCallback(ProcessCreateCallback callback);

    /**
     * @brief Sets a filter for processes to monitor.
     *
     * @param filter Function that returns true for processes to monitor.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setProcessFilter(ProcessFilter filter);

    /**
     * @brief Retrieves the PID of a process by its name.
     *
     * @param name The name of the process.
     * @return The PID of the process or 0 if not found.
     */
    [[nodiscard]] auto getPidByName(const std::string& name) const -> pid_t;

    /**
     * @brief Retrieves multiple PIDs by process name (for multiple instances).
     *
     * @param name The name of the process.
     * @return Vector of PIDs matching the name.
     */
    [[nodiscard]] auto getPidsByName(const std::string& name) const
        -> std::vector<pid_t>;

    /**
     * @brief Get information about a process.
     *
     * @param pid The process ID.
     * @return Process information or nullopt if not found.
     */
    [[nodiscard]] auto getProcessInfo(pid_t pid) const
        -> std::optional<ProcessInfo>;

    /**
     * @brief Get a list of all currently running processes.
     *
     * @return Vector of ProcessInfo for all running processes.
     */
    [[nodiscard]] auto getAllProcesses() const -> std::vector<ProcessInfo>;

    /**
     * @brief Get child processes of a specific process.
     *
     * @param pid The parent process ID.
     * @return Vector of child process IDs.
     */
    [[nodiscard]] auto getChildProcesses(pid_t pid) const -> std::vector<pid_t>;

    /**
     * @brief Starts monitoring the specified process by name.
     *
     * @param name The name of the process to monitor.
     * @param config Optional specific configuration for this process.
     * @return True if monitoring started successfully, false otherwise.
     */
    auto start(const std::string& name, const MonitorConfig* config = nullptr)
        -> bool;

    /**
     * @brief Starts monitoring a process by its PID.
     *
     * @param pid The process ID to monitor.
     * @param config Optional specific configuration for this process.
     * @return True if monitoring started successfully, false otherwise.
     */
    auto startByPid(pid_t pid, const MonitorConfig* config = nullptr) -> bool;

    /**
     * @brief Starts monitoring multiple processes at once.
     *
     * @param process_names Vector of process names to monitor.
     * @param config Optional specific configuration for these processes.
     * @return Number of successfully started monitors.
     */
    auto startMultiple(const std::vector<std::string>& process_names,
                       const MonitorConfig* config = nullptr) -> size_t;

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
    auto switchToProcess(const std::string& name) -> bool;

    /**
     * @brief Switches the target process to monitor by PID.
     *
     * @param pid The process ID to switch to.
     * @return True if the process was successfully switched, false otherwise.
     */
    auto switchToProcessById(pid_t pid) -> bool;

    /**
     * @brief Check if the watcher is actively monitoring any processes.
     *
     * @return True if at least one process is being monitored.
     */
    [[nodiscard]] bool isActive() const;

    /**
     * @brief Checks if a specific process is being monitored.
     *
     * @param pid The process ID to check.
     * @return True if the process is being monitored.
     */
    [[nodiscard]] bool isMonitoring(pid_t pid) const;

    /**
     * @brief Checks if a specific process is running.
     *
     * @param pid The process ID to check.
     * @return True if the process is running.
     */
    [[nodiscard]] bool isProcessRunning(pid_t pid) const;

    /**
     * @brief Gets CPU usage of a process.
     *
     * @param pid The process ID.
     * @return CPU usage as percentage or -1.0 on error.
     */
    [[nodiscard]] double getProcessCpuUsage(pid_t pid) const;

    /**
     * @brief Gets memory usage of a process.
     *
     * @param pid The process ID.
     * @return Memory usage in KB or 0 on error.
     */
    [[nodiscard]] size_t getProcessMemoryUsage(pid_t pid) const;

    /**
     * @brief Gets thread count of a process.
     *
     * @param pid The process ID.
     * @return Number of threads or 0 on error.
     */
    [[nodiscard]] unsigned int getProcessThreadCount(pid_t pid) const;

    /**
     * @brief Gets I/O statistics for a process.
     *
     * @param pid The process ID.
     * @return ProcessIOStats structure with I/O information.
     */
    [[nodiscard]] ProcessIOStats getProcessIOStats(pid_t pid) const;

    /**
     * @brief Gets the process status.
     *
     * @param pid The process ID.
     * @return Process status enum.
     */
    [[nodiscard]] ProcessStatus getProcessStatus(pid_t pid) const;

    /**
     * @brief Gets the uptime of a process.
     *
     * @param pid The process ID.
     * @return Process uptime in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getProcessUptime(pid_t pid) const;

    /**
     * @brief Launch a new process.
     *
     * @param command The command to execute.
     * @param args Vector of command arguments.
     * @param auto_monitor Whether to automatically start monitoring the new
     * process.
     * @return PID of the new process or 0 on failure.
     */
    pid_t launchProcess(const std::string& command,
                        const std::vector<std::string>& args = {},
                        bool auto_monitor = true);

    /**
     * @brief Terminate a process.
     *
     * @param pid The process ID to terminate.
     * @param force Whether to force termination (SIGKILL vs SIGTERM).
     * @return True if termination signal was sent successfully.
     */
    bool terminateProcess(pid_t pid, bool force = false);

    /**
     * @brief Configure process resource limits.
     *
     * @param pid The process ID.
     * @param limits The resource limits to apply.
     * @return True if limits were set successfully.
     */
    bool setResourceLimits(pid_t pid, const ResourceLimits& limits);

    /**
     * @brief Change process priority.
     *
     * @param pid The process ID.
     * @param priority New priority value (nice).
     * @return True if priority was changed successfully.
     */
    bool setProcessPriority(pid_t pid, int priority);

    /**
     * @brief Configure process auto-restart behavior.
     *
     * @param pid The process ID.
     * @param enable Whether to enable auto-restart.
     * @param max_attempts Maximum number of restart attempts.
     * @return True if configuration was applied successfully.
     */
    bool configureAutoRestart(pid_t pid, bool enable, int max_attempts = 3);

    /**
     * @brief Restart a process.
     *
     * @param pid The process ID to restart.
     * @return PID of the new process or 0 on failure.
     */
    pid_t restartProcess(pid_t pid);

    /**
     * @brief Dump process information to a log or file.
     *
     * @param pid The process ID.
     * @param detailed Whether to include detailed information.
     * @param output_file Optional file to write to (default: log).
     * @return True if dump was successful.
     */
    bool dumpProcessInfo(pid_t pid, bool detailed = false,
                         const std::string& output_file = "");

    /**
     * @brief Get monitoring statistics.
     *
     * @return Map of monitoring statistics by process ID.
     */
    [[nodiscard]] std::unordered_map<pid_t, std::map<std::string, double>>
    getMonitoringStats() const;

    /**
     * @brief Set rate limiting for monitoring to prevent high CPU usage.
     *
     * @param max_updates_per_second Maximum update operations per second.
     * @return Reference to this object for method chaining.
     */
    PidWatcher& setRateLimiting(unsigned int max_updates_per_second);

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
     * @brief The thread function for monitoring resource usage.
     */
    void resourceMonitorThread();

    /**
     * @brief The thread function for auto-restarting processes.
     */
    void autoRestartThread();

    /**
     * @brief The watchdog thread to ensure watcher doesn't hang.
     */
    void watchdogThread();

    /**
     * @brief Update process information.
     *
     * @param pid The process ID to update.
     * @return Updated process information.
     */
    ProcessInfo updateProcessInfo(pid_t pid);

    /**
     * @brief Check if we can perform an update based on rate limiting.
     *
     * @return True if update is allowed, false if it should be skipped.
     */
    bool checkRateLimit();

    /**
     * @brief Internal function to monitor child processes.
     *
     * @param parent_pid The parent process ID.
     */
    void monitorChildProcesses(pid_t parent_pid);

    /**
     * @brief Get process username by PID.
     *
     * @param pid The process ID.
     * @return Username or empty string if not found.
     */
    std::string getProcessUsername(pid_t pid) const;

    /**
     * @brief Get process command line.
     *
     * @param pid The process ID.
     * @return Full command line or empty string if not found.
     */
    std::string getProcessCommandLine(pid_t pid) const;

    /**
     * @brief Update I/O statistics for a process.
     *
     * @param pid The process ID.
     * @param stats Reference to stats structure to update.
     */
    void updateIOStats(pid_t pid, ProcessIOStats& stats) const;

    /**
     * @brief Internal helper to check resource limits.
     *
     * @param pid The process ID.
     * @param info Process information.
     */
    void checkResourceLimits(pid_t pid, const ProcessInfo& info);

    pid_t pid_;  ///< The PID of the currently primary monitored process.
    std::atomic<bool> running_;  ///< Flag indicating if the watcher is running.
    std::atomic<bool>
        monitoring_;  ///< Flag indicating if a process is being monitored.
    std::atomic<bool>
        watchdog_healthy_;  ///< Flag for watchdog health checking.

    ProcessCallback
        exit_callback_;  ///< Callback function to execute on process exit.
    ProcessCallback
        monitor_callback_;  ///< Monitor function to execute at intervals.
    MultiProcessCallback
        multi_process_callback_;    ///< Callback for multiple processes.
    ErrorCallback error_callback_;  ///< Callback for error handling.
    ResourceLimitCallback
        resource_limit_callback_;  ///< Callback for resource limits.
    ProcessCreateCallback
        process_create_callback_;   ///< Callback for process creation.
    ProcessFilter process_filter_;  ///< Filter for processes to monitor.

    MonitorConfig global_config_;  ///< Global monitoring configuration.
    std::unordered_map<pid_t, MonitorConfig>
        process_configs_;  ///< Per-process configurations.

    std::chrono::milliseconds
        monitor_interval_;  ///< Interval for monitor function execution.

    std::unordered_map<pid_t, ProcessInfo>
        monitored_processes_;  ///< Map of monitored processes
    std::unordered_map<pid_t, int>
        restart_attempts_;  ///< Track restart attempts
    std::unordered_map<pid_t,
                       std::chrono::time_point<std::chrono::steady_clock>>
        last_update_time_;  ///< Last update time

    // For rate limiting
    unsigned int max_updates_per_second_;  ///< Maximum updates per second
    std::chrono::time_point<std::chrono::steady_clock>
        rate_limit_start_time_;               ///< Rate limit period start
    std::atomic<unsigned int> update_count_;  ///< Updates in current period

    // CPU usage tracking
    struct CPUUsageData {
        unsigned long long lastTotalUser;
        unsigned long long lastTotalUserLow;
        unsigned long long lastTotalSys;
        unsigned long long lastTotalIdle;
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };

    mutable std::unordered_map<pid_t, CPUUsageData>
        cpu_usage_data_;  ///< CPU usage tracking data

    // I/O tracking
    mutable std::unordered_map<pid_t, ProcessIOStats>
        prev_io_stats_;  ///< Previous I/O stats

    // Monitoring statistics
    mutable std::unordered_map<pid_t, std::map<std::string, double>>
        monitoring_stats_;  ///< Statistics

#if __cplusplus >= 202002L
    std::jthread monitor_thread_;  ///< Thread for monitoring the process.
    std::jthread exit_thread_;     ///< Thread for handling process exit.
    std::jthread
        multi_monitor_thread_;  ///< Thread for monitoring multiple processes.
    std::jthread
        resource_monitor_thread_;  ///< Thread for monitoring resource usage.
    std::jthread
        auto_restart_thread_;       ///< Thread for auto-restarting processes.
    std::jthread watchdog_thread_;  ///< Watchdog thread.
#else
    std::thread monitor_thread_;  ///< Thread for monitoring the process.
    std::thread exit_thread_;     ///< Thread for handling process exit.
    std::thread
        multi_monitor_thread_;  ///< Thread for monitoring multiple processes.
    std::thread
        resource_monitor_thread_;  ///< Thread for monitoring resource usage.
    std::thread
        auto_restart_thread_;      ///< Thread for auto-restarting processes.
    std::thread watchdog_thread_;  ///< Watchdog thread.
#endif

    mutable std::mutex mutex_;  ///< Mutex for thread synchronization.
    std::condition_variable
        monitor_cv_;                   ///< Condition variable for monitoring.
    std::condition_variable exit_cv_;  ///< Condition variable for process exit.
    std::condition_variable multi_monitor_cv_;  ///< Condition variable for
                                                ///< multi-process monitoring.
    std::condition_variable
        resource_monitor_cv_;  ///< Condition variable for resource monitoring.
    std::condition_variable
        auto_restart_cv_;  ///< Condition variable for auto-restart.
    std::condition_variable watchdog_cv_;  ///< Condition variable for watchdog.
};

}  // namespace atom::system

#endif