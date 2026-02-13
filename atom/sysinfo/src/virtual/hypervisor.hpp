/**
 * @file hypervisor.hpp
 * @brief Hypervisor-specific detection and identification
 *
 * This file contains specialized detection methods for identifying
 * specific hypervisor types and their characteristics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_HYPERVISOR_HPP
#define ATOM_SYSINFO_VIRTUAL_HYPERVISOR_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace atom::system::virtual_env::hypervisor {

/**
 * @brief Hypervisor types
 */
enum class HypervisorType {
    UNKNOWN,
    VMWARE_WORKSTATION,
    VMWARE_ESXI,
    VIRTUALBOX,
    HYPER_V,
    KVM,
    QEMU,
    XEN,
    PARALLELS,
    DOCKER,
    LXC,
    KUBERNETES,
    CLOUD_HYPERVISOR,
    FIRECRACKER
};

/**
 * @brief Hypervisor information structure
 */
struct HypervisorInfo {
    HypervisorType type = HypervisorType::UNKNOWN;
    std::string name;
    std::string vendor;
    std::string version;
    std::vector<std::string> features;
    std::unordered_map<std::string, std::string> properties;
    double detection_confidence = 0.0;
};

/**
 * @brief VMware-specific detection
 */
namespace vmware {
    /**
     * @brief Detect VMware hypervisor
     * @return bool True if VMware detected
     */
    auto detect() -> bool;

    /**
     * @brief Get VMware version information
     * @return std::string VMware version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Check if running on VMware Workstation
     * @return bool True if VMware Workstation
     */
    auto isWorkstation() -> bool;

    /**
     * @brief Check if running on VMware ESXi
     * @return bool True if VMware ESXi
     */
    auto isESXi() -> bool;

    /**
     * @brief Get VMware tools status
     * @return bool True if VMware Tools installed
     */
    auto hasVMwareTools() -> bool;

    /**
     * @brief Get VMware-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;
}

/**
 * @brief VirtualBox-specific detection
 */
namespace virtualbox {
    /**
     * @brief Detect VirtualBox hypervisor
     * @return bool True if VirtualBox detected
     */
    auto detect() -> bool;

    /**
     * @brief Get VirtualBox version information
     * @return std::string VirtualBox version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Get Guest Additions status
     * @return bool True if Guest Additions installed
     */
    auto hasGuestAdditions() -> bool;

    /**
     * @brief Get VirtualBox-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;

    /**
     * @brief Check for VirtualBox-specific hardware
     * @return bool True if VirtualBox hardware detected
     */
    auto checkHardware() -> bool;
}

/**
 * @brief Hyper-V-specific detection
 */
namespace hyperv {
    /**
     * @brief Detect Hyper-V hypervisor
     * @return bool True if Hyper-V detected
     */
    auto detect() -> bool;

    /**
     * @brief Get Hyper-V version information
     * @return std::string Hyper-V version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Check for Hyper-V Integration Services
     * @return bool True if Integration Services installed
     */
    auto hasIntegrationServices() -> bool;

    /**
     * @brief Get Hyper-V generation (1 or 2)
     * @return int Generation number
     */
    auto getGeneration() -> int;

    /**
     * @brief Get Hyper-V-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;
}

/**
 * @brief KVM/QEMU-specific detection
 */
namespace kvm {
    /**
     * @brief Detect KVM hypervisor
     * @return bool True if KVM detected
     */
    auto detect() -> bool;

    /**
     * @brief Detect QEMU emulator
     * @return bool True if QEMU detected
     */
    auto detectQEMU() -> bool;

    /**
     * @brief Get KVM version information
     * @return std::string KVM version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Get QEMU version information
     * @return std::string QEMU version
     */
    auto getQEMUVersion() -> std::string;

    /**
     * @brief Check for QEMU Guest Agent
     * @return bool True if Guest Agent running
     */
    auto hasGuestAgent() -> bool;

    /**
     * @brief Get KVM/QEMU-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;

    /**
     * @brief Check for VirtIO devices
     * @return bool True if VirtIO devices found
     */
    auto hasVirtIODevices() -> bool;
}

/**
 * @brief Xen-specific detection
 */
namespace xen {
    /**
     * @brief Detect Xen hypervisor
     * @return bool True if Xen detected
     */
    auto detect() -> bool;

    /**
     * @brief Get Xen version information
     * @return std::string Xen version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Check if running as Xen PV guest
     * @return bool True if PV guest
     */
    auto isParavirtualized() -> bool;

    /**
     * @brief Check if running as Xen HVM guest
     * @return bool True if HVM guest
     */
    auto isHVM() -> bool;

    /**
     * @brief Get Xen-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;
}

/**
 * @brief Parallels-specific detection
 */
namespace parallels {
    /**
     * @brief Detect Parallels hypervisor
     * @return bool True if Parallels detected
     */
    auto detect() -> bool;

    /**
     * @brief Get Parallels version information
     * @return std::string Parallels version
     */
    auto getVersion() -> std::string;

    /**
     * @brief Check for Parallels Tools
     * @return bool True if Parallels Tools installed
     */
    auto hasParallelsTools() -> bool;

    /**
     * @brief Get Parallels-specific features
     * @return std::vector<std::string> List of features
     */
    auto getFeatures() -> std::vector<std::string>;
}

/**
 * @brief Cloud hypervisor detection
 */
namespace cloud {
    /**
     * @brief Detect cloud hypervisor
     * @return bool True if cloud hypervisor detected
     */
    auto detect() -> bool;

    /**
     * @brief Detect AWS Nitro hypervisor
     * @return bool True if AWS Nitro detected
     */
    auto detectAWSNitro() -> bool;

    /**
     * @brief Detect Google Cloud hypervisor
     * @return bool True if GCP hypervisor detected
     */
    auto detectGCPHypervisor() -> bool;

    /**
     * @brief Detect Azure hypervisor
     * @return bool True if Azure hypervisor detected
     */
    auto detectAzureHypervisor() -> bool;

    /**
     * @brief Get cloud provider information
     * @return std::string Cloud provider name
     */
    auto getCloudProvider() -> std::string;

    /**
     * @brief Get cloud instance metadata
     * @return std::unordered_map<std::string, std::string> Metadata
     */
    auto getCloudMetadata() -> std::unordered_map<std::string, std::string>;
}

/**
 * @brief General hypervisor detection and identification
 */

/**
 * @brief Detect hypervisor type
 * @return HypervisorType Detected hypervisor type
 */
auto detectHypervisorType() -> HypervisorType;

/**
 * @brief Get comprehensive hypervisor information
 * @return HypervisorInfo Complete hypervisor details
 */
auto getHypervisorInfo() -> HypervisorInfo;

/**
 * @brief Convert hypervisor type to string
 * @param type Hypervisor type
 * @return std::string String representation
 */
auto hypervisorTypeToString(HypervisorType type) -> std::string;

/**
 * @brief Get hypervisor vendor from CPUID
 * @return std::string Vendor string
 */
auto getHypervisorVendor() -> std::string;

/**
 * @brief Check if running in nested virtualization
 * @return bool True if nested virtualization detected
 */
auto isNestedVirtualization() -> bool;

/**
 * @brief Get virtualization capabilities
 * @return std::vector<std::string> List of capabilities
 */
auto getVirtualizationCapabilities() -> std::vector<std::string>;

} // namespace atom::system::virtual_env::hypervisor

#endif // ATOM_SYSINFO_VIRTUAL_HYPERVISOR_HPP
