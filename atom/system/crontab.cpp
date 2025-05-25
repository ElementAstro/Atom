#include "crontab.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <unordered_set>

#include "atom/system/command.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;

const std::unordered_map<std::string, std::string>
    CronManager::specialExpressions_ = {
        {"@yearly", "0 0 1 1 *"},  {"@annually", "0 0 1 1 *"},
        {"@monthly", "0 0 1 * *"}, {"@weekly", "0 0 * * 0"},
        {"@daily", "0 0 * * *"},   {"@midnight", "0 0 * * *"},
        {"@hourly", "0 * * * *"},  {"@reboot", "@reboot"}};

namespace {
auto timePointToString(const std::chrono::system_clock::time_point& timePoint)
    -> std::string {
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

auto stringToTimePoint(const std::string& timeStr)
    -> std::chrono::system_clock::time_point {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}
}  // namespace

auto CronJob::getId() const -> std::string { return time_ + "_" + command_; }

auto CronJob::toJson() const -> json {
    std::string createdAtStr = timePointToString(created_at_);
    std::string lastRunStr = "";
    if (last_run_ != std::chrono::system_clock::time_point()) {
        lastRunStr = timePointToString(last_run_);
    }

    json historyJson = json::array();
    for (const auto& entry : execution_history_) {
        historyJson.push_back({{"timestamp", timePointToString(entry.first)},
                               {"success", entry.second}});
    }

    return json{{"time", time_},
                {"command", command_},
                {"enabled", enabled_},
                {"category", category_},
                {"description", description_},
                {"created_at", createdAtStr},
                {"last_run", lastRunStr},
                {"run_count", run_count_},
                {"priority", priority_},
                {"max_retries", max_retries_},
                {"current_retries", current_retries_},
                {"one_time", one_time_},
                {"execution_history", historyJson}};
}

auto CronJob::fromJson(const json& jsonObj) -> CronJob {
    CronJob job;
    job.time_ = jsonObj.at("time").get<std::string>();
    job.command_ = jsonObj.at("command").get<std::string>();
    job.enabled_ = jsonObj.at("enabled").get<bool>();

    job.category_ = jsonObj.value("category", "default");
    job.description_ = jsonObj.value("description", "");

    if (jsonObj.contains("created_at") &&
        !jsonObj.at("created_at").get<std::string>().empty()) {
        job.created_at_ =
            stringToTimePoint(jsonObj.at("created_at").get<std::string>());
    } else {
        job.created_at_ = std::chrono::system_clock::now();
    }

    if (jsonObj.contains("last_run") &&
        !jsonObj.at("last_run").get<std::string>().empty()) {
        job.last_run_ =
            stringToTimePoint(jsonObj.at("last_run").get<std::string>());
    }

    job.run_count_ = jsonObj.value("run_count", 0);
    job.priority_ = jsonObj.value("priority", 5);
    job.max_retries_ = jsonObj.value("max_retries", 0);
    job.current_retries_ = jsonObj.value("current_retries", 0);
    job.one_time_ = jsonObj.value("one_time", false);

    if (jsonObj.contains("execution_history") &&
        jsonObj["execution_history"].is_array()) {
        for (const auto& entry : jsonObj["execution_history"]) {
            if (entry.contains("timestamp") && entry.contains("success")) {
                auto timestamp =
                    stringToTimePoint(entry["timestamp"].get<std::string>());
                bool success = entry["success"].get<bool>();
                job.execution_history_.push_back({timestamp, success});
            }
        }
    }

    return job;
}

void CronJob::recordExecution(bool success) {
    last_run_ = std::chrono::system_clock::now();
    run_count_++;
    execution_history_.push_back({last_run_, success});

    constexpr size_t MAX_HISTORY = 100;
    if (execution_history_.size() > MAX_HISTORY) {
        execution_history_.erase(execution_history_.begin(),
                                 execution_history_.begin() +
                                     (execution_history_.size() - MAX_HISTORY));
    }
}

CronManager::CronManager() {
    jobs_ = listCronJobs();
    refreshJobIndex();
}

CronManager::~CronManager() { exportToCrontab(); }

void CronManager::refreshJobIndex() {
    jobIndex_.clear();
    for (size_t i = 0; i < jobs_.size(); ++i) {
        jobIndex_[jobs_[i].getId()] = i;
    }
}

auto CronManager::validateJob(const CronJob& job) -> bool {
    if (job.time_.empty() || job.command_.empty()) {
        spdlog::error("Invalid job: time or command is empty");
        return false;
    }
    return validateCronExpression(job.time_).valid;
}

auto CronManager::validateCronExpression(const std::string& cronExpr)
    -> CronValidationResult {
    if (!cronExpr.empty() && cronExpr[0] == '@') {
        std::string converted = convertSpecialExpression(cronExpr);
        if (converted == cronExpr) {
            return {false, "Unknown special expression"};
        } else if (!converted.empty()) {
            if (converted == "@reboot") {
                return {true, "Valid special expression: reboot"};
            }
            return validateCronExpression(converted);
        }
    }

    std::regex cronRegex(R"(^(\S+\s+){4}\S+$)");
    if (!std::regex_match(cronExpr, cronRegex)) {
        return {false, "Invalid cron expression format. Expected 5 fields."};
    }

    std::stringstream ss(cronExpr);
    std::string minute, hour, dayOfMonth, month, dayOfWeek;
    ss >> minute >> hour >> dayOfMonth >> month >> dayOfWeek;

    std::regex minuteRegex(
        R"(^(\*|[0-5]?[0-9](-[0-5]?[0-9])?)(,(\*|[0-5]?[0-9](-[0-5]?[0-9])?))*$)");
    if (!std::regex_match(minute, minuteRegex)) {
        return {false, "Invalid minute field"};
    }

    std::regex hourRegex(
        R"(^(\*|[01]?[0-9]|2[0-3](-([01]?[0-9]|2[0-3]))?)(,(\*|[01]?[0-9]|2[0-3](-([01]?[0-9]|2[0-3]))?))*$)");
    if (!std::regex_match(hour, hourRegex)) {
        return {false, "Invalid hour field"};
    }

    return {true, "Valid cron expression"};
}

auto CronManager::convertSpecialExpression(const std::string& specialExpr)
    -> std::string {
    if (specialExpr.empty() || specialExpr[0] != '@') {
        return specialExpr;
    }

    auto it = specialExpressions_.find(specialExpr);
    if (it != specialExpressions_.end()) {
        return it->second;
    }

    spdlog::warn("Unknown special cron expression: {}", specialExpr);
    return "";
}

auto CronManager::createCronJob(const CronJob& job) -> bool {
    spdlog::info("Creating Cron job: {} {}", job.time_, job.command_);

    if (!validateJob(job)) {
        spdlog::error("Invalid cron job");
        return false;
    }

    for (const auto& existingJob : jobs_) {
        if (existingJob.command_ == job.command_ &&
            existingJob.time_ == job.time_) {
            spdlog::warn("Duplicate cron job");
            return false;
        }
    }

    if (job.enabled_) {
        std::string command = "crontab -l 2>/dev/null | { cat; echo \"" +
                              job.time_ + " " + job.command_ +
                              "\"; } | crontab -";
        if (atom::system::executeCommandWithStatus(command).second != 0) {
            spdlog::error("Failed to add job to system crontab");
            return false;
        }
    }

    jobs_.push_back(job);
    refreshJobIndex();

    spdlog::info("Cron job created successfully");
    return true;
}

auto CronManager::createJobWithSpecialTime(
    const std::string& specialTime, const std::string& command, bool enabled,
    const std::string& category, const std::string& description, int priority,
    int maxRetries, bool oneTime) -> bool {
    spdlog::info("Creating Cron job with special time: {} {}", specialTime,
                 command);

    std::string standardTime = convertSpecialExpression(specialTime);
    if (standardTime.empty()) {
        spdlog::error("Invalid special time expression: {}", specialTime);
        return false;
    }

    CronJob job(standardTime, command, enabled, category, description);
    job.priority_ = priority;
    job.max_retries_ = maxRetries;
    job.one_time_ = oneTime;

    return createCronJob(job);
}

auto CronManager::deleteCronJob(const std::string& command) -> bool {
    spdlog::info("Deleting Cron job with command: {}", command);
    std::string jobToDelete = " " + command;
    std::string cmd =
        "crontab -l | grep -v \"" + jobToDelete + "\" | crontab -";

    if (atom::system::executeCommandWithStatus(cmd).second == 0) {
        auto originalSize = jobs_.size();
        jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                                   [&](const CronJob& job) {
                                       return job.command_ == command;
                                   }),
                    jobs_.end());

        if (jobs_.size() < originalSize) {
            refreshJobIndex();
            spdlog::info("Cron job deleted successfully");
            return true;
        }
    }
    spdlog::error("Failed to delete Cron job");
    return false;
}

auto CronManager::deleteCronJobById(const std::string& id) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return deleteCronJob(jobs_[it->second].command_);
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::listCronJobs() -> std::vector<CronJob> {
    spdlog::info("Listing all Cron jobs");
    std::vector<CronJob> currentJobs;
    std::string cmd = "crontab -l";
    std::array<char, 128> buffer;

    using pclose_t = int (*)(FILE*);
    std::unique_ptr<FILE, pclose_t> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to list Cron jobs");
        return currentJobs;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            size_t lastFieldPos = line.find(' ', spacePos + 1);
            for (int i = 0; i < 3 && lastFieldPos != std::string::npos; ++i) {
                lastFieldPos = line.find(' ', lastFieldPos + 1);
            }

            if (lastFieldPos != std::string::npos) {
                std::string time = line.substr(0, lastFieldPos + 1);
                std::string command = line.substr(lastFieldPos + 1);

                auto it = std::find_if(jobs_.begin(), jobs_.end(),
                                       [&command](const CronJob& job) {
                                           return job.command_ == command;
                                       });

                if (it != jobs_.end()) {
                    CronJob existingJob = *it;
                    existingJob.time_ = time;
                    existingJob.enabled_ = true;
                    currentJobs.push_back(existingJob);
                } else {
                    currentJobs.emplace_back(time, command, true);
                }
            }
        }
    }

    spdlog::info("Retrieved {} Cron jobs", currentJobs.size());
    return currentJobs;
}

auto CronManager::listCronJobsByCategory(const std::string& category)
    -> std::vector<CronJob> {
    spdlog::info("Listing Cron jobs in category: {}", category);
    std::vector<CronJob> filteredJobs;

    std::copy_if(
        jobs_.begin(), jobs_.end(), std::back_inserter(filteredJobs),
        [&category](const CronJob& job) { return job.category_ == category; });

    spdlog::info("Found {} jobs in category {}", filteredJobs.size(), category);
    return filteredJobs;
}

auto CronManager::getCategories() -> std::vector<std::string> {
    std::unordered_set<std::string> categories;
    for (const auto& job : jobs_) {
        categories.insert(job.category_);
    }

    std::vector<std::string> result(categories.begin(), categories.end());
    std::sort(result.begin(), result.end());
    return result;
}

auto CronManager::exportToJSON(const std::string& filename) -> bool {
    spdlog::info("Exporting Cron jobs to JSON file: {}", filename);
    json jsonObj = json::array();
    for (const auto& job : jobs_) {
        jsonObj.push_back(job.toJson());
    }

    std::ofstream file(filename);
    if (file.is_open()) {
        file << jsonObj.dump(4);
        spdlog::info("Exported Cron jobs to {} successfully", filename);
        return true;
    }
    spdlog::error("Failed to open file: {}", filename);
    return false;
}

auto CronManager::importFromJSON(const std::string& filename) -> bool {
    spdlog::info("Importing Cron jobs from JSON file: {}", filename);
    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
        return false;
    }

    try {
        json jsonObj;
        file >> jsonObj;

        int successCount = 0;
        for (const auto& jobJson : jsonObj) {
            CronJob job = CronJob::fromJson(jobJson);
            if (createCronJob(job)) {
                spdlog::info("Imported Cron job: {}", job.command_);
                successCount++;
            } else {
                spdlog::warn("Failed to import Cron job: {}", job.command_);
            }
        }
        spdlog::info("Successfully imported {} of {} jobs", successCount,
                     jsonObj.size());
        return successCount > 0;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing JSON file: {}", e.what());
        return false;
    }
}

auto CronManager::updateCronJob(const std::string& oldCommand,
                                const CronJob& newJob) -> bool {
    spdlog::info("Updating Cron job. Old command: {}, New command: {}",
                 oldCommand, newJob.command_);

    if (!validateJob(newJob)) {
        spdlog::error("Invalid new job");
        return false;
    }

    if (deleteCronJob(oldCommand)) {
        return createCronJob(newJob);
    }
    spdlog::error("Failed to update Cron job");
    return false;
}

auto CronManager::updateCronJobById(const std::string& id,
                                    const CronJob& newJob) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return updateCronJob(jobs_[it->second].command_, newJob);
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::viewCronJob(const std::string& command) -> CronJob {
    spdlog::info("Viewing Cron job with command: {}", command);
    auto it = std::find_if(jobs_.begin(), jobs_.end(), [&](const CronJob& job) {
        return job.command_ == command;
    });

    if (it != jobs_.end()) {
        spdlog::info("Cron job found");
        return *it;
    }
    spdlog::warn("Cron job not found");
    return CronJob{"", "", false};
}

auto CronManager::viewCronJobById(const std::string& id) -> CronJob {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return jobs_[it->second];
    }
    spdlog::warn("Cron job with ID {} not found", id);
    return CronJob{"", "", false};
}

auto CronManager::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    spdlog::info("Searching Cron jobs with query: {}", query);
    std::vector<CronJob> foundJobs;

    for (const auto& job : jobs_) {
        if (job.command_.find(query) != std::string::npos ||
            job.time_.find(query) != std::string::npos ||
            job.category_.find(query) != std::string::npos ||
            job.description_.find(query) != std::string::npos) {
            foundJobs.push_back(job);
        }
    }

    spdlog::info("Found {} matching Cron jobs", foundJobs.size());
    return foundJobs;
}

auto CronManager::statistics() -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;

    stats["total"] = static_cast<int>(jobs_.size());
    stats["enabled"] = 0;
    stats["disabled"] = 0;

    for (const auto& job : jobs_) {
        if (job.enabled_) {
            stats["enabled"]++;
        } else {
            stats["disabled"]++;
        }
    }

    std::unordered_map<std::string, int> categoryStats;
    for (const auto& job : jobs_) {
        categoryStats[job.category_]++;
    }

    for (const auto& [category, count] : categoryStats) {
        stats["category_" + category] = count;
    }

    stats["total_executions"] = 0;
    for (const auto& job : jobs_) {
        stats["total_executions"] += job.run_count_;
    }

    spdlog::info(
        "Generated statistics. Total jobs: {}, enabled: {}, disabled: {}",
        stats["total"], stats["enabled"], stats["disabled"]);

    return stats;
}

auto CronManager::enableCronJob(const std::string& command) -> bool {
    spdlog::info("Enabling Cron job with command: {}", command);
    for (auto& job : jobs_) {
        if (job.command_ == command) {
            job.enabled_ = true;
            return exportToCrontab();
        }
    }
    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::disableCronJob(const std::string& command) -> bool {
    spdlog::info("Disabling Cron job with command: {}", command);
    for (auto& job : jobs_) {
        if (job.command_ == command) {
            job.enabled_ = false;
            return exportToCrontab();
        }
    }
    spdlog::error("Cron job not found");
    return false;
}

auto CronManager::setJobEnabledById(const std::string& id, bool enabled)
    -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].enabled_ = enabled;
        return exportToCrontab();
    }
    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::enableCronJobsByCategory(const std::string& category) -> int {
    spdlog::info("Enabling all cron jobs in category: {}", category);
    int count = 0;

    for (auto& job : jobs_) {
        if (job.category_ == category && !job.enabled_) {
            job.enabled_ = true;
            count++;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            spdlog::info("Enabled {} jobs in category {}", count, category);
        } else {
            spdlog::error("Failed to update crontab after enabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::disableCronJobsByCategory(const std::string& category)
    -> int {
    spdlog::info("Disabling all cron jobs in category: {}", category);
    int count = 0;

    for (auto& job : jobs_) {
        if (job.category_ == category && job.enabled_) {
            job.enabled_ = false;
            count++;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            spdlog::info("Disabled {} jobs in category {}", count, category);
        } else {
            spdlog::error("Failed to update crontab after disabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::exportToCrontab() -> bool {
    spdlog::info("Exporting enabled Cron jobs to crontab");

    std::string tmpFilename =
        "/tmp/new_crontab_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
    std::ofstream tmpCrontab(tmpFilename);
    if (!tmpCrontab.is_open()) {
        spdlog::error("Failed to open temporary crontab file");
        return false;
    }

    for (const auto& job : jobs_) {
        if (job.enabled_) {
            tmpCrontab << job.time_ << " " << job.command_ << "\n";
        }
    }
    tmpCrontab.close();

    std::string loadCmd = "crontab " + tmpFilename;
    if (atom::system::executeCommandWithStatus(loadCmd).second == 0) {
        spdlog::info("Crontab updated successfully with {} enabled jobs",
                     static_cast<int>(std::count_if(
                         jobs_.begin(), jobs_.end(),
                         [](const CronJob& j) { return j.enabled_; })));
        std::remove(tmpFilename.c_str());
        return true;
    }

    spdlog::error("Failed to load new crontab");
    std::remove(tmpFilename.c_str());
    return false;
}

auto CronManager::batchCreateJobs(const std::vector<CronJob>& jobs) -> int {
    spdlog::info("Batch creating {} cron jobs", jobs.size());
    int successCount = 0;

    for (const auto& job : jobs) {
        if (createCronJob(job)) {
            successCount++;
        }
    }

    spdlog::info("Successfully created {} of {} jobs", successCount,
                 jobs.size());
    return successCount;
}

auto CronManager::batchDeleteJobs(const std::vector<std::string>& commands)
    -> int {
    spdlog::info("Batch deleting {} cron jobs", commands.size());
    int successCount = 0;

    for (const auto& command : commands) {
        if (deleteCronJob(command)) {
            successCount++;
        }
    }

    spdlog::info("Successfully deleted {} of {} jobs", successCount,
                 commands.size());
    return successCount;
}

auto CronManager::recordJobExecution(const std::string& command) -> bool {
    auto it = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&command](const CronJob& job) { return job.command_ == command; });

    if (it != jobs_.end()) {
        it->last_run_ = std::chrono::system_clock::now();
        it->run_count_++;
        it->recordExecution(true);

        if (it->one_time_) {
            std::string jobId = it->getId();
            spdlog::info("One-time job completed, removing: {}", jobId);
            deleteCronJobById(jobId);
            return true;
        }

        spdlog::info("Recorded execution of job: {} (Run count: {})", command,
                     it->run_count_);
        return true;
    }

    spdlog::warn("Tried to record execution for unknown job: {}", command);
    return false;
}

auto CronManager::clearAllJobs() -> bool {
    spdlog::info("Clearing all cron jobs");

    std::string cmd = "crontab -r";
    if (atom::system::executeCommandWithStatus(cmd).second != 0) {
        spdlog::error("Failed to clear system crontab");
        return false;
    }

    jobs_.clear();
    jobIndex_.clear();

    spdlog::info("All cron jobs cleared successfully");
    return true;
}

auto CronManager::setJobPriority(const std::string& id, int priority) -> bool {
    if (priority < 1 || priority > 10) {
        spdlog::error("Invalid priority value {}. Must be between 1-10",
                      priority);
        return false;
    }

    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].priority_ = priority;
        spdlog::info("Set priority to {} for job: {}", priority, id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::setJobMaxRetries(const std::string& id, int maxRetries)
    -> bool {
    if (maxRetries < 0) {
        spdlog::error("Invalid max retries value {}. Must be non-negative",
                      maxRetries);
        return false;
    }

    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].max_retries_ = maxRetries;
        if (jobs_[it->second].current_retries_ > maxRetries) {
            jobs_[it->second].current_retries_ = 0;
        }
        spdlog::info("Set max retries to {} for job: {}", maxRetries, id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::setJobOneTime(const std::string& id, bool oneTime) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].one_time_ = oneTime;
        spdlog::info("Set one-time status to {} for job: {}",
                     oneTime ? "true" : "false", id);
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::getJobExecutionHistory(const std::string& id)
    -> std::vector<std::pair<std::chrono::system_clock::time_point, bool>> {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return jobs_[it->second].execution_history_;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return {};
}

auto CronManager::recordJobExecutionResult(const std::string& id, bool success)
    -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        CronJob& job = jobs_[it->second];
        job.recordExecution(success);

        if (success && job.one_time_) {
            spdlog::info("One-time job completed successfully, removing: {}",
                         id);
            return deleteCronJobById(id);
        } else if (!success) {
            return handleJobFailure(id);
        }

        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::handleJobFailure(const std::string& id) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        CronJob& job = jobs_[it->second];

        if (job.max_retries_ > 0 && job.current_retries_ < job.max_retries_) {
            job.current_retries_++;
            spdlog::info("Job failed, scheduling retry {}/{} for: {}",
                         job.current_retries_, job.max_retries_, id);
            return true;
        } else if (job.current_retries_ >= job.max_retries_ &&
                   job.max_retries_ > 0) {
            spdlog::warn("Job failed after {} retries, no more retries for: {}",
                         job.max_retries_, id);
        }
        return true;
    }

    spdlog::error("Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::getJobsByPriority() -> std::vector<CronJob> {
    std::vector<CronJob> sortedJobs = jobs_;

    std::sort(sortedJobs.begin(), sortedJobs.end(),
              [](const CronJob& a, const CronJob& b) {
                  return a.priority_ < b.priority_;
              });

    return sortedJobs;
}