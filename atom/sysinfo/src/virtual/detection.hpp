/**
 * @file detection.hpp
 * @brief Core virtualization detection methods
 *
 * This file contains the core detection methods for identifying
 * virtualization environments through various system characteristics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_DETECTION_HPP
#define ATOM_SYSINFO_VIRTUAL_DETECTION_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace atom::system::virtual_env::detection {

/**
 * @brief CPUID-based detection methods
 */
namespace cpuid {
    /**
     * @brief Check if hypervisor bit is set in CPUID
     * @return bool True if hypervisor present
     */
    auto isHypervisorPresent() -> bool;

    /**
     * @brief Get hypervisor vendor string from CPUID
     * @return std::string Hypervisor vendor or empty string
     */
    auto getHypervisorVendor() -> std::string;

    /**
     * @brief Get CPU vendor string
     * @return std::string CPU vendor
     */
    auto getCPUVendor() -> std::string;

    /**
     * @brief Check for virtualization-specific CPU features
     * @return std::vector<std::string> List of detected features
     */
    auto getVirtualizationFeatures() -> std::vector<std::string>;
}

/**
 * @brief BIOS/UEFI-based detection methods
 */
namespace bios {
    /**
     * @brief Check BIOS information for virtualization signs
     * @return bool True if virtualization indicators found
     */
    auto checkBIOSInfo() -> bool;

    /**
     * @brief Get BIOS manufacturer information
     * @return std::string BIOS manufacturer
     */
    auto getBIOSManufacturer() -> std::string;

    /**
     * @brief Get system manufacturer information
     * @return std::string System manufacturer
     */
    auto getSystemManufacturer() -> std::string;

    /**
     * @brief Get product name from BIOS
     * @return std::string Product name
     */
    auto getProductName() -> std::string;

    /**
     * @brief Get BIOS version information
     * @return std::string BIOS version
     */
    auto getBIOSVersion() -> std::string;
}

/**
 * @brief Hardware-based detection methods
 */
namespace hardware {
    /**
     * @brief Check network adapters for virtualization indicators
     * @return bool True if virtual network adapters found
     */
    auto checkNetworkAdapters() -> bool;

    /**
     * @brief Get network adapter information
     * @return std::vector<std::string> List of network adapters
     */
    auto getNetworkAdapters() -> std::vector<std::string>;

    /**
     * @brief Check disk information for virtualization signs
     * @return bool True if virtual disks detected
     */
    auto checkDiskInfo() -> bool;

    /**
     * @brief Get disk information
     * @return std::vector<std::string> List of disk devices
     */
    auto getDiskInfo() -> std::vector<std::string>;

    /**
     * @brief Check graphics card for virtualization indicators
     * @return bool True if virtual graphics detected
     */
    auto checkGraphicsCard() -> bool;

    /**
     * @brief Get graphics card information
     * @return std::vector<std::string> List of graphics devices
     */
    auto getGraphicsInfo() -> std::vector<std::string>;

    /**
     * @brief Check PCI bus for virtualization devices
     * @return bool True if virtual PCI devices found
     */
    auto checkPCIBus() -> bool;

    /**
     * @brief Get PCI device information
     * @return std::vector<std::string> List of PCI devices
     */
    auto getPCIDevices() -> std::vector<std::string>;

    /**
     * @brief Check USB devices for virtualization indicators
     * @return bool True if virtual USB devices found
     */
    auto checkUSBDevices() -> bool;

    /**
     * @brief Get USB device information
     * @return std::vector<std::string> List of USB devices
     */
    auto getUSBDevices() -> std::vector<std::string>;
}

/**
 * @brief Process and service-based detection methods
 */
namespace processes {
    /**
     * @brief Check for virtualization-related processes
     * @return bool True if VM processes found
     */
    auto checkVirtualizationProcesses() -> bool;

    /**
     * @brief Get list of running processes
     * @return std::vector<std::string> List of process names
     */
    auto getRunningProcesses() -> std::vector<std::string>;

    /**
     * @brief Check for virtualization services
     * @return bool True if VM services found
     */
    auto checkVirtualizationServices() -> bool;

    /**
     * @brief Get list of running services
     * @return std::vector<std::string> List of service names
     */
    auto getRunningServices() -> std::vector<std::string>;

    /**
     * @brief Check for virtualization drivers
     * @return bool True if VM drivers found
     */
    auto checkVirtualizationDrivers() -> bool;

    /**
     * @brief Get list of loaded drivers
     * @return std::vector<std::string> List of driver names
     */
    auto getLoadedDrivers() -> std::vector<std::string>;
}

/**
 * @brief Timing and performance-based detection methods
 */
namespace timing {
    /**
     * @brief Check for time drift anomalies
     * @return bool True if time drift detected
     */
    auto checkTimeDrift() -> bool;

    /**
     * @brief Measure instruction timing for VM detection
     * @return bool True if VM timing patterns detected
     */
    auto checkInstructionTiming() -> bool;

    /**
     * @brief Check CPU performance characteristics
     * @return bool True if VM performance patterns detected
     */
    auto checkPerformanceCharacteristics() -> bool;

    /**
     * @brief Measure memory access patterns
     * @return bool True if VM memory patterns detected
     */
    auto checkMemoryAccessPatterns() -> bool;
}

/**
 * @brief Registry and configuration-based detection (Windows)
 */
namespace registry {
    /**
     * @brief Check Windows registry for virtualization indicators
     * @return bool True if VM registry entries found
     */
    auto checkVirtualizationRegistry() -> bool;

    /**
     * @brief Get virtualization-related registry values
     * @return std::vector<std::string> List of registry indicators
     */
    auto getVirtualizationRegistryEntries() -> std::vector<std::string>;

    /**
     * @brief Check for VM-specific registry keys
     * @return bool True if VM registry keys found
     */
    auto checkVMRegistryKeys() -> bool;
}

/**
 * @brief File system-based detection
 */
namespace filesystem {
    /**
     * @brief Check for virtualization-specific files
     * @return bool True if VM files found
     */
    auto checkVirtualizationFiles() -> bool;

    /**
     * @brief Get list of virtualization indicator files
     * @return std::vector<std::string> List of file paths
     */
    auto getVirtualizationFiles() -> std::vector<std::string>;

    /**
     * @brief Check for VM-specific mount points
     * @return bool True if VM mount points found
     */
    auto checkVirtualizationMounts() -> bool;

    /**
     * @brief Get virtualization-related mount information
     * @return std::vector<std::string> List of mount points
     */
    auto getVirtualizationMounts() -> std::vector<std::string>;
}

/**
 * @brief Environment variable-based detection
 */
namespace environment {
    /**
     * @brief Check environment variables for virtualization indicators
     * @return bool True if VM environment variables found
     */
    auto checkVirtualizationEnvironment() -> bool;

    /**
     * @brief Get virtualization-related environment variables
     * @return std::vector<std::string> List of environment variables
     */
    auto getVirtualizationEnvironmentVars() -> std::vector<std::string>;
}

} // namespace atom::system::virtual_env::detection

#endif // ATOM_SYSINFO_VIRTUAL_DETECTION_HPP
