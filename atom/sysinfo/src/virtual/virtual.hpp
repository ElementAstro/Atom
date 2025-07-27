/**
 * @file virtual.hpp
 * @brief Main header for the virtual module - comprehensive virtualization detection
 *
 * This is the main header for the virtual module that provides comprehensive
 * virtualization and container detection capabilities. It includes all the
 * sub-modules and provides a unified interface.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_VIRTUAL_HPP
#define ATOM_SYSINFO_VIRTUAL_VIRTUAL_HPP

#include "common.hpp"
#include "detection.hpp"
#include "hypervisor.hpp"
#include "container.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace atom::system::virtual_env {

/**
 * @brief Comprehensive virtualization information structure
 */
struct VirtualizationInfo {
    bool is_virtual = false;
    bool is_container = false;
    std::string hypervisor_vendor;
    std::string virtualization_type;
    std::string container_type;
    double confidence_score = 0.0;
    std::unordered_map<std::string, bool> detection_methods;
    std::vector<std::string> indicators;
    std::string cloud_provider;
    std::string hardware_profile;
};

/**
 * @brief Main virtualization detection class
 *
 * This class provides a comprehensive interface for detecting and analyzing
 * virtualization environments. It combines multiple detection methods and
 * provides detailed information about the virtual environment.
 */
class VirtualizationDetector {
public:
    VirtualizationDetector();
    ~VirtualizationDetector() = default;

    // Disable copy constructor and assignment operator
    VirtualizationDetector(const VirtualizationDetector&) = delete;
    VirtualizationDetector& operator=(const VirtualizationDetector&) = delete;

    // Enable move constructor and assignment operator
    VirtualizationDetector(VirtualizationDetector&&) = default;
    VirtualizationDetector& operator=(VirtualizationDetector&&) = default;

    /**
     * @brief Perform comprehensive virtualization detection
     * @return VirtualizationInfo Complete virtualization information
     */
    auto detect() -> VirtualizationInfo;

    /**
     * @brief Quick check if running in virtual environment
     * @return bool True if virtual environment detected
     */
    auto isVirtual() -> bool;

    /**
     * @brief Quick check if running in container
     * @return bool True if container detected
     */
    auto isContainer() -> bool;

    /**
     * @brief Get confidence score for virtualization detection
     * @return double Confidence score between 0.0 and 1.0
     */
    auto getConfidenceScore() -> double;

    /**
     * @brief Get detailed detection report
     * @return std::string Human-readable detection report
     */
    auto getDetectionReport() -> std::string;

    /**
     * @brief Enable or disable specific detection methods
     * @param method Detection method name
     * @param enabled Whether to enable the method
     */
    void setDetectionMethod(const std::string& method, bool enabled);

    /**
     * @brief Get list of available detection methods
     * @return std::vector<std::string> List of method names
     */
    auto getAvailableDetectionMethods() -> std::vector<std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Global convenience functions for backward compatibility
 */

/**
 * @brief Quick virtualization check
 * @return bool True if running in virtual environment
 */
auto isVirtualEnvironment() -> bool;

/**
 * @brief Quick container check
 * @return bool True if running in container
 */
auto isContainerEnvironment() -> bool;

/**
 * @brief Get virtualization type
 * @return std::string Type of virtualization detected
 */
auto getVirtualizationType() -> std::string;

/**
 * @brief Get container type
 * @return std::string Type of container detected
 */
auto getContainerType() -> std::string;

/**
 * @brief Get comprehensive virtualization information
 * @return VirtualizationInfo Complete detection results
 */
auto getVirtualizationInfo() -> VirtualizationInfo;

} // namespace atom::system::virtual_env

#endif // ATOM_SYSINFO_VIRTUAL_VIRTUAL_HPP
