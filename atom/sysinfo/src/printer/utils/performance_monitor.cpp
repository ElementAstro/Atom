#include "performance_monitor.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace atom::system::utils {

PerformanceTimer::PerformanceTimer(const std::string& name, const std::string& category)
    : name_(name), category_(category), startTime_(std::chrono::steady_clock::now()) {
}

PerformanceTimer::~PerformanceTimer() {
    auto& monitor = PerformanceMonitor::getInstance();
    if (!monitor.isEnabled()) {
        return;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);

    PerformanceMetric metric;
    metric.name = name_;
    metric.category = category_;
    metric.startTime = startTime_;
    metric.endTime = endTime;
    metric.duration = duration;
    metric.memoryUsage = getCurrentMemoryUsage();

    monitor.recordMetric(metric);
}

PerformanceMonitor& PerformanceMonitor::getInstance() {
    static PerformanceMonitor instance;
    return instance;
}

void PerformanceMonitor::recordMetric(const PerformanceMetric& metric) {
    if (!enabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.push_back(metric);
}

auto PerformanceMonitor::getMetrics() const -> std::vector<PerformanceMetric> {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

auto PerformanceMonitor::getMetricsByCategory(const std::string& category) const -> std::vector<PerformanceMetric> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PerformanceMetric> result;
    std::copy_if(metrics_.begin(), metrics_.end(), std::back_inserter(result),
                 [&category](const PerformanceMetric& metric) {
                     return metric.category == category;
                 });

    return result;
}

auto PerformanceMonitor::getSummary() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    if (metrics_.empty()) {
        return "No performance metrics recorded.";
    }

    std::stringstream ss;
    ss << "Performance Summary:\n";
    ss << "==================\n";
    ss << "Total operations: " << metrics_.size() << "\n\n";

    // Group by category
    std::unordered_map<std::string, std::vector<PerformanceMetric>> byCategory;
    for (const auto& metric : metrics_) {
        byCategory[metric.category].push_back(metric);
    }

    for (const auto& [category, categoryMetrics] : byCategory) {
        ss << "Category: " << category << "\n";
        ss << calculateStatistics(categoryMetrics) << "\n";
    }

    return ss.str();
}

void PerformanceMonitor::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

void PerformanceMonitor::setEnabled(bool enabled) {
    enabled_ = enabled;
}

auto PerformanceMonitor::isEnabled() const -> bool {
    return enabled_;
}

auto getCurrentMemoryUsage() -> size_t {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024; // Convert from KB to bytes on Linux
    }
    return 0;
#endif
}

auto formatDuration(std::chrono::microseconds duration) -> std::string {
    auto us = duration.count();

    if (us < 1000) {
        return std::to_string(us) + " μs";
    } else if (us < 1000000) {
        return std::to_string(us / 1000.0) + " ms";
    } else {
        return std::to_string(us / 1000000.0) + " s";
    }
}

auto calculateStatistics(const std::vector<PerformanceMetric>& metrics) -> std::string {
    if (metrics.empty()) {
        return "No metrics available.";
    }

    std::vector<double> durations;
    durations.reserve(metrics.size());

    for (const auto& metric : metrics) {
        durations.push_back(static_cast<double>(metric.duration.count()));
    }

    std::sort(durations.begin(), durations.end());

    double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
    double mean = sum / durations.size();

    double median = durations.size() % 2 == 0
        ? (durations[durations.size() / 2 - 1] + durations[durations.size() / 2]) / 2.0
        : durations[durations.size() / 2];

    double min = durations.front();
    double max = durations.back();

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "  Operations: " << metrics.size() << "\n";
    ss << "  Mean: " << formatDuration(std::chrono::microseconds(static_cast<long long>(mean))) << "\n";
    ss << "  Median: " << formatDuration(std::chrono::microseconds(static_cast<long long>(median))) << "\n";
    ss << "  Min: " << formatDuration(std::chrono::microseconds(static_cast<long long>(min))) << "\n";
    ss << "  Max: " << formatDuration(std::chrono::microseconds(static_cast<long long>(max))) << "\n";

    return ss.str();
}

} // namespace atom::system::utils
