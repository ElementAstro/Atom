#ifndef CRON_MANAGER_HPP
#define CRON_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <set>
#include <queue>
#include <optional>

#include "cron_job.hpp"
#include "cron_validation.hpp"

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
        return total > 0 ? static_cast<double>(successful_executions.load()) / total : 0.0;
    }
};

/**
 * @brief Cache for frequently accessed job data
 */
struct JobCache {
    std::unordered_map<std::string, std::weak_ptr<CronJob>> job_cache;
    std::unordered_map<std::string, std::vector<std::string>> category_cache;
    std::set<std::string> enabled_jobs_cache;
    std::priority_queue<std::pair<JobPriority, std::string>> priority_queue_cache;
    std::atomic<bool> cache_valid{false};

    void invalidate() { cache_valid.store(false); }
    bool isValid() const { return cache_valid.load(); }
    void markValid() { cache_valid.store(true); }
};

/**
 * @brief Optimized Cron job manager with enhanced performance and concurrency support.
 *
 * Optimizations:
 * - Thread-safe operations with shared_mutex for read/write separation
 * - Efficient indexing with multiple data structures
 * - Smart pointer management for memory efficiency
 * - Caching layer for frequently accessed data
 * - Batch operations for better performance
 * - Priority-based job scheduling
 */
class CronManager {
public:
    /**
     * @brief Constructs a new CronManager object with optimized initialization.
     */
    CronManager();

    /**
     * @brief Destroys the CronManager object with proper cleanup.
     */
    ~CronManager();

    // Disable copy operations to prevent accidental expensive copies
    CronManager(const CronManager&) = delete;
    CronManager& operator=(const CronManager&) = delete;

    // Move operations need custom implementation due to mutex members
    CronManager(CronManager&&) noexcept;
    CronManager& operator=(CronManager&&) noexcept;

    /**
     * @brief Adds a new Cron job with move semantics.
     * @param job The CronJob object to be added.
     * @return True if the job was added successfully, false otherwise.
     */
    auto createCronJob(CronJob job) -> bool;

    /**
     * @brief Creates a job from parameters with perfect forwarding.
     * @param args Arguments to construct the CronJob.
     * @return True if the job was created successfully, false otherwise.
     */
    template<typename... Args>
    auto emplaceJob(Args&&... args) -> bool {
        std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
        try {
            auto job = std::make_shared<CronJob>(std::forward<Args>(args)...);
            return addJobInternal(std::move(job));
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief Creates a new job with a special time expression.
     * @param specialTime Special time expression (e.g., @daily, @weekly).
     * @param command The command to execute.
     * @param enabled Whether the job is enabled.
     * @param category The category of the job.
     * @param description The description of the job.
     * @param priority The priority of the job.
     * @param maxRetries Maximum number of retries.
     * @param oneTime Whether this is a one-time job.
     * @return True if successful, false otherwise.
     */
    auto createJobWithSpecialTime(const std::string& specialTime,
                                  const std::string& command,
                                  bool enabled = true,
                                  const std::string& category = "default",
                                  const std::string& description = "",
                                  int priority = 5, int maxRetries = 0,
                                  bool oneTime = false) -> bool;

    /**
     * @brief Validates a cron expression.
     * @param cronExpr The cron expression to validate.
     * @return Validation result with validity and message.
     */
    static auto validateCronExpression(const std::string& cronExpr)
        -> CronValidationResult;

    /**
     * @brief Deletes a Cron job with the specified command.
     * @param command The command of the Cron job to be deleted.
     * @return True if the job was deleted successfully, false otherwise.
     */
    auto deleteCronJob(const std::string& command) -> bool;

    /**
     * @brief Deletes a Cron job by its unique identifier.
     * @param id The unique identifier of the job.
     * @return True if the job was deleted successfully, false otherwise.
     */
    auto deleteCronJobById(const std::string& id) -> bool;

    /**
     * @brief Lists all current Cron jobs.
     * @return A vector of all current CronJob objects.
     */
    auto listCronJobs() -> std::vector<CronJob>;

    /**
     * @brief Lists all current Cron jobs in a specific category.
     * @param category The category to filter by.
     * @return A vector of CronJob objects in the specified category.
     */
    auto listCronJobsByCategory(const std::string& category)
        -> std::vector<CronJob>;

    /**
     * @brief Gets all available job categories.
     * @return A vector of category names.
     */
    auto getCategories() -> std::vector<std::string>;

    /**
     * @brief Exports all Cron jobs to a JSON file.
     * @param filename The name of the file to export to.
     * @return True if the export was successful, false otherwise.
     */
    auto exportToJSON(const std::string& filename) -> bool;

    /**
     * @brief Imports Cron jobs from a JSON file.
     * @param filename The name of the file to import from.
     * @return True if the import was successful, false otherwise.
     */
    auto importFromJSON(const std::string& filename) -> bool;

    /**
     * @brief Updates an existing Cron job.
     * @param oldCommand The command of the Cron job to be updated.
     * @param newJob The new CronJob object to replace the old one.
     * @return True if the job was updated successfully, false otherwise.
     */
    auto updateCronJob(const std::string& oldCommand, const CronJob& newJob)
        -> bool;

    /**
     * @brief Updates a Cron job by its unique identifier.
     * @param id The unique identifier of the job.
     * @param newJob The new CronJob object to replace the old one.
     * @return True if the job was updated successfully, false otherwise.
     */
    auto updateCronJobById(const std::string& id, const CronJob& newJob)
        -> bool;

    /**
     * @brief Views the details of a Cron job with the specified command.
     * @param command The command of the Cron job to view.
     * @return The CronJob object with the specified command.
     */
    auto viewCronJob(const std::string& command) -> CronJob;

    /**
     * @brief Views the details of a Cron job by its unique identifier.
     * @param id The unique identifier of the job.
     * @return The CronJob object with the specified id.
     */
    auto viewCronJobById(const std::string& id) -> CronJob;

    /**
     * @brief Searches for Cron jobs that match the specified query.
     * @param query The query string to search for.
     * @return A vector of CronJob objects that match the query.
     */
    auto searchCronJobs(const std::string& query) -> std::vector<CronJob>;

    /**
     * @brief Gets statistics about the current Cron jobs.
     * @return An unordered map with statistics about the jobs.
     */
    auto statistics() -> std::unordered_map<std::string, int>;

    /**
     * @brief Enables a Cron job with the specified command.
     * @param command The command of the Cron job to enable.
     * @return True if the job was enabled successfully, false otherwise.
     */
    auto enableCronJob(const std::string& command) -> bool;

    /**
     * @brief Disables a Cron job with the specified command.
     * @param command The command of the Cron job to disable.
     * @return True if the job was disabled successfully, false otherwise.
     */
    auto disableCronJob(const std::string& command) -> bool;

    /**
     * @brief Enable or disable a Cron job by its unique identifier.
     * @param id The unique identifier of the job.
     * @param enabled Whether to enable or disable the job.
     * @return True if the operation was successful, false otherwise.
     */
    auto setJobEnabledById(const std::string& id, bool enabled) -> bool;

    /**
     * @brief Enables all Cron jobs in a specific category.
     * @param category The category of jobs to enable.
     * @return Number of jobs successfully enabled.
     */
    auto enableCronJobsByCategory(const std::string& category) -> int;

    /**
     * @brief Disables all Cron jobs in a specific category.
     * @param category The category of jobs to disable.
     * @return Number of jobs successfully disabled.
     */
    auto disableCronJobsByCategory(const std::string& category) -> int;

    /**
     * @brief Exports enabled Cron jobs to the system crontab.
     * @return True if the export was successful, false otherwise.
     */
    auto exportToCrontab() -> bool;

    /**
     * @brief Batch creation of multiple Cron jobs.
     * @param jobs Vector of CronJob objects to create.
     * @return Number of jobs successfully created.
     */
    auto batchCreateJobs(const std::vector<CronJob>& jobs) -> int;

    /**
     * @brief Batch deletion of multiple Cron jobs.
     * @param commands Vector of commands identifying jobs to delete.
     * @return Number of jobs successfully deleted.
     */
    auto batchDeleteJobs(const std::vector<std::string>& commands) -> int;

    /**
     * @brief Records that a job has been executed.
     * @param command The command of the executed job.
     * @return True if the job was found and updated, false otherwise.
     */
    auto recordJobExecution(const std::string& command) -> bool;

    /**
     * @brief Clears all cron jobs in memory and from system crontab.
     * @return True if all jobs were cleared successfully, false otherwise.
     */
    auto clearAllJobs() -> bool;

    /**
     * @brief Converts a special cron expression to standard format.
     * @param specialExpr The special expression to convert (e.g., @daily).
     * @return The standard cron expression or empty string if not recognized.
     */
    static auto convertSpecialExpression(const std::string& specialExpr)
        -> std::string;

    /**
     * @brief Sets the priority of a job.
     * @param id The unique identifier of the job.
     * @param priority Priority value (1-10, 1 is highest).
     * @return True if successful, false otherwise.
     */
    auto setJobPriority(const std::string& id, int priority) -> bool;

    /**
     * @brief Sets the maximum number of retries for a job.
     * @param id The unique identifier of the job.
     * @param maxRetries Maximum retry count.
     * @return True if successful, false otherwise.
     */
    auto setJobMaxRetries(const std::string& id, int maxRetries) -> bool;

    /**
     * @brief Sets whether a job is a one-time job.
     * @param id The unique identifier of the job.
     * @param oneTime Whether the job should be deleted after execution.
     * @return True if successful, false otherwise.
     */
    auto setJobOneTime(const std::string& id, bool oneTime) -> bool;

    /**
     * @brief Gets the execution history of a job.
     * @param id The unique identifier of the job.
     * @return Vector of execution history entries (timestamp, success status).
     */
    auto getJobExecutionHistory(const std::string& id)
        -> std::vector<std::pair<std::chrono::system_clock::time_point, bool>>;

    /**
     * @brief Record a job execution result.
     * @param id The unique identifier of the job.
     * @param success Whether the execution was successful.
     * @return True if the record was added, false otherwise.
     */
    auto recordJobExecutionResult(const std::string& id, bool success) -> bool;

    /**
     * @brief Get jobs sorted by priority.
     * @return Vector of jobs sorted by priority (highest first).
     */
    auto getJobsByPriority() -> std::vector<CronJob>;

    /**
     * @brief Enhanced batch operations for better performance.
     */
    auto batchCreateJobsOptimized(std::vector<CronJob> jobs) -> size_t;
    auto batchUpdateJobs(const std::vector<std::pair<std::string, CronJob>>& updates) -> size_t;

    /**
     * @brief Advanced querying with caching.
     */
    auto getJobsByCategory(const std::string& category) -> std::vector<std::shared_ptr<CronJob>>;
    auto getJobsByStatus(JobStatus status) -> std::vector<std::shared_ptr<CronJob>>;
    auto getJobsByPriorityRange(JobPriority min_priority, JobPriority max_priority)
        -> std::vector<std::shared_ptr<CronJob>>;

    /**
     * @brief Performance monitoring.
     */
    auto getJobStats(const std::string& job_id) -> std::optional<JobStats>;
    auto getOverallStats() -> JobStats;
    void recordJobExecution(const std::string& job_id, bool success,
                           std::chrono::milliseconds execution_time);

    /**
     * @brief Cache management.
     */
    void invalidateCache();
    void rebuildCache();
    auto getCachedJob(const std::string& job_id) -> std::shared_ptr<CronJob>;

private:
    // Core data storage with smart pointers for efficient memory management
    std::unordered_map<std::string, std::shared_ptr<CronJob>> jobs_;

    // Multiple indices for fast lookups
    std::unordered_map<std::string, std::string> command_to_id_index_;
    std::unordered_map<std::string, std::vector<std::string>> category_index_;
    std::unordered_map<JobStatus, std::set<std::string>> status_index_;
    std::unordered_map<JobPriority, std::set<std::string>> priority_index_;

    // Performance monitoring
    std::unordered_map<std::string, JobStats> job_stats_;

    // Caching layer
    mutable JobCache cache_;

    // Thread safety
    mutable std::shared_mutex jobs_mutex_;
    mutable std::shared_mutex stats_mutex_;
    mutable std::shared_mutex cache_mutex_;

    // Configuration
    std::atomic<size_t> max_jobs_{10000};
    std::atomic<bool> auto_cleanup_enabled_{true};

    // Internal helper methods
    auto addJobInternal(std::shared_ptr<CronJob> job) -> bool;
    auto removeJobInternal(const std::string& job_id) -> bool;
    void updateIndices(const std::string& job_id, const CronJob& job);
    void removeFromIndices(const std::string& job_id, const CronJob& job);
    auto validateJobInternal(const CronJob& job) -> bool;
    auto generateJobId(const CronJob& job) -> std::string;
    void cleanupExpiredJobs();
    auto findJobById(const std::string& job_id) -> std::shared_ptr<CronJob>;
    auto findJobByCommand(const std::string& command) -> std::shared_ptr<CronJob>;
};

#endif // CRON_MANAGER_HPP
