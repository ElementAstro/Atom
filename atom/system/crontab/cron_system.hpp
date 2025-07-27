#ifndef CRON_SYSTEM_HPP
#define CRON_SYSTEM_HPP

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "cron_job.hpp"

/**
 * @brief System platform enumeration
 */
enum class SystemPlatform {
    LINUX = 0,
    WINDOWS = 1,
    MACOS = 2,
    UNKNOWN = 3
};

/**
 * @brief Cron service type enumeration
 */
enum class CronServiceType {
    CRONTAB = 0,      // Traditional crontab
    SYSTEMD = 1,      // systemd timers
    WINDOWS_TASK = 2, // Windows Task Scheduler
    LAUNCHD = 3       // macOS launchd
};

/**
 * @brief System integration result with detailed error information
 */
struct SystemResult {
    bool success;
    std::string message;
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    std::chrono::milliseconds execution_time;

    SystemResult(bool s = false, std::string msg = "", int code = -1)
        : success(s), message(std::move(msg)), exit_code(code), execution_time(0) {}
};

/**
 * @brief Job monitoring information
 */
struct JobMonitorInfo {
    std::string job_id;
    std::chrono::system_clock::time_point last_execution;
    std::chrono::system_clock::time_point next_execution;
    bool is_running;
    int process_id;
    std::chrono::milliseconds execution_duration;
    int exit_code;
    std::string output;

    JobMonitorInfo(std::string id) : job_id(std::move(id)), is_running(false),
                                    process_id(-1), exit_code(-1) {}
};

/**
 * @brief Enhanced system-level cron operations with cross-platform support.
 *
 * Features:
 * - Cross-platform support (Linux, Windows, macOS)
 * - systemd timer integration for modern Linux systems
 * - Windows Task Scheduler integration
 * - macOS launchd integration
 * - Job monitoring and process tracking
 * - Better error handling and reporting
 * - Atomic operations for system modifications
 * - Background monitoring service
 */
class CronSystem {
public:
    /**
     * @brief Initializes the cron system with platform detection.
     * @return True if initialization successful, false otherwise.
     */
    static auto initialize() -> bool;

    /**
     * @brief Shuts down the cron system and cleanup resources.
     */
    static void shutdown();

    /**
     * @brief Gets the detected system platform.
     * @return Current system platform.
     */
    static auto getPlatform() -> SystemPlatform;

    /**
     * @brief Gets the preferred cron service type for current platform.
     * @return Preferred cron service type.
     */
    static auto getPreferredServiceType() -> CronServiceType;

    /**
     * @brief Sets the cron service type to use.
     * @param service_type Service type to use.
     * @return True if service type is supported, false otherwise.
     */
    static auto setServiceType(CronServiceType service_type) -> bool;

    // Enhanced job management methods
    /**
     * @brief Adds a job to the system with enhanced error reporting.
     * @param job The job to add.
     * @return Detailed result of the operation.
     */
    static auto addJobToSystem(const CronJob& job) -> SystemResult;

    /**
     * @brief Removes a job from the system with enhanced error reporting.
     * @param command The command of the job to remove.
     * @return Detailed result of the operation.
     */
    static auto removeJobFromSystem(const std::string& command) -> SystemResult;

    /**
     * @brief Lists all jobs from the system with enhanced parsing.
     * @return Vector of jobs from system crontab.
     */
    static auto listSystemJobs() -> std::vector<CronJob>;

    /**
     * @brief Exports enabled jobs to the system with atomic operation.
     * @param jobs Vector of jobs to export.
     * @return Detailed result of the operation.
     */
    static auto exportJobsToSystem(const std::vector<CronJob>& jobs) -> SystemResult;

    /**
     * @brief Clears all jobs from the system with backup.
     * @return Detailed result of the operation.
     */
    static auto clearSystemJobs() -> SystemResult;

    // Job monitoring and process tracking
    /**
     * @brief Starts monitoring a job execution.
     * @param job_id Unique identifier for the job.
     * @param command Command being executed.
     * @return True if monitoring started successfully.
     */
    static auto startJobMonitoring(const std::string& job_id, const std::string& command) -> bool;

    /**
     * @brief Stops monitoring a job execution.
     * @param job_id Unique identifier for the job.
     * @return Job monitoring information if available.
     */
    static auto stopJobMonitoring(const std::string& job_id) -> std::optional<JobMonitorInfo>;

    /**
     * @brief Gets monitoring information for a job.
     * @param job_id Unique identifier for the job.
     * @return Job monitoring information if available.
     */
    static auto getJobMonitorInfo(const std::string& job_id) -> std::optional<JobMonitorInfo>;

    /**
     * @brief Lists all currently monitored jobs.
     * @return Vector of job monitoring information.
     */
    static auto listMonitoredJobs() -> std::vector<JobMonitorInfo>;

    // System health and validation
    /**
     * @brief Validates system cron service availability.
     * @return True if cron service is available and working.
     */
    static auto validateCronService() -> bool;

    /**
     * @brief Gets system cron service status.
     * @return Detailed status information.
     */
    static auto getCronServiceStatus() -> SystemResult;

    /**
     * @brief Restarts the cron service (requires appropriate permissions).
     * @return Result of the restart operation.
     */
    static auto restartCronService() -> SystemResult;

    // Cross-platform specific methods
    /**
     * @brief Creates a systemd timer for a job (Linux only).
     * @param job The job to create timer for.
     * @return Result of the operation.
     */
    static auto createSystemdTimer(const CronJob& job) -> SystemResult;

    /**
     * @brief Creates a Windows scheduled task (Windows only).
     * @param job The job to create task for.
     * @return Result of the operation.
     */
    static auto createWindowsTask(const CronJob& job) -> SystemResult;

    /**
     * @brief Creates a launchd job (macOS only).
     * @param job The job to create for.
     * @return Result of the operation.
     */
    static auto createLaunchdJob(const CronJob& job) -> SystemResult;

    // Utility methods
    /**
     * @brief Executes a system command with timeout and monitoring.
     * @param command Command to execute.
     * @param timeout_seconds Timeout in seconds.
     * @return Detailed execution result.
     */
    static auto executeSystemCommand(const std::string& command,
                                   int timeout_seconds = 30) -> SystemResult;

    /**
     * @brief Backs up current system crontab.
     * @param backup_path Path to save backup.
     * @return True if backup successful.
     */
    static auto backupSystemCrontab(const std::string& backup_path) -> bool;

    /**
     * @brief Restores system crontab from backup.
     * @param backup_path Path to backup file.
     * @return Result of the restore operation.
     */
    static auto restoreSystemCrontab(const std::string& backup_path) -> SystemResult;

private:
    static SystemPlatform platform_;
    static CronServiceType service_type_;
    static std::atomic<bool> initialized_;
    static std::atomic<bool> monitoring_enabled_;
    static std::mutex monitor_mutex_;
    static std::unordered_map<std::string, JobMonitorInfo> monitored_jobs_;
    static std::thread monitor_thread_;

    // Internal helper methods
    static auto detectPlatform() -> SystemPlatform;
    static auto parseCrontabLine(const std::string& line) -> CronJob;
    static auto formatJobForCrontab(const CronJob& job) -> std::string;
    static auto formatJobForSystemd(const CronJob& job) -> std::string;
    static auto formatJobForWindows(const CronJob& job) -> std::string;
    static auto formatJobForLaunchd(const CronJob& job) -> std::string;
    static void monitoringLoop();
    static auto isServiceAvailable(CronServiceType service) -> bool;
};

#endif // CRON_SYSTEM_HPP
