#ifndef CRONJOB_H
#define CRONJOB_H

#include <string>
#include "atom/type/json_fwd.hpp"

/**
 * @brief Represents a Cron job with a scheduled time and command.
 */
struct alignas(64) CronJob {
public:
    std::string time_;     ///< Scheduled time for the Cron job.
    std::string command_;  ///< Command to be executed by the Cron job.
    bool enabled_;         ///< Status of the Cron job.

    CronJob(const std::string& time = "", const std::string& command = "", bool enabled = true)
        : time_(time), command_(command), enabled_(enabled) {}

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
};

/**
 * @brief Manages a collection of Cron jobs.
 */
class CronManager {
public:
    /**
     * @brief Adds a new Cron job.
     * @param job The CronJob object to be added.
     * @return True if the job was added successfully, false otherwise.
     */
    auto createCronJob(const CronJob& job) -> bool;

    /**
     * @brief Deletes a Cron job with the specified command.
     * @param command The command of the Cron job to be deleted.
     * @return True if the job was deleted successfully, false otherwise.
     */
    auto deleteCronJob(const std::string& command) -> bool;

    /**
     * @brief Lists all current Cron jobs.
     * @return A vector of all current CronJob objects.
     */
    auto listCronJobs() -> std::vector<CronJob>;

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
     * @brief Views the details of a Cron job with the specified command.
     * @param command The command of the Cron job to view.
     * @return The CronJob object with the specified command.
     */
    auto viewCronJob(const std::string& command) -> CronJob;

    /**
     * @brief Searches for Cron jobs that match the specified query.
     * @param query The query string to search for.
     * @return A vector of CronJob objects that match the query.
     */
    auto searchCronJobs(const std::string& query) -> std::vector<CronJob>;

    /**
     * @brief Gets statistics about the current Cron jobs.
     * @return An integer representing the number of Cron jobs.
     */
    auto statistics() -> int;

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

    auto exportToCrontab() -> bool;

private:
    std::vector<CronJob> jobs_;  ///< List of Cron jobs.
};

#endif  // CRONJOB_H