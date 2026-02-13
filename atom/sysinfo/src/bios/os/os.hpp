/**
 * @file os.hpp
 * @brief Enhanced Operating System Information Module
 *
 * This file contains the enhanced operating system information module with
 * comprehensive system analysis, monitoring, and management capabilities.
 * Provides advanced OS detection, performance monitoring, security analysis,
 * and system optimization features across Windows, Linux, and macOS platforms.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_BIOS_OS_OS_HPP
#define ATOM_SYSINFO_BIOS_OS_OS_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "common.hpp"

namespace atom::system {

/**
 * @brief System monitoring configuration
 */
struct SystemMonitoringConfig {
    uint32_t performanceIntervalMs = 1000;
    uint32_t securityIntervalMs = 5000;
    uint32_t networkIntervalMs = 2000;
    bool enablePerformanceMonitoring = true;
    bool enableSecurityMonitoring = true;
    bool enableNetworkMonitoring = true;
    bool enableEventLogging = true;
    std::string logFilePath;
    uint32_t maxLogEntries = 10000;
};

/**
 * @brief System optimization configuration
 */
struct SystemOptimizationConfig {
    bool optimizeMemory = true;
    bool optimizeDisk = true;
    bool optimizeNetwork = true;
    bool optimizeServices = false;
    bool optimizeStartup = false;
    bool cleanTemporaryFiles = true;
    bool defragmentDisk = false;
    std::vector<std::string> excludedServices;
    std::vector<std::string> excludedStartupPrograms;
};

/**
 * @brief Enhanced Operating System Manager
 *
 * Provides comprehensive operating system information, monitoring, and management
 * capabilities with platform-specific optimizations and advanced features.
 */
class EnhancedOSManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the EnhancedOSManager instance
     */
    static auto getInstance() -> EnhancedOSManager&;

    ~EnhancedOSManager();

    // Disable copy and move operations
    EnhancedOSManager(const EnhancedOSManager&) = delete;
    EnhancedOSManager(EnhancedOSManager&&) = delete;
    auto operator=(const EnhancedOSManager&) -> EnhancedOSManager& = delete;
    auto operator=(EnhancedOSManager&&) -> EnhancedOSManager& = delete;

    /**
     * @brief Gets comprehensive enhanced OS information
     * @param forceRefresh Force refresh of all information
     * @return Enhanced OS information structure
     */
    auto getEnhancedOSInfo(bool forceRefresh = false) -> EnhancedOSInfo;

    /**
     * @brief Gets current system performance metrics
     * @return Current performance metrics
     */
    auto getPerformanceMetrics() -> SystemPerformanceMetrics;

    /**
     * @brief Gets current system security information
     * @return Current security information
     */
    auto getSecurityInfo() -> SystemSecurityInfo;

    /**
     * @brief Gets current network configuration
     * @return Current network configuration
     */
    auto getNetworkConfiguration() -> NetworkConfiguration;

    /**
     * @brief Gets current system environment
     * @return Current system environment
     */
    auto getSystemEnvironment() -> SystemEnvironment;

    /**
     * @brief Starts comprehensive system monitoring
     * @param config Monitoring configuration
     * @return true if monitoring started successfully
     */
    auto startMonitoring(const SystemMonitoringConfig& config) -> bool;

    /**
     * @brief Stops system monitoring
     */
    void stopMonitoring();

    /**
     * @brief Checks if monitoring is active
     * @return true if monitoring is active
     */
    auto isMonitoring() const -> bool;

    /**
     * @brief Sets performance monitoring callback
     * @param callback Callback function for performance updates
     */
    void setPerformanceCallback(SystemMonitorCallback callback);

    /**
     * @brief Sets security event callback
     * @param callback Callback function for security events
     */
    void setSecurityCallback(SecurityEventCallback callback);

    /**
     * @brief Performs comprehensive system health check
     * @return Health check results and recommendations
     */
    auto performHealthCheck() -> std::vector<std::string>;

    /**
     * @brief Optimizes system performance
     * @param config Optimization configuration
     * @return Optimization results
     */
    auto optimizeSystem(const SystemOptimizationConfig& config) -> std::vector<std::string>;

    /**
     * @brief Gets system logs
     * @param category Log category (system, security, application, etc.)
     * @param maxEntries Maximum number of entries to retrieve
     * @return Log entries
     */
    auto getSystemLogs(const std::string& category = "system",
                      uint32_t maxEntries = 100) -> std::vector<std::string>;

    /**
     * @brief Analyzes system resource usage
     * @return Resource usage analysis
     */
    auto analyzeResourceUsage() -> std::unordered_map<std::string, double>;

    /**
     * @brief Gets detailed process information
     * @param includeThreads Include thread information
     * @return Process information
     */
    auto getProcessInfo(bool includeThreads = false) -> std::vector<std::unordered_map<std::string, std::string>>;

    /**
     * @brief Manages system services
     * @param serviceName Service name
     * @param action Action to perform (start, stop, restart, enable, disable)
     * @return Operation result
     */
    auto manageService(const std::string& serviceName,
                      const std::string& action) -> OSOperationResult;

    /**
     * @brief Updates system packages/software
     * @param criticalOnly Update only critical/security updates
     * @return Operation result
     */
    auto updateSystem(bool criticalOnly = false) -> OSOperationResult;

    /**
     * @brief Configures system firewall
     * @param rules Firewall rules to apply
     * @return Operation result
     */
    auto configureFirewall(const std::vector<std::string>& rules) -> OSOperationResult;

    /**
     * @brief Performs security scan
     * @param deepScan Perform deep security scan
     * @return Security scan results
     */
    auto performSecurityScan(bool deepScan = false) -> std::vector<std::string>;

    /**
     * @brief Gets hardware information
     * @return Hardware information
     */
    auto getHardwareInfo() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Gets installed software information
     * @return Installed software list
     */
    auto getInstalledSoftware() -> std::vector<std::string>;

    /**
     * @brief Checks for available system updates
     * @return Available updates
     */
    auto checkForUpdates() -> std::vector<std::string>;

    /**
     * @brief Validates system integrity
     * @return true if system integrity is valid
     */
    auto validateSystemIntegrity() -> bool;

    /**
     * @brief Creates system backup/snapshot
     * @param backupPath Path for backup storage
     * @param includeUserData Include user data in backup
     * @return Operation result
     */
    auto createSystemBackup(const std::string& backupPath,
                           bool includeUserData = false) -> OSOperationResult;

    /**
     * @brief Restores system from backup
     * @param backupPath Path to backup
     * @return Operation result
     */
    auto restoreSystemBackup(const std::string& backupPath) -> OSOperationResult;

    /**
     * @brief Gets system benchmark results
     * @param runBenchmark Run new benchmark if true
     * @return Benchmark results
     */
    auto getBenchmarkResults(bool runBenchmark = false) -> std::unordered_map<std::string, double>;

    /**
     * @brief Configures system power management
     * @param powerPlan Power plan to set
     * @return Operation result
     */
    auto configurePowerManagement(const std::string& powerPlan) -> OSOperationResult;

    /**
     * @brief Gets system event history
     * @param eventType Type of events to retrieve
     * @param maxEvents Maximum number of events
     * @return Event history
     */
    auto getEventHistory(const std::string& eventType = "all",
                        uint32_t maxEvents = 1000) -> std::vector<std::unordered_map<std::string, std::string>>;

    /**
     * @brief Exports system information to file
     * @param filePath Output file path
     * @param format Export format (json, xml, csv)
     * @return Operation result
     */
    auto exportSystemInfo(const std::string& filePath,
                         const std::string& format = "json") -> OSOperationResult;

    /**
     * @brief Gets system compatibility information
     * @param softwareName Software to check compatibility for
     * @return Compatibility information
     */
    auto getCompatibilityInfo(const std::string& softwareName = "") -> std::unordered_map<std::string, std::string>;

private:
    EnhancedOSManager();

    class EnhancedOSManagerImpl;
    std::unique_ptr<EnhancedOSManagerImpl> m_impl;
};

// Enhanced standalone functions

/**
 * @brief Gets basic operating system information (legacy compatibility)
 * @return Basic OS information
 */
auto getOperatingSystemInfo() -> OperatingSystemInfo;

/**
 * @brief Gets enhanced operating system information
 * @return Enhanced OS information
 */
auto getEnhancedOperatingSystemInfo() -> EnhancedOSInfo;

/**
 * @brief Gets computer name
 * @return Computer name or nullopt if failed
 */
auto getComputerName() -> std::optional<std::string>;

/**
 * @brief Gets system uptime
 * @return System uptime in seconds
 */
auto getSystemUptime() -> std::chrono::seconds;

/**
 * @brief Gets last boot time
 * @return Last boot time as string
 */
auto getLastBootTime() -> std::string;

/**
 * @brief Gets system timezone
 * @return System timezone
 */
auto getSystemTimeZone() -> std::string;

/**
 * @brief Gets installed updates
 * @return List of installed updates
 */
auto getInstalledUpdates() -> std::vector<std::string>;

/**
 * @brief Gets system language
 * @return System language
 */
auto getSystemLanguage() -> std::string;

/**
 * @brief Gets system encoding
 * @return System encoding
 */
auto getSystemEncoding() -> std::string;

/**
 * @brief Checks if OS is server edition
 * @return true if server edition
 */
auto isServerEdition() -> bool;

/**
 * @brief Detects virtualization environment
 * @return Virtualization type or empty string if none
 */
auto detectVirtualization() -> std::string;

/**
 * @brief Gets system architecture details
 * @return Architecture information
 */
auto getArchitectureInfo() -> std::unordered_map<std::string, std::string>;

/**
 * @brief Gets system locale information
 * @return Locale information
 */
auto getLocaleInfo() -> std::unordered_map<std::string, std::string>;

/**
 * @brief Gets system font information
 * @return Font information
 */
auto getFontInfo() -> std::vector<std::string>;

/**
 * @brief Gets system display information
 * @return Display information
 */
auto getDisplayInfo() -> std::unordered_map<std::string, std::string>;

}  // namespace atom::system

// Include platform-specific headers
#ifdef _WIN32
#include "windows.hpp"
#elif defined(__linux__) || defined(__linux)
#include "linux.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
#endif

#endif  // ATOM_SYSINFO_BIOS_OS_OS_HPP
