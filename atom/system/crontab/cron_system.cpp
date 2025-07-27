#include "cron_system.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>


#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <sys/utsname.h>
#else
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

#include "atom/system/command/executor.hpp"
#include "spdlog/spdlog.h"

// Static member definitions
SystemPlatform CronSystem::platform_ = SystemPlatform::UNKNOWN;
CronServiceType CronSystem::service_type_ = CronServiceType::CRONTAB;
std::atomic<bool> CronSystem::initialized_{false};
std::atomic<bool> CronSystem::monitoring_enabled_{false};
std::mutex CronSystem::monitor_mutex_;
std::unordered_map<std::string, JobMonitorInfo> CronSystem::monitored_jobs_;
std::thread CronSystem::monitor_thread_;

auto CronSystem::initialize() -> bool {
    if (initialized_.load()) {
        return true;
    }

    platform_ = detectPlatform();

    // Set preferred service type based on platform
    switch (platform_) {
        case SystemPlatform::LINUX:
            service_type_ = isServiceAvailable(CronServiceType::SYSTEMD) ?
                           CronServiceType::SYSTEMD : CronServiceType::CRONTAB;
            break;
        case SystemPlatform::WINDOWS:
            service_type_ = CronServiceType::WINDOWS_TASK;
            break;
        case SystemPlatform::MACOS:
            service_type_ = CronServiceType::LAUNCHD;
            break;
        default:
            service_type_ = CronServiceType::CRONTAB;
            break;
    }

    // Validate that the selected service is available
    if (!validateCronService()) {
        spdlog::error("Selected cron service is not available");
        return false;
    }

    initialized_.store(true);
    spdlog::info("CronSystem initialized for platform: {}, service: {}",
                static_cast<int>(platform_), static_cast<int>(service_type_));

    return true;
}

void CronSystem::shutdown() {
    if (!initialized_.load()) {
        return;
    }

    monitoring_enabled_.store(false);

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
        monitored_jobs_.clear();
    }

    initialized_.store(false);
    spdlog::info("CronSystem shutdown completed");
}

auto CronSystem::getPlatform() -> SystemPlatform {
    return platform_;
}

auto CronSystem::getPreferredServiceType() -> CronServiceType {
    return service_type_;
}

auto CronSystem::setServiceType(CronServiceType service_type) -> bool {
    if (!isServiceAvailable(service_type)) {
        spdlog::error("Service type {} is not available on this platform",
                     static_cast<int>(service_type));
        return false;
    }

    service_type_ = service_type;
    spdlog::info("CronSystem service type changed to: {}", static_cast<int>(service_type));
    return true;
}

auto CronSystem::detectPlatform() -> SystemPlatform {
#ifdef _WIN32
    return SystemPlatform::WINDOWS;
#elif __APPLE__
    return SystemPlatform::MACOS;
#elif __linux__
    return SystemPlatform::LINUX;
#else
    return SystemPlatform::UNKNOWN;
#endif
}

auto CronSystem::addJobToSystem(const CronJob& job) -> SystemResult {
    auto start_time = std::chrono::steady_clock::now();
    SystemResult result;

    if (!job.isEnabled()) {
        result.success = true;
        result.message = "Job is disabled, not added to system";
        return result;
    }

    try {
        switch (service_type_) {
            case CronServiceType::CRONTAB: {
                const std::string command = "crontab -l 2>/dev/null | { cat; echo \"" +
                                          formatJobForCrontab(job) + "\"; } | crontab -";
                auto [output, exit_code] = atom::system::executeCommandWithStatus(command);
                result.success = (exit_code == 0);
                result.exit_code = exit_code;
                result.stdout_output = output;
                result.message = result.success ? "Job added successfully" : "Failed to add job";
                break;
            }
            case CronServiceType::SYSTEMD:
                result = createSystemdTimer(job);
                break;
            case CronServiceType::WINDOWS_TASK:
                result = createWindowsTask(job);
                break;
            case CronServiceType::LAUNCHD:
                result = createLaunchdJob(job);
                break;
            default:
                result.success = false;
                result.message = "Unsupported service type";
                break;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Exception occurred: " + std::string(e.what());
    }

    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);

    return result;
}

auto CronSystem::removeJobFromSystem(const std::string& command) -> SystemResult {
    auto start_time = std::chrono::steady_clock::now();
    SystemResult result;

    try {
        const std::string cmd = "crontab -l | grep -v \" " + command + "\" | crontab -";
        auto [output, exit_code] = atom::system::executeCommandWithStatus(cmd);

        result.success = (exit_code == 0);
        result.exit_code = exit_code;
        result.stdout_output = output;
        result.message = result.success ? "Job removed successfully" : "Failed to remove job";

    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Exception occurred: " + std::string(e.what());
    }

    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);

    return result;
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

auto CronSystem::exportJobsToSystem(const std::vector<CronJob>& jobs) -> SystemResult {
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

    SystemResult result;
    int enabledCount = 0;

    for (const auto& job : jobs) {
        if (job.isEnabled()) {
            tmpCrontab << formatJobForCrontab(job) << "\n";
            enabledCount++;
        }
    }
    tmpCrontab.close();

    const std::string loadCmd = "crontab " + tmpFilename;
    auto [output, exit_code] = atom::system::executeCommandWithStatus(loadCmd);

    std::remove(tmpFilename.c_str());

    result.success = (exit_code == 0);
    result.exit_code = exit_code;
    result.stdout_output = output;

    if (result.success) {
        result.message = "System crontab updated successfully with " +
                        std::to_string(enabledCount) + " enabled jobs";
        spdlog::info(result.message);
    } else {
        result.message = "Failed to load new crontab to system";
        spdlog::error(result.message);
    }

    return result;
}

auto CronSystem::clearSystemJobs() -> SystemResult {
    SystemResult result;
    spdlog::info("Clearing all system cron jobs");

    try {
        const std::string cmd = "crontab -r";
        auto [output, exit_code] = atom::system::executeCommandWithStatus(cmd);

        result.success = (exit_code == 0);
        result.exit_code = exit_code;
        result.stdout_output = output;

        if (result.success) {
            result.message = "All system cron jobs cleared successfully";
            spdlog::info(result.message);
        } else {
            result.message = "Failed to clear system crontab";
            spdlog::error(result.message);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Exception occurred: " + std::string(e.what());
    }

    return result;
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

auto CronSystem::formatJobForCrontab(const CronJob& job) -> std::string {
    return job.time_ + " " + job.command_;
}

auto CronSystem::formatJobForSystemd([[maybe_unused]] const CronJob& job) -> std::string {
    // TODO: Implement systemd timer format conversion
    return formatJobForCrontab(job);
}

auto CronSystem::formatJobForWindows([[maybe_unused]] const CronJob& job) -> std::string {
    // TODO: Implement Windows Task Scheduler format conversion
    return formatJobForCrontab(job);
}

auto CronSystem::formatJobForLaunchd([[maybe_unused]] const CronJob& job) -> std::string {
    // TODO: Implement launchd format conversion
    return formatJobForCrontab(job);
}

auto CronSystem::validateCronService() -> bool {
    return isServiceAvailable(service_type_);
}

auto CronSystem::isServiceAvailable(CronServiceType service) -> bool {
    switch (service) {
        case CronServiceType::CRONTAB: {
            // Check if crontab command is available
            auto result = atom::system::executeCommandWithStatus("which crontab");
            return result.second == 0;
        }
        case CronServiceType::SYSTEMD: {
            // Check if systemctl is available and systemd is running
            auto result = atom::system::executeCommandWithStatus("systemctl --version");
            return result.second == 0;
        }
        case CronServiceType::WINDOWS_TASK: {
            // Check if schtasks is available (Windows)
            auto result = atom::system::executeCommandWithStatus("schtasks /?");
            return result.second == 0;
        }
        case CronServiceType::LAUNCHD: {
            // Check if launchctl is available (macOS)
            auto result = atom::system::executeCommandWithStatus("launchctl version");
            return result.second == 0;
        }
        default:
            return false;
    }
}

// Placeholder implementations for advanced features
auto CronSystem::createSystemdTimer([[maybe_unused]] const CronJob& job) -> SystemResult {
    SystemResult result;
    result.success = false;
    result.message = "systemd timer creation not yet implemented";
    spdlog::warn(result.message);
    return result;
}

auto CronSystem::createWindowsTask([[maybe_unused]] const CronJob& job) -> SystemResult {
    SystemResult result;
    result.success = false;
    result.message = "Windows Task Scheduler integration not yet implemented";
    spdlog::warn(result.message);
    return result;
}

auto CronSystem::createLaunchdJob([[maybe_unused]] const CronJob& job) -> SystemResult {
    SystemResult result;
    result.success = false;
    result.message = "launchd integration not yet implemented";
    spdlog::warn(result.message);
    return result;
}

auto CronSystem::getCronServiceStatus() -> SystemResult {
    SystemResult result;

    switch (service_type_) {
        case CronServiceType::CRONTAB: {
            auto [output, exit_code] = atom::system::executeCommandWithStatus("crontab -l");
            result.success = (exit_code == 0);
            result.exit_code = exit_code;
            result.stdout_output = output;
            result.message = result.success ? "Crontab service is available" : "Crontab service unavailable";
            break;
        }
        case CronServiceType::SYSTEMD: {
            auto [output, exit_code] = atom::system::executeCommandWithStatus("systemctl status cron");
            result.success = (exit_code == 0);
            result.exit_code = exit_code;
            result.stdout_output = output;
            result.message = result.success ? "systemd cron service is running" : "systemd cron service not running";
            break;
        }
        default:
            result.success = false;
            result.message = "Service status check not implemented for current service type";
            break;
    }

    return result;
}

auto CronSystem::restartCronService() -> SystemResult {
    SystemResult result;
    result.success = false;
    result.message = "Cron service restart not yet implemented";
    spdlog::warn(result.message);
    return result;
}
