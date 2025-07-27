/**
 * @file common.cpp
 * @brief Common implementation for memory information module
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "common.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>
#include <numeric>
#include <mutex>
#include <unordered_map>
#include <fstream>

#include <spdlog/spdlog.h>
#include "memory.hpp"


#ifdef _WIN32
#include <processthreadsapi.h>
#include <windows.h>
#endif

namespace atom::system {
namespace internal {

std::atomic<bool> g_monitoringActive(false);
std::atomic<bool> g_pressureMonitoringActive(false);
std::vector<MemoryPressureInfo> g_pressureHistory;

// Memory allocation tracking globals
std::atomic<bool> g_allocationTrackingActive(false);
AllocationTracker g_allocationTracker;
std::mutex g_allocationMutex;
std::unordered_map<void*, MemoryAllocation> g_activeAllocations;

// Memory compression monitoring globals
std::atomic<bool> g_compressionMonitoringActive(false);
std::vector<MemoryCompressionInfo> g_compressionHistory;

// Enhanced leak detection globals
std::atomic<bool> g_leakMonitoringActive(false);
std::vector<MemoryLeak> g_detectedLeaks;
std::vector<MemoryInfo> g_memoryHistory;
std::mutex g_leakDetectionMutex;

auto formatByteSize(unsigned long long bytes) -> std::string {
    static constexpr const char* UNITS[] = {"B",  "KB", "MB", "GB",
                                            "TB", "PB", "EB", "ZB", "YB"};
    static constexpr int MAX_UNIT_INDEX = 8;

    if (bytes == 0) {
        return "0 B";
    }

    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < MAX_UNIT_INDEX) {
        size /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " "
        << UNITS[unitIndex];
    return oss.str();
}

/**
 * @brief Enhanced byte size formatting with custom options
 */
auto formatByteSizeAdvanced(unsigned long long bytes, bool binary = true,
                           int precision = 2, bool showUnit = true) -> std::string {
    if (bytes == 0) {
        return showUnit ? "0 B" : "0";
    }

    static constexpr const char* BINARY_UNITS[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    static constexpr const char* DECIMAL_UNITS[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    static constexpr int MAX_UNIT_INDEX = 8;

    const char* const* units = binary ? BINARY_UNITS : DECIMAL_UNITS;
    const double divisor = binary ? 1024.0 : 1000.0;

    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= divisor && unitIndex < MAX_UNIT_INDEX) {
        size /= divisor;
        ++unitIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << size;

    if (showUnit) {
        oss << " " << units[unitIndex];
    }

    return oss.str();
}

/**
 * @brief Format memory bandwidth with appropriate units
 */
auto formatBandwidth(double bytesPerSecond) -> std::string {
    static constexpr const char* UNITS[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    static constexpr int MAX_UNIT_INDEX = 4;

    if (bytesPerSecond <= 0.0) {
        return "0 B/s";
    }

    int unitIndex = 0;
    double rate = bytesPerSecond;

    while (rate >= 1024.0 && unitIndex < MAX_UNIT_INDEX) {
        rate /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << rate << " " << UNITS[unitIndex];
    return oss.str();
}

/**
 * @brief Format time duration with appropriate units
 */
auto formatDuration(std::chrono::nanoseconds duration) -> std::string {
    auto ns = duration.count();

    if (ns < 1000) {
        return std::to_string(ns) + " ns";
    } else if (ns < 1000000) {
        return std::to_string(ns / 1000) + " μs";
    } else if (ns < 1000000000) {
        return std::to_string(ns / 1000000) + " ms";
    } else {
        return std::to_string(ns / 1000000000) + " s";
    }
}

/**
 * @brief Parse byte size string to bytes
 */
auto parseByteSize(const std::string& sizeStr) -> unsigned long long {
    if (sizeStr.empty()) {
        return 0;
    }

    std::string str = sizeStr;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);

    // Extract numeric part
    size_t pos = 0;
    double value = std::stod(str, &pos);

    if (value < 0) {
        return 0;
    }

    // Extract unit part
    std::string unit = str.substr(pos);
    unit.erase(std::remove_if(unit.begin(), unit.end(), ::isspace), unit.end());

    unsigned long long multiplier = 1;

    if (unit == "B" || unit.empty()) {
        multiplier = 1;
    } else if (unit == "KB" || unit == "K") {
        multiplier = 1000;
    } else if (unit == "KIB") {
        multiplier = 1024;
    } else if (unit == "MB" || unit == "M") {
        multiplier = 1000 * 1000;
    } else if (unit == "MIB") {
        multiplier = 1024 * 1024;
    } else if (unit == "GB" || unit == "G") {
        multiplier = 1000 * 1000 * 1000;
    } else if (unit == "GIB") {
        multiplier = 1024 * 1024 * 1024;
    } else if (unit == "TB" || unit == "T") {
        multiplier = 1000ULL * 1000 * 1000 * 1000;
    } else if (unit == "TIB") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    }

    return static_cast<unsigned long long>(value * multiplier);
}

/**
 * @brief Enhanced memory benchmarking with statistical analysis
 */
auto benchmarkMemoryAdvanced(size_t testSizeBytes, int iterations,
                           const std::string& pattern, bool warmup = true) -> std::map<std::string, double> {
    std::map<std::string, double> results;

    if (testSizeBytes == 0 || iterations <= 0) {
        return results;
    }

    std::vector<double> readTimes, writeTimes, copyTimes;
    std::vector<int> testData(testSizeBytes / sizeof(int));

    // Warmup if requested
    if (warmup) {
        for (size_t i = 0; i < testData.size() / 10; ++i) {
            testData[i] = static_cast<int>(i);
        }
    }

    for (int iter = 0; iter < iterations; ++iter) {
        // Write benchmark
        auto start = std::chrono::high_resolution_clock::now();
        if (pattern == "sequential" || pattern == "mixed") {
            for (size_t i = 0; i < testData.size(); ++i) {
                testData[i] = static_cast<int>(i);
            }
        } else if (pattern == "random") {
            for (int i = 0; i < 10000; ++i) {
                size_t idx = (static_cast<size_t>(i) * 7919) % testData.size();
                testData[idx] = i;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end - start).count();
        writeTimes.push_back(testSizeBytes / (duration * 1024.0 * 1024.0));

        // Read benchmark
        start = std::chrono::high_resolution_clock::now();
        volatile int sum = 0;
        if (pattern == "sequential" || pattern == "mixed") {
            for (size_t i = 0; i < testData.size(); ++i) {
                sum += testData[i];
            }
        } else if (pattern == "random") {
            for (int i = 0; i < 10000; ++i) {
                size_t idx = (static_cast<size_t>(i) * 7919) % testData.size();
                sum += testData[idx];
            }
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration<double>(end - start).count();
        readTimes.push_back(testSizeBytes / (duration * 1024.0 * 1024.0));
        (void)sum; // Suppress unused warning

        // Copy benchmark
        std::vector<int> copyData(testData.size());
        start = std::chrono::high_resolution_clock::now();
        std::copy(testData.begin(), testData.end(), copyData.begin());
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration<double>(end - start).count();
        copyTimes.push_back(testSizeBytes / (duration * 1024.0 * 1024.0));
    }

    // Calculate statistics
    auto calculateStats = [](const std::vector<double>& values) -> std::map<std::string, double> {
        std::map<std::string, double> stats;
        if (values.empty()) return stats;

        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        double mean = sum / values.size();

        double variance = 0.0;
        for (double val : values) {
            variance += (val - mean) * (val - mean);
        }
        variance /= values.size();

        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());

        stats["mean"] = mean;
        stats["min"] = sorted.front();
        stats["max"] = sorted.back();
        stats["median"] = sorted[sorted.size() / 2];
        stats["stddev"] = std::sqrt(variance);
        stats["p95"] = sorted[static_cast<size_t>(sorted.size() * 0.95)];
        stats["p99"] = sorted[static_cast<size_t>(sorted.size() * 0.99)];

        return stats;
    };

    auto readStats = calculateStats(readTimes);
    auto writeStats = calculateStats(writeTimes);
    auto copyStats = calculateStats(copyTimes);

    // Combine results
    for (const auto& [key, value] : readStats) {
        results["read_" + key] = value;
    }
    for (const auto& [key, value] : writeStats) {
        results["write_" + key] = value;
    }
    for (const auto& [key, value] : copyStats) {
        results["copy_" + key] = value;
    }

    return results;
}

/**
 * @brief Memory latency profiler with cache analysis
 */
auto profileMemoryLatency(size_t maxSize = 64 * 1024 * 1024) -> std::map<size_t, double> {
    std::map<size_t, double> latencyProfile;

    // Test different memory sizes to identify cache levels
    std::vector<size_t> testSizes = {
        4 * 1024,      // L1 cache size
        32 * 1024,     // Larger L1
        256 * 1024,    // L2 cache size
        2 * 1024 * 1024,   // L3 cache size
        8 * 1024 * 1024,   // Larger L3
        32 * 1024 * 1024,  // Main memory
        maxSize        // Large memory
    };

    for (size_t size : testSizes) {
        if (size > maxSize) continue;

        std::vector<int> testData(size / sizeof(int));

        // Random access pattern to measure latency
        const int accessCount = 10000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < accessCount; ++i) {
            size_t idx = (static_cast<size_t>(i) * 7919) % testData.size();
            volatile int val = testData[idx];
            (void)val;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        double avgLatency = static_cast<double>(duration.count()) / accessCount;
        latencyProfile[size] = avgLatency;
    }

    return latencyProfile;
}

/**
 * @brief System memory stress test
 */
auto stressTestMemory(std::chrono::seconds duration, size_t maxMemoryMB = 1024) -> std::map<std::string, double> {
    std::map<std::string, double> results;

    const auto startTime = std::chrono::steady_clock::now();
    const auto endTime = startTime + duration;

    std::vector<std::vector<char>> allocations;
    size_t totalAllocated = 0;
    size_t allocationCount = 0;
    size_t failedAllocations = 0;

    const size_t chunkSize = 1024 * 1024; // 1MB chunks
    const size_t maxBytes = maxMemoryMB * 1024 * 1024;

    while (std::chrono::steady_clock::now() < endTime && totalAllocated < maxBytes) {
        try {
            allocations.emplace_back(chunkSize);
            totalAllocated += chunkSize;
            allocationCount++;

            // Write to memory to ensure it's actually allocated
            std::fill(allocations.back().begin(), allocations.back().end(),
                     static_cast<char>(allocationCount % 256));

            // Occasionally free some memory to test fragmentation
            if (allocationCount % 10 == 0 && !allocations.empty()) {
                size_t freeIndex = allocationCount % allocations.size();
                totalAllocated -= allocations[freeIndex].size();
                allocations.erase(allocations.begin() + freeIndex);
            }

        } catch (const std::bad_alloc&) {
            failedAllocations++;
            break;
        }

        // Small delay to prevent overwhelming the system
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto actualDuration = std::chrono::steady_clock::now() - startTime;
    const auto durationSeconds = std::chrono::duration<double>(actualDuration).count();

    results["total_allocated_mb"] = static_cast<double>(totalAllocated) / (1024.0 * 1024.0);
    results["allocation_count"] = static_cast<double>(allocationCount);
    results["failed_allocations"] = static_cast<double>(failedAllocations);
    results["allocation_rate_per_sec"] = allocationCount / durationSeconds;
    results["throughput_mb_per_sec"] = (static_cast<double>(totalAllocated) / (1024.0 * 1024.0)) / durationSeconds;
    results["duration_seconds"] = durationSeconds;
    results["success_rate"] = (static_cast<double>(allocationCount) / (allocationCount + failedAllocations)) * 100.0;

    return results;
}

auto benchmarkMemoryPerformance(size_t testSizeBytes) -> double {
    if (testSizeBytes == 0) {
        return 0.0;
    }

    std::vector<char> testBuffer(testSizeBytes);

    const auto start = std::chrono::high_resolution_clock::now();

    std::fill(testBuffer.begin(), testBuffer.end(), static_cast<char>(0xAA));

    volatile char sum = 0;
    for (const auto& byte : testBuffer) {
        sum += byte;
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration<double>(end - start).count();

    if (duration <= 0.0) {
        return 0.0;
    }

    constexpr double BYTES_TO_MB = 1.0 / (1024.0 * 1024.0);
    const double mbProcessed =
        static_cast<double>(testSizeBytes * 2) * BYTES_TO_MB;

    return mbProcessed / duration;
}

/**
 * @brief Enhanced memory benchmarking with detailed metrics
 */
auto benchmarkMemoryDetailed(size_t testSizeBytes, int iterations,
                           const std::string& pattern) -> MemoryPerformance {
    MemoryPerformance perf{};
    perf.timestamp = std::chrono::steady_clock::now();

    if (testSizeBytes == 0 || iterations <= 0) {
        return perf;
    }

    std::vector<double> readTimes, writeTimes, randomReadTimes, randomWriteTimes;
    std::vector<double> accessTimes;

    try {
        std::vector<int> testBuffer(testSizeBytes / sizeof(int));

        for (int iter = 0; iter < iterations; ++iter) {
            // Sequential write test
            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < testBuffer.size(); ++i) {
                testBuffer[i] = static_cast<int>(i);
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end - start).count();
            writeTimes.push_back(testSizeBytes / (duration * 1024.0 * 1024.0));

            // Sequential read test
            start = std::chrono::high_resolution_clock::now();
            volatile int sum = 0;
            for (size_t i = 0; i < testBuffer.size(); ++i) {
                sum += testBuffer[i];
            }
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration<double>(end - start).count();
            readTimes.push_back(testSizeBytes / (duration * 1024.0 * 1024.0));
            (void)sum; // Suppress unused warning

            // Random access test
            if (pattern == "random" || pattern == "mixed") {
                start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < 10000; ++i) {
                    size_t idx = (static_cast<size_t>(i) * 7919) % testBuffer.size();
                    testBuffer[idx] = i;
                }
                end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration<double>(end - start).count();
                randomWriteTimes.push_back((10000 * sizeof(int)) / (duration * 1024.0 * 1024.0));

                start = std::chrono::high_resolution_clock::now();
                sum = 0;
                for (int i = 0; i < 10000; ++i) {
                    size_t idx = (static_cast<size_t>(i) * 7919) % testBuffer.size();
                    sum += testBuffer[idx];
                }
                end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration<double>(end - start).count();
                randomReadTimes.push_back((10000 * sizeof(int)) / (duration * 1024.0 * 1024.0));

                // Measure individual access times
                start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < 1000; ++i) {
                    auto accessStart = std::chrono::high_resolution_clock::now();
                    size_t idx = (static_cast<size_t>(i) * 7919) % testBuffer.size();
                    volatile int val = testBuffer[idx];
                    auto accessEnd = std::chrono::high_resolution_clock::now();
                    accessTimes.push_back(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(accessEnd - accessStart).count());
                    (void)val;
                }
            }
        }

        // Calculate averages
        if (!readTimes.empty()) {
            perf.sequentialReadSpeed = std::accumulate(readTimes.begin(), readTimes.end(), 0.0) / readTimes.size();
        }
        if (!writeTimes.empty()) {
            perf.sequentialWriteSpeed = std::accumulate(writeTimes.begin(), writeTimes.end(), 0.0) / writeTimes.size();
        }
        if (!randomReadTimes.empty()) {
            perf.randomReadSpeed = std::accumulate(randomReadTimes.begin(), randomReadTimes.end(), 0.0) / randomReadTimes.size();
        }
        if (!randomWriteTimes.empty()) {
            perf.randomWriteSpeed = std::accumulate(randomWriteTimes.begin(), randomWriteTimes.end(), 0.0) / randomWriteTimes.size();
        }

        // Set legacy fields for compatibility
        perf.readSpeed = perf.sequentialReadSpeed;
        perf.writeSpeed = perf.sequentialWriteSpeed;

        // Calculate access time statistics
        if (!accessTimes.empty()) {
            std::sort(accessTimes.begin(), accessTimes.end());
            perf.averageAccessTime = std::accumulate(accessTimes.begin(), accessTimes.end(), 0.0) / accessTimes.size();
            perf.minimumAccessTime = accessTimes.front();
            perf.peakAccessTime = accessTimes.back();
            perf.latency = perf.averageAccessTime;
        }

        // Estimate bandwidth usage
        const auto totalMemoryMB = getTotalMemorySize() / (1024.0 * 1024.0);
        perf.bandwidthUsage = std::min(100.0,
            (perf.readSpeed + perf.writeSpeed) / totalMemoryMB * 100.0);

    } catch (const std::exception& e) {
        spdlog::error("Error in detailed memory benchmark: {}", e.what());
    }

    return perf;
}

// Enhanced leak detection helper functions
auto detectGradualLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak>;
auto detectSuddenLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak>;
auto detectPeriodicLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak>;
auto detectAllocationTrackingLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak>;

}  // namespace internal

auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback)
    -> void {
    if (internal::g_monitoringActive.exchange(true)) {
        spdlog::warn("Memory monitoring is already active");
        return;
    }

    spdlog::info("Starting memory monitoring");

    std::thread monitorThread([callback = std::move(callback)]() {
        while (internal::g_monitoringActive.load()) {
            try {
                const auto info = getDetailedMemoryStats();
                callback(info);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (const std::exception& e) {
                spdlog::error("Error in memory monitoring thread: {}",
                              e.what());
                break;
            }
        }
        spdlog::info("Memory monitoring stopped");
    });

    monitorThread.detach();
}

auto stopMemoryMonitoring() -> void {
    bool expected = true;
    if (internal::g_monitoringActive.compare_exchange_strong(expected, false)) {
        spdlog::info("Stopping memory monitoring");
    } else {
        spdlog::warn("Memory monitoring is not active");
    }
}

auto getMemoryTimeline(std::chrono::minutes duration)
    -> std::vector<MemoryInfo> {
    spdlog::info("Collecting memory timeline for {} minutes", duration.count());

    const size_t reserveSize = static_cast<size_t>(duration.count() * 60);
    std::vector<MemoryInfo> timeline;
    timeline.reserve(reserveSize);

    const auto endTime = std::chrono::steady_clock::now() + duration;

    while (std::chrono::steady_clock::now() < endTime) {
        try {
            timeline.emplace_back(getDetailedMemoryStats());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            spdlog::error("Error collecting memory timeline: {}", e.what());
            break;
        }
    }

    spdlog::info("Collected {} memory samples", timeline.size());
    return timeline;
}

auto detectMemoryLeaks() -> std::vector<std::string> {
    spdlog::info("Starting memory leak detection");
    std::vector<std::string> leaks;

    const auto before = getDetailedMemoryStats();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    const auto after = getDetailedMemoryStats();

    constexpr unsigned long long LEAK_THRESHOLD = 1024 * 1024;  // 1MB

    if (after.workingSetSize > before.workingSetSize + LEAK_THRESHOLD) {
        const auto sizeDiff = after.workingSetSize - before.workingSetSize;
        leaks.emplace_back(
            "Potential memory leak detected: Working set increased by " +
            internal::formatByteSize(sizeDiff) + " in 5 seconds");
    }

    spdlog::info("Memory leak detection completed, found {} potential issues",
                 leaks.size());
    return leaks;
}

auto getMemoryFragmentation() -> double {
    spdlog::info("Calculating memory fragmentation");

    const auto total = getTotalMemorySize();
    const auto available = getAvailableMemorySize();

    if (available == 0) {
        return 0.0;
    }

    size_t allocatableSize = 0;
    try {
        constexpr size_t MAX_ALLOC_SIZE = 100 * 1024 * 1024;  // 100 MB
        const size_t testSize = std::min(MAX_ALLOC_SIZE, static_cast<size_t>(available));
        std::vector<char> testAlloc;
        testAlloc.reserve(testSize);
        allocatableSize = testAlloc.capacity();
    } catch (...) {
        allocatableSize = 0;
    }

    const double fragmentation = std::max(
        0.0, std::min(1.0, 1.0 - (static_cast<double>(allocatableSize) /
                                  static_cast<double>(available))));

    const double fragmentationPercent = fragmentation * 100.0;
    spdlog::info("Memory fragmentation estimated at {:.2f}%",
                 fragmentationPercent);
    return fragmentationPercent;
}

auto optimizeMemoryUsage() -> bool {
    spdlog::info("Attempting to optimize memory usage");

    bool success = false;

#ifdef _WIN32
    try {
        success = SetProcessWorkingSetSize(GetCurrentProcess(),
                                           static_cast<SIZE_T>(-1),
                                           static_cast<SIZE_T>(-1)) != 0;
    } catch (...) {
        spdlog::error("Failed to optimize memory on Windows");
    }
#elif defined(__linux__)
    try {
        if (auto* fp = fopen("/proc/self/oom_score_adj", "w")) {
            fprintf(fp, "500\n");
            fclose(fp);
            success = true;
        }
    } catch (...) {
        spdlog::error("Failed to optimize memory on Linux");
    }
#endif

    spdlog::info("Memory optimization {}", success ? "succeeded" : "failed");
    return success;
}

auto analyzeMemoryBottlenecks() -> std::vector<std::string> {
    spdlog::info("Analyzing memory bottlenecks");

    std::vector<std::string> bottlenecks;
    const auto perf = getMemoryPerformance();
    const auto info = getDetailedMemoryStats();

    if (info.memoryLoadPercentage > 90.0) {
        bottlenecks.emplace_back(
            "High memory usage: " +
            std::to_string(static_cast<int>(info.memoryLoadPercentage)) +
            "% of physical memory is in use");
    }

    if (info.swapMemoryTotal > 0) {
        const double swapUsagePercent =
            (static_cast<double>(info.swapMemoryUsed) / info.swapMemoryTotal) *
            100.0;

        if (swapUsagePercent > 50.0) {
            bottlenecks.emplace_back(
                "High swap usage: " +
                std::to_string(static_cast<int>(swapUsagePercent)) +
                "% of swap space is in use, indicating insufficient RAM");
        }
    }

    constexpr double LATENCY_THRESHOLD = 100.0;
    if (perf.latency > LATENCY_THRESHOLD) {
        bottlenecks.emplace_back(
            "High memory latency: " +
            std::to_string(static_cast<int>(perf.latency)) +
            " ns, may slow memory-intensive operations");
    }

    if (perf.bandwidthUsage > 80.0) {
        bottlenecks.emplace_back(
            "High memory bandwidth usage: " +
            std::to_string(static_cast<int>(perf.bandwidthUsage)) +
            "%, indicating potential bandwidth bottleneck");
    }

    const double fragPercent = getMemoryFragmentation();
    if (fragPercent > 30.0) {
        bottlenecks.emplace_back("High memory fragmentation: " +
                                 std::to_string(static_cast<int>(fragPercent)) +
                                 "%, may cause allocation failures");
    }

    spdlog::info("Memory bottleneck analysis completed, found {} issues",
                 bottlenecks.size());
    return bottlenecks;
}

auto detectMemoryPressure() -> MemoryPressureInfo {
    spdlog::debug("Detecting memory pressure");

    MemoryPressureInfo pressureInfo{};
    pressureInfo.timestamp = std::chrono::steady_clock::now();

    // Get current memory statistics
    const auto memInfo = getDetailedMemoryStats();
    const auto perf = getMemoryPerformance();

    pressureInfo.memoryUsagePercent = memInfo.memoryLoadPercentage;
    pressureInfo.swapUsagePercent = memInfo.swapMemoryTotal > 0 ?
        (static_cast<double>(memInfo.swapMemoryUsed) / memInfo.swapMemoryTotal) * 100.0 : 0.0;

    // Calculate page fault rates (simplified - would need historical data for accurate rates)
    pressureInfo.pageFaultRate = static_cast<double>(memInfo.pageFaultCount);
    pressureInfo.majorFaultRate = pressureInfo.pageFaultRate * 0.1; // Estimate major faults as 10% of total
    pressureInfo.allocationFailureRate = 0.0; // Would need kernel-level monitoring

    // Calculate pressure score based on multiple factors
    double score = 0.0;

    // Memory usage contribution (0-40 points)
    if (pressureInfo.memoryUsagePercent > 95.0) {
        score += 40.0;
        pressureInfo.factors.push_back("Critical memory usage: " +
            std::to_string(static_cast<int>(pressureInfo.memoryUsagePercent)) + "%");
    } else if (pressureInfo.memoryUsagePercent > 90.0) {
        score += 30.0;
        pressureInfo.factors.push_back("High memory usage: " +
            std::to_string(static_cast<int>(pressureInfo.memoryUsagePercent)) + "%");
    } else if (pressureInfo.memoryUsagePercent > 80.0) {
        score += 20.0;
        pressureInfo.factors.push_back("Elevated memory usage: " +
            std::to_string(static_cast<int>(pressureInfo.memoryUsagePercent)) + "%");
    } else if (pressureInfo.memoryUsagePercent > 70.0) {
        score += 10.0;
    }

    // Swap usage contribution (0-25 points)
    if (pressureInfo.swapUsagePercent > 80.0) {
        score += 25.0;
        pressureInfo.factors.push_back("Heavy swap usage: " +
            std::to_string(static_cast<int>(pressureInfo.swapUsagePercent)) + "%");
    } else if (pressureInfo.swapUsagePercent > 50.0) {
        score += 15.0;
        pressureInfo.factors.push_back("Moderate swap usage: " +
            std::to_string(static_cast<int>(pressureInfo.swapUsagePercent)) + "%");
    } else if (pressureInfo.swapUsagePercent > 20.0) {
        score += 8.0;
    }

    // Memory performance contribution (0-20 points)
    if (perf.latency > 200.0) {
        score += 20.0;
        pressureInfo.factors.push_back("High memory latency: " +
            std::to_string(static_cast<int>(perf.latency)) + " ns");
    } else if (perf.latency > 100.0) {
        score += 10.0;
    }

    if (perf.bandwidthUsage > 90.0) {
        score += 15.0;
        pressureInfo.factors.push_back("High memory bandwidth usage: " +
            std::to_string(static_cast<int>(perf.bandwidthUsage)) + "%");
    } else if (perf.bandwidthUsage > 70.0) {
        score += 8.0;
    }

    // Memory fragmentation contribution (0-15 points)
    const double fragmentation = getMemoryFragmentation();
    if (fragmentation > 50.0) {
        score += 15.0;
        pressureInfo.factors.push_back("High memory fragmentation: " +
            std::to_string(static_cast<int>(fragmentation)) + "%");
    } else if (fragmentation > 30.0) {
        score += 8.0;
        pressureInfo.factors.push_back("Moderate memory fragmentation: " +
            std::to_string(static_cast<int>(fragmentation)) + "%");
    }

    pressureInfo.pressureScore = std::min(100.0, score);

    // Determine pressure level
    if (pressureInfo.pressureScore >= 80.0) {
        pressureInfo.level = MemoryPressureLevel::CRITICAL;
    } else if (pressureInfo.pressureScore >= 60.0) {
        pressureInfo.level = MemoryPressureLevel::HIGH;
    } else if (pressureInfo.pressureScore >= 40.0) {
        pressureInfo.level = MemoryPressureLevel::MEDIUM;
    } else if (pressureInfo.pressureScore >= 20.0) {
        pressureInfo.level = MemoryPressureLevel::LOW;
    } else {
        pressureInfo.level = MemoryPressureLevel::NONE;
    }

    // Generate recommendations based on pressure level and factors
    switch (pressureInfo.level) {
        case MemoryPressureLevel::CRITICAL:
            pressureInfo.recommendations.push_back("Immediately close unnecessary applications");
            pressureInfo.recommendations.push_back("Consider adding more RAM");
            pressureInfo.recommendations.push_back("Check for memory leaks in running processes");
            pressureInfo.recommendations.push_back("Increase swap space if possible");
            break;
        case MemoryPressureLevel::HIGH:
            pressureInfo.recommendations.push_back("Close non-essential applications");
            pressureInfo.recommendations.push_back("Monitor memory usage closely");
            pressureInfo.recommendations.push_back("Consider memory optimization");
            break;
        case MemoryPressureLevel::MEDIUM:
            pressureInfo.recommendations.push_back("Monitor memory-intensive applications");
            pressureInfo.recommendations.push_back("Consider closing unused browser tabs");
            break;
        case MemoryPressureLevel::LOW:
            pressureInfo.recommendations.push_back("Memory usage is elevated but manageable");
            break;
        case MemoryPressureLevel::NONE:
            pressureInfo.recommendations.push_back("Memory usage is optimal");
            break;
    }

    spdlog::debug("Memory pressure detected: level={}, score={:.1f}",
                  static_cast<int>(pressureInfo.level), pressureInfo.pressureScore);

    return pressureInfo;
}

auto getMemoryPressureLevel() -> MemoryPressureLevel {
    const auto pressureInfo = detectMemoryPressure();
    return pressureInfo.level;
}

auto startMemoryPressureMonitoring(
    std::function<void(const MemoryPressureInfo&)> callback,
    MemoryPressureLevel threshold) -> void {

    if (internal::g_pressureMonitoringActive.exchange(true)) {
        spdlog::warn("Memory pressure monitoring is already active");
        return;
    }

    spdlog::info("Starting memory pressure monitoring with threshold level {}",
                 static_cast<int>(threshold));

    std::thread monitorThread([callback = std::move(callback), threshold]() {
        MemoryPressureLevel lastLevel = MemoryPressureLevel::NONE;

        while (internal::g_pressureMonitoringActive.load()) {
            try {
                const auto pressureInfo = detectMemoryPressure();

                // Store in history
                internal::g_pressureHistory.push_back(pressureInfo);
                if (internal::g_pressureHistory.size() > 1000) { // Keep last 1000 entries
                    internal::g_pressureHistory.erase(internal::g_pressureHistory.begin());
                }

                // Call callback if pressure level changed or exceeds threshold
                if (pressureInfo.level != lastLevel || pressureInfo.level >= threshold) {
                    callback(pressureInfo);
                    lastLevel = pressureInfo.level;
                }

                // Adjust monitoring frequency based on pressure level
                std::chrono::seconds sleepDuration(5); // Default 5 seconds
                switch (pressureInfo.level) {
                    case MemoryPressureLevel::CRITICAL:
                        sleepDuration = std::chrono::seconds(1);
                        break;
                    case MemoryPressureLevel::HIGH:
                        sleepDuration = std::chrono::seconds(2);
                        break;
                    case MemoryPressureLevel::MEDIUM:
                        sleepDuration = std::chrono::seconds(3);
                        break;
                    default:
                        sleepDuration = std::chrono::seconds(5);
                        break;
                }

                std::this_thread::sleep_for(sleepDuration);

            } catch (const std::exception& e) {
                spdlog::error("Error in memory pressure monitoring thread: {}", e.what());
                break;
            }
        }
        spdlog::info("Memory pressure monitoring stopped");
    });

    monitorThread.detach();
}

auto stopMemoryPressureMonitoring() -> void {
    bool expected = true;
    if (internal::g_pressureMonitoringActive.compare_exchange_strong(expected, false)) {
        spdlog::info("Stopping memory pressure monitoring");
    } else {
        spdlog::warn("Memory pressure monitoring is not active");
    }
}

auto getMemoryPressureHistory(std::chrono::minutes duration) -> std::vector<MemoryPressureInfo> {
    const auto cutoffTime = std::chrono::steady_clock::now() - duration;
    std::vector<MemoryPressureInfo> result;

    for (const auto& entry : internal::g_pressureHistory) {
        if (entry.timestamp >= cutoffTime) {
            result.push_back(entry);
        }
    }

    spdlog::info("Retrieved {} memory pressure history entries for {} minutes",
                 result.size(), duration.count());
    return result;
}

auto getEnhancedMemoryPerformance() -> MemoryPerformance {
    spdlog::debug("Getting enhanced memory performance metrics");

    // Start with basic performance metrics
    auto perf = getMemoryPerformance();

    // Add enhanced benchmarking
    const auto detailedPerf = internal::benchmarkMemoryDetailed(32 * 1024 * 1024, 3, "mixed");

    // Merge results
    perf.sequentialReadSpeed = detailedPerf.sequentialReadSpeed;
    perf.sequentialWriteSpeed = detailedPerf.sequentialWriteSpeed;
    perf.randomReadSpeed = detailedPerf.randomReadSpeed;
    perf.randomWriteSpeed = detailedPerf.randomWriteSpeed;
    perf.averageAccessTime = detailedPerf.averageAccessTime;
    perf.minimumAccessTime = detailedPerf.minimumAccessTime;
    perf.peakAccessTime = detailedPerf.peakAccessTime;

    // Estimate cache hit rates (simplified - would need hardware counters for accuracy)
    perf.l1CacheHitRate = 95.0; // Typical L1 hit rate
    perf.l2CacheHitRate = 85.0; // Typical L2 hit rate
    perf.l3CacheHitRate = 70.0; // Typical L3 hit rate
    perf.tlbHitRate = 98.0;     // Typical TLB hit rate

    // Estimate memory controller utilization based on bandwidth usage
    perf.memoryControllerUtilization = perf.bandwidthUsage * 0.8; // Conservative estimate

    // Get system-wide statistics
    try {
        const auto memInfo = getDetailedMemoryStats();
        perf.pageFaultsPerSecond = memInfo.pageFaultCount; // Would need rate calculation
        perf.majorFaultsPerSecond = perf.pageFaultsPerSecond * 0.1; // Estimate

        // Estimate context switches (would need /proc/stat monitoring)
        perf.contextSwitchesPerSecond = 1000; // Placeholder

        // Bandwidth breakdown
        perf.systemBandwidthUsage = perf.bandwidthUsage;
        perf.processBandwidthUsage = perf.bandwidthUsage * 0.3; // Estimate current process usage

    } catch (const std::exception& e) {
        spdlog::warn("Error getting enhanced memory stats: {}", e.what());
    }

    perf.timestamp = std::chrono::steady_clock::now();

    spdlog::debug("Enhanced memory performance: seq_read={:.1f} MB/s, seq_write={:.1f} MB/s, "
                  "rand_read={:.1f} MB/s, rand_write={:.1f} MB/s, avg_latency={:.1f} ns",
                  perf.sequentialReadSpeed, perf.sequentialWriteSpeed,
                  perf.randomReadSpeed, perf.randomWriteSpeed, perf.averageAccessTime);

    return perf;
}

auto benchmarkMemoryPerformance(size_t testSizeBytes, int iterations,
                               const std::string& testPattern) -> MemoryPerformance {
    spdlog::info("Running memory benchmark: size={} MB, iterations={}, pattern={}",
                 testSizeBytes / (1024 * 1024), iterations, testPattern);

    return internal::benchmarkMemoryDetailed(testSizeBytes, iterations, testPattern);
}

auto getMemoryCacheStats() -> MemoryPerformance {
    spdlog::debug("Getting memory cache statistics");

    MemoryPerformance perf{};
    perf.timestamp = std::chrono::steady_clock::now();

    try {
        // Perform cache-specific benchmarks
        constexpr size_t L1_SIZE = 32 * 1024;    // Typical L1 cache size
        constexpr size_t L2_SIZE = 256 * 1024;   // Typical L2 cache size
        constexpr size_t L3_SIZE = 8 * 1024 * 1024; // Typical L3 cache size

        // Test L1 cache performance
        auto l1Perf = internal::benchmarkMemoryDetailed(L1_SIZE, 5, "sequential");

        // Test L2 cache performance
        auto l2Perf = internal::benchmarkMemoryDetailed(L2_SIZE, 5, "sequential");

        // Test L3 cache performance
        auto l3Perf = internal::benchmarkMemoryDetailed(L3_SIZE, 3, "sequential");

        // Test main memory performance
        auto memPerf = internal::benchmarkMemoryDetailed(64 * 1024 * 1024, 2, "sequential");

        // Calculate cache hit rates based on performance differences
        if (memPerf.sequentialReadSpeed > 0) {
            perf.l3CacheHitRate = std::min(99.0, (l3Perf.sequentialReadSpeed / memPerf.sequentialReadSpeed) * 100.0);
            perf.l2CacheHitRate = std::min(99.0, (l2Perf.sequentialReadSpeed / l3Perf.sequentialReadSpeed) * 100.0);
            perf.l1CacheHitRate = std::min(99.0, (l1Perf.sequentialReadSpeed / l2Perf.sequentialReadSpeed) * 100.0);
        }

        // TLB hit rate estimation (simplified)
        auto randomPerf = internal::benchmarkMemoryDetailed(4 * 1024 * 1024, 3, "random");
        auto seqPerf = internal::benchmarkMemoryDetailed(4 * 1024 * 1024, 3, "sequential");

        if (seqPerf.sequentialReadSpeed > 0) {
            perf.tlbHitRate = std::min(99.0, (randomPerf.randomReadSpeed / seqPerf.sequentialReadSpeed) * 100.0);
        }

        // Copy other relevant metrics
        perf.readSpeed = l1Perf.sequentialReadSpeed;
        perf.writeSpeed = l1Perf.sequentialWriteSpeed;
        perf.latency = l1Perf.averageAccessTime;

        spdlog::debug("Cache statistics: L1={:.1f}%, L2={:.1f}%, L3={:.1f}%, TLB={:.1f}%",
                      perf.l1CacheHitRate, perf.l2CacheHitRate, perf.l3CacheHitRate, perf.tlbHitRate);

    } catch (const std::exception& e) {
        spdlog::error("Error measuring cache statistics: {}", e.what());
        // Set default values
        perf.l1CacheHitRate = 95.0;
        perf.l2CacheHitRate = 85.0;
        perf.l3CacheHitRate = 70.0;
        perf.tlbHitRate = 98.0;
    }

    return perf;
}

auto startAllocationTracking(bool trackCallStacks) -> bool {
    spdlog::info("Starting memory allocation tracking (call stacks: {})", trackCallStacks);

    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    if (internal::g_allocationTrackingActive.exchange(true)) {
        spdlog::warn("Memory allocation tracking is already active");
        return false;
    }

    // Initialize tracker
    internal::g_allocationTracker = AllocationTracker{};
    internal::g_allocationTracker.isActive = true;
    internal::g_allocationTracker.startTime = std::chrono::steady_clock::now();
    internal::g_allocationTracker.lastUpdate = internal::g_allocationTracker.startTime;

    // Clear previous allocations
    internal::g_activeAllocations.clear();

    spdlog::info("Memory allocation tracking started successfully");
    return true;
}

auto stopAllocationTracking() -> AllocationTracker {
    spdlog::info("Stopping memory allocation tracking");

    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    internal::g_allocationTrackingActive = false;
    internal::g_allocationTracker.isActive = false;
    internal::g_allocationTracker.lastUpdate = std::chrono::steady_clock::now();

    // Analyze remaining allocations for potential leaks
    for (const auto& [address, allocation] : internal::g_activeAllocations) {
        const auto now = std::chrono::steady_clock::now();
        const auto age = std::chrono::duration_cast<std::chrono::minutes>(now - allocation.timestamp);

        // Consider allocations older than 10 minutes as potential leaks
        if (age.count() > 10) {
            internal::g_allocationTracker.suspectedLeaks.push_back(allocation);
        }
    }

    spdlog::info("Memory allocation tracking stopped. Total allocations: {}, "
                 "Current allocations: {}, Suspected leaks: {}",
                 internal::g_allocationTracker.totalAllocations,
                 internal::g_allocationTracker.currentAllocations,
                 internal::g_allocationTracker.suspectedLeaks.size());

    return internal::g_allocationTracker;
}

auto getAllocationTracker() -> AllocationTracker {
    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);
    internal::g_allocationTracker.lastUpdate = std::chrono::steady_clock::now();
    return internal::g_allocationTracker;
}

auto analyzeHeapMemory() -> HeapInfo {
    spdlog::debug("Analyzing heap memory");

    HeapInfo heapInfo{};
    heapInfo.timestamp = std::chrono::steady_clock::now();

    try {
        // Get basic memory information
        const auto memInfo = getDetailedMemoryStats();

        // Estimate heap information (simplified - would need platform-specific APIs for accuracy)
        heapInfo.totalHeapSize = memInfo.workingSetSize;
        heapInfo.usedHeapSize = static_cast<size_t>(memInfo.workingSetSize * 0.8); // Estimate
        heapInfo.freeHeapSize = heapInfo.totalHeapSize - heapInfo.usedHeapSize;

        // Analyze active allocations if tracking is enabled
        if (internal::g_allocationTrackingActive) {
            std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

            heapInfo.numberOfAllocations = internal::g_activeAllocations.size();

            // Build allocation size histogram
            for (const auto& [address, allocation] : internal::g_activeAllocations) {
                // Round to nearest power of 2 for histogram
                size_t bucket = 1;
                while (bucket < allocation.size) {
                    bucket <<= 1;
                }
                heapInfo.allocationSizeHistogram[bucket]++;
            }

            // Estimate fragmentation based on allocation patterns
            if (heapInfo.numberOfAllocations > 0) {
                size_t totalAllocatedSize = 0;
                for (const auto& [address, allocation] : internal::g_activeAllocations) {
                    totalAllocatedSize += allocation.size;
                }

                if (totalAllocatedSize > 0) {
                    heapInfo.fragmentationRatio = 1.0 - (static_cast<double>(totalAllocatedSize) / heapInfo.usedHeapSize);
                }
            }
        } else {
            // Estimate fragmentation using memory fragmentation function
            heapInfo.fragmentationRatio = getMemoryFragmentation() / 100.0;
        }

        // Estimate largest free block (simplified)
        heapInfo.largestFreeBlock = heapInfo.freeHeapSize / 2; // Conservative estimate
        heapInfo.numberOfFreeBlocks = 10; // Placeholder

        // Add common memory pools (simplified)
        heapInfo.memoryPools.emplace_back("System Heap", heapInfo.totalHeapSize);
        heapInfo.memoryPools.emplace_back("Stack", 8 * 1024 * 1024); // Typical stack size

        spdlog::debug("Heap analysis: total={} bytes, used={} bytes, allocations={}, fragmentation={:.2f}",
                      heapInfo.totalHeapSize, heapInfo.usedHeapSize,
                      heapInfo.numberOfAllocations, heapInfo.fragmentationRatio);

    } catch (const std::exception& e) {
        spdlog::error("Error analyzing heap memory: {}", e.what());
    }

    return heapInfo;
}

auto getMemoryAllocations() -> std::vector<MemoryAllocation> {
    std::vector<MemoryAllocation> allocations;

    if (!internal::g_allocationTrackingActive) {
        spdlog::warn("Memory allocation tracking is not active");
        return allocations;
    }

    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    allocations.reserve(internal::g_activeAllocations.size());
    for (const auto& [address, allocation] : internal::g_activeAllocations) {
        allocations.push_back(allocation);
    }

    spdlog::debug("Retrieved {} active memory allocations", allocations.size());
    return allocations;
}

auto detectMemoryLeaksAdvanced() -> std::vector<MemoryAllocation> {
    spdlog::info("Performing advanced memory leak detection");

    std::vector<MemoryAllocation> leaks;

    if (!internal::g_allocationTrackingActive) {
        spdlog::warn("Memory allocation tracking is not active - using basic leak detection");
        const auto basicLeaks = detectMemoryLeaks();
        // Convert string descriptions to MemoryAllocation objects (simplified)
        for (const auto& leak : basicLeaks) {
            MemoryAllocation allocation{};
            allocation.allocationType = "Unknown";
            allocation.sourceLocation = leak;
            allocation.timestamp = std::chrono::steady_clock::now();
            leaks.push_back(allocation);
        }
        return leaks;
    }

    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    const auto now = std::chrono::steady_clock::now();

    for (const auto& [address, allocation] : internal::g_activeAllocations) {
        bool isLeak = false;

        // Check allocation age
        const auto age = std::chrono::duration_cast<std::chrono::minutes>(now - allocation.timestamp);
        if (age.count() > 30) { // Allocations older than 30 minutes
            isLeak = true;
        }

        // Check allocation size (very large allocations might be leaks)
        if (allocation.size > 100 * 1024 * 1024) { // > 100MB
            isLeak = true;
        }

        // Check allocation pattern (many small allocations from same location)
        int sameLocationCount = 0;
        for (const auto& [otherAddress, otherAllocation] : internal::g_activeAllocations) {
            if (otherAddress != address &&
                otherAllocation.sourceLocation == allocation.sourceLocation &&
                otherAllocation.size == allocation.size) {
                sameLocationCount++;
            }
        }

        if (sameLocationCount > 1000) { // Many similar allocations
            isLeak = true;
        }

        if (isLeak) {
            leaks.push_back(allocation);
        }
    }

    spdlog::info("Advanced leak detection found {} potential leaks out of {} allocations",
                 leaks.size(), internal::g_activeAllocations.size());

    return leaks;
}

auto profileMemoryAllocations(std::chrono::minutes duration) -> HeapInfo {
    spdlog::info("Profiling memory allocations for {} minutes", duration.count());

    if (!internal::g_allocationTrackingActive) {
        spdlog::warn("Memory allocation tracking is not active - starting tracking");
        startAllocationTracking(false);
    }

    const auto startTime = std::chrono::steady_clock::now();
    const auto endTime = startTime + duration;

    // Take initial snapshot
    const auto initialAllocations = internal::g_activeAllocations.size();

    // Wait for profiling duration
    std::this_thread::sleep_for(duration);

    // Analyze allocation patterns
    HeapInfo profileInfo = analyzeHeapMemory();

    // Add profiling-specific information
    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    const auto finalAllocations = internal::g_activeAllocations.size();
    const auto allocationGrowth = finalAllocations - initialAllocations;

    // Add growth information to memory pools
    profileInfo.memoryPools.emplace_back("Allocation Growth", allocationGrowth * sizeof(MemoryAllocation));

    // Analyze allocation frequency by source location
    std::map<std::string, size_t> locationFrequency;
    for (const auto& [address, allocation] : internal::g_activeAllocations) {
        if (allocation.timestamp >= startTime && allocation.timestamp <= endTime) {
            locationFrequency[allocation.sourceLocation]++;
        }
    }

    // Find hot spots (top allocation sources)
    std::vector<std::pair<std::string, size_t>> hotSpots(locationFrequency.begin(), locationFrequency.end());
    std::sort(hotSpots.begin(), hotSpots.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Add hot spots to memory pools for reporting
    for (size_t i = 0; i < std::min(size_t(5), hotSpots.size()); ++i) {
        profileInfo.memoryPools.emplace_back("Hot Spot " + std::to_string(i + 1) + ": " + hotSpots[i].first,
                                           hotSpots[i].second);
    }

    spdlog::info("Memory allocation profiling completed. Growth: {} allocations, Hot spots: {}",
                 allocationGrowth, std::min(size_t(5), hotSpots.size()));

    return profileInfo;
}

auto detectMemoryCompression() -> MemoryCompressionInfo {
    spdlog::debug("Detecting memory compression status");

    MemoryCompressionInfo compressionInfo{};
    compressionInfo.timestamp = std::chrono::steady_clock::now();

    try {
        // Initialize with default values
        compressionInfo.isSupported = false;
        compressionInfo.isEnabled = false;
        compressionInfo.algorithm = CompressionAlgorithm::NONE;

#ifdef __linux__
        // Check for zswap (Linux memory compression)
        std::ifstream zswapEnabled("/sys/module/zswap/parameters/enabled");
        if (zswapEnabled.is_open()) {
            std::string enabled;
            zswapEnabled >> enabled;
            compressionInfo.isSupported = true;
            compressionInfo.isEnabled = (enabled == "Y" || enabled == "1");

            if (compressionInfo.isEnabled) {
                // Try to determine compression algorithm
                std::ifstream zswapCompressor("/sys/module/zswap/parameters/compressor");
                if (zswapCompressor.is_open()) {
                    std::string compressor;
                    zswapCompressor >> compressor;

                    if (compressor == "lz4" || compressor == "lz4hc") {
                        compressionInfo.algorithm = CompressionAlgorithm::LZ4;
                    } else if (compressor == "zstd") {
                        compressionInfo.algorithm = CompressionAlgorithm::ZSTD;
                    } else if (compressor == "lzo" || compressor == "lzo-rle") {
                        compressionInfo.algorithm = CompressionAlgorithm::LZO;
                    } else if (compressor == "deflate") {
                        compressionInfo.algorithm = CompressionAlgorithm::DEFLATE;
                    } else {
                        compressionInfo.algorithm = CompressionAlgorithm::UNKNOWN;
                    }
                }

                // Get zswap statistics
                std::ifstream zswapStats("/sys/kernel/debug/zswap/pool_total_size");
                if (zswapStats.is_open()) {
                    size_t poolSize;
                    zswapStats >> poolSize;
                    compressionInfo.compressedSize = poolSize;
                }

                // Try to get more detailed statistics
                std::ifstream zswapStored("/sys/kernel/debug/zswap/stored_pages");
                if (zswapStored.is_open()) {
                    size_t storedPages;
                    zswapStored >> storedPages;
                    compressionInfo.compressedPages = storedPages;
                    compressionInfo.originalSize = storedPages * 4096; // Assume 4KB pages
                }
            }
        }

        // Check for zram (compressed RAM disk)
        std::ifstream zramStat("/sys/block/zram0/stat");
        if (zramStat.is_open()) {
            compressionInfo.isSupported = true;

            // Check if zram is active
            std::ifstream zramDisksize("/sys/block/zram0/disksize");
            if (zramDisksize.is_open()) {
                size_t disksize;
                zramDisksize >> disksize;
                if (disksize > 0) {
                    compressionInfo.isEnabled = true;

                    // Get compression statistics
                    std::ifstream zramCompAlgo("/sys/block/zram0/comp_algorithm");
                    if (zramCompAlgo.is_open()) {
                        std::string algo;
                        std::getline(zramCompAlgo, algo);

                        // Parse algorithm (format: "lzo [lz4] lz4hc zstd")
                        if (algo.find("[lz4]") != std::string::npos) {
                            compressionInfo.algorithm = CompressionAlgorithm::LZ4;
                        } else if (algo.find("[zstd]") != std::string::npos) {
                            compressionInfo.algorithm = CompressionAlgorithm::ZSTD;
                        } else if (algo.find("[lzo]") != std::string::npos) {
                            compressionInfo.algorithm = CompressionAlgorithm::LZO;
                        }
                    }

                    // Get compression ratio
                    std::ifstream zramOrigSize("/sys/block/zram0/orig_data_size");
                    std::ifstream zramCompSize("/sys/block/zram0/compr_data_size");

                    if (zramOrigSize.is_open() && zramCompSize.is_open()) {
                        size_t origSize, compSize;
                        zramOrigSize >> origSize;
                        zramCompSize >> compSize;

                        compressionInfo.originalSize = origSize;
                        compressionInfo.compressedSize = compSize;

                        if (origSize > 0) {
                            compressionInfo.compressionRatio = static_cast<double>(compSize) / origSize;
                            compressionInfo.spaceSavings = (1.0 - compressionInfo.compressionRatio) * 100.0;
                            compressionInfo.savedBytes = origSize - compSize;
                        }
                    }
                }
            }
        }

#elif defined(_WIN32)
        // Windows memory compression detection
        // Note: This would require Windows-specific APIs and is simplified here
        compressionInfo.isSupported = true; // Windows 10+ supports memory compression

        // Try to detect if memory compression is enabled (simplified)
        // In reality, this would require querying Windows performance counters
        // or using undocumented APIs
        compressionInfo.isEnabled = true; // Assume enabled on modern Windows
        compressionInfo.algorithm = CompressionAlgorithm::UNKNOWN; // Windows uses proprietary algorithm

#elif defined(__APPLE__)
        // macOS memory compression detection
        // macOS uses memory compression but doesn't expose detailed statistics
        compressionInfo.isSupported = true;
        compressionInfo.isEnabled = true; // macOS always uses memory compression
        compressionInfo.algorithm = CompressionAlgorithm::UNKNOWN; // Apple proprietary

#endif

        // Calculate efficiency metrics
        if (compressionInfo.isEnabled && compressionInfo.originalSize > 0 && compressionInfo.compressedSize > 0) {
            compressionInfo.compressionEfficiency =
                (static_cast<double>(compressionInfo.savedBytes) / compressionInfo.originalSize) * 100.0;
        }

        // Estimate performance metrics (simplified - would need actual benchmarking)
        if (compressionInfo.isEnabled) {
            switch (compressionInfo.algorithm) {
                case CompressionAlgorithm::LZ4:
                    compressionInfo.compressionLatency = 5.0;    // microseconds
                    compressionInfo.decompressionLatency = 3.0;
                    compressionInfo.compressionThroughput = 2000.0; // MB/s
                    compressionInfo.decompressionThroughput = 3000.0;
                    compressionInfo.cpuOverhead = 2.0; // percent
                    break;
                case CompressionAlgorithm::ZSTD:
                    compressionInfo.compressionLatency = 15.0;
                    compressionInfo.decompressionLatency = 8.0;
                    compressionInfo.compressionThroughput = 800.0;
                    compressionInfo.decompressionThroughput = 1500.0;
                    compressionInfo.cpuOverhead = 5.0;
                    break;
                case CompressionAlgorithm::LZO:
                    compressionInfo.compressionLatency = 8.0;
                    compressionInfo.decompressionLatency = 4.0;
                    compressionInfo.compressionThroughput = 1500.0;
                    compressionInfo.decompressionThroughput = 2500.0;
                    compressionInfo.cpuOverhead = 3.0;
                    break;
                default:
                    compressionInfo.compressionLatency = 10.0;
                    compressionInfo.decompressionLatency = 6.0;
                    compressionInfo.compressionThroughput = 1000.0;
                    compressionInfo.decompressionThroughput = 2000.0;
                    compressionInfo.cpuOverhead = 4.0;
                    break;
            }

            compressionInfo.memoryOverhead = compressionInfo.originalSize * 0.05; // 5% overhead estimate
        }

        spdlog::debug("Memory compression: supported={}, enabled={}, algorithm={}, ratio={:.2f}",
                      compressionInfo.isSupported, compressionInfo.isEnabled,
                      static_cast<int>(compressionInfo.algorithm), compressionInfo.compressionRatio);

    } catch (const std::exception& e) {
        spdlog::error("Error detecting memory compression: {}", e.what());
    }

    return compressionInfo;
}

auto isMemoryCompressionSupported() -> bool {
    const auto compressionInfo = detectMemoryCompression();
    return compressionInfo.isSupported;
}

auto getMemoryCompressionAlgorithm() -> CompressionAlgorithm {
    const auto compressionInfo = detectMemoryCompression();
    return compressionInfo.algorithm;
}

auto benchmarkMemoryCompression(size_t testSizeBytes, CompressionAlgorithm algorithm) -> MemoryCompressionInfo {
    spdlog::info("Benchmarking memory compression: size={} MB, algorithm={}",
                 testSizeBytes / (1024 * 1024), static_cast<int>(algorithm));

    MemoryCompressionInfo benchmarkInfo{};
    benchmarkInfo.timestamp = std::chrono::steady_clock::now();
    benchmarkInfo.isSupported = true;
    benchmarkInfo.isEnabled = true;
    benchmarkInfo.algorithm = algorithm;

    try {
        // Generate test data with varying compressibility
        std::vector<char> testData(testSizeBytes);

        // Fill with semi-random data (more realistic than pure random)
        for (size_t i = 0; i < testSizeBytes; ++i) {
            testData[i] = static_cast<char>((i % 256) ^ ((i / 256) % 256));
        }

        // Simulate compression based on algorithm characteristics
        double compressionRatio = 0.5; // Default 50% compression
        double compressionSpeed = 1000.0; // MB/s
        double decompressionSpeed = 2000.0; // MB/s

        switch (algorithm) {
            case CompressionAlgorithm::LZ4:
                compressionRatio = 0.6; // LZ4 is fast but less compression
                compressionSpeed = 2500.0;
                decompressionSpeed = 3500.0;
                break;
            case CompressionAlgorithm::ZSTD:
                compressionRatio = 0.4; // ZSTD has better compression
                compressionSpeed = 800.0;
                decompressionSpeed = 1800.0;
                break;
            case CompressionAlgorithm::LZO:
                compressionRatio = 0.55;
                compressionSpeed = 2000.0;
                decompressionSpeed = 3000.0;
                break;
            case CompressionAlgorithm::SNAPPY:
                compressionRatio = 0.65;
                compressionSpeed = 2200.0;
                decompressionSpeed = 3200.0;
                break;
            case CompressionAlgorithm::DEFLATE:
                compressionRatio = 0.35;
                compressionSpeed = 400.0;
                decompressionSpeed = 800.0;
                break;
            default:
                // Use default values
                break;
        }

        // Simulate compression timing
        const auto compressionStart = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::microseconds(
            static_cast<long>(testSizeBytes / (compressionSpeed * 1024.0 * 1024.0) * 1000000.0)));
        const auto compressionEnd = std::chrono::high_resolution_clock::now();

        // Simulate decompression timing
        const auto decompressionStart = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::microseconds(
            static_cast<long>(testSizeBytes / (decompressionSpeed * 1024.0 * 1024.0) * 1000000.0)));
        const auto decompressionEnd = std::chrono::high_resolution_clock::now();

        // Calculate metrics
        benchmarkInfo.originalSize = testSizeBytes;
        benchmarkInfo.compressedSize = static_cast<size_t>(testSizeBytes * compressionRatio);
        benchmarkInfo.savedBytes = benchmarkInfo.originalSize - benchmarkInfo.compressedSize;
        benchmarkInfo.compressionRatio = compressionRatio;
        benchmarkInfo.spaceSavings = (1.0 - compressionRatio) * 100.0;
        benchmarkInfo.compressionEfficiency = benchmarkInfo.spaceSavings;

        // Calculate latencies
        benchmarkInfo.compressionLatency =
            std::chrono::duration_cast<std::chrono::microseconds>(compressionEnd - compressionStart).count();
        benchmarkInfo.decompressionLatency =
            std::chrono::duration_cast<std::chrono::microseconds>(decompressionEnd - decompressionStart).count();

        // Calculate throughput
        const double testSizeMB = testSizeBytes / (1024.0 * 1024.0);
        benchmarkInfo.compressionThroughput = testSizeMB / (benchmarkInfo.compressionLatency / 1000000.0);
        benchmarkInfo.decompressionThroughput = testSizeMB / (benchmarkInfo.decompressionLatency / 1000000.0);

        // Estimate CPU overhead
        benchmarkInfo.cpuOverhead = (benchmarkInfo.compressionLatency + benchmarkInfo.decompressionLatency) / 10000.0;
        benchmarkInfo.memoryOverhead = testSizeBytes * 0.1; // 10% overhead estimate

        spdlog::info("Compression benchmark completed: ratio={:.2f}, compression={:.1f} MB/s, decompression={:.1f} MB/s",
                     benchmarkInfo.compressionRatio, benchmarkInfo.compressionThroughput,
                     benchmarkInfo.decompressionThroughput);

    } catch (const std::exception& e) {
        spdlog::error("Error in compression benchmark: {}", e.what());
    }

    return benchmarkInfo;
}

auto startMemoryCompressionMonitoring(std::function<void(const MemoryCompressionInfo&)> callback) -> void {
    if (internal::g_compressionMonitoringActive.exchange(true)) {
        spdlog::warn("Memory compression monitoring is already active");
        return;
    }

    spdlog::info("Starting memory compression monitoring");

    std::thread monitorThread([callback = std::move(callback)]() {
        while (internal::g_compressionMonitoringActive.load()) {
            try {
                const auto compressionInfo = detectMemoryCompression();

                // Store in history
                internal::g_compressionHistory.push_back(compressionInfo);
                if (internal::g_compressionHistory.size() > 1000) { // Keep last 1000 entries
                    internal::g_compressionHistory.erase(internal::g_compressionHistory.begin());
                }

                callback(compressionInfo);
                std::this_thread::sleep_for(std::chrono::seconds(5));

            } catch (const std::exception& e) {
                spdlog::error("Error in compression monitoring thread: {}", e.what());
                break;
            }
        }
        spdlog::info("Memory compression monitoring stopped");
    });

    monitorThread.detach();
}

auto stopMemoryCompressionMonitoring() -> void {
    bool expected = true;
    if (internal::g_compressionMonitoringActive.compare_exchange_strong(expected, false)) {
        spdlog::info("Stopping memory compression monitoring");
    } else {
        spdlog::warn("Memory compression monitoring is not active");
    }
}

auto getMemoryCompressionHistory(std::chrono::minutes duration) -> std::vector<MemoryCompressionInfo> {
    const auto cutoffTime = std::chrono::steady_clock::now() - duration;
    std::vector<MemoryCompressionInfo> result;

    for (const auto& entry : internal::g_compressionHistory) {
        if (entry.timestamp >= cutoffTime) {
            result.push_back(entry);
        }
    }

    spdlog::info("Retrieved {} memory compression history entries for {} minutes",
                 result.size(), duration.count());
    return result;
}

auto detectNumaTopology() -> NumaTopology {
    spdlog::debug("Detecting NUMA topology");

    NumaTopology topology{};
    topology.timestamp = std::chrono::steady_clock::now();
    topology.isNumaSystem = false;
    topology.nodeCount = 1;

    try {
#ifdef __linux__
        // Check if NUMA is available
        std::ifstream numaNodes("/sys/devices/system/node/possible");
        if (numaNodes.is_open()) {
            std::string nodeRange;
            std::getline(numaNodes, nodeRange);

            if (!nodeRange.empty() && nodeRange != "0") {
                topology.isNumaSystem = true;

                // Parse node range (e.g., "0-3" or "0,2-3")
                std::vector<int> nodeIds;
                std::istringstream iss(nodeRange);
                std::string token;

                while (std::getline(iss, token, ',')) {
                    if (token.find('-') != std::string::npos) {
                        // Range format (e.g., "0-3")
                        size_t dashPos = token.find('-');
                        int start = std::stoi(token.substr(0, dashPos));
                        int end = std::stoi(token.substr(dashPos + 1));
                        for (int i = start; i <= end; ++i) {
                            nodeIds.push_back(i);
                        }
                    } else {
                        // Single node
                        nodeIds.push_back(std::stoi(token));
                    }
                }

                topology.nodeCount = nodeIds.size();

                // Gather information for each NUMA node
                for (int nodeId : nodeIds) {
                    NumaNode node{};
                    node.nodeId = nodeId;
                    node.timestamp = topology.timestamp;

                    // Get memory information
                    std::ifstream nodeMeminfo("/sys/devices/system/node/node" + std::to_string(nodeId) + "/meminfo");
                    if (nodeMeminfo.is_open()) {
                        std::string line;
                        while (std::getline(nodeMeminfo, line)) {
                            if (line.find("MemTotal:") != std::string::npos) {
                                std::istringstream iss(line);
                                std::string label, value, unit;
                                iss >> label >> value >> unit;
                                node.totalMemory = std::stoull(value) * 1024; // Convert kB to bytes
                            } else if (line.find("MemFree:") != std::string::npos) {
                                std::istringstream iss(line);
                                std::string label, value, unit;
                                iss >> label >> value >> unit;
                                node.freeMemory = std::stoull(value) * 1024;
                            }
                        }

                        node.usedMemory = node.totalMemory - node.freeMemory;
                        if (node.totalMemory > 0) {
                            node.memoryUsagePercent = (static_cast<double>(node.usedMemory) / node.totalMemory) * 100.0;
                        }
                    }

                    // Get CPU list
                    std::ifstream nodeCpulist("/sys/devices/system/node/node" + std::to_string(nodeId) + "/cpulist");
                    if (nodeCpulist.is_open()) {
                        std::string cpuRange;
                        std::getline(nodeCpulist, cpuRange);

                        // Parse CPU range (similar to node range parsing)
                        std::istringstream cpuIss(cpuRange);
                        std::string cpuToken;

                        while (std::getline(cpuIss, cpuToken, ',')) {
                            if (cpuToken.find('-') != std::string::npos) {
                                size_t dashPos = cpuToken.find('-');
                                int start = std::stoi(cpuToken.substr(0, dashPos));
                                int end = std::stoi(cpuToken.substr(dashPos + 1));
                                for (int i = start; i <= end; ++i) {
                                    node.cpuList.push_back(i);
                                }
                            } else {
                                node.cpuList.push_back(std::stoi(cpuToken));
                            }
                        }

                        node.cpuCount = node.cpuList.size();
                    }

                    // Get distance information
                    std::ifstream nodeDistance("/sys/devices/system/node/node" + std::to_string(nodeId) + "/distance");
                    if (nodeDistance.is_open()) {
                        std::string distanceLine;
                        std::getline(nodeDistance, distanceLine);

                        std::istringstream distIss(distanceLine);
                        std::string distance;
                        int targetNode = 0;

                        while (distIss >> distance) {
                            node.nodeDistances[targetNode] = std::stoi(distance);
                            targetNode++;
                        }
                    }

                    // Estimate performance characteristics
                    node.localAccessLatency = 100.0; // Base latency in nanoseconds
                    node.localBandwidth = 25000.0;   // Base bandwidth in MB/s

                    // Calculate remote access characteristics based on distances
                    for (const auto& [remoteNode, distance] : node.nodeDistances) {
                        if (remoteNode != nodeId) {
                            // Higher distance = higher latency and lower bandwidth
                            double latencyMultiplier = 1.0 + (distance - 10) * 0.1;
                            double bandwidthMultiplier = 1.0 / (1.0 + (distance - 10) * 0.05);

                            node.remoteAccessLatency[remoteNode] = node.localAccessLatency * latencyMultiplier;
                            node.remoteBandwidth[remoteNode] = node.localBandwidth * bandwidthMultiplier;
                        }
                    }

                    // Get NUMA statistics if available
                    std::ifstream numastat("/sys/devices/system/node/node" + std::to_string(nodeId) + "/numastat");
                    if (numastat.is_open()) {
                        std::string line;
                        while (std::getline(numastat, line)) {
                            if (line.find("numa_hit") != std::string::npos) {
                                std::istringstream iss(line);
                                std::string label, value;
                                iss >> label >> value;
                                node.localAllocations = std::stoull(value);
                            } else if (line.find("numa_miss") != std::string::npos) {
                                std::istringstream iss(line);
                                std::string label, value;
                                iss >> label >> value;
                                node.remoteAllocations = std::stoull(value);
                            }
                        }

                        // Calculate NUMA hit ratio
                        size_t totalAccesses = node.localAllocations + node.remoteAllocations;
                        if (totalAccesses > 0) {
                            node.numaHitRatio = (static_cast<double>(node.localAllocations) / totalAccesses) * 100.0;
                        }
                    }

                    topology.nodes.push_back(node);
                }

                // Calculate system-wide statistics
                size_t totalLocalAllocations = 0;
                size_t totalRemoteAllocations = 0;

                for (const auto& node : topology.nodes) {
                    totalLocalAllocations += node.localAllocations;
                    totalRemoteAllocations += node.remoteAllocations;
                }

                topology.totalNumaAllocations = totalLocalAllocations + totalRemoteAllocations;
                topology.totalRemoteAccesses = totalRemoteAllocations;

                if (topology.totalNumaAllocations > 0) {
                    topology.overallNumaHitRatio =
                        (static_cast<double>(totalLocalAllocations) / topology.totalNumaAllocations) * 100.0;
                }

                // Get migration statistics
                std::ifstream vmstat("/proc/vmstat");
                if (vmstat.is_open()) {
                    std::string line;
                    while (std::getline(vmstat, line)) {
                        if (line.find("numa_pte_updates") != std::string::npos) {
                            std::istringstream iss(line);
                            std::string label, value;
                            iss >> label >> value;
                            topology.automaticMigrations = std::stoull(value);
                        } else if (line.find("pgmigrate_success") != std::string::npos) {
                            std::istringstream iss(line);
                            std::string label, value;
                            iss >> label >> value;
                            topology.pageMigrations = std::stoull(value);
                        }
                    }
                }

                // Calculate performance impact
                if (topology.overallNumaHitRatio < 90.0) {
                    topology.numaPerformanceImpact = (90.0 - topology.overallNumaHitRatio) * 0.5;
                }

                // Generate recommendations
                if (topology.overallNumaHitRatio < 80.0) {
                    topology.recommendations.push_back("Consider enabling NUMA balancing: echo 1 > /proc/sys/kernel/numa_balancing");
                    topology.recommendations.push_back("Use numactl to bind processes to specific NUMA nodes");
                }

                if (topology.totalRemoteAccesses > topology.totalNumaAllocations * 0.2) {
                    topology.recommendations.push_back("High remote memory access detected - consider memory locality optimization");
                }
            }
        }

#elif defined(_WIN32)
        // Windows NUMA detection (simplified)
        topology.isNumaSystem = true; // Assume modern Windows systems support NUMA
        topology.nodeCount = 1; // Would need Windows API calls for actual detection

#elif defined(__APPLE__)
        // macOS typically doesn't expose NUMA information
        topology.isNumaSystem = false;
        topology.nodeCount = 1;
#endif

        // If not a NUMA system, create a single node representing the entire system
        if (!topology.isNumaSystem) {
            NumaNode singleNode{};
            singleNode.nodeId = 0;
            singleNode.totalMemory = getTotalMemorySize();
            singleNode.freeMemory = getAvailableMemorySize();
            singleNode.usedMemory = singleNode.totalMemory - singleNode.freeMemory;
            singleNode.memoryUsagePercent = (static_cast<double>(singleNode.usedMemory) / singleNode.totalMemory) * 100.0;
            singleNode.cpuCount = std::thread::hardware_concurrency();
            singleNode.localAccessLatency = 100.0;
            singleNode.localBandwidth = 25000.0;
            singleNode.numaHitRatio = 100.0; // All accesses are "local"
            singleNode.timestamp = topology.timestamp;

            topology.nodes.push_back(singleNode);
            topology.overallNumaHitRatio = 100.0;
        }

        spdlog::debug("NUMA topology: isNuma={}, nodes={}, hitRatio={:.1f}%",
                      topology.isNumaSystem, topology.nodeCount, topology.overallNumaHitRatio);

    } catch (const std::exception& e) {
        spdlog::error("Error detecting NUMA topology: {}", e.what());

        // Fallback to single node
        topology.isNumaSystem = false;
        topology.nodeCount = 1;
        NumaNode fallbackNode{};
        fallbackNode.nodeId = 0;
        fallbackNode.timestamp = topology.timestamp;
        topology.nodes.push_back(fallbackNode);
    }

    return topology;
}

auto isNumaSystem() -> bool {
    const auto topology = detectNumaTopology();
    return topology.isNumaSystem;
}

auto getNumaNodeCount() -> int {
    const auto topology = detectNumaTopology();
    return topology.nodeCount;
}

auto getNumaNodeInfo(int nodeId) -> NumaNode {
    const auto topology = detectNumaTopology();

    for (const auto& node : topology.nodes) {
        if (node.nodeId == nodeId) {
            return node;
        }
    }

    // Return empty node if not found
    NumaNode emptyNode{};
    emptyNode.nodeId = nodeId;
    emptyNode.timestamp = std::chrono::steady_clock::now();
    return emptyNode;
}

auto getNumaPolicy() -> std::string {
    std::string policy = "default";

#ifdef __linux__
    try {
        std::ifstream statusFile("/proc/self/status");
        if (statusFile.is_open()) {
            std::string line;
            while (std::getline(statusFile, line)) {
                if (line.find("Mems_allowed:") != std::string::npos) {
                    policy = "allowed nodes: " + line.substr(line.find(':') + 1);
                    break;
                }
            }
        }

        // Try to get more detailed policy information
        std::ifstream numaMapFile("/proc/self/numa_maps");
        if (numaMapFile.is_open()) {
            std::string firstLine;
            if (std::getline(numaMapFile, firstLine)) {
                if (firstLine.find("bind:") != std::string::npos) {
                    policy = "bind policy detected";
                } else if (firstLine.find("interleave:") != std::string::npos) {
                    policy = "interleave policy detected";
                } else if (firstLine.find("preferred:") != std::string::npos) {
                    policy = "preferred policy detected";
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Error reading NUMA policy: {}", e.what());
    }
#endif

    return policy;
}

auto analyzeNumaPerformance() -> NumaTopology {
    spdlog::info("Analyzing NUMA performance");

    auto topology = detectNumaTopology();

    if (!topology.isNumaSystem) {
        spdlog::info("System is not NUMA - performance analysis not applicable");
        return topology;
    }

    try {
        // Enhanced performance analysis
        double totalPerformanceImpact = 0.0;

        for (auto& node : topology.nodes) {
            // Analyze memory access patterns
            if (node.numaHitRatio < 90.0) {
                double nodeImpact = (90.0 - node.numaHitRatio) * 0.3;
                totalPerformanceImpact += nodeImpact;

                topology.recommendations.push_back(
                    "Node " + std::to_string(node.nodeId) + " has low NUMA hit ratio (" +
                    std::to_string(static_cast<int>(node.numaHitRatio)) + "%)");
            }

            // Check for memory imbalance
            double avgMemoryUsage = 0.0;
            for (const auto& otherNode : topology.nodes) {
                avgMemoryUsage += otherNode.memoryUsagePercent;
            }
            avgMemoryUsage /= topology.nodes.size();

            if (std::abs(node.memoryUsagePercent - avgMemoryUsage) > 20.0) {
                topology.recommendations.push_back(
                    "Memory imbalance detected on node " + std::to_string(node.nodeId) +
                    " (usage: " + std::to_string(static_cast<int>(node.memoryUsagePercent)) +
                    "%, average: " + std::to_string(static_cast<int>(avgMemoryUsage)) + "%)");
            }
        }

        topology.numaPerformanceImpact = totalPerformanceImpact / topology.nodes.size();

        // Add general recommendations
        if (topology.overallNumaHitRatio < 85.0) {
            topology.recommendations.push_back("Consider using numactl for process binding");
            topology.recommendations.push_back("Enable automatic NUMA balancing if available");
        }

        if (topology.totalRemoteAccesses > topology.totalNumaAllocations * 0.15) {
            topology.recommendations.push_back("High remote access rate - optimize data locality");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error in NUMA performance analysis: {}", e.what());
    }

    return topology;
}

auto benchmarkNumaMemoryAccess(size_t testSizeBytes) -> NumaTopology {
    spdlog::info("Benchmarking NUMA memory access with {} MB test size",
                 testSizeBytes / (1024 * 1024));

    auto topology = detectNumaTopology();

    if (!topology.isNumaSystem) {
        spdlog::info("System is not NUMA - using single node benchmark");
        return topology;
    }

    try {
        // Benchmark memory access for each node
        for (auto& node : topology.nodes) {
            // Simulate local memory access benchmark
            std::vector<char> testData(testSizeBytes / topology.nodeCount);

            // Local access benchmark
            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < testData.size(); i += 64) { // Cache line size
                volatile char val = testData[i];
                (void)val;
            }
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            node.localAccessLatency = static_cast<double>(duration.count()) / (static_cast<double>(testData.size()) / 64.0);

            // Estimate bandwidth based on latency
            node.localBandwidth = (testData.size() / (1024.0 * 1024.0)) /
                                 (duration.count() / 1000000000.0);

            // Simulate remote access characteristics
            for (const auto& [remoteNodeId, distance] : node.nodeDistances) {
                if (remoteNodeId != node.nodeId) {
                    // Remote access is typically 1.5-3x slower
                    double latencyMultiplier = 1.5 + (distance - 10) * 0.1;
                    double bandwidthMultiplier = 1.0 / latencyMultiplier;

                    node.remoteAccessLatency[remoteNodeId] = node.localAccessLatency * latencyMultiplier;
                    node.remoteBandwidth[remoteNodeId] = node.localBandwidth * bandwidthMultiplier;
                }
            }
        }

        spdlog::info("NUMA memory access benchmark completed");

    } catch (const std::exception& e) {
        spdlog::error("Error in NUMA memory benchmark: {}", e.what());
    }

    return topology;
}

auto getNumaMemoryStats() -> std::vector<NumaNode> {
    const auto topology = detectNumaTopology();
    return topology.nodes;
}

auto optimizeNumaAllocation() -> std::vector<std::string> {
    spdlog::info("Generating NUMA allocation optimization recommendations");

    std::vector<std::string> recommendations;
    const auto topology = analyzeNumaPerformance();

    if (!topology.isNumaSystem) {
        recommendations.push_back("System is not NUMA - no NUMA-specific optimizations needed");
        return recommendations;
    }

    // Add topology-specific recommendations
    recommendations.insert(recommendations.end(),
                          topology.recommendations.begin(),
                          topology.recommendations.end());

    // Add general NUMA optimization recommendations
    recommendations.push_back("Use numactl --cpubind=<node> --membind=<node> for CPU-memory binding");
    recommendations.push_back("Consider using libnuma for programmatic NUMA control");
    recommendations.push_back("Monitor /proc/*/numa_maps for process memory distribution");
    recommendations.push_back("Use 'numastat' command to monitor NUMA statistics");

    if (topology.overallNumaHitRatio < 90.0) {
        recommendations.push_back("Enable transparent hugepages for better NUMA performance");
        recommendations.push_back("Consider using NUMA-aware memory allocators");
    }

    spdlog::info("Generated {} NUMA optimization recommendations", recommendations.size());
    return recommendations;
}

auto detectMemoryLeaksEnhanced(const LeakDetectionConfig& config) -> std::vector<MemoryLeak> {
    spdlog::info("Starting enhanced memory leak detection");

    std::vector<MemoryLeak> detectedLeaks;
    std::lock_guard<std::mutex> lock(internal::g_leakDetectionMutex);

    try {
        // Collect current memory statistics
        const auto currentMemInfo = getDetailedMemoryStats();
        internal::g_memoryHistory.push_back(currentMemInfo);

        // Keep history within analysis window
        const auto cutoffTime = std::chrono::steady_clock::now() - config.analysisWindow;
        internal::g_memoryHistory.erase(
            std::remove_if(internal::g_memoryHistory.begin(), internal::g_memoryHistory.end(),
                          [cutoffTime](const MemoryInfo& info) {
                              // Note: MemoryInfo doesn't have timestamp, so we'll use a simplified approach
                              return false; // Keep all for now
                          }),
            internal::g_memoryHistory.end());

        // Limit history size
        if (internal::g_memoryHistory.size() > 1000) {
            internal::g_memoryHistory.erase(internal::g_memoryHistory.begin(),
                                          internal::g_memoryHistory.begin() +
                                          (internal::g_memoryHistory.size() - 1000));
        }

        if (internal::g_memoryHistory.size() < 3) {
            spdlog::debug("Insufficient memory history for leak detection");
            return detectedLeaks;
        }

        // Algorithm 1: Gradual Memory Growth Detection
        if (config.reportGradualLeaks) {
            auto gradualLeaks = internal::detectGradualLeaks(config);
            detectedLeaks.insert(detectedLeaks.end(), gradualLeaks.begin(), gradualLeaks.end());
        }

        // Algorithm 2: Sudden Memory Spike Detection
        if (config.reportSuddenLeaks) {
            auto suddenLeaks = internal::detectSuddenLeaks(config);
            detectedLeaks.insert(detectedLeaks.end(), suddenLeaks.begin(), suddenLeaks.end());
        }

        // Algorithm 3: Periodic Leak Pattern Detection
        if (config.reportPeriodicLeaks && config.enablePatternRecognition) {
            auto periodicLeaks = internal::detectPeriodicLeaks(config);
            detectedLeaks.insert(detectedLeaks.end(), periodicLeaks.begin(), periodicLeaks.end());
        }

        // Algorithm 4: Allocation Tracking Based Detection
        if (internal::g_allocationTrackingActive) {
            auto trackingLeaks = internal::detectAllocationTrackingLeaks(config);
            detectedLeaks.insert(detectedLeaks.end(), trackingLeaks.begin(), trackingLeaks.end());
        }

        // Filter leaks by confidence threshold
        detectedLeaks.erase(
            std::remove_if(detectedLeaks.begin(), detectedLeaks.end(),
                          [&config](const MemoryLeak& leak) {
                              return leak.confidence < config.confidenceThreshold;
                          }),
            detectedLeaks.end());

        // Analyze patterns if enabled
        if (config.enablePatternRecognition && !detectedLeaks.empty()) {
            detectedLeaks = analyzeLeakPatterns(detectedLeaks);
        }

        spdlog::info("Enhanced leak detection completed: found {} potential leaks", detectedLeaks.size());

    } catch (const std::exception& e) {
        spdlog::error("Error in enhanced leak detection: {}", e.what());
    }

    return detectedLeaks;
}

auto detectGradualLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak> {
    std::vector<MemoryLeak> gradualLeaks;

    if (internal::g_memoryHistory.size() < 5) {
        return gradualLeaks;
    }

    // Analyze memory growth trend
    std::vector<size_t> workingSetSizes;
    for (const auto& memInfo : internal::g_memoryHistory) {
        workingSetSizes.push_back(memInfo.workingSetSize);
    }

    // Calculate linear regression to detect consistent growth
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    size_t n = workingSetSizes.size();

    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(workingSetSizes[i]);
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double intercept = (sumY - slope * sumX) / n;

    // Calculate correlation coefficient
    double meanX = sumX / n;
    double meanY = sumY / n;
    double numerator = 0, denomX = 0, denomY = 0;

    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(workingSetSizes[i]);
        numerator += (x - meanX) * (y - meanY);
        denomX += (x - meanX) * (x - meanX);
        denomY += (y - meanY) * (y - meanY);
    }

    double correlation = numerator / std::sqrt(denomX * denomY);

    // Detect gradual leak if there's strong positive correlation and significant slope
    if (correlation > 0.8 && slope > static_cast<double>(config.minimumLeakSize)) {
        MemoryLeak leak{};
        leak.leakId = "gradual_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        leak.severity = slope > 1024 * 1024 ? LeakSeverity::HIGH : LeakSeverity::MEDIUM; // 1MB threshold
        leak.type = LeakType::GRADUAL;
        leak.leakRate = static_cast<size_t>(slope); // bytes per measurement interval
        leak.confidence = correlation * 100.0;
        leak.sourceLocation = "Unknown (system-wide analysis)";
        leak.processName = "System";
        leak.processId = getpid();
        leak.firstDetected = std::chrono::steady_clock::now() - config.analysisWindow;
        leak.lastSeen = std::chrono::steady_clock::now();
        leak.duration = config.analysisWindow;
        leak.totalLeakedBytes = static_cast<size_t>(slope * n);
        leak.estimatedLossPerHour = static_cast<size_t>(slope * 3600); // Assuming 1-second intervals
        leak.memoryImpact = (static_cast<double>(leak.totalLeakedBytes) / getTotalMemorySize()) * 100.0;
        leak.performanceImpact = leak.memoryImpact * 0.5; // Estimate
        leak.timestamp = std::chrono::steady_clock::now();

        leak.recommendations.push_back("Monitor process memory usage to identify leaking process");
        leak.recommendations.push_back("Use memory profiling tools to identify leak source");
        if (leak.severity >= LeakSeverity::HIGH) {
            leak.recommendations.push_back("Consider restarting affected processes");
        }

        gradualLeaks.push_back(leak);
    }

    return gradualLeaks;
}

auto detectSuddenLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak> {
    std::vector<MemoryLeak> suddenLeaks;

    if (internal::g_memoryHistory.size() < 3) {
        return suddenLeaks;
    }

    // Look for sudden spikes in memory usage
    for (size_t i = 2; i < internal::g_memoryHistory.size(); ++i) {
        const auto& current = internal::g_memoryHistory[i];
        const auto& previous = internal::g_memoryHistory[i-1];
        const auto& beforePrevious = internal::g_memoryHistory[i-2];

        // Calculate average of previous measurements
        size_t avgPrevious = (previous.workingSetSize + beforePrevious.workingSetSize) / 2;

        // Check for sudden increase
        if (current.workingSetSize > avgPrevious + config.minimumLeakSize) {
            size_t increase = current.workingSetSize - avgPrevious;

            MemoryLeak leak{};
            leak.leakId = "sudden_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            leak.severity = increase > 10 * 1024 * 1024 ? LeakSeverity::CRITICAL : LeakSeverity::HIGH;
            leak.type = LeakType::SUDDEN;
            leak.totalLeakedBytes = increase;
            leak.confidence = 85.0; // High confidence for sudden spikes
            leak.sourceLocation = "Unknown (sudden allocation)";
            leak.processName = "System";
            leak.processId = getpid();
            leak.firstDetected = std::chrono::steady_clock::now();
            leak.lastSeen = std::chrono::steady_clock::now();
            leak.duration = std::chrono::minutes(1); // Recent
            leak.estimatedLossPerHour = increase; // Assume it happened in last hour
            leak.memoryImpact = (static_cast<double>(increase) / getTotalMemorySize()) * 100.0;
            leak.performanceImpact = leak.memoryImpact * 0.8; // Higher impact for sudden leaks
            leak.timestamp = std::chrono::steady_clock::now();

            leak.recommendations.push_back("Investigate recent process activity");
            leak.recommendations.push_back("Check system logs for unusual activity");
            leak.recommendations.push_back("Monitor for continued growth");

            suddenLeaks.push_back(leak);
        }
    }

    return suddenLeaks;
}

auto detectPeriodicLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak> {
    std::vector<MemoryLeak> periodicLeaks;

    if (internal::g_memoryHistory.size() < 10) {
        return periodicLeaks; // Need more data for pattern detection
    }

    // Analyze for periodic patterns in memory usage
    std::vector<size_t> workingSetSizes;
    for (const auto& memInfo : internal::g_memoryHistory) {
        workingSetSizes.push_back(memInfo.workingSetSize);
    }

    // Simple periodic pattern detection using autocorrelation
    size_t n = workingSetSizes.size();
    std::vector<double> autocorr;

    // Calculate autocorrelation for different lags
    for (size_t lag = 1; lag < n / 2; ++lag) {
        double sum1 = 0, sum2 = 0, sum12 = 0;
        size_t count = n - lag;

        for (size_t i = 0; i < count; ++i) {
            double x1 = static_cast<double>(workingSetSizes[i]);
            double x2 = static_cast<double>(workingSetSizes[i + lag]);
            sum1 += x1;
            sum2 += x2;
            sum12 += x1 * x2;
        }

        double mean1 = sum1 / count;
        double mean2 = sum2 / count;
        double covariance = (sum12 / count) - (mean1 * mean2);

        // Calculate standard deviations
        double var1 = 0, var2 = 0;
        for (size_t i = 0; i < count; ++i) {
            double x1 = static_cast<double>(workingSetSizes[i]);
            double x2 = static_cast<double>(workingSetSizes[i + lag]);
            var1 += (x1 - mean1) * (x1 - mean1);
            var2 += (x2 - mean2) * (x2 - mean2);
        }

        double std1 = std::sqrt(var1 / count);
        double std2 = std::sqrt(var2 / count);

        if (std1 > 0 && std2 > 0) {
            autocorr.push_back(covariance / (std1 * std2));
        } else {
            autocorr.push_back(0.0);
        }
    }

    // Look for strong periodic correlations
    for (size_t i = 0; i < autocorr.size(); ++i) {
        if (autocorr[i] > 0.7) { // Strong correlation threshold
            MemoryLeak leak{};
            leak.leakId = "periodic_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            leak.severity = LeakSeverity::MEDIUM;
            leak.type = LeakType::PERIODIC;
            leak.confidence = autocorr[i] * 100.0;
            leak.sourceLocation = "Unknown (periodic pattern)";
            leak.processName = "System";
            leak.processId = getpid();
            leak.firstDetected = std::chrono::steady_clock::now() - config.analysisWindow;
            leak.lastSeen = std::chrono::steady_clock::now();
            leak.duration = config.analysisWindow;

            // Estimate leak characteristics
            size_t period = i + 1;
            size_t avgIncrease = 0;
            size_t samples = 0;

            for (size_t j = period; j < workingSetSizes.size(); j += period) {
                if (j < workingSetSizes.size() && j >= period) {
                    if (workingSetSizes[j] > workingSetSizes[j - period]) {
                        avgIncrease += workingSetSizes[j] - workingSetSizes[j - period];
                        samples++;
                    }
                }
            }

            if (samples > 0) {
                leak.leakRate = avgIncrease / samples;
                leak.totalLeakedBytes = leak.leakRate * samples;
                leak.estimatedLossPerHour = leak.leakRate * (3600 / period); // Assuming 1-second intervals
            }

            leak.memoryImpact = (static_cast<double>(leak.totalLeakedBytes) / getTotalMemorySize()) * 100.0;
            leak.performanceImpact = leak.memoryImpact * 0.3; // Lower impact for periodic
            leak.timestamp = std::chrono::steady_clock::now();

            leak.recommendations.push_back("Investigate periodic processes or scheduled tasks");
            leak.recommendations.push_back("Check for memory cleanup in periodic operations");
            leak.recommendations.push_back("Monitor pattern period: " + std::to_string(period) + " intervals");

            periodicLeaks.push_back(leak);
        }
    }

    return periodicLeaks;
}

auto detectAllocationTrackingLeaks(const LeakDetectionConfig& config) -> std::vector<MemoryLeak> {
    std::vector<MemoryLeak> trackingLeaks;

    if (!internal::g_allocationTrackingActive) {
        return trackingLeaks;
    }

    std::lock_guard<std::mutex> lock(internal::g_allocationMutex);

    const auto now = std::chrono::steady_clock::now();
    std::map<std::string, std::vector<MemoryAllocation>> locationGroups;

    // Group allocations by source location
    for (const auto& [address, allocation] : internal::g_activeAllocations) {
        locationGroups[allocation.sourceLocation].push_back(allocation);
    }

    // Analyze each location group for leaks
    for (const auto& [location, allocations] : locationGroups) {
        if (allocations.size() < 10) continue; // Need sufficient allocations

        // Check for old allocations
        size_t oldAllocations = 0;
        size_t totalSize = 0;
        auto oldestTime = now;

        for (const auto& alloc : allocations) {
            totalSize += alloc.size;
            if (alloc.timestamp < oldestTime) {
                oldestTime = alloc.timestamp;
            }

            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - alloc.timestamp);
            if (age > config.minimumAge) {
                oldAllocations++;
            }
        }

        // Check if this looks like a leak
        double oldRatio = static_cast<double>(oldAllocations) / allocations.size();
        if (oldRatio > 0.5 && totalSize > config.minimumLeakSize) {
            MemoryLeak leak{};
            leak.leakId = "tracking_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            leak.severity = totalSize > 10 * 1024 * 1024 ? LeakSeverity::HIGH : LeakSeverity::MEDIUM;
            leak.type = LeakType::PERSISTENT;
            leak.confidence = oldRatio * 100.0;
            leak.sourceLocation = location;
            leak.processName = "Current Process";
            leak.processId = getpid();
            leak.firstDetected = oldestTime;
            leak.lastSeen = now;
            leak.duration = std::chrono::duration_cast<std::chrono::minutes>(now - oldestTime);
            leak.totalLeakedBytes = totalSize;

            // Calculate leak rate
            auto durationHours = std::chrono::duration_cast<std::chrono::hours>(leak.duration);
            if (durationHours.count() > 0) {
                leak.estimatedLossPerHour = totalSize / durationHours.count();
                leak.leakRate = leak.estimatedLossPerHour / 3600; // bytes per second
            }

            leak.memoryImpact = (static_cast<double>(totalSize) / getTotalMemorySize()) * 100.0;
            leak.performanceImpact = leak.memoryImpact * 0.6;
            leak.timestamp = now;

            // Store allocation details
            for (const auto& alloc : allocations) {
                leak.allocationSizes.push_back(alloc.size);
                leak.allocationTimes.push_back(alloc.timestamp);
            }

            leak.recommendations.push_back("Review memory management at: " + location);
            leak.recommendations.push_back("Check for missing free/delete calls");
            leak.recommendations.push_back("Consider using smart pointers or RAII");
            if (allocations.size() > 1000) {
                leak.recommendations.push_back("High allocation count - check for loops or recursive calls");
            }

            trackingLeaks.push_back(leak);
        }
    }

    return trackingLeaks;
}

}  // namespace internal

auto analyzeLeakPatterns(const std::vector<MemoryLeak>& leaks) -> std::vector<MemoryLeak> {
    spdlog::debug("Analyzing leak patterns for {} leaks", leaks.size());

    std::vector<MemoryLeak> analyzedLeaks = leaks;

    // Pattern analysis and enhancement
    for (auto& leak : analyzedLeaks) {
        // Enhance confidence based on multiple factors
        double confidenceBoost = 0.0;

        // Age factor - older leaks are more likely to be real
        auto ageHours = std::chrono::duration_cast<std::chrono::hours>(leak.duration);
        if (ageHours.count() > 24) {
            confidenceBoost += 10.0;
        } else if (ageHours.count() > 6) {
            confidenceBoost += 5.0;
        }

        // Size factor - larger leaks are more significant
        if (leak.totalLeakedBytes > 100 * 1024 * 1024) { // > 100MB
            confidenceBoost += 15.0;
            leak.severity = LeakSeverity::CRITICAL;
        } else if (leak.totalLeakedBytes > 10 * 1024 * 1024) { // > 10MB
            confidenceBoost += 10.0;
            if (leak.severity < LeakSeverity::HIGH) {
                leak.severity = LeakSeverity::HIGH;
            }
        }

        // Rate factor - fast leaks are more concerning
        if (leak.leakRate > 1024 * 1024) { // > 1MB/s
            confidenceBoost += 20.0;
            leak.severity = LeakSeverity::CRITICAL;
        } else if (leak.leakRate > 1024) { // > 1KB/s
            confidenceBoost += 10.0;
        }

        // Apply confidence boost
        leak.confidence = std::min(100.0, leak.confidence + confidenceBoost);

        // Add pattern-specific recommendations
        switch (leak.type) {
            case LeakType::GRADUAL:
                leak.recommendations.push_back("Gradual leak detected - check for accumulating data structures");
                break;
            case LeakType::SUDDEN:
                leak.recommendations.push_back("Sudden leak detected - investigate recent changes or events");
                break;
            case LeakType::PERIODIC:
                leak.recommendations.push_back("Periodic leak detected - check scheduled tasks or timers");
                break;
            case LeakType::PERSISTENT:
                leak.recommendations.push_back("Persistent leak detected - review object lifecycle management");
                break;
            default:
                break;
        }

        // Add severity-specific recommendations
        if (leak.severity >= LeakSeverity::HIGH) {
            leak.recommendations.push_back("High severity leak - consider immediate action");
        }
        if (leak.severity == LeakSeverity::CRITICAL) {
            leak.recommendations.push_back("CRITICAL leak - immediate intervention required");
        }
    }

    return analyzedLeaks;
}
