#ifndef CRON_MONITOR_HPP
#define CRON_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cron_monitor_types.hpp"

/**
 * @brief Comprehensive monitoring and logging system for cron jobs
 */
class CronMonitor {
public:
    /**
     * @brief Constructs a new CronMonitor
     */
    CronMonitor();

    /**
     * @brief Destructor
     */
    ~CronMonitor();

    // Disable copy operations
    CronMonitor(const CronMonitor&) = delete;
    CronMonitor& operator=(const CronMonitor&) = delete;

    // Enable move operations
    CronMonitor(CronMonitor&&) noexcept = default;
    CronMonitor& operator=(CronMonitor&&) noexcept = default;

    /**
     * @brief Starts the monitoring system
     * @return True if started successfully
     */
    auto start() -> bool;

    /**
     * @brief Stops the monitoring system
     */
    void stop();

    /**
     * @brief Checks if monitoring is active
     * @return True if monitoring is running
     */
    auto isRunning() const -> bool;

    // Event logging
    /**
     * @brief Logs a monitoring event
     * @param event Event to log
     */
    void logEvent(const MonitorEvent& event);

    /**
     * @brief Logs a job start event
     * @param job_id Job identifier
     * @param metadata Additional metadata
     */
    void logJobStart(const std::string& job_id,
                    const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Logs a job completion event
     * @param job_id Job identifier
     * @param success Whether job completed successfully
     * @param execution_time Time taken to execute
     * @param metadata Additional metadata
     */
    void logJobCompletion(const std::string& job_id, bool success,
                         std::chrono::milliseconds execution_time,
                         const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Logs a system error event
     * @param error_message Error description
     * @param metadata Additional context
     */
    void logSystemError(const std::string& error_message,
                       const std::unordered_map<std::string, std::string>& metadata = {});

    // Performance monitoring
    /**
     * @brief Records performance metrics
     * @param metrics Performance metrics to record
     */
    void recordMetrics(const PerformanceMetrics& metrics);

    /**
     * @brief Gets recent performance metrics for a job
     * @param job_id Job identifier
     * @param duration Time range to query
     * @return Vector of performance metrics
     */
    auto getJobMetrics(const std::string& job_id,
                      std::chrono::minutes duration = std::chrono::minutes(60))
        -> std::vector<PerformanceMetrics>;

    /**
     * @brief Gets system-wide performance metrics
     * @param duration Time range to query
     * @return Vector of system metrics
     */
    auto getSystemMetrics(std::chrono::minutes duration = std::chrono::minutes(60))
        -> std::vector<PerformanceMetrics>;

    // Health checks
    /**
     * @brief Registers a health check
     * @param check_name Name of the health check
     * @param check_function Function to perform the check
     * @param interval How often to run the check
     * @return True if registered successfully
     */
    auto registerHealthCheck(const std::string& check_name,
                            std::function<HealthCheckResult()> check_function,
                            std::chrono::minutes interval = std::chrono::minutes(5)) -> bool;

    /**
     * @brief Unregisters a health check
     * @param check_name Name of the health check to remove
     * @return True if removed successfully
     */
    auto unregisterHealthCheck(const std::string& check_name) -> bool;

    /**
     * @brief Gets the latest health check results
     * @return Map of health check results
     */
    auto getHealthStatus() -> std::unordered_map<std::string, HealthCheckResult>;

    /**
     * @brief Runs all health checks immediately
     * @return Overall health status
     */
    auto runHealthChecks() -> bool;

    // Alerting
    /**
     * @brief Adds an alert configuration
     * @param config Alert configuration
     * @return True if added successfully
     */
    auto addAlertConfig(const AlertConfig& config) -> bool;

    /**
     * @brief Removes an alert configuration
     * @param alert_id Alert configuration ID
     * @return True if removed successfully
     */
    auto removeAlertConfig(const std::string& alert_id) -> bool;

    /**
     * @brief Gets active alerts
     * @return Vector of active alerts
     */
    auto getActiveAlerts() -> std::vector<Alert>;

    /**
     * @brief Acknowledges an alert
     * @param alert_id Alert ID
     * @param acknowledged_by Who acknowledged the alert
     * @return True if acknowledged successfully
     */
    auto acknowledgeAlert(const std::string& alert_id, const std::string& acknowledged_by) -> bool;

    // Query and reporting
    /**
     * @brief Queries events by criteria
     * @param start_time Start of time range
     * @param end_time End of time range
     * @param event_types Event types to include (empty = all)
     * @param job_ids Job IDs to include (empty = all)
     * @return Vector of matching events
     */
    auto queryEvents(std::chrono::system_clock::time_point start_time,
                    std::chrono::system_clock::time_point end_time,
                    const std::vector<MonitorEventType>& event_types = {},
                    const std::vector<std::string>& job_ids = {}) -> std::vector<MonitorEvent>;

    /**
     * @brief Generates a monitoring report
     * @param duration Time range for the report
     * @return JSON-formatted report
     */
    auto generateReport(std::chrono::hours duration = std::chrono::hours(24)) -> std::string;

    /**
     * @brief Exports monitoring data to file
     * @param filename Output filename
     * @param format Export format ("json", "csv", "xml")
     * @param duration Time range to export
     * @return True if exported successfully
     */
    auto exportData(const std::string& filename, const std::string& format = "json",
                   std::chrono::hours duration = std::chrono::hours(24)) -> bool;

private:
    mutable std::mutex monitor_mutex_;
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
    std::thread health_check_thread_;

    // Event storage
    std::queue<MonitorEvent> event_queue_;
    std::vector<MonitorEvent> event_history_;
    static constexpr size_t MAX_EVENT_HISTORY = 10000;

    // Metrics storage
    std::unordered_map<std::string, std::vector<PerformanceMetrics>> job_metrics_;
    std::vector<PerformanceMetrics> system_metrics_;
    static constexpr size_t MAX_METRICS_PER_JOB = 1000;
    static constexpr size_t MAX_SYSTEM_METRICS = 1000;

    // Health checks
    struct HealthCheck {
        std::function<HealthCheckResult()> check_function;
        std::chrono::minutes interval;
        std::chrono::system_clock::time_point last_run;
        HealthCheckResult last_result;

        HealthCheck(std::function<HealthCheckResult()> func, std::chrono::minutes intv)
            : check_function(std::move(func)), interval(intv),
              last_run(std::chrono::system_clock::time_point::min()),
              last_result("", false, "Not run yet") {}
    };
    std::unordered_map<std::string, HealthCheck> health_checks_;

    // Alerting
    std::unordered_map<std::string, AlertConfig> alert_configs_;
    std::vector<Alert> active_alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> alert_cooldowns_;

    // Internal methods
    void monitorLoop();
    void healthCheckLoop();
    void processEventQueue();
    void checkAlertConditions(const MonitorEvent& event);
    void checkMetricAlerts(const PerformanceMetrics& metrics);
    void triggerAlert(const AlertConfig& config, const std::string& message,
                     const std::unordered_map<std::string, std::string>& context = {});
    void cleanupOldData();
    auto generateEventId() -> std::string;
    auto generateAlertId() -> std::string;
};

#endif // CRON_MONITOR_HPP
