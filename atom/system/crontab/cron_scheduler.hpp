#ifndef CRON_SCHEDULER_HPP
#define CRON_SCHEDULER_HPP

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "cron_job.hpp"
#include "cron_types.hpp"

/**
 * @brief Job dependency types
 */
enum class DependencyType {
    SUCCESS = 0,    // Depends on successful completion
    COMPLETION = 1, // Depends on any completion (success or failure)
    FAILURE = 2,    // Depends on failure
    ALWAYS = 3      // Always run regardless
};

/**
 * @brief Job dependency definition
 */
struct JobDependency {
    std::string dependent_job_id;
    std::string prerequisite_job_id;
    DependencyType type;
    std::chrono::minutes timeout{60}; // Max wait time for prerequisite

    JobDependency(std::string dep_id, std::string prereq_id, DependencyType dep_type)
        : dependent_job_id(std::move(dep_id)), prerequisite_job_id(std::move(prereq_id)), type(dep_type) {}
};

/**
 * @brief Conditional execution rule
 */
struct ExecutionCondition {
    std::string condition_id;
    std::string job_id;
    std::function<bool()> condition_func;
    std::string description;
    bool is_enabled;

    ExecutionCondition(std::string id, std::string j_id, std::function<bool()> func, std::string desc)
        : condition_id(std::move(id)), job_id(std::move(j_id)), condition_func(std::move(func)),
          description(std::move(desc)), is_enabled(true) {}
};

/**
 * @brief Advanced scheduling pattern types
 */
enum class SchedulePattern {
    STANDARD_CRON = 0,  // Traditional 5-field cron
    EXTENDED_CRON = 1,  // 6-field cron with seconds
    INTERVAL = 2,       // Fixed interval execution
    DELAY = 3,          // Delay after previous execution
    BUSINESS_HOURS = 4, // Only during business hours
    HOLIDAY_AWARE = 5,  // Skip holidays
    LOAD_AWARE = 6      // Based on system load
};

/**
 * @brief Extended scheduling configuration
 */
struct ScheduleConfig {
    SchedulePattern pattern;
    std::string expression;
    TimezoneInfo timezone;
    std::chrono::minutes interval{0};
    std::chrono::minutes delay{0};
    std::string business_hours_start{"09:00"};
    std::string business_hours_end{"17:00"};
    std::vector<int> business_days{1, 2, 3, 4, 5}; // Monday-Friday
    std::vector<std::string> holidays;
    double max_system_load{0.8};

    ScheduleConfig(SchedulePattern p = SchedulePattern::STANDARD_CRON, std::string expr = "")
        : pattern(p), expression(std::move(expr)) {}
};

/**
 * @brief Job execution context with advanced scheduling
 */
struct ExecutionContext {
    std::string job_id;
    std::chrono::system_clock::time_point scheduled_time;
    std::chrono::system_clock::time_point actual_start_time;
    TimezoneInfo timezone;
    std::unordered_map<std::string, std::string> environment_vars;
    std::string working_directory;
    int priority_boost{0};
    bool is_retry{false};
    int retry_count{0};

    ExecutionContext(std::string id) : job_id(std::move(id)) {}
};

/**
 * @brief Advanced cron scheduler with dependency management and conditional execution
 */
class CronScheduler {
public:
    /**
     * @brief Constructs a new CronScheduler
     */
    CronScheduler();

    /**
     * @brief Destructor
     */
    ~CronScheduler();

    // Disable copy operations
    CronScheduler(const CronScheduler&) = delete;
    CronScheduler& operator=(const CronScheduler&) = delete;

    // Enable move operations
    CronScheduler(CronScheduler&&) noexcept = default;
    CronScheduler& operator=(CronScheduler&&) noexcept = default;

    /**
     * @brief Starts the scheduler
     * @return True if started successfully
     */
    auto start() -> bool;

    /**
     * @brief Stops the scheduler
     */
    void stop();

    /**
     * @brief Checks if scheduler is running
     * @return True if running
     */
    auto isRunning() const -> bool;

    // Job management with advanced scheduling
    /**
     * @brief Adds a job with advanced scheduling configuration
     * @param job The job to add
     * @param config Advanced scheduling configuration
     * @return True if added successfully
     */
    auto addJob(std::shared_ptr<CronJob> job, const ScheduleConfig& config) -> bool;

    /**
     * @brief Removes a job from the scheduler
     * @param job_id Job identifier
     * @return True if removed successfully
     */
    auto removeJob(const std::string& job_id) -> bool;

    /**
     * @brief Updates job scheduling configuration
     * @param job_id Job identifier
     * @param config New scheduling configuration
     * @return True if updated successfully
     */
    auto updateJobSchedule(const std::string& job_id, const ScheduleConfig& config) -> bool;

    // Dependency management
    /**
     * @brief Adds a job dependency
     * @param dependency Dependency definition
     * @return True if added successfully
     */
    auto addDependency(const JobDependency& dependency) -> bool;

    /**
     * @brief Removes a job dependency
     * @param dependent_job_id Dependent job ID
     * @param prerequisite_job_id Prerequisite job ID
     * @return True if removed successfully
     */
    auto removeDependency(const std::string& dependent_job_id,
                         const std::string& prerequisite_job_id) -> bool;

    /**
     * @brief Gets all dependencies for a job
     * @param job_id Job identifier
     * @return Vector of dependencies
     */
    auto getJobDependencies(const std::string& job_id) -> std::vector<JobDependency>;

    // Conditional execution
    /**
     * @brief Adds an execution condition
     * @param condition Execution condition
     * @return True if added successfully
     */
    auto addCondition(const ExecutionCondition& condition) -> bool;

    /**
     * @brief Removes an execution condition
     * @param condition_id Condition identifier
     * @return True if removed successfully
     */
    auto removeCondition(const std::string& condition_id) -> bool;

    /**
     * @brief Evaluates all conditions for a job
     * @param job_id Job identifier
     * @return True if all conditions are met
     */
    auto evaluateConditions(const std::string& job_id) -> bool;

    // Timezone support
    /**
     * @brief Sets the default timezone for scheduling
     * @param timezone Timezone information
     */
    void setDefaultTimezone(const TimezoneInfo& timezone);

    /**
     * @brief Gets the default timezone
     * @return Current default timezone
     */
    auto getDefaultTimezone() const -> const TimezoneInfo&;

    /**
     * @brief Converts time between timezones
     * @param time Time point to convert
     * @param from_tz Source timezone
     * @param to_tz Target timezone
     * @return Converted time point
     */
    static auto convertTimezone(std::chrono::system_clock::time_point time,
                               const TimezoneInfo& from_tz,
                               const TimezoneInfo& to_tz) -> std::chrono::system_clock::time_point;

    // Advanced scheduling patterns
    /**
     * @brief Calculates next execution time for a job
     * @param job_id Job identifier
     * @param from_time Starting time point
     * @return Next execution time if calculable
     */
    auto calculateNextExecution(const std::string& job_id,
                               std::chrono::system_clock::time_point from_time = std::chrono::system_clock::now())
        -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Gets all jobs scheduled for execution within a time range
     * @param start_time Range start
     * @param end_time Range end
     * @return Vector of job IDs and their execution times
     */
    auto getScheduledJobs(std::chrono::system_clock::time_point start_time,
                         std::chrono::system_clock::time_point end_time)
        -> std::vector<std::pair<std::string, std::chrono::system_clock::time_point>>;

    /**
     * @brief Manually triggers job execution
     * @param job_id Job identifier
     * @param context Optional execution context
     * @return True if triggered successfully
     */
    auto triggerJob(const std::string& job_id,
                   const std::optional<ExecutionContext>& context = std::nullopt) -> bool;

private:
    mutable std::mutex scheduler_mutex_;
    std::condition_variable scheduler_cv_;
    std::atomic<bool> running_{false};
    std::thread scheduler_thread_;

    // Job storage and configuration
    std::unordered_map<std::string, std::shared_ptr<CronJob>> jobs_;
    std::unordered_map<std::string, ScheduleConfig> job_schedules_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> next_executions_;

    // Dependencies and conditions
    std::vector<JobDependency> dependencies_;
    std::unordered_map<std::string, ExecutionCondition> conditions_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> job_completions_;

    // Configuration
    TimezoneInfo default_timezone_;

    // Internal methods
    void schedulerLoop();
    auto isDependencySatisfied(const JobDependency& dep) -> bool;
    auto isBusinessHours(std::chrono::system_clock::time_point time, const ScheduleConfig& config) -> bool;
    auto isHoliday(std::chrono::system_clock::time_point time, const ScheduleConfig& config) -> bool;
    auto getSystemLoad() -> double;
    auto parseExtendedCron(const std::string& expression) -> std::optional<std::chrono::system_clock::time_point>;
    auto calculateIntervalExecution(const ScheduleConfig& config,
                                   std::chrono::system_clock::time_point from_time) -> std::chrono::system_clock::time_point;
    void executeJob(const std::string& job_id, const ExecutionContext& context);
    void recordJobCompletion(const std::string& job_id, bool success);
};

#endif // CRON_SCHEDULER_HPP
