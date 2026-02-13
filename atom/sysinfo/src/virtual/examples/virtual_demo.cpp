#include <iostream>
#include <iomanip>
#include "../virtual.hpp"

using namespace atom::system::virtual_env;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void demonstrateBasicDetection() {
    printSeparator("Basic Virtualization Detection");

    VirtualizationDetector detector;

    // Quick checks
    std::cout << "Quick Detection Results:\n";
    std::cout << "  Virtual Environment: " << (detector.isVirtual() ? "Yes" : "No") << "\n";
    std::cout << "  Container Environment: " << (detector.isContainer() ? "Yes" : "No") << "\n";
    std::cout << "  Confidence Score: " << std::fixed << std::setprecision(2)
              << (detector.getConfidenceScore() * 100) << "%\n";
}

void demonstrateDetailedDetection() {
    printSeparator("Detailed Virtualization Analysis");

    VirtualizationDetector detector;
    auto info = detector.detect();

    std::cout << "Comprehensive Detection Results:\n";
    std::cout << "  Platform: " << platform::getPlatformName() << "\n";
    std::cout << "  Virtual Environment: " << (info.is_virtual ? "Yes" : "No") << "\n";
    std::cout << "  Container Environment: " << (info.is_container ? "Yes" : "No") << "\n";
    std::cout << "  Confidence Score: " << std::fixed << std::setprecision(2)
              << (info.confidence_score * 100) << "%\n";

    if (info.is_virtual) {
        std::cout << "\nVirtualization Details:\n";
        std::cout << "  Hypervisor Vendor: " << info.hypervisor_vendor << "\n";
        std::cout << "  Virtualization Type: " << info.virtualization_type << "\n";

        if (!info.cloud_provider.empty()) {
            std::cout << "  Cloud Provider: " << info.cloud_provider << "\n";
        }

        if (!info.hardware_profile.empty()) {
            std::cout << "  Hardware Profile: " << info.hardware_profile << "\n";
        }
    }

    if (info.is_container) {
        std::cout << "\nContainer Details:\n";
        std::cout << "  Container Type: " << info.container_type << "\n";
    }

    if (!info.indicators.empty()) {
        std::cout << "\nDetection Indicators:\n";
        for (const auto& indicator : info.indicators) {
            std::cout << "  - " << indicator << "\n";
        }
    }
}

void demonstrateHypervisorDetection() {
    printSeparator("Hypervisor-Specific Detection");

    auto hypervisorInfo = hypervisor::getHypervisorInfo();

    std::cout << "Hypervisor Information:\n";
    std::cout << "  Type: " << hypervisorInfo.name << "\n";
    std::cout << "  Vendor: " << hypervisorInfo.vendor << "\n";
    std::cout << "  Version: " << hypervisorInfo.version << "\n";
    std::cout << "  Detection Confidence: " << std::fixed << std::setprecision(2)
              << (hypervisorInfo.detection_confidence * 100) << "%\n";

    if (!hypervisorInfo.features.empty()) {
        std::cout << "\nHypervisor Features:\n";
        for (const auto& feature : hypervisorInfo.features) {
            std::cout << "  - " << feature << "\n";
        }
    }

    if (!hypervisorInfo.properties.empty()) {
        std::cout << "\nHypervisor Properties:\n";
        for (const auto& [key, value] : hypervisorInfo.properties) {
            std::cout << "  " << key << ": " << value << "\n";
        }
    }

    // Test specific hypervisor types
    std::cout << "\nSpecific Hypervisor Tests:\n";
    std::cout << "  VMware: " << (hypervisor::vmware::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  VirtualBox: " << (hypervisor::virtualbox::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  Hyper-V: " << (hypervisor::hyperv::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  KVM: " << (hypervisor::kvm::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  Xen: " << (hypervisor::xen::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  Cloud: " << (hypervisor::cloud::detect() ? "Detected" : "Not detected") << "\n";
}

void demonstrateContainerDetection() {
    printSeparator("Container-Specific Detection");

    auto containerInfo = container::getContainerInfo();

    std::cout << "Container Information:\n";
    std::cout << "  Type: " << containerInfo.name << "\n";
    std::cout << "  Runtime: " << container::containerRuntimeToString(containerInfo.runtime) << "\n";
    std::cout << "  ID: " << containerInfo.id << "\n";
    std::cout << "  Detection Confidence: " << std::fixed << std::setprecision(2)
              << (containerInfo.detection_confidence * 100) << "%\n";

    if (!containerInfo.metadata.empty()) {
        std::cout << "\nContainer Metadata:\n";
        for (const auto& [key, value] : containerInfo.metadata) {
            std::cout << "  " << key << ": " << value << "\n";
        }
    }

    // Test specific container types
    std::cout << "\nSpecific Container Tests:\n";
    std::cout << "  Docker: " << (container::docker::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  LXC: " << (container::lxc::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  Podman: " << (container::podman::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  Kubernetes: " << (container::kubernetes::detect() ? "Detected" : "Not detected") << "\n";
    std::cout << "  systemd-nspawn: " << (container::systemd_nspawn::detect() ? "Detected" : "Not detected") << "\n";

    // Additional container information
    std::cout << "\nContainer Environment:\n";
    std::cout << "  Orchestration Platform: " << container::getOrchestrationPlatform() << "\n";
    std::cout << "  Networking Mode: " << container::getNetworkingMode() << "\n";

    auto resourceLimits = container::getResourceLimits();
    if (!resourceLimits.empty()) {
        std::cout << "\nResource Limits:\n";
        for (const auto& [resource, limit] : resourceLimits) {
            std::cout << "  " << resource << ": " << limit << "\n";
        }
    }
}

void demonstrateDetectionMethods() {
    printSeparator("Detection Methods Control");

    VirtualizationDetector detector;
    auto methods = detector.getAvailableDetectionMethods();

    std::cout << "Available Detection Methods:\n";
    for (const auto& method : methods) {
        std::cout << "  - " << method << "\n";
    }

    // Demonstrate method control
    std::cout << "\nTesting Method Control:\n";
    std::cout << "  Disabling 'Time Drift' method...\n";
    detector.setDetectionMethod("Time Drift", false);

    auto info1 = detector.detect();
    std::cout << "  Confidence without Time Drift: " << std::fixed << std::setprecision(2)
              << (info1.confidence_score * 100) << "%\n";

    std::cout << "  Re-enabling 'Time Drift' method...\n";
    detector.setDetectionMethod("Time Drift", true);

    auto info2 = detector.detect();
    std::cout << "  Confidence with Time Drift: " << std::fixed << std::setprecision(2)
              << (info2.confidence_score * 100) << "%\n";
}

void demonstrateCompatibilityAPI() {
    printSeparator("Backward Compatibility API");

    // Test the old API functions for backward compatibility
    std::cout << "Legacy API Results:\n";
    std::cout << "  isVirtualMachine(): " << (atom::system::isVirtualMachine() ? "Yes" : "No") << "\n";
    std::cout << "  isContainer(): " << (atom::system::isContainer() ? "Yes" : "No") << "\n";
    std::cout << "  getHypervisorVendor(): " << atom::system::getHypervisorVendor() << "\n";
    std::cout << "  getVirtualizationType(): " << atom::system::getVirtualizationType() << "\n";
    std::cout << "  getContainerType(): " << atom::system::getContainerType() << "\n";
    std::cout << "  getVirtualizationConfidence(): " << std::fixed << std::setprecision(2)
              << (atom::system::getVirtualizationConfidence() * 100) << "%\n";

    // Test individual detection methods
    std::cout << "\nLegacy Detection Methods:\n";
    std::cout << "  checkBIOS(): " << (atom::system::checkBIOS() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkNetworkAdapter(): " << (atom::system::checkNetworkAdapter() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkDisk(): " << (atom::system::checkDisk() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkGraphicsCard(): " << (atom::system::checkGraphicsCard() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkProcesses(): " << (atom::system::checkProcesses() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkPCIBus(): " << (atom::system::checkPCIBus() ? "Detected" : "Not detected") << "\n";
    std::cout << "  checkTimeDrift(): " << (atom::system::checkTimeDrift() ? "Detected" : "Not detected") << "\n";
    std::cout << "  isDockerContainer(): " << (atom::system::isDockerContainer() ? "Detected" : "Not detected") << "\n";
}

void demonstrateFullReport() {
    printSeparator("Complete Detection Report");

    VirtualizationDetector detector;
    std::string report = detector.getDetectionReport();

    std::cout << report << std::endl;
}

int main() {
    std::cout << "Virtual Environment Detection Demo\n";
    std::cout << "==================================\n";

    try {
        demonstrateBasicDetection();
        demonstrateDetailedDetection();
        demonstrateHypervisorDetection();
        demonstrateContainerDetection();
        demonstrateDetectionMethods();
        demonstrateCompatibilityAPI();
        demonstrateFullReport();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Demo completed successfully!\n";
        std::cout << std::string(60, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
