/*
 * disk_analytics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Analytics and Predictive Maintenance

**************************************************/

#include "disk_analytics.hpp"
#include "disk_info.hpp"
#include "disk_performance.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace atom::system {

namespace {
// Analytics data cache
std::mutex g_analyticsCacheMutex;
std::unordered_map<std::string, std::vector<DiskInfo>> g_diskUsageHistory;
std::unordered_map<std::string, std::vector<ExtendedPerformanceMetrics>> g_performanceHistory;

// Helper function to calculate linear regression
struct LinearRegression {
    double slope{0.0};
    double intercept{0.0};
    double correlation{0.0};
    
    LinearRegression(const std::vector<double>& x, const std::vector<double>& y) {
        if (x.size() != y.size() || x.size() < 2) {
            return;
        }
        
        size_t n = x.size();
        double sumX = std::accumulate(x.begin(), x.end(), 0.0);
        double sumY = std::accumulate(y.begin(), y.end(), 0.0);
        double sumXY = 0.0, sumX2 = 0.0, sumY2 = 0.0;
        
        for (size_t i = 0; i < n; ++i) {
            sumXY += x[i] * y[i];
            sumX2 += x[i] * x[i];
            sumY2 += y[i] * y[i];
        }
        
        double denominator = n * sumX2 - sumX * sumX;
        if (std::abs(denominator) > 1e-10) {
            slope = (n * sumXY - sumX * sumY) / denominator;
            intercept = (sumY - slope * sumX) / n;
            
            // Calculate correlation coefficient
            double numerator = n * sumXY - sumX * sumY;
            double denom1 = std::sqrt(n * sumX2 - sumX * sumX);
            double denom2 = std::sqrt(n * sumY2 - sumY * sumY);
            if (denom1 > 1e-10 && denom2 > 1e-10) {
                correlation = numerator / (denom1 * denom2);
            }
        }
    }
};

// Helper function to detect seasonal patterns
bool detectSeasonalPattern(const std::vector<double>& values, std::string& description) {
    if (values.size() < 24) {  // Need at least 24 data points
        return false;
    }
    
    // Simple seasonal detection based on autocorrelation
    std::vector<double> autocorr;
    for (size_t lag = 1; lag < std::min(values.size() / 2, size_t(24)); ++lag) {
        double correlation = 0.0;
        double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        
        double numerator = 0.0, denominator = 0.0;
        for (size_t i = lag; i < values.size(); ++i) {
            numerator += (values[i] - mean) * (values[i - lag] - mean);
            denominator += (values[i] - mean) * (values[i] - mean);
        }
        
        if (denominator > 1e-10) {
            correlation = numerator / denominator;
        }
        autocorr.push_back(std::abs(correlation));
    }
    
    // Find peak autocorrelation
    auto maxIt = std::max_element(autocorr.begin(), autocorr.end());
    if (maxIt != autocorr.end() && *maxIt > 0.3) {  // Threshold for seasonal pattern
        size_t period = std::distance(autocorr.begin(), maxIt) + 1;
        if (period >= 7 && period <= 24) {
            description = "Detected " + std::to_string(period) + "-period seasonal pattern";
            return true;
        }
    }
    
    return false;
}

// Helper function to calculate anomaly score
double calculateAnomalyScore(const std::vector<double>& values, double currentValue) {
    if (values.size() < 3) {
        return 0.0;
    }
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;
    
    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= (values.size() - 1);
    
    double stddev = std::sqrt(variance);
    if (stddev < 1e-10) {
        return 0.0;
    }
    
    return std::abs(currentValue - mean) / stddev;
}
}  // anonymous namespace

UsagePattern analyzeUsagePatterns(const std::string& devicePath, uint32_t analysisDays) {
    UsagePattern pattern;
    pattern.devicePath = devicePath;
    
    try {
        // Get historical disk usage data
        std::lock_guard<std::mutex> lock(g_analyticsCacheMutex);
        auto it = g_diskUsageHistory.find(devicePath);
        if (it == g_diskUsageHistory.end() || it->second.size() < 10) {
            spdlog::warn("Insufficient usage history for pattern analysis: {}", devicePath);
            return pattern;
        }
        
        const auto& history = it->second;
        
        // Calculate daily growth rate
        std::vector<double> usageSizes;
        std::vector<double> timePoints;
        
        for (size_t i = 0; i < history.size(); ++i) {
            usageSizes.push_back(static_cast<double>(history[i].getUsedSpace()));
            timePoints.push_back(static_cast<double>(i));
        }
        
        LinearRegression regression(timePoints, usageSizes);
        pattern.avgDailyGrowthMB = regression.slope / (1024.0 * 1024.0);  // Convert to MB
        
        // Simulate hourly and weekly patterns (in real implementation, this would use actual timestamps)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> hourlyDist(0.5, 0.2);
        std::normal_distribution<> weeklyDist(0.7, 0.15);
        
        for (size_t i = 0; i < 24; ++i) {
            pattern.hourlyUsagePattern[i] = std::max(0.0, hourlyDist(gen));
        }
        
        for (size_t i = 0; i < 7; ++i) {
            pattern.weeklyUsagePattern[i] = std::max(0.0, weeklyDist(gen));
        }
        
        // Find peak and minimum usage hours
        auto maxHourIt = std::max_element(pattern.hourlyUsagePattern.begin(), pattern.hourlyUsagePattern.end());
        auto minHourIt = std::min_element(pattern.hourlyUsagePattern.begin(), pattern.hourlyUsagePattern.end());
        
        pattern.peakUsageHour = std::distance(pattern.hourlyUsagePattern.begin(), maxHourIt);
        pattern.minUsageHour = std::distance(pattern.hourlyUsagePattern.begin(), minHourIt);
        
        // Predict days until full
        if (pattern.avgDailyGrowthMB > 0 && !history.empty()) {
            double currentFreeBytes = static_cast<double>(history.back().freeSpace);
            double freeMB = currentFreeBytes / (1024.0 * 1024.0);
            pattern.daysUntilFull = static_cast<uint32_t>(freeMB / pattern.avgDailyGrowthMB);
        }
        
        // Project future size
        pattern.projectedSizeGB = (static_cast<double>(history.back().getUsedSpace()) + 
                                 pattern.avgDailyGrowthMB * 365 * 1024 * 1024) / (1024.0 * 1024.0 * 1024.0);
        
    } catch (const std::exception& e) {
        spdlog::error("Error analyzing usage patterns for {}: {}", devicePath, e.what());
    }
    
    return pattern;
}

CapacityPlanningResult performCapacityPlanning(const std::string& devicePath, uint32_t projectionMonths) {
    CapacityPlanningResult result;
    result.devicePath = devicePath;
    
    try {
        // Get current disk info
        auto diskInfo = getDiskInfoCached(devicePath);
        result.currentUsedBytes = diskInfo.getUsedSpace();
        result.currentTotalBytes = diskInfo.totalSpace;
        result.currentUsagePercent = diskInfo.usagePercent;
        
        // Analyze usage patterns
        auto usagePattern = analyzeUsagePatterns(devicePath, 30);
        
        // Calculate growth rates
        result.dailyGrowthRate = usagePattern.avgDailyGrowthMB * 1024 * 1024;  // Convert to bytes
        result.weeklyGrowthRate = result.dailyGrowthRate * 7;
        result.monthlyGrowthRate = result.dailyGrowthRate * 30;
        
        // Project future usage
        result.projectedUsedBytes6Months = result.currentUsedBytes + 
            static_cast<uint64_t>(result.dailyGrowthRate * 180);
        result.projectedUsedBytes1Year = result.currentUsedBytes + 
            static_cast<uint64_t>(result.dailyGrowthRate * 365);
        result.projectedUsedBytes2Years = result.currentUsedBytes + 
            static_cast<uint64_t>(result.dailyGrowthRate * 730);
        
        // Check if expansion is needed
        float projected6MonthsPercent = (static_cast<float>(result.projectedUsedBytes6Months) / 
                                       result.currentTotalBytes) * 100.0f;
        
        if (projected6MonthsPercent > 90.0f) {
            result.needsExpansion = true;
            uint64_t additionalNeeded = result.projectedUsedBytes1Year - 
                static_cast<uint64_t>(result.currentTotalBytes * 0.8);  // Keep 20% free
            result.recommendedExpansionGB = static_cast<uint32_t>(additionalNeeded / (1024ULL * 1024 * 1024));
        }
        
        // Calculate days until critical
        if (result.dailyGrowthRate > 0) {
            uint64_t bytesUntilCritical = static_cast<uint64_t>(result.currentTotalBytes * 0.95) - 
                                        result.currentUsedBytes;
            result.daysUntilCritical = static_cast<uint32_t>(bytesUntilCritical / result.dailyGrowthRate);
        }
        
        // Generate recommendations
        if (result.currentUsagePercent > 80.0f) {
            result.recommendations.push_back("Current usage is high - monitor closely");
        }
        
        if (result.needsExpansion) {
            result.recommendations.push_back("Expansion required within 6 months");
            result.recommendations.push_back("Consider adding " + std::to_string(result.recommendedExpansionGB) + 
                                           " GB of storage");
        }
        
        if (result.daysUntilCritical < 90) {
            result.recommendations.push_back("Critical capacity will be reached in " + 
                                           std::to_string(result.daysUntilCritical) + " days");
        }
        
        if (result.dailyGrowthRate > 1024 * 1024 * 1024) {  // > 1GB per day
            result.recommendations.push_back("High growth rate detected - investigate data retention policies");
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Error performing capacity planning for {}: {}", devicePath, e.what());
    }
    
    return result;
}

std::vector<MaintenanceAlert> generateMaintenanceAlerts(const std::string& devicePath,
                                                       const AnalyticsConfig& config) {
    std::vector<MaintenanceAlert> alerts;

    try {
        // Get current metrics
        auto currentMetrics = getCurrentPerformanceMetrics(devicePath);
        auto diskInfos = getDiskInfo(true);
        DiskInfo diskInfo;
        for (const auto& info : diskInfos) {
            if (info.devicePath == devicePath) {
                diskInfo = info;
                break;
            }
        }

        // Capacity alerts
        if (diskInfo.usagePercent > config.capacityCriticalThreshold) {
            MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::CAPACITY_WARNING,
                                 MaintenanceAlert::Severity::CRITICAL,
                                 "Disk usage critical: " + std::to_string(diskInfo.usagePercent) + "%");
            alert.confidence = 1.0f;
            alert.recommendedActions.push_back("Immediate cleanup or expansion required");
            alert.recommendedActions.push_back("Move data to alternative storage");
            alerts.push_back(alert);
        } else if (diskInfo.usagePercent > config.capacityWarningThreshold) {
            MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::CAPACITY_WARNING,
                                 MaintenanceAlert::Severity::WARNING,
                                 "Disk usage warning: " + std::to_string(diskInfo.usagePercent) + "%");
            alert.confidence = 0.9f;
            alert.recommendedActions.push_back("Plan for capacity expansion");
            alert.recommendedActions.push_back("Review data retention policies");
            alerts.push_back(alert);
        }

        // Performance alerts
        if (currentMetrics) {
            if (currentMetrics->healthScore < 70.0f) {
                MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::HEALTH_WARNING,
                                     MaintenanceAlert::Severity::WARNING,
                                     "Health score declining: " + std::to_string(currentMetrics->healthScore));
                alert.confidence = 0.8f;
                alert.recommendedActions.push_back("Run comprehensive diagnostics");
                alert.recommendedActions.push_back("Backup critical data");
                alerts.push_back(alert);
            }

            if (currentMetrics->basic.temperature > 70.0f) {
                MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::TEMPERATURE_ALERT,
                                     MaintenanceAlert::Severity::WARNING,
                                     "High temperature: " + std::to_string(currentMetrics->basic.temperature) + "°C");
                alert.confidence = 1.0f;
                alert.recommendedActions.push_back("Improve cooling");
                alert.recommendedActions.push_back("Check ventilation");
                alerts.push_back(alert);
            }

            // Performance degradation detection
            std::lock_guard<std::mutex> lock(g_analyticsCacheMutex);
            auto it = g_performanceHistory.find(devicePath);
            if (it != g_performanceHistory.end() && it->second.size() > 10) {
                const auto& history = it->second;

                // Calculate baseline performance (average of last 10 samples)
                double baselineIOPS = 0.0;
                for (size_t i = history.size() - 10; i < history.size(); ++i) {
                    baselineIOPS += history[i].totalIOPS;
                }
                baselineIOPS /= 10.0;

                if (baselineIOPS > 0 && currentMetrics->totalIOPS < baselineIOPS * 0.7) {
                    MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::PERFORMANCE_DEGRADATION,
                                         MaintenanceAlert::Severity::WARNING,
                                         "Performance degradation detected");
                    alert.confidence = 0.7f;
                    alert.recommendedActions.push_back("Check for fragmentation");
                    alert.recommendedActions.push_back("Run performance optimization");
                    alerts.push_back(alert);
                }
            }
        }

        // Predictive failure analysis
        auto capacityPlanning = performCapacityPlanning(devicePath, 12);
        if (capacityPlanning.daysUntilCritical < 30) {
            MaintenanceAlert alert(devicePath, MaintenanceAlert::AlertType::FAILURE_PREDICTION,
                                 MaintenanceAlert::Severity::CRITICAL,
                                 "Critical capacity predicted in " +
                                 std::to_string(capacityPlanning.daysUntilCritical) + " days");
            alert.confidence = 0.85f;
            alert.recommendedActions.push_back("Plan immediate capacity expansion");
            alerts.push_back(alert);
        }

    } catch (const std::exception& e) {
        spdlog::error("Error generating maintenance alerts for {}: {}", devicePath, e.what());
    }

    return alerts;
}

AdvancedTrendAnalysis performAdvancedTrendAnalysis(const std::string& devicePath,
                                                  const AnalyticsConfig& config) {
    AdvancedTrendAnalysis analysis;
    analysis.devicePath = devicePath;

    try {
        std::lock_guard<std::mutex> lock(g_analyticsCacheMutex);

        // Get performance history
        auto perfIt = g_performanceHistory.find(devicePath);
        if (perfIt != g_performanceHistory.end() && perfIt->second.size() >= config.minSamplesRequired) {
            const auto& perfHistory = perfIt->second;

            // Analyze performance trends
            std::vector<double> iopsSamples, healthSamples;
            for (const auto& metrics : perfHistory) {
                iopsSamples.push_back(metrics.totalIOPS);
                healthSamples.push_back(static_cast<double>(metrics.healthScore));
            }

            // Performance trend analysis
            std::vector<double> timePoints(iopsSamples.size());
            std::iota(timePoints.begin(), timePoints.end(), 0.0);

            LinearRegression perfRegression(timePoints, iopsSamples);
            if (std::abs(perfRegression.correlation) > 0.3) {
                if (perfRegression.slope > 0.01) {
                    analysis.performanceTrend = AdvancedTrendAnalysis::TrendType::IMPROVING;
                } else if (perfRegression.slope < -0.01) {
                    analysis.performanceTrend = AdvancedTrendAnalysis::TrendType::DEGRADING;
                }
                analysis.performanceTrendConfidence = std::abs(perfRegression.correlation);
            }

            // Health trend analysis
            LinearRegression healthRegression(timePoints, healthSamples);
            if (std::abs(healthRegression.correlation) > 0.3) {
                if (healthRegression.slope > 0.1) {
                    analysis.healthTrend = AdvancedTrendAnalysis::TrendType::IMPROVING;
                } else if (healthRegression.slope < -0.1) {
                    analysis.healthTrend = AdvancedTrendAnalysis::TrendType::DEGRADING;
                }
                analysis.healthTrendConfidence = std::abs(healthRegression.correlation);
            }

            // Anomaly detection
            for (size_t i = 10; i < iopsSamples.size(); ++i) {
                std::vector<double> baseline(iopsSamples.begin() + i - 10, iopsSamples.begin() + i);
                double anomalyScore = calculateAnomalyScore(baseline, iopsSamples[i]);

                if (anomalyScore > config.anomalyThreshold) {
                    analysis.hasAnomalies = true;
                    analysis.detectedAnomalies.push_back(
                        "Performance anomaly at sample " + std::to_string(i) +
                        " (score: " + std::to_string(anomalyScore) + ")");
                }
            }

            // Seasonal pattern detection
            if (config.enableSeasonalDetection && iopsSamples.size() >= 24) {
                std::string seasonalDesc;
                if (detectSeasonalPattern(iopsSamples, seasonalDesc)) {
                    analysis.hasSeasonalPattern = true;
                    analysis.seasonalDescription = seasonalDesc;
                }
            }

            // Failure prediction
            if (analysis.healthTrend == AdvancedTrendAnalysis::TrendType::DEGRADING &&
                analysis.healthTrendConfidence > 0.7f) {

                // Simple linear extrapolation to predict when health reaches critical level
                double currentHealth = healthSamples.back();
                if (healthRegression.slope < 0 && currentHealth > 30.0) {
                    double daysToFailure = (currentHealth - 30.0) / std::abs(healthRegression.slope);
                    analysis.predictedFailureDays = static_cast<uint32_t>(daysToFailure);
                }
            }

            // Performance drop prediction
            if (analysis.performanceTrend == AdvancedTrendAnalysis::TrendType::DEGRADING &&
                analysis.performanceTrendConfidence > 0.6f) {

                double currentPerf = iopsSamples.back();
                if (perfRegression.slope < 0 && currentPerf > 0) {
                    // Predict 20% performance drop
                    double targetPerf = currentPerf * 0.8;
                    double daysToDegrade = (currentPerf - targetPerf) / std::abs(perfRegression.slope);
                    analysis.predictedPerformanceDrop = 20.0f;
                }
            }
        }

        // Get usage history for usage trend analysis
        auto usageIt = g_diskUsageHistory.find(devicePath);
        if (usageIt != g_diskUsageHistory.end() && usageIt->second.size() >= 10) {
            const auto& usageHistory = usageIt->second;

            std::vector<double> usageSamples;
            for (const auto& info : usageHistory) {
                usageSamples.push_back(static_cast<double>(info.usagePercent));
            }

            std::vector<double> timePoints(usageSamples.size());
            std::iota(timePoints.begin(), timePoints.end(), 0.0);

            LinearRegression usageRegression(timePoints, usageSamples);
            if (std::abs(usageRegression.correlation) > 0.3) {
                if (usageRegression.slope > 0.1) {
                    analysis.usageTrend = AdvancedTrendAnalysis::TrendType::DEGRADING;  // Increasing usage
                } else if (usageRegression.slope < -0.1) {
                    analysis.usageTrend = AdvancedTrendAnalysis::TrendType::IMPROVING;  // Decreasing usage
                }
                analysis.usageTrendConfidence = std::abs(usageRegression.correlation);
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Error performing advanced trend analysis for {}: {}", devicePath, e.what());
    }

    return analysis;
}

std::vector<std::pair<std::chrono::system_clock::time_point, std::string>>
predictMaintenanceSchedule(const std::string& devicePath,
                          const ExtendedPerformanceMetrics& currentMetrics) {
    std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> schedule;

    try {
        auto now = std::chrono::system_clock::now();

        // Regular maintenance based on health score
        if (currentMetrics.healthScore < 80.0f) {
            auto nextCheck = now + std::chrono::hours(24 * 7);  // Weekly
            schedule.emplace_back(nextCheck, "Health check and diagnostics");
        } else {
            auto nextCheck = now + std::chrono::hours(24 * 30);  // Monthly
            schedule.emplace_back(nextCheck, "Routine health check");
        }

        // Temperature-based maintenance
        if (currentMetrics.basic.temperature > 60.0f) {
            auto coolingCheck = now + std::chrono::hours(24);
            schedule.emplace_back(coolingCheck, "Check cooling system");
        }

        // Performance-based maintenance
        if (currentMetrics.totalIOPS < 100.0) {
            auto perfOptimization = now + std::chrono::hours(24 * 3);
            schedule.emplace_back(perfOptimization, "Performance optimization");
        }

        // Predictive maintenance based on trends
        auto trendAnalysis = performAdvancedTrendAnalysis(devicePath);
        if (trendAnalysis.predictedFailureDays && *trendAnalysis.predictedFailureDays < 90) {
            auto replacementPlanning = now + std::chrono::hours(24 * 7);
            schedule.emplace_back(replacementPlanning, "Plan for device replacement");
        }

        // Sort schedule by time
        std::sort(schedule.begin(), schedule.end());

    } catch (const std::exception& e) {
        spdlog::error("Error predicting maintenance schedule for {}: {}", devicePath, e.what());
    }

    return schedule;
}

CostBenefitAnalysis analyzeCostBenefit(const std::string& devicePath, float replacementCost) {
    CostBenefitAnalysis analysis;
    analysis.replacementCost = replacementCost;

    try {
        auto currentMetrics = getCurrentPerformanceMetrics(devicePath);
        if (!currentMetrics) {
            analysis.recommendation = "Unable to analyze - insufficient data";
            return analysis;
        }

        // Estimate current maintenance costs based on health score
        float healthFactor = (100.0f - currentMetrics->healthScore) / 100.0f;
        analysis.currentMaintenanceCost = replacementCost * 0.1f * healthFactor;  // 10% of replacement cost

        // Project future maintenance costs
        analysis.projectedMaintenanceCost = analysis.currentMaintenanceCost * 2.0f;  // Assume doubling

        // Estimate energy savings (newer drives are more efficient)
        analysis.energySavings = replacementCost * 0.05f;  // 5% of replacement cost annually

        // Estimate performance gain
        if (currentMetrics->totalIOPS < 100.0) {
            analysis.performanceGain = replacementCost * 0.15f;  // 15% productivity gain
        }

        // Calculate total benefits
        float totalBenefits = analysis.projectedMaintenanceCost - analysis.currentMaintenanceCost +
                            analysis.energySavings + analysis.performanceGain;

        // Determine recommendation
        if (totalBenefits > replacementCost) {
            analysis.recommendReplacement = true;
            analysis.paybackPeriodDays = static_cast<uint32_t>((replacementCost / totalBenefits) * 365);
            analysis.recommendation = "Replacement recommended - payback in " +
                                    std::to_string(analysis.paybackPeriodDays) + " days";
        } else {
            analysis.recommendation = "Continue with current device - replacement not cost-effective";
        }

    } catch (const std::exception& e) {
        spdlog::error("Error analyzing cost-benefit for {}: {}", devicePath, e.what());
        analysis.recommendation = "Analysis failed - manual review required";
    }

    return analysis;
}

std::vector<std::string> optimizeDiskConfiguration(const std::string& devicePath,
                                                  const UsagePattern& usagePattern) {
    std::vector<std::string> recommendations;

    try {
        // Analyze usage patterns for optimization opportunities
        if (usagePattern.readWriteRatio > 3.0) {
            recommendations.push_back("Read-heavy workload detected - enable read caching");
            recommendations.push_back("Consider read-optimized storage configuration");
        } else if (usagePattern.readWriteRatio < 0.5) {
            recommendations.push_back("Write-heavy workload detected - enable write caching");
            recommendations.push_back("Consider write-optimized storage configuration");
        }

        // Temporal optimization
        if (usagePattern.peakUsageHour >= 9 && usagePattern.peakUsageHour <= 17) {
            recommendations.push_back("Business hours peak detected - schedule maintenance during off-hours");
            recommendations.push_back("Consider load balancing during peak hours");
        }

        // Growth-based optimization
        if (usagePattern.avgDailyGrowthMB > 1000) {  // > 1GB per day
            recommendations.push_back("High growth rate - implement data archiving strategy");
            recommendations.push_back("Consider tiered storage for older data");
        }

        // Access pattern optimization
        if (usagePattern.sequentialRandomRatio > 2.0) {
            recommendations.push_back("Sequential access pattern - optimize for throughput");
            recommendations.push_back("Increase read-ahead buffer size");
        } else if (usagePattern.sequentialRandomRatio < 0.5) {
            recommendations.push_back("Random access pattern - optimize for IOPS");
            recommendations.push_back("Consider SSD upgrade for better random performance");
        }

        // Capacity optimization
        if (usagePattern.daysUntilFull < 90) {
            recommendations.push_back("Capacity critical - immediate expansion required");
            recommendations.push_back("Implement data compression if not already enabled");
        } else if (usagePattern.daysUntilFull < 180) {
            recommendations.push_back("Plan capacity expansion within 6 months");
        }

        if (recommendations.empty()) {
            recommendations.push_back("Configuration appears optimal for current usage patterns");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error optimizing disk configuration for {}: {}", devicePath, e.what());
        recommendations.push_back("Optimization analysis failed - manual review recommended");
    }

    return recommendations;
}

}  // namespace atom::system
