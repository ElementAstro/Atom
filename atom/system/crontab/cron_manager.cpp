#include "cron_manager.hpp"

#include <algorithm>
#include <chrono>

#include "cron_storage.hpp"
#include "cron_system.hpp"
#include "spdlog/spdlog.h"

CronManager::CronManager() {
    jobs_ = CronSystem::listSystemJobs();
    jobs_.reserve(1000);
    refreshJobIndex();
}

CronManager::~CronManager() { exportToCrontab(); }

void CronManager::refreshJobIndex() {
    jobIndex_.clear();
    categoryIndex_.clear();

    for (size_t i = 0; i < jobs_.size(); ++i) {
        jobIndex_[jobs_[i].getId()] = i;
        categoryIndex_[jobs_[i].category_].push_back(i);
    }
}

auto CronManager::validateJob(const CronJob& job) -> bool {
    if (job.time_.empty() || job.command_.empty()) {
        spdlog::error("Invalid job: time or command is empty");
        return false;
    }
    return validateCronExpression(job.time_).valid;
}

auto CronManager::validateCronExpression(const std::string& cronExpr)
    -> CronValidationResult {
    return CronValidation::validateCronExpression(cronExpr);
}

auto CronManager::convertSpecialExpression(const std::string& specialExpr)
    -> std::string {
    return CronValidation::convertSpecialExpression(specialExpr);
}

auto CronManager::createCronJob(const CronJob& job) -> bool {
    spdlog::info("Creating Cron job: {} {}", job.time_, job.command_);

    if (!validateJob(job)) {
        spdlog::error("Invalid cron job");
        return false;
    }

    auto isDuplicate = std::any_of(
        jobs_.begin(), jobs_.end(), [&job](const CronJob& existingJob) {
            return existingJob.command_ == job.command_ &&
                   existingJob.time_ == job.time_;
        });

    if (isDuplicate) {
        spdlog::warn("Duplicate cron job");
        return false;
    }

    if (!CronSystem::addJobToSystem(job)) {
        spdlog::error("Failed to add job to system crontab");
        return false;
    }

    jobs_.push_back(job);
    refreshJobIndex();

    spdlog::info("Cron job created successfully");
    return true;
}

auto CronManager::createJobWithSpecialTime(
    const std::string& specialTime, const std::string& command, bool enabled,
    const std::string& category, const std::string& description, int priority,
    int maxRetries, bool oneTime) -> bool {
    spdlog::info("Creating Cron job with special time: {} {}", specialTime,
                 command);

    const std::string standardTime = convertSpecialExpression(specialTime);
    if (standardTime.empty()) {
        spdlog::error("Invalid special time expression: {}", specialTime);
        return false;
    }

    CronJob job(standardTime, command, enabled, category, description);
    job.priority_ = priority;
    job.max_retries_ = maxRetries;
    job.one_time_ = oneTime;

    return createCronJob(job);
}

auto CronManager::deleteCronJob(const std::string& command) -> bool {
    spdlog::info("Deleting Cron job with command: {}", command);

    if (!CronSystem::removeJobFromSystem(command)) {
        spdlog::error("Failed to remove job from system crontab");
        return false;
    }

    const auto originalSize = jobs_.size();
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                               [&command](const CronJob& job) {
                                   return job.command_ == command;
                               }),
                jobs_.end());

    if (jobs_.size() < originalSize) {
        refreshJobIndex();
        spdlog::info("Cron job deleted successfully");
        return true;
    }

    spdlog::error("Failed to delete Cron job");
    return false;
}

auto CronManager::deleteCronJobById(const std::string& id) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return deleteCronJob(jobs_[it->second].command_);
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::listCronJobs() -> std::vector<CronJob> {
    spdlog::info("Listing all Cron jobs");

    // Merge with system jobs to ensure consistency
    auto systemJobs = CronSystem::listSystemJobs();

    // Update existing jobs with system data
    for (const auto& systemJob : systemJobs) {
        auto existingIt = std::find_if(jobs_.begin(), jobs_.end(),
                                       [&systemJob](const CronJob& job) {
                                           return job.command_ == systemJob.command_;
                                       });

        if (existingIt != jobs_.end()) {
            existingIt->time_ = systemJob.time_;
            existingIt->enabled_ = true;
        } else {
            jobs_.push_back(systemJob);
        }
    }

    refreshJobIndex();
    spdlog::info("Retrieved {} Cron jobs", jobs_.size());
    return jobs_;
}

auto CronManager::listCronJobsByCategory(const std::string& category)
    -> std::vector<CronJob> {
    spdlog::info("Listing Cron jobs in category: {}", category);

    auto it = categoryIndex_.find(category);
    if (it == categoryIndex_.end()) {
        spdlog::info("Found 0 jobs in category {}", category);
        return {};
    }

    std::vector<CronJob> filteredJobs;
    filteredJobs.reserve(it->second.size());

    for (size_t index : it->second) {
        if (index < jobs_.size()) {
            filteredJobs.push_back(jobs_[index]);
        }
    }

    spdlog::info("Found {} jobs in category {}", filteredJobs.size(), category);
    return filteredJobs;
}

auto CronManager::getCategories() -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(categoryIndex_.size());

    for (const auto& [category, _] : categoryIndex_) {
        result.push_back(category);
    }

    std::sort(result.begin(), result.end());
    return result;
}

auto CronManager::exportToJSON(const std::string& filename) -> bool {
    return CronStorage::exportToJSON(jobs_, filename);
}

auto CronManager::importFromJSON(const std::string& filename) -> bool {
    spdlog::info("Importing Cron jobs from JSON file: {}", filename);

    auto importedJobs = CronStorage::importFromJSON(filename);
    if (importedJobs.empty()) {
        return false;
    }

    int successCount = 0;
    for (const auto& job : importedJobs) {
        if (createCronJob(job)) {
            ++successCount;
        } else {
            spdlog::warn("Failed to import job: {} {}", job.time_, job.command_);
        }
    }

    spdlog::info("Successfully imported {} of {} jobs", successCount,
                 importedJobs.size());
    return successCount > 0;
}

auto CronManager::updateCronJob(const std::string& oldCommand,
                                const CronJob& newJob) -> bool {
    spdlog::info("Updating Cron job. Old command: {}, New command: {}",
                 oldCommand, newJob.command_);

    if (!validateJob(newJob)) {
        spdlog::error("Invalid new job");
        return false;
    }

    return deleteCronJob(oldCommand) && createCronJob(newJob);
}

auto CronManager::updateCronJobById(const std::string& id,
                                    const CronJob& newJob) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return updateCronJob(jobs_[it->second].command_, newJob);
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::viewCronJob(const std::string& command) -> CronJob {
    spdlog::info("Viewing Cron job with command: {}", command);

    auto it = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&command](const CronJob& job) { return job.command_ == command; });

    if (it != jobs_.end()) {
        spdlog::info("Cron job found");
        return *it;
    }

    spdlog::warn("Cron job not found");
    return CronJob{"", "", false};
}

auto CronManager::viewCronJobById(const std::string& id) -> CronJob {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return jobs_[it->second];
    }
    spdlog::warn("Cron job with ID {} not found", id);
    return CronJob{"", "", false};
}

auto CronManager::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    spdlog::info("Searching Cron jobs with query: {}", query);

    std::vector<CronJob> foundJobs;
    std::copy_if(jobs_.begin(), jobs_.end(), std::back_inserter(foundJobs),
                 [&query](const CronJob& job) {
                     return job.command_.find(query) != std::string::npos ||
                            job.time_.find(query) != std::string::npos ||
                            job.category_.find(query) != std::string::npos ||
                            job.description_.find(query) != std::string::npos;
                 });

    spdlog::info("Found {} matching Cron jobs", foundJobs.size());
    return foundJobs;
}

auto CronManager::statistics() -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;

    stats["total"] = static_cast<int>(jobs_.size());

    int enabledCount = 0;
    int totalExecutions = 0;

    for (const auto& job : jobs_) {
        if (job.enabled_) {
            ++enabledCount;
        }
        totalExecutions += job.run_count_;
    }

    stats["enabled"] = enabledCount;
    stats["disabled"] = static_cast<int>(jobs_.size()) - enabledCount;
    stats["total_executions"] = totalExecutions;

    for (const auto& [category, indices] : categoryIndex_) {
        stats["category_" + category] = static_cast<int>(indices.size());
    }

    spdlog::info(
        "Generated statistics. Total jobs: {}, enabled: {}, disabled: {}",
        stats["total"], stats["enabled"], stats["disabled"]);

    return stats;
}

auto CronManager::enableCronJob(const std::string& command) -> bool {
    spdlog::info("Enabling Cron job with command: {}", command);

    auto it = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&command](CronJob& job) { return job.command_ == command; });

    if (it != jobs_.end()) {
        it->enabled_ = true;
        return exportToCrontab();
    }

    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::disableCronJob(const std::string& command) -> bool {
    spdlog::info("Disabling Cron job with command: {}", command);

    auto it = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&command](CronJob& job) { return job.command_ == command; });

    if (it != jobs_.end()) {
        it->enabled_ = false;
        return exportToCrontab();
    }

    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::setJobEnabledById(const std::string& id, bool enabled)
    -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].enabled_ = enabled;
        return exportToCrontab();
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::enableCronJobsByCategory(const std::string& category) -> int {
    spdlog::info("Enabling all cron jobs in category: {}", category);

    auto it = categoryIndex_.find(category);
    if (it == categoryIndex_.end()) {
        return 0;
    }

    int count = 0;
    for (size_t index : it->second) {
        if (index < jobs_.size() && !jobs_[index].enabled_) {
            jobs_[index].enabled_ = true;
            ++count;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            spdlog::info("Enabled {} jobs in category {}", count, category);
        } else {
            spdlog::error("Failed to update crontab after enabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::disableCronJobsByCategory(const std::string& category)
    -> int {
    spdlog::info("Disabling all cron jobs in category: {}", category);

    auto it = categoryIndex_.find(category);
    if (it == categoryIndex_.end()) {
        return 0;
    }

    int count = 0;
    for (size_t index : it->second) {
        if (index < jobs_.size() && jobs_[index].enabled_) {
            jobs_[index].enabled_ = false;
            ++count;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            spdlog::info("Disabled {} jobs in category {}", count, category);
        } else {
            spdlog::error("Failed to update crontab after disabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::exportToCrontab() -> bool {
    return CronSystem::exportJobsToSystem(jobs_);
}

auto CronManager::batchCreateJobs(const std::vector<CronJob>& jobs) -> int {
    spdlog::info("Batch creating {} cron jobs", jobs.size());

    int successCount = 0;
    for (const auto& job : jobs) {
        if (createCronJob(job)) {
            ++successCount;
        }
    }

    spdlog::info("Successfully created {} of {} jobs", successCount,
                 jobs.size());
    return successCount;
}

auto CronManager::batchDeleteJobs(const std::vector<std::string>& commands)
    -> int {
    spdlog::info("Batch deleting {} cron jobs", commands.size());

    int successCount = 0;
    for (const auto& command : commands) {
        if (deleteCronJob(command)) {
            ++successCount;
        }
    }

    spdlog::info("Successfully deleted {} of {} jobs", successCount,
                 commands.size());
    return successCount;
}

auto CronManager::recordJobExecution(const std::string& command) -> bool {
    auto it = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&command](CronJob& job) { return job.command_ == command; });

    if (it != jobs_.end()) {
        it->last_run_ = std::chrono::system_clock::now();
        ++it->run_count_;
        it->recordExecution(true);

        if (it->one_time_) {
            const std::string jobId = it->getId();
            spdlog::info("One-time job completed, removing: {}", jobId);
            return deleteCronJobById(jobId);
        }

        spdlog::info("Recorded execution of job: {} (Run count: {})", command,
                     it->run_count_);
        return true;
    }

    spdlog::warn("Tried to record execution for unknown job: {}", command);
    return false;
}

auto CronManager::clearAllJobs() -> bool {
    spdlog::info("Clearing all cron jobs");

    if (!CronSystem::clearSystemJobs()) {
        return false;
    }

    jobs_.clear();
    jobIndex_.clear();
    categoryIndex_.clear();

    spdlog::info("All cron jobs cleared successfully");
    return true;
}

auto CronManager::setJobPriority(const std::string& id, int priority) -> bool {
    if (priority < 1 || priority > 10) {
        spdlog::error("Invalid priority value {}. Must be between 1-10",
                      priority);
        return false;
    }

    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].priority_ = priority;
        spdlog::info("Set priority to {} for job: {}", priority, id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::setJobMaxRetries(const std::string& id, int maxRetries)
    -> bool {
    if (maxRetries < 0) {
        spdlog::error("Invalid max retries value {}. Must be non-negative",
                      maxRetries);
        return false;
    }

    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].max_retries_ = maxRetries;
        if (jobs_[it->second].current_retries_ > maxRetries) {
            jobs_[it->second].current_retries_ = 0;
        }
        spdlog::info("Set max retries to {} for job: {}", maxRetries, id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::setJobOneTime(const std::string& id, bool oneTime) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].one_time_ = oneTime;
        spdlog::info("Set one-time status to {} for job: {}",
                     oneTime ? "true" : "false", id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::getJobExecutionHistory(const std::string& id)
    -> std::vector<std::pair<std::chrono::system_clock::time_point, bool>> {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return jobs_[it->second].execution_history_;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return {};
}

auto CronManager::recordJobExecutionResult(const std::string& id, bool success)
    -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        CronJob& job = jobs_[it->second];
        job.recordExecution(success);

        if (success && job.one_time_) {
            spdlog::info("One-time job completed successfully, removing: {}",
                         id);
            return deleteCronJobById(id);
        }

        if (!success) {
            return handleJobFailure(id);
        }

        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::handleJobFailure(const std::string& id) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        CronJob& job = jobs_[it->second];

        if (job.max_retries_ > 0 && job.current_retries_ < job.max_retries_) {
            ++job.current_retries_;
            spdlog::info("Job failed, scheduling retry {}/{} for: {}",
                         job.current_retries_, job.max_retries_, id);
        } else if (job.current_retries_ >= job.max_retries_ &&
                   job.max_retries_ > 0) {
            spdlog::warn("Job failed after {} retries, no more retries for: {}",
                         job.max_retries_, id);
        }
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::getJobsByPriority() -> std::vector<CronJob> {
    std::vector<CronJob> sortedJobs = jobs_;

    std::sort(sortedJobs.begin(), sortedJobs.end(),
              [](const CronJob& a, const CronJob& b) {
                  return a.priority_ < b.priority_;
              });

    return sortedJobs;
}
