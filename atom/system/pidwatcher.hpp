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
 * @brief Process status enumeration
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
    uint64_t read_bytes{0};    ///< Total bytes read
    uint64_t write_bytes{0};   ///< Total bytes written
    double read_rate{0.0};     ///< Current read rate (bytes/sec)
    double write_rate{0.0};    ///< Current write rate (bytes/sec)
    std::chrono::steady_clock::time_point last_update{std::chrono::steady_clock::now()}; ///< Last update time
};

/**
 * @brief Process information structure
 */
struct ProcessInfo {
    pid_t pid{0};                           ///< Process ID
    pid_t parent_pid{0};                    ///< Parent process ID
    std::string name;                       ///< Process name
    std::string command_line;               ///< Full command line
    std::string username;                   ///< Owner username
    ProcessStatus status{ProcessStatus::UNKNOWN}; ///< Process status
    bool running{false};                    ///< Process running status
    double cpu_usage{0.0};                  ///< CPU usage percentage
    size_t memory_usage{0};                 ///< Memory usage in KB
    size_t virtual_memory{0};               ///< Virtual memory in KB
    size_t shared_memory{0};                ///< Shared memory in KB
    int priority{0};                        ///< Process priority/nice value
    unsigned int thread_count{0};           ///< Number of threads
    ProcessIOStats io_stats;                ///< I/O statistics
    std::chrono::system_clock::time_point start_time; ///< Process start time
    std::chrono::milliseconds uptime{0};    ///< Process uptime
    std::vector<pid_t> child_processes;     ///< Child process IDs
};

/**
 * @brief Resource limit configuration
 */
struct ResourceLimits {
    double max_cpu_percent{0.0}; ///< Maximum CPU usage percentage
    size_t max_memory_kb{0};     ///< Maximum memory usage in KB
};

/**
 * @brief Configuration for process monitoring
 */
struct MonitorConfig {
    std::chrono::milliseconds update_interval{1000}; ///< Update interval
    bool monitor_children{false};          ///< Whether to monitor child processes
    bool auto_restart{false};              ///< Whether to restart process on exit
    int max_restart_attempts{3};           ///< Maximum restart attempts
    ResourceLimits resource_limits;        ///< Resource limits
};

/**
 * @brief A class for monitoring processes by their PID.
 */
class PidWatcher {
public:
    using ProcessCallback = std::function<void(const ProcessInfo&)>;
    using MultiProcessCallback = std::function<void(const std::vector<ProcessInfo>&)>;
    using ErrorCallback = std::function<void(const std::string&, int)>;
    using ResourceLimitCallback = std::function<void(const ProcessInfo&, const ResourceLimits&)>;
    using ProcessCreateCallback = std::function<void(pid_t, const std::string&)>;
    using ProcessFilter = std::function<bool(const ProcessInfo&)>;

    /**
     * @brief Constructs a PidWatcher object.
     */
    PidWatcher();

    /**
     * @brief Constructs a PidWatcher object with specific configuration.
     * @param config The monitoring configuration
     */
    explicit PidWatcher(const MonitorConfig& config);

    /**
     * @brief Destroys the PidWatcher object.
     */
    ~PidWatcher();

    PidWatcher(const PidWatcher&) = delete;
    PidWatcher& operator=(const PidWatcher&) = delete;
    PidWatcher(PidWatcher&&) = delete;
    PidWatcher& operator=(PidWatcher&&) = delete;

    /**
     * @brief Sets the callback function to be executed on process exit.
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setExitCallback(ProcessCallback callback);

    /**
     * @brief Sets the monitor function to be executed at specified intervals.
     * @param callback The monitor function to set.
     * @param interval The interval at which the monitor function should run.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setMonitorFunction(ProcessCallback callback, std::chrono::milliseconds interval);

    /**
     * @brief Sets the callback for monitoring multiple processes.
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setMultiProcessCallback(MultiProcessCallback callback);

    /**
     * @brief Sets the error callback function.
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setErrorCallback(ErrorCallback callback);

    /**
     * @brief Sets the callback for resource limit violations.
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setResourceLimitCallback(ResourceLimitCallback callback);

    /**
     * @brief Sets the callback for process creation events.
     * @param callback The callback function to set.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setProcessCreateCallback(ProcessCreateCallback callback);

    /**
     * @brief Sets a filter for processes to monitor.
     * @param filter Function that returns true for processes to monitor.
     * @return Reference to this object for method chaining
     */
    PidWatcher& setProcessFilter(ProcessFilter filter);

    /**
     * @brief Retrieves the PID of a process by its name.
     * @param name The name of the process.
     * @return The PID of the process or 0 if not found.
     */
    [[nodiscard]] pid_t getPidByName(const std::string& name) const;

    /**
     * @brief Retrieves multiple PIDs by process name (for multiple instances).
     * @param name The name of the process.
     * @return Vector of PIDs matching the name.
     */
    [[nodiscard]] std::vector<pid_t> getPidsByName(const std::string& name) const;

    /**
     * @brief Get information about a process.
     * @param pid The process ID.
     * @return Process information or nullopt if not found.
     */
    [[nodiscard]] std::optional<ProcessInfo> getProcessInfo(pid_t pid);

    /**
     * @brief Get a list of all currently running processes.
     * @return Vector of ProcessInfo for all running processes.
     */
    [[nodiscard]] std::vector<ProcessInfo> getAllProcesses();

    /**
     * @brief Get child processes of a specific process.
     * @param pid The parent process ID.
     * @return Vector of child process IDs.
     */
    [[nodiscard]] std::vector<pid_t> getChildProcesses(pid_t pid) const;

    /**
     * @brief Starts monitoring the specified process by name.
     * @param name The name of the process to monitor.
     * @param config Optional specific configuration for this process.
     * @return True if monitoring started successfully, false otherwise.
     */
    bool start(const std::string& name, const MonitorConfig* config = nullptr);

    /**
     * @brief Starts monitoring a process by its PID.
     * @param pid The process ID to monitor.
     * @param config Optional specific configuration for this process.
     * @return True if monitoring started successfully, false otherwise.
     */
    bool startByPid(pid_t pid, const MonitorConfig* config = nullptr);

    /**
     * @brief Starts monitoring multiple processes at once.
     * @param process_names Vector of process names to monitor.
     * @param config Optional specific configuration for these processes.
     * @return Number of successfully started monitors.
     */
    size_t startMultiple(const std::vector<std::string>& process_names, const MonitorConfig* config = nullptr);

    /**
     * @brief Stops monitoring all processes.
     */
    void stop();

    /**
     * @brief Stops monitoring a specific process.
     * @param pid The process ID to stop monitoring.
     * @return True if the process was being monitored and stopped.
     */
    bool stopProcess(pid_t pid);

    /**
     * @brief Switches the target process to monitor.
     * @param name The name of the process to switch to.
     * @return True if the process was successfully switched, false otherwise.
     */
    bool switchToProcess(const std::string& name);

    /**
     * @brief Switches the target process to monitor by PID.
     * @param pid The process ID to switch to.
     * @return True if the process was successfully switched, false otherwise.
     */
    bool switchToProcessById(pid_t pid);

    /**
     * @brief Check if the watcher is actively monitoring any processes.
     * @return True if at least one process is being monitored.
     */
    [[nodiscard]] bool isActive() const;

    /**
     * @brief Checks if a specific process is being monitored.
     * @param pid The process ID to check.
     * @return True if the process is being monitored.
     */
    [[nodiscard]] bool isMonitoring(pid_t pid) const;

    /**
     * @brief Checks if a specific process is running.
     * @param pid The process ID to check.
     * @return True if the process is running.
     */
    [[nodiscard]] bool isProcessRunning(pid_t pid) const;

    /**
     * @brief Gets CPU usage of a process.
     * @param pid The process ID.
     * @return CPU usage as percentage or -1.0 on error.
     */
    [[nodiscard]] double getProcessCpuUsage(pid_t pid) const;

    /**
     * @brief Gets memory usage of a process.
     * @param pid The process ID.
     * @return Memory usage in KB or 0 on error.
     */
    [[nodiscard]] size_t getProcessMemoryUsage(pid_t pid) const;

    /**
     * @brief Gets thread count of a process.
     * @param pid The process ID.
     * @return Number of threads or 0 on error.
     */
    [[nodiscard]] unsigned int getProcessThreadCount(pid_t pid) const;

    /**
     * @brief Gets I/O statistics for a process.
     * @param pid The process ID.
     * @return ProcessIOStats structure with I/O information.
     */
    [[nodiscard]] ProcessIOStats getProcessIOStats(pid_t pid);

    /**
     * @brief Gets the process status.
     * @param pid The process ID.
     * @return Process status enum.
     */
    [[nodiscard]] ProcessStatus getProcessStatus(pid_t pid) const;

    /**
     * @brief Gets the uptime of a process.
     * @param pid The process ID.
     * @return Process uptime in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getProcessUptime(pid_t pid) const;

    /**
     * @brief Launch a new process.
     * @param command The command to execute.
     * @param args Vector of command arguments.
     * @param auto_monitor Whether to automatically start monitoring the new process.
     * @return PID of the new process or 0 on failure.
     */
    pid_t launchProcess(const std::string& command, const std::vector<std::string>& args = {}, bool auto_monitor = true);

    /**
     * @brief Terminate a process.
     * @param pid The process ID to terminate.
     * @param force Whether to force termination (SIGKILL vs SIGTERM).
     * @return True if termination signal was sent successfully.
     */
    bool terminateProcess(pid_t pid, bool force = false);

    /**
     * @brief Configure process resource limits.
     * @param pid The process ID.
     * @param limits The resource limits to apply.
     * @return True if limits were set successfully.
     */
    bool setResourceLimits(pid_t pid, const ResourceLimits& limits);

    /**
     * @brief Change process priority.
     * @param pid The process ID.
     * @param priority New priority value (nice).
     * @return True if priority was changed successfully.
     */
    bool setProcessPriority(pid_t pid, int priority);

    /**
     * @brief Configure process auto-restart behavior.
     * @param pid The process ID.
     * @param enable Whether to enable auto-restart.
     * @param max_attempts Maximum number of restart attempts.
     * @return True if configuration was applied successfully.
     */
    bool configureAutoRestart(pid_t pid, bool enable, int max_attempts = 3);

    /**
     * @brief Restart a process.
     * @param pid The process ID to restart.
     * @return PID of the new process or 0 on failure.
     */
    pid_t restartProcess(pid_t pid);

    /**
     * @brief Dump process information to a log or file.
     * @param pid The process ID.
     * @param detailed Whether to include detailed information.
     * @param output_file Optional file to write to (default: log).
     * @return True if dump was successful.
     */
    bool dumpProcessInfo(pid_t pid, bool detailed = false, const std::string& output_file = "");

    /**
     * @brief Get monitoring statistics.
     * @return Map of monitoring statistics by process ID.
     */
    [[nodiscard]] std::unordered_map<pid_t, std::map<std::string, double>> getMonitoringStats() const;

    /**
     * @brief Set rate limiting for monitoring to prevent high CPU usage.
     * @param max_updates_per_second Maximum update operations per second.
     * @return Reference to this object for method chaining.
     */
    PidWatcher& setRateLimiting(unsigned int max_updates_per_second);

private:
    void monitorThread();
    void exitThread();
    void multiMonitorThread();
    void resourceMonitorThread();
    void autoRestartThread();
    void watchdogThread();

    ProcessInfo updateProcessInfo(pid_t pid);
    bool checkRateLimit();
    void monitorChildProcesses(pid_t parent_pid);
    std::string getProcessUsername(pid_t pid) const;
    std::string getProcessCommandLine(pid_t pid) const;
    void updateIOStats(pid_t pid, ProcessIOStats& stats) const;
    void checkResourceLimits(pid_t pid, const ProcessInfo& info);

    std::atomic<pid_t> primary_pid_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> monitoring_{false};
    std::atomic<bool> watchdog_healthy_{true};

    ProcessCallback exit_callback_;
    ProcessCallback monitor_callback_;
    MultiProcessCallback multi_process_callback_;
    ErrorCallback error_callback_;
    ResourceLimitCallback resource_limit_callback_;
    ProcessCreateCallback process_create_callback_;
    ProcessFilter process_filter_;

    MonitorConfig global_config_;
    std::unordered_map<pid_t, MonitorConfig> process_configs_;
    std::chrono::milliseconds monitor_interval_{1000};

    std::unordered_map<pid_t, ProcessInfo> monitored_processes_;
    std::unordered_map<pid_t, int> restart_attempts_;
    std::unordered_map<pid_t, std::chrono::time_point<std::chrono::steady_clock>> last_update_time_;

    std::atomic<unsigned int> max_updates_per_second_{10};
    std::chrono::time_point<std::chrono::steady_clock> rate_limit_start_time_;
    std::atomic<unsigned int> update_count_{0};

    struct CPUUsageData {
        uint64_t last_total_user{0};
        uint64_t last_total_user_low{0};
        uint64_t last_total_sys{0};
        uint64_t last_total_idle{0};
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };

    mutable std::unordered_map<pid_t, CPUUsageData> cpu_usage_data_;
    mutable std::unordered_map<pid_t, ProcessIOStats> prev_io_stats_;
    mutable std::unordered_map<pid_t, std::map<std::string, double>> monitoring_stats_;

    std::thread monitor_thread_;
    std::thread exit_thread_;
    std::thread multi_monitor_thread_;
    std::thread resource_monitor_thread_;
    std::thread auto_restart_thread_;
    std::thread watchdog_thread_;

    mutable std::mutex mutex_;
    std::condition_variable monitor_cv_;
    std::condition_variable exit_cv_;
    std::condition_variable multi_monitor_cv_;
    std::condition_variable resource_monitor_cv_;
    std::condition_variable auto_restart_cv_;
    std::condition_variable watchdog_cv_;
};

} // namespace atom::system

#endif