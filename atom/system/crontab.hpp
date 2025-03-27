#ifndef CRONJOB_H
#define CRONJOB_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include "atom/type/json_fwd.hpp"

/**
 * @brief Represents a Cron job with a scheduled time and command.
 */
struct alignas(64) CronJob {
public:
    std::string time_;         ///< Scheduled time for the Cron job.
    std::string command_;      ///< Command to be executed by the Cron job.
    bool enabled_;             ///< Status of the Cron job.
    std::string category_;     ///< Category of the Cron job for organization.
    std::string description_;  ///< Description of what the job does.
    std::chrono::system_clock::time_point created_at_;  ///< Creation timestamp.
    std::chrono::system_clock::time_point
        last_run_;   ///< Last execution timestamp.
    int run_count_;  ///< Number of times this job has been executed.

    CronJob(const std::string& time = "", const std::string& command = "",
            bool enabled = true, const std::string& category = "default",
            const std::string& description = "")
        : time_(time),
          command_(command),
          enabled_(enabled),
          category_(category),
          description_(description),
          created_at_(std::chrono::system_clock::now()),
          last_run_(std::chrono::system_clock::time_point()),
          run_count_(0) {}

    /**
     * @brief Converts the CronJob object to a JSON representation.
     * @return JSON representation of the CronJob object.
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Creates a CronJob object from a JSON representation.
     * @param jsonObj JSON object representing a CronJob.
     * @return CronJob object created from the JSON representation.
     */
    static auto fromJson(const nlohmann::json& jsonObj) -> CronJob;

    /**
     * @brief Gets a unique identifier for this job.
     * @return A string that uniquely identifies this job.
     */
    [[nodiscard]] auto getId() const -> std::string;
};

/**
 * @brief Result of cron validation
 */
struct CronValidationResult {
    bool valid;
    std::string message;
};

/**
 * @brief Manages a collection of Cron jobs.
 */
class CronManager {
public:
    CronManager();
    ~CronManager();

    /**
     * @brief Adds a new Cron job.
     * @param job The CronJob object to be added.
     * @return True if the job was added successfully, false otherwise.
     */
    auto createCronJob(const CronJob& job) -> bool;

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
    auto updateCronJob(const std::string& oldCommand,
                       const CronJob& newJob) -> bool;

    /**
     * @brief Updates a Cron job by its unique identifier.
     * @param id The unique identifier of the job.
     * @param newJob The new CronJob object to replace the old one.
     * @return True if the job was updated successfully, false otherwise.
     */
    auto updateCronJobById(const std::string& id,
                           const CronJob& newJob) -> bool;

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

private:
    std::vector<CronJob> jobs_;  ///< List of Cron jobs.
    std::unordered_map<std::string, size_t>
        jobIndex_;  ///< Index for faster job lookup

    /**
     * @brief Refreshes the job index map for fast lookups.
     */
    void refreshJobIndex();

    /**
     * @brief Validates a CronJob object.
     * @param job The CronJob to validate.
     * @return True if the job is valid, false otherwise.
     */
    auto validateJob(const CronJob& job) -> bool;
};

#endif  // CRONJOB_H