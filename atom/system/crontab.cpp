#include "crontab.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <memory>

#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;

auto CronJob::toJson() const -> json {
    return json{{"time", time_}, {"command", command_}, {"enabled", enabled_}};
}

auto CronJob::fromJson(const json& jsonObj) -> CronJob {
    return CronJob{jsonObj.at("time").get<std::string>(),
                   jsonObj.at("command").get<std::string>(),
                   jsonObj.at("enabled").get<bool>()};
}

auto CronManager::createCronJob(const CronJob& job) -> bool {
    LOG_F(INFO, "Creating Cron job: {} {}", job.time_, job.command_);
    std::string command = "crontab -l 2>/dev/null | { cat; echo \"" +
                          job.time_ + " " + job.command_ + "\"; } | crontab -";
    if (atom::system::executeCommandWithStatus(command).second == 0) {
        jobs_.push_back(job);
        LOG_F(INFO, "Cron job created successfully.");
        return true;
    }
    LOG_F(ERROR, "Failed to create Cron job.");
    return false;
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
            LOG_F(INFO, "Cron job deleted successfully.");
            return true;
        }
    }
    LOG_F(ERROR, "Failed to delete Cron job.");
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
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            std::string time = line.substr(0, spacePos);
            std::string command = line.substr(spacePos + 1);
            // Remove potential trailing newline
            command.erase(std::remove(command.begin(), command.end(), '\n'),
                          command.end());
            currentJobs.emplace_back(time, command, true);
        }
    }
    LOG_F(INFO, "Retrieved %zu Cron jobs.", currentJobs.size());
    return currentJobs;
}

auto CronManager::exportToJSON(const std::string& filename) -> bool {
    LOG_F(INFO, "Exporting Cron jobs to JSON file: {}", filename);
    json jsonObj;
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

    json jsonObj;
    file >> jsonObj;

    for (const auto& jobJson : jsonObj) {
        CronJob job = CronJob::fromJson(jobJson);
        if (createCronJob(job)) {
            LOG_F(INFO, "Imported Cron job: {}", job.command_);
        } else {
            LOG_F(WARNING, "Failed to import Cron job: {}", job.command_);
        }
    }
    return true;
}

auto CronManager::updateCronJob(const std::string& oldCommand,
                                const CronJob& newJob) -> bool {
    LOG_F(INFO, "Updating Cron job. Old command: {}, New command: {}",
          oldCommand, newJob.command_);
    if (deleteCronJob(oldCommand)) {
        return createCronJob(newJob);
    }
    LOG_F(ERROR, "Failed to update Cron job.");
    return false;
}

auto CronManager::viewCronJob(const std::string& command) -> CronJob {
    LOG_F(INFO, "Viewing Cron job with command: {}", command);
    auto iterator = std::find_if(
        jobs_.begin(), jobs_.end(),
        [&](const CronJob& job) { return job.command_ == command; });
    if (iterator != jobs_.end()) {
        LOG_F(INFO, "Cron job found.");
        return *iterator;
    }
    LOG_F(WARNING, "Cron job not found.");
    return CronJob{"", "", false};  // 返回一个空的任务
}

auto CronManager::searchCronJobs(const std::string& query)
    -> std::vector<CronJob> {
    LOG_F(INFO, "Searching Cron jobs with query: {}", query);
    std::vector<CronJob> foundJobs;
    for (const auto& job : jobs_) {
        if (job.command_.find(query) != std::string::npos ||
            job.time_.find(query) != std::string::npos) {
            foundJobs.push_back(job);
        }
    }
    LOG_F(INFO, "Found %zu matching Cron jobs.", foundJobs.size());
    return foundJobs;
}

auto CronManager::statistics() -> int {
    LOG_F(INFO, "Getting statistics. Total Cron jobs: %zu", jobs_.size());
    return static_cast<int>(jobs_.size());
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

auto CronManager::exportToCrontab() -> bool {
    LOG_F(INFO, "Exporting enabled Cron jobs to crontab.");
    std::string cmd = "crontab -r";
    if (atom::system::executeCommandWithStatus(cmd).second != 0) {
        LOG_F(ERROR, "Failed to clear existing crontab.");
        return false;
    }

    std::string newCrontab;
    for (const auto& job : jobs_) {
        if (job.enabled_) {
            newCrontab += job.time_ + " " + job.command_ + "\n";
        }
    }

    std::ofstream tmpCrontab("/tmp/new_crontab");
    if (!tmpCrontab.is_open()) {
        LOG_F(ERROR, "Failed to open temporary crontab file.");
        return false;
    }
    tmpCrontab << newCrontab;
    tmpCrontab.close();

    std::string loadCmd = "crontab /tmp/new_crontab";
    if (atom::system::executeCommandWithStatus(loadCmd).second == 0) {
        LOG_F(INFO, "Crontab updated successfully.");
        return true;
    }
    LOG_F(ERROR, "Failed to load new crontab.");
    return false;
}