/*
 * cron_monitor_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_MONITOR_TYPES_HPP
#define CRON_MONITOR_TYPES_HPP

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Monitoring event types
 */
enum class MonitorEventType {
    JOB_STARTED = 0,
    JOB_COMPLETED = 1,
    JOB_FAILED = 2,
    JOB_TIMEOUT = 3,
    JOB_SKIPPED = 4,
    SYSTEM_ERROR = 5,
    PERFORMANCE_WARNING = 6,
    RESOURCE_ALERT = 7
};

/**
 * @brief Monitoring event severity levels
 */
enum class EventSeverity {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief Monitoring event data
 */
struct MonitorEvent {
    std::string event_id;
    MonitorEventType type;
    EventSeverity severity;
    std::chrono::system_clock::time_point timestamp;
    std::string job_id;
    std::string message;
    std::unordered_map<std::string, std::string> metadata;

    MonitorEvent(std::string id, MonitorEventType t, EventSeverity s,
                 std::string j_id, std::string msg)
        : event_id(std::move(id)),
          type(t),
          severity(s),
          timestamp(std::chrono::system_clock::now()),
          job_id(std::move(j_id)),
          message(std::move(msg)) {}
};

/**
 * @brief Performance metrics for jobs and system
 */
struct PerformanceMetrics {
    std::string metric_id;
    std::chrono::system_clock::time_point timestamp;

    // Job-specific metrics
    std::chrono::milliseconds execution_time{0};
    std::chrono::milliseconds queue_time{0};
    size_t memory_usage_mb{0};
    double cpu_usage_percent{0.0};
    int exit_code{0};

    // System-wide metrics
    size_t total_jobs_running{0};
    size_t total_jobs_queued{0};
    double system_load_average{0.0};
    size_t system_memory_usage_mb{0};
    double system_cpu_usage_percent{0.0};

    PerformanceMetrics(std::string id)
        : metric_id(std::move(id)),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    std::string check_name;
    bool is_healthy;
    std::string status_message;
    std::chrono::system_clock::time_point last_check;
    std::chrono::milliseconds response_time{0};
    std::unordered_map<std::string, std::string> details;

    HealthCheckResult(std::string name, bool healthy, std::string msg)
        : check_name(std::move(name)),
          is_healthy(healthy),
          status_message(std::move(msg)),
          last_check(std::chrono::system_clock::now()) {}
};

/**
 * @brief Alert configuration
 */
struct AlertConfig {
    std::string alert_id;
    std::string name;
    std::string description;
    std::function<bool(const MonitorEvent&)> trigger_condition;
    std::function<bool(const PerformanceMetrics&)> metric_condition;
    std::chrono::minutes cooldown_period{5};
    std::vector<std::string> notification_channels;
    bool is_enabled{true};

    AlertConfig(std::string id, std::string n, std::string desc)
        : alert_id(std::move(id)),
          name(std::move(n)),
          description(std::move(desc)) {}
};

/**
 * @brief Alert instance
 */
struct Alert {
    std::string alert_id;
    std::string config_id;
    EventSeverity severity;
    std::chrono::system_clock::time_point triggered_at;
    std::string message;
    std::unordered_map<std::string, std::string> context;
    bool is_acknowledged{false};
    std::chrono::system_clock::time_point acknowledged_at;
    std::string acknowledged_by;

    Alert(std::string a_id, std::string c_id, EventSeverity sev,
          std::string msg)
        : alert_id(std::move(a_id)),
          config_id(std::move(c_id)),
          severity(sev),
          triggered_at(std::chrono::system_clock::now()),
          message(std::move(msg)) {}
};

#endif  // CRON_MONITOR_TYPES_HPP
