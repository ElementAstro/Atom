/*
 * disk_performance.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Performance Monitoring

**************************************************/

#include "atom/sysinfo/disk/disk_performance.hpp"
#include "atom/sysinfo/disk/disk_info.hpp"
#include "atom/sysinfo/disk/disk_util.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_map>

#ifdef __linux__
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

namespace {
// Helper function to calculate statistics
template<typename T>
struct Statistics {
    T min{};
    T max{};
    T avg{};
    T stddev{};
    
    Statistics() = default;
    
    explicit Statistics(const std::vector<T>& values) {
        if (values.empty()) return;
        
        min = *std::min_element(values.begin(), values.end());
        max = *std::max_element(values.begin(), values.end());
        avg = std::accumulate(values.begin(), values.end(), T{}) / values.size();
        
        if (values.size() > 1) {
            T variance = 0;
            for (const auto& value : values) {
                variance += (value - avg) * (value - avg);
            }
            variance /= (values.size() - 1);
            stddev = std::sqrt(variance);
        }
    }
};

// Performance data cache
std::mutex g_performanceCacheMutex;
std::unordered_map<std::string, std::vector<ExtendedPerformanceMetrics>> g_performanceHistory;
constexpr size_t MAX_HISTORY_SIZE = 10000;

void addToHistory(const std::string& devicePath, const ExtendedPerformanceMetrics& metrics) {
    std::lock_guard<std::mutex> lock(g_performanceCacheMutex);
    auto& history = g_performanceHistory[devicePath];
    history.push_back(metrics);
    
    // Keep history size manageable
    if (history.size() > MAX_HISTORY_SIZE) {
        history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY_SIZE));
    }
}

std::vector<ExtendedPerformanceMetrics> getHistory(const std::string& devicePath, 
                                                  std::chrono::hours duration) {
    std::lock_guard<std::mutex> lock(g_performanceCacheMutex);
    auto it = g_performanceHistory.find(devicePath);
    if (it == g_performanceHistory.end()) {
        return {};
    }
    
    const auto cutoffTime = std::chrono::system_clock::now() - duration;
    std::vector<ExtendedPerformanceMetrics> result;
    
    for (const auto& metrics : it->second) {
        if (metrics.timestamp >= cutoffTime) {
            result.push_back(metrics);
        }
    }
    
    return result;
}
}  // anonymous namespace

std::optional<ExtendedPerformanceMetrics> getCurrentPerformanceMetrics(const std::string& devicePath) {
    try {
        ExtendedPerformanceMetrics metrics;
        
        // Get basic SMART data
        auto smartData = getDiskSmartData(devicePath);
        if (smartData) {
            metrics.basic = *smartData;
        }
        
#ifdef __linux__
        // Parse /proc/diskstats for detailed I/O statistics
        std::string deviceName = devicePath;
        size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }
        
        std::ifstream diskstats("/proc/diskstats");
        std::string line;
        while (std::getline(diskstats, line)) {
            std::istringstream iss(line);
            std::string major, minor, name;
            uint64_t readOps, readMerges, readSectors, readTicks;
            uint64_t writeOps, writeMerges, writeSectors, writeTicks;
            uint64_t inFlight, ioTicks, timeInQueue;
            
            if (iss >> major >> minor >> name >> readOps >> readMerges >> readSectors >> readTicks >>
                      writeOps >> writeMerges >> writeSectors >> writeTicks >> inFlight >> ioTicks >> timeInQueue) {
                
                if (name == deviceName) {
                    metrics.basic.readOperations = readOps;
                    metrics.basic.writeOperations = writeOps;
                    metrics.basic.readBytes = readSectors * 512;
                    metrics.basic.writeBytes = writeSectors * 512;
                    metrics.basic.readLatencyMs = readTicks;
                    metrics.basic.writeLatencyMs = writeTicks;
                    metrics.basic.queueDepth = static_cast<uint32_t>(inFlight);
                    
                    // Calculate derived metrics
                    if (ioTicks > 0) {
                        metrics.diskUtilization = (static_cast<float>(ioTicks) / 1000.0f) * 100.0f;
                    }
                    
                    // Calculate IOPS and throughput (these would need time-based calculations in real implementation)
                    metrics.readIOPS = static_cast<double>(readOps);
                    metrics.writeIOPS = static_cast<double>(writeOps);
                    metrics.totalIOPS = metrics.readIOPS + metrics.writeIOPS;
                    
                    constexpr double MB = 1024.0 * 1024.0;
                    metrics.readThroughputMBps = static_cast<double>(metrics.basic.readBytes) / MB;
                    metrics.writeThroughputMBps = static_cast<double>(metrics.basic.writeBytes) / MB;
                    metrics.totalThroughputMBps = metrics.readThroughputMBps + metrics.writeThroughputMBps;
                    
                    break;
                }
            }
        }
        
        // Get temperature if available
        auto temp = getDiskTemperature(devicePath);
        if (temp) {
            metrics.basic.temperature = *temp;
        }
        
#endif
        
        // Calculate health score (simplified algorithm)
        metrics.healthScore = 100.0f;
        if (metrics.basic.temperature > 60.0f) {
            metrics.healthScore -= (metrics.basic.temperature - 60.0f) * 2.0f;
        }
        
        // Predict lifespan (very simplified)
        if (metrics.healthScore > 90.0f) {
            metrics.predictedLifespanDays = 1825;  // 5 years
        } else if (metrics.healthScore > 70.0f) {
            metrics.predictedLifespanDays = 1095;  // 3 years
        } else if (metrics.healthScore > 50.0f) {
            metrics.predictedLifespanDays = 365;   // 1 year
        } else {
            metrics.predictedLifespanDays = 90;    // 3 months
        }
        
        return metrics;
        
    } catch (const std::exception& e) {
        spdlog::error("Error getting performance metrics for {}: {}", devicePath, e.what());
        return std::nullopt;
    }
}

ExtendedPerformanceMetrics performBenchmark(const std::string& devicePath, const BenchmarkConfig& config) {
    ExtendedPerformanceMetrics result;
    
    try {
        spdlog::info("Starting performance benchmark for device: {}", devicePath);
        
        const size_t testSize = static_cast<size_t>(config.testSizeMB) * 1024 * 1024;
        const size_t blockSize = static_cast<size_t>(config.blockSizeKB) * 1024;
        
        std::vector<char> testData(blockSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        // Fill test data with random content
        std::generate(testData.begin(), testData.end(), [&]() {
            return static_cast<char>(dis(gen));
        });
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Write test
        if (config.writeTest) {
            const std::string testFile = devicePath + "_benchmark_test.tmp";
            std::ofstream file(testFile, std::ios::binary);
            
            if (file.is_open()) {
                auto writeStart = std::chrono::high_resolution_clock::now();
                
                for (size_t written = 0; written < testSize; written += blockSize) {
                    file.write(testData.data(), blockSize);
                    file.flush();
                }
                
                auto writeEnd = std::chrono::high_resolution_clock::now();
                auto writeDuration = std::chrono::duration_cast<std::chrono::microseconds>(writeEnd - writeStart);
                
                result.basic.writeOperations = testSize / blockSize;
                result.basic.writeBytes = testSize;
                result.avgWriteLatencyUs = writeDuration.count() / result.basic.writeOperations;
                result.writeThroughputMBps = (static_cast<double>(testSize) / (1024.0 * 1024.0)) / 
                                           (writeDuration.count() / 1000000.0);
                
                file.close();
                std::remove(testFile.c_str());
            }
        }
        
        // Read test (using existing file or device)
        if (config.readTest) {
            std::ifstream file(devicePath, std::ios::binary);
            if (file.is_open()) {
                auto readStart = std::chrono::high_resolution_clock::now();
                
                std::vector<char> readBuffer(blockSize);
                size_t totalRead = 0;
                
                while (totalRead < testSize && file.read(readBuffer.data(), blockSize)) {
                    totalRead += file.gcount();
                }
                
                auto readEnd = std::chrono::high_resolution_clock::now();
                auto readDuration = std::chrono::duration_cast<std::chrono::microseconds>(readEnd - readStart);
                
                result.basic.readOperations = totalRead / blockSize;
                result.basic.readBytes = totalRead;
                result.avgReadLatencyUs = readDuration.count() / result.basic.readOperations;
                result.readThroughputMBps = (static_cast<double>(totalRead) / (1024.0 * 1024.0)) / 
                                          (readDuration.count() / 1000000.0);
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        
        // Calculate IOPS
        result.readIOPS = static_cast<double>(result.basic.readOperations) / totalDuration.count();
        result.writeIOPS = static_cast<double>(result.basic.writeOperations) / totalDuration.count();
        result.totalIOPS = result.readIOPS + result.writeIOPS;
        result.totalThroughputMBps = result.readThroughputMBps + result.writeThroughputMBps;
        
        spdlog::info("Benchmark completed for {}: Read IOPS: {:.2f}, Write IOPS: {:.2f}, "
                    "Read Throughput: {:.2f} MB/s, Write Throughput: {:.2f} MB/s",
                    devicePath, result.readIOPS, result.writeIOPS, 
                    result.readThroughputMBps, result.writeThroughputMBps);
        
    } catch (const std::exception& e) {
        spdlog::error("Error during benchmark for {}: {}", devicePath, e.what());
    }
    
    return result;
}

std::future<void> startPerformanceMonitoring(const std::string& devicePath,
                                            std::function<void(const ExtendedPerformanceMetrics&)> callback,
                                            const PerformanceMonitorConfig& config) {
    return std::async(std::launch::async, [devicePath, callback, config]() {
        spdlog::info("Starting performance monitoring for device: {}", devicePath);

        while (true) {
            try {
                auto metrics = getCurrentPerformanceMetrics(devicePath);
                if (metrics) {
                    addToHistory(devicePath, *metrics);
                    callback(*metrics);
                }

                std::this_thread::sleep_for(config.sampleInterval);
            } catch (const std::exception& e) {
                spdlog::error("Error in performance monitoring for {}: {}", devicePath, e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    });
}

PerformanceTrend analyzePerformanceTrends(const std::string& devicePath, uint32_t historyDays) {
    PerformanceTrend trend;
    trend.devicePath = devicePath;

    try {
        auto history = getHistory(devicePath, std::chrono::hours(historyDays * 24));
        if (history.size() < 10) {
            spdlog::warn("Insufficient data for trend analysis: {} samples", history.size());
            return trend;
        }

        // Analyze IOPS trend
        std::vector<double> iopsSamples;
        std::vector<double> throughputSamples;
        std::vector<uint64_t> latencySamples;
        std::vector<float> healthSamples;

        for (const auto& metrics : history) {
            iopsSamples.push_back(metrics.totalIOPS);
            throughputSamples.push_back(metrics.totalThroughputMBps);
            latencySamples.push_back((metrics.avgReadLatencyUs + metrics.avgWriteLatencyUs) / 2);
            healthSamples.push_back(metrics.healthScore);
        }

        // Simple linear regression for trend analysis
        auto calculateTrend = [](const std::vector<double>& values) -> std::pair<int, float> {
            if (values.size() < 2) return {0, 0.0f};

            double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
            size_t n = values.size();

            for (size_t i = 0; i < n; ++i) {
                sumX += i;
                sumY += values[i];
                sumXY += i * values[i];
                sumX2 += i * i;
            }

            double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
            double correlation = std::abs(slope) / (std::max(1.0, *std::max_element(values.begin(), values.end())));

            int trendDirection = (slope > 0.01) ? 1 : (slope < -0.01) ? -1 : 0;
            float confidence = std::min(1.0, correlation);

            return {trendDirection, confidence};
        };

        auto [iopsTrendDir, iopsTrendConf] = calculateTrend(iopsSamples);
        trend.iopstrend = iopsTrendDir;
        trend.iopsTrendConfidence = iopsTrendConf;

        auto [throughputTrendDir, throughputTrendConf] = calculateTrend(throughputSamples);
        trend.throughputTrend = throughputTrendDir;
        trend.throughputTrendConfidence = throughputTrendConf;

        // Convert latency samples to double for trend analysis
        std::vector<double> latencyDouble(latencySamples.begin(), latencySamples.end());
        auto [latencyTrendDir, latencyTrendConf] = calculateTrend(latencyDouble);
        trend.latencyTrend = -latencyTrendDir;  // Invert because increasing latency is bad
        trend.latencyTrendConfidence = latencyTrendConf;

        std::vector<double> healthDouble(healthSamples.begin(), healthSamples.end());
        auto [healthTrendDir, healthTrendConf] = calculateTrend(healthDouble);
        trend.healthTrend = healthTrendDir;
        trend.healthTrendConfidence = healthTrendConf;

        // Generate recommendations
        if (trend.healthTrend < 0 && trend.healthTrendConfidence > 0.7f) {
            trend.recommendations.push_back("Device health is declining - consider replacement");
            if (healthSamples.back() < 50.0f) {
                trend.predictedFailureDays = 30;  // Simplified prediction
            }
        }

        if (trend.iopstrend < 0 && trend.iopsTrendConfidence > 0.6f) {
            trend.recommendations.push_back("IOPS performance is declining - check for fragmentation");
        }

        if (trend.latencyTrend < 0 && trend.latencyTrendConfidence > 0.6f) {
            trend.recommendations.push_back("Latency is increasing - consider defragmentation or optimization");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error analyzing performance trends for {}: {}", devicePath, e.what());
    }

    return trend;
}

std::pair<float, uint32_t> predictDiskHealth(const std::string& devicePath,
                                            const std::vector<ExtendedPerformanceMetrics>& metrics) {
    if (metrics.empty()) {
        return {100.0f, 1825};  // Default: healthy, 5 years
    }

    try {
        // Calculate average health score
        float avgHealth = 0.0f;
        float avgTemp = 0.0f;
        double avgIOPS = 0.0;

        for (const auto& metric : metrics) {
            avgHealth += metric.healthScore;
            avgTemp += metric.basic.temperature;
            avgIOPS += metric.totalIOPS;
        }

        avgHealth /= metrics.size();
        avgTemp /= metrics.size();
        avgIOPS /= metrics.size();

        // Health prediction algorithm (simplified)
        float healthScore = avgHealth;

        // Temperature impact
        if (avgTemp > 70.0f) {
            healthScore -= (avgTemp - 70.0f) * 2.0f;
        }

        // Performance degradation impact
        if (metrics.size() > 1) {
            double recentIOPS = 0.0;
            size_t recentCount = std::min(static_cast<size_t>(10), metrics.size());

            for (size_t i = metrics.size() - recentCount; i < metrics.size(); ++i) {
                recentIOPS += metrics[i].totalIOPS;
            }
            recentIOPS /= recentCount;

            if (avgIOPS > 0 && recentIOPS < avgIOPS * 0.8) {
                healthScore -= 10.0f;  // Performance degradation penalty
            }
        }

        healthScore = std::max(0.0f, std::min(100.0f, healthScore));

        // Predict lifespan based on health score
        uint32_t predictedDays;
        if (healthScore > 90.0f) {
            predictedDays = 1825;  // 5 years
        } else if (healthScore > 80.0f) {
            predictedDays = 1460;  // 4 years
        } else if (healthScore > 70.0f) {
            predictedDays = 1095;  // 3 years
        } else if (healthScore > 60.0f) {
            predictedDays = 730;   // 2 years
        } else if (healthScore > 50.0f) {
            predictedDays = 365;   // 1 year
        } else if (healthScore > 30.0f) {
            predictedDays = 180;   // 6 months
        } else {
            predictedDays = 90;    // 3 months
        }

        return {healthScore, predictedDays};

    } catch (const std::exception& e) {
        spdlog::error("Error predicting disk health for {}: {}", devicePath, e.what());
        return {50.0f, 365};  // Conservative estimate
    }
}

bool detectPerformanceAnomaly(const ExtendedPerformanceMetrics& baseline,
                             const ExtendedPerformanceMetrics& current,
                             float threshold) {
    try {
        // Check IOPS deviation
        if (baseline.totalIOPS > 0) {
            float iopsDiff = std::abs(current.totalIOPS - baseline.totalIOPS) / baseline.totalIOPS;
            if (iopsDiff > threshold * 0.1f) {  // 10% * threshold
                return true;
            }
        }

        // Check throughput deviation
        if (baseline.totalThroughputMBps > 0) {
            float throughputDiff = std::abs(current.totalThroughputMBps - baseline.totalThroughputMBps) /
                                 baseline.totalThroughputMBps;
            if (throughputDiff > threshold * 0.1f) {
                return true;
            }
        }

        // Check latency deviation
        uint64_t baselineLatency = (baseline.avgReadLatencyUs + baseline.avgWriteLatencyUs) / 2;
        uint64_t currentLatency = (current.avgReadLatencyUs + current.avgWriteLatencyUs) / 2;

        if (baselineLatency > 0) {
            float latencyDiff = static_cast<float>(std::abs(static_cast<int64_t>(currentLatency - baselineLatency))) /
                              baselineLatency;
            if (latencyDiff > threshold * 0.2f) {  // 20% * threshold
                return true;
            }
        }

        // Check temperature deviation
        if (baseline.basic.temperature > 0) {
            float tempDiff = std::abs(current.basic.temperature - baseline.basic.temperature);
            if (tempDiff > threshold * 5.0f) {  // 5°C * threshold
                return true;
            }
        }

        return false;

    } catch (const std::exception& e) {
        spdlog::error("Error detecting performance anomaly: {}", e.what());
        return false;
    }
}

std::vector<std::string> optimizePerformanceSettings(const std::string& devicePath,
                                                    const ExtendedPerformanceMetrics& currentMetrics) {
    std::vector<std::string> recommendations;

    try {
        // Check IOPS performance
        if (currentMetrics.totalIOPS < 100.0) {
            recommendations.push_back("Consider enabling write caching to improve IOPS");
            recommendations.push_back("Check for background processes that may be affecting performance");
        }

        // Check throughput
        if (currentMetrics.totalThroughputMBps < 50.0) {
            recommendations.push_back("Consider using larger block sizes for sequential operations");
            recommendations.push_back("Check disk alignment and partition boundaries");
        }

        // Check latency
        uint64_t avgLatency = (currentMetrics.avgReadLatencyUs + currentMetrics.avgWriteLatencyUs) / 2;
        if (avgLatency > 10000) {  // 10ms
            recommendations.push_back("High latency detected - consider disk defragmentation");
            recommendations.push_back("Check for disk errors using SMART diagnostics");
        }

        // Check temperature
        if (currentMetrics.basic.temperature > 60.0f) {
            recommendations.push_back("High temperature detected - improve cooling");
            if (currentMetrics.basic.temperature > 70.0f) {
                recommendations.push_back("Critical temperature - immediate cooling required");
            }
        }

        // Check utilization
        if (currentMetrics.diskUtilization > 90.0f) {
            recommendations.push_back("High disk utilization - consider load balancing");
            recommendations.push_back("Monitor for I/O bottlenecks");
        }

        // Check health score
        if (currentMetrics.healthScore < 80.0f) {
            recommendations.push_back("Health score is declining - backup important data");
            if (currentMetrics.healthScore < 50.0f) {
                recommendations.push_back("Critical health status - plan for disk replacement");
            }
        }

        // Queue depth optimization
        if (currentMetrics.avgQueueDepth < 1.0) {
            recommendations.push_back("Consider increasing queue depth for better parallelism");
        } else if (currentMetrics.avgQueueDepth > 32.0) {
            recommendations.push_back("High queue depth may indicate I/O congestion");
        }

        if (recommendations.empty()) {
            recommendations.push_back("Performance appears optimal - no immediate optimizations needed");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error generating optimization recommendations: {}", e.what());
        recommendations.push_back("Error analyzing performance - manual inspection recommended");
    }

    return recommendations;
}

// PerformanceManager implementation
class PerformanceManager::Impl {
public:
    PerformanceMonitorConfig config;
    std::atomic<bool> monitoringActive{false};
    std::future<void> monitoringFuture;
    std::vector<std::string> monitoredDevices;
    mutable std::mutex devicesMutex;
    std::atomic<size_t> samplesCollected{0};
    std::atomic<size_t> anomaliesDetected{0};
    std::chrono::system_clock::time_point startTime;

    Impl() : startTime(std::chrono::system_clock::now()) {}

    explicit Impl(const PerformanceMonitorConfig& cfg)
        : config(cfg), startTime(std::chrono::system_clock::now()) {}

    void addDevice(const std::string& devicePath) {
        std::lock_guard<std::mutex> lock(devicesMutex);
        if (std::find(monitoredDevices.begin(), monitoredDevices.end(), devicePath) == monitoredDevices.end()) {
            monitoredDevices.push_back(devicePath);
        }
    }

    void removeDevice(const std::string& devicePath) {
        std::lock_guard<std::mutex> lock(devicesMutex);
        monitoredDevices.erase(
            std::remove(monitoredDevices.begin(), monitoredDevices.end(), devicePath),
            monitoredDevices.end());
    }

    void startMonitoring(std::function<void(const std::string&, const ExtendedPerformanceMetrics&)> callback) {
        if (monitoringActive.load()) {
            return;
        }

        monitoringActive = true;
        monitoringFuture = std::async(std::launch::async, [this, callback]() {
            while (monitoringActive.load()) {
                try {
                    std::vector<std::string> devices;
                    {
                        std::lock_guard<std::mutex> lock(devicesMutex);
                        devices = monitoredDevices;
                    }

                    for (const auto& devicePath : devices) {
                        auto metrics = getCurrentPerformanceMetrics(devicePath);
                        if (metrics) {
                            addToHistory(devicePath, *metrics);
                            ++samplesCollected;
                            callback(devicePath, *metrics);

                            // Check for anomalies
                            auto history = getHistory(devicePath, std::chrono::hours(1));
                            if (history.size() > 10) {
                                // Use average of last 10 samples as baseline
                                ExtendedPerformanceMetrics baseline;
                                for (size_t i = history.size() - 10; i < history.size() - 1; ++i) {
                                    baseline.totalIOPS += history[i].totalIOPS;
                                    baseline.totalThroughputMBps += history[i].totalThroughputMBps;
                                    baseline.avgReadLatencyUs += history[i].avgReadLatencyUs;
                                    baseline.avgWriteLatencyUs += history[i].avgWriteLatencyUs;
                                    baseline.basic.temperature += history[i].basic.temperature;
                                }
                                baseline.totalIOPS /= 9;
                                baseline.totalThroughputMBps /= 9;
                                baseline.avgReadLatencyUs /= 9;
                                baseline.avgWriteLatencyUs /= 9;
                                baseline.basic.temperature /= 9;

                                if (detectPerformanceAnomaly(baseline, *metrics, config.anomalyThreshold)) {
                                    ++anomaliesDetected;
                                    spdlog::warn("Performance anomaly detected for device: {}", devicePath);
                                }
                            }
                        }
                    }

                    std::this_thread::sleep_for(config.sampleInterval);
                } catch (const std::exception& e) {
                    spdlog::error("Error in performance monitoring: {}", e.what());
                }
            }
        });
    }

    void stopMonitoring() {
        monitoringActive = false;
    }

    PerformanceManager::MonitoringStats getStats() const {
        const auto now = std::chrono::system_clock::now();
        const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

        std::lock_guard<std::mutex> lock(devicesMutex);
        return {monitoredDevices.size(), samplesCollected.load(), anomaliesDetected.load(), startTime, uptime};
    }
};

PerformanceManager::PerformanceManager() : pImpl(std::make_unique<Impl>()) {}

PerformanceManager::PerformanceManager(const PerformanceMonitorConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {}

PerformanceManager::~PerformanceManager() {
    stopMonitoring();
}

PerformanceManager::PerformanceManager(PerformanceManager&&) noexcept = default;

PerformanceManager& PerformanceManager::operator=(PerformanceManager&&) noexcept = default;

void PerformanceManager::addDevice(const std::string& devicePath) {
    pImpl->addDevice(devicePath);
}

void PerformanceManager::removeDevice(const std::string& devicePath) {
    pImpl->removeDevice(devicePath);
}

void PerformanceManager::startMonitoring(std::function<void(const std::string&, const ExtendedPerformanceMetrics&)> callback) {
    pImpl->startMonitoring(callback);
}

void PerformanceManager::stopMonitoring() {
    pImpl->stopMonitoring();
}

std::optional<ExtendedPerformanceMetrics> PerformanceManager::getCurrentMetrics(const std::string& devicePath) const {
    return getCurrentPerformanceMetrics(devicePath);
}

std::vector<ExtendedPerformanceMetrics> PerformanceManager::getPerformanceHistory(
    const std::string& devicePath, std::chrono::hours duration) const {
    return getHistory(devicePath, duration);
}

ExtendedPerformanceMetrics PerformanceManager::benchmark(const std::string& devicePath, const BenchmarkConfig& config) {
    return performBenchmark(devicePath, config);
}

PerformanceTrend PerformanceManager::analyzeTrends(const std::string& devicePath, uint32_t historyDays) const {
    return analyzePerformanceTrends(devicePath, historyDays);
}

auto PerformanceManager::getStats() const -> MonitoringStats {
    return pImpl->getStats();
}

void PerformanceManager::updateConfig(const PerformanceMonitorConfig& config) {
    pImpl->config = config;
}

const PerformanceMonitorConfig& PerformanceManager::getConfig() const {
    return pImpl->config;
}

}  // namespace atom::system
