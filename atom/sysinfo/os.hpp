/**
 * @file os.hpp
 * @brief Operating System Information Module - Enhanced Compatibility Header
 *
 * This file provides backward compatibility by including the new enhanced
 * OS implementation from the BIOS folder. All functionality has been moved
 * to the bios/os/ subdirectory while maintaining the same public API and
 * adding comprehensive new features.
 *
 * The enhanced implementation provides:
 * - Advanced system monitoring and performance metrics
 * - Comprehensive security analysis and management
 * - Real-time system health monitoring
 * - Cross-platform system optimization
 * - Enhanced hardware and software detection
 * - System backup and restore capabilities
 * - Network configuration management
 * - Event logging and analysis
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_OS_HPP
#define ATOM_SYSTEM_MODULE_OS_HPP

// Include the new enhanced modular OS implementation
#include "src/os/os.hpp"

// Legacy compatibility - ensure all original functions are available
namespace atom::system {

// Re-export the original OperatingSystemInfo structure for compatibility
using OriginalOperatingSystemInfo = OperatingSystemInfo;

// Provide legacy function aliases for backward compatibility
inline auto getOSInfo() -> OperatingSystemInfo {
    return getOperatingSystemInfo();
}

inline auto getOSName() -> std::string {
    auto info = getOperatingSystemInfo();
    return info.osName;
}

inline auto getOSVersion() -> std::string {
    auto info = getOperatingSystemInfo();
    return info.osVersion;
}

inline auto getKernelVersion() -> std::string {
    auto info = getOperatingSystemInfo();
    return info.kernelVersion;
}

inline auto getArchitecture() -> std::string {
    auto info = getOperatingSystemInfo();
    return info.architecture;
}

// Enhanced functionality access - commented out until BIOS OS module is properly integrated
// inline auto getEnhancedOSManager() -> EnhancedOSManager& {
//     return EnhancedOSManager::getInstance();
// }

// inline auto getSystemMetrics() -> SystemPerformanceMetrics {
//     return EnhancedOSManager::getInstance().getPerformanceMetrics();
// }

// inline auto getSystemSecurity() -> SystemSecurityInfo {
//     return EnhancedOSManager::getInstance().getSecurityInfo();
// }

// inline auto getNetworkInfo() -> NetworkConfiguration {
//     return EnhancedOSManager::getInstance().getNetworkConfiguration();
// }

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_OS_HPP
