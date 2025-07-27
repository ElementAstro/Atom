/**
 * @file performance_monitor.hpp
 * @brief Performance monitoring utilities
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_PERFORMANCE_MONITOR_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_PERFORMANCE_MONITOR_HPP

#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace atom::system::utils {

/**
 * @struct PerformanceMetric
 * @brief Performance measurement data
 */
struct PerformanceMetric {
    std::string name;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    std::chrono::microseconds duration;
    size_t memoryUsage = 0;
    std::string category;
};

/**
 * @class PerformanceTimer
 * @brief RAII timer for measuring performance
 */
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& name, const std::string& category = "general");
    ~PerformanceTimer();
    
    // Non-copyable, non-movable
    PerformanceTimer(const PerformanceTimer&) = delete;
    PerformanceTimer& operator=(const PerformanceTimer&) = delete;
    PerformanceTimer(PerformanceTimer&&) = delete;
    PerformanceTimer& operator=(PerformanceTimer&&) = delete;

private:
    std::string name_;
    std::string category_;
    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @class PerformanceMonitor
 * @brief Singleton class for collecting and analyzing performance metrics
 */
class PerformanceMonitor {
public:
    static PerformanceMonitor& getInstance();
    
    /**
     * @brief Record a performance metric
     * @param metric The metric to record
     */
    void recordMetric(const PerformanceMetric& metric);
    
    /**
     * @brief Get all recorded metrics
     * @return Vector of all metrics
     */
    [[nodiscard]] auto getMetrics() const -> std::vector<PerformanceMetric>;
    
    /**
     * @brief Get metrics by category
     * @param category The category to filter by
     * @return Vector of metrics in the category
     */
    [[nodiscard]] auto getMetricsByCategory(const std::string& category) const -> std::vector<PerformanceMetric>;
    
    /**
     * @brief Get performance summary
     * @return Formatted performance summary string
     */
    [[nodiscard]] auto getSummary() const -> std::string;
    
    /**
     * @brief Clear all recorded metrics
     */
    void clear();
    
    /**
     * @brief Enable/disable performance monitoring
     * @param enabled Whether to enable monitoring
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if monitoring is enabled
     * @return true if monitoring is enabled
     */
    [[nodiscard]] auto isEnabled() const -> bool;

private:
    PerformanceMonitor() = default;
    
    std::vector<PerformanceMetric> metrics_;
    mutable std::mutex mutex_;
    bool enabled_ = false;
};

/**
 * @brief Macro for easy performance timing
 */
#define PERF_TIMER(name) \
    atom::system::utils::PerformanceTimer _perf_timer(name)

#define PERF_TIMER_CATEGORY(name, category) \
    atom::system::utils::PerformanceTimer _perf_timer(name, category)

/**
 * @brief Get current memory usage in bytes
 * @return Memory usage in bytes
 */
[[nodiscard]] auto getCurrentMemoryUsage() -> size_t;

/**
 * @brief Format duration for display
 * @param duration Duration in microseconds
 * @return Formatted duration string
 */
[[nodiscard]] auto formatDuration(std::chrono::microseconds duration) -> std::string;

/**
 * @brief Calculate statistics for a set of metrics
 * @param metrics Vector of metrics
 * @return Statistics summary string
 */
[[nodiscard]] auto calculateStatistics(const std::vector<PerformanceMetric>& metrics) -> std::string;

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_PERFORMANCE_MONITOR_HPP
