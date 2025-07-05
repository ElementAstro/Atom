#ifndef CRON_JOB_HPP
#define CRON_JOB_HPP

#include <chrono>
#include <string>
#include <vector>
#include "atom/type/json_fwd.hpp"

/**
 * @brief Represents a Cron job with a scheduled time and command.
 */
struct alignas(64) CronJob {
public:
    std::string time_;
    std::string command_;
    bool enabled_;
    std::string category_;
    std::string description_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point last_run_;
    int run_count_;
    int priority_;
    int max_retries_;
    int current_retries_;
    bool one_time_;
    std::vector<std::pair<std::chrono::system_clock::time_point, bool>>
        execution_history_;

    /**
     * @brief Constructs a new CronJob object.
     * @param time Scheduled time for the Cron job
     * @param command Command to be executed by the Cron job
     * @param enabled Status of the Cron job
     * @param category Category of the Cron job for organization
     * @param description Description of what the job does
     */
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
          run_count_(0),
          priority_(5),
          max_retries_(0),
          current_retries_(0),
          one_time_(false) {
        execution_history_.reserve(100);
    }

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

    /**
     * @brief Records an execution result in the job's history.
     * @param success Whether the execution was successful.
     */
    void recordExecution(bool success);
};

#endif // CRON_JOB_HPP
