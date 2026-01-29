#ifndef ATOM_SYSINFO_BIOS_HPP
#define ATOM_SYSINFO_BIOS_HPP

#include <chrono>
#include <string>
#include <vector>
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Structure containing BIOS information data
 */
struct BiosInfoData {
    std::string version;
    std::string manufacturer;
    std::string releaseDate;
    std::string serialNumber;
    std::string characteristics;
    bool isUpgradeable = false;
    std::chrono::system_clock::time_point lastUpdate;

    /**
     * @brief Check if BIOS information is valid
     * @return True if essential fields are populated
     */
    bool isValid() const;

    /**
     * @brief Convert BIOS information to string representation
     * @return Formatted string containing all BIOS information
     */
    std::string toString() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Structure containing BIOS health status information
 */
struct BiosHealthStatus {
    bool isHealthy = true;
    int biosAgeInDays = 0;
    int64_t lastCheckTime = 0;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Structure containing BIOS update information
 */
struct BiosUpdateInfo {
    std::string currentVersion;
    std::string latestVersion;
    bool updateAvailable = false;
    std::string updateUrl;
    std::string releaseNotes;
};

/**
 * @brief Singleton class for managing BIOS information and operations
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
    bool isSecureBootSupported();

    /**
     * @brief Check if UEFI Boot is supported
     * @return True if UEFI Boot is supported
     */
    bool isUEFIBootSupported();

private:
    BiosInfo() = default;
    BiosInfoData fetchBiosInfo();
    std::string getManufacturerUpdateUrl() const;

    BiosInfoData cachedInfo;
    std::chrono::system_clock::time_point cacheTime;
    static constexpr auto CACHE_DURATION = std::chrono::minutes(5);
};

}  // namespace atom::system

#endif  // ATOM_SYSINFO_BIOS_HPP
