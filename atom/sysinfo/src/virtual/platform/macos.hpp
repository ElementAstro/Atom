/**
 * @file macos.hpp
 * @brief macOS-specific virtualization detection
 *
 * This file contains macOS-specific implementations for detecting
 * virtualization environments using macOS APIs, sysctl, IOKit, and system profiler.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_MACOS_HPP
#define ATOM_SYSINFO_VIRTUAL_MACOS_HPP

#include <string>
#include <vector>

namespace atom::system::virtual_env::platform::macos_impl {

/**
 * @brief Virtualization detection result for macOS
 */
struct VirtualizationResult {
    bool is_virtual = false;
    double confidence = 0.0;
    std::string platform;
    std::string virtualization_type;
    std::vector<std::string> indicators;
};

/**
 * @brief Perform comprehensive macOS virtualization detection
 * @return VirtualizationResult Detection results
 */
auto detectVirtualizationMacOS() -> VirtualizationResult;

/**
 * @brief Check sysctl for virtualization indicators
 * @return std::vector<std::string> List of sysctl-based indicators
 */
auto checkSysctlVirtualization() -> std::vector<std::string>;

/**
 * @brief Check IOKit for hardware virtualization indicators
 * @return std::vector<std::string> List of IOKit-based indicators
 */
auto checkIOKitVirtualization() -> std::vector<std::string>;

/**
 * @brief Check for virtualization-related processes on macOS
 * @return std::vector<std::string> List of VM-related processes
 */
auto checkVirtualizationProcesses() -> std::vector<std::string>;

/**
 * @brief Check system profiler for virtualization indicators
 * @return std::vector<std::string> List of system profiler indicators
 */
auto checkSystemProfiler() -> std::vector<std::string>;

/**
 * @brief Check if Hypervisor framework is available
 * @return bool True if Hypervisor framework is available
 */
auto checkHypervisorFramework() -> bool;

/**
 * @brief Get specific virtualization type on macOS
 * @return std::string Virtualization type
 */
auto getMacOSVirtualizationType() -> std::string;

/**
 * @brief Check for containerization on macOS
 * @return bool True if containerization detected
 */
auto checkMacOSContainerization() -> bool;

} // namespace atom::system::virtual_env::platform::macos_impl

#endif // ATOM_SYSINFO_VIRTUAL_MACOS_HPP
