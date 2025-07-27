/**
 * @file linux.hpp
 * @brief Enhanced Linux-specific OS information functions
 *
 * This file contains enhanced Linux-specific implementations for retrieving
 * comprehensive operating system information, including advanced system
 * monitoring, security analysis, and performance metrics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_BIOS_OS_LINUX_HPP
#define ATOM_SYSINFO_BIOS_OS_LINUX_HPP

#if defined(__linux__) || defined(__linux)

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"

namespace atom::system {

/**
 * @brief Linux distribution information
 */
struct LinuxDistributionInfo {
    std::string name;
    std::string version;
    std::string codename;
    std::string id;
    std::string idLike;
    std::string versionId;
    std::string prettyName;
    std::string homeUrl;
    std::string supportUrl;
    std::string bugReportUrl;
};

/**
 * @brief Linux kernel information
 */
struct LinuxKernelInfo {
    std::string version;
    std::string release;
    std::string buildDate;
    std::string compiler;
    std::vector<std::string> modules;
    std::vector<std::string> parameters;
    bool isRealtime = false;
    bool isLowLatency = false;
};

/**
 * @brief Linux system services information
 */
struct LinuxSystemServices {
    std::vector<std::string> activeServices;
    std::vector<std::string> failedServices;
    std::vector<std::string> enabledServices;
    std::vector<std::string> disabledServices;
    std::string initSystem; // systemd, sysvinit, upstart, etc.
};

/**
 * @brief Linux package management information
 */
struct LinuxPackageInfo {
    std::string packageManager; // apt, yum, dnf, pacman, etc.
    std::vector<std::string> repositories;
    std::vector<std::string> installedPackages;
    std::vector<std::string> availableUpdates;
    std::vector<std::string> securityUpdates;
    uint64_t totalPackages = 0;
    uint64_t upgradablePackages = 0;
};

/**
 * @brief Linux container information
 */
struct LinuxContainerInfo {
    bool isContainer = false;
    std::string containerType; // docker, lxc, systemd-nspawn, etc.
    std::string containerRuntime;
    std::string containerImage;
    std::vector<std::string> mountedVolumes;
    std::unordered_map<std::string, std::string> environmentVariables;
};

/**
 * @brief Enhanced Linux OS implementation
 */
class LinuxOSImplementation {
public:
    LinuxOSImplementation() = default;
    ~LinuxOSImplementation() = default;

    /**
     * @brief Gets enhanced Linux OS information
     * @return Enhanced OS information structure
     */
    auto getEnhancedOSInfo() -> EnhancedOSInfo;

    /**
     * @brief Gets Linux distribution information
     * @return Distribution information
     */
    auto getDistributionInfo() -> LinuxDistributionInfo;

    /**
     * @brief Gets Linux kernel information
     * @return Kernel information
     */
    auto getKernelInfo() -> LinuxKernelInfo;

    /**
     * @brief Gets system services information
     * @return System services information
     */
    auto getSystemServices() -> LinuxSystemServices;

    /**
     * @brief Gets package management information
     * @return Package information
     */
    auto getPackageInfo() -> LinuxPackageInfo;

    /**
     * @brief Gets container information
     * @return Container information
     */
    auto getContainerInfo() -> LinuxContainerInfo;

    /**
     * @brief Gets system performance metrics
     * @return Performance metrics
     */
    auto getPerformanceMetrics() -> SystemPerformanceMetrics;

    /**
     * @brief Gets security information
     * @return Security information
     */
    auto getSecurityInfo() -> SystemSecurityInfo;

    /**
     * @brief Gets network configuration
     * @return Network configuration
     */
    auto getNetworkConfig() -> NetworkConfiguration;

    /**
     * @brief Monitors system performance
     * @param callback Callback function for performance updates
     * @param intervalMs Monitoring interval in milliseconds
     * @return true if monitoring started successfully
     */
    auto startPerformanceMonitoring(SystemMonitorCallback callback, 
                                   uint32_t intervalMs = 1000) -> bool;

    /**
     * @brief Stops performance monitoring
     */
    void stopPerformanceMonitoring();

    /**
     * @brief Checks system health
     * @return Health status and recommendations
     */
    auto checkSystemHealth() -> std::vector<std::string>;

    /**
     * @brief Gets system logs
     * @param service Service name (empty for all)
     * @param lines Number of lines to retrieve
     * @return Log entries
     */
    auto getSystemLogs(const std::string& service = "", 
                      uint32_t lines = 100) -> std::vector<std::string>;

    /**
     * @brief Analyzes disk usage
     * @return Disk usage analysis
     */
    auto analyzeDiskUsage() -> std::unordered_map<std::string, uint64_t>;

    /**
     * @brief Gets process information
     * @param detailed Include detailed process information
     * @return Process list
     */
    auto getProcessInfo(bool detailed = false) -> std::vector<std::string>;

    /**
     * @brief Manages system services
     * @param service Service name
     * @param action Action to perform (start, stop, restart, enable, disable)
     * @return Operation result
     */
    auto manageService(const std::string& service, 
                      const std::string& action) -> OSOperationResult;

    /**
     * @brief Updates system packages
     * @param securityOnly Update only security packages
     * @return Operation result
     */
    auto updatePackages(bool securityOnly = false) -> OSOperationResult;

    /**
     * @brief Configures firewall
     * @param rules Firewall rules to apply
     * @return Operation result
     */
    auto configureFirewall(const std::vector<std::string>& rules) -> OSOperationResult;

    /**
     * @brief Gets hardware information
     * @return Hardware details
     */
    auto getHardwareInfo() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Checks for rootkits and malware
     * @return Security scan results
     */
    auto performSecurityScan() -> std::vector<std::string>;

    /**
     * @brief Optimizes system performance
     * @return Optimization results
     */
    auto optimizeSystem() -> std::vector<std::string>;

private:
    bool m_monitoringActive = false;
    std::thread m_monitoringThread;
    
    // Helper functions
    auto parseOSRelease() -> LinuxDistributionInfo;
    auto parseKernelVersion() -> LinuxKernelInfo;
    auto detectPackageManager() -> std::string;
    auto detectInitSystem() -> std::string;
    auto detectContainerEnvironment() -> LinuxContainerInfo;
    auto readProcFile(const std::string& path) -> std::string;
    auto readSysFile(const std::string& path) -> std::string;
    auto executeSystemCommand(const std::string& command) -> std::string;
};

// Legacy compatibility functions
auto getComputerNameLinux() -> std::optional<std::string>;
void getOperatingSystemInfoLinux(OperatingSystemInfo& osInfo);
auto getSystemUptimeLinux() -> std::chrono::seconds;
auto getSystemTimeZoneLinux() -> std::string;
auto getInstalledUpdatesLinux() -> std::vector<std::string>;
auto getSystemLanguageLinux() -> std::string;
auto getSystemEncodingLinux() -> std::string;
auto isServerEditionLinux() -> bool;

}  // namespace atom::system

#endif  // defined(__linux__) || defined(__linux)

#endif  // ATOM_SYSINFO_BIOS_OS_LINUX_HPP
