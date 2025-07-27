#include "linux.hpp"

#if defined(__linux__) || defined(__linux)

#include <array>
#include <fstream>

#include <sys/sysinfo.h>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include "common.hpp"
#include "os.hpp"

namespace atom::system {

auto getComputerNameLinux() -> std::optional<std::string> {
    spdlog::debug("Retrieving computer name on Linux");
    constexpr size_t bufferSize = 256;
    std::array<char, bufferSize> buffer;
    
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        spdlog::info("Successfully retrieved computer name: {}", buffer.data());
        return std::string(buffer.data());
    } else {
        spdlog::error("Failed to get computer name on Linux");
        return std::nullopt;
    }
}

void getOperatingSystemInfoLinux(OperatingSystemInfo& osInfo) {
    spdlog::debug("Using Linux API for OS information");
    auto osReleaseInfo = parseFile("/etc/os-release");
    if (!osReleaseInfo.first.empty()) {
        osInfo.osName = osReleaseInfo.first;
        osInfo.osVersion = osReleaseInfo.second;
    } else {
        auto lsbReleaseInfo = parseFile("/etc/lsb-release");
        if (!lsbReleaseInfo.first.empty()) {
            osInfo.osName = lsbReleaseInfo.first;
            osInfo.osVersion = lsbReleaseInfo.second;
        } else {
            std::ifstream redhatReleaseFile("/etc/redhat-release");
            if (redhatReleaseFile.is_open()) {
                std::string line;
                std::getline(redhatReleaseFile, line);
                osInfo.osName = line;
                redhatReleaseFile.close();
                spdlog::info("Retrieved OS info from /etc/redhat-release: {}",
                             line);
            }
        }
    }

    if (osInfo.osName.empty()) {
        spdlog::error("Failed to get OS name on Linux");
    }

    std::ifstream kernelVersionFile("/proc/version");
    if (kernelVersionFile.is_open()) {
        std::string line;
        std::getline(kernelVersionFile, line);
        osInfo.kernelVersion = line.substr(0, line.find(" "));
        kernelVersionFile.close();
        spdlog::info("Retrieved kernel version: {}", osInfo.kernelVersion);
    } else {
        spdlog::error("Failed to open /proc/version");
    }
}

auto getSystemUptimeLinux() -> std::chrono::seconds {
    spdlog::debug("Getting system uptime on Linux");
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return std::chrono::seconds(si.uptime);
    }
    return std::chrono::seconds(0);
}

auto getSystemTimeZoneLinux() -> std::string {
    spdlog::debug("Getting system timezone on Linux");
    std::ifstream tz("/etc/timezone");
    if (tz.is_open()) {
        std::string timezone;
        std::getline(tz, timezone);
        return timezone;
    }
    return "Unknown";
}

auto getInstalledUpdatesLinux() -> std::vector<std::string> {
    spdlog::debug("Getting installed updates on Linux");
    std::vector<std::string> updates;
    
    std::ifstream log("/var/log/dpkg.log");
    if (log.is_open()) {
        std::string line;
        while (std::getline(log, line)) {
            if (line.find("install") != std::string::npos) {
                updates.push_back(line);
            }
        }
    }
    
    spdlog::info("Found {} installed updates on Linux", updates.size());
    return updates;
}

auto getSystemLanguageLinux() -> std::string {
    spdlog::debug("Getting system language on Linux");
    const char* lang = getenv("LANG");
    if (lang) {
        return std::string(lang);
    }
    return "Unknown";
}

auto getSystemEncodingLinux() -> std::string {
    spdlog::debug("Getting system encoding on Linux");
    const char* encoding = getenv("LC_CTYPE");
    if (encoding) {
        return std::string(encoding);
    }
    return "UTF-8";
}

}  // namespace atom::system

#endif  // defined(__linux__) || defined(__linux)
