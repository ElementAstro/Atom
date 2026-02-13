/*
 * cron_manager_batch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_MANAGER_BATCH_HPP
#define CRON_MANAGER_BATCH_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cron_job.hpp"

// Forward declaration
class CronManager;

/**
 * @brief Batch and I/O operations for cron job management.
 *
 * Provides batch creation/deletion/update, import/export to JSON,
 * export to system crontab, and clear-all operations. Operates on
 * shared data structures owned by CronManager and delegates single-job
 * operations back to CronManager.
 */
class CronManagerBatch {
public:
    /**
     * @brief Constructs a batch helper bound to a CronManager.
     * @param manager Reference to the owning CronManager.
     * @param jobs Reference to the job storage map.
     * @param jobs_mutex Reference to the shared mutex for jobs.
     */
    CronManagerBatch(
        CronManager& manager,
        std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs,
        std::shared_mutex& jobs_mutex);

    /**
     * @brief Batch creation of multiple Cron jobs.
     * @param jobs Vector of CronJob objects to create.
     * @return Number of jobs successfully created.
     */
    auto batchCreateJobs(const std::vector<CronJob>& jobs) -> int;

    /**
     * @brief Enhanced batch creation with move semantics.
     * @param jobs Vector of CronJob objects to create (moved).
     * @return Number of jobs successfully created.
     */
    auto batchCreateJobsOptimized(std::vector<CronJob> jobs) -> size_t;

    /**
     * @brief Batch deletion of multiple Cron jobs.
     * @param commands Vector of commands identifying jobs to delete.
     * @return Number of jobs successfully deleted.
     */
    auto batchDeleteJobs(const std::vector<std::string>& commands) -> int;

    /**
     * @brief Batch update of multiple Cron jobs.
     * @param updates Vector of (id, new_job) pairs.
     * @return Number of jobs successfully updated.
     */
    auto batchUpdateJobs(
        const std::vector<std::pair<std::string, CronJob>>& updates)
        -> size_t;

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
     * @brief Exports enabled Cron jobs to the system crontab.
     * @return True if the export was successful, false otherwise.
     */
    auto exportToCrontab() -> bool;

    /**
     * @brief Clears all cron jobs in memory and from system crontab.
     * @return True if all jobs were cleared successfully, false otherwise.
     */
    auto clearAllJobs() -> bool;

private:
    CronManager& manager_;
    std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs_;
    std::shared_mutex& jobs_mutex_;

    /**
     * @brief Helper to collect jobs as a vector for export operations.
     */
    auto collectJobVector() -> std::vector<CronJob>;
};

#endif  // CRON_MANAGER_BATCH_HPP
