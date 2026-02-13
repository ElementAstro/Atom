/*
 * cron_manager_query.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cron_manager_query.hpp"

#include <algorithm>

#include "spdlog/spdlog.h"

CronManagerQuery::CronManagerQuery(
    const std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs,
    const std::unordered_map<std::string, std::string>& command_to_id_index,
    const std::unordered_map<std::string, std::vector<std::string>>&
        category_index,
    const std::unordered_map<JobStatus, std::set<std::string>>& status_index,
    const std::unordered_map<JobPriority, std::set<std::string>>&
        priority_index,
    const std::unordered_map<std::string, JobStats>& job_stats,
    std::shared_mutex& jobs_mutex, std::shared_mutex& stats_mutex)
    : jobs_(jobs),
      command_to_id_index_(command_to_id_index),
      category_index_(category_index),
      status_index_(status_index),
      priority_index_(priority_index),
      job_stats_(job_stats),
      jobs_mutex_(jobs_mutex),
      stats_mutex_(stats_mutex) {}

auto CronManagerQuery::listCronJobs() -> std::vector<CronJob> {
    spdlog::info("Listing all Cron jobs");

    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<CronJob> result;
    result.reserve(jobs_.size());

    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr) {
            CronJob job_copy(job_ptr->time_, job_ptr->command_,
                             job_ptr->isEnabled(), job_ptr->getCategory(),
                             job_ptr->getDescription());
            job_copy.setPriority(job_ptr->getPriority());
            job_copy.setMaxRetries(job_ptr->getMaxRetries());
            job_copy.setOneTime(job_ptr->isOneTime());
            result.push_back(std::move(job_copy));
        }
    }

    spdlog::info("Retrieved {} Cron jobs", result.size());
    return result;
}

auto CronManagerQuery::listCronJobsByCategory(const std::string& category)
    -> std::vector<CronJob> {
    spdlog::info("Listing Cron jobs in category: {}", category);

    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    auto it = category_index_.find(category);
    if (it == category_index_.end()) {
        spdlog::info("Found 0 jobs in category {}", category);
        return {};
    }

    std::vector<CronJob> filteredJobs;
    filteredJobs.reserve(it->second.size());

    for (const std::string& job_id : it->second) {
        auto job_it = jobs_.find(job_id);
        if (job_it != jobs_.end() && job_it->second) {
            auto job_ptr = job_it->second;
            CronJob job_copy(job_ptr->time_, job_ptr->command_,
                             job_ptr->isEnabled(), job_ptr->getCategory(),
                             job_ptr->getDescription());
            job_copy.setPriority(job_ptr->getPriority());
            job_copy.setMaxRetries(job_ptr->getMaxRetries());
            job_copy.setOneTime(job_ptr->isOneTime());
            filteredJobs.push_back(std::move(job_copy));
        }
    }

    spdlog::info("Found {} jobs in category {}", filteredJobs.size(), category);
    return filteredJobs;
}

auto CronManagerQuery::getCategories() -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<std::string> result;
    result.reserve(category_index_.size());

    for (const auto& [category, _] : category_index_) {
        result.push_back(category);
    }

    std::sort(result.begin(), result.end());
    return result;
}

auto CronManagerQuery::viewCronJob(const std::string& command) -> CronJob {
    spdlog::info("Viewing Cron job with command: {}", command);

    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&command](const auto& pair) {
                               return pair.second->command_ == command;
                           });

    if (it != jobs_.end()) {
        spdlog::info("Cron job found");
        auto& job = it->second;
        CronJob job_copy(job->time_, job->command_, job->isEnabled(),
                         job->getCategory(), job->getDescription());
        job_copy.setPriority(job->getPriority());
        job_copy.setMaxRetries(job->getMaxRetries());
        job_copy.setOneTime(job->isOneTime());
        return job_copy;
    }

    spdlog::warn("Cron job not found");
    return CronJob{"", "", false};
}

auto CronManagerQuery::viewCronJobById(const std::string& id) -> CronJob {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        auto& job = it->second;
        CronJob job_copy(job->time_, job->command_, job->isEnabled(),
                         job->getCategory(), job->getDescription());
        job_copy.setPriority(job->getPriority());
        job_copy.setMaxRetries(job->getMaxRetries());
        job_copy.setOneTime(job->isOneTime());
        return job_copy;
    }
    spdlog::warn("Cron job with ID {} not found", id);
    return CronJob{"", "", false};
}

auto CronManagerQuery::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    spdlog::info("Searching Cron jobs with query: {}", query);

    std::vector<CronJob> foundJobs;
    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr &&
            (job_ptr->command_.find(query) != std::string::npos ||
             job_ptr->time_.find(query) != std::string::npos ||
             job_ptr->getCategory().find(query) != std::string::npos ||
             job_ptr->getDescription().find(query) != std::string::npos)) {
            CronJob job_copy(job_ptr->time_, job_ptr->command_,
                             job_ptr->isEnabled(), job_ptr->getCategory(),
                             job_ptr->getDescription());
            job_copy.setPriority(job_ptr->getPriority());
            job_copy.setMaxRetries(job_ptr->getMaxRetries());
            job_copy.setOneTime(job_ptr->isOneTime());
            foundJobs.push_back(std::move(job_copy));
        }
    }

    spdlog::info("Found {} matching Cron jobs", foundJobs.size());
    return foundJobs;
}

auto CronManagerQuery::statistics() -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;

    stats["total"] = static_cast<int>(jobs_.size());

    int enabledCount = 0;
    int totalExecutions = 0;

    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr && job_ptr->isEnabled()) {
            ++enabledCount;
        }
    }

    stats["enabled"] = enabledCount;
    stats["disabled"] = static_cast<int>(jobs_.size()) - enabledCount;
    stats["total_executions"] = totalExecutions;

    for (const auto& [category, job_ids] : category_index_) {
        stats["category_" + category] = static_cast<int>(job_ids.size());
    }

    spdlog::info(
        "Generated statistics. Total jobs: {}, enabled: {}, disabled: {}",
        stats["total"], stats["enabled"], stats["disabled"]);

    return stats;
}

auto CronManagerQuery::getJobsByPriority() -> std::vector<CronJob> {
    std::vector<CronJob> sortedJobs;
    sortedJobs.reserve(jobs_.size());

    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr) {
            CronJob job_copy(job_ptr->time_, job_ptr->command_,
                             job_ptr->isEnabled(), job_ptr->getCategory(),
                             job_ptr->getDescription());
            job_copy.setPriority(job_ptr->getPriority());
            job_copy.setMaxRetries(job_ptr->getMaxRetries());
            job_copy.setOneTime(job_ptr->isOneTime());
            sortedJobs.push_back(std::move(job_copy));
        }
    }

    std::sort(sortedJobs.begin(), sortedJobs.end(),
              [](const CronJob& a, const CronJob& b) {
                  return a.getPriority() < b.getPriority();
              });

    return sortedJobs;
}

auto CronManagerQuery::getJobsByCategory(const std::string& category)
    -> std::vector<std::shared_ptr<CronJob>> {
    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<std::shared_ptr<CronJob>> result;
    auto it = category_index_.find(category);
    if (it != category_index_.end()) {
        result.reserve(it->second.size());
        for (const auto& job_id : it->second) {
            auto job_it = jobs_.find(job_id);
            if (job_it != jobs_.end() && job_it->second) {
                result.push_back(job_it->second);
            }
        }
    }
    return result;
}

auto CronManagerQuery::getJobsByStatus(JobStatus status)
    -> std::vector<std::shared_ptr<CronJob>> {
    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<std::shared_ptr<CronJob>> result;
    auto it = status_index_.find(status);
    if (it != status_index_.end()) {
        result.reserve(it->second.size());
        for (const auto& job_id : it->second) {
            auto job_it = jobs_.find(job_id);
            if (job_it != jobs_.end() && job_it->second) {
                result.push_back(job_it->second);
            }
        }
    }
    return result;
}

auto CronManagerQuery::getJobsByPriorityRange(JobPriority min_priority,
                                              JobPriority max_priority)
    -> std::vector<std::shared_ptr<CronJob>> {
    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<std::shared_ptr<CronJob>> result;
    for (const auto& [priority, job_ids] : priority_index_) {
        if (priority >= min_priority && priority <= max_priority) {
            for (const auto& job_id : job_ids) {
                auto job_it = jobs_.find(job_id);
                if (job_it != jobs_.end() && job_it->second) {
                    result.push_back(job_it->second);
                }
            }
        }
    }
    return result;
}

auto CronManagerQuery::getJobStats(const std::string& job_id)
    -> std::optional<JobStats> {
    std::shared_lock<std::shared_mutex> lock(stats_mutex_);

    auto it = job_stats_.find(job_id);
    if (it != job_stats_.end()) {
        JobStats stats_copy;
        stats_copy.total_executions =
            it->second.total_executions.load();
        stats_copy.successful_executions =
            it->second.successful_executions.load();
        stats_copy.failed_executions =
            it->second.failed_executions.load();
        stats_copy.last_execution = it->second.last_execution;
        stats_copy.avg_execution_time = it->second.avg_execution_time;
        return stats_copy;
    }
    return std::nullopt;
}

auto CronManagerQuery::getOverallStats() -> JobStats {
    std::shared_lock<std::shared_mutex> lock(stats_mutex_);

    JobStats overall;
    for (const auto& [job_id, stats] : job_stats_) {
        overall.total_executions += stats.total_executions.load();
        overall.successful_executions += stats.successful_executions.load();
        overall.failed_executions += stats.failed_executions.load();
    }
    return overall;
}
