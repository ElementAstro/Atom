#include "hypervisor.hpp"
#include "common.hpp"
#include "detection.hpp"

#include <algorithm>
#include <sstream>

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::hypervisor {

auto hypervisorTypeToString(HypervisorType type) -> std::string {
    switch (type) {
        case HypervisorType::VMWARE_WORKSTATION: return "VMware Workstation";
        case HypervisorType::VMWARE_ESXI: return "VMware ESXi";
        case HypervisorType::VIRTUALBOX: return "VirtualBox";
        case HypervisorType::HYPER_V: return "Hyper-V";
        case HypervisorType::KVM: return "KVM";
        case HypervisorType::QEMU: return "QEMU";
        case HypervisorType::XEN: return "Xen";
        case HypervisorType::PARALLELS: return "Parallels";
        case HypervisorType::DOCKER: return "Docker";
        case HypervisorType::LXC: return "LXC";
        case HypervisorType::KUBERNETES: return "Kubernetes";
        case HypervisorType::CLOUD_HYPERVISOR: return "Cloud Hypervisor";
        case HypervisorType::FIRECRACKER: return "Firecracker";
        case HypervisorType::UNKNOWN:
        default: return "Unknown";
    }
}

auto getHypervisorVendor() -> std::string {
    return detection::cpuid::getHypervisorVendor();
}

auto detectHypervisorType() -> HypervisorType {
    std::string vendor = getHypervisorVendor();
    
    if (vendor.find("VMware") != std::string::npos) {
        if (vmware::isESXi()) {
            return HypervisorType::VMWARE_ESXI;
        }
        return HypervisorType::VMWARE_WORKSTATION;
    }
    
    if (vendor.find("VBoxVBox") != std::string::npos) {
        return HypervisorType::VIRTUALBOX;
    }
    
    if (vendor.find("Microsoft") != std::string::npos) {
        return HypervisorType::HYPER_V;
    }
    
    if (vendor.find("KVMKVMKVM") != std::string::npos) {
        return HypervisorType::KVM;
    }
    
    if (vendor.find("XenVMMXen") != std::string::npos) {
        return HypervisorType::XEN;
    }
    
    // Check for other indicators
    if (vmware::detect()) return HypervisorType::VMWARE_WORKSTATION;
    if (virtualbox::detect()) return HypervisorType::VIRTUALBOX;
    if (hyperv::detect()) return HypervisorType::HYPER_V;
    if (kvm::detect()) return HypervisorType::KVM;
    if (xen::detect()) return HypervisorType::XEN;
    if (parallels::detect()) return HypervisorType::PARALLELS;
    if (cloud::detect()) return HypervisorType::CLOUD_HYPERVISOR;
    
    return HypervisorType::UNKNOWN;
}

auto getHypervisorInfo() -> HypervisorInfo {
    HypervisorInfo info;
    info.type = detectHypervisorType();
    info.name = hypervisorTypeToString(info.type);
    info.vendor = getHypervisorVendor();
    
    switch (info.type) {
        case HypervisorType::VMWARE_WORKSTATION:
        case HypervisorType::VMWARE_ESXI:
            info.version = vmware::getVersion();
            info.features = vmware::getFeatures();
            info.detection_confidence = 0.95;
            break;
            
        case HypervisorType::VIRTUALBOX:
            info.version = virtualbox::getVersion();
            info.features = virtualbox::getFeatures();
            info.detection_confidence = 0.90;
            break;
            
        case HypervisorType::HYPER_V:
            info.version = hyperv::getVersion();
            info.features = hyperv::getFeatures();
            info.detection_confidence = 0.90;
            break;
            
        case HypervisorType::KVM:
            info.version = kvm::getVersion();
            info.features = kvm::getFeatures();
            info.detection_confidence = 0.85;
            break;
            
        case HypervisorType::XEN:
            info.version = xen::getVersion();
            info.features = xen::getFeatures();
            info.detection_confidence = 0.85;
            break;
            
        case HypervisorType::PARALLELS:
            info.version = parallels::getVersion();
            info.features = parallels::getFeatures();
            info.detection_confidence = 0.80;
            break;
            
        case HypervisorType::CLOUD_HYPERVISOR:
            info.properties = cloud::getCloudMetadata();
            info.detection_confidence = 0.75;
            break;
            
        default:
            info.detection_confidence = 0.0;
            break;
    }
    
    return info;
}

auto isNestedVirtualization() -> bool {
    // Check for nested virtualization indicators
    auto cpuFeatures = detection::cpuid::getVirtualizationFeatures();
    
    // If we're in a VM but have virtualization features, it might be nested
    bool inVM = detection::cpuid::isHypervisorPresent();
    bool hasVirtFeatures = std::any_of(cpuFeatures.begin(), cpuFeatures.end(),
        [](const std::string& feature) {
            return feature.find("VMX") != std::string::npos ||
                   feature.find("SVM") != std::string::npos;
        });
    
    return inVM && hasVirtFeatures;
}

auto getVirtualizationCapabilities() -> std::vector<std::string> {
    return detection::cpuid::getVirtualizationFeatures();
}

namespace vmware {
    auto detect() -> bool {
        // Check CPUID vendor
        std::string vendor = getHypervisorVendor();
        if (vendor.find("VMware") != std::string::npos) {
            return true;
        }
        
        // Check BIOS information
        std::string biosManufacturer = detection::bios::getBIOSManufacturer();
        std::string systemManufacturer = detection::bios::getSystemManufacturer();
        
        return containsVMKeywords(biosManufacturer + " " + systemManufacturer);
    }
    
    auto getVersion() -> std::string {
        // Try to get VMware Tools version
        std::string output = executeCommand("vmware-toolbox-cmd -v 2>/dev/null || echo ''");
        if (!output.empty()) {
            return output;
        }
        
        // Check environment variable
        const char* version = std::getenv("VMWARE_TOOLS_VERSION");
        if (version) {
            return std::string(version);
        }
        
        return "Unknown";
    }
    
    auto isWorkstation() -> bool {
        std::string productName = detection::bios::getProductName();
        return productName.find("VMware Virtual Platform") != std::string::npos;
    }
    
    auto isESXi() -> bool {
        std::string productName = detection::bios::getProductName();
        return productName.find("VMware7,1") != std::string::npos ||
               productName.find("ESXi") != std::string::npos;
    }
    
    auto hasVMwareTools() -> bool {
        // Check for VMware Tools processes
        std::string processes = executeCommand("ps aux | grep vmtoolsd || echo ''");
        return !processes.empty() && processes.find("vmtoolsd") != std::string::npos;
    }
    
    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;
        
        if (hasVMwareTools()) {
            features.push_back("VMware Tools");
        }
        
        if (isWorkstation()) {
            features.push_back("Workstation");
        } else if (isESXi()) {
            features.push_back("ESXi");
        }
        
        return features;
    }
}

namespace virtualbox {
    auto detect() -> bool {
        std::string vendor = getHypervisorVendor();
        if (vendor.find("VBoxVBox") != std::string::npos) {
            return true;
        }
        
        std::string biosManufacturer = detection::bios::getBIOSManufacturer();
        std::string systemManufacturer = detection::bios::getSystemManufacturer();
        
        return (biosManufacturer.find("innotek") != std::string::npos ||
                systemManufacturer.find("innotek") != std::string::npos ||
                biosManufacturer.find("Oracle") != std::string::npos);
    }
    
    auto getVersion() -> std::string {
        const char* version = std::getenv("VBOX_VERSION");
        if (version) {
            return std::string(version);
        }
        
        return "Unknown";
    }
    
    auto hasGuestAdditions() -> bool {
        std::string processes = executeCommand("ps aux | grep VBoxService || echo ''");
        return !processes.empty() && processes.find("VBoxService") != std::string::npos;
    }
    
    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;
        
        if (hasGuestAdditions()) {
            features.push_back("Guest Additions");
        }
        
        if (checkHardware()) {
            features.push_back("VirtualBox Hardware");
        }
        
        return features;
    }
    
    auto checkHardware() -> bool {
        // Check for VirtualBox-specific hardware
        std::string pciDevices = executeCommand("lspci | grep -i virtualbox || echo ''");
        return !pciDevices.empty();
    }
}

namespace hyperv {
    auto detect() -> bool {
        std::string vendor = getHypervisorVendor();
        if (vendor.find("Microsoft") != std::string::npos) {
            return true;
        }

        std::string biosManufacturer = detection::bios::getBIOSManufacturer();
        std::string systemManufacturer = detection::bios::getSystemManufacturer();

        return (biosManufacturer.find("Microsoft") != std::string::npos ||
                systemManufacturer.find("Microsoft") != std::string::npos);
    }

    auto getVersion() -> std::string {
        // Try to get Hyper-V version from registry or system info
#ifdef _WIN32
        std::string output = executeCommand("wmic computersystem get model");
        if (output.find("Virtual Machine") != std::string::npos) {
            return "Hyper-V";
        }
#endif
        return "Unknown";
    }

    auto hasIntegrationServices() -> bool {
#ifdef _WIN32
        std::string services = executeCommand("sc query vmicheartbeat");
        return services.find("RUNNING") != std::string::npos;
#else
        std::string processes = executeCommand("ps aux | grep hv_ || echo ''");
        return !processes.empty();
#endif
    }

    auto getGeneration() -> int {
        // Try to determine Hyper-V generation
        std::string productName = detection::bios::getProductName();
        if (productName.find("Virtual Machine") != std::string::npos) {
            // Generation 2 VMs typically have UEFI
            if (fileExists("/sys/firmware/efi")) {
                return 2;
            }
            return 1;
        }
        return 0;
    }

    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;

        if (hasIntegrationServices()) {
            features.push_back("Integration Services");
        }

        int gen = getGeneration();
        if (gen > 0) {
            features.push_back("Generation " + std::to_string(gen));
        }

        return features;
    }
}

namespace kvm {
    auto detect() -> bool {
        std::string vendor = getHypervisorVendor();
        if (vendor.find("KVMKVMKVM") != std::string::npos) {
            return true;
        }

        // Check for KVM-specific files
        return fileExists("/dev/kvm") || fileExists("/sys/module/kvm");
    }

    auto detectQEMU() -> bool {
        std::string biosManufacturer = detection::bios::getBIOSManufacturer();
        std::string systemManufacturer = detection::bios::getSystemManufacturer();

        return (biosManufacturer.find("QEMU") != std::string::npos ||
                systemManufacturer.find("QEMU") != std::string::npos);
    }

    auto getVersion() -> std::string {
        std::string output = executeCommand("qemu-system-x86_64 --version 2>/dev/null || echo ''");
        if (!output.empty()) {
            return output;
        }

        return "Unknown";
    }

    auto getQEMUVersion() -> std::string {
        return getVersion();
    }

    auto hasGuestAgent() -> bool {
        std::string processes = executeCommand("ps aux | grep qemu-ga || echo ''");
        return !processes.empty() && processes.find("qemu-ga") != std::string::npos;
    }

    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;

        if (hasGuestAgent()) {
            features.push_back("QEMU Guest Agent");
        }

        if (hasVirtIODevices()) {
            features.push_back("VirtIO Devices");
        }

        if (detectQEMU()) {
            features.push_back("QEMU Emulation");
        }

        return features;
    }

    auto hasVirtIODevices() -> bool {
        std::string pciDevices = executeCommand("lspci | grep -i virtio || echo ''");
        return !pciDevices.empty();
    }
}

namespace xen {
    auto detect() -> bool {
        std::string vendor = getHypervisorVendor();
        if (vendor.find("XenVMMXen") != std::string::npos) {
            return true;
        }

        return fileExists("/proc/xen") || fileExists("/sys/hypervisor/uuid");
    }

    auto getVersion() -> std::string {
        if (fileExists("/sys/hypervisor/version/major")) {
            std::string major = readFileContent("/sys/hypervisor/version/major");
            std::string minor = readFileContent("/sys/hypervisor/version/minor");
            return major + "." + minor;
        }

        return "Unknown";
    }

    auto isParavirtualized() -> bool {
        // Check for PV-specific indicators
        return fileExists("/proc/xen/capabilities");
    }

    auto isHVM() -> bool {
        // Check for HVM-specific indicators
        std::string capabilities = readFileContent("/proc/xen/capabilities");
        return capabilities.find("control_d") != std::string::npos;
    }

    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;

        if (isParavirtualized()) {
            features.push_back("Paravirtualized");
        }

        if (isHVM()) {
            features.push_back("Hardware Virtual Machine");
        }

        return features;
    }
}

namespace parallels {
    auto detect() -> bool {
        std::string biosManufacturer = detection::bios::getBIOSManufacturer();
        std::string systemManufacturer = detection::bios::getSystemManufacturer();

        return (biosManufacturer.find("Parallels") != std::string::npos ||
                systemManufacturer.find("Parallels") != std::string::npos);
    }

    auto getVersion() -> std::string {
        std::string output = executeCommand("prlctl --version 2>/dev/null || echo ''");
        if (!output.empty()) {
            return output;
        }

        return "Unknown";
    }

    auto hasParallelsTools() -> bool {
        std::string processes = executeCommand("ps aux | grep prl_ || echo ''");
        return !processes.empty();
    }

    auto getFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;

        if (hasParallelsTools()) {
            features.push_back("Parallels Tools");
        }

        return features;
    }
}

namespace cloud {
    auto detect() -> bool {
        // Check for cloud-specific metadata endpoints
        std::string awsMetadata = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-id 2>/dev/null || echo ''");
        if (!awsMetadata.empty() && awsMetadata.find("i-") == 0) {
            return true;
        }

        // Check for GCP metadata
        std::string gcpMetadata = executeCommand("curl -s --max-time 2 -H 'Metadata-Flavor: Google' http://169.254.169.254/computeMetadata/v1/instance/id 2>/dev/null || echo ''");
        if (!gcpMetadata.empty() && gcpMetadata.length() > 5) {
            return true;
        }

        // Check for Azure metadata
        std::string azureMetadata = executeCommand("curl -s --max-time 2 -H 'Metadata: true' http://169.254.169.254/metadata/instance/compute/vmId?api-version=2021-02-01&format=text 2>/dev/null || echo ''");
        if (!azureMetadata.empty() && azureMetadata.length() > 10) {
            return true;
        }

        return false;
    }

    auto detectAWSNitro() -> bool {
        std::string awsMetadata = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-id 2>/dev/null || echo ''");
        if (!awsMetadata.empty() && awsMetadata.find("i-") == 0) {
            // Check for Nitro-specific indicators
            std::string instanceType = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-type 2>/dev/null || echo ''");
            return instanceType.find("nitro") != std::string::npos ||
                   instanceType.find("m5") != std::string::npos ||
                   instanceType.find("c5") != std::string::npos ||
                   instanceType.find("r5") != std::string::npos;
        }
        return false;
    }

    auto detectGCPHypervisor() -> bool {
        std::string gcpMetadata = executeCommand("curl -s --max-time 2 -H 'Metadata-Flavor: Google' http://169.254.169.254/computeMetadata/v1/instance/id 2>/dev/null || echo ''");
        return !gcpMetadata.empty() && gcpMetadata.length() > 5;
    }

    auto detectAzureHypervisor() -> bool {
        std::string azureMetadata = executeCommand("curl -s --max-time 2 -H 'Metadata: true' http://169.254.169.254/metadata/instance/compute/vmId?api-version=2021-02-01&format=text 2>/dev/null || echo ''");
        return !azureMetadata.empty() && azureMetadata.length() > 10;
    }

    auto getCloudProvider() -> std::string {
        if (detectAWSNitro()) {
            return "Amazon Web Services";
        }

        if (detectGCPHypervisor()) {
            return "Google Cloud Platform";
        }

        if (detectAzureHypervisor()) {
            return "Microsoft Azure";
        }

        return "Unknown";
    }

    auto getCloudMetadata() -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> metadata;

        // Try AWS metadata
        std::string awsInstanceId = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-id 2>/dev/null || echo ''");
        if (!awsInstanceId.empty() && awsInstanceId.find("i-") == 0) {
            metadata["provider"] = "AWS";
            metadata["instance-id"] = awsInstanceId;

            std::string instanceType = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/instance-type 2>/dev/null || echo ''");
            if (!instanceType.empty()) {
                metadata["instance-type"] = instanceType;
            }

            std::string region = executeCommand("curl -s --max-time 2 http://169.254.169.254/latest/meta-data/placement/region 2>/dev/null || echo ''");
            if (!region.empty()) {
                metadata["region"] = region;
            }
        }

        // Try GCP metadata
        std::string gcpInstanceId = executeCommand("curl -s --max-time 2 -H 'Metadata-Flavor: Google' http://169.254.169.254/computeMetadata/v1/instance/id 2>/dev/null || echo ''");
        if (!gcpInstanceId.empty() && gcpInstanceId.length() > 5) {
            metadata["provider"] = "GCP";
            metadata["instance-id"] = gcpInstanceId;

            std::string machineType = executeCommand("curl -s --max-time 2 -H 'Metadata-Flavor: Google' http://169.254.169.254/computeMetadata/v1/instance/machine-type 2>/dev/null || echo ''");
            if (!machineType.empty()) {
                metadata["machine-type"] = machineType;
            }

            std::string zone = executeCommand("curl -s --max-time 2 -H 'Metadata-Flavor: Google' http://169.254.169.254/computeMetadata/v1/instance/zone 2>/dev/null || echo ''");
            if (!zone.empty()) {
                metadata["zone"] = zone;
            }
        }

        // Try Azure metadata
        std::string azureVmId = executeCommand("curl -s --max-time 2 -H 'Metadata: true' http://169.254.169.254/metadata/instance/compute/vmId?api-version=2021-02-01&format=text 2>/dev/null || echo ''");
        if (!azureVmId.empty() && azureVmId.length() > 10) {
            metadata["provider"] = "Azure";
            metadata["vm-id"] = azureVmId;

            std::string vmSize = executeCommand("curl -s --max-time 2 -H 'Metadata: true' http://169.254.169.254/metadata/instance/compute/vmSize?api-version=2021-02-01&format=text 2>/dev/null || echo ''");
            if (!vmSize.empty()) {
                metadata["vm-size"] = vmSize;
            }

            std::string location = executeCommand("curl -s --max-time 2 -H 'Metadata: true' http://169.254.169.254/metadata/instance/compute/location?api-version=2021-02-01&format=text 2>/dev/null || echo ''");
            if (!location.empty()) {
                metadata["location"] = location;
            }
        }

        return metadata;
    }
}

} // namespace atom::system::virtual_env::hypervisor
