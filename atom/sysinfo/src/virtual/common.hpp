/**
 * @file common.hpp
 * @brief Common utilities and definitions for virtualization detection
 *
 * This file contains common utilities, constants, and helper functions
 * used across the virtualization detection module.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_COMMON_HPP
#define ATOM_SYSINFO_VIRTUAL_COMMON_HPP

#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace atom::system::virtual_env {

/**
 * @brief Constants used in virtualization detection
 */
namespace constants {
    // CPUID constants
    constexpr int CPUID_HYPERVISOR = 0x40000000;
    constexpr int CPUID_FEATURES = 1;
    constexpr int VENDOR_STRING_LENGTH = 12;
    constexpr int BIOS_INFO_LENGTH = 256;
    constexpr int HYPERVISOR_PRESENT_BIT = 31;
    
    // Time drift detection constants
    constexpr int TIME_DRIFT_UPPER_BOUND = 1050;
    constexpr int TIME_DRIFT_LOWER_BOUND = 950;
    
    // Detection weights
    constexpr double CPUID_WEIGHT = 0.25;
    constexpr double BIOS_WEIGHT = 0.20;
    constexpr double NETWORK_WEIGHT = 0.10;
    constexpr double DISK_WEIGHT = 0.15;
    constexpr double GRAPHICS_WEIGHT = 0.10;
    constexpr double PROCESS_WEIGHT = 0.05;
    constexpr double PCI_WEIGHT = 0.10;
    constexpr double TIME_DRIFT_WEIGHT = 0.05;
}

/**
 * @brief Known virtualization keywords for detection
 */
namespace keywords {
    constexpr std::array<std::string_view, 12> VM_KEYWORDS = {
        "VMware", "VirtualBox", "QEMU", "Xen", "KVM", "Hyper-V", 
        "Parallels", "VirtIO", "Virtual", "vbox", "vmware", "qemu"
    };
    
    constexpr std::array<std::string_view, 8> CONTAINER_KEYWORDS = {
        "docker", "lxc", "kubepods", "containerd", "podman", 
        "systemd-nspawn", "rkt", "garden"
    };
    
    constexpr std::array<std::string_view, 6> CLOUD_KEYWORDS = {
        "amazon", "google", "microsoft", "azure", "aws", "gcp"
    };
}

/**
 * @brief Detection method information
 */
struct DetectionMethod {
    std::string name;
    std::function<bool()> detector;
    double weight;
    bool enabled;
    std::string description;
};

/**
 * @brief Detection result for a single method
 */
struct DetectionResult {
    std::string method_name;
    bool detected;
    double confidence;
    std::string details;
    std::vector<std::string> indicators;
};

/**
 * @brief Utility functions
 */

/**
 * @brief Execute a system command and return output
 * @param command Command to execute
 * @return std::string Command output or empty string on failure
 */
auto executeCommand(std::string_view command) -> std::string;

/**
 * @brief Check if text contains virtualization keywords
 * @param text Text to search
 * @param keywords Array of keywords to search for
 * @return bool True if any keyword is found
 */
template<size_t N>
auto containsKeywords(std::string_view text, 
                     const std::array<std::string_view, N>& keywords) -> bool;

/**
 * @brief Check if text contains VM-specific keywords
 * @param text Text to search
 * @return bool True if VM keywords found
 */
auto containsVMKeywords(std::string_view text) -> bool;

/**
 * @brief Check if text contains container-specific keywords
 * @param text Text to search
 * @return bool True if container keywords found
 */
auto containsContainerKeywords(std::string_view text) -> bool;

/**
 * @brief Check if text contains cloud provider keywords
 * @param text Text to search
 * @return bool True if cloud keywords found
 */
auto containsCloudKeywords(std::string_view text) -> bool;

/**
 * @brief Convert string to lowercase
 * @param str Input string
 * @return std::string Lowercase string
 */
auto toLowercase(std::string_view str) -> std::string;

/**
 * @brief Read file content as string
 * @param filepath Path to file
 * @return std::string File content or empty string on failure
 */
auto readFileContent(std::string_view filepath) -> std::string;

/**
 * @brief Check if file exists
 * @param filepath Path to file
 * @return bool True if file exists
 */
auto fileExists(std::string_view filepath) -> bool;

/**
 * @brief Get CPU information using CPUID
 * @param leaf CPUID leaf to query
 * @return std::array<unsigned int, 4> CPUID result
 */
auto getCPUInfo(unsigned int leaf) -> std::array<unsigned int, 4>;

/**
 * @brief Format detection results as human-readable string
 * @param results Vector of detection results
 * @return std::string Formatted report
 */
auto formatDetectionReport(const std::vector<DetectionResult>& results) -> std::string;

/**
 * @brief Calculate weighted confidence score
 * @param results Vector of detection results
 * @return double Confidence score between 0.0 and 1.0
 */
auto calculateConfidenceScore(const std::vector<DetectionResult>& results) -> double;

/**
 * @brief Platform detection utilities
 */
namespace platform {
    /**
     * @brief Check if running on Windows
     * @return bool True if Windows
     */
    auto isWindows() -> bool;
    
    /**
     * @brief Check if running on Linux
     * @return bool True if Linux
     */
    auto isLinux() -> bool;
    
    /**
     * @brief Check if running on macOS
     * @return bool True if macOS
     */
    auto isMacOS() -> bool;
    
    /**
     * @brief Get platform name
     * @return std::string Platform name
     */
    auto getPlatformName() -> std::string;
}

} // namespace atom::system::virtual_env

#endif // ATOM_SYSINFO_VIRTUAL_COMMON_HPP
