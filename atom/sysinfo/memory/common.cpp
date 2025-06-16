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

#include <spdlog/spdlog.h>
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

}  // namespace atom::system
