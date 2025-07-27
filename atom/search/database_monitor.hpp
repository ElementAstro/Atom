/**
 * @file database_monitor.hpp
 * @brief Database monitoring and health check utilities
 * @date 2025-07-24
 */

#ifndef ATOM_SEARCH_DATABASE_MONITOR_HPP
#define ATOM_SEARCH_DATABASE_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "database_base.hpp"

namespace atom::database {

/**
 * @brief Database health status
 */
enum class DatabaseHealth {
    HEALTHY,
    DEGRADED,
    CRITICAL,
    OFFLINE
};

/**
 * @brief Database health report
 */
struct HealthReport {
    DatabaseHealth status;
    std::string database_type;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> details;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    bool is_healthy() const {
        return status == DatabaseHealth::HEALTHY;
    }
    
    std::string to_string() const {
        std::string result = "Database Health Report:\n";
        result += "  Type: " + database_type + "\n";
        result += "  Status: " + health_to_string(status) + "\n";
        
        if (!warnings.empty()) {
            result += "  Warnings:\n";
            for (const auto& warning : warnings) {
                result += "    - " + warning + "\n";
            }
        }
        
        if (!errors.empty()) {
            result += "  Errors:\n";
            for (const auto& error : errors) {
                result += "    - " + error + "\n";
            }
        }
        
        return result;
    }

private:
    std::string health_to_string(DatabaseHealth health) const {
        switch (health) {
            case DatabaseHealth::HEALTHY: return "HEALTHY";
            case DatabaseHealth::DEGRADED: return "DEGRADED";
            case DatabaseHealth::CRITICAL: return "CRITICAL";
            case DatabaseHealth::OFFLINE: return "OFFLINE";
            default: return "UNKNOWN";
        }
    }
};

/**
 * @brief Performance metrics aggregator
 */
class PerformanceMetrics {
public:
    void record_query_time(std::chrono::nanoseconds duration) {
        total_query_time_ += duration.count();
        query_count_++;
        
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        if (duration_ms > max_query_time_ms_) {
            max_query_time_ms_ = duration_ms;
        }
        
        // Update histogram
        if (duration_ms < 10) {
            fast_queries_++;
        } else if (duration_ms < 100) {
            medium_queries_++;
        } else if (duration_ms < 1000) {
            slow_queries_++;
        } else {
            very_slow_queries_++;
        }
    }
    
    void record_connection_event(bool acquired) {
        if (acquired) {
            successful_connections_++;
        } else {
            failed_connections_++;
        }
    }
    
    void record_error() {
        error_count_++;
    }
    
    std::unordered_map<std::string, double> get_metrics() const {
        std::unordered_map<std::string, double> metrics;
        
        uint64_t total_queries = query_count_.load();
        if (total_queries > 0) {
            metrics["avg_query_time_ms"] = static_cast<double>(total_query_time_.load()) / 
                                          (total_queries * 1000000.0);
            metrics["max_query_time_ms"] = max_query_time_ms_.load();
            metrics["queries_per_second"] = calculate_qps();
        }
        
        uint64_t total_connections = successful_connections_.load() + failed_connections_.load();
        if (total_connections > 0) {
            metrics["connection_success_rate"] = static_cast<double>(successful_connections_.load()) / 
                                                total_connections;
        }
        
        metrics["error_rate"] = total_queries > 0 ? 
            static_cast<double>(error_count_.load()) / total_queries : 0.0;
        
        // Query distribution
        metrics["fast_query_percentage"] = total_queries > 0 ? 
            static_cast<double>(fast_queries_.load()) / total_queries * 100.0 : 0.0;
        metrics["slow_query_percentage"] = total_queries > 0 ? 
            static_cast<double>(slow_queries_.load() + very_slow_queries_.load()) / total_queries * 100.0 : 0.0;
        
        return metrics;
    }
    
    void reset() {
        total_query_time_ = 0;
        query_count_ = 0;
        max_query_time_ms_ = 0;
        successful_connections_ = 0;
        failed_connections_ = 0;
        error_count_ = 0;
        fast_queries_ = 0;
        medium_queries_ = 0;
        slow_queries_ = 0;
        very_slow_queries_ = 0;
        start_time_ = std::chrono::steady_clock::now();
    }

private:
    double calculate_qps() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        return duration > 0 ? static_cast<double>(query_count_.load()) / duration : 0.0;
    }
    
    std::atomic<uint64_t> total_query_time_{0};
    std::atomic<uint64_t> query_count_{0};
    std::atomic<uint64_t> max_query_time_ms_{0};
    std::atomic<uint64_t> successful_connections_{0};
    std::atomic<uint64_t> failed_connections_{0};
    std::atomic<uint64_t> error_count_{0};
    
    // Query time distribution
    std::atomic<uint64_t> fast_queries_{0};      // < 10ms
    std::atomic<uint64_t> medium_queries_{0};    // 10-100ms
    std::atomic<uint64_t> slow_queries_{0};      // 100ms-1s
    std::atomic<uint64_t> very_slow_queries_{0}; // > 1s
    
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
};

/**
 * @brief Database health monitor
 */
class DatabaseHealthMonitor {
public:
    using HealthCheckFunction = std::function<HealthReport()>;
    
    DatabaseHealthMonitor(std::chrono::seconds check_interval = std::chrono::seconds(60))
        : check_interval_(check_interval), running_(false) {}
    
    ~DatabaseHealthMonitor() {
        stop();
    }
    
    void add_database(const std::string& name, HealthCheckFunction health_check) {
        std::lock_guard<std::mutex> lock(mutex_);
        health_checks_[name] = std::move(health_check);
    }
    
    void remove_database(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        health_checks_.erase(name);
        last_reports_.erase(name);
    }
    
    void start() {
        if (running_.exchange(true)) {
            return;  // Already running
        }
        
        monitor_thread_ = std::thread([this] {
            while (running_) {
                perform_health_checks();
                std::this_thread::sleep_for(check_interval_);
            }
        });
    }
    
    void stop() {
        if (!running_.exchange(false)) {
            return;  // Already stopped
        }
        
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
    
    std::unordered_map<std::string, HealthReport> get_health_reports() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_reports_;
    }
    
    HealthReport get_health_report(const std::string& database_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = last_reports_.find(database_name);
        if (it != last_reports_.end()) {
            return it->second;
        }
        
        // Return default unhealthy report if not found
        HealthReport report;
        report.status = DatabaseHealth::OFFLINE;
        report.database_type = "unknown";
        report.timestamp = std::chrono::steady_clock::now();
        report.errors.push_back("Database not found in monitor");
        return report;
    }
    
    bool is_all_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, report] : last_reports_) {
            if (!report.is_healthy()) {
                return false;
            }
        }
        return true;
    }
    
    void set_alert_callback(std::function<void(const std::string&, const HealthReport&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        alert_callback_ = std::move(callback);
    }

private:
    void perform_health_checks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, health_check] : health_checks_) {
            try {
                auto report = health_check();
                report.timestamp = std::chrono::steady_clock::now();
                
                // Check if status changed
                auto it = last_reports_.find(name);
                bool status_changed = (it == last_reports_.end()) || 
                                     (it->second.status != report.status);
                
                last_reports_[name] = report;
                
                // Trigger alert if status changed to unhealthy
                if (status_changed && !report.is_healthy() && alert_callback_) {
                    alert_callback_(name, report);
                }
                
            } catch (const std::exception& e) {
                HealthReport error_report;
                error_report.status = DatabaseHealth::OFFLINE;
                error_report.database_type = "unknown";
                error_report.timestamp = std::chrono::steady_clock::now();
                error_report.errors.push_back("Health check failed: " + std::string(e.what()));
                
                last_reports_[name] = error_report;
                
                if (alert_callback_) {
                    alert_callback_(name, error_report);
                }
            }
        }
    }
    
    std::chrono::seconds check_interval_;
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, HealthCheckFunction> health_checks_;
    std::unordered_map<std::string, HealthReport> last_reports_;
    std::function<void(const std::string&, const HealthReport&)> alert_callback_;
};

/**
 * @brief Database performance profiler
 */
class DatabaseProfiler {
public:
    struct QueryProfile {
        std::string query;
        std::chrono::nanoseconds total_time{0};
        uint64_t execution_count{0};
        std::chrono::nanoseconds min_time{std::chrono::nanoseconds::max()};
        std::chrono::nanoseconds max_time{0};
        
        double get_average_time_ms() const {
            return execution_count > 0 ? 
                std::chrono::duration<double, std::milli>(total_time).count() / execution_count : 0.0;
        }
    };
    
    void record_query(const std::string& query, std::chrono::nanoseconds duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& profile = query_profiles_[query];
        profile.query = query;
        profile.total_time += duration;
        profile.execution_count++;
        profile.min_time = std::min(profile.min_time, duration);
        profile.max_time = std::max(profile.max_time, duration);
    }
    
    std::vector<QueryProfile> get_top_queries(size_t limit = 10) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<QueryProfile> profiles;
        profiles.reserve(query_profiles_.size());
        
        for (const auto& [query, profile] : query_profiles_) {
            profiles.push_back(profile);
        }
        
        // Sort by total time descending
        std::sort(profiles.begin(), profiles.end(), 
                  [](const QueryProfile& a, const QueryProfile& b) {
                      return a.total_time > b.total_time;
                  });
        
        if (profiles.size() > limit) {
            profiles.resize(limit);
        }
        
        return profiles;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        query_profiles_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, QueryProfile> query_profiles_;
};

}  // namespace atom::database

#endif  // ATOM_SEARCH_DATABASE_MONITOR_HPP
