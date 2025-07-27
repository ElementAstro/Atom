#ifndef ATOM_SYSINFO_BIOS_BIOS_HPP
#define ATOM_SYSINFO_BIOS_BIOS_HPP

#include "common.hpp"
#include <memory>

namespace atom::system {

/**
 * @brief Singleton class for managing BIOS information and operations
 * 
 * This class provides a unified interface for BIOS operations across different platforms.
 * It uses platform-specific implementations internally while maintaining a consistent API.
 */
class BiosInfo {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton instance
     */
    static BiosInfo& getInstance();

    /**
     * @brief Get cached BIOS information
     * @param forceUpdate Force refresh of cached data
     * @return Reference to BIOS information data
     */
    const BiosInfoData& getBiosInfo(bool forceUpdate = false);

    /**
     * @brief Refresh BIOS information from system
     * @return True if refresh was successful
     */
    bool refreshBiosInfo();

    /**
     * @brief Check BIOS health status
     * @return BIOS health status information
     */
    BiosHealthStatus checkHealth() const;

    /**
     * @brief Check for available BIOS updates
     * @return BIOS update information
     */
    BiosUpdateInfo checkForUpdates() const;

    /**
     * @brief Get SMBIOS data
     * @return Vector of SMBIOS data strings
     */
    std::vector<std::string> getSMBIOSData() const;

    /**
     * @brief Enable or disable Secure Boot
     * @param enable True to enable, false to disable
     * @return True if operation was successful
     */
    bool setSecureBoot(bool enable);

    /**
     * @brief Enable or disable UEFI Boot
     * @param enable True to enable, false to disable
     * @return True if operation was successful
     */
    bool setUEFIBoot(bool enable);

    /**
     * @brief Backup BIOS settings to file
     * @param filepath Path to backup file
     * @return True if backup was successful
     */
    bool backupBiosSettings(const std::string& filepath);

    /**
     * @brief Restore BIOS settings from file
     * @param filepath Path to backup file
     * @return True if restore was successful
     */
    bool restoreBiosSettings(const std::string& filepath);

    /**
     * @brief Check if Secure Boot is supported
     * @return True if Secure Boot is supported
     */
    bool isSecureBootSupported() const;

    /**
     * @brief Check if UEFI Boot is supported
     * @return True if UEFI Boot is supported
     */
    bool isUEFIBootSupported() const;

    // Enhanced features
    
    /**
     * @brief Get detailed firmware information
     * @return Firmware information structure
     */
    FirmwareInfo getFirmwareInfo() const;

    /**
     * @brief Get current boot configuration
     * @return Boot configuration structure
     */
    BootConfiguration getBootConfiguration() const;

    /**
     * @brief Get BIOS security settings
     * @return Security settings structure
     */
    BiosSecuritySettings getSecuritySettings() const;

    /**
     * @brief Set boot device order
     * @param order Vector of boot device names in desired order
     * @return Operation result
     */
    BiosOperationResult setBootOrder(const std::vector<std::string>& order);

    /**
     * @brief Enable or disable CPU virtualization features
     * @param enable True to enable, false to disable
     * @return Operation result
     */
    BiosOperationResult enableVirtualization(bool enable);

    /**
     * @brief Enable or disable CPU hyper-threading
     * @param enable True to enable, false to disable
     * @return Operation result
     */
    BiosOperationResult enableHyperThreading(bool enable);

    /**
     * @brief Get list of available boot devices
     * @return Vector of boot device names
     */
    std::vector<std::string> getAvailableBootDevices() const;

    /**
     * @brief Validate BIOS/firmware integrity
     * @return True if integrity check passes
     */
    bool validateBiosIntegrity() const;

    /**
     * @brief Get human-readable string for operation result
     * @param result Operation result enum
     * @return Descriptive string
     */
    static std::string operationResultToString(BiosOperationResult result);

    // Additional enhanced features

    /**
     * @brief Get power management settings
     * @return Power management settings structure
     */
    PowerManagementSettings getPowerManagementSettings() const;

    /**
     * @brief Set power management settings
     * @param settings Power management settings to apply
     * @return Operation result
     */
    BiosOperationResult setPowerManagementSettings(const PowerManagementSettings& settings);

    /**
     * @brief Get overclocking information
     * @return Overclocking information structure
     */
    OverclockingInfo getOverclockingInfo() const;

    /**
     * @brief Set overclocking profile
     * @param profile Profile name to apply
     * @return Operation result
     */
    BiosOperationResult setOverclockingProfile(const std::string& profile);

    /**
     * @brief Get hardware monitoring data
     * @return Hardware monitoring structure
     */
    HardwareMonitoring getHardwareMonitoring() const;

    /**
     * @brief Run comprehensive BIOS diagnostics
     * @return Diagnostics results structure
     */
    BiosDiagnostics runDiagnostics() const;

    /**
     * @brief Reset BIOS settings to factory defaults
     * @return Operation result
     */
    BiosOperationResult resetToDefaults();

    /**
     * @brief Get list of supported features on this platform
     * @return Vector of feature names
     */
    std::vector<std::string> getSupportedFeatures() const;

private:
    BiosInfo();
    ~BiosInfo() = default;
    
    // Prevent copying
    BiosInfo(const BiosInfo&) = delete;
    BiosInfo& operator=(const BiosInfo&) = delete;

    std::unique_ptr<BiosImplementation> impl_;
    BiosInfoData cachedInfo_;
    std::chrono::system_clock::time_point cacheTime_;
    static constexpr auto CACHE_DURATION = std::chrono::minutes(5);
};

}  // namespace atom::system

#endif  // ATOM_SYSINFO_BIOS_BIOS_HPP
