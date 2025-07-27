/**
 * @file monitor.hpp
 * @brief System monitoring and metrics collection for UV components
 * @version 1.0
 */

#ifndef ATOM_EXTRA_UV_MONITOR_HPP
#define ATOM_EXTRA_UV_MONITOR_HPP

#include <uv.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <functional>
#include <thread>
#include <queue>
#include <fstream>
#include <optional>

namespace uv_monitor {

/**
 * @struct SystemMetrics
 * @brief System-wide performance metrics
 */
struct SystemMetrics {
    // CPU metrics
    double cpu_usage_percent = 0.0;
    double load_average_1m = 0.0;
    double load_average_5m = 0.0;
    double load_average_15m = 0.0;
    uint32_t cpu_count = 0;
    
    // Memory metrics
    uint64_t total_memory = 0;
    uint64_t free_memory = 0;
    uint64_t available_memory = 0;
    uint64_t used_memory = 0;
    double memory_usage_percent = 0.0;
    
    // Process metrics
    uint64_t process_count = 0;
    uint64_t thread_count = 0;
    uint64_t handle_count = 0;
    
    // Network metrics
    uint64_t network_bytes_sent = 0;
    uint64_t network_bytes_received = 0;
    uint64_t network_packets_sent = 0;
    uint64_t network_packets_received = 0;
    
    // Disk I/O metrics
    uint64_t disk_bytes_read = 0;
    uint64_t disk_bytes_written = 0;
    uint64_t disk_reads = 0;
    uint64_t disk_writes = 0;
    
    // System uptime
    std::chrono::seconds uptime{0};
    
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
};

/**
 * @struct ProcessMetrics
 * @brief Process-specific performance metrics
 */
struct ProcessMetrics {
    int pid = 0;
    std::string name;
    
    // CPU metrics
    double cpu_usage_percent = 0.0;
    uint64_t cpu_time_user = 0;
    uint64_t cpu_time_system = 0;
    
    // Memory metrics
    uint64_t memory_rss = 0;        // Resident Set Size
    uint64_t memory_vms = 0;        // Virtual Memory Size
    uint64_t memory_shared = 0;     // Shared memory
    uint64_t memory_text = 0;       // Text (code) memory
    uint64_t memory_data = 0;       // Data memory
    
    // I/O metrics
    uint64_t io_bytes_read = 0;
    uint64_t io_bytes_written = 0;
    uint64_t io_read_ops = 0;
    uint64_t io_write_ops = 0;
    
    // File descriptor metrics
    uint32_t open_files = 0;
    uint32_t max_files = 0;
    
    // Thread metrics
    uint32_t thread_count = 0;
    
    // Context switches
    uint64_t voluntary_context_switches = 0;
    uint64_t involuntary_context_switches = 0;
    
    // Process state
    std::string state;
    int priority = 0;
    int nice_value = 0;
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
};

/**
 * @struct UvLoopMetrics
 * @brief libuv event loop specific metrics
 */
struct UvLoopMetrics {
    // Loop statistics
    uint64_t iteration_count = 0;
    std::chrono::microseconds avg_iteration_time{0};
    std::chrono::microseconds max_iteration_time{0};
    std::chrono::microseconds min_iteration_time{std::chrono::microseconds::max()};
    
    // Handle counts
    uint32_t active_handles = 0;
    uint32_t active_requests = 0;
    uint32_t total_handles = 0;
    uint32_t total_requests = 0;
    
    // Handle type breakdown
    uint32_t tcp_handles = 0;
    uint32_t udp_handles = 0;
    uint32_t pipe_handles = 0;
    uint32_t timer_handles = 0;
    uint32_t async_handles = 0;
    uint32_t fs_handles = 0;
    uint32_t process_handles = 0;
    
    // Event loop health
    bool is_alive = false;
    bool is_running = false;
    std::chrono::steady_clock::time_point last_activity;
    
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
};

/**
 * @class MetricsCollector
 * @brief Base class for metrics collection
 */
class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;
    virtual void collect() = 0;
    virtual std::string get_name() const = 0;
    virtual bool is_enabled() const { return enabled_; }
    virtual void set_enabled(bool enabled) { enabled_ = enabled; }

protected:
    std::atomic<bool> enabled_{true};
};

/**
 * @class SystemMetricsCollector
 * @brief Collects system-wide metrics
 */
class SystemMetricsCollector : public MetricsCollector {
public:
    SystemMetricsCollector();
    
    void collect() override;
    std::string get_name() const override { return "system"; }
    
    SystemMetrics get_latest() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return latest_metrics_;
    }
    
    std::vector<SystemMetrics> get_history(size_t count = 0) const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (count == 0 || count > history_.size()) {
            return history_;
        }
        return std::vector<SystemMetrics>(history_.end() - count, history_.end());
    }

private:
    mutable std::mutex metrics_mutex_;
    SystemMetrics latest_metrics_;
    std::vector<SystemMetrics> history_;
    size_t max_history_size_ = 1000;
    
    void collect_cpu_metrics();
    void collect_memory_metrics();
    void collect_network_metrics();
    void collect_disk_metrics();
};

/**
 * @class ProcessMetricsCollector
 * @brief Collects process-specific metrics
 */
class ProcessMetricsCollector : public MetricsCollector {
public:
    explicit ProcessMetricsCollector(int pid = 0); // 0 = current process
    
    void collect() override;
    std::string get_name() const override { return "process_" + std::to_string(pid_); }
    
    ProcessMetrics get_latest() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return latest_metrics_;
    }
    
    std::vector<ProcessMetrics> get_history(size_t count = 0) const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (count == 0 || count > history_.size()) {
            return history_;
        }
        return std::vector<ProcessMetrics>(history_.end() - count, history_.end());
    }

private:
    int pid_;
    mutable std::mutex metrics_mutex_;
    ProcessMetrics latest_metrics_;
    std::vector<ProcessMetrics> history_;
    size_t max_history_size_ = 1000;
    
    void collect_cpu_metrics();
    void collect_memory_metrics();
    void collect_io_metrics();
    void collect_fd_metrics();
};

/**
 * @class UvLoopMetricsCollector
 * @brief Collects libuv event loop metrics
 */
class UvLoopMetricsCollector : public MetricsCollector {
public:
    explicit UvLoopMetricsCollector(uv_loop_t* loop);
    
    void collect() override;
    std::string get_name() const override { return "uv_loop"; }
    
    UvLoopMetrics get_latest() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return latest_metrics_;
    }
    
    std::vector<UvLoopMetrics> get_history(size_t count = 0) const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (count == 0 || count > history_.size()) {
            return history_;
        }
        return std::vector<UvLoopMetrics>(history_.end() - count, history_.end());
    }

private:
    uv_loop_t* loop_;
    mutable std::mutex metrics_mutex_;
    UvLoopMetrics latest_metrics_;
    std::vector<UvLoopMetrics> history_;
    size_t max_history_size_ = 1000;
    
    void collect_handle_metrics();
    void collect_timing_metrics();
    std::chrono::steady_clock::time_point last_collect_time_;
    uint64_t last_iteration_count_ = 0;
};

/**
 * @struct MonitorConfig
 * @brief Configuration for the monitoring system
 */
struct MonitorConfig {
    std::chrono::milliseconds collection_interval{1000}; // 1 second
    bool enable_system_metrics = true;
    bool enable_process_metrics = true;
    bool enable_uv_metrics = true;
    
    // Export settings
    bool enable_prometheus_export = false;
    uint16_t prometheus_port = 9090;
    std::string prometheus_path = "/metrics";
    
    bool enable_json_export = false;
    std::string json_export_file;
    
    bool enable_csv_export = false;
    std::string csv_export_file;
    
    // Alerting
    bool enable_alerting = false;
    double cpu_alert_threshold = 80.0;
    double memory_alert_threshold = 80.0;
    std::function<void(const std::string&)> alert_callback;
    
    // History settings
    size_t max_history_size = 1000;
    std::chrono::hours history_retention{24};
};

/**
 * @class Monitor
 * @brief Main monitoring system coordinator
 */
class Monitor {
public:
    explicit Monitor(const MonitorConfig& config = {}, uv_loop_t* loop = nullptr);
    ~Monitor();

    // Control
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Collector management
    void add_collector(std::unique_ptr<MetricsCollector> collector);
    void remove_collector(const std::string& name);
    MetricsCollector* get_collector(const std::string& name) const;
    
    // Metrics access
    SystemMetrics get_system_metrics() const;
    ProcessMetrics get_process_metrics() const;
    UvLoopMetrics get_uv_metrics() const;
    
    // Export functions
    std::string export_prometheus() const;
    std::string export_json() const;
    void export_csv(const std::string& filename) const;
    
    // Alerting
    void check_alerts();
    void add_alert_rule(const std::string& name, std::function<bool()> condition, 
                       std::function<void()> action);
    void remove_alert_rule(const std::string& name);
    
    // Configuration
    const MonitorConfig& get_config() const { return config_; }
    void set_config(const MonitorConfig& config);

private:
    MonitorConfig config_;
    uv_loop_t* loop_;
    bool loop_owned_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    // Collectors
    std::unordered_map<std::string, std::unique_ptr<MetricsCollector>> collectors_;
    mutable std::mutex collectors_mutex_;
    
    // Collection timer
    uv_timer_t collection_timer_;
    
    // Alert rules
    struct AlertRule {
        std::function<bool()> condition;
        std::function<void()> action;
        std::chrono::steady_clock::time_point last_triggered;
        std::chrono::seconds cooldown{60};
    };
    std::unordered_map<std::string, AlertRule> alert_rules_;
    mutable std::mutex alerts_mutex_;
    
    // Export thread
    std::thread export_thread_;
    
    // Internal methods
    static void collection_timer_callback(uv_timer_t* timer);
    void collect_all_metrics();
    void export_loop();
    void setup_default_collectors();
    void cleanup();
};

} // namespace uv_monitor

#endif // ATOM_EXTRA_UV_MONITOR_HPP
