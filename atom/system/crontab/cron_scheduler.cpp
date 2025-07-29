#include "cron_scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <regex>

#include "spdlog/spdlog.h"

CronScheduler::CronScheduler() : default_timezone_("UTC", 0, false) {
    spdlog::info("CronScheduler initialized");
}

CronScheduler::~CronScheduler() {
    stop();
    spdlog::info("CronScheduler destroyed");
}

auto CronScheduler::start() -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    if (running_.load()) {
        spdlog::warn("CronScheduler is already running");
        return true;
    }

    try {
        running_.store(true);
        scheduler_thread_ = std::thread(&CronScheduler::schedulerLoop, this);

        spdlog::info("CronScheduler started successfully");
        return true;

    } catch (const std::exception& e) {
        running_.store(false);
        spdlog::error("Failed to start CronScheduler: {}", e.what());
        return false;
    }
}

void CronScheduler::stop() {
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        scheduler_cv_.notify_all();
    }

    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }

    spdlog::info("CronScheduler stopped");
}

auto CronScheduler::isRunning() const -> bool {
    return running_.load();
}

auto CronScheduler::addJob(std::shared_ptr<CronJob> job, const ScheduleConfig& config) -> bool {
    if (!job) {
        spdlog::error("Cannot add null job to scheduler");
        return false;
    }

    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    std::string job_id = job->getId();

    // Check if job already exists
    if (jobs_.find(job_id) != jobs_.end()) {
        spdlog::warn("Job {} already exists in scheduler", job_id);
        return false;
    }

    // Add job and configuration
    jobs_[job_id] = job;
    job_schedules_[job_id] = config;

    // Calculate initial next execution time
    auto next_time = calculateNextExecution(job_id);
    if (next_time.has_value()) {
        next_executions_[job_id] = next_time.value();
    }

    spdlog::info("Added job {} to scheduler with pattern {}", job_id, static_cast<int>(config.pattern));
    scheduler_cv_.notify_one();

    return true;
}

auto CronScheduler::removeJob(const std::string& job_id) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto job_it = jobs_.find(job_id);
    if (job_it == jobs_.end()) {
        spdlog::warn("Job {} not found in scheduler", job_id);
        return false;
    }

    // Remove from all data structures
    jobs_.erase(job_it);
    job_schedules_.erase(job_id);
    next_executions_.erase(job_id);
    job_completions_.erase(job_id);

    // Remove dependencies involving this job
    dependencies_.erase(
        std::remove_if(dependencies_.begin(), dependencies_.end(),
                      [&job_id](const JobDependency& dep) {
                          return dep.dependent_job_id == job_id ||
                                 dep.prerequisite_job_id == job_id;
                      }),
        dependencies_.end());

    // Remove conditions for this job
    auto cond_it = conditions_.begin();
    while (cond_it != conditions_.end()) {
        if (cond_it->second.job_id == job_id) {
            cond_it = conditions_.erase(cond_it);
        } else {
            ++cond_it;
        }
    }

    spdlog::info("Removed job {} from scheduler", job_id);
    return true;
}

auto CronScheduler::updateJobSchedule(const std::string& job_id, const ScheduleConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto job_it = jobs_.find(job_id);
    if (job_it == jobs_.end()) {
        spdlog::warn("Job {} not found in scheduler", job_id);
        return false;
    }

    // Update configuration
    job_schedules_[job_id] = config;

    // Recalculate next execution time
    auto next_time = calculateNextExecution(job_id);
    if (next_time.has_value()) {
        next_executions_[job_id] = next_time.value();
    } else {
        next_executions_.erase(job_id);
    }

    spdlog::info("Updated schedule for job {} with pattern {}", job_id, static_cast<int>(config.pattern));
    scheduler_cv_.notify_one();

    return true;
}

auto CronScheduler::addDependency(const JobDependency& dependency) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    // Validate that both jobs exist
    if (jobs_.find(dependency.dependent_job_id) == jobs_.end() ||
        jobs_.find(dependency.prerequisite_job_id) == jobs_.end()) {
        spdlog::error("Cannot add dependency: one or both jobs do not exist");
        return false;
    }

    // Check for circular dependencies (simplified check)
    for (const auto& dep : dependencies_) {
        if (dep.dependent_job_id == dependency.prerequisite_job_id &&
            dep.prerequisite_job_id == dependency.dependent_job_id) {
            spdlog::error("Circular dependency detected between {} and {}",
                         dependency.dependent_job_id, dependency.prerequisite_job_id);
            return false;
        }
    }

    dependencies_.push_back(dependency);
    spdlog::info("Added dependency: {} depends on {} (type: {})",
                dependency.dependent_job_id, dependency.prerequisite_job_id,
                static_cast<int>(dependency.type));

    return true;
}

auto CronScheduler::removeDependency(const std::string& dependent_job_id,
                                    const std::string& prerequisite_job_id) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto it = std::find_if(dependencies_.begin(), dependencies_.end(),
                          [&](const JobDependency& dep) {
                              return dep.dependent_job_id == dependent_job_id &&
                                     dep.prerequisite_job_id == prerequisite_job_id;
                          });

    if (it != dependencies_.end()) {
        dependencies_.erase(it);
        spdlog::info("Removed dependency: {} no longer depends on {}",
                    dependent_job_id, prerequisite_job_id);
        return true;
    }

    spdlog::warn("Dependency not found: {} -> {}", dependent_job_id, prerequisite_job_id);
    return false;
}

auto CronScheduler::getJobDependencies(const std::string& job_id) -> std::vector<JobDependency> {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    std::vector<JobDependency> result;
    std::copy_if(dependencies_.begin(), dependencies_.end(), std::back_inserter(result),
                [&job_id](const JobDependency& dep) {
                    return dep.dependent_job_id == job_id;
                });

    return result;
}

auto CronScheduler::addCondition(const ExecutionCondition& condition) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    // Validate that the job exists
    if (jobs_.find(condition.job_id) == jobs_.end()) {
        spdlog::error("Cannot add condition: job {} does not exist", condition.job_id);
        return false;
    }

    conditions_[condition.condition_id] = condition;
    spdlog::info("Added execution condition {} for job {}",
                condition.condition_id, condition.job_id);

    return true;
}

auto CronScheduler::removeCondition(const std::string& condition_id) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto it = conditions_.find(condition_id);
    if (it != conditions_.end()) {
        spdlog::info("Removed execution condition {} for job {}",
                    condition_id, it->second.job_id);
        conditions_.erase(it);
        return true;
    }

    spdlog::warn("Condition {} not found", condition_id);
    return false;
}

auto CronScheduler::evaluateConditions(const std::string& job_id) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    for (const auto& [condition_id, condition] : conditions_) {
        if (condition.job_id == job_id && condition.is_enabled) {
            try {
                if (!condition.condition_func()) {
                    spdlog::debug("Condition {} failed for job {}", condition_id, job_id);
                    return false;
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception in condition {}: {}", condition_id, e.what());
                return false;
            }
        }
    }

    return true;
}

void CronScheduler::setDefaultTimezone(const TimezoneInfo& timezone) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    default_timezone_ = timezone;
    spdlog::info("Set default timezone to {} (UTC{}{})",
                timezone.timezone_id,
                timezone.utc_offset_minutes >= 0 ? "+" : "",
                timezone.utc_offset_minutes / 60);
}

auto CronScheduler::getDefaultTimezone() const -> const TimezoneInfo& {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    return default_timezone_;
}

auto CronScheduler::convertTimezone(std::chrono::system_clock::time_point time,
                                   const TimezoneInfo& from_tz,
                                   const TimezoneInfo& to_tz) -> std::chrono::system_clock::time_point {
    // Simple timezone conversion (in production, use proper timezone library)
    auto offset_diff = std::chrono::minutes(to_tz.utc_offset_minutes - from_tz.utc_offset_minutes);
    return time + offset_diff;
}

auto CronScheduler::calculateNextExecution(const std::string& job_id,
                                          std::chrono::system_clock::time_point from_time)
    -> std::optional<std::chrono::system_clock::time_point> {

    auto job_it = jobs_.find(job_id);
    auto config_it = job_schedules_.find(job_id);

    if (job_it == jobs_.end() || config_it == job_schedules_.end()) {
        return std::nullopt;
    }

    const auto& config = config_it->second;

    switch (config.pattern) {
        case SchedulePattern::STANDARD_CRON: {
            // Use existing cron parsing logic (simplified)
            // In production, implement full cron expression parsing
            return from_time + std::chrono::hours(1); // Placeholder
        }
        case SchedulePattern::INTERVAL: {
            return calculateIntervalExecution(config, from_time);
        }
        case SchedulePattern::DELAY: {
            auto completion_it = job_completions_.find(job_id);
            if (completion_it != job_completions_.end()) {
                return completion_it->second + config.delay;
            }
            return from_time + config.delay;
        }
        case SchedulePattern::BUSINESS_HOURS: {
            auto next_time = from_time;
            while (!isBusinessHours(next_time, config)) {
                next_time += std::chrono::hours(1);
            }
            return next_time;
        }
        case SchedulePattern::HOLIDAY_AWARE: {
            auto next_time = from_time + std::chrono::hours(24);
            while (isHoliday(next_time, config)) {
                next_time += std::chrono::hours(24);
            }
            return next_time;
        }
        case SchedulePattern::LOAD_AWARE: {
            if (getSystemLoad() <= config.max_system_load) {
                return from_time + std::chrono::minutes(5);
            }
            return from_time + std::chrono::minutes(15);
        }
        default:
            return std::nullopt;
    }
}

auto CronScheduler::getScheduledJobs(std::chrono::system_clock::time_point start_time,
                                    std::chrono::system_clock::time_point end_time)
    -> std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> {

    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> result;

    for (const auto& [job_id, next_time] : next_executions_) {
        if (next_time >= start_time && next_time <= end_time) {
            result.emplace_back(job_id, next_time);
        }
    }

    // Sort by execution time
    std::sort(result.begin(), result.end(),
             [](const auto& a, const auto& b) {
                 return a.second < b.second;
             });

    return result;
}

auto CronScheduler::triggerJob(const std::string& job_id,
                              const std::optional<ExecutionContext>& context) -> bool {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto job_it = jobs_.find(job_id);
    if (job_it == jobs_.end()) {
        spdlog::error("Cannot trigger job {}: job not found", job_id);
        return false;
    }

    ExecutionContext exec_context = context.value_or(ExecutionContext(job_id));
    exec_context.actual_start_time = std::chrono::system_clock::now();

    // Execute in separate thread to avoid blocking
    std::thread([this, job_id, exec_context]() {
        executeJob(job_id, exec_context);
    }).detach();

    spdlog::info("Manually triggered job {}", job_id);
    return true;
}

void CronScheduler::schedulerLoop() {
    spdlog::info("CronScheduler main loop started");

    while (running_.load()) {
        try {
            std::unique_lock<std::mutex> lock(scheduler_mutex_);

            auto now = std::chrono::system_clock::now();
            std::vector<std::string> jobs_to_execute;

            // Find jobs ready for execution
            for (auto& [job_id, next_time] : next_executions_) {
                if (next_time <= now) {
                    // Check dependencies
                    bool dependencies_satisfied = true;
                    for (const auto& dep : dependencies_) {
                        if (dep.dependent_job_id == job_id) {
                            if (!isDependencySatisfied(dep)) {
                                dependencies_satisfied = false;
                                break;
                            }
                        }
                    }

                    // Check conditions
                    if (dependencies_satisfied && evaluateConditions(job_id)) {
                        jobs_to_execute.push_back(job_id);
                    }
                }
            }

            // Execute jobs (unlock during execution)
            lock.unlock();

            for (const auto& job_id : jobs_to_execute) {
                ExecutionContext context(job_id);
                context.scheduled_time = next_executions_[job_id];
                context.actual_start_time = std::chrono::system_clock::now();
                context.timezone = default_timezone_;

                // Execute in separate thread
                std::thread([this, job_id, context]() {
                    executeJob(job_id, context);
                }).detach();

                // Update next execution time
                lock.lock();
                auto next_time = calculateNextExecution(job_id, std::chrono::system_clock::now());
                if (next_time.has_value()) {
                    next_executions_[job_id] = next_time.value();
                } else {
                    next_executions_.erase(job_id);
                }
                lock.unlock();
            }

            // Wait for next check or notification
            lock.lock();
            scheduler_cv_.wait_for(lock, std::chrono::seconds(10));

        } catch (const std::exception& e) {
            spdlog::error("Exception in scheduler loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    spdlog::info("CronScheduler main loop stopped");
}

auto CronScheduler::isDependencySatisfied(const JobDependency& dep) -> bool {
    auto completion_it = job_completions_.find(dep.prerequisite_job_id);
    if (completion_it == job_completions_.end()) {
        return false; // Prerequisite has never completed
    }

    // Check timeout
    auto now = std::chrono::system_clock::now();
    if (now - completion_it->second > dep.timeout) {
        return false;
    }

    // For now, assume all completions are successful
    // In production, track success/failure status
    return true;
}

auto CronScheduler::isBusinessHours(std::chrono::system_clock::time_point time,
                                   const ScheduleConfig& config) -> bool {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto tm = *std::localtime(&time_t);

    // Check if it's a business day
    int weekday = tm.tm_wday; // 0 = Sunday, 1 = Monday, etc.
    bool is_business_day = std::find(config.business_days.begin(),
                                    config.business_days.end(),
                                    weekday) != config.business_days.end();

    if (!is_business_day) {
        return false;
    }

    // Check if it's within business hours (simplified)
    int hour = tm.tm_hour;
    return hour >= 9 && hour < 17; // 9 AM to 5 PM
}

auto CronScheduler::isHoliday(std::chrono::system_clock::time_point time,
                             const ScheduleConfig& config) -> bool {
    // Simplified holiday check
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto tm = *std::localtime(&time_t);

    char date_str[11];
    std::strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);

    return std::find(config.holidays.begin(), config.holidays.end(),
                    std::string(date_str)) != config.holidays.end();
}

auto CronScheduler::getSystemLoad() -> double {
    // Simplified system load check
    // In production, read from /proc/loadavg or use system APIs
    return 0.5; // Placeholder
}

auto CronScheduler::calculateIntervalExecution(const ScheduleConfig& config,
                                              std::chrono::system_clock::time_point from_time)
    -> std::chrono::system_clock::time_point {
    return from_time + config.interval;
}

void CronScheduler::executeJob(const std::string& job_id, const ExecutionContext& context) {
    spdlog::info("Executing job {} (scheduled: {}, actual: {})",
                job_id,
                std::chrono::duration_cast<std::chrono::seconds>(
                    context.scheduled_time.time_since_epoch()).count(),
                std::chrono::duration_cast<std::chrono::seconds>(
                    context.actual_start_time.time_since_epoch()).count());

    auto job_it = jobs_.find(job_id);
    if (job_it == jobs_.end()) {
        spdlog::error("Job {} not found during execution", job_id);
        return;
    }

    auto job = job_it->second;
    bool success = false;

    try {
        // Record execution start
        job->recordExecution(true); // Simplified - assume success for now
        success = true;

        spdlog::info("Job {} executed successfully", job_id);

    } catch (const std::exception& e) {
        job->recordExecution(false, e.what());
        spdlog::error("Job {} execution failed: {}", job_id, e.what());
    }

    recordJobCompletion(job_id, success);
}

void CronScheduler::recordJobCompletion(const std::string& job_id, bool success) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    job_completions_[job_id] = std::chrono::system_clock::now();

    spdlog::debug("Recorded completion for job {} (success: {})", job_id, success);
}
