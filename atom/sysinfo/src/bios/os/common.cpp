/**
 * @file common.cpp
 * @brief Enhanced common utilities implementation for OS information module
 *
 * This file contains the implementation of enhanced common utilities and
 * helper functions used across different platform implementations.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "common.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::system {

auto parseFile(const std::string& filePath) -> std::pair<std::string, std::string> {
    spdlog::debug("Parsing file: {}", filePath);
    std::ifstream file(filePath);
    std::string osName, osVersion;

    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filePath);
        return {"", ""};
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("NAME=") == 0) {
            osName = line.substr(5);
            osName.erase(std::remove(osName.begin(), osName.end(), '"'), osName.end());
        } else if (line.find("VERSION=") == 0) {
            osVersion = line.substr(8);
            osVersion.erase(std::remove(osVersion.begin(), osVersion.end(), '"'), osVersion.end());
        } else if (line.find("DISTRIB_ID=") == 0) {
            osName = line.substr(11);
        } else if (line.find("DISTRIB_RELEASE=") == 0) {
            osVersion = line.substr(16);
        }
    }

    spdlog::info("Parsed OS info - Name: {}, Version: {}", osName, osVersion);
    return {osName, osVersion};
}

auto detectWSL() -> std::optional<std::string> {
    spdlog::debug("Detecting WSL environment");

#ifdef __linux__
    std::ifstream procVersion("/proc/version");
    if (procVersion.is_open()) {
        std::string line;
        std::getline(procVersion, line);
        procVersion.close();

        if (line.find("microsoft") != std::string::npos ||
            line.find("WSL") != std::string::npos) {

            // Try to determine WSL version
            std::ifstream wslConf("/proc/sys/kernel/osrelease");
            if (wslConf.is_open()) {
                std::string osrelease;
                std::getline(wslConf, osrelease);
                wslConf.close();

                if (osrelease.find("WSL2") != std::string::npos) {
                    spdlog::info("Detected WSL2 environment");
                    return "WSL2";
                } else {
                    spdlog::info("Detected WSL1 environment");
                    return "WSL1";
                }
            }

            spdlog::info("Detected WSL environment (version unknown)");
            return "WSL";
        }
    }
#endif

    return std::nullopt;
}

auto executeCommand(const std::string& command) -> std::string {
    spdlog::debug("Executing command: {}", command);

    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    spdlog::debug("Command output: {}", result);
    return result;
}

auto executeCommandLines(const std::string& command) -> std::vector<std::string> {
    std::string output = executeCommand(command);
    std::vector<std::string> lines;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    return lines;
}

auto osTypeToString(OSType type) -> std::string {
    switch (type) {
        case OSType::WINDOWS: return "Windows";
        case OSType::LINUX: return "Linux";
        case OSType::MACOS: return "macOS";
        case OSType::FREEBSD: return "FreeBSD";
        case OSType::ANDROID: return "Android";
        case OSType::IOS: return "iOS";
        case OSType::UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

auto osArchitectureToString(OSArchitecture arch) -> std::string {
    switch (arch) {
        case OSArchitecture::X86: return "x86";
        case OSArchitecture::X64: return "x64";
        case OSArchitecture::ARM: return "ARM";
        case OSArchitecture::ARM64: return "ARM64";
        case OSArchitecture::MIPS: return "MIPS";
        case OSArchitecture::POWERPC: return "PowerPC";
        case OSArchitecture::SPARC: return "SPARC";
        case OSArchitecture::UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

auto detectOSType() -> OSType {
#ifdef _WIN32
    return OSType::WINDOWS;
#elif defined(__APPLE__)
    #ifdef TARGET_OS_IPHONE
        return OSType::IOS;
    #else
        return OSType::MACOS;
    #endif
#elif defined(__ANDROID__)
    return OSType::ANDROID;
#elif defined(__linux__)
    return OSType::LINUX;
#elif defined(__FreeBSD__)
    return OSType::FREEBSD;
#else
    return OSType::UNKNOWN;
#endif
}

auto detectOSArchitecture() -> OSArchitecture {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return OSArchitecture::X64;
        case PROCESSOR_ARCHITECTURE_INTEL:
            return OSArchitecture::X86;
        case PROCESSOR_ARCHITECTURE_ARM:
            return OSArchitecture::ARM;
        case PROCESSOR_ARCHITECTURE_ARM64:
            return OSArchitecture::ARM64;
        default:
            return OSArchitecture::UNKNOWN;
    }
#elif defined(__linux__) || defined(__APPLE__)
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::string machine = unameData.machine;

        if (machine == "x86_64" || machine == "amd64") {
            return OSArchitecture::X64;
        } else if (machine == "i386" || machine == "i686") {
            return OSArchitecture::X86;
        } else if (machine.find("arm") == 0) {
            if (machine.find("64") != std::string::npos) {
                return OSArchitecture::ARM64;
            } else {
                return OSArchitecture::ARM;
            }
        } else if (machine.find("aarch64") == 0) {
            return OSArchitecture::ARM64;
        }
    }
#endif

    return OSArchitecture::UNKNOWN;
}

auto EnhancedOSInfo::toJson() const -> std::string {
    std::ostringstream json;
    json << "{\n";
    json << "  \"osName\": \"" << osName << "\",\n";
    json << "  \"osVersion\": \"" << osVersion << "\",\n";
    json << "  \"kernelVersion\": \"" << kernelVersion << "\",\n";
    json << "  \"architecture\": \"" << architecture << "\",\n";
    json << "  \"computerName\": \"" << computerName << "\",\n";
    json << "  \"osType\": \"" << osTypeToString(osType) << "\",\n";
    json << "  \"osArchitecture\": \"" << osArchitectureToString(osArch) << "\",\n";
    json << "  \"isServer\": " << (isServer ? "true" : "false") << ",\n";
    json << "  \"uptime\": " << uptime.count() << ",\n";
    json << "  \"totalMemoryBytes\": " << totalMemoryBytes << ",\n";
    json << "  \"availableMemoryBytes\": " << availableMemoryBytes << "\n";
    json << "}";
    return json.str();
}

auto EnhancedOSInfo::toDetailedString() const -> std::string {
    std::ostringstream details;
    details << "Enhanced Operating System Information:\n";
    details << "=====================================\n";
    details << "OS Name: " << osName << "\n";
    details << "OS Version: " << osVersion << "\n";
    details << "Kernel Version: " << kernelVersion << "\n";
    details << "Architecture: " << architecture << "\n";
    details << "Computer Name: " << computerName << "\n";
    details << "OS Type: " << osTypeToString(osType) << "\n";
    details << "OS Architecture: " << osArchitectureToString(osArch) << "\n";
    details << "Server Edition: " << (isServer ? "Yes" : "No") << "\n";
    details << "Boot Time: " << bootTime << "\n";
    details << "Time Zone: " << timeZone << "\n";
    details << "Character Set: " << charSet << "\n";
    details << "Uptime: " << uptime.count() << " seconds\n";
    details << "Total Memory: " << totalMemoryBytes << " bytes\n";
    details << "Available Memory: " << availableMemoryBytes << " bytes\n";

    if (!buildNumber.empty()) {
        details << "Build Number: " << buildNumber << "\n";
    }

    if (!edition.empty()) {
        details << "Edition: " << edition << "\n";
    }

    return details.str();
}

auto checkForUpdates() -> std::vector<std::string> {
    spdlog::debug("Checking for available updates");
    std::vector<std::string> updates;

#ifdef _WIN32
    // Windows Update check would require WUA API
    spdlog::info("Windows update check requires WUA API implementation");
#elif defined(__linux__)
    // Check for apt updates
    auto aptOutput = executeCommandLines("apt list --upgradable 2>/dev/null");
    for (const auto& line : aptOutput) {
        if (line.find("/") != std::string::npos && line != "Listing...") {
            updates.push_back(line);
        }
    }

    // If apt is not available, try yum/dnf
    if (updates.empty()) {
        auto yumOutput = executeCommandLines("yum check-update 2>/dev/null");
        for (const auto& line : yumOutput) {
            if (!line.empty() && line.find(".") != std::string::npos) {
                updates.push_back(line);
            }
        }
    }
#elif defined(__APPLE__)
    auto brewOutput = executeCommandLines("brew outdated 2>/dev/null");
    updates = brewOutput;
#endif

    spdlog::info("Found {} available updates", updates.size());
    return updates;
}

auto getSystemPerformanceMetrics() -> SystemPerformanceMetrics {
    spdlog::debug("Getting system performance metrics");
    SystemPerformanceMetrics metrics;

#ifdef _WIN32
    // Windows performance metrics implementation
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        metrics.memoryUsagePercent = static_cast<double>(memStatus.dwMemoryLoad);
    }
#elif defined(__linux__)
    // Linux performance metrics implementation
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        uint64_t totalMem = 0, availableMem = 0;

        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                totalMem = std::stoull(line.substr(9));
            } else if (line.find("MemAvailable:") == 0) {
                availableMem = std::stoull(line.substr(13));
            }
        }

        if (totalMem > 0) {
            metrics.memoryUsagePercent =
                static_cast<double>(totalMem - availableMem) / totalMem * 100.0;
        }
    }

    // Get process count
    auto processCount = executeCommand("ps aux | wc -l");
    if (!processCount.empty()) {
        metrics.processCount = std::stoull(processCount);
    }
#elif defined(__APPLE__)
    // macOS performance metrics implementation
    size_t size = sizeof(uint64_t);
    uint64_t memsize;
    if (sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0) == 0) {
        // Get memory pressure
        auto vmStat = executeCommand("vm_stat");
        // Parse vm_stat output for memory usage
    }
#endif

    return metrics;
}

auto getSystemSecurityInfo() -> SystemSecurityInfo {
    spdlog::debug("Getting system security information");
    SystemSecurityInfo secInfo;

#ifdef _WIN32
    // Windows security information
    secInfo.firewallEnabled = true; // Placeholder - requires Windows Firewall API
    secInfo.secureBootEnabled = false; // Requires UEFI API
#elif defined(__linux__)
    // Linux security information
    auto firewallStatus = executeCommand("ufw status 2>/dev/null");
    secInfo.firewallEnabled = firewallStatus.find("active") != std::string::npos;

    // Check for SELinux
    auto selinuxStatus = executeCommand("getenforce 2>/dev/null");
    if (!selinuxStatus.empty()) {
        secInfo.securityFeatures.push_back("SELinux: " + selinuxStatus);
    }

    // Check for AppArmor
    auto apparmorStatus = executeCommand("aa-status 2>/dev/null");
    if (!apparmorStatus.empty()) {
        secInfo.securityFeatures.push_back("AppArmor");
    }
#elif defined(__APPLE__)
    // macOS security information
    secInfo.firewallEnabled = true; // macOS has built-in firewall
    secInfo.securityFeatures.push_back("System Integrity Protection");
    secInfo.securityFeatures.push_back("Gatekeeper");
    secInfo.securityFeatures.push_back("XProtect");
#endif

    return secInfo;
}

auto getNetworkConfiguration() -> NetworkConfiguration {
    spdlog::debug("Getting network configuration");
    NetworkConfiguration netConfig;

#ifdef _WIN32
    // Windows network configuration
    // Implementation would use GetAdaptersInfo API
#elif defined(__linux__)
    // Linux network configuration
    auto ipOutput = executeCommandLines("ip route show default");
    for (const auto& line : ipOutput) {
        if (line.find("dev") != std::string::npos) {
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (token == "dev") {
                    iss >> netConfig.primaryInterface;
                    break;
                }
            }
        }
    }

    if (!netConfig.primaryInterface.empty()) {
        auto ifconfigOutput = executeCommand("ip addr show " + netConfig.primaryInterface);
        // Parse IP address from output
        std::regex ipRegex(R"(inet (\d+\.\d+\.\d+\.\d+))");
        std::smatch match;
        if (std::regex_search(ifconfigOutput, match, ipRegex)) {
            netConfig.ipAddress = match[1].str();
        }
    }
#elif defined(__APPLE__)
    // macOS network configuration
    auto routeOutput = executeCommand("route get default");
    // Parse default route information
#endif

    return netConfig;
}

auto getSystemEnvironment() -> SystemEnvironment {
    spdlog::debug("Getting system environment");
    SystemEnvironment sysEnv;

    // Get environment variables
    extern char **environ;
    for (char **env = environ; *env != nullptr; env++) {
        std::string envVar = *env;
        auto pos = envVar.find('=');
        if (pos != std::string::npos) {
            std::string key = envVar.substr(0, pos);
            std::string value = envVar.substr(pos + 1);
            sysEnv.environmentVariables[key] = value;
        }
    }

    // Get system paths
    auto pathVar = sysEnv.environmentVariables.find("PATH");
    if (pathVar != sysEnv.environmentVariables.end()) {
        std::istringstream pathStream(pathVar->second);
        std::string path;

#ifdef _WIN32
        char delimiter = ';';
#else
        char delimiter = ':';
#endif

        while (std::getline(pathStream, path, delimiter)) {
            if (!path.empty()) {
                sysEnv.systemPath.push_back(path);
            }
        }
    }

    return sysEnv;
}

auto validateSystemIntegrity() -> bool {
    spdlog::debug("Validating system integrity");

#ifdef _WIN32
    // Windows system file checker
    auto sfcResult = executeCommand("sfc /verifyonly");
    return sfcResult.find("did not find any integrity violations") != std::string::npos;
#elif defined(__linux__)
    // Linux package verification
    auto dpkgResult = executeCommand("dpkg --verify 2>/dev/null");
    return dpkgResult.empty(); // No output means no issues
#elif defined(__APPLE__)
    // macOS system verification
    auto result = executeCommand("system_profiler SPSoftwareDataType | grep 'System Integrity'");
    return result.find("Enabled") != std::string::npos;
#endif

    return false;
}

auto getInstalledSoftware() -> std::vector<std::string> {
    spdlog::debug("Getting installed software list");
    std::vector<std::string> software;

#ifdef _WIN32
    // Windows installed programs (requires registry access)
    spdlog::info("Windows software enumeration requires registry access");
#elif defined(__linux__)
    // Linux package list
    auto dpkgOutput = executeCommandLines("dpkg -l | grep '^ii'");
    for (const auto& line : dpkgOutput) {
        std::istringstream iss(line);
        std::string status, name;
        iss >> status >> name;
        if (!name.empty()) {
            software.push_back(name);
        }
    }

    // If dpkg is not available, try rpm
    if (software.empty()) {
        auto rpmOutput = executeCommandLines("rpm -qa");
        software = rpmOutput;
    }
#elif defined(__APPLE__)
    // macOS applications
    auto brewOutput = executeCommandLines("brew list 2>/dev/null");
    software = brewOutput;

    // Add system applications
    auto appsOutput = executeCommandLines("ls /Applications");
    for (const auto& app : appsOutput) {
        software.push_back(app);
    }
#endif

    spdlog::info("Found {} installed software packages", software.size());
    return software;
}

auto EnhancedOSInfo::refresh() -> bool {
    spdlog::debug("Refreshing enhanced OS information");

    try {
        // Update performance metrics
        performance = getSystemPerformanceMetrics();

        // Update security information
        security = getSystemSecurityInfo();

        // Update network configuration
        network = getNetworkConfiguration();

        // Update environment information
        environment = getSystemEnvironment();

        // Update available updates
        availableUpdates = checkForUpdates();

        // Update installed software
        installedSoftware = getInstalledSoftware();

        // Update timestamp
        lastRefresh = std::chrono::system_clock::now();

        spdlog::info("Successfully refreshed enhanced OS information");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh OS information: {}", e.what());
        return false;
    }
}

}  // namespace atom::system
