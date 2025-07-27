#include "cron_monitor.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>

#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;

CronMonitor::CronMonitor() {
    spdlog::info("CronMonitor initialized");
}

CronMonitor::~CronMonitor() {
    stop();
    spdlog::info("CronMonitor destroyed");
}

auto CronMonitor::start() -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    if (running_.load()) {
        spdlog::warn("CronMonitor is already running");
        return true;
    }
    
    try {
        running_.store(true);
        
        // Start monitoring thread
        monitor_thread_ = std::thread(&CronMonitor::monitorLoop, this);
        
        // Start health check thread
        health_check_thread_ = std::thread(&CronMonitor::healthCheckLoop, this);
        
        spdlog::info("CronMonitor started successfully");
        return true;
        
    } catch (const std::exception& e) {
        running_.store(false);
        spdlog::error("Failed to start CronMonitor: {}", e.what());
        return false;
    }
}

void CronMonitor::stop() {
    {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
    }
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
    
    spdlog::info("CronMonitor stopped");
}

auto CronMonitor::isRunning() const -> bool {
    return running_.load();
}

void CronMonitor::logEvent(const MonitorEvent& event) {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    // Add to event queue for processing
    event_queue_.push(event);
    
    // Log to spdlog based on severity
    switch (event.severity) {
        case EventSeverity::DEBUG:
            spdlog::debug("[{}] {}: {}", event.job_id, 
                         static_cast<int>(event.type), event.message);
            break;
        case EventSeverity::INFO:
            spdlog::info("[{}] {}: {}", event.job_id, 
                        static_cast<int>(event.type), event.message);
            break;
        case EventSeverity::WARNING:
            spdlog::warn("[{}] {}: {}", event.job_id, 
                        static_cast<int>(event.type), event.message);
            break;
        case EventSeverity::ERROR:
            spdlog::error("[{}] {}: {}", event.job_id, 
                         static_cast<int>(event.type), event.message);
            break;
        case EventSeverity::CRITICAL:
            spdlog::critical("[{}] {}: {}", event.job_id, 
                            static_cast<int>(event.type), event.message);
            break;
    }
}

void CronMonitor::logJobStart(const std::string& job_id, 
                             const std::unordered_map<std::string, std::string>& metadata) {
    MonitorEvent event(generateEventId(), MonitorEventType::JOB_STARTED, 
                      EventSeverity::INFO, job_id, "Job started");
    event.metadata = metadata;
    logEvent(event);
}

void CronMonitor::logJobCompletion(const std::string& job_id, bool success, 
                                  std::chrono::milliseconds execution_time,
                                  const std::unordered_map<std::string, std::string>& metadata) {
    MonitorEventType type = success ? MonitorEventType::JOB_COMPLETED : MonitorEventType::JOB_FAILED;
    EventSeverity severity = success ? EventSeverity::INFO : EventSeverity::ERROR;
    
    std::string message = success ? "Job completed successfully" : "Job failed";
    message += " (execution time: " + std::to_string(execution_time.count()) + "ms)";
    
    MonitorEvent event(generateEventId(), type, severity, job_id, message);
    event.metadata = metadata;
    event.metadata["execution_time_ms"] = std::to_string(execution_time.count());
    event.metadata["success"] = success ? "true" : "false";
    
    logEvent(event);
}

void CronMonitor::logSystemError(const std::string& error_message,
                                const std::unordered_map<std::string, std::string>& metadata) {
    MonitorEvent event(generateEventId(), MonitorEventType::SYSTEM_ERROR, 
                      EventSeverity::ERROR, "system", error_message);
    event.metadata = metadata;
    logEvent(event);
}

void CronMonitor::recordMetrics(const PerformanceMetrics& metrics) {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    if (!metrics.metric_id.empty() && metrics.metric_id != "system") {
        // Job-specific metrics
        auto& job_metrics = job_metrics_[metrics.metric_id];
        job_metrics.push_back(metrics);
        
        // Limit metrics history per job
        if (job_metrics.size() > MAX_METRICS_PER_JOB) {
            job_metrics.erase(job_metrics.begin(), 
                             job_metrics.begin() + (job_metrics.size() - MAX_METRICS_PER_JOB));
        }
    } else {
        // System-wide metrics
        system_metrics_.push_back(metrics);
        
        // Limit system metrics history
        if (system_metrics_.size() > MAX_SYSTEM_METRICS) {
            system_metrics_.erase(system_metrics_.begin(), 
                                 system_metrics_.begin() + (system_metrics_.size() - MAX_SYSTEM_METRICS));
        }
    }
    
    // Check for metric-based alerts
    checkMetricAlerts(metrics);
}

auto CronMonitor::getJobMetrics(const std::string& job_id, std::chrono::minutes duration)
    -> std::vector<PerformanceMetrics> {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    auto it = job_metrics_.find(job_id);
    if (it == job_metrics_.end()) {
        return {};
    }
    
    auto cutoff_time = std::chrono::system_clock::now() - duration;
    std::vector<PerformanceMetrics> result;
    
    std::copy_if(it->second.begin(), it->second.end(), std::back_inserter(result),
                [cutoff_time](const PerformanceMetrics& metric) {
                    return metric.timestamp >= cutoff_time;
                });
    
    return result;
}

auto CronMonitor::getSystemMetrics(std::chrono::minutes duration) -> std::vector<PerformanceMetrics> {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    auto cutoff_time = std::chrono::system_clock::now() - duration;
    std::vector<PerformanceMetrics> result;
    
    std::copy_if(system_metrics_.begin(), system_metrics_.end(), std::back_inserter(result),
                [cutoff_time](const PerformanceMetrics& metric) {
                    return metric.timestamp >= cutoff_time;
                });
    
    return result;
}

auto CronMonitor::registerHealthCheck(const std::string& check_name,
                                     std::function<HealthCheckResult()> check_function,
                                     std::chrono::minutes interval) -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    if (health_checks_.find(check_name) != health_checks_.end()) {
        spdlog::warn("Health check {} already exists", check_name);
        return false;
    }
    
    health_checks_.emplace(check_name, HealthCheck(std::move(check_function), interval));
    spdlog::info("Registered health check: {} (interval: {}min)", check_name, interval.count());
    
    return true;
}

auto CronMonitor::unregisterHealthCheck(const std::string& check_name) -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    auto it = health_checks_.find(check_name);
    if (it != health_checks_.end()) {
        health_checks_.erase(it);
        spdlog::info("Unregistered health check: {}", check_name);
        return true;
    }
    
    spdlog::warn("Health check {} not found", check_name);
    return false;
}

auto CronMonitor::getHealthStatus() -> std::unordered_map<std::string, HealthCheckResult> {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    std::unordered_map<std::string, HealthCheckResult> result;
    for (const auto& [name, check] : health_checks_) {
        result[name] = check.last_result;
    }
    
    return result;
}

auto CronMonitor::runHealthChecks() -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    bool overall_healthy = true;
    auto now = std::chrono::system_clock::now();
    
    for (auto& [name, check] : health_checks_) {
        try {
            auto start_time = std::chrono::steady_clock::now();
            auto result = check.check_function();
            auto end_time = std::chrono::steady_clock::now();
            
            result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            result.last_check = now;
            
            check.last_result = result;
            check.last_run = now;
            
            if (!result.is_healthy) {
                overall_healthy = false;
                spdlog::warn("Health check {} failed: {}", name, result.status_message);
            } else {
                spdlog::debug("Health check {} passed: {}", name, result.status_message);
            }
            
        } catch (const std::exception& e) {
            overall_healthy = false;
            check.last_result = HealthCheckResult(name, false, 
                                                 "Exception: " + std::string(e.what()));
            check.last_result.last_check = now;
            check.last_run = now;
            
            spdlog::error("Health check {} threw exception: {}", name, e.what());
        }
    }
    
    return overall_healthy;
}

auto CronMonitor::addAlertConfig(const AlertConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    if (alert_configs_.find(config.alert_id) != alert_configs_.end()) {
        spdlog::warn("Alert config {} already exists", config.alert_id);
        return false;
    }
    
    alert_configs_[config.alert_id] = config;
    spdlog::info("Added alert config: {} - {}", config.alert_id, config.name);
    
    return true;
}

auto CronMonitor::removeAlertConfig(const std::string& alert_id) -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    auto it = alert_configs_.find(alert_id);
    if (it != alert_configs_.end()) {
        alert_configs_.erase(it);
        spdlog::info("Removed alert config: {}", alert_id);
        return true;
    }
    
    spdlog::warn("Alert config {} not found", alert_id);
    return false;
}

auto CronMonitor::getActiveAlerts() -> std::vector<Alert> {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    std::vector<Alert> result;
    std::copy_if(active_alerts_.begin(), active_alerts_.end(), std::back_inserter(result),
                [](const Alert& alert) {
                    return !alert.is_acknowledged;
                });
    
    return result;
}

auto CronMonitor::acknowledgeAlert(const std::string& alert_id, const std::string& acknowledged_by) -> bool {
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    auto it = std::find_if(active_alerts_.begin(), active_alerts_.end(),
                          [&alert_id](const Alert& alert) {
                              return alert.alert_id == alert_id;
                          });
    
    if (it != active_alerts_.end()) {
        it->is_acknowledged = true;
        it->acknowledged_at = std::chrono::system_clock::now();
        it->acknowledged_by = acknowledged_by;
        
        spdlog::info("Alert {} acknowledged by {}", alert_id, acknowledged_by);
        return true;
    }
    
    spdlog::warn("Alert {} not found", alert_id);
    return false;
}

auto CronMonitor::queryEvents(std::chrono::system_clock::time_point start_time,
                             std::chrono::system_clock::time_point end_time,
                             const std::vector<MonitorEventType>& event_types,
                             const std::vector<std::string>& job_ids) -> std::vector<MonitorEvent> {
    std::lock_guard<std::mutex> lock(monitor_mutex_);

    std::vector<MonitorEvent> result;

    for (const auto& event : event_history_) {
        // Check time range
        if (event.timestamp < start_time || event.timestamp > end_time) {
            continue;
        }

        // Check event types filter
        if (!event_types.empty()) {
            if (std::find(event_types.begin(), event_types.end(), event.type) == event_types.end()) {
                continue;
            }
        }

        // Check job IDs filter
        if (!job_ids.empty()) {
            if (std::find(job_ids.begin(), job_ids.end(), event.job_id) == job_ids.end()) {
                continue;
            }
        }

        result.push_back(event);
    }

    // Sort by timestamp
    std::sort(result.begin(), result.end(),
             [](const MonitorEvent& a, const MonitorEvent& b) {
                 return a.timestamp < b.timestamp;
             });

    return result;
}

auto CronMonitor::generateReport(std::chrono::hours duration) -> std::string {
    auto end_time = std::chrono::system_clock::now();
    auto start_time = end_time - duration;

    auto events = queryEvents(start_time, end_time);
    auto system_metrics = getSystemMetrics(std::chrono::duration_cast<std::chrono::minutes>(duration));
    auto health_status = getHealthStatus();
    auto active_alerts = getActiveAlerts();

    json report;
    report["report_generated"] = std::chrono::duration_cast<std::chrono::seconds>(
        end_time.time_since_epoch()).count();
    report["duration_hours"] = duration.count();

    // Event summary
    std::unordered_map<MonitorEventType, int> event_counts;
    for (const auto& event : events) {
        event_counts[event.type]++;
    }

    json event_summary;
    for (const auto& [type, count] : event_counts) {
        event_summary[std::to_string(static_cast<int>(type))] = count;
    }
    report["event_summary"] = event_summary;

    // Health status
    json health_json;
    bool overall_healthy = true;
    for (const auto& [name, result] : health_status) {
        health_json[name] = {
            {"healthy", result.is_healthy},
            {"message", result.status_message},
            {"last_check", std::chrono::duration_cast<std::chrono::seconds>(
                result.last_check.time_since_epoch()).count()},
            {"response_time_ms", result.response_time.count()}
        };
        if (!result.is_healthy) {
            overall_healthy = false;
        }
    }
    report["health_status"] = health_json;
    report["overall_healthy"] = overall_healthy;

    // Active alerts
    json alerts_json = json::array();
    for (const auto& alert : active_alerts) {
        alerts_json.push_back({
            {"alert_id", alert.alert_id},
            {"severity", static_cast<int>(alert.severity)},
            {"message", alert.message},
            {"triggered_at", std::chrono::duration_cast<std::chrono::seconds>(
                alert.triggered_at.time_since_epoch()).count()}
        });
    }
    report["active_alerts"] = alerts_json;

    return report.dump(2);
}

auto CronMonitor::exportData(const std::string& filename, const std::string& format,
                            std::chrono::hours duration) -> bool {
    try {
        auto end_time = std::chrono::system_clock::now();
        auto start_time = end_time - duration;

        auto events = queryEvents(start_time, end_time);

        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for export: {}", filename);
            return false;
        }

        if (format == "json") {
            json export_data = json::array();
            for (const auto& event : events) {
                json event_json = {
                    {"event_id", event.event_id},
                    {"type", static_cast<int>(event.type)},
                    {"severity", static_cast<int>(event.severity)},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                        event.timestamp.time_since_epoch()).count()},
                    {"job_id", event.job_id},
                    {"message", event.message},
                    {"metadata", event.metadata}
                };
                export_data.push_back(event_json);
            }
            file << export_data.dump(2);

        } else if (format == "csv") {
            // CSV header
            file << "event_id,type,severity,timestamp,job_id,message\n";

            for (const auto& event : events) {
                file << event.event_id << ","
                     << static_cast<int>(event.type) << ","
                     << static_cast<int>(event.severity) << ","
                     << std::chrono::duration_cast<std::chrono::seconds>(
                         event.timestamp.time_since_epoch()).count() << ","
                     << event.job_id << ","
                     << "\"" << event.message << "\"\n";
            }

        } else {
            spdlog::error("Unsupported export format: {}", format);
            return false;
        }

        file.close();
        spdlog::info("Exported {} events to {} (format: {})", events.size(), filename, format);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to export data: {}", e.what());
        return false;
    }
}

void CronMonitor::monitorLoop() {
    spdlog::info("CronMonitor main loop started");

    while (running_.load()) {
        try {
            processEventQueue();
            cleanupOldData();

            std::this_thread::sleep_for(std::chrono::seconds(1));

        } catch (const std::exception& e) {
            spdlog::error("Exception in monitor loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    spdlog::info("CronMonitor main loop stopped");
}

void CronMonitor::healthCheckLoop() {
    spdlog::info("CronMonitor health check loop started");

    while (running_.load()) {
        try {
            std::lock_guard<std::mutex> lock(monitor_mutex_);
            auto now = std::chrono::system_clock::now();

            for (auto& [name, check] : health_checks_) {
                if (now - check.last_run >= check.interval) {
                    try {
                        auto start_time = std::chrono::steady_clock::now();
                        auto result = check.check_function();
                        auto end_time = std::chrono::steady_clock::now();

                        result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time);
                        result.last_check = now;

                        check.last_result = result;
                        check.last_run = now;

                        if (!result.is_healthy) {
                            spdlog::warn("Health check {} failed: {}", name, result.status_message);
                        }

                    } catch (const std::exception& e) {
                        check.last_result = HealthCheckResult(name, false,
                                                             "Exception: " + std::string(e.what()));
                        check.last_result.last_check = now;
                        check.last_run = now;

                        spdlog::error("Health check {} threw exception: {}", name, e.what());
                    }
                }
            }

        } catch (const std::exception& e) {
            spdlog::error("Exception in health check loop: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    spdlog::info("CronMonitor health check loop stopped");
}

void CronMonitor::processEventQueue() {
    std::lock_guard<std::mutex> lock(monitor_mutex_);

    while (!event_queue_.empty()) {
        auto event = event_queue_.front();
        event_queue_.pop();

        // Add to history
        event_history_.push_back(event);

        // Limit history size
        if (event_history_.size() > MAX_EVENT_HISTORY) {
            event_history_.erase(event_history_.begin(),
                                event_history_.begin() + (event_history_.size() - MAX_EVENT_HISTORY));
        }

        // Check alert conditions
        checkAlertConditions(event);
    }
}

void CronMonitor::checkAlertConditions(const MonitorEvent& event) {
    auto now = std::chrono::system_clock::now();

    for (const auto& [config_id, config] : alert_configs_) {
        if (!config.is_enabled) {
            continue;
        }

        // Check cooldown
        auto cooldown_it = alert_cooldowns_.find(config_id);
        if (cooldown_it != alert_cooldowns_.end()) {
            if (now - cooldown_it->second < config.cooldown_period) {
                continue;
            }
        }

        // Check trigger condition
        try {
            if (config.trigger_condition && config.trigger_condition(event)) {
                triggerAlert(config, "Event-triggered alert: " + event.message);
                alert_cooldowns_[config_id] = now;
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in alert condition {}: {}", config_id, e.what());
        }
    }
}

void CronMonitor::checkMetricAlerts(const PerformanceMetrics& metrics) {
    auto now = std::chrono::system_clock::now();

    for (const auto& [config_id, config] : alert_configs_) {
        if (!config.is_enabled || !config.metric_condition) {
            continue;
        }

        // Check cooldown
        auto cooldown_it = alert_cooldowns_.find(config_id);
        if (cooldown_it != alert_cooldowns_.end()) {
            if (now - cooldown_it->second < config.cooldown_period) {
                continue;
            }
        }

        // Check metric condition
        try {
            if (config.metric_condition(metrics)) {
                triggerAlert(config, "Metric-triggered alert for " + metrics.metric_id);
                alert_cooldowns_[config_id] = now;
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in metric alert condition {}: {}", config_id, e.what());
        }
    }
}

void CronMonitor::triggerAlert(const AlertConfig& config, const std::string& message,
                              const std::unordered_map<std::string, std::string>& context) {
    Alert alert(generateAlertId(), config.alert_id, EventSeverity::WARNING, message);
    alert.context = context;

    active_alerts_.push_back(alert);

    spdlog::warn("Alert triggered: {} - {}", config.name, message);

    // TODO: Implement notification channels (email, webhook, etc.)
}

void CronMonitor::cleanupOldData() {
    // This method can be enhanced to clean up old metrics and events
    // based on configurable retention policies
}

auto CronMonitor::generateEventId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    return "evt_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

auto CronMonitor::generateAlertId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    return "alert_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}
