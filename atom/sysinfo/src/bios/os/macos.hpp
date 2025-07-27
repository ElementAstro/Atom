/**
 * @file macos.hpp
 * @brief Enhanced macOS-specific OS information functions
 *
 * This file contains enhanced macOS-specific implementations for retrieving
 * comprehensive operating system information, including advanced system
 * monitoring, security analysis, and performance metrics using IOKit and system APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_BIOS_OS_MACOS_HPP
#define ATOM_SYSINFO_BIOS_OS_MACOS_HPP

#ifdef __APPLE__

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"

namespace atom::system {

/**
 * @brief macOS version information
 */
struct MacOSVersionInfo {
    std::string productName;
    std::string productVersion;
    std::string productBuildVersion;
    std::string kernelVersion;
    std::string systemVersion;
    std::string darwinVersion;
    std::string xcodeVersion;
    std::string commandLineToolsVersion;
    bool isBetaVersion = false;
    bool isAppleSilicon = false;
};

/**
 * @brief macOS security information
 */
struct MacOSSecurityInfo {
    bool sipEnabled = false; // System Integrity Protection
    bool gatekeeperEnabled = false;
    bool xprotectEnabled = false;
    bool firewallEnabled = false;
    bool filevaultEnabled = false;
    bool secureBootEnabled = false;
    std::string secureBootLevel;
    std::vector<std::string> securityFeatures;
    std::vector<std::string> quarantinedFiles;
    std::string lastXprotectUpdate;
};

/**
 * @brief macOS application information
 */
struct MacOSApplicationInfo {
    std::vector<std::string> installedApplications;
    std::vector<std::string> systemApplications;
    std::vector<std::string> userApplications;
    std::vector<std::string> brewPackages;
    std::vector<std::string> macPortsPackages;
    std::vector<std::string> launchAgents;
    std::vector<std::string> launchDaemons;
};

/**
 * @brief macOS hardware information
 */
struct MacOSHardwareInfo {
    std::string modelIdentifier;
    std::string modelName;
    std::string chipType; // Intel, Apple M1, M2, etc.
    std::string serialNumber;
    std::string hardwareUUID;
    std::string platformUUID;
    uint64_t memorySize = 0;
    std::string bootROMVersion;
    std::string smcVersion;
    std::vector<std::string> ioDevices;
};

/**
 * @brief macOS system preferences
 */
struct MacOSSystemPreferences {
    std::unordered_map<std::string, std::string> systemDefaults;
    std::unordered_map<std::string, std::string> userDefaults;
    std::vector<std::string> loginItems;
    std::vector<std::string> startupItems;
    std::string computerName;
    std::string localHostName;
    std::string hostName;
};

/**
 * @brief Enhanced macOS OS implementation
 */
class MacOSOSImplementation {
public:
    MacOSOSImplementation();
    ~MacOSOSImplementation();

    /**
     * @brief Gets enhanced macOS OS information
     * @return Enhanced OS information structure
     */
    auto getEnhancedOSInfo() -> EnhancedOSInfo;

    /**
     * @brief Gets macOS version information
     * @return Version information
     */
    auto getVersionInfo() -> MacOSVersionInfo;

    /**
     * @brief Gets macOS security information
     * @return Security information
     */
    auto getSecurityInfo() -> MacOSSecurityInfo;

    /**
     * @brief Gets macOS application information
     * @return Application information
     */
    auto getApplicationInfo() -> MacOSApplicationInfo;

    /**
     * @brief Gets macOS hardware information
     * @return Hardware information
     */
    auto getHardwareInfo() -> MacOSHardwareInfo;

    /**
     * @brief Gets macOS system preferences
     * @return System preferences
     */
    auto getSystemPreferences() -> MacOSSystemPreferences;

    /**
     * @brief Gets system performance metrics using IOKit
     * @return Performance metrics
     */
    auto getPerformanceMetrics() -> SystemPerformanceMetrics;

    /**
     * @brief Gets network configuration using system APIs
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
     * @brief Gets system logs using log command
     * @param predicate Log predicate filter
     * @param maxEntries Maximum number of entries to retrieve
     * @return Log entries
     */
    auto getSystemLogs(const std::string& predicate = "",
                      uint32_t maxEntries = 100) -> std::vector<std::string>;

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
     * @brief Manages launchd services
     * @param serviceName Service name (plist file)
     * @param action Action to perform (load, unload, start, stop)
     * @return Operation result
     */
    auto manageService(const std::string& serviceName,
                      const std::string& action) -> OSOperationResult;

    /**
     * @brief Updates system using softwareupdate
     * @param installAll Install all available updates
     * @return Operation result
     */
    auto updateSystem(bool installAll = false) -> OSOperationResult;

    /**
     * @brief Configures macOS firewall
     * @param rules Firewall rules to apply
     * @return Operation result
     */
    auto configureFirewall(const std::vector<std::string>& rules) -> OSOperationResult;

    /**
     * @brief Performs security scan using built-in tools
     * @return Scan results
     */
    auto performSecurityScan() -> std::vector<std::string>;

    /**
     * @brief Optimizes macOS system performance
     * @return Optimization results
     */
    auto optimizeSystem() -> std::vector<std::string>;

    /**
     * @brief Gets Spotlight metadata for files
     * @param path File or directory path
     * @return Metadata information
     */
    auto getSpotlightMetadata(const std::string& path) -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Manages system preferences
     * @param domain Preference domain
     * @param key Preference key
     * @param value Preference value (empty to read)
     * @return Operation result or current value
     */
    auto manageSystemPreference(const std::string& domain,
                               const std::string& key,
                               const std::string& value = "") -> std::string;

    /**
     * @brief Gets Time Machine backup information
     * @return Backup information
     */
    auto getTimeMachineInfo() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Gets energy and thermal information
     * @return Energy and thermal data
     */
    auto getEnergyInfo() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Gets Keychain information
     * @return Keychain details
     */
    auto getKeychainInfo() -> std::vector<std::string>;

    /**
     * @brief Checks for malware using XProtect
     * @return Malware scan results
     */
    auto checkForMalware() -> std::vector<std::string>;

private:
    bool m_monitoringActive = false;
    std::thread m_monitoringThread;

    // Helper functions
    auto executeSystemProfiler(const std::string& dataType) -> std::string;
    auto executeDefaults(const std::string& domain, const std::string& operation) -> std::string;
    auto executeLog(const std::string& arguments) -> std::vector<std::string>;
    auto getIORegistryProperty(const std::string& service, const std::string& property) -> std::string;
    auto getSysctl(const std::string& name) -> std::string;
    auto parseSystemProfilerOutput(const std::string& output) -> std::unordered_map<std::string, std::string>;
    auto checkSIPStatus() -> bool;
    auto checkGatekeeperStatus() -> bool;
    auto checkFileVaultStatus() -> bool;
    auto getInstalledApplications() -> std::vector<std::string>;
    auto getBrewPackages() -> std::vector<std::string>;
    auto getLaunchAgents() -> std::vector<std::string>;
    auto getLaunchDaemons() -> std::vector<std::string>;
};

// Legacy compatibility functions
auto getComputerNameMacOS() -> std::optional<std::string>;
void getOperatingSystemInfoMacOS(OperatingSystemInfo& osInfo);
auto getSystemUptimeMacOS() -> std::chrono::seconds;
auto getSystemTimeZoneMacOS() -> std::string;
auto getInstalledUpdatesMacOS() -> std::vector<std::string>;
auto getSystemLanguageMacOS() -> std::string;
auto getSystemEncodingMacOS() -> std::string;
auto isServerEditionMacOS() -> bool;

}  // namespace atom::system

#endif  // __APPLE__

#endif  // ATOM_SYSINFO_BIOS_OS_MACOS_HPP
