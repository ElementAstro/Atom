/**
 * @file linux.cpp
 * @brief Enhanced Linux-specific OS information implementation
 *
 * This file contains the implementation of enhanced Linux-specific functions
 * for retrieving comprehensive operating system information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "linux.hpp"

#if defined(__linux__) || defined(__linux)

#include <algorithm>
#include <array>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace atom::system {

auto LinuxOSImplementation::getEnhancedOSInfo() -> EnhancedOSInfo {
    spdlog::debug("Getting enhanced Linux OS information");
    EnhancedOSInfo info;

    // Get basic information
    auto distInfo = getDistributionInfo();
    info.osName = distInfo.name;
    info.osVersion = distInfo.version;
    info.osType = OSType::LINUX;
    info.osArch = detectOSArchitecture();

    // Get kernel information
    auto kernelInfo = getKernelInfo();
    info.kernelVersion = kernelInfo.version;

    // Get computer name
    std::array<char, 256> hostname;
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        info.computerName = hostname.data();
    }

    // Get architecture
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        info.architecture = unameData.machine;
    }

    // Get uptime
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.uptime = std::chrono::seconds(si.uptime);
        info.totalMemoryBytes = si.totalram * si.mem_unit;
        info.availableMemoryBytes = si.freeram * si.mem_unit;
    }

    // Get performance metrics
    info.performance = getPerformanceMetrics();

    // Get security information
    info.security = getSecurityInfo();

    // Get network configuration
    info.network = getNetworkConfig();

    // Get environment
    info.environment = getSystemEnvironment();

    // Get container information
    auto containerInfo = getContainerInfo();
    if (containerInfo.isContainer) {
        info.environment.environmentVariables["CONTAINER_TYPE"] = containerInfo.containerType;
        info.environment.environmentVariables["CONTAINER_RUNTIME"] = containerInfo.containerRuntime;
    }

    // Set timestamps
    info.lastRefresh = std::chrono::system_clock::now();

    spdlog::info("Successfully retrieved enhanced Linux OS information");
    return info;
}

auto LinuxOSImplementation::getDistributionInfo() -> LinuxDistributionInfo {
    return parseOSRelease();
}

auto LinuxOSImplementation::getKernelInfo() -> LinuxKernelInfo {
    return parseKernelVersion();
}

auto LinuxOSImplementation::getSystemServices() -> LinuxSystemServices {
    spdlog::debug("Getting Linux system services information");
    LinuxSystemServices services;

    services.initSystem = detectInitSystem();

    if (services.initSystem == "systemd") {
        // Get systemd services
        auto activeServices = executeCommandLines("systemctl list-units --type=service --state=active --no-pager --no-legend");
        for (const auto& line : activeServices) {
            std::istringstream iss(line);
            std::string serviceName;
            iss >> serviceName;
            if (!serviceName.empty()) {
                services.activeServices.push_back(serviceName);
            }
        }

        auto failedServices = executeCommandLines("systemctl list-units --type=service --state=failed --no-pager --no-legend");
        for (const auto& line : failedServices) {
            std::istringstream iss(line);
            std::string serviceName;
            iss >> serviceName;
            if (!serviceName.empty()) {
                services.failedServices.push_back(serviceName);
            }
        }
    }

    return services;
}

auto LinuxOSImplementation::getPackageInfo() -> LinuxPackageInfo {
    spdlog::debug("Getting Linux package information");
    LinuxPackageInfo packageInfo;

    packageInfo.packageManager = detectPackageManager();

    if (packageInfo.packageManager == "apt") {
        // Get installed packages
        auto installedPackages = executeCommandLines("dpkg -l | grep '^ii' | awk '{print $2}'");
        packageInfo.installedPackages = installedPackages;
        packageInfo.totalPackages = installedPackages.size();

        // Get available updates
        auto availableUpdates = executeCommandLines("apt list --upgradable 2>/dev/null | grep -v 'Listing...'");
        packageInfo.availableUpdates = availableUpdates;
        packageInfo.upgradablePackages = availableUpdates.size();

        // Get security updates
        auto securityUpdates = executeCommandLines("apt list --upgradable 2>/dev/null | grep -i security");
        packageInfo.securityUpdates = securityUpdates;

        // Get repositories
        auto repositories = executeCommandLines("grep -h '^deb ' /etc/apt/sources.list /etc/apt/sources.list.d/*.list 2>/dev/null");
        packageInfo.repositories = repositories;
    } else if (packageInfo.packageManager == "yum" || packageInfo.packageManager == "dnf") {
        // Get installed packages
        auto installedPackages = executeCommandLines("rpm -qa --queryformat '%{NAME}\n'");
        packageInfo.installedPackages = installedPackages;
        packageInfo.totalPackages = installedPackages.size();

        // Get available updates
        auto availableUpdates = executeCommandLines(packageInfo.packageManager + " check-update 2>/dev/null | grep -v '^$'");
        packageInfo.availableUpdates = availableUpdates;
        packageInfo.upgradablePackages = availableUpdates.size();
    }

    return packageInfo;
}

auto LinuxOSImplementation::getContainerInfo() -> LinuxContainerInfo {
    return detectContainerEnvironment();
}

auto LinuxOSImplementation::getPerformanceMetrics() -> SystemPerformanceMetrics {
    spdlog::debug("Getting Linux performance metrics");
    SystemPerformanceMetrics metrics;

    // Get memory usage
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

    // Get CPU usage (simplified)
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        double load1min;
        loadavg >> load1min;
        metrics.cpuUsagePercent = load1min * 100.0; // Simplified calculation
    }

    // Get process count
    auto processCount = executeCommand("ps aux | wc -l");
    if (!processCount.empty()) {
        metrics.processCount = std::stoull(processCount);
    }

    // Get disk usage for root filesystem
    auto diskUsage = executeCommand("df / | tail -1 | awk '{print $5}' | sed 's/%//'");
    if (!diskUsage.empty()) {
        metrics.diskUsagePercent = std::stod(diskUsage);
    }

    return metrics;
}

auto LinuxOSImplementation::getSecurityInfo() -> SystemSecurityInfo {
    spdlog::debug("Getting Linux security information");
    SystemSecurityInfo secInfo;

    // Check firewall status
    auto ufwStatus = executeCommand("ufw status 2>/dev/null");
    secInfo.firewallEnabled = ufwStatus.find("active") != std::string::npos;

    if (!secInfo.firewallEnabled) {
        auto iptablesStatus = executeCommand("iptables -L 2>/dev/null | wc -l");
        secInfo.firewallEnabled = !iptablesStatus.empty() && std::stoi(iptablesStatus) > 8;
    }

    // Check SELinux
    auto selinuxStatus = executeCommand("getenforce 2>/dev/null");
    if (!selinuxStatus.empty()) {
        secInfo.securityFeatures.push_back("SELinux: " + selinuxStatus);
    }

    // Check AppArmor
    auto apparmorStatus = executeCommand("aa-status 2>/dev/null | head -1");
    if (!apparmorStatus.empty()) {
        secInfo.securityFeatures.push_back("AppArmor: " + apparmorStatus);
    }

    // Check for antivirus
    auto clamavStatus = executeCommand("which clamav 2>/dev/null");
    secInfo.antivirusEnabled = !clamavStatus.empty();

    return secInfo;
}

auto LinuxOSImplementation::getNetworkConfig() -> NetworkConfiguration {
    spdlog::debug("Getting Linux network configuration");
    NetworkConfiguration netConfig;

    // Get default route interface
    auto defaultRoute = executeCommand("ip route show default | head -1");
    if (!defaultRoute.empty()) {
        std::istringstream iss(defaultRoute);
        std::string token;
        while (iss >> token) {
            if (token == "dev") {
                iss >> netConfig.primaryInterface;
                break;
            }
        }
    }

    if (!netConfig.primaryInterface.empty()) {
        // Get IP address
        auto ipAddr = executeCommand("ip addr show " + netConfig.primaryInterface + " | grep 'inet ' | head -1");
        std::regex ipRegex(R"(inet (\d+\.\d+\.\d+\.\d+))");
        std::smatch match;
        if (std::regex_search(ipAddr, match, ipRegex)) {
            netConfig.ipAddress = match[1].str();
        }

        // Get MAC address
        auto macAddr = executeCommand("ip link show " + netConfig.primaryInterface + " | grep 'link/ether'");
        std::regex macRegex(R"(link/ether ([a-fA-F0-9:]{17}))");
        if (std::regex_search(macAddr, match, macRegex)) {
            netConfig.macAddress = match[1].str();
        }
    }

    // Get DNS servers
    std::ifstream resolvConf("/etc/resolv.conf");
    if (resolvConf.is_open()) {
        std::string line;
        while (std::getline(resolvConf, line)) {
            if (line.find("nameserver") == 0) {
                std::istringstream iss(line);
                std::string nameserver, dns;
                iss >> nameserver >> dns;
                if (!dns.empty()) {
                    netConfig.dnsServers.push_back(dns);
                }
            }
        }
    }

    return netConfig;
}

// Helper function implementations
auto LinuxOSImplementation::parseOSRelease() -> LinuxDistributionInfo {
    spdlog::debug("Parsing /etc/os-release");
    LinuxDistributionInfo distInfo;

    std::ifstream osRelease("/etc/os-release");
    if (osRelease.is_open()) {
        std::string line;
        while (std::getline(osRelease, line)) {
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Remove quotes
            if (value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            if (key == "NAME") {
                distInfo.name = value;
            } else if (key == "VERSION") {
                distInfo.version = value;
            } else if (key == "VERSION_CODENAME") {
                distInfo.codename = value;
            } else if (key == "ID") {
                distInfo.id = value;
            } else if (key == "ID_LIKE") {
                distInfo.idLike = value;
            } else if (key == "VERSION_ID") {
                distInfo.versionId = value;
            } else if (key == "PRETTY_NAME") {
                distInfo.prettyName = value;
            } else if (key == "HOME_URL") {
                distInfo.homeUrl = value;
            } else if (key == "SUPPORT_URL") {
                distInfo.supportUrl = value;
            } else if (key == "BUG_REPORT_URL") {
                distInfo.bugReportUrl = value;
            }
        }
    }

    return distInfo;
}

auto LinuxOSImplementation::parseKernelVersion() -> LinuxKernelInfo {
    spdlog::debug("Parsing kernel version information");
    LinuxKernelInfo kernelInfo;

    struct utsname unameData;
    if (uname(&unameData) == 0) {
        kernelInfo.version = unameData.release;
        kernelInfo.release = unameData.version;
    }

    // Check for realtime kernel
    kernelInfo.isRealtime = kernelInfo.version.find("rt") != std::string::npos;
    kernelInfo.isLowLatency = kernelInfo.version.find("lowlatency") != std::string::npos;

    // Get loaded modules
    auto modules = executeCommandLines("lsmod | tail -n +2 | awk '{print $1}'");
    kernelInfo.modules = modules;

    return kernelInfo;
}

auto LinuxOSImplementation::detectPackageManager() -> std::string {
    if (executeCommand("which apt 2>/dev/null").empty() == false) {
        return "apt";
    } else if (executeCommand("which yum 2>/dev/null").empty() == false) {
        return "yum";
    } else if (executeCommand("which dnf 2>/dev/null").empty() == false) {
        return "dnf";
    } else if (executeCommand("which pacman 2>/dev/null").empty() == false) {
        return "pacman";
    } else if (executeCommand("which zypper 2>/dev/null").empty() == false) {
        return "zypper";
    }
    return "unknown";
}

auto LinuxOSImplementation::detectInitSystem() -> std::string {
    if (executeCommand("which systemctl 2>/dev/null").empty() == false) {
        return "systemd";
    } else if (executeCommand("which service 2>/dev/null").empty() == false) {
        return "sysvinit";
    } else if (executeCommand("which initctl 2>/dev/null").empty() == false) {
        return "upstart";
    }
    return "unknown";
}

auto LinuxOSImplementation::detectContainerEnvironment() -> LinuxContainerInfo {
    spdlog::debug("Detecting container environment");
    LinuxContainerInfo containerInfo;

    // Check for Docker
    std::ifstream dockerEnv("/.dockerenv");
    if (dockerEnv.good()) {
        containerInfo.isContainer = true;
        containerInfo.containerType = "docker";
        containerInfo.containerRuntime = "docker";
    }

    // Check for LXC
    if (!containerInfo.isContainer) {
        std::ifstream cgroupFile("/proc/1/cgroup");
        if (cgroupFile.is_open()) {
            std::string line;
            while (std::getline(cgroupFile, line)) {
                if (line.find("lxc") != std::string::npos) {
                    containerInfo.isContainer = true;
                    containerInfo.containerType = "lxc";
                    break;
                } else if (line.find("docker") != std::string::npos) {
                    containerInfo.isContainer = true;
                    containerInfo.containerType = "docker";
                    break;
                }
            }
        }
    }

    return containerInfo;
}

// Legacy compatibility functions
auto getComputerNameLinux() -> std::optional<std::string> {
    std::array<char, 256> hostname;
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        return std::string(hostname.data());
    }
    return std::nullopt;
}

void getOperatingSystemInfoLinux(OperatingSystemInfo& osInfo) {
    LinuxOSImplementation impl;
    auto enhancedInfo = impl.getEnhancedOSInfo();

    osInfo.osName = enhancedInfo.osName;
    osInfo.osVersion = enhancedInfo.osVersion;
    osInfo.kernelVersion = enhancedInfo.kernelVersion;
    osInfo.architecture = enhancedInfo.architecture;
    osInfo.computerName = enhancedInfo.computerName;
    osInfo.isServer = enhancedInfo.isServer;
}

auto getSystemUptimeLinux() -> std::chrono::seconds {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return std::chrono::seconds(si.uptime);
    }
    return std::chrono::seconds(0);
}

auto getSystemTimeZoneLinux() -> std::string {
    std::ifstream tz("/etc/timezone");
    if (tz.is_open()) {
        std::string timezone;
        std::getline(tz, timezone);
        return timezone;
    }
    return "Unknown";
}

auto getInstalledUpdatesLinux() -> std::vector<std::string> {
    LinuxOSImplementation impl;
    auto packageInfo = impl.getPackageInfo();
    return packageInfo.installedPackages;
}

auto getSystemLanguageLinux() -> std::string {
    auto lang = executeCommand("echo $LANG");
    return lang.empty() ? "Unknown" : lang;
}

auto getSystemEncodingLinux() -> std::string {
    auto encoding = executeCommand("locale charmap");
    return encoding.empty() ? "Unknown" : encoding;
}

auto isServerEditionLinux() -> bool {
    // Check for server indicators
    auto hostname = executeCommand("hostname");
    if (hostname.find("server") != std::string::npos) {
        return true;
    }

    // Check for GUI
    auto display = executeCommand("echo $DISPLAY");
    return display.empty();
}

}  // namespace atom::system

#endif  // defined(__linux__) || defined(__linux)
