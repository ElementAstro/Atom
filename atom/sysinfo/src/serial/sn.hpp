#ifndef ATOM_SYSINFO_SN_SN_HPP
#define ATOM_SYSINFO_SN_SN_HPP

#include "common.hpp"
#include <memory>

namespace atom::system {

/**
 * @brief Enhanced system information class providing comprehensive hardware identification
 * 
 * This class serves as a unified interface for collecting various types of system
 * identification information including hardware serial numbers, system UUIDs,
 * network interfaces, memory modules, and more. It uses platform-specific
 * implementations internally while maintaining a consistent cross-platform API.
 * 
 * Features:
 * - Hardware serial numbers (BIOS, motherboard, CPU, disks)
 * - System identification (UUID, machine ID, MAC addresses)
 * - Memory module information
 * - Network interface details
 * - Comprehensive system fingerprinting
 * - Configurable data collection
 * - Result caching with configurable timeout
 */
class SystemInfo {
public:
    /**
     * @brief Default constructor
     */
    SystemInfo();

    /**
     * @brief Constructor with configuration
     * @param config Configuration options for data collection
     */
    explicit SystemInfo(const SystemInfoConfig& config);

    /**
     * @brief Destructor
     */
    ~SystemInfo();

    // Disable copy constructor and assignment operator
    SystemInfo(const SystemInfo&) = delete;
    SystemInfo& operator=(const SystemInfo&) = delete;

    // Enable move constructor and assignment operator
    SystemInfo(SystemInfo&& other) noexcept;
    SystemInfo& operator=(SystemInfo&& other) noexcept;

    /**
     * @brief Get hardware serial numbers
     * @param forceRefresh Force refresh of cached data
     * @return Result containing hardware serial data
     */
    SystemInfoResult<HardwareSerialData> getHardwareSerials(bool forceRefresh = false);

    /**
     * @brief Get system identification information
     * @param forceRefresh Force refresh of cached data
     * @return Result containing system identification data
     */
    SystemInfoResult<SystemIdentificationData> getSystemIdentification(bool forceRefresh = false);

    /**
     * @brief Get memory module information
     * @param forceRefresh Force refresh of cached data
     * @return Result containing vector of memory module info
     */
    SystemInfoResult<std::vector<MemoryModuleInfo>> getMemoryModules(bool forceRefresh = false);

    /**
     * @brief Get network interface information
     * @param forceRefresh Force refresh of cached data
     * @return Result containing vector of network interface info
     */
    SystemInfoResult<std::vector<NetworkInterfaceInfo>> getNetworkInterfaces(bool forceRefresh = false);

    /**
     * @brief Get comprehensive system information
     * @param forceRefresh Force refresh of cached data
     * @return Result containing comprehensive system info
     */
    SystemInfoResult<ComprehensiveSystemInfo> getComprehensiveInfo(bool forceRefresh = false);

    /**
     * @brief Query specific system identifier
     * @param type Type of system identifier to query
     * @param forceRefresh Force refresh of cached data
     * @return Result containing system identifier query result
     */
    SystemInfoResult<SystemIdQuery> querySystemId(SystemIdType type, bool forceRefresh = false);

    /**
     * @brief Get system fingerprint
     * @param forceRefresh Force refresh of cached data
     * @return Unique system fingerprint string
     */
    std::string getSystemFingerprint(bool forceRefresh = false) const;

    /**
     * @brief Check if system information collection is supported on current platform
     * @return True if supported
     */
    bool isSupported() const;

    /**
     * @brief Get platform name
     * @return Platform name (e.g., "Windows", "Linux")
     */
    std::string getPlatformName() const;

    /**
     * @brief Get last error message
     * @return Last error message from operations
     */
    std::string getLastError() const;

    /**
     * @brief Update configuration
     * @param config New configuration options
     */
    void updateConfig(const SystemInfoConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration options
     */
    const SystemInfoConfig& getConfig() const;

    /**
     * @brief Clear all cached data
     */
    void clearCache();

    /**
     * @brief Check if cached data is still valid
     * @return True if cache is valid
     */
    bool isCacheValid() const;

    /**
     * @brief Get cache age in seconds
     * @return Age of cached data in seconds
     */
    std::chrono::seconds getCacheAge() const;

    /**
     * @brief Refresh all cached data
     * @return True if refresh was successful
     */
    bool refreshAll();

    /**
     * @brief Export system information to JSON string
     * @param includeAll Include all available information
     * @return JSON string representation
     */
    std::string exportToJson(bool includeAll = true) const;

    /**
     * @brief Export system information to XML string
     * @param includeAll Include all available information
     * @return XML string representation
     */
    std::string exportToXml(bool includeAll = true) const;

    /**
     * @brief Get system information summary
     * @return Human-readable summary string
     */
    std::string getSummary() const;

    /**
     * @brief Validate system information integrity
     * @return True if all collected data appears valid
     */
    bool validateIntegrity() const;

    /**
     * @brief Get collection statistics
     * @return Map of collection statistics
     */
    std::map<std::string, std::string> getCollectionStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Initialize platform-specific implementation
     */
    void initializePlatformImpl();

    /**
     * @brief Check if cache should be refreshed
     * @param forceRefresh Force refresh flag
     * @return True if cache should be refreshed
     */
    bool shouldRefreshCache(bool forceRefresh) const;

    /**
     * @brief Update cache timestamp
     */
    void updateCacheTimestamp();
};

/**
 * @brief Factory function to create SystemInfo instance
 * @param config Optional configuration
 * @return Unique pointer to SystemInfo instance
 */
std::unique_ptr<SystemInfo> createSystemInfo(const SystemInfoConfig& config = {});

/**
 * @brief Get quick system fingerprint without creating SystemInfo instance
 * @return System fingerprint string
 */
std::string getQuickSystemFingerprint();

/**
 * @brief Check if system information collection is available on current platform
 * @return True if available
 */
bool isSystemInfoAvailable();

/**
 * @brief Get platform-specific capabilities
 * @return Map of capability name to availability
 */
std::map<std::string, bool> getPlatformCapabilities();

/**
 * @brief Utility functions for system information
 */
namespace SystemInfoHelpers {
    /**
     * @brief Compare two system fingerprints
     * @param fingerprint1 First fingerprint
     * @param fingerprint2 Second fingerprint
     * @return Similarity score (0.0 to 1.0)
     */
    double compareFingerprints(const std::string& fingerprint1, const std::string& fingerprint2);

    /**
     * @brief Anonymize system information for privacy
     * @param info System information to anonymize
     * @return Anonymized system information
     */
    ComprehensiveSystemInfo anonymizeSystemInfo(const ComprehensiveSystemInfo& info);

    /**
     * @brief Generate hardware-based license key
     * @param info System information
     * @param salt Optional salt for key generation
     * @return Hardware-based license key
     */
    std::string generateHardwareLicenseKey(const ComprehensiveSystemInfo& info, 
                                           const std::string& salt = "");

    /**
     * @brief Validate hardware-based license key
     * @param licenseKey License key to validate
     * @param info Current system information
     * @param salt Salt used for key generation
     * @return True if license key is valid for current system
     */
    bool validateHardwareLicenseKey(const std::string& licenseKey,
                                    const ComprehensiveSystemInfo& info,
                                    const std::string& salt = "");

    /**
     * @brief Get system change detection hash
     * @param info System information
     * @return Hash for detecting system changes
     */
    std::string getSystemChangeHash(const ComprehensiveSystemInfo& info);

    /**
     * @brief Detect significant system changes
     * @param oldHash Previous system hash
     * @param newHash Current system hash
     * @return True if significant changes detected
     */
    bool detectSystemChanges(const std::string& oldHash, const std::string& newHash);
}

} // namespace atom::system

#endif // ATOM_SYSINFO_SN_SN_HPP
