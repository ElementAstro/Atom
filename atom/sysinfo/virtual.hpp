/**
 * @file virtual.hpp
 * @brief Virtualization and container detection functionality (Compatibility Layer)
 *
 * This file provides backward compatibility for the original virtual.hpp API
 * while internally using the new modular virtual detection system.
 *
 * For new code, consider using the enhanced API in virtual/virtual.hpp
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_HPP
#define ATOM_SYSINFO_VIRTUAL_HPP

#include <string>

// Include the new modular virtual detection system
#include "src/virtual/virtual.hpp"

namespace atom::system {

/**
 * @brief Retrieves the vendor information of the hypervisor
 *
 * This function uses CPUID instruction to get the hypervisor vendor string.
 * Common vendors include VMware, VirtualBox, Hyper-V, KVM, and others.
 *
 * @return std::string The vendor string of the hypervisor, or empty if no
 * hypervisor is detected
 */
inline auto getHypervisorVendor() -> std::string {
    return virtual_env::hypervisor::getHypervisorVendor();
}

/**
 * @brief Detects if the system is running inside a virtual machine
 *
 * This function uses various techniques, including CPUID instruction, to
 * determine if the current system is a virtual machine.
 *
 * @return bool True if the system is a virtual machine, false otherwise
 */
inline auto isVirtualMachine() -> bool {
    return virtual_env::isVirtualEnvironment();
}

/**
 * @brief Checks BIOS information to identify if the system is a virtual machine
 *
 * This function inspects the BIOS information for signs that indicate the
 * presence of a virtual machine, such as manufacturer strings, version
 * information, and other BIOS characteristics commonly associated with
 * virtualization.
 *
 * @return bool True if the BIOS information suggests a virtual machine, false
 * otherwise
 */
inline auto checkBIOS() -> bool {
    return virtual_env::detection::bios::checkBIOSInfo();
}

/**
 * @brief Checks the network adapter for common virtual machine adapters
 *
 * This function looks for network adapters that are commonly used by virtual
 * machines, such as "VMware Virtual Ethernet Adapter", "VirtualBox Host-Only
 * Adapter", or other virtualization-specific MAC address prefixes.
 *
 * @return bool True if a virtual machine network adapter is found, false
 * otherwise
 */
inline auto checkNetworkAdapter() -> bool {
    return virtual_env::detection::hardware::checkNetworkAdapters();
}

/**
 * @brief Checks disk information for identifiers commonly used by virtual
 * machines
 *
 * This function inspects the disk information to find identifiers that are
 * typically associated with virtual machine disks, such as specific model
 * names, serial numbers, or device paths that indicate virtualized storage.
 *
 * @return bool True if virtual machine disk identifiers are found, false
 * otherwise
 */
inline auto checkDisk() -> bool {
    return virtual_env::detection::hardware::checkDiskInfo();
}

/**
 * @brief Checks the graphics card device for signs of virtualization
 *
 * This function examines the graphics card device to determine if it is a type
 * commonly used by virtual machines, such as virtual GPU adapters or basic
 * display adapters with limited capabilities typical of virtualized
 * environments.
 *
 * @return bool True if a virtual machine graphics card is detected, false
 * otherwise
 */
inline auto checkGraphicsCard() -> bool {
    return virtual_env::detection::hardware::checkGraphicsCard();
}

/**
 * @brief Checks for the presence of common virtual machine processes
 *
 * This function scans the system processes to identify any that are typically
 * associated with virtual machines, such as VMware Tools, VirtualBox Guest
 * Additions, or other virtualization management services.
 *
 * @return bool True if virtual machine processes are found, false otherwise
 */
inline auto checkProcesses() -> bool {
    return virtual_env::detection::processes::checkVirtualizationProcesses();
}

/**
 * @brief Checks PCI bus devices for virtualization indicators
 *
 * This function inspects the PCI bus devices to see if any of them are known
 * to be used by virtual machines, including specific vendor IDs and device IDs
 * that are associated with virtualization platforms.
 *
 * @return bool True if virtual machine PCI bus devices are found, false
 * otherwise
 */
inline auto checkPCIBus() -> bool {
    return virtual_env::detection::hardware::checkPCIBus();
}

/**
 * @brief Detects time drift and offset issues that may indicate a virtual
 * machine
 *
 * This function checks for irregularities in system time management, which can
 * be a sign of running inside a virtual machine. It measures timing
 * discrepancies between different clock sources that often occur in virtualized
 * environments.
 *
 * @return bool True if time drift or offset issues are detected, false
 * otherwise
 */
inline auto checkTimeDrift() -> bool {
    return virtual_env::detection::timing::checkTimeDrift();
}

/**
 * @brief Detects if the system is running inside a Docker container
 *
 * This function examines system characteristics specific to Docker
 * containerization, such as checking for the presence of /.dockerenv file,
 * cgroup configurations, and container-specific environment variables.
 *
 * @return bool True if running in a Docker container, false otherwise
 */
inline auto isDockerContainer() -> bool {
    return virtual_env::container::docker::detect();
}

/**
 * @brief Comprehensive virtualization detection with confidence score
 *
 * Combines multiple detection methods to provide a more accurate assessment
 * of whether the system is running in a virtualized environment. The function
 * weighs different indicators based on their reliability to calculate an
 * overall confidence score.
 *
 * @return double Confidence score between 0.0 and 1.0, with higher values
 *                indicating greater likelihood of virtualization
 */
inline auto getVirtualizationConfidence() -> double {
    virtual_env::VirtualizationDetector detector;
    return detector.getConfidenceScore();
}

/**
 * @brief Detects the specific type of virtualization technology in use
 *
 * This function attempts to identify the specific virtualization platform
 * or technology being used, such as VMware, VirtualBox, Hyper-V, KVM/QEMU,
 * Xen, or others.
 *
 * @return std::string Name of the detected virtualization technology, or
 *                     "Unknown" if the type cannot be determined
 */
inline auto getVirtualizationType() -> std::string {
    return virtual_env::getVirtualizationType();
}

/**
 * @brief Detects if the system is running inside a container
 *
 * This function checks for various container technologies including Docker,
 * LXC/LXD, Kubernetes pods, and other containerization solutions.
 *
 * @return bool True if running in any type of container, false otherwise
 */
inline auto isContainer() -> bool {
    return virtual_env::isContainerEnvironment();
}

/**
 * @brief Gets the container type if running in a containerized environment
 *
 * If the system is running in a container, this function attempts to identify
 * the specific container technology in use.
 *
 * @return std::string Name of the container technology (e.g., "Docker", "LXC"),
 *                     or empty string if not in a container
 */
inline auto getContainerType() -> std::string {
    return virtual_env::getContainerType();
}

}  // namespace atom::system

#endif  // ATOM_SYSINFO_VIRTUAL_HPP
