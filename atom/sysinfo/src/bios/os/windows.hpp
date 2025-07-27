/**
 * @file windows.hpp
 * @brief Enhanced Windows-specific OS information functions
 *
 * This file contains enhanced Windows-specific implementations for retrieving
 * comprehensive operating system information, including advanced system
 * monitoring, security analysis, and performance metrics using WMI and Win32 APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_BIOS_OS_WINDOWS_HPP
#define ATOM_SYSINFO_BIOS_OS_WINDOWS_HPP

#ifdef _WIN32

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"

namespace atom::system {

/**
 * @brief Windows edition information
 */
struct WindowsEditionInfo {
    std::string productName;
    std::string edition;
    std::string version;
    std::string buildNumber;
    std::string displayVersion;
    std::string releaseId;
    std::string currentBuild;
    std::string ubr; // Update Build Revision
    std::string installationType;
    std::string registeredOwner;
    std::string registeredOrganization;
    std::string productId;
    bool isServerCore = false;
    bool isNanoServer = false;
};

/**
 * @brief Windows update information
 */
struct WindowsUpdateInfo {
    std::vector<std::string> installedUpdates;
    std::vector<std::string> availableUpdates;
    std::vector<std::string> criticalUpdates;
    std::vector<std::string> securityUpdates;
    std::string lastUpdateCheck;
    std::string lastInstallDate;
    bool automaticUpdatesEnabled = false;
    std::string updateSource;
};

/**
 * @brief Windows services information
 */
struct WindowsServicesInfo {
    std::vector<std::string> runningServices;
    std::vector<std::string> stoppedServices;
    std::vector<std::string> automaticServices;
    std::vector<std::string> manualServices;
    std::vector<std::string> disabledServices;
    std::vector<std::string> criticalServices;
};

/**
 * @brief Windows registry information
 */
struct WindowsRegistryInfo {
    std::unordered_map<std::string, std::string> systemKeys;
    std::unordered_map<std::string, std::string> softwareKeys;
    std::unordered_map<std::string, std::string> hardwareKeys;
    std::vector<std::string> startupPrograms;
    std::vector<std::string> installedPrograms;
};

/**
 * @brief Windows security information
 */
struct WindowsSecurityInfo {
    bool windowsDefenderEnabled = false;
    bool firewallEnabled = false;
    bool uacEnabled = false;
    bool bitLockerEnabled = false;
    bool secureBootEnabled = false;
    bool tpmEnabled = false;
    std::string antivirusProduct;
    std::string firewallProduct;
    std::vector<std::string> securityProviders;
    std::vector<std::string> securityEvents;
};

/**
 * @brief Enhanced Windows OS implementation
 */
class WindowsOSImplementation {
public:
    WindowsOSImplementation();
    ~WindowsOSImplementation();

    /**
     * @brief Gets enhanced Windows OS information
     * @return Enhanced OS information structure
     */
    auto getEnhancedOSInfo() -> EnhancedOSInfo;

    /**
     * @brief Gets Windows edition information
     * @return Edition information
     */
    auto getEditionInfo() -> WindowsEditionInfo;

    /**
     * @brief Gets Windows update information
     * @return Update information
     */
    auto getUpdateInfo() -> WindowsUpdateInfo;

    /**
     * @brief Gets Windows services information
     * @return Services information
     */
    auto getServicesInfo() -> WindowsServicesInfo;

    /**
     * @brief Gets Windows registry information
     * @return Registry information
     */
    auto getRegistryInfo() -> WindowsRegistryInfo;

    /**
     * @brief Gets Windows security information
     * @return Security information
     */
    auto getSecurityInfo() -> WindowsSecurityInfo;

    /**
     * @brief Gets system performance metrics using WMI
     * @return Performance metrics
     */
    auto getPerformanceMetrics() -> SystemPerformanceMetrics;

    /**
     * @brief Gets network configuration using WMI
     * @return Network configuration
     */
    auto getNetworkConfig() -> NetworkConfiguration;

    /**
     * @brief Monitors system performance using WMI
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
     * @brief Checks system health using WMI
     * @return Health status and recommendations
     */
    auto checkSystemHealth() -> std::vector<std::string>;

    /**
     * @brief Gets Windows event logs
     * @param logName Log name (System, Application, Security, etc.)
     * @param maxEvents Maximum number of events to retrieve
     * @return Event log entries
     */
    auto getEventLogs(const std::string& logName = "System", 
                     uint32_t maxEvents = 100) -> std::vector<std::string>;

    /**
     * @brief Analyzes disk usage using WMI
     * @return Disk usage analysis
     */
    auto analyzeDiskUsage() -> std::unordered_map<std::string, uint64_t>;

    /**
     * @brief Gets process information using WMI
     * @param detailed Include detailed process information
     * @return Process list
     */
    auto getProcessInfo(bool detailed = false) -> std::vector<std::string>;

    /**
     * @brief Manages Windows services
     * @param serviceName Service name
     * @param action Action to perform (start, stop, restart, enable, disable)
     * @return Operation result
     */
    auto manageService(const std::string& serviceName, 
                      const std::string& action) -> OSOperationResult;

    /**
     * @brief Installs Windows updates
     * @param criticalOnly Install only critical updates
     * @return Operation result
     */
    auto installUpdates(bool criticalOnly = false) -> OSOperationResult;

    /**
     * @brief Configures Windows Firewall
     * @param rules Firewall rules to apply
     * @return Operation result
     */
    auto configureFirewall(const std::vector<std::string>& rules) -> OSOperationResult;

    /**
     * @brief Gets hardware information using WMI
     * @return Hardware details
     */
    auto getHardwareInfo() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Performs Windows Defender scan
     * @param quickScan Perform quick scan instead of full scan
     * @return Scan results
     */
    auto performSecurityScan(bool quickScan = true) -> std::vector<std::string>;

    /**
     * @brief Optimizes Windows system performance
     * @return Optimization results
     */
    auto optimizeSystem() -> std::vector<std::string>;

    /**
     * @brief Gets Windows features information
     * @return Installed Windows features
     */
    auto getWindowsFeatures() -> std::vector<std::string>;

    /**
     * @brief Manages Windows features
     * @param featureName Feature name
     * @param enable Enable or disable the feature
     * @return Operation result
     */
    auto manageWindowsFeature(const std::string& featureName, 
                             bool enable) -> OSOperationResult;

    /**
     * @brief Gets system restore points
     * @return List of restore points
     */
    auto getRestorePoints() -> std::vector<std::string>;

    /**
     * @brief Creates a system restore point
     * @param description Description for the restore point
     * @return Operation result
     */
    auto createRestorePoint(const std::string& description) -> OSOperationResult;

private:
    bool m_monitoringActive = false;
    std::thread m_monitoringThread;
    void* m_wmiLocator = nullptr;
    void* m_wmiServices = nullptr;
    
    // Helper functions
    auto initializeWMI() -> bool;
    void cleanupWMI();
    auto queryWMI(const std::string& query, const std::string& className) -> std::vector<std::unordered_map<std::string, std::string>>;
    auto readRegistryValue(const std::string& keyPath, const std::string& valueName) -> std::string;
    auto writeRegistryValue(const std::string& keyPath, const std::string& valueName, const std::string& value) -> bool;
    auto executeWMIMethod(const std::string& className, const std::string& methodName, const std::unordered_map<std::string, std::string>& parameters) -> bool;
};

// Legacy compatibility functions
auto getComputerNameWindows() -> std::optional<std::string>;
void getOperatingSystemInfoWindows(OperatingSystemInfo& osInfo);
auto getSystemUptimeWindows() -> std::chrono::seconds;
auto getSystemTimeZoneWindows() -> std::string;
auto getInstalledUpdatesWindows() -> std::vector<std::string>;
auto getSystemLanguageWindows() -> std::string;
auto getSystemEncodingWindows() -> std::string;
auto isServerEditionWindows() -> bool;

}  // namespace atom::system

#endif  // _WIN32

#endif  // ATOM_SYSINFO_BIOS_OS_WINDOWS_HPP
