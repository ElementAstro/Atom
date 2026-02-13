#ifndef ATOM_SYSINFO_SN_COMMON_HPP
#define ATOM_SYSINFO_SN_COMMON_HPP

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Structure containing basic hardware serial number information
 */
struct HardwareSerialData {
    std::string biosSerial;
    std::string motherboardSerial;
    std::string cpuSerial;
    std::vector<std::string> diskSerials;
    std::chrono::system_clock::time_point lastUpdate;

    /**
     * @brief Check if hardware serial data is valid
     * @return True if at least one serial number is available
     */
    bool isValid() const;

    /**
     * @brief Convert hardware serial data to string representation
     * @return Formatted string containing all serial information
     */
    std::string toString() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Structure containing extended system identification information
 */
struct SystemIdentificationData {
    std::string systemUuid;
    std::string machineId;
    std::string bootId;
    std::vector<std::string> macAddresses;
    std::string hostname;
    std::string domainName;
    std::chrono::system_clock::time_point lastUpdate;

    /**
     * @brief Check if system identification data is valid
     * @return True if essential fields are populated
     */
    bool isValid() const;

    /**
     * @brief Convert system identification data to string representation
     * @return Formatted string containing all identification information
     */
    std::string toString() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Structure containing memory module information
 */
struct MemoryModuleInfo {
    std::string serialNumber;
    std::string manufacturer;
    std::string partNumber;
    std::string type;           // DDR4, DDR5, etc.
    uint64_t sizeBytes = 0;
    uint32_t speedMHz = 0;
    std::string formFactor;     // DIMM, SO-DIMM, etc.
    uint8_t slot = 0;

    /**
     * @brief Check if memory module info is valid
     * @return True if essential fields are populated
     */
    bool isValid() const;

    /**
     * @brief Convert memory module info to string representation
     * @return Formatted string containing all memory module information
     */
    std::string toString() const;
};

/**
 * @brief Structure containing network interface information
 */
struct NetworkInterfaceInfo {
    std::string name;
    std::string macAddress;
    std::string type;           // Ethernet, WiFi, etc.
    std::string manufacturer;
    std::string driver;
    bool isActive = false;
    std::vector<std::string> ipAddresses;

    /**
     * @brief Check if network interface info is valid
     * @return True if essential fields are populated
     */
    bool isValid() const;

    /**
     * @brief Convert network interface info to string representation
     * @return Formatted string containing all network interface information
     */
    std::string toString() const;
};

/**
 * @brief Structure containing comprehensive system information
 */
struct ComprehensiveSystemInfo {
    HardwareSerialData hardwareSerials;
    SystemIdentificationData systemId;
    std::vector<MemoryModuleInfo> memoryModules;
    std::vector<NetworkInterfaceInfo> networkInterfaces;
    std::map<std::string, std::string> additionalProperties;
    std::chrono::system_clock::time_point lastUpdate;

    /**
     * @brief Check if comprehensive system info is valid
     * @return True if essential data is available
     */
    bool isValid() const;

    /**
     * @brief Convert comprehensive system info to string representation
     * @return Formatted string containing all system information
     */
    std::string toString() const;

    /**
     * @brief Get a unique system fingerprint
     * @return Unique fingerprint string based on hardware characteristics
     */
    std::string getSystemFingerprint() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Enumeration for different types of system identifiers
 */
enum class SystemIdType {
    BIOS_SERIAL,
    MOTHERBOARD_SERIAL,
    CPU_SERIAL,
    SYSTEM_UUID,
    MACHINE_ID,
    MAC_ADDRESS,
    DISK_SERIAL,
    MEMORY_SERIAL
};

/**
 * @brief Structure for system identifier query results
 */
struct SystemIdQuery {
    SystemIdType type;
    std::string value;
    bool isAvailable = false;
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Configuration options for system information collection
 */
struct SystemInfoConfig {
    bool includeMemoryModules = true;
    bool includeNetworkInterfaces = true;
    bool includeDiskSerials = true;
    bool includeSystemUuid = true;
    bool includeMacAddresses = true;
    bool cacheResults = true;
    std::chrono::seconds cacheTimeout{300}; // 5 minutes default
    bool enableLogging = true;
};

/**
 * @brief Error codes for system information operations
 */
enum class SystemInfoError {
    SUCCESS = 0,
    ACCESS_DENIED,
    NOT_SUPPORTED,
    HARDWARE_NOT_FOUND,
    INVALID_DATA,
    TIMEOUT,
    UNKNOWN_ERROR
};

/**
 * @brief Result structure for system information operations
 */
template<typename T>
struct SystemInfoResult {
    T data;
    SystemInfoError error = SystemInfoError::SUCCESS;
    std::string errorMessage;
    bool success = true;

    /**
     * @brief Check if the operation was successful
     * @return True if operation succeeded
     */
    bool isSuccess() const { return success && error == SystemInfoError::SUCCESS; }

    /**
     * @brief Get error description
     * @return Human-readable error description
     */
    std::string getErrorDescription() const;
};

/**
 * @brief Utility functions for system information
 */
namespace SystemInfoUtils {
    /**
     * @brief Generate a hash from a string
     * @param input Input string to hash
     * @return Hash value as string
     */
    std::string generateHash(const std::string& input);

    /**
     * @brief Validate a serial number format
     * @param serial Serial number to validate
     * @return True if serial number appears valid
     */
    bool isValidSerial(const std::string& serial);

    /**
     * @brief Validate a MAC address format
     * @param macAddress MAC address to validate
     * @return True if MAC address format is valid
     */
    bool isValidMacAddress(const std::string& macAddress);

    /**
     * @brief Validate a UUID format
     * @param uuid UUID to validate
     * @return True if UUID format is valid
     */
    bool isValidUuid(const std::string& uuid);

    /**
     * @brief Format bytes to human-readable string
     * @param bytes Number of bytes
     * @return Formatted string (e.g., "8.0 GB")
     */
    std::string formatBytes(uint64_t bytes);

    /**
     * @brief Get current timestamp as string
     * @return Current timestamp in ISO 8601 format
     */
    std::string getCurrentTimestamp();
}

} // namespace atom::system

#endif // ATOM_SYSINFO_SN_COMMON_HPP
