#include "virtual.hpp"
#include "common.hpp"
#include "detection.hpp"
#include "hypervisor.hpp"
#include "container.hpp"

// Platform-specific includes
#ifdef _WIN32
#include "platform/windows.hpp"
#elif __linux__
#include "platform/linux.hpp"
#elif __APPLE__
#include "platform/macos.hpp"
#endif

#include <algorithm>
#include <memory>
#include <sstream>

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env {

// Implementation class for VirtualizationDetector
class VirtualizationDetector::Impl {
public:
    Impl() {
        initializeDetectionMethods();
    }

    auto detect() -> VirtualizationInfo {
        VirtualizationInfo info;
        std::vector<DetectionResult> results;

        spdlog::info("Starting comprehensive virtualization detection");

        // Run all enabled detection methods
        for (const auto& method : detectionMethods) {
            if (method.enabled) {
                try {
                    DetectionResult result;
                    result.method_name = method.name;
                    result.detected = method.detector();
                    result.confidence = result.detected ? method.weight : 0.0;
                    result.details = method.description;

                    results.push_back(result);
                    spdlog::debug("Detection method '{}': {}", method.name, result.detected ? "detected" : "not detected");
                } catch (const std::exception& e) {
                    spdlog::warn("Error in detection method '{}': {}", method.name, e.what());
                }
            }
        }

        // Calculate overall confidence
        info.confidence_score = calculateConfidenceScore(results);
        info.is_virtual = info.confidence_score > 0.3;

        // Get detailed information if virtualization is detected
        if (info.is_virtual) {
            info.hypervisor_vendor = hypervisor::getHypervisorVendor();
            info.virtualization_type = hypervisor::hypervisorTypeToString(hypervisor::detectHypervisorType());

            // Check for containers
            info.is_container = container::isContainer();
            if (info.is_container) {
                info.container_type = container::containerTypeToString(container::detectContainerType());
            }

            // Get cloud provider information
            auto cloudMetadata = hypervisor::cloud::getCloudMetadata();
            if (!cloudMetadata.empty() && cloudMetadata.count("provider")) {
                info.cloud_provider = cloudMetadata["provider"];
            }

            // Collect indicators from detection results
            for (const auto& result : results) {
                if (result.detected) {
                    info.indicators.push_back(result.method_name);
                    info.detection_methods[result.method_name] = true;
                } else {
                    info.detection_methods[result.method_name] = false;
                }
            }

            // Get hardware profile
            info.hardware_profile = getHardwareProfile();
        }

        spdlog::info("Virtualization detection completed. Confidence: {:.2f}, Virtual: {}",
                    info.confidence_score, info.is_virtual);

        return info;
    }

    auto isVirtual() -> bool {
        // Quick check using most reliable methods
        return detection::cpuid::isHypervisorPresent() ||
               detection::bios::checkBIOSInfo() ||
               container::isContainer();
    }

    auto isContainer() -> bool {
        return container::isContainer();
    }

    auto getConfidenceScore() -> double {
        auto info = detect();
        return info.confidence_score;
    }

    auto getDetectionReport() -> std::string {
        auto info = detect();

        std::ostringstream report;
        report << "=== Virtualization Detection Report ===\n\n";
        report << "Platform: " << platform::getPlatformName() << "\n";
        report << "Virtual Environment: " << (info.is_virtual ? "Yes" : "No") << "\n";
        report << "Confidence Score: " << (info.confidence_score * 100) << "%\n\n";

        if (info.is_virtual) {
            report << "Virtualization Details:\n";
            report << "  Type: " << info.virtualization_type << "\n";
            report << "  Vendor: " << info.hypervisor_vendor << "\n";

            if (info.is_container) {
                report << "  Container: " << info.container_type << "\n";
            }

            if (!info.cloud_provider.empty()) {
                report << "  Cloud Provider: " << info.cloud_provider << "\n";
            }

            if (!info.hardware_profile.empty()) {
                report << "  Hardware Profile: " << info.hardware_profile << "\n";
            }

            report << "\nDetection Methods:\n";
            for (const auto& [method, detected] : info.detection_methods) {
                report << "  " << method << ": " << (detected ? "✓" : "✗") << "\n";
            }

            if (!info.indicators.empty()) {
                report << "\nIndicators:\n";
                for (const auto& indicator : info.indicators) {
                    report << "  - " << indicator << "\n";
                }
            }
        }

        return report.str();
    }

    void setDetectionMethod(const std::string& method, bool enabled) {
        auto it = std::find_if(detectionMethods.begin(), detectionMethods.end(),
            [&method](const DetectionMethod& m) { return m.name == method; });

        if (it != detectionMethods.end()) {
            it->enabled = enabled;
            spdlog::debug("Detection method '{}' {}", method, enabled ? "enabled" : "disabled");
        } else {
            spdlog::warn("Unknown detection method: {}", method);
        }
    }

    auto getAvailableDetectionMethods() -> std::vector<std::string> {
        std::vector<std::string> methods;
        for (const auto& method : detectionMethods) {
            methods.push_back(method.name);
        }
        return methods;
    }

private:
    std::vector<DetectionMethod> detectionMethods;

    void initializeDetectionMethods() {
        detectionMethods = {
            {"CPUID", detection::cpuid::isHypervisorPresent, constants::CPUID_WEIGHT, true, "Check CPUID hypervisor bit"},
            {"BIOS", detection::bios::checkBIOSInfo, constants::BIOS_WEIGHT, true, "Check BIOS/UEFI information"},
            {"Network", detection::hardware::checkNetworkAdapters, constants::NETWORK_WEIGHT, true, "Check network adapters"},
            {"Disk", detection::hardware::checkDiskInfo, constants::DISK_WEIGHT, true, "Check disk information"},
            {"Graphics", detection::hardware::checkGraphicsCard, constants::GRAPHICS_WEIGHT, true, "Check graphics card"},
            {"Processes", detection::processes::checkVirtualizationProcesses, constants::PROCESS_WEIGHT, true, "Check VM processes"},
            {"PCI Bus", detection::hardware::checkPCIBus, constants::PCI_WEIGHT, true, "Check PCI bus devices"},
            {"Time Drift", detection::timing::checkTimeDrift, constants::TIME_DRIFT_WEIGHT, true, "Check time drift"},
            {"Container", container::isContainer, 0.3, true, "Check for container environment"},
            {"Registry", detection::registry::checkVirtualizationRegistry, 0.2, platform::isWindows(), "Check Windows registry"},
            {"Filesystem", detection::filesystem::checkVirtualizationFiles, 0.15, true, "Check filesystem indicators"},
            {"Environment", detection::environment::checkVirtualizationEnvironment, 0.1, true, "Check environment variables"}
        };

        spdlog::debug("Initialized {} detection methods", detectionMethods.size());
    }

    auto getHardwareProfile() -> std::string {
        // Platform-specific hardware profiling
#ifdef _WIN32
        auto wmiInfo = windows_impl::getWMISystemInfo();
        if (wmiInfo.count("Model")) {
            return wmiInfo["Model"];
        }
#elif __linux__
        auto dmiInfo = platform::linux_impl::getDMIInfo();
        if (dmiInfo.count("product_name")) {
            return dmiInfo["product_name"];
        }
#elif __APPLE__
        return macos_impl::getMacOSVirtualizationType();
#endif
        return "Unknown";
    }
};

// VirtualizationDetector implementation
VirtualizationDetector::VirtualizationDetector() : pImpl(std::make_unique<Impl>()) {}

auto VirtualizationDetector::detect() -> VirtualizationInfo {
    return pImpl->detect();
}

auto VirtualizationDetector::isVirtual() -> bool {
    return pImpl->isVirtual();
}

auto VirtualizationDetector::isContainer() -> bool {
    return pImpl->isContainer();
}

auto VirtualizationDetector::getConfidenceScore() -> double {
    return pImpl->getConfidenceScore();
}

auto VirtualizationDetector::getDetectionReport() -> std::string {
    return pImpl->getDetectionReport();
}

void VirtualizationDetector::setDetectionMethod(const std::string& method, bool enabled) {
    pImpl->setDetectionMethod(method, enabled);
}

auto VirtualizationDetector::getAvailableDetectionMethods() -> std::vector<std::string> {
    return pImpl->getAvailableDetectionMethods();
}

// Global convenience functions
auto isVirtualEnvironment() -> bool {
    VirtualizationDetector detector;
    return detector.isVirtual();
}

auto isContainerEnvironment() -> bool {
    VirtualizationDetector detector;
    return detector.isContainer();
}

auto getVirtualizationType() -> std::string {
    auto hypervisorType = hypervisor::detectHypervisorType();
    return hypervisor::hypervisorTypeToString(hypervisorType);
}

auto getContainerType() -> std::string {
    auto containerType = container::detectContainerType();
    return container::containerTypeToString(containerType);
}

auto getVirtualizationInfo() -> VirtualizationInfo {
    VirtualizationDetector detector;
    return detector.detect();
}

} // namespace atom::system::virtual_env
