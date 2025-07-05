#ifndef CRON_STORAGE_HPP
#define CRON_STORAGE_HPP

#include <string>
#include <vector>
#include "cron_job.hpp"

/**
 * @brief Handles JSON import/export for cron jobs
 */
class CronStorage {
public:
    /**
     * @brief Exports cron jobs to a JSON file.
     * @param jobs Vector of jobs to export.
     * @param filename The name of the file to export to.
     * @return True if the export was successful, false otherwise.
     */
    static auto exportToJSON(const std::vector<CronJob>& jobs,
                            const std::string& filename) -> bool;

    /**
     * @brief Imports cron jobs from a JSON file.
     * @param filename The name of the file to import from.
     * @return Vector of imported jobs, empty if failed.
     */
    static auto importFromJSON(const std::string& filename) -> std::vector<CronJob>;
};

#endif // CRON_STORAGE_HPP
