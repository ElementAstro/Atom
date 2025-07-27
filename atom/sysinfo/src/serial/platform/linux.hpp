#ifndef ATOM_SYSINFO_SN_LINUX_HPP
#define ATOM_SYSINFO_SN_LINUX_HPP

#ifndef _WIN32

#include "common.hpp"
#include <memory>

namespace atom::system {

/**
 * @brief Linux-specific implementation for system information collection
 * 
 * This class provides Linux-specific methods for collecting hardware serial numbers
 * and system identification information using filesystem interfaces and system calls.
 */
class LinuxSystemInfo {
public:
    LinuxSystemInfo();
    ~LinuxSystemInfo() = default;

    // Disable copy constructor and assignment operator
    LinuxSystemInfo(const LinuxSystemInfo&) = delete;
    LinuxSystemInfo& operator=(const LinuxSystemInfo&) = delete;

    // Enable move constructor and assignment operator
    LinuxSystemInfo(LinuxSystemInfo&& other) noexcept = default;
    LinuxSystemInfo& operator=(LinuxSystemInfo&& other) noexcept = default;

    /**
     * @brief Get hardware serial numbers
     * @return Result containing hardware serial data
     */
    SystemInfoResult<HardwareSerialData> getHardwareSerials();

    /**
     * @brief Get system identification information
     * @return Result containing system identification data
     */
    SystemInfoResult<SystemIdentificationData> getSystemIdentification();

    /**
     * @brief Get memory module information
     * @return Result containing vector of memory module info
     */
    SystemInfoResult<std::vector<MemoryModuleInfo>> getMemoryModules();

    /**
     * @brief Get network interface information
     * @return Result containing vector of network interface info
     */
    SystemInfoResult<std::vector<NetworkInterfaceInfo>> getNetworkInterfaces();

    /**
     * @brief Get comprehensive system information
     * @param config Configuration options for data collection
     * @return Result containing comprehensive system info
     */
    SystemInfoResult<ComprehensiveSystemInfo> getComprehensiveInfo(const SystemInfoConfig& config = {});

    /**
     * @brief Query specific system identifier
     * @param type Type of system identifier to query
     * @return Result containing system identifier query result
     */
    SystemInfoResult<SystemIdQuery> querySystemId(SystemIdType type);

    /**
     * @brief Check if DMI information is available
     * @return True if DMI can be accessed
     */
    bool isDmiAvailable() const;

    /**
     * @brief Get last error message
     * @return Last error message from system operations
     */
    std::string getLastError() const;

private:
    std::string lastError_;

    /**
     * @brief Read content from a file
     * @param path File path to read
     * @param key Optional key to search for in the file
     * @return File content or value associated with key
     */
    std::string readFile(const std::string& path, const std::string& key = "") const;

    /**
     * @brief Read all lines from a file
     * @param path File path to read
     * @return Vector of lines from the file
     */
    std::vector<std::string> readFileLines(const std::string& path) const;

    /**
     * @brief Execute a command and get output
     * @param command Command to execute
     * @return Command output as string
     */
    std::string executeCommand(const std::string& command) const;

    /**
     * @brief Parse key-value pairs from text
     * @param text Text containing key-value pairs
     * @param delimiter Delimiter between key and value (default: ":")
     * @return Map of key-value pairs
     */
    std::map<std::string, std::string> parseKeyValuePairs(const std::string& text, 
                                                           const std::string& delimiter = ":") const;

    /**
     * @brief Get DMI information
     * @param dmiType DMI type identifier
     * @param field Field name to retrieve
     * @return DMI field value
     */
    std::string getDmiInfo(const std::string& dmiType, const std::string& field) const;

    /**
     * @brief Get system UUID from DMI
     * @return System UUID string
     */
    std::string getSystemUuid() const;

    /**
     * @brief Get machine ID from systemd
     * @return Machine ID string
     */
    std::string getMachineId() const;

    /**
     * @brief Get boot ID from proc
     * @return Boot ID string
     */
    std::string getBootId() const;

    /**
     * @brief Get hostname
     * @return Hostname string
     */
    std::string getHostname() const;

    /**
     * @brief Get domain name
     * @return Domain name string
     */
    std::string getDomainName() const;

    /**
     * @brief Get network interface MAC addresses
     * @return Vector of MAC addresses
     */
    std::vector<std::string> getNetworkMacAddresses() const;

    /**
     * @brief Get disk serial numbers from various sources
     * @return Vector of disk serial numbers
     */
    std::vector<std::string> getDiskSerials() const;

    /**
     * @brief Get memory module information from DMI
     * @return Vector of memory module info
     */
    std::vector<MemoryModuleInfo> getMemoryModulesFromDmi() const;

    /**
     * @brief Get memory module information from /proc/meminfo
     * @return Basic memory information
     */
    MemoryModuleInfo getMemoryInfoFromProc() const;

    /**
     * @brief Get network interfaces from /sys/class/net
     * @return Vector of network interface info
     */
    std::vector<NetworkInterfaceInfo> getNetworkInterfacesFromSys() const;

    /**
     * @brief Get network interface details
     * @param interfaceName Name of the network interface
     * @return Network interface info
     */
    NetworkInterfaceInfo getNetworkInterfaceDetails(const std::string& interfaceName) const;

    /**
     * @brief Parse memory size from string
     * @param sizeStr Size string with unit (e.g., "8192 MB")
     * @return Size in bytes
     */
    uint64_t parseMemorySize(const std::string& sizeStr) const;

    /**
     * @brief Parse memory speed from string
     * @param speedStr Speed string (e.g., "3200 MHz")
     * @return Speed in MHz
     */
    uint32_t parseMemorySpeed(const std::string& speedStr) const;

    /**
     * @brief Validate and clean serial number
     * @param serial Raw serial number
     * @return Cleaned serial number or empty string if invalid
     */
    std::string validateSerial(const std::string& serial) const;

    /**
     * @brief Check if file exists and is readable
     * @param path File path to check
     * @return True if file exists and is readable
     */
    bool isFileReadable(const std::string& path) const;

    /**
     * @brief Check if directory exists
     * @param path Directory path to check
     * @return True if directory exists
     */
    bool isDirectoryExists(const std::string& path) const;

    /**
     * @brief Trim whitespace from string
     * @param str String to trim
     * @return Trimmed string
     */
    std::string trim(const std::string& str) const;

    /**
     * @brief Convert string to lowercase
     * @param str String to convert
     * @return Lowercase string
     */
    std::string toLower(const std::string& str) const;

    /**
     * @brief Split string by delimiter
     * @param str String to split
     * @param delimiter Delimiter character
     * @return Vector of split strings
     */
    std::vector<std::string> split(const std::string& str, char delimiter) const;

    /**
     * @brief Handle system errors
     * @param operation Description of the operation that failed
     * @param errorCode System error code (errno)
     * @return SystemInfoError enum value
     */
    SystemInfoError handleSystemError(const std::string& operation, int errorCode = 0);
};

/**
 * @brief Factory function to create Linux system info instance
 * @return Unique pointer to LinuxSystemInfo instance
 */
std::unique_ptr<LinuxSystemInfo> createLinuxSystemInfo();

} // namespace atom::system

#endif // !_WIN32

#endif // ATOM_SYSINFO_SN_LINUX_HPP
