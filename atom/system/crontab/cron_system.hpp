#ifndef CRON_SYSTEM_HPP
#define CRON_SYSTEM_HPP

#include <string>
#include <vector>
#include "cron_job.hpp"

/**
 * @brief Handles system-level cron operations
 */
class CronSystem {
public:
    /**
     * @brief Adds a job to the system crontab.
     * @param job The job to add.
     * @return True if successful, false otherwise.
     */
    static auto addJobToSystem(const CronJob& job) -> bool;

    /**
     * @brief Removes a job from the system crontab.
     * @param command The command of the job to remove.
     * @return True if successful, false otherwise.
     */
    static auto removeJobFromSystem(const std::string& command) -> bool;

    /**
     * @brief Lists all jobs from the system crontab.
     * @return Vector of jobs from system crontab.
     */
    static auto listSystemJobs() -> std::vector<CronJob>;

    /**
     * @brief Exports enabled jobs to the system crontab.
     * @param jobs Vector of jobs to export.
     * @return True if successful, false otherwise.
     */
    static auto exportJobsToSystem(const std::vector<CronJob>& jobs) -> bool;

    /**
     * @brief Clears all jobs from the system crontab.
     * @return True if successful, false otherwise.
     */
    static auto clearSystemJobs() -> bool;

private:
    /**
     * @brief Parses a crontab line into a CronJob.
     * @param line The crontab line to parse.
     * @return CronJob if parsing successful, empty job otherwise.
     */
    static auto parseCrontabLine(const std::string& line) -> CronJob;
};

#endif // CRON_SYSTEM_HPP
