/*
 * cron_manager_query.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_MANAGER_QUERY_HPP
#define CRON_MANAGER_QUERY_HPP

#include <memory>
#include <optional>
#include <shared_mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "cron_job.hpp"
#include "cron_manager_types.hpp"

/**
 * @brief Query and search operations for cron jobs.
 *
 * Provides read-only operations for listing, filtering, searching,
 * and computing statistics over the job collection. Operates on
 * shared data structures owned by CronManager.
 */
class CronManagerQuery {
public:
    /**
     * @brief Constructs a query helper bound to external data.
     * @param jobs Reference to the job storage map.
     * @param command_to_id_index Reference to command-to-id index.
     * @param category_index Reference to category index.
     * @param status_index Reference to status index.
     * @param priority_index Reference to priority index.
     * @param job_stats Reference to job statistics map.
     * @param jobs_mutex Reference to the shared mutex for jobs.
     * @param stats_mutex Reference to the shared mutex for stats.
     */
    CronManagerQuery(
        const std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs,
        const std::unordered_map<std::string, std::string>&
            command_to_id_index,
        const std::unordered_map<std::string, std::vector<std::string>>&
            category_index,
        const std::unordered_map<JobStatus, std::set<std::string>>&
            status_index,
        const std::unordered_map<JobPriority, std::set<std::string>>&
            priority_index,
        const std::unordered_map<std::string, JobStats>& job_stats,
        std::shared_mutex& jobs_mutex, std::shared_mutex& stats_mutex);

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
     * @brief Get jobs sorted by priority.
     * @return Vector of jobs sorted by priority (highest first).
     */
    auto getJobsByPriority() -> std::vector<CronJob>;

    /**
     * @brief Get jobs filtered by category using index.
     * @param category The category to filter by.
     * @return Vector of shared_ptr to matching jobs.
     */
    auto getJobsByCategory(const std::string& category)
        -> std::vector<std::shared_ptr<CronJob>>;

    /**
     * @brief Get jobs filtered by status using index.
     * @param status The status to filter by.
     * @return Vector of shared_ptr to matching jobs.
     */
    auto getJobsByStatus(JobStatus status)
        -> std::vector<std::shared_ptr<CronJob>>;

    /**
     * @brief Get jobs filtered by priority range.
     * @param min_priority Minimum priority (inclusive).
     * @param max_priority Maximum priority (inclusive).
     * @return Vector of shared_ptr to matching jobs.
     */
    auto getJobsByPriorityRange(JobPriority min_priority,
                                JobPriority max_priority)
        -> std::vector<std::shared_ptr<CronJob>>;

    /**
     * @brief Get execution stats for a specific job.
     * @param job_id The unique identifier of the job.
     * @return Optional JobStats if the job exists.
     */
    auto getJobStats(const std::string& job_id) -> std::optional<JobStats>;

    /**
     * @brief Get aggregated stats across all jobs.
     * @return Aggregated JobStats.
     */
    auto getOverallStats() -> JobStats;

private:
    const std::unordered_map<std::string, std::shared_ptr<CronJob>>& jobs_;
    const std::unordered_map<std::string, std::string>& command_to_id_index_;
    const std::unordered_map<std::string, std::vector<std::string>>&
        category_index_;
    const std::unordered_map<JobStatus, std::set<std::string>>& status_index_;
    const std::unordered_map<JobPriority, std::set<std::string>>&
        priority_index_;
    const std::unordered_map<std::string, JobStats>& job_stats_;
    std::shared_mutex& jobs_mutex_;
    std::shared_mutex& stats_mutex_;
};

#endif  // CRON_MANAGER_QUERY_HPP
