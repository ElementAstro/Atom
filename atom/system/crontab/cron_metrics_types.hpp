/*
 * cron_metrics_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_METRICS_TYPES_HPP
#define CRON_METRICS_TYPES_HPP

#include <atomic>
#include <chrono>
#include <cstdint>

/**
 * @brief Performance metrics for the cron system
 */
struct CronSystemMetrics {
    // Job statistics
    std::atomic<uint64_t> totalJobs{0};
    std::atomic<uint64_t> activeJobs{0};
    std::atomic<uint64_t> completedJobs{0};
    std::atomic<uint64_t> failedJobs{0};
    std::atomic<uint64_t> skippedJobs{0};

    // Execution statistics
    std::atomic<uint64_t> totalExecutions{0};
    std::atomic<uint64_t> successfulExecutions{0};
    std::atomic<uint64_t> failedExecutions{0};
    std::atomic<uint64_t> timeoutExecutions{0};

    // Performance metrics
    std::atomic<uint64_t> totalExecutionTime{0};  // microseconds
    std::atomic<uint64_t> minExecutionTime{UINT64_MAX};
    std::atomic<uint64_t> maxExecutionTime{0};
    std::atomic<uint64_t> schedulingLatency{0};  // microseconds

    // Resource usage
    std::atomic<uint64_t> memoryUsageBytes{0};
    std::atomic<uint64_t> peakMemoryUsageBytes{0};
    std::atomic<uint32_t> activeThreads{0};
    std::atomic<uint32_t> peakActiveThreads{0};

    // Cache statistics
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> cacheEvictions{0};

    // Error tracking
    std::atomic<uint64_t> validationErrors{0};
    std::atomic<uint64_t> securityViolations{0};
    std::atomic<uint64_t> systemErrors{0};

    std::chrono::steady_clock::time_point startTime{
        std::chrono::steady_clock::now()};

    void reset() noexcept {
        totalJobs = 0;
        activeJobs = 0;
        completedJobs = 0;
        failedJobs = 0;
        skippedJobs = 0;
        totalExecutions = 0;
        successfulExecutions = 0;
        failedExecutions = 0;
        timeoutExecutions = 0;
        totalExecutionTime = 0;
        minExecutionTime = UINT64_MAX;
        maxExecutionTime = 0;
        schedulingLatency = 0;
        memoryUsageBytes = 0;
        peakMemoryUsageBytes = 0;
        activeThreads = 0;
        peakActiveThreads = 0;
        cacheHits = 0;
        cacheMisses = 0;
        cacheEvictions = 0;
        validationErrors = 0;
        securityViolations = 0;
        systemErrors = 0;
        startTime = std::chrono::steady_clock::now();
    }

    double getSuccessRate() const noexcept {
        auto total = totalExecutions.load();
        return total > 0
                   ? static_cast<double>(successfulExecutions.load()) / total
                   : 0.0;
    }

    double getAverageExecutionTime() const noexcept {
        auto total = totalExecutions.load();
        return total > 0
                   ? static_cast<double>(totalExecutionTime.load()) / total
                   : 0.0;
    }

    double getCacheHitRatio() const noexcept {
        auto total = cacheHits.load() + cacheMisses.load();
        return total > 0 ? static_cast<double>(cacheHits.load()) / total : 0.0;
    }

    std::chrono::seconds getUptime() const noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);
    }
};

// Convenience macro for accessing global metrics
#define CRON_METRICS() CronConfigManager::getInstance().getMetrics()

#endif  // CRON_METRICS_TYPES_HPP
