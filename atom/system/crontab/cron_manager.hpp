#ifndef CRON_MANAGER_HPP
#define CRON_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#include "cron_job.hpp"
#include "cron_validation.hpp"

/**
 * @brief Manages a collection of Cron jobs.
 */
class CronManager {
public:
    /**
     * @brief Constructs a new CronManager object.
     */
    CronManager();

    /**
     * @brief Destroys the CronManager object.
     */
    ~CronManager();

    /**
     * @brief Adds a new Cron job.
     * @param job The CronJob object to be added.
     * @return True if the job was added successfully, false otherwise.
     */
    auto createCronJob(const CronJob& job) -> bool;

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

private:
    std::vector<CronJob> jobs_;
    std::unordered_map<std::string, size_t> jobIndex_;
    std::unordered_map<std::string, std::vector<size_t>> categoryIndex_;

    void refreshJobIndex();
    auto validateJob(const CronJob& job) -> bool;
    auto handleJobFailure(const std::string& id) -> bool;
};

#endif // CRON_MANAGER_HPP
