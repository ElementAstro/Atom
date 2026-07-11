/*
 * cron_manager_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_MANAGER_TYPES_HPP
#define CRON_MANAGER_TYPES_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "cron_job.hpp"

/**
 * @brief Job execution statistics for monitoring
 */
struct JobStats {
    std::atomic<uint64_t> total_executions{0};
    std::atomic<uint64_t> successful_executions{0};
    std::atomic<uint64_t> failed_executions{0};
    std::chrono::system_clock::time_point last_execution;
    std::chrono::milliseconds avg_execution_time{0};

    JobStats() = default;

    // Atomics make JobStats non-copyable by default; provide value-snapshot
    // copy semantics so it can be returned by value (e.g. std::optional).
    JobStats(const JobStats& other)
        : total_executions(other.total_executions.load()),
          successful_executions(other.successful_executions.load()),
          failed_executions(other.failed_executions.load()),
          last_execution(other.last_execution),
          avg_execution_time(other.avg_execution_time) {}

    JobStats& operator=(const JobStats& other) {
        if (this != &other) {
            total_executions.store(other.total_executions.load());
            successful_executions.store(other.successful_executions.load());
            failed_executions.store(other.failed_executions.load());
            last_execution = other.last_execution;
            avg_execution_time = other.avg_execution_time;
        }
        return *this;
    }

    double getSuccessRate() const {
        uint64_t total = total_executions.load();
        return total > 0
                   ? static_cast<double>(successful_executions.load()) / total
                   : 0.0;
    }
};

/**
 * @brief Cache for frequently accessed job data
 */
struct JobCache {
    std::unordered_map<std::string, std::weak_ptr<CronJob>> job_cache;
    std::unordered_map<std::string, std::vector<std::string>> category_cache;
    std::set<std::string> enabled_jobs_cache;
    std::priority_queue<std::pair<JobPriority, std::string>>
        priority_queue_cache;
    std::atomic<bool> cache_valid{false};

    void invalidate() { cache_valid.store(false); }
    bool isValid() const { return cache_valid.load(); }
    void markValid() { cache_valid.store(true); }
};

#endif  // CRON_MANAGER_TYPES_HPP
