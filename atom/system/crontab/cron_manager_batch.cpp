/*
 * cron_manager_batch.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "cron_manager_batch.hpp"

#include <algorithm>

#include "cron_manager.hpp"
#include "cron_storage.hpp"
#include "cron_system.hpp"
#include "spdlog/spdlog.h"

CronManagerBatch::CronManagerBatch(
    CronManager& manager,
    std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs,
    std::shared_mutex& jobs_mutex)
    : manager_(manager), jobs_(jobs), jobs_mutex_(jobs_mutex) {}

auto CronManagerBatch::collectJobVector() -> std::vector<CronJob> {
    std::vector<CronJob> job_vector;
    job_vector.reserve(jobs_.size());

    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr) {
            CronJob job_copy(job_ptr->time_, job_ptr->command_,
                             job_ptr->isEnabled(), job_ptr->getCategory(),
                             job_ptr->getDescription());
            job_copy.setPriority(job_ptr->getPriority());
            job_copy.setMaxRetries(job_ptr->getMaxRetries());
            job_copy.setOneTime(job_ptr->isOneTime());
            job_vector.push_back(std::move(job_copy));
        }
    }

    return job_vector;
}

auto CronManagerBatch::batchCreateJobs(const std::vector<CronJob>& jobs)
    -> int {
    spdlog::info("Batch creating {} cron jobs", jobs.size());

    int successCount = 0;
    for (const auto& job : jobs) {
        CronJob job_copy(job.time_, job.command_, job.isEnabled(),
                         job.getCategory(), job.getDescription());
        job_copy.setPriority(job.getPriority());
        job_copy.setMaxRetries(job.getMaxRetries());
        job_copy.setOneTime(job.isOneTime());

        if (manager_.createCronJob(std::move(job_copy))) {
            ++successCount;
        }
    }

    spdlog::info("Successfully created {} of {} jobs", successCount,
                 jobs.size());
    return successCount;
}

auto CronManagerBatch::batchCreateJobsOptimized(std::vector<CronJob> jobs)
    -> size_t {
    spdlog::info("Batch creating (optimized) {} cron jobs", jobs.size());

    size_t successCount = 0;
    for (auto& job : jobs) {
        if (manager_.createCronJob(std::move(job))) {
            ++successCount;
        }
    }

    spdlog::info("Successfully created {} of {} jobs", successCount,
                 jobs.size());
    return successCount;
}

auto CronManagerBatch::batchDeleteJobs(
    const std::vector<std::string>& commands) -> int {
    spdlog::info("Batch deleting {} cron jobs", commands.size());

    int successCount = 0;
    for (const auto& command : commands) {
        if (manager_.deleteCronJob(command)) {
            ++successCount;
        }
    }

    spdlog::info("Successfully deleted {} of {} jobs", successCount,
                 commands.size());
    return successCount;
}

auto CronManagerBatch::batchUpdateJobs(
    const std::vector<std::pair<std::string, CronJob>>& updates) -> size_t {
    spdlog::info("Batch updating {} cron jobs", updates.size());

    size_t successCount = 0;
    for (const auto& [id, new_job] : updates) {
        if (manager_.updateCronJobById(id, new_job)) {
            ++successCount;
        }
    }

    spdlog::info("Successfully updated {} of {} jobs", successCount,
                 updates.size());
    return successCount;
}

auto CronManagerBatch::exportToJSON(const std::string& filename) -> bool {
    auto job_list = collectJobVector();
    return CronStorage::exportToJSON(job_list, filename);
}

auto CronManagerBatch::importFromJSON(const std::string& filename) -> bool {
    spdlog::info("Importing Cron jobs from JSON file: {}", filename);

    auto importedJobs = CronStorage::importFromJSON(filename);
    if (importedJobs.empty()) {
        return false;
    }

    int successCount = 0;
    for (auto& job : importedJobs) {
        if (manager_.createCronJob(std::move(job))) {
            ++successCount;
        } else {
            spdlog::warn("Failed to import job: {} {}", job.time_,
                         job.command_);
        }
    }

    spdlog::info("Successfully imported {} of {} jobs", successCount,
                 importedJobs.size());
    return successCount > 0;
}

auto CronManagerBatch::exportToCrontab() -> bool {
    auto job_vector = collectJobVector();
    auto result = CronSystem::exportJobsToSystem(job_vector);
    return result.success;
}

auto CronManagerBatch::clearAllJobs() -> bool {
    spdlog::info("Clearing all cron jobs");

    auto result = CronSystem::clearSystemJobs();
    if (!result.success) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
    jobs_.clear();

    spdlog::info("All cron jobs cleared successfully");
    return true;
}
