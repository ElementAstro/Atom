#include "cron_manager.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "cron_cache.hpp"
#include "cron_config.hpp"
#include "cron_storage.hpp"
#include "cron_system.hpp"
#include "cron_thread_pool.hpp"
#include "spdlog/spdlog.h"

void CronManager::initSubComponents() {
    query_ = std::make_unique<CronManagerQuery>(
        jobs_, command_to_id_index_, category_index_, status_index_,
        priority_index_, job_stats_, jobs_mutex_, stats_mutex_);
    batch_ = std::make_unique<CronManagerBatch>(*this, jobs_, jobs_mutex_);
}

CronManager::CronManager() {
    // Load existing jobs from system with optimized storage
    auto system_jobs = CronSystem::listSystemJobs();

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
    jobs_.reserve(system_jobs.size() + 1000);  // Reserve space for growth

    for (auto& job : system_jobs) {
        auto job_ptr = std::make_shared<CronJob>(std::move(job));
        std::string job_id = job_ptr->getId();
        jobs_[job_id] = job_ptr;
        updateIndices(job_id, *job_ptr);
    }

    lock.unlock();
    initSubComponents();

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
    lock.unlock();
    initSubComponents();
}

CronManager& CronManager::operator=(CronManager&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock1(jobs_mutex_,
                                                   std::defer_lock);
        std::unique_lock<std::shared_mutex> lock2(other.jobs_mutex_,
                                                   std::defer_lock);
        std::lock(lock1, lock2);

        jobs_ = std::move(other.jobs_);
        command_to_id_index_ = std::move(other.command_to_id_index_);
        category_index_ = std::move(other.category_index_);
        status_index_ = std::move(other.status_index_);
        priority_index_ = std::move(other.priority_index_);
        job_stats_ = std::move(other.job_stats_);
        max_jobs_ = other.max_jobs_.load();
        auto_cleanup_enabled_ = other.auto_cleanup_enabled_.load();
        lock1.unlock();
        lock2.unlock();
        initSubComponents();
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

    auto result = CronSystem::addJobToSystem(*job);
    if (!result.success) {
        spdlog::error("Failed to add job to system crontab: {}", result.message);
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

    auto result = CronSystem::removeJobFromSystem(job_ptr->command_);
    if (!result.success) {
        spdlog::error("Failed to remove job from system crontab: {}", result.message);
        return false;
    }

    // Remove from indices
    removeFromIndices(job_id, *job_ptr);

    // Remove from main storage
    jobs_.erase(it);

    spdlog::info("Cron job deleted successfully: {}", job_id);
    return true;
}

// ============================================================================
// Delegated Query Operations
// ============================================================================

auto CronManager::listCronJobs() -> std::vector<CronJob> {
    return query_->listCronJobs();
}

auto CronManager::listCronJobsByCategory(const std::string& category)
    -> std::vector<CronJob> {
    return query_->listCronJobsByCategory(category);
}

auto CronManager::getCategories() -> std::vector<std::string> {
    return query_->getCategories();
}

auto CronManager::viewCronJob(const std::string& command) -> CronJob {
    return query_->viewCronJob(command);
}

auto CronManager::viewCronJobById(const std::string& id) -> CronJob {
    return query_->viewCronJobById(id);
}

auto CronManager::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    return query_->searchCronJobs(query);
}

auto CronManager::statistics() -> std::unordered_map<std::string, int> {
    return query_->statistics();
}

auto CronManager::getJobsByPriority() -> std::vector<CronJob> {
    return query_->getJobsByPriority();
}

auto CronManager::getJobsByCategory(const std::string& category)
    -> std::vector<std::shared_ptr<CronJob>> {
    return query_->getJobsByCategory(category);
}

auto CronManager::getJobsByStatus(JobStatus status)
    -> std::vector<std::shared_ptr<CronJob>> {
    return query_->getJobsByStatus(status);
}

auto CronManager::getJobsByPriorityRange(JobPriority min_priority,
                                          JobPriority max_priority)
    -> std::vector<std::shared_ptr<CronJob>> {
    return query_->getJobsByPriorityRange(min_priority, max_priority);
}

auto CronManager::getJobStats(const std::string& job_id)
    -> std::optional<JobStats> {
    return query_->getJobStats(job_id);
}

auto CronManager::getOverallStats() -> JobStats {
    return query_->getOverallStats();
}

// ============================================================================
// Delegated Batch/IO Operations
// ============================================================================

auto CronManager::exportToJSON(const std::string& filename) -> bool {
    return batch_->exportToJSON(filename);
}

auto CronManager::importFromJSON(const std::string& filename) -> bool {
    return batch_->importFromJSON(filename);
}

// ============================================================================
// Core Update Operations
// ============================================================================

auto CronManager::updateCronJob(const std::string& oldCommand,
                                const CronJob& newJob) -> bool {
    spdlog::info("Updating Cron job. Old command: {}, New command: {}",
                 oldCommand, newJob.command_);

    if (!validateJobInternal(newJob)) {
        spdlog::error("Invalid new job");
        return false;
    }

    CronJob job_copy(newJob.time_, newJob.command_, newJob.isEnabled(),
                     newJob.getCategory(), newJob.getDescription());
    job_copy.setPriority(newJob.getPriority());
    job_copy.setMaxRetries(newJob.getMaxRetries());
    job_copy.setOneTime(newJob.isOneTime());

    return deleteCronJob(oldCommand) && createCronJob(std::move(job_copy));
}

auto CronManager::updateCronJobById(const std::string& id,
                                    const CronJob& newJob) -> bool {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        return updateCronJob(it->second->command_, newJob);
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

// ============================================================================
// Enable/Disable Operations
// ============================================================================

auto CronManager::enableCronJob(const std::string& command) -> bool {
    spdlog::info("Enabling Cron job with command: {}", command);

    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&command](const auto& pair) {
                               return pair.second->command_ == command;
                           });

    if (it != jobs_.end()) {
        it->second->enable();
        return exportToCrontab();
    }

    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::disableCronJob(const std::string& command) -> bool {
    spdlog::info("Disabling Cron job with command: {}", command);

    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&command](const auto& pair) {
                               return pair.second->command_ == command;
                           });

    if (it != jobs_.end()) {
        it->second->disable();
        return exportToCrontab();
    }

    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::setJobEnabledById(const std::string& id, bool enabled)
    -> bool {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        if (enabled) {
            it->second->enable();
        } else {
            it->second->disable();
        }
        return exportToCrontab();
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::enableCronJobsByCategory(const std::string& category)
    -> int {
    spdlog::info("Enabling all cron jobs in category: {}", category);

    auto it = category_index_.find(category);
    if (it == category_index_.end()) {
        return 0;
    }

    int count = 0;
    for (const std::string& job_id : it->second) {
        auto job_it = jobs_.find(job_id);
        if (job_it != jobs_.end() && !job_it->second->isEnabled()) {
            job_it->second->enable();
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

    auto it = category_index_.find(category);
    if (it == category_index_.end()) {
        return 0;
    }

    int count = 0;
    for (const std::string& job_id : it->second) {
        auto job_it = jobs_.find(job_id);
        if (job_it != jobs_.end() && job_it->second->isEnabled()) {
            job_it->second->disable();
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

// ============================================================================
// Property Management
// ============================================================================

auto CronManager::setJobPriority(const std::string& id, int priority) -> bool {
    if (priority < 1 || priority > 10) {
        spdlog::error("Invalid priority value {}. Must be between 1-10",
                       priority);
        return false;
    }

    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        it->second->setPriority(static_cast<JobPriority>(priority));
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

    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        it->second->setMaxRetries(static_cast<uint8_t>(maxRetries));
        it->second->resetRetries();
        spdlog::info("Set max retries to {} for job: {}", maxRetries, id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::setJobOneTime(const std::string& id, bool oneTime) -> bool {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        it->second->setOneTime(oneTime);
        spdlog::info("Set one-time status to {} for job: {}",
                      oneTime ? "true" : "false", id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

// ============================================================================
// Execution Tracking
// ============================================================================

auto CronManager::recordJobExecution(const std::string& command) -> bool {
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                           [&command](const auto& pair) {
                               return pair.second->command_ == command;
                           });

    if (it != jobs_.end()) {
        auto& job = it->second;
        job->recordExecution(true);

        if (job->isOneTime()) {
            const std::string jobId = job->getId();
            spdlog::info("One-time job completed, removing: {}", jobId);
            return deleteCronJobById(jobId);
        }

        spdlog::info("Recorded execution of job: {}", command);
        return true;
    }

    spdlog::warn("Tried to record execution for unknown job: {}", command);
    return false;
}

auto CronManager::recordJobExecutionResult(const std::string& id, bool success)
    -> bool {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        auto& job = it->second;
        job->recordExecution(success);

        if (success && job->isOneTime()) {
            spdlog::info("One-time job completed successfully, removing: {}",
                          id);
            return deleteCronJobById(id);
        }

        if (!success) {
            spdlog::warn("Job failed: {}", id);
        }

        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

void CronManager::recordJobExecution(const std::string& job_id, bool success,
                                     std::chrono::milliseconds execution_time) {
    {
        std::unique_lock<std::shared_mutex> lock(stats_mutex_);
        auto& stats = job_stats_[job_id];
        stats.total_executions++;
        if (success) {
            stats.successful_executions++;
        } else {
            stats.failed_executions++;
        }
        stats.last_execution = std::chrono::system_clock::now();
        auto total = stats.total_executions.load();
        stats.avg_execution_time = std::chrono::milliseconds(
            (stats.avg_execution_time.count() * (total - 1) +
             execution_time.count()) /
            total);
    }

    recordJobExecutionResult(job_id, success);
}

auto CronManager::getJobExecutionHistory(const std::string& id)
    -> std::vector<std::pair<std::chrono::system_clock::time_point, bool>> {
    auto it = jobs_.find(id);
    if (it != jobs_.end()) {
        auto history = it->second->getExecutionHistory();
        std::vector<std::pair<std::chrono::system_clock::time_point, bool>>
            result;
        result.reserve(history.size());
        for (const auto& entry : history) {
            result.emplace_back(entry.timestamp, entry.success);
        }
        return result;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return {};
}

// ============================================================================
// Delegated Batch/IO Operations (continued)
// ============================================================================

auto CronManager::exportToCrontab() -> bool {
    return batch_->exportToCrontab();
}

auto CronManager::batchCreateJobs(const std::vector<CronJob>& jobs) -> int {
    return batch_->batchCreateJobs(jobs);
}

auto CronManager::batchCreateJobsOptimized(std::vector<CronJob> jobs)
    -> size_t {
    return batch_->batchCreateJobsOptimized(std::move(jobs));
}

auto CronManager::batchDeleteJobs(const std::vector<std::string>& commands)
    -> int {
    return batch_->batchDeleteJobs(commands);
}

auto CronManager::batchUpdateJobs(
    const std::vector<std::pair<std::string, CronJob>>& updates) -> size_t {
    return batch_->batchUpdateJobs(updates);
}

auto CronManager::clearAllJobs() -> bool {
    spdlog::info("Clearing all cron jobs");

    auto result = CronSystem::clearSystemJobs();
    if (!result.success) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
    jobs_.clear();
    command_to_id_index_.clear();
    category_index_.clear();
    status_index_.clear();
    priority_index_.clear();
    invalidateCache();

    spdlog::info("All cron jobs cleared successfully");
    return true;
}
