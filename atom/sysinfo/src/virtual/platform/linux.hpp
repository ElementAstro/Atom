/**
 * @file linux.hpp
 * @brief Linux-specific virtualization detection
 *
 * This file contains Linux-specific implementations for detecting
 * virtualization and container environments.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_LINUX_HPP
#define ATOM_SYSINFO_VIRTUAL_LINUX_HPP

#include <string>
#include <vector>
#include <unordered_map>

namespace atom::system::virtual_env::platform::linux_impl {

/**
 * @brief Virtualization detection result for Linux
 */
struct VirtualizationResult {
    bool is_virtual = false;
    double confidence = 0.0;
    std::string platform;
    std::string virtualization_type;
    std::string container_type;
    std::vector<std::string> indicators;
};

/**
 * @brief Container information for Linux
 */
struct ContainerInfo {
    std::string type;
    std::string runtime;
    std::string id;
    std::string namespace_name;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Perform comprehensive Linux virtualization detection
 * @return VirtualizationResult Detection results
 */
auto detectVirtualizationLinux() -> VirtualizationResult;

/**
 * @brief Check /proc/cpuinfo for hypervisor flag
 * @return bool True if hypervisor flag found
 */
auto checkCPUInfoHypervisor() -> bool;

/**
 * @brief Get DMI (Desktop Management Interface) information
 * @return std::unordered_map<std::string, std::string> DMI key-value pairs
 */
auto getDMIInfo() -> std::unordered_map<std::string, std::string>;

/**
 * @brief Check for virtualization-specific files in Linux
 * @return std::vector<std::string> List of found virtualization files
 */
auto checkVirtualizationFiles() -> std::vector<std::string>;

/**
 * @brief Check cgroups for container indicators
 * @return std::string Container type detected from cgroups
 */
auto checkCgroups() -> std::string;

/**
 * @brief Check loaded kernel modules for virtualization indicators
 * @return std::vector<std::string> List of VM-related kernel modules
 */
auto checkKernelModules() -> std::vector<std::string>;

/**
 * @brief Check /proc/devices for virtual devices
 * @return std::vector<std::string> List of virtual devices
 */
auto checkVirtualDevices() -> std::vector<std::string>;

/**
 * @brief Check systemd for container environment
 * @return bool True if systemd container environment detected
 */
auto checkSystemdContainer() -> bool;

/**
 * @brief Get specific virtualization type on Linux
 * @return std::string Virtualization type
 */
auto getLinuxVirtualizationType() -> std::string;

/**
 * @brief Get detailed container information on Linux
 * @return ContainerInfo Container details
 */
auto getLinuxContainerInfo() -> ContainerInfo;

} // namespace atom::system::virtual_env::platform::linux_impl

#endif // ATOM_SYSINFO_VIRTUAL_LINUX_HPP
