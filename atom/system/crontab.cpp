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

#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;

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

auto CronJob::getId() const -> std::string {
    return time_ + "_" + command_;
}

auto CronJob::toJson() const -> json {
    std::string createdAtStr = timePointToString(created_at_);
    std::string lastRunStr = "";
    if (last_run_ != std::chrono::system_clock::time_point()) {
        lastRunStr = timePointToString(last_run_);
    }

    return json{{"time", time_},
                {"command", command_},
                {"enabled", enabled_},
                {"category", category_},
                {"description", description_},
                {"created_at", createdAtStr},
                {"last_run", lastRunStr},
                {"run_count", run_count_}};
}

auto CronJob::fromJson(const json& jsonObj) -> CronJob {
    CronJob job;
    job.time_ = jsonObj.at("time").get<std::string>();
    job.command_ = jsonObj.at("command").get<std::string>();
    job.enabled_ = jsonObj.at("enabled").get<bool>();

    // 处理可选字段
    if (jsonObj.contains("category")) {
        job.category_ = jsonObj.at("category").get<std::string>();
    } else {
        job.category_ = "default";
    }

    if (jsonObj.contains("description")) {
        job.description_ = jsonObj.at("description").get<std::string>();
    }

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

    if (jsonObj.contains("run_count")) {
        job.run_count_ = jsonObj.at("run_count").get<int>();
    } else {
        job.run_count_ = 0;
    }

    return job;
}

CronManager::CronManager() {
    // 初始化时从系统读取当前cron任务
    jobs_ = listCronJobs();
    refreshJobIndex();
}

CronManager::~CronManager() {
    // 析构函数中确保所有更改已同步至系统
    exportToCrontab();
}

void CronManager::refreshJobIndex() {
    jobIndex_.clear();
    for (size_t i = 0; i < jobs_.size(); ++i) {
        jobIndex_[jobs_[i].getId()] = i;
    }
}

auto CronManager::validateJob(const CronJob& job) -> bool {
    // 验证任务的有效性
    if (job.time_.empty() || job.command_.empty()) {
        LOG_F(ERROR, "Invalid job: time or command is empty");
        return false;
    }

    return validateCronExpression(job.time_).valid;
}

auto CronManager::validateCronExpression(const std::string& cronExpr)
    -> CronValidationResult {
    // 简单的cron表达式验证逻辑
    std::regex cronRegex(R"(^(\S+\s+){4}\S+$)");
    if (!std::regex_match(cronExpr, cronRegex)) {
        return {false, "Invalid cron expression format. Expected 5 fields."};
    }

    std::stringstream ss(cronExpr);
    std::string minute, hour, dayOfMonth, month, dayOfWeek;
    ss >> minute >> hour >> dayOfMonth >> month >> dayOfWeek;

    // 检查每个字段的有效性（这里只是一个简单实现，可以扩展）
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

    // 可以继续添加其他字段的验证...

    return {true, "Valid cron expression"};
}

auto CronManager::createCronJob(const CronJob& job) -> bool {
    LOG_F(INFO, "Creating Cron job: {} {}", job.time_, job.command_);

    if (!validateJob(job)) {
        LOG_F(ERROR, "Invalid cron job");
        return false;
    }

    // 检查重复
    for (const auto& existingJob : jobs_) {
        if (existingJob.command_ == job.command_ &&
            existingJob.time_ == job.time_) {
            LOG_F(WARNING, "Duplicate cron job");
            return false;
        }
    }

    // 只有启用的任务才添加到系统crontab
    if (job.enabled_) {
        std::string command = "crontab -l 2>/dev/null | { cat; echo \"" +
                              job.time_ + " " + job.command_ +
                              "\"; } | crontab -";
        if (atom::system::executeCommandWithStatus(command).second != 0) {
            LOG_F(ERROR, "Failed to add job to system crontab");
            return false;
        }
    }

    // 添加到内存中的任务列表
    jobs_.push_back(job);
    refreshJobIndex();

    LOG_F(INFO, "Cron job created successfully.");
    return true;
}

auto CronManager::deleteCronJob(const std::string& command) -> bool {
    LOG_F(INFO, "Deleting Cron job with command: {}", command);
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
            LOG_F(INFO, "Cron job deleted successfully.");
            return true;
        }
    }
    LOG_F(ERROR, "Failed to delete Cron job.");
    return false;
}

auto CronManager::deleteCronJobById(const std::string& id) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return deleteCronJob(jobs_[it->second].command_);
    }
    LOG_F(ERROR, "Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::listCronJobs() -> std::vector<CronJob> {
    LOG_F(INFO, "Listing all Cron jobs.");
    std::vector<CronJob> currentJobs;
    std::string cmd = "crontab -l";
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!pipe) {
        LOG_F(ERROR, "Failed to list Cron jobs.");
        return currentJobs;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        // 去除行尾的换行符
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        // 查找第一个空格的位置
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            // 查找第4个空格的位置（cron格式有5个字段，因此需要找到第5个空格）
            size_t lastFieldPos = line.find(' ', spacePos + 1);
            for (int i = 0; i < 3 && lastFieldPos != std::string::npos; ++i) {
                lastFieldPos = line.find(' ', lastFieldPos + 1);
            }

            if (lastFieldPos != std::string::npos) {
                std::string time = line.substr(0, lastFieldPos + 1);
                std::string command = line.substr(lastFieldPos + 1);

                // 尝试从现有记录中查找此命令
                auto it = std::find_if(jobs_.begin(), jobs_.end(),
                                       [&command](const CronJob& job) {
                                           return job.command_ == command;
                                       });

                if (it != jobs_.end()) {
                    // 如果找到，更新时间并添加
                    CronJob existingJob = *it;
                    existingJob.time_ = time;
                    existingJob.enabled_ = true;
                    currentJobs.push_back(existingJob);
                } else {
                    // 否则创建新任务
                    currentJobs.emplace_back(time, command, true);
                }
            }
        }
    }

    LOG_F(INFO, "Retrieved %zu Cron jobs.", currentJobs.size());
    return currentJobs;
}

auto CronManager::listCronJobsByCategory(const std::string& category)
    -> std::vector<CronJob> {
    LOG_F(INFO, "Listing Cron jobs in category: {}", category);
    std::vector<CronJob> filteredJobs;

    std::copy_if(
        jobs_.begin(), jobs_.end(), std::back_inserter(filteredJobs),
        [&category](const CronJob& job) { return job.category_ == category; });

    LOG_F(INFO, "Found %zu jobs in category %s", filteredJobs.size(),
          category.c_str());
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
    LOG_F(INFO, "Exporting Cron jobs to JSON file: {}", filename);
    json jsonObj = json::array();
    for (const auto& job : jobs_) {
        jsonObj.push_back(job.toJson());
    }

    std::ofstream file(filename);
    if (file.is_open()) {
        file << jsonObj.dump(4);
        LOG_F(INFO, "Exported Cron jobs to {} successfully.", filename);
        return true;
    }
    LOG_F(ERROR, "Failed to open file: {}", filename);
    return false;
}

auto CronManager::importFromJSON(const std::string& filename) -> bool {
    LOG_F(INFO, "Importing Cron jobs from JSON file: {}", filename);
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open file: {}", filename);
        return false;
    }

    try {
        json jsonObj;
        file >> jsonObj;

        int successCount = 0;
        for (const auto& jobJson : jsonObj) {
            CronJob job = CronJob::fromJson(jobJson);
            if (createCronJob(job)) {
                LOG_F(INFO, "Imported Cron job: {}", job.command_);
                successCount++;
            } else {
                LOG_F(WARNING, "Failed to import Cron job: {}", job.command_);
            }
        }
        LOG_F(INFO, "Successfully imported %d of %zu jobs", successCount,
              jsonObj.size());
        return successCount > 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error parsing JSON file: %s", e.what());
        return false;
    }
}

auto CronManager::updateCronJob(const std::string& oldCommand,
                                const CronJob& newJob) -> bool {
    LOG_F(INFO, "Updating Cron job. Old command: {}, New command: {}",
          oldCommand, newJob.command_);

    if (!validateJob(newJob)) {
        LOG_F(ERROR, "Invalid new job");
        return false;
    }

    if (deleteCronJob(oldCommand)) {
        return createCronJob(newJob);
    }
    LOG_F(ERROR, "Failed to update Cron job.");
    return false;
}

auto CronManager::updateCronJobById(const std::string& id,
                                    const CronJob& newJob) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return updateCronJob(jobs_[it->second].command_, newJob);
    }
    LOG_F(ERROR, "Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::viewCronJob(const std::string& command) -> CronJob {
    LOG_F(INFO, "Viewing Cron job with command: {}", command);
    auto it = std::find_if(jobs_.begin(), jobs_.end(), [&](const CronJob& job) {
        return job.command_ == command;
    });

    if (it != jobs_.end()) {
        LOG_F(INFO, "Cron job found.");
        return *it;
    }
    LOG_F(WARNING, "Cron job not found.");
    return CronJob{"", "", false};  // 返回一个空的任务
}

auto CronManager::viewCronJobById(const std::string& id) -> CronJob {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        return jobs_[it->second];
    }
    LOG_F(WARNING, "Cron job with ID %s not found.", id.c_str());
    return CronJob{"", "", false};
}

auto CronManager::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    LOG_F(INFO, "Searching Cron jobs with query: {}", query);
    std::vector<CronJob> foundJobs;

    for (const auto& job : jobs_) {
        if (job.command_.find(query) != std::string::npos ||
            job.time_.find(query) != std::string::npos ||
            job.category_.find(query) != std::string::npos ||
            job.description_.find(query) != std::string::npos) {
            foundJobs.push_back(job);
        }
    }

    LOG_F(INFO, "Found %zu matching Cron jobs.", foundJobs.size());
    return foundJobs;
}

auto CronManager::statistics() -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;

    stats["total"] = static_cast<int>(jobs_.size());

    // 统计启用和禁用的任务数量
    stats["enabled"] = 0;
    stats["disabled"] = 0;
    for (const auto& job : jobs_) {
        if (job.enabled_) {
            stats["enabled"]++;
        } else {
            stats["disabled"]++;
        }
    }

    // 统计每个分类的任务数量
    std::unordered_map<std::string, int> categoryStats;
    for (const auto& job : jobs_) {
        categoryStats[job.category_]++;
    }

    for (const auto& [category, count] : categoryStats) {
        stats["category_" + category] = count;
    }

    // 统计执行次数
    stats["total_executions"] = 0;
    for (const auto& job : jobs_) {
        stats["total_executions"] += job.run_count_;
    }

    LOG_F(INFO,
          "Generated statistics. Total jobs: %d, enabled: %d, disabled: %d",
          stats["total"], stats["enabled"], stats["disabled"]);

    return stats;
}

auto CronManager::enableCronJob(const std::string& command) -> bool {
    LOG_F(INFO, "Enabling Cron job with command: {}", command);
    for (auto& job : jobs_) {
        if (job.command_ == command) {
            job.enabled_ = true;
            // Update crontab
            return exportToCrontab();
        }
    }
    LOG_F(ERROR, "Cron job not found.");
    return false;
}

auto CronManager::disableCronJob(const std::string& command) -> bool {
    LOG_F(INFO, "Disabling Cron job with command: {}", command);
    for (auto& job : jobs_) {
        if (job.command_ == command) {
            job.enabled_ = false;
            // Update crontab
            return exportToCrontab();
        }
    }
    LOG_F(ERROR, "Cron job not found.");
    return false;
}

auto CronManager::setJobEnabledById(const std::string& id,
                                    bool enabled) -> bool {
    auto it = jobIndex_.find(id);
    if (it != jobIndex_.end()) {
        jobs_[it->second].enabled_ = enabled;
        return exportToCrontab();
    }
    LOG_F(ERROR, "Failed to find job with ID: {}", id);
    return false;
}

auto CronManager::enableCronJobsByCategory(const std::string& category) -> int {
    LOG_F(INFO, "Enabling all cron jobs in category: {}", category);
    int count = 0;

    for (auto& job : jobs_) {
        if (job.category_ == category && !job.enabled_) {
            job.enabled_ = true;
            count++;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            LOG_F(INFO, "Enabled %d jobs in category %s", count,
                  category.c_str());
        } else {
            LOG_F(ERROR, "Failed to update crontab after enabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::disableCronJobsByCategory(const std::string& category)
    -> int {
    LOG_F(INFO, "Disabling all cron jobs in category: {}", category);
    int count = 0;

    for (auto& job : jobs_) {
        if (job.category_ == category && job.enabled_) {
            job.enabled_ = false;
            count++;
        }
    }

    if (count > 0) {
        if (exportToCrontab()) {
            LOG_F(INFO, "Disabled %d jobs in category %s", count,
                  category.c_str());
        } else {
            LOG_F(ERROR, "Failed to update crontab after disabling jobs");
            return 0;
        }
    }

    return count;
}

auto CronManager::exportToCrontab() -> bool {
    LOG_F(INFO, "Exporting enabled Cron jobs to crontab.");

    // 创建临时文件
    std::string tmpFilename =
        "/tmp/new_crontab_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
    std::ofstream tmpCrontab(tmpFilename);
    if (!tmpCrontab.is_open()) {
        LOG_F(ERROR, "Failed to open temporary crontab file.");
        return false;
    }

    // 写入所有启用的任务
    for (const auto& job : jobs_) {
        if (job.enabled_) {
            tmpCrontab << job.time_ << " " << job.command_ << "\n";
        }
    }
    tmpCrontab.close();

    // 导入到系统crontab
    std::string loadCmd = "crontab " + tmpFilename;
    if (atom::system::executeCommandWithStatus(loadCmd).second == 0) {
        LOG_F(INFO, "Crontab updated successfully with %d enabled jobs.",
              static_cast<int>(
                  std::count_if(jobs_.begin(), jobs_.end(),
                                [](const CronJob& j) { return j.enabled_; })));

        // 删除临时文件
        std::remove(tmpFilename.c_str());
        return true;
    }

    LOG_F(ERROR, "Failed to load new crontab.");
    // 删除临时文件
    std::remove(tmpFilename.c_str());
    return false;
}

auto CronManager::batchCreateJobs(const std::vector<CronJob>& jobs) -> int {
    LOG_F(INFO, "Batch creating %zu cron jobs", jobs.size());
    int successCount = 0;

    for (const auto& job : jobs) {
        if (createCronJob(job)) {
            successCount++;
        }
    }

    LOG_F(INFO, "Successfully created %d of %zu jobs", successCount,
          jobs.size());
    return successCount;
}

auto CronManager::batchDeleteJobs(const std::vector<std::string>& commands)
    -> int {
    LOG_F(INFO, "Batch deleting %zu cron jobs", commands.size());
    int successCount = 0;

    for (const auto& command : commands) {
        if (deleteCronJob(command)) {
            successCount++;
        }
    }

    LOG_F(INFO, "Successfully deleted %d of %zu jobs", successCount,
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
        LOG_F(INFO, "Recorded execution of job: %s (Run count: %d)",
              command.c_str(), it->run_count_);
        return true;
    }

    LOG_F(WARNING, "Tried to record execution for unknown job: %s",
          command.c_str());
    return false;
}

auto CronManager::clearAllJobs() -> bool {
    LOG_F(INFO, "Clearing all cron jobs");

    // 清除系统crontab
    std::string cmd = "crontab -r";
    if (atom::system::executeCommandWithStatus(cmd).second != 0) {
        LOG_F(ERROR, "Failed to clear system crontab");
        return false;
    }

    // 清除内存中的任务
    jobs_.clear();
    jobIndex_.clear();

    LOG_F(INFO, "All cron jobs cleared successfully");
    return true;
}