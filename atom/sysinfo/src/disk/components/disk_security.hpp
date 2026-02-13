/*
 * disk_security.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Security

**************************************************/

#ifndef ATOM_SYSTEM_DISK_SECURITY_HPP
#define ATOM_SYSTEM_DISK_SECURITY_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "disk_types.hpp"

namespace atom::system {

/**
 * @brief Encryption status of a device
 */
enum class EncryptionStatus {
    UNKNOWN,
    NOT_ENCRYPTED,
    ENCRYPTED,
    PARTIALLY_ENCRYPTED,
    ENCRYPTION_IN_PROGRESS
};

/**
 * @brief Threat level classification
 */
enum class ThreatLevel {
    NONE,
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * @brief Structure for device fingerprint
 */
struct DeviceFingerprint {
    std::string devicePath;
    std::string serialNumber;
    std::string model;
    std::string vendor;
    std::string firmwareVersion;
    uint64_t sizeBytes{0};
    std::string checksumMBR;  // Master Boot Record checksum
    std::string checksumPartitionTable;
    std::chrono::system_clock::time_point createdAt{std::chrono::system_clock::now()};

    DeviceFingerprint() = default;

    DeviceFingerprint(const std::string& path, const StorageDevice& device)
        : devicePath(path), serialNumber(device.serialNumber),
          model(device.model), vendor(device.vendor),
          firmwareVersion(device.firmwareVersion), sizeBytes(device.sizeBytes) {}
};

/**
 * @brief Structure for threat scan result
 */
struct ThreatScanResult {
    bool success{false};
    int suspiciousFiles{0};
    int quarantinedFiles{0};
    ThreatLevel maxThreatLevel{ThreatLevel::NONE};
    std::vector<std::string> threatDetails;
    std::chrono::system_clock::time_point scanTime{std::chrono::system_clock::now()};
    std::chrono::milliseconds scanDuration{0};

    ThreatScanResult() = default;

    ThreatScanResult(bool success, int suspicious, ThreatLevel level)
        : success(success), suspiciousFiles(suspicious), maxThreatLevel(level) {}
};

/**
 * @brief Structure for security audit result
 */
struct SecurityAuditResult {
    std::string devicePath;
    EncryptionStatus encryptionStatus{EncryptionStatus::UNKNOWN};
    bool hasSecureBoot{false};
    bool hasTPMSupport{false};
    bool isReadOnly{false};
    bool isInWhitelist{false};
    ThreatScanResult lastScanResult;
    DeviceFingerprint fingerprint;
    std::vector<std::string> securityIssues;
    std::chrono::system_clock::time_point auditTime{std::chrono::system_clock::now()};
};

/**
 * @brief Configuration for advanced security scanning
 */
struct SecurityScanConfig {
    bool enableMLPatterns{true};
    bool enableBehaviorAnalysis{true};
    bool enableDeepScan{false};
    bool quarantineThreats{true};
    int maxScanDepth{10};
    std::chrono::minutes scanTimeout{std::chrono::minutes(30)};
    ThreatLevel quarantineThreshold{ThreatLevel::MEDIUM};
    std::vector<std::string> excludedExtensions{".tmp", ".log", ".cache"};
    std::vector<std::string> suspiciousExtensions{".exe", ".bat", ".cmd", ".scr", ".vbs"};
};

/**
 * @brief Adds a device to the security whitelist
 *
 * @param deviceIdentifier Device identifier (serial number, UUID, etc.)
 * @return true if successful, false otherwise
 */
auto addDeviceToWhitelist(const std::string& deviceIdentifier) -> bool;

/**
 * @brief Removes a device from the security whitelist
 *
 * @param deviceIdentifier Device identifier (serial number, UUID, etc.)
 * @return true if successful, false otherwise
 */
auto removeDeviceFromWhitelist(const std::string& deviceIdentifier) -> bool;

/**
 * @brief Checks if a device is in the whitelist
 *
 * @param deviceIdentifier Device identifier to check
 * @return true if in whitelist, false otherwise
 */
[[nodiscard]] auto isDeviceInWhitelist(const std::string& deviceIdentifier)
    -> bool;

/**
 * @brief Sets a disk to read-only mode for security
 *
 * @param path The path to the disk or mount point
 * @return true if successful, false otherwise
 */
auto setDiskReadOnly(const std::string& path) -> bool;

/**
 * @brief Scans a disk for malicious files
 *
 * @param path The path to the disk or mount point
 * @param scanDepth How many directory levels to scan (0 for unlimited)
 * @return A pair containing success status and number of suspicious files found
 */
auto scanDiskForThreats(const std::string& path, int scanDepth = 0)
    -> std::pair<bool, int>;

/**
 * @brief Detects encryption status of a device
 *
 * @param devicePath Path to the device
 * @return Encryption status of the device
 */
[[nodiscard]] auto detectEncryptionStatus(const std::string& devicePath) -> EncryptionStatus;

/**
 * @brief Advanced threat scanning with machine learning patterns
 *
 * @param path The path to scan
 * @param config Configuration for the scan
 * @return Detailed threat scan result
 */
[[nodiscard]] auto scanForThreatsAdvanced(const std::string& path,
                                         const SecurityScanConfig& config = {}) -> ThreatScanResult;

/**
 * @brief Creates a device fingerprint for identification
 *
 * @param devicePath Path to the device
 * @return Device fingerprint or nullopt if failed
 */
[[nodiscard]] auto createDeviceFingerprint(const std::string& devicePath) -> std::optional<DeviceFingerprint>;

/**
 * @brief Verifies a device against its stored fingerprint
 *
 * @param devicePath Path to the device
 * @param storedFingerprint Previously stored fingerprint
 * @return True if device matches fingerprint, false otherwise
 */
[[nodiscard]] auto verifyDeviceFingerprint(const std::string& devicePath,
                                          const DeviceFingerprint& storedFingerprint) -> bool;

/**
 * @brief Performs comprehensive security audit of a device
 *
 * @param devicePath Path to the device
 * @return Complete security audit result
 */
[[nodiscard]] auto performSecurityAudit(const std::string& devicePath) -> SecurityAuditResult;

/**
 * @brief Quarantines suspicious files found during scan
 *
 * @param filePaths Vector of file paths to quarantine
 * @param quarantineDir Directory to move quarantined files
 * @return Number of successfully quarantined files
 */
auto quarantineFiles(const std::vector<std::string>& filePaths,
                    const std::string& quarantineDir = "/tmp/quarantine") -> int;

/**
 * @brief Establishes secure communication channel with device
 *
 * @param devicePath Path to the device
 * @return True if secure channel established, false otherwise
 */
auto establishSecureChannel(const std::string& devicePath) -> bool;

/**
 * @brief Validates device integrity using checksums
 *
 * @param devicePath Path to the device
 * @return True if device integrity is valid, false otherwise
 */
[[nodiscard]] auto validateDeviceIntegrity(const std::string& devicePath) -> bool;

/**
 * @brief Encrypts a device using system encryption
 *
 * @param devicePath Path to the device
 * @param passphrase Encryption passphrase
 * @return True if encryption successful, false otherwise
 */
auto encryptDevice(const std::string& devicePath, const std::string& passphrase) -> bool;

/**
 * @brief Securely wipes a device
 *
 * @param devicePath Path to the device
 * @param passes Number of overwrite passes (default: 3)
 * @return True if wipe successful, false otherwise
 */
auto secureWipeDevice(const std::string& devicePath, int passes = 3) -> bool;

/**
 * @brief Class for advanced security management
 */
class SecurityManager {
public:
    SecurityManager();
    ~SecurityManager();

    // Disable copy constructor and assignment
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;

    // Enable move constructor and assignment
    SecurityManager(SecurityManager&&) noexcept;
    SecurityManager& operator=(SecurityManager&&) noexcept;

    /**
     * @brief Add device to whitelist with fingerprinting
     */
    bool addDeviceToWhitelist(const std::string& devicePath);

    /**
     * @brief Remove device from whitelist
     */
    bool removeDeviceFromWhitelist(const std::string& deviceIdentifier);

    /**
     * @brief Check if device is whitelisted
     */
    [[nodiscard]] bool isDeviceWhitelisted(const std::string& devicePath) const;

    /**
     * @brief Perform comprehensive security scan
     */
    [[nodiscard]] ThreatScanResult performSecurityScan(const std::string& path,
                                                       const SecurityScanConfig& config = {});

    /**
     * @brief Monitor device for security events
     */
    void startSecurityMonitoring(std::function<void(const SecurityAuditResult&)> callback);

    /**
     * @brief Stop security monitoring
     */
    void stopSecurityMonitoring();

    /**
     * @brief Get security statistics
     */
    struct SecurityStats {
        size_t devicesMonitored{0};
        size_t threatsDetected{0};
        size_t filesQuarantined{0};
        size_t securityViolations{0};
        std::chrono::system_clock::time_point lastScan;
    };

    [[nodiscard]] SecurityStats getSecurityStats() const;

    /**
     * @brief Update security configuration
     */
    void updateScanConfig(const SecurityScanConfig& config);

    /**
     * @brief Get current security configuration
     */
    [[nodiscard]] const SecurityScanConfig& getScanConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_SECURITY_HPP
