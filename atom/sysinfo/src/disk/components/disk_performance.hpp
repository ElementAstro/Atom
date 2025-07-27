/*
 * disk_performance.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Performance Monitoring

**************************************************/

#ifndef ATOM_SYSTEM_DISK_PERFORMANCE_HPP
#define ATOM_SYSTEM_DISK_PERFORMANCE_HPP

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/sysinfo/disk/disk_types.hpp"

namespace atom::system {

/**
 * @brief Extended performance metrics with detailed statistics
 */
struct ExtendedPerformanceMetrics {
    DiskPerformanceMetrics basic;

    // IOPS metrics
    double readIOPS{0.0};
    double writeIOPS{0.0};
    double totalIOPS{0.0};

    // Throughput metrics (MB/s)
    double readThroughputMBps{0.0};
    double writeThroughputMBps{0.0};
    double totalThroughputMBps{0.0};

    // Latency statistics (microseconds)
    uint64_t minReadLatencyUs{0};
    uint64_t maxReadLatencyUs{0};
    uint64_t avgReadLatencyUs{0};
    uint64_t minWriteLatencyUs{0};
    uint64_t maxWriteLatencyUs{0};
    uint64_t avgWriteLatencyUs{0};

    // Queue metrics
    double avgQueueDepth{0.0};
    uint32_t maxQueueDepth{0};

    // Utilization metrics
    float diskUtilization{0.0f};  // Percentage
    float bandwidthUtilization{0.0f};  // Percentage

    // Health prediction metrics
    float healthScore{100.0f};  // 0-100 scale
    uint32_t predictedLifespanDays{0};

    // Timestamp
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    ExtendedPerformanceMetrics() = default;

    ExtendedPerformanceMetrics(const DiskPerformanceMetrics& basicMetrics)
        : basic(basicMetrics) {}
};

/**
 * @brief Performance benchmark configuration
 */
struct BenchmarkConfig {
    uint32_t testSizeMB{100};
    uint32_t blockSizeKB{4};
    uint32_t queueDepth{1};
    uint32_t testDurationSeconds{60};
    bool randomAccess{false};
    bool writeTest{true};
    bool readTest{true};
    bool mixedTest{true};
    float readWriteRatio{0.7f};  // 70% read, 30% write for mixed test

    BenchmarkConfig() = default;
};

/**
 * @brief Performance monitoring configuration
 */
struct PerformanceMonitorConfig {
    std::chrono::milliseconds sampleInterval{std::chrono::seconds(1)};
    uint32_t historySize{3600};  // Keep 1 hour of samples by default
    bool enablePredictiveAnalysis{true};
    bool enableAnomalyDetection{true};
    float anomalyThreshold{2.0f};  // Standard deviations
    bool enableHealthPrediction{true};

    PerformanceMonitorConfig() = default;
};

/**
 * @brief Performance trend analysis result
 */
struct PerformanceTrend {
    std::string devicePath;
    std::chrono::system_clock::time_point analysisTime{std::chrono::system_clock::now()};

    // Trend indicators (-1: declining, 0: stable, 1: improving)
    int iopstrend{0};
    int throughputTrend{0};
    int latencyTrend{0};
    int healthTrend{0};

    // Confidence levels (0.0 - 1.0)
    float iopsTrendConfidence{0.0f};
    float throughputTrendConfidence{0.0f};
    float latencyTrendConfidence{0.0f};
    float healthTrendConfidence{0.0f};

    // Predictions
    std::optional<uint32_t> predictedFailureDays;
    std::optional<float> predictedPerformanceDegradation;  // Percentage

    std::vector<std::string> recommendations;

    PerformanceTrend() = default;
};

/**
 * @brief Retrieves current performance metrics for a device
 *
 * @param devicePath Path to the device
 * @return Extended performance metrics or nullopt if failed
 */
[[nodiscard]] auto getCurrentPerformanceMetrics(const std::string& devicePath)
    -> std::optional<ExtendedPerformanceMetrics>;

/**
 * @brief Performs comprehensive performance benchmark
 *
 * @param devicePath Path to the device
 * @param config Benchmark configuration
 * @return Benchmark results
 */
[[nodiscard]] auto performBenchmark(const std::string& devicePath,
                                   const BenchmarkConfig& config = {})
    -> ExtendedPerformanceMetrics;

/**
 * @brief Starts continuous performance monitoring
 *
 * @param devicePath Path to the device
 * @param callback Function to call with performance updates
 * @param config Monitoring configuration
 * @return Future that can be used to stop monitoring
 */
[[nodiscard]] auto startPerformanceMonitoring(const std::string& devicePath,
                                             std::function<void(const ExtendedPerformanceMetrics&)> callback,
                                             const PerformanceMonitorConfig& config = {})
    -> std::future<void>;

/**
 * @brief Analyzes performance trends for a device
 *
 * @param devicePath Path to the device
 * @param historyDays Number of days of history to analyze
 * @return Performance trend analysis
 */
[[nodiscard]] auto analyzePerformanceTrends(const std::string& devicePath,
                                           uint32_t historyDays = 7)
    -> PerformanceTrend;

/**
 * @brief Predicts disk health based on performance metrics
 *
 * @param devicePath Path to the device
 * @param metrics Recent performance metrics
 * @return Health score (0-100) and predicted lifespan
 */
[[nodiscard]] auto predictDiskHealth(const std::string& devicePath,
                                    const std::vector<ExtendedPerformanceMetrics>& metrics)
    -> std::pair<float, uint32_t>;

/**
 * @brief Detects performance anomalies
 *
 * @param baseline Baseline performance metrics
 * @param current Current performance metrics
 * @param threshold Anomaly detection threshold (standard deviations)
 * @return True if anomaly detected, false otherwise
 */
[[nodiscard]] auto detectPerformanceAnomaly(const ExtendedPerformanceMetrics& baseline,
                                           const ExtendedPerformanceMetrics& current,
                                           float threshold = 2.0f) -> bool;

/**
 * @brief Optimizes disk performance settings
 *
 * @param devicePath Path to the device
 * @param currentMetrics Current performance metrics
 * @return Vector of optimization recommendations
 */
[[nodiscard]] auto optimizePerformanceSettings(const std::string& devicePath,
                                              const ExtendedPerformanceMetrics& currentMetrics)
    -> std::vector<std::string>;

/**
 * @brief Class for comprehensive disk performance management
 */
class PerformanceManager {
public:
    PerformanceManager();
    explicit PerformanceManager(const PerformanceMonitorConfig& config);
    ~PerformanceManager();

    // Disable copy constructor and assignment
    PerformanceManager(const PerformanceManager&) = delete;
    PerformanceManager& operator=(const PerformanceManager&) = delete;

    // Enable move constructor and assignment
    PerformanceManager(PerformanceManager&&) noexcept;
    PerformanceManager& operator=(PerformanceManager&&) noexcept;

    /**
     * @brief Add device to performance monitoring
     */
    void addDevice(const std::string& devicePath);

    /**
     * @brief Remove device from monitoring
     */
    void removeDevice(const std::string& devicePath);

    /**
     * @brief Start monitoring all added devices
     */
    void startMonitoring(std::function<void(const std::string&, const ExtendedPerformanceMetrics&)> callback);

    /**
     * @brief Stop performance monitoring
     */
    void stopMonitoring();

    /**
     * @brief Get current metrics for a device
     */
    [[nodiscard]] std::optional<ExtendedPerformanceMetrics> getCurrentMetrics(const std::string& devicePath) const;

    /**
     * @brief Get performance history for a device
     */
    [[nodiscard]] std::vector<ExtendedPerformanceMetrics> getPerformanceHistory(
        const std::string& devicePath, std::chrono::hours duration = std::chrono::hours(24)) const;

    /**
     * @brief Perform benchmark on a device
     */
    [[nodiscard]] ExtendedPerformanceMetrics benchmark(const std::string& devicePath,
                                                      const BenchmarkConfig& config = {});

    /**
     * @brief Analyze trends for a device
     */
    [[nodiscard]] PerformanceTrend analyzeTrends(const std::string& devicePath,
                                                uint32_t historyDays = 7) const;

    /**
     * @brief Get monitoring statistics
     */
    struct MonitoringStats {
        size_t devicesMonitored{0};
        size_t samplesCollected{0};
        size_t anomaliesDetected{0};
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime{0};
    };

    [[nodiscard]] MonitoringStats getStats() const;

    /**
     * @brief Update monitoring configuration
     */
    void updateConfig(const PerformanceMonitorConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const PerformanceMonitorConfig& getConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_PERFORMANCE_HPP
