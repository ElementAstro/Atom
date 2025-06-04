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
#include "atom/log/loguru.hpp"
#include "memory.hpp"


#ifdef _WIN32
#include <processthreadsapi.h>
#include <windows.h>
#endif

namespace atom::system {
namespace internal {

std::atomic<bool> g_monitoringActive(false);

auto formatByteSize(unsigned long long bytes) -> std::string {
    static constexpr const char* UNITS[] = {"B",  "KB", "MB", "GB",
                                            "TB", "PB", "EB"};
    static constexpr int MAX_UNIT_INDEX = 6;

    if (bytes == 0)
        return "0 B";

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

auto benchmarkMemoryPerformance(size_t testSizeBytes) -> double {
    if (testSizeBytes == 0)
        return 0.0;

    std::vector<char> testBuffer;
    testBuffer.reserve(testSizeBytes);
    testBuffer.resize(testSizeBytes);

    const auto start = std::chrono::high_resolution_clock::now();

    // Write test - use memset for better performance
    std::fill(testBuffer.begin(), testBuffer.end(), static_cast<char>(0xAA));

    // Read test - prevent compiler optimization
    volatile char sum = 0;
    for (const auto& byte : testBuffer) {
        sum += byte;
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration<double>(end - start).count();

    if (duration <= 0.0)
        return 0.0;

    constexpr double BYTES_TO_MB = 1.0 / (1024.0 * 1024.0);
    const double mbProcessed =
        static_cast<double>(testSizeBytes * 2) * BYTES_TO_MB;

    return mbProcessed / duration;
}

}  // namespace internal

/**
 * @brief Starts continuous memory monitoring with callback
 * @param callback Function to be called with memory information updates
 */
auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback)
    -> void {
    if (internal::g_monitoringActive.exchange(true)) {
        LOG_F(WARNING, "Memory monitoring is already active");
        return;
    }

    LOG_F(INFO, "Starting memory monitoring");

    std::thread monitorThread([callback = std::move(callback)]() {
        while (internal::g_monitoringActive.load()) {
            try {
                const auto info = getDetailedMemoryStats();
                callback(info);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error in memory monitoring thread: %s", e.what());
                break;
            }
        }
        LOG_F(INFO, "Memory monitoring stopped");
    });

    monitorThread.detach();
}

/**
 * @brief Stops memory monitoring
 */
auto stopMemoryMonitoring() -> void {
    bool expected = true;
    if (internal::g_monitoringActive.compare_exchange_strong(expected, false)) {
        LOG_F(INFO, "Stopping memory monitoring");
    } else {
        LOG_F(WARNING, "Memory monitoring is not active");
    }
}

/**
 * @brief Collects memory usage timeline over specified duration
 * @param duration Time period to collect samples
 * @return Vector of memory information samples
 */
auto getMemoryTimeline(std::chrono::minutes duration)
    -> std::vector<MemoryInfo> {
    LOG_F(INFO, "Collecting memory timeline for %ld minutes", duration.count());

    std::vector<MemoryInfo> timeline;
    timeline.reserve(static_cast<size_t>(duration.count() * 60));

    const auto endTime = std::chrono::steady_clock::now() + duration;

    while (std::chrono::steady_clock::now() < endTime) {
        try {
            timeline.emplace_back(getDetailedMemoryStats());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error collecting memory timeline: %s", e.what());
            break;
        }
    }

    LOG_F(INFO, "Collected %zu memory samples", timeline.size());
    return timeline;
}

/**
 * @brief Performs basic memory leak detection
 * @return Vector of potential memory leak descriptions
 */
auto detectMemoryLeaks() -> std::vector<std::string> {
    LOG_F(INFO, "Starting memory leak detection");
    std::vector<std::string> leaks;

    const auto before = getDetailedMemoryStats();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    const auto after = getDetailedMemoryStats();

    constexpr unsigned long long LEAK_THRESHOLD = 1024 * 1024;  // 1MB
    if (after.workingSetSize > before.workingSetSize + LEAK_THRESHOLD) {
        std::ostringstream oss;
        oss << "Potential memory leak detected: Working set increased by "
            << internal::formatByteSize(after.workingSetSize -
                                        before.workingSetSize)
            << " in 5 seconds";
        leaks.emplace_back(oss.str());
    }

    LOG_F(INFO, "Memory leak detection completed, found %zu potential issues",
          leaks.size());
    return leaks;
}

/**
 * @brief Calculates memory fragmentation percentage
 * @return Fragmentation percentage (0-100)
 */
auto getMemoryFragmentation() -> double {
    LOG_F(INFO, "Calculating memory fragmentation");

    const auto total = getTotalMemorySize();
    const auto available = getAvailableMemorySize();

    if (available == 0)
        return 0.0;

    size_t allocatableSize = 0;
    try {
        constexpr size_t MAX_ALLOC_SIZE = 100 * 1024 * 1024;  // 100 MB
        std::vector<char> testAlloc;
        testAlloc.reserve(std::min(MAX_ALLOC_SIZE, available));
        allocatableSize = testAlloc.capacity();
    } catch (...) {
        allocatableSize = 0;
    }

    const double fragmentation = std::max(
        0.0, std::min(1.0, 1.0 - (static_cast<double>(allocatableSize) /
                                  static_cast<double>(available))));

    const double fragmentationPercent = fragmentation * 100.0;
    LOG_F(INFO, "Memory fragmentation estimated at %.2f%%",
          fragmentationPercent);
    return fragmentationPercent;
}

/**
 * @brief Attempts to optimize memory usage
 * @return True if optimization was successful
 */
auto optimizeMemoryUsage() -> bool {
    LOG_F(INFO, "Attempting to optimize memory usage");

    bool success = false;

#ifdef _WIN32
    try {
        success = SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1) != 0;
    } catch (...) {
        LOG_F(ERROR, "Failed to optimize memory on Windows");
    }
#elif defined(__linux__)
    try {
        if (auto fp = fopen("/proc/self/oom_score_adj", "w")) {
            fprintf(fp, "500\n");
            fclose(fp);
            success = true;
        }
    } catch (...) {
        LOG_F(ERROR, "Failed to optimize memory on Linux");
    }
#endif

    LOG_F(INFO, "Memory optimization %s", success ? "succeeded" : "failed");
    return success;
}

/**
 * @brief Analyzes system for memory bottlenecks
 * @return Vector of bottleneck descriptions
 */
auto analyzeMemoryBottlenecks() -> std::vector<std::string> {
    LOG_F(INFO, "Analyzing memory bottlenecks");

    std::vector<std::string> bottlenecks;
    const auto perf = getMemoryPerformance();
    const auto info = getDetailedMemoryStats();

    // High memory usage
    if (info.memoryLoadPercentage > 90.0) {
        std::ostringstream oss;
        oss << "High memory usage: "
            << static_cast<int>(info.memoryLoadPercentage)
            << "% of physical memory is in use";
        bottlenecks.emplace_back(oss.str());
    }

    // High swap usage
    if (info.swapMemoryTotal > 0) {
        const double swapUsagePercent =
            (static_cast<double>(info.swapMemoryUsed) / info.swapMemoryTotal) *
            100.0;
        if (swapUsagePercent > 50.0) {
            std::ostringstream oss;
            oss << "High swap usage: " << static_cast<int>(swapUsagePercent)
                << "% of swap space is in use, indicating insufficient RAM";
            bottlenecks.emplace_back(oss.str());
        }
    }

    // High memory latency
    constexpr double LATENCY_THRESHOLD = 100.0;
    if (perf.latency > LATENCY_THRESHOLD) {
        std::ostringstream oss;
        oss << "High memory latency: " << static_cast<int>(perf.latency)
            << " ns, may slow memory-intensive operations";
        bottlenecks.emplace_back(oss.str());
    }

    // High bandwidth usage
    if (perf.bandwidthUsage > 80.0) {
        std::ostringstream oss;
        oss << "High memory bandwidth usage: "
            << static_cast<int>(perf.bandwidthUsage)
            << "%, indicating potential bandwidth bottleneck";
        bottlenecks.emplace_back(oss.str());
    }

    // High fragmentation
    const double fragPercent = getMemoryFragmentation();
    if (fragPercent > 30.0) {
        std::ostringstream oss;
        oss << "High memory fragmentation: " << static_cast<int>(fragPercent)
            << "%, may cause allocation failures";
        bottlenecks.emplace_back(oss.str());
    }

    LOG_F(INFO, "Memory bottleneck analysis completed, found %zu issues",
          bottlenecks.size());
    return bottlenecks;
}

}  // namespace atom::system
