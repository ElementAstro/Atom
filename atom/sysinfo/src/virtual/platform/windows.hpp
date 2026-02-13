/**
 * @file windows.hpp
 * @brief Windows-specific virtualization detection
 *
 * This file contains Windows-specific implementations for detecting
 * virtualization environments using Windows APIs, registry, WMI, and services.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_WINDOWS_HPP
#define ATOM_SYSINFO_VIRTUAL_WINDOWS_HPP

#include <string>
#include <vector>
#include <unordered_map>

namespace atom::system::virtual_env::platform::windows_impl {

/**
 * @brief Virtualization detection result for Windows
 */
struct VirtualizationResult {
    bool is_virtual = false;
    double confidence = 0.0;
    std::string platform;
    std::string virtualization_type;
    std::vector<std::string> indicators;
};

/**
 * @brief Perform comprehensive Windows virtualization detection
 * @return VirtualizationResult Detection results
 */
auto detectVirtualizationWindows() -> VirtualizationResult;

/**
 * @brief Check Windows registry for virtualization indicators
 * @return std::vector<std::string> List of registry-based indicators
 */
auto checkRegistryVirtualization() -> std::vector<std::string>;

/**
 * @brief Get system information using WMI (Windows Management Instrumentation)
 * @return std::unordered_map<std::string, std::string> WMI system information
 */
auto getWMISystemInfo() -> std::unordered_map<std::string, std::string>;

/**
 * @brief Check for virtualization-related Windows services
 * @return std::vector<std::string> List of VM-related services
 */
auto checkVirtualizationServices() -> std::vector<std::string>;

/**
 * @brief Check for virtualization-related processes on Windows
 * @return std::vector<std::string> List of VM-related processes
 */
auto checkVirtualizationProcesses() -> std::vector<std::string>;

/**
 * @brief Check hardware information for virtualization indicators
 * @return std::vector<std::string> List of hardware-based indicators
 */
auto checkHardwareVirtualization() -> std::vector<std::string>;

/**
 * @brief Get specific virtualization type on Windows
 * @return std::string Virtualization type
 */
auto getWindowsVirtualizationType() -> std::string;

} // namespace atom::system::virtual_env::platform::windows_impl

#endif // ATOM_SYSINFO_VIRTUAL_WINDOWS_HPP
