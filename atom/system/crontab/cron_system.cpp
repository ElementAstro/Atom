#include "cron_system.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>

#include "atom/system/command/executor.hpp"
#include "spdlog/spdlog.h"

auto CronSystem::addJobToSystem(const CronJob& job) -> bool {
    if (!job.enabled_) {
        return true; // No need to add disabled jobs to system
    }

    const std::string command = "crontab -l 2>/dev/null | { cat; echo \"" +
                                job.time_ + " " + job.command_ +
                                "\"; } | crontab -";
    return atom::system::executeCommandWithStatus(command).second == 0;
}

auto CronSystem::removeJobFromSystem(const std::string& command) -> bool {
    const std::string cmd =
        "crontab -l | grep -v \" " + command + "\" | crontab -";
    return atom::system::executeCommandWithStatus(cmd).second == 0;
}

auto CronSystem::listSystemJobs() -> std::vector<CronJob> {
    spdlog::info("Listing all system Cron jobs");
    std::vector<CronJob> currentJobs;

    const std::string cmd = "crontab -l";
    std::array<char, 128> buffer;

    using pclose_t = int (*)(FILE*);
    std::unique_ptr<FILE, pclose_t> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to list system Cron jobs");
        return currentJobs;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        CronJob job = parseCrontabLine(line);
        if (!job.time_.empty() && !job.command_.empty()) {
            currentJobs.push_back(std::move(job));
        }
    }

    spdlog::info("Retrieved {} system Cron jobs", currentJobs.size());
    return currentJobs;
}

auto CronSystem::exportJobsToSystem(const std::vector<CronJob>& jobs) -> bool {
    spdlog::info("Exporting enabled Cron jobs to system crontab");

    const std::string tmpFilename =
        "/tmp/new_crontab_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());

    std::ofstream tmpCrontab(tmpFilename);
    if (!tmpCrontab.is_open()) {
        spdlog::error("Failed to open temporary crontab file");
        return false;
    }

    for (const auto& job : jobs) {
        if (job.enabled_) {
            tmpCrontab << job.time_ << " " << job.command_ << "\n";
        }
    }
    tmpCrontab.close();

    const std::string loadCmd = "crontab " + tmpFilename;
    const bool success =
        atom::system::executeCommandWithStatus(loadCmd).second == 0;

    std::remove(tmpFilename.c_str());

    if (success) {
        const int enabledCount = static_cast<int>(
            std::count_if(jobs.begin(), jobs.end(),
                          [](const CronJob& j) { return j.enabled_; }));
        spdlog::info("System crontab updated successfully with {} enabled jobs",
                     enabledCount);
        return true;
    }

    spdlog::error("Failed to load new crontab to system");
    return false;
}

auto CronSystem::clearSystemJobs() -> bool {
    spdlog::info("Clearing all system cron jobs");

    const std::string cmd = "crontab -r";
    if (atom::system::executeCommandWithStatus(cmd).second != 0) {
        spdlog::error("Failed to clear system crontab");
        return false;
    }

    spdlog::info("All system cron jobs cleared successfully");
    return true;
}

auto CronSystem::parseCrontabLine(const std::string& line) -> CronJob {
    size_t spaceCount = 0;
    size_t lastFieldPos = 0;
    for (size_t i = 0; i < line.length() && spaceCount < 5; ++i) {
        if (line[i] == ' ') {
            ++spaceCount;
            if (spaceCount == 5) {
                lastFieldPos = i;
                break;
            }
        }
    }

    if (spaceCount == 5 && lastFieldPos < line.length()) {
        const std::string time = line.substr(0, lastFieldPos);
        const std::string command = line.substr(lastFieldPos + 1);
        return CronJob(time, command, true);
    }

    return CronJob{}; // Return empty job if parsing fails
}
