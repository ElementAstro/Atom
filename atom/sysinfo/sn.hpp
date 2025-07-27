#ifndef ATOM_SYSINFO_SN_HPP
#define ATOM_SYSINFO_SN_HPP

// Include the new modular system information interface (renamed to serial)
#include "src/serial/sn.hpp"

#include <string>
#include <vector>
#include <memory>

/**
 * @brief Hardware information class that provides access to system hardware
 * serial numbers
 *
 * This class maintains backward compatibility with the original API while
 * internally using the new enhanced modular system information framework.
 * It supports both Windows (via WMI) and Linux (via filesystem) platforms.
 *
 * @deprecated This class is maintained for backward compatibility.
 * New code should use atom::system::SystemInfo directly for enhanced features.
 */
class HardwareInfo {
public:
    HardwareInfo();
    ~HardwareInfo();

    // Copy constructor
    HardwareInfo(const HardwareInfo& other);

    // Copy assignment operator
    HardwareInfo& operator=(const HardwareInfo& other);

    // Move constructor
    HardwareInfo(HardwareInfo&& other) noexcept;

    // Move assignment operator
    HardwareInfo& operator=(HardwareInfo&& other) noexcept;

    /**
     * @brief Get BIOS serial number
     * @return BIOS serial number as string
     */
    auto getBiosSerialNumber() -> std::string;

    /**
     * @brief Get motherboard serial number
     * @return Motherboard serial number as string
     */
    auto getMotherboardSerialNumber() -> std::string;

    /**
     * @brief Get CPU serial number
     * @return CPU serial number as string
     */
    auto getCpuSerialNumber() -> std::string;

    /**
     * @brief Get disk serial numbers
     * @return Vector of disk serial numbers
     */
    auto getDiskSerialNumbers() -> std::vector<std::string>;

    /**
     * @brief Get enhanced system information (new feature)
     * @return Pointer to enhanced system info, or nullptr if not available
     */
    auto getEnhancedSystemInfo() -> atom::system::SystemInfo*;

    /**
     * @brief Get system fingerprint (new feature)
     * @return Unique system fingerprint string
     */
    auto getSystemFingerprint() -> std::string;

    /**
     * @brief Check if enhanced features are available (new feature)
     * @return True if enhanced features are supported
     */
    auto isEnhancedModeAvailable() -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Type aliases for backward compatibility
using SystemHardwareInfo = HardwareInfo;
using HwInfo = HardwareInfo;

// Convenience functions for quick access (new features)
namespace HardwareInfoUtils {
    /**
     * @brief Get quick system fingerprint without creating HardwareInfo instance
     * @return System fingerprint string
     */
    std::string getQuickFingerprint();

    /**
     * @brief Check if system information is available
     * @return True if system information can be collected
     */
    bool isAvailable();

    /**
     * @brief Get basic hardware serials as a formatted string
     * @return Formatted string with all available serial numbers
     */
    std::string getFormattedSerials();
}

#endif  // ATOM_SYSINFO_SN_HPP
