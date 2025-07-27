#ifndef ATOM_SYSINFO_SN_WINDOWS_HPP
#define ATOM_SYSINFO_SN_WINDOWS_HPP

#ifdef _WIN32

#include "common.hpp"
#include <memory>

namespace atom::system {

/**
 * @brief Windows-specific implementation for system information collection
 * 
 * This class provides Windows-specific methods for collecting hardware serial numbers
 * and system identification information using WMI (Windows Management Instrumentation).
 */
class WindowsSystemInfo {
public:
    WindowsSystemInfo();
    ~WindowsSystemInfo();

    // Disable copy constructor and assignment operator
    WindowsSystemInfo(const WindowsSystemInfo&) = delete;
    WindowsSystemInfo& operator=(const WindowsSystemInfo&) = delete;

    // Enable move constructor and assignment operator
    WindowsSystemInfo(WindowsSystemInfo&& other) noexcept;
    WindowsSystemInfo& operator=(WindowsSystemInfo&& other) noexcept;

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
     * @brief Check if WMI is available and accessible
     * @return True if WMI can be used
     */
    bool isWmiAvailable() const;

    /**
     * @brief Get last error message
     * @return Last error message from WMI operations
     */
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Initialize WMI components
     * @return True if initialization successful
     */
    bool initializeWmi();

    /**
     * @brief Cleanup WMI resources
     */
    void cleanupWmi();

    /**
     * @brief Get single WMI property value
     * @param wmiClass WMI class name
     * @param property Property name to retrieve
     * @return Property value as string
     */
    std::string getWmiProperty(const std::wstring& wmiClass, const std::wstring& property);

    /**
     * @brief Get multiple WMI property values
     * @param wmiClass WMI class name
     * @param property Property name to retrieve
     * @return Vector of property values
     */
    std::vector<std::string> getWmiPropertyMultiple(const std::wstring& wmiClass, const std::wstring& property);

    /**
     * @brief Get WMI object properties
     * @param wmiClass WMI class name
     * @param properties Vector of property names to retrieve
     * @return Map of property name to value
     */
    std::map<std::string, std::string> getWmiObjectProperties(const std::wstring& wmiClass, 
                                                               const std::vector<std::wstring>& properties);

    /**
     * @brief Get multiple WMI objects with properties
     * @param wmiClass WMI class name
     * @param properties Vector of property names to retrieve
     * @return Vector of maps containing property name to value mappings
     */
    std::vector<std::map<std::string, std::string>> getWmiObjectsProperties(const std::wstring& wmiClass,
                                                                             const std::vector<std::wstring>& properties);

    /**
     * @brief Convert BSTR to std::string
     * @param bstr BSTR to convert
     * @return Converted string
     */
    std::string bstrToString(const BSTR& bstr);

    /**
     * @brief Convert std::string to std::wstring
     * @param str String to convert
     * @return Converted wide string
     */
    std::wstring stringToWstring(const std::string& str);

    /**
     * @brief Parse memory size from string
     * @param sizeStr Size string (e.g., "8589934592")
     * @return Size in bytes
     */
    uint64_t parseMemorySize(const std::string& sizeStr);

    /**
     * @brief Parse memory speed from string
     * @param speedStr Speed string (e.g., "3200")
     * @return Speed in MHz
     */
    uint32_t parseMemorySpeed(const std::string& speedStr);

    /**
     * @brief Get network adapter MAC addresses
     * @return Vector of MAC addresses
     */
    std::vector<std::string> getNetworkMacAddresses();

    /**
     * @brief Get system UUID from WMI
     * @return System UUID string
     */
    std::string getSystemUuid();

    /**
     * @brief Get machine GUID from registry
     * @return Machine GUID string
     */
    std::string getMachineGuid();

    /**
     * @brief Get computer name
     * @return Computer name string
     */
    std::string getComputerName();

    /**
     * @brief Get domain name
     * @return Domain name string
     */
    std::string getDomainName();

    /**
     * @brief Validate and clean serial number
     * @param serial Raw serial number
     * @return Cleaned serial number or empty string if invalid
     */
    std::string validateSerial(const std::string& serial);

    /**
     * @brief Handle WMI errors
     * @param hresult HRESULT from WMI operation
     * @param operation Description of the operation that failed
     * @return SystemInfoError enum value
     */
    SystemInfoError handleWmiError(long hresult, const std::string& operation);
};

/**
 * @brief Factory function to create Windows system info instance
 * @return Unique pointer to WindowsSystemInfo instance
 */
std::unique_ptr<WindowsSystemInfo> createWindowsSystemInfo();

} // namespace atom::system

#endif // _WIN32

#endif // ATOM_SYSINFO_SN_WINDOWS_HPP
