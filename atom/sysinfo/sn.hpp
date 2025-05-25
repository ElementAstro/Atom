#ifndef ATOM_SYSINFO_SN_HPP
#define ATOM_SYSINFO_SN_HPP

#include <string>
#include <vector>

/**
 * @brief Hardware information class that provides access to system hardware
 * serial numbers
 *
 * This class uses the PIMPL idiom to hide platform-specific implementation
 * details. It supports both Windows (via WMI) and Linux (via filesystem)
 * platforms.
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

private:
    class Impl;
    Impl* impl_;
};

#endif  // ATOM_SYSINFO_SN_HPP
