#ifndef ATOM_SYSINFO_BIOS_COMMON_HPP
#define ATOM_SYSINFO_BIOS_COMMON_HPP

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
 * @brief Structure containing firmware information
 */
struct FirmwareInfo {
    std::string type;           // UEFI, Legacy BIOS, etc.
    std::string version;
    std::string vendor;
    std::string buildDate;
    std::vector<std::string> supportedFeatures;
    bool secureBootCapable = false;
    bool tpmSupported = false;
};

/**
 * @brief Structure containing boot configuration information
 */
struct BootConfiguration {
    std::vector<std::string> bootOrder;
    std::string currentBootDevice;
    bool uefiMode = false;
    bool secureBootEnabled = false;
    bool fastBootEnabled = false;
    std::vector<std::string> bootDevices;
};

/**
 * @brief Structure containing BIOS security settings
 */
struct BiosSecuritySettings {
    bool passwordProtected = false;
    bool secureBootEnabled = false;
    bool tpmEnabled = false;
    bool virtualizationEnabled = false;
    bool hyperThreadingEnabled = false;
    std::vector<std::string> securityFeatures;
};

/**
 * @brief Enumeration for BIOS operation results
 */
enum class BiosOperationResult {
    SUCCESS,
    FAILED,
    NOT_SUPPORTED,
    INSUFFICIENT_PRIVILEGES,
    REBOOT_REQUIRED
};

/**
 * @brief Structure containing power management settings
 */
struct PowerManagementSettings {
    bool acpiEnabled = false;
    bool wakeOnLanEnabled = false;
    bool wakeOnKeyboardEnabled = false;
    bool wakeOnMouseEnabled = false;
    std::string powerProfile; // Performance, Balanced, Power Saver
    int cpuPowerLimit = 0; // In watts, 0 means no limit
    std::vector<std::string> supportedSleepStates;
};

/**
 * @brief Structure containing overclocking information
 */
struct OverclockingInfo {
    bool overclockingSupported = false;
    bool overclockingEnabled = false;
    int baseCpuFrequency = 0; // In MHz
    int currentCpuFrequency = 0; // In MHz
    int maxCpuFrequency = 0; // In MHz
    int baseMemoryFrequency = 0; // In MHz
    int currentMemoryFrequency = 0; // In MHz
    std::vector<std::string> availableProfiles;
};

/**
 * @brief Structure containing hardware monitoring data
 */
struct HardwareMonitoring {
    std::vector<int> cpuTemperatures; // In Celsius
    std::vector<int> fanSpeeds; // In RPM
    std::vector<double> voltages; // In volts
    int systemTemperature = 0; // In Celsius
    bool thermalThrottling = false;
    std::vector<std::string> sensorNames;
};

/**
 * @brief Enhanced BIOS diagnostics information
 */
struct BiosDiagnostics {
    bool postTestPassed = true;
    bool memoryTestPassed = true;
    bool cpuTestPassed = true;
    bool storageTestPassed = true;
    std::vector<std::string> failedComponents;
    std::vector<std::string> warnings;
    std::chrono::system_clock::time_point lastDiagnosticTime;
    int diagnosticCode = 0;
};

/**
 * @brief Abstract base class for platform-specific BIOS implementations
 */
class BiosImplementation {
public:
    virtual ~BiosImplementation() = default;

    virtual BiosInfoData fetchBiosInfo() = 0;
    virtual BiosHealthStatus checkHealth() const = 0;
    virtual BiosUpdateInfo checkForUpdates() const = 0;
    virtual std::vector<std::string> getSMBIOSData() const = 0;
    virtual bool setSecureBoot(bool enable) = 0;
    virtual bool setUEFIBoot(bool enable) = 0;
    virtual bool backupBiosSettings(const std::string& filepath) = 0;
    virtual bool restoreBiosSettings(const std::string& filepath) = 0;
    virtual bool isSecureBootSupported() const = 0;
    virtual bool isUEFIBootSupported() const = 0;

    // New enhanced features
    virtual FirmwareInfo getFirmwareInfo() const = 0;
    virtual BootConfiguration getBootConfiguration() const = 0;
    virtual BiosSecuritySettings getSecuritySettings() const = 0;
    virtual BiosOperationResult setBootOrder(const std::vector<std::string>& order) = 0;
    virtual BiosOperationResult enableVirtualization(bool enable) = 0;
    virtual BiosOperationResult enableHyperThreading(bool enable) = 0;
    virtual std::vector<std::string> getAvailableBootDevices() const = 0;
    virtual bool validateBiosIntegrity() const = 0;

    // Additional enhanced features
    virtual PowerManagementSettings getPowerManagementSettings() const = 0;
    virtual BiosOperationResult setPowerManagementSettings(const PowerManagementSettings& settings) = 0;
    virtual OverclockingInfo getOverclockingInfo() const = 0;
    virtual BiosOperationResult setOverclockingProfile(const std::string& profile) = 0;
    virtual HardwareMonitoring getHardwareMonitoring() const = 0;
    virtual BiosDiagnostics runDiagnostics() const = 0;
    virtual BiosOperationResult resetToDefaults() = 0;
    virtual std::vector<std::string> getSupportedFeatures() const = 0;
};



/**
 * @brief Factory function to create platform-specific BIOS implementation
 * @return Unique pointer to platform-specific implementation
 */
std::unique_ptr<BiosImplementation> createBiosImplementation();

/**
 * @brief Utility functions for BIOS operations
 */
namespace BiosUtils {
    /**
     * @brief Convert temperature from Celsius to Fahrenheit
     * @param celsius Temperature in Celsius
     * @return Temperature in Fahrenheit
     */
    double celsiusToFahrenheit(double celsius);

    /**
     * @brief Convert frequency from MHz to GHz
     * @param mhz Frequency in MHz
     * @return Frequency in GHz
     */
    double mhzToGhz(int mhz);

    /**
     * @brief Format boot time for display
     * @param bootTime Boot time in milliseconds
     * @return Formatted string
     */
    std::string formatBootTime(int bootTimeMs);

    /**
     * @brief Validate BIOS version format
     * @param version BIOS version string
     * @return True if format is valid
     */
    bool isValidBiosVersion(const std::string& version);

    /**
     * @brief Parse BIOS date string
     * @param dateString Date string from BIOS
     * @return Parsed time point
     */
    std::chrono::system_clock::time_point parseBiosDate(const std::string& dateString);

    /**
     * @brief Calculate BIOS age in days
     * @param biosDate BIOS release date
     * @return Age in days
     */
    int calculateBiosAge(const std::chrono::system_clock::time_point& biosDate);
}

}  // namespace atom::system

#endif  // ATOM_SYSINFO_BIOS_COMMON_HPP
