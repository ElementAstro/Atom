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
