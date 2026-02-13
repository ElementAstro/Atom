#ifndef CRON_MANAGER_HPP
#define CRON_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "cron_job.hpp"
#include "cron_manager_batch.hpp"
#include "cron_manager_query.hpp"
#include "cron_manager_types.hpp"
#include "cron_validation.hpp"

/**
 * @brief Optimized Cron job manager with enhanced performance and concurrency
 * support.
 *
 * Core CRUD operations, enable/disable, property management, execution
 * tracking, and cache management. Query/search and batch/IO operations are
 * delegated to CronManagerQuery and CronManagerBatch respectively.
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

    // ========================================================================
    // Core CRUD Operations
    // ========================================================================

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
    template <typename... Args>
    auto emplaceJob(Args&&... args) -> bool {
        std::unique_lock<std::shared_mutex> lock(jobs_mutex_);
        try {
            auto job =
                std::make_shared<CronJob>(std::forward<Args>(args)...);
            return addJobInternal(std::move(job));
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief Creates a new job with a special time expression.
     */
    auto createJobWithSpecialTime(
        const std::string& specialTime, const std::string& command,
        bool enabled = true, const std::string& category = "default",
        const std::string& description = "", int priority = 5,
        int maxRetries = 0, bool oneTime = false) -> bool;

    /**
     * @brief Validates a cron expression.
     */
    static auto validateCronExpression(const std::string& cronExpr)
        -> CronValidationResult;

    /**
     * @brief Converts a special cron expression to standard format.
     */
    static auto convertSpecialExpression(const std::string& specialExpr)
        -> std::string;

    /**
     * @brief Deletes a Cron job with the specified command.
     */
    auto deleteCronJob(const std::string& command) -> bool;

    /**
     * @brief Deletes a Cron job by its unique identifier.
     */
    auto deleteCronJobById(const std::string& id) -> bool;

    /**
     * @brief Updates an existing Cron job.
     */
    auto updateCronJob(const std::string& oldCommand, const CronJob& newJob)
        -> bool;

    /**
     * @brief Updates a Cron job by its unique identifier.
     */
    auto updateCronJobById(const std::string& id, const CronJob& newJob)
        -> bool;

    // ========================================================================
    // Enable/Disable Operations
    // ========================================================================

    auto enableCronJob(const std::string& command) -> bool;
    auto disableCronJob(const std::string& command) -> bool;
    auto setJobEnabledById(const std::string& id, bool enabled) -> bool;
    auto enableCronJobsByCategory(const std::string& category) -> int;
    auto disableCronJobsByCategory(const std::string& category) -> int;

    // ========================================================================
    // Property Management
    // ========================================================================

    auto setJobPriority(const std::string& id, int priority) -> bool;
    auto setJobMaxRetries(const std::string& id, int maxRetries) -> bool;
    auto setJobOneTime(const std::string& id, bool oneTime) -> bool;

    // ========================================================================
    // Execution Tracking
    // ========================================================================

    auto recordJobExecution(const std::string& command) -> bool;
    auto recordJobExecutionResult(const std::string& id, bool success) -> bool;
    auto getJobExecutionHistory(const std::string& id)
        -> std::vector<std::pair<std::chrono::system_clock::time_point, bool>>;
    void recordJobExecution(const std::string& job_id, bool success,
                            std::chrono::milliseconds execution_time);

    // ========================================================================
    // Cache Management
    // ========================================================================

    void invalidateCache();
    void rebuildCache();
    auto getCachedJob(const std::string& job_id) -> std::shared_ptr<CronJob>;

    // ========================================================================
    // Delegated Query Operations (via CronManagerQuery)
    // ========================================================================

    auto listCronJobs() -> std::vector<CronJob>;
    auto listCronJobsByCategory(const std::string& category)
        -> std::vector<CronJob>;
    auto getCategories() -> std::vector<std::string>;
    auto viewCronJob(const std::string& command) -> CronJob;
    auto viewCronJobById(const std::string& id) -> CronJob;
    auto searchCronJobs(const std::string& query) -> std::vector<CronJob>;
    auto statistics() -> std::unordered_map<std::string, int>;
    auto getJobsByPriority() -> std::vector<CronJob>;
    auto getJobsByCategory(const std::string& category)
        -> std::vector<std::shared_ptr<CronJob>>;
    auto getJobsByStatus(JobStatus status)
        -> std::vector<std::shared_ptr<CronJob>>;
    auto getJobsByPriorityRange(JobPriority min_priority,
                                JobPriority max_priority)
        -> std::vector<std::shared_ptr<CronJob>>;
    auto getJobStats(const std::string& job_id) -> std::optional<JobStats>;
    auto getOverallStats() -> JobStats;

    // ========================================================================
    // Delegated Batch/IO Operations (via CronManagerBatch)
    // ========================================================================

    auto batchCreateJobs(const std::vector<CronJob>& jobs) -> int;
    auto batchCreateJobsOptimized(std::vector<CronJob> jobs) -> size_t;
    auto batchDeleteJobs(const std::vector<std::string>& commands) -> int;
    auto batchUpdateJobs(
        const std::vector<std::pair<std::string, CronJob>>& updates) -> size_t;
    auto exportToJSON(const std::string& filename) -> bool;
    auto importFromJSON(const std::string& filename) -> bool;
    auto exportToCrontab() -> bool;
    auto clearAllJobs() -> bool;

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

    // Sub-component helpers
    std::unique_ptr<CronManagerQuery> query_;
    std::unique_ptr<CronManagerBatch> batch_;

    // Internal helper methods
    auto addJobInternal(std::shared_ptr<CronJob> job) -> bool;
    auto removeJobInternal(const std::string& job_id) -> bool;
    void updateIndices(const std::string& job_id, const CronJob& job);
    void removeFromIndices(const std::string& job_id, const CronJob& job);
    auto validateJobInternal(const CronJob& job) -> bool;
    auto generateJobId(const CronJob& job) -> std::string;
    void cleanupExpiredJobs();
    auto findJobById(const std::string& job_id) -> std::shared_ptr<CronJob>;
    auto findJobByCommand(const std::string& command)
        -> std::shared_ptr<CronJob>;

    void initSubComponents();
};

#endif  // CRON_MANAGER_HPP
