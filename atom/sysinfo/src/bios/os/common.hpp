/**
 * @file common.hpp
 * @brief Enhanced common utilities and definitions for OS information module
 *
 * This file contains enhanced common utilities, structures, and helper functions
 * used across different platform implementations of the OS information module.
 * Provides advanced OS analysis, monitoring, and management capabilities.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_BIOS_OS_COMMON_HPP
#define ATOM_SYSINFO_BIOS_OS_COMMON_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Enhanced operating system types
 */
enum class OSType {
    WINDOWS,
    LINUX,
    MACOS,
    FREEBSD,
    ANDROID,
    IOS,
    UNKNOWN
};

/**
 * @brief Operating system architecture types
 */
enum class OSArchitecture {
    X86,
    X64,
    ARM,
    ARM64,
    MIPS,
    POWERPC,
    SPARC,
    UNKNOWN
};

/**
 * @brief System performance metrics
 */
struct SystemPerformanceMetrics {
    double cpuUsagePercent = 0.0;
    double memoryUsagePercent = 0.0;
    double diskUsagePercent = 0.0;
    double networkUsageKbps = 0.0;
    std::chrono::milliseconds responseTime{0};
    uint64_t processCount = 0;
    uint64_t threadCount = 0;
} ATOM_ALIGNAS(64);

/**
 * @brief System security information
 */
struct SystemSecurityInfo {
    bool firewallEnabled = false;
    bool antivirusEnabled = false;
    bool encryptionEnabled = false;
    bool secureBootEnabled = false;
    bool tpmEnabled = false;
    std::vector<std::string> securityFeatures;
    std::vector<std::string> vulnerabilities;
    std::string lastSecurityUpdate;
} ATOM_ALIGNAS(64);

/**
 * @brief Network configuration information
 */
struct NetworkConfiguration {
    std::string primaryInterface;
    std::string ipAddress;
    std::string subnetMask;
    std::string gateway;
    std::vector<std::string> dnsServers;
    std::string macAddress;
    bool dhcpEnabled = false;
    std::unordered_map<std::string, std::string> additionalInterfaces;
} ATOM_ALIGNAS(64);

/**
 * @brief System environment information
 */
struct SystemEnvironment {
    std::unordered_map<std::string, std::string> environmentVariables;
    std::vector<std::string> systemPath;
    std::string workingDirectory;
    std::string tempDirectory;
    std::string userProfile;
    std::string systemRoot;
} ATOM_ALIGNAS(64);

/**
 * @brief Enhanced operating system information structure
 */
struct EnhancedOSInfo {
    // Basic information
    std::string osName;
    std::string osVersion;
    std::string kernelVersion;
    std::string architecture;
    std::string computerName;
    std::string bootTime;
    std::string installDate;
    std::string lastUpdate;
    std::string timeZone;
    std::string charSet;
    bool isServer = false;
    
    // Enhanced information
    OSType osType = OSType::UNKNOWN;
    OSArchitecture osArch = OSArchitecture::UNKNOWN;
    std::string buildNumber;
    std::string servicePackVersion;
    std::string edition;
    std::string productKey;
    std::string serialNumber;
    
    // System metrics
    SystemPerformanceMetrics performance;
    SystemSecurityInfo security;
    NetworkConfiguration network;
    SystemEnvironment environment;
    
    // Update information
    std::vector<std::string> installedUpdates;
    std::vector<std::string> availableUpdates;
    std::vector<std::string> installedSoftware;
    
    // Hardware information
    std::string motherboardModel;
    std::string biosVersion;
    std::string cpuModel;
    uint64_t totalMemoryBytes = 0;
    uint64_t availableMemoryBytes = 0;
    
    // Runtime information
    std::chrono::system_clock::time_point lastRefresh;
    std::chrono::seconds uptime{0};
    
    EnhancedOSInfo() = default;
    
    /**
     * @brief Converts the enhanced OS information to JSON format
     * @return JSON string representation
     */
    auto toJson() const -> std::string;
    
    /**
     * @brief Converts to detailed string format
     * @return Detailed string representation
     */
    auto toDetailedString() const -> std::string;
    
    /**
     * @brief Refreshes all dynamic information
     * @return true if successful, false otherwise
     */
    auto refresh() -> bool;
} ATOM_ALIGNAS(128);

/**
 * @brief OS operation result codes
 */
enum class OSOperationResult {
    SUCCESS,
    FAILED,
    NOT_SUPPORTED,
    INSUFFICIENT_PRIVILEGES,
    TIMEOUT,
    INVALID_PARAMETER
};

/**
 * @brief System monitoring callback function type
 */
using SystemMonitorCallback = std::function<void(const SystemPerformanceMetrics&)>;

/**
 * @brief Security event callback function type
 */
using SecurityEventCallback = std::function<void(const std::string& event, const std::string& details)>;

/**
 * @brief Parses a configuration file for OS information
 * @param filePath Path to the file to parse
 * @return Pair containing OS name and version
 */
auto parseFile(const std::string& filePath) -> std::pair<std::string, std::string>;

/**
 * @brief Enhanced WSL detection with version information
 * @return Optional containing WSL version if detected, nullopt otherwise
 */
auto detectWSL() -> std::optional<std::string>;

/**
 * @brief Checks for available system updates
 * @return Vector containing available updates
 */
auto checkForUpdates() -> std::vector<std::string>;

/**
 * @brief Gets system performance metrics
 * @return Current system performance metrics
 */
auto getSystemPerformanceMetrics() -> SystemPerformanceMetrics;

/**
 * @brief Gets system security information
 * @return Current system security information
 */
auto getSystemSecurityInfo() -> SystemSecurityInfo;

/**
 * @brief Gets network configuration
 * @return Current network configuration
 */
auto getNetworkConfiguration() -> NetworkConfiguration;

/**
 * @brief Gets system environment information
 * @return Current system environment
 */
auto getSystemEnvironment() -> SystemEnvironment;

/**
 * @brief Converts OS type enum to string
 * @param type OS type enum value
 * @return String representation of OS type
 */
auto osTypeToString(OSType type) -> std::string;

/**
 * @brief Converts OS architecture enum to string
 * @param arch OS architecture enum value
 * @return String representation of OS architecture
 */
auto osArchitectureToString(OSArchitecture arch) -> std::string;

/**
 * @brief Detects OS type from system information
 * @return Detected OS type
 */
auto detectOSType() -> OSType;

/**
 * @brief Detects OS architecture from system information
 * @return Detected OS architecture
 */
auto detectOSArchitecture() -> OSArchitecture;

/**
 * @brief Executes a system command and returns output
 * @param command Command to execute
 * @return Command output or empty string on failure
 */
auto executeCommand(const std::string& command) -> std::string;

/**
 * @brief Executes a system command and returns output lines
 * @param command Command to execute
 * @return Vector of output lines
 */
auto executeCommandLines(const std::string& command) -> std::vector<std::string>;

/**
 * @brief Validates system integrity
 * @return true if system integrity is valid, false otherwise
 */
auto validateSystemIntegrity() -> bool;

/**
 * @brief Gets installed software list
 * @return Vector of installed software names
 */
auto getInstalledSoftware() -> std::vector<std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSINFO_BIOS_OS_COMMON_HPP
