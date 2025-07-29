/*
 * disk_analytics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Analytics and Predictive Maintenance

**************************************************/

#ifndef ATOM_SYSTEM_DISK_ANALYTICS_HPP
#define ATOM_SYSTEM_DISK_ANALYTICS_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "disk_types.hpp"
#include "disk_performance.hpp"

namespace atom::system {

/**
 * @brief Disk usage pattern analysis
 */
struct UsagePattern {
    std::string devicePath;
    std::chrono::system_clock::time_point analysisTime{std::chrono::system_clock::now()};

    // Usage statistics
    double avgDailyGrowthMB{0.0};
    double peakUsageHour{12.0};  // Hour of day (0-23)
    double minUsageHour{3.0};    // Hour of day (0-23)

    // Access patterns
    double readWriteRatio{1.0};  // Read operations / Write operations
    double sequentialRandomRatio{1.0};  // Sequential / Random access ratio

    // Temporal patterns
    std::vector<double> hourlyUsagePattern;  // 24 hours
    std::vector<double> weeklyUsagePattern;  // 7 days
    std::vector<double> monthlyUsagePattern; // 30 days

    // Predictive metrics
    uint32_t daysUntilFull{365};
    double projectedSizeGB{0.0};

    UsagePattern() {
        hourlyUsagePattern.resize(24, 0.0);
        weeklyUsagePattern.resize(7, 0.0);
        monthlyUsagePattern.resize(30, 0.0);
    }
};

/**
 * @brief Capacity planning recommendation
 */
struct CapacityPlanningResult {
    std::string devicePath;
    std::chrono::system_clock::time_point analysisTime{std::chrono::system_clock::now()};

    // Current status
    uint64_t currentUsedBytes{0};
    uint64_t currentTotalBytes{0};
    float currentUsagePercent{0.0f};

    // Projections
    uint64_t projectedUsedBytes6Months{0};
    uint64_t projectedUsedBytes1Year{0};
    uint64_t projectedUsedBytes2Years{0};

    // Recommendations
    bool needsExpansion{false};
    uint32_t recommendedExpansionGB{0};
    uint32_t daysUntilCritical{365};

    // Growth analysis
    double dailyGrowthRate{0.0};  // Bytes per day
    double weeklyGrowthRate{0.0}; // Bytes per week
    double monthlyGrowthRate{0.0}; // Bytes per month

    std::vector<std::string> recommendations;

    CapacityPlanningResult() = default;
};

/**
 * @brief Predictive maintenance alert
 */
struct MaintenanceAlert {
    enum class AlertType {
        PERFORMANCE_DEGRADATION,
        HEALTH_WARNING,
        CAPACITY_WARNING,
        TEMPERATURE_ALERT,
        FAILURE_PREDICTION,
        OPTIMIZATION_OPPORTUNITY
    };

    enum class Severity {
        INFO,
        WARNING,
        CRITICAL,
        EMERGENCY
    };

    std::string devicePath;
    AlertType type;
    Severity severity;
    std::string message;
    std::string details;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    // Prediction confidence (0.0 - 1.0)
    float confidence{0.0f};

    // Recommended actions
    std::vector<std::string> recommendedActions;

    MaintenanceAlert(const std::string& path, AlertType alertType, Severity sev, const std::string& msg)
        : devicePath(path), type(alertType), severity(sev), message(msg) {}
};

/**
 * @brief Performance trend analysis with machine learning insights
 */
struct AdvancedTrendAnalysis {
    std::string devicePath;
    std::chrono::system_clock::time_point analysisTime{std::chrono::system_clock::now()};

    // Trend classifications
    enum class TrendType {
        STABLE,
        IMPROVING,
        DEGRADING,
        VOLATILE,
        SEASONAL
    };

    TrendType performanceTrend{TrendType::STABLE};
    TrendType healthTrend{TrendType::STABLE};
    TrendType usageTrend{TrendType::STABLE};

    // Confidence levels
    float performanceTrendConfidence{0.0f};
    float healthTrendConfidence{0.0f};
    float usageTrendConfidence{0.0f};

    // Anomaly detection
    bool hasAnomalies{false};
    std::vector<std::string> detectedAnomalies;

    // Seasonal patterns
    bool hasSeasonalPattern{false};
    std::string seasonalDescription;

    // Predictions
    std::optional<uint32_t> predictedFailureDays;
    std::optional<float> predictedPerformanceDrop;  // Percentage

    AdvancedTrendAnalysis() = default;
};

/**
 * @brief Configuration for analytics engine
 */
struct AnalyticsConfig {
    std::chrono::hours analysisWindow{std::chrono::hours(24 * 7)};  // 1 week
    uint32_t minSamplesRequired{100};
    float anomalyThreshold{2.5f};  // Standard deviations
    bool enablePredictiveAnalysis{true};
    bool enableSeasonalDetection{true};
    bool enableCapacityPlanning{true};
    float capacityWarningThreshold{80.0f};  // Percentage
    float capacityCriticalThreshold{95.0f}; // Percentage

    AnalyticsConfig() = default;
};

/**
 * @brief Analyzes disk usage patterns over time
 *
 * @param devicePath Path to the device
 * @param analysisDays Number of days to analyze
 * @return Usage pattern analysis
 */
[[nodiscard]] auto analyzeUsagePatterns(const std::string& devicePath,
                                       uint32_t analysisDays = 30) -> UsagePattern;

/**
 * @brief Performs capacity planning analysis
 *
 * @param devicePath Path to the device
 * @param projectionMonths Number of months to project
 * @return Capacity planning recommendations
 */
[[nodiscard]] auto performCapacityPlanning(const std::string& devicePath,
                                          uint32_t projectionMonths = 12) -> CapacityPlanningResult;

/**
 * @brief Generates predictive maintenance alerts
 *
 * @param devicePath Path to the device
 * @param config Analytics configuration
 * @return Vector of maintenance alerts
 */
[[nodiscard]] auto generateMaintenanceAlerts(const std::string& devicePath,
                                            const AnalyticsConfig& config = {})
    -> std::vector<MaintenanceAlert>;

/**
 * @brief Performs advanced trend analysis with ML insights
 *
 * @param devicePath Path to the device
 * @param config Analytics configuration
 * @return Advanced trend analysis results
 */
[[nodiscard]] auto performAdvancedTrendAnalysis(const std::string& devicePath,
                                               const AnalyticsConfig& config = {})
    -> AdvancedTrendAnalysis;

/**
 * @brief Predicts optimal maintenance schedule
 *
 * @param devicePath Path to the device
 * @param currentMetrics Current performance metrics
 * @return Recommended maintenance schedule
 */
[[nodiscard]] auto predictMaintenanceSchedule(const std::string& devicePath,
                                             const ExtendedPerformanceMetrics& currentMetrics)
    -> std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>;

/**
 * @brief Analyzes cost-benefit of disk replacement
 *
 * @param devicePath Path to the device
 * @param replacementCost Cost of replacement in currency units
 * @return Cost-benefit analysis result
 */
struct CostBenefitAnalysis {
    float currentMaintenanceCost{0.0f};
    float projectedMaintenanceCost{0.0f};
    float replacementCost{0.0f};
    float energySavings{0.0f};
    float performanceGain{0.0f};
    bool recommendReplacement{false};
    uint32_t paybackPeriodDays{0};
    std::string recommendation;
};

[[nodiscard]] auto analyzeCostBenefit(const std::string& devicePath,
                                     float replacementCost) -> CostBenefitAnalysis;

/**
 * @brief Optimizes disk configuration based on usage patterns
 *
 * @param devicePath Path to the device
 * @param usagePattern Current usage pattern
 * @return Configuration optimization recommendations
 */
[[nodiscard]] auto optimizeDiskConfiguration(const std::string& devicePath,
                                            const UsagePattern& usagePattern)
    -> std::vector<std::string>;

/**
 * @brief Class for comprehensive disk analytics and predictive maintenance
 */
class DiskAnalytics {
public:
    DiskAnalytics();
    explicit DiskAnalytics(const AnalyticsConfig& config);
    ~DiskAnalytics();

    // Disable copy constructor and assignment
    DiskAnalytics(const DiskAnalytics&) = delete;
    DiskAnalytics& operator=(const DiskAnalytics&) = delete;

    // Enable move constructor and assignment
    DiskAnalytics(DiskAnalytics&&) noexcept;
    DiskAnalytics& operator=(DiskAnalytics&&) noexcept;

    /**
     * @brief Add device to analytics monitoring
     */
    void addDevice(const std::string& devicePath);

    /**
     * @brief Remove device from monitoring
     */
    void removeDevice(const std::string& devicePath);

    /**
     * @brief Start continuous analytics monitoring
     */
    void startMonitoring(std::function<void(const MaintenanceAlert&)> alertCallback);

    /**
     * @brief Stop analytics monitoring
     */
    void stopMonitoring();

    /**
     * @brief Get usage patterns for a device
     */
    [[nodiscard]] UsagePattern getUsagePatterns(const std::string& devicePath,
                                               uint32_t analysisDays = 30) const;

    /**
     * @brief Get capacity planning for a device
     */
    [[nodiscard]] CapacityPlanningResult getCapacityPlanning(const std::string& devicePath,
                                                            uint32_t projectionMonths = 12) const;

    /**
     * @brief Get advanced trend analysis
     */
    [[nodiscard]] AdvancedTrendAnalysis getTrendAnalysis(const std::string& devicePath) const;

    /**
     * @brief Get all active alerts
     */
    [[nodiscard]] std::vector<MaintenanceAlert> getActiveAlerts() const;

    /**
     * @brief Get analytics statistics
     */
    struct AnalyticsStats {
        size_t devicesMonitored{0};
        size_t alertsGenerated{0};
        size_t predictionsAccurate{0};
        size_t predictionsTotal{0};
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime{0};
    };

    [[nodiscard]] AnalyticsStats getStats() const;

    /**
     * @brief Update analytics configuration
     */
    void updateConfig(const AnalyticsConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const AnalyticsConfig& getConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_ANALYTICS_HPP
