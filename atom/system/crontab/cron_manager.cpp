#include "cron_manager.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "cron_storage.hpp"
#include "cron_system.hpp"
#include "cron_config.hpp"
#include "cron_cache.hpp"
#include "cron_thread_pool.hpp"
#include "spdlog/spdlog.h"

CronManager::CronManager() {
    // Load existing jobs from system with optimized storage
    auto system_jobs = CronSystem::listSystemJobs();

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
    jobs_.reserve(system_jobs.size() + 1000); // Reserve space for growth

    for (auto& job : system_jobs) {
        auto job_ptr = std::make_shared<CronJob>(std::move(job));
        std::string job_id = job_ptr->getId();
        jobs_[job_id] = job_ptr;
        updateIndices(job_id, *job_ptr);
    }

    spdlog::info("CronManager initialized with {} jobs", jobs_.size());
}

CronManager::~CronManager() {
    try {
        exportToCrontab();
        spdlog::info("CronManager destroyed, exported {} jobs", jobs_.size());
    } catch (const std::exception& e) {
        spdlog::error("Error during CronManager destruction: {}", e.what());
    }
}

CronManager::CronManager(CronManager&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.jobs_mutex_);
    jobs_ = std::move(other.jobs_);
    command_to_id_index_ = std::move(other.command_to_id_index_);
    category_index_ = std::move(other.category_index_);
    status_index_ = std::move(other.status_index_);
    priority_index_ = std::move(other.priority_index_);
    job_stats_ = std::move(other.job_stats_);
    max_jobs_ = other.max_jobs_.load();
    auto_cleanup_enabled_ = other.auto_cleanup_enabled_.load();
}

CronManager& CronManager::operator=(CronManager&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock1(jobs_mutex_, std::defer_lock);
        std::unique_lock<std::shared_mutex> lock2(other.jobs_mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        jobs_ = std::move(other.jobs_);
        command_to_id_index_ = std::move(other.command_to_id_index_);
        category_index_ = std::move(other.category_index_);
        status_index_ = std::move(other.status_index_);
        priority_index_ = std::move(other.priority_index_);
        job_stats_ = std::move(other.job_stats_);
        max_jobs_ = other.max_jobs_.load();
        auto_cleanup_enabled_ = other.auto_cleanup_enabled_.load();
    }
    return *this;
}

void CronManager::updateIndices(const std::string& job_id, const CronJob& job) {
    // Update command to ID mapping
    command_to_id_index_[job.command_] = job_id;

    // Update category index
    category_index_[job.getCategory()].push_back(job_id);

    // Update status index
    status_index_[job.getStatus()].insert(job_id);

    // Update priority index
    priority_index_[job.getPriority()].insert(job_id);

    // Invalidate cache
    invalidateCache();
}

void CronManager::removeFromIndices(const std::string& job_id, const CronJob& job) {
    // Remove from command mapping
    command_to_id_index_.erase(job.command_);

    // Remove from category index
    auto& category_jobs = category_index_[job.getCategory()];
    category_jobs.erase(std::remove(category_jobs.begin(), category_jobs.end(), job_id),
                       category_jobs.end());

    // Remove from status index
    status_index_[job.getStatus()].erase(job_id);

    // Remove from priority index
    priority_index_[job.getPriority()].erase(job_id);

    // Invalidate cache
    invalidateCache();
}

auto CronManager::validateJobInternal(const CronJob& job) -> bool {
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

auto CronManager::createCronJob(CronJob job) -> bool {
    spdlog::info("Creating Cron job: {} {}", job.time_, job.command_);

    if (!validateJobInternal(job)) {
        spdlog::error("Invalid cron job");
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);

    // Check for duplicates using command index
    auto it = command_to_id_index_.find(job.command_);
    if (it != command_to_id_index_.end()) {
        auto existing_job = jobs_[it->second];
        if (existing_job && existing_job->time_ == job.time_) {
            spdlog::warn("Duplicate cron job");
            return false;
        }
    }

    // Check job limit
    if (jobs_.size() >= max_jobs_.load()) {
        spdlog::error("Maximum job limit reached: {}", max_jobs_.load());
        return false;
    }

    auto job_ptr = std::make_shared<CronJob>(std::move(job));
    return addJobInternal(job_ptr);
}

auto CronManager::addJobInternal(std::shared_ptr<CronJob> job) -> bool {
    if (!job) return false;

    std::string job_id = generateJobId(*job);

    if (!CronSystem::addJobToSystem(*job)) {
        spdlog::error("Failed to add job to system crontab");
        return false;
    }

    jobs_[job_id] = job;
    updateIndices(job_id, *job);

    spdlog::info("Cron job created successfully with ID: {}", job_id);
    return true;
}

auto CronManager::generateJobId(const CronJob& job) -> std::string {
    return job.getId(); // Use the existing ID generation from CronJob
}

void CronManager::invalidateCache() {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    cache_.invalidate();
}

void CronManager::rebuildCache() {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    cache_.job_cache.clear();
    cache_.category_cache.clear();
    cache_.enabled_jobs_cache.clear();

    // Rebuild cache from current data
    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr) {
            cache_.job_cache[job_id] = job_ptr;
            if (job_ptr->isEnabled()) {
                cache_.enabled_jobs_cache.insert(job_id);
            }
        }
    }

    cache_.markValid();
}

auto CronManager::getCachedJob(const std::string& job_id) -> std::shared_ptr<CronJob> {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);

    if (!cache_.isValid()) {
        lock.unlock();
        rebuildCache();
        lock.lock();
    }

    auto it = cache_.job_cache.find(job_id);
    if (it != cache_.job_cache.end()) {
        return it->second.lock(); // Convert weak_ptr to shared_ptr
    }

    return nullptr;
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
    job.setPriority(static_cast<JobPriority>(std::clamp(priority, 1, 10)));
    job.setMaxRetries(static_cast<uint8_t>(maxRetries));
    job.setOneTime(oneTime);

    return createCronJob(std::move(job));
}

auto CronManager::deleteCronJob(const std::string& command) -> bool {
    spdlog::info("Deleting Cron job with command: {}", command);

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);

    // Find job by command
    auto cmd_it = command_to_id_index_.find(command);
    if (cmd_it == command_to_id_index_.end()) {
        spdlog::error("Failed to find job with command: {}", command);
        return false;
    }

    return removeJobInternal(cmd_it->second);
}

auto CronManager::deleteCronJobById(const std::string& id) -> bool {
    spdlog::info("Deleting Cron job with ID: {}", id);

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
    return removeJobInternal(id);
}

auto CronManager::removeJobInternal(const std::string& job_id) -> bool {
    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) {
        spdlog::error("Failed to find job with ID: {}", job_id);
        return false;
    }

    auto job_ptr = it->second;
    if (!job_ptr) {
        spdlog::error("Job pointer is null for ID: {}", job_id);
        return false;
    }

    if (!CronSystem::removeJobFromSystem(job_ptr->command_)) {
        spdlog::error("Failed to remove job from system crontab");
        return false;
    }

    // Remove from indices
    removeFromIndices(job_id, *job_ptr);

    // Remove from main storage
    jobs_.erase(it);

    spdlog::info("Cron job deleted successfully: {}", job_id);
    return true;
}

auto CronManager::listCronJobs() -> std::vector<CronJob> {
    spdlog::info("Listing all Cron jobs");

    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<CronJob> result;
    result.reserve(jobs_.size());

    for (const auto& [job_id, job_ptr] : jobs_) {
        if (job_ptr) {
            // Create a new CronJob with the same data (since copy is deleted)
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

auto CronManager::listCronJobsByCategory(const std::string& category)
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

auto CronManager::getCategories() -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(jobs_mutex_);

    std::vector<std::string> result;
    result.reserve(category_index_.size());

    for (const auto& [category, _] : category_index_) {
        result.push_back(category);
    }

    std::sort(result.begin(), result.end());
    return result;
}

auto CronManager::exportToJSON(const std::string& filename) -> bool {
    auto job_list = listCronJobs(); // This creates copies we can export
    return CronStorage::exportToJSON(job_list, filename);
}

auto CronManager::importFromJSON(const std::string& filename) -> bool {
    spdlog::info("Importing Cron jobs from JSON file: {}", filename);

    auto importedJobs = CronStorage::importFromJSON(filename);
    if (importedJobs.empty()) {
        return false;
    }

    int successCount = 0;
    for (auto& job : importedJobs) {
        if (createCronJob(std::move(job))) {
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
