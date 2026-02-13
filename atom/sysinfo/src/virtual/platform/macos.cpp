#include "macos.hpp"
#include "common.hpp"

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::platform::macos_impl {

#ifdef __APPLE__

auto detectVirtualizationMacOS() -> VirtualizationResult {
    VirtualizationResult result;
    result.platform = "macOS";

    // Check system control information
    auto sysctlInfo = checkSysctlVirtualization();
    if (!sysctlInfo.empty()) {
        for (const auto& info : sysctlInfo) {
            result.indicators.push_back("sysctl: " + info);
            result.confidence += 0.2;
        }
    }

    // Check IOKit for hardware information
    auto ioKitInfo = checkIOKitVirtualization();
    if (!ioKitInfo.empty()) {
        for (const auto& info : ioKitInfo) {
            result.indicators.push_back("IOKit: " + info);
            result.confidence += 0.2;
        }
    }

    // Check for VM-specific processes
    auto vmProcesses = checkVirtualizationProcesses();
    for (const auto& process : vmProcesses) {
        result.indicators.push_back("VM Process: " + process);
        result.confidence += 0.1;
    }

    // Check system profiler information
    auto profilerInfo = checkSystemProfiler();
    if (!profilerInfo.empty()) {
        for (const auto& info : profilerInfo) {
            result.indicators.push_back("System Profiler: " + info);
            result.confidence += 0.15;
        }
    }

    // Check for hypervisor framework
    if (checkHypervisorFramework()) {
        result.indicators.push_back("Hypervisor Framework available");
        result.confidence += 0.1;
    }

    // Limit confidence to 1.0
    result.confidence = std::min(result.confidence, 1.0);
    result.is_virtual = result.confidence > 0.3;

    return result;
}

auto checkSysctlVirtualization() -> std::vector<std::string> {
    std::vector<std::string> indicators;

    // Check for hypervisor presence
    int hypervisor = 0;
    size_t size = sizeof(hypervisor);
    if (sysctlbyname("kern.hv_support", &hypervisor, &size, nullptr, 0) == 0) {
        if (hypervisor) {
            indicators.push_back("Hypervisor support detected");
        }
    }

    // Check machine model
    char model[256];
    size = sizeof(model);
    if (sysctlbyname("hw.model", model, &size, nullptr, 0) == 0) {
        std::string modelStr(model);
        if (containsVMKeywords(modelStr)) {
            indicators.push_back("VM model detected: " + modelStr);
        }

        // Check for specific VM models
        if (modelStr.find("VMware") != std::string::npos) {
            indicators.push_back("VMware model detected");
        }
        if (modelStr.find("VirtualBox") != std::string::npos) {
            indicators.push_back("VirtualBox model detected");
        }
        if (modelStr.find("Parallels") != std::string::npos) {
            indicators.push_back("Parallels model detected");
        }
    }

    // Check CPU brand
    char cpuBrand[256];
    size = sizeof(cpuBrand);
    if (sysctlbyname("machdep.cpu.brand_string", cpuBrand, &size, nullptr, 0) == 0) {
        std::string brandStr(cpuBrand);
        if (containsVMKeywords(brandStr)) {
            indicators.push_back("VM CPU brand detected: " + brandStr);
        }
    }

    // Check for virtualization features
    uint32_t features = 0;
    size = sizeof(features);
    if (sysctlbyname("machdep.cpu.features", &features, &size, nullptr, 0) == 0) {
        // Check for hypervisor bit (bit 31)
        if (features & (1u << 31)) {
            indicators.push_back("CPU hypervisor bit set");
        }
    }

    return indicators;
}

auto checkIOKitVirtualization() -> std::vector<std::string> {
    std::vector<std::string> indicators;

    // Get IOKit registry
    io_registry_entry_t registry = IORegistryGetRootEntry(kIOMasterPortDefault);
    if (registry == MACH_PORT_NULL) {
        return indicators;
    }

    // Check system information
    CFMutableDictionaryRef properties = nullptr;
    kern_return_t result = IORegistryEntryCreateCFProperties(
        registry, &properties, kCFAllocatorDefault, kNilOptions);

    if (result == KERN_SUCCESS && properties != nullptr) {
        // Check for VM-specific properties
        CFStringRef keys[] = {
            CFSTR("manufacturer"),
            CFSTR("product-name"),
            CFSTR("version"),
            CFSTR("serial-number")
        };

        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            CFTypeRef value = CFDictionaryGetValue(properties, keys[i]);
            if (value && CFGetTypeID(value) == CFStringGetTypeID()) {
                char buffer[256];
                if (CFStringGetCString((CFStringRef)value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                    std::string valueStr(buffer);
                    if (containsVMKeywords(valueStr)) {
                        char keyBuffer[256];
                        CFStringGetCString(keys[i], keyBuffer, sizeof(keyBuffer), kCFStringEncodingUTF8);
                        indicators.push_back(std::string(keyBuffer) + ": " + valueStr);
                    }
                }
            }
        }

        CFRelease(properties);
    }

    IOObjectRelease(registry);
    return indicators;
}

auto checkVirtualizationProcesses() -> std::vector<std::string> {
    std::vector<std::string> vmProcesses;

    std::string output = executeCommand("ps aux");

    std::vector<std::string> processNames = {
        "vmware", "VBoxService", "VBoxClient", "parallels",
        "qemu", "VMware Tools", "Parallels Tools"
    };

    for (const auto& processName : processNames) {
        if (output.find(processName) != std::string::npos) {
            vmProcesses.push_back(processName);
        }
    }

    return vmProcesses;
}

auto checkSystemProfiler() -> std::vector<std::string> {
    std::vector<std::string> indicators;

    // Check hardware overview
    std::string hwOutput = executeCommand("system_profiler SPHardwareDataType");
    if (containsVMKeywords(hwOutput)) {
        indicators.push_back("VM hardware detected in system profiler");
    }

    // Check for specific VM indicators in hardware info
    if (hwOutput.find("VMware") != std::string::npos) {
        indicators.push_back("VMware detected in system profiler");
    }
    if (hwOutput.find("VirtualBox") != std::string::npos) {
        indicators.push_back("VirtualBox detected in system profiler");
    }
    if (hwOutput.find("Parallels") != std::string::npos) {
        indicators.push_back("Parallels detected in system profiler");
    }

    // Check PCI devices
    std::string pciOutput = executeCommand("system_profiler SPPCIDataType");
    if (containsVMKeywords(pciOutput)) {
        indicators.push_back("VM PCI devices detected");
    }

    // Check USB devices
    std::string usbOutput = executeCommand("system_profiler SPUSBDataType");
    if (containsVMKeywords(usbOutput)) {
        indicators.push_back("VM USB devices detected");
    }

    return indicators;
}

auto checkHypervisorFramework() -> bool {
    // Check if Hypervisor framework is available
    // This is a simplified check - in practice, you'd need to link against Hypervisor.framework

    // Check for hypervisor entitlements
    std::string entitlements = executeCommand("codesign -d --entitlements - /System/Library/Frameworks/Hypervisor.framework/Hypervisor 2>/dev/null || echo ''");

    return !entitlements.empty() && entitlements.find("com.apple.security.hypervisor") != std::string::npos;
}

auto getMacOSVirtualizationType() -> std::string {
    // Check sysctl information
    char model[256];
    size_t size = sizeof(model);
    if (sysctlbyname("hw.model", model, &size, nullptr, 0) == 0) {
        std::string modelStr(model);

        if (modelStr.find("VMware") != std::string::npos) {
            return "VMware Fusion";
        }
        if (modelStr.find("VirtualBox") != std::string::npos) {
            return "VirtualBox";
        }
        if (modelStr.find("Parallels") != std::string::npos) {
            return "Parallels Desktop";
        }
    }

    // Check system profiler
    std::string hwOutput = executeCommand("system_profiler SPHardwareDataType");

    if (hwOutput.find("VMware") != std::string::npos) {
        return "VMware Fusion";
    }
    if (hwOutput.find("VirtualBox") != std::string::npos) {
        return "VirtualBox";
    }
    if (hwOutput.find("Parallels") != std::string::npos) {
        return "Parallels Desktop";
    }
    if (hwOutput.find("QEMU") != std::string::npos) {
        return "QEMU";
    }

    // Check for processes
    auto processes = checkVirtualizationProcesses();
    for (const auto& process : processes) {
        if (process.find("vmware") != std::string::npos) {
            return "VMware Fusion";
        }
        if (process.find("VBox") != std::string::npos) {
            return "VirtualBox";
        }
        if (process.find("parallels") != std::string::npos) {
            return "Parallels Desktop";
        }
    }

    return "Unknown";
}

auto checkMacOSContainerization() -> bool {
    // macOS doesn't typically run in containers like Linux,
    // but we can check for some containerization technologies

    // Check for Docker Desktop
    std::string dockerCheck = executeCommand("ps aux | grep -i docker || echo ''");
    if (!dockerCheck.empty() && dockerCheck.find("docker") != std::string::npos) {
        return true;
    }

    // Check for other containerization tools
    std::string containerCheck = executeCommand("ps aux | grep -E 'podman|containerd|nerdctl' || echo ''");
    if (!containerCheck.empty()) {
        return true;
    }

    return false;
}

#else

// Non-macOS implementations (stubs)
auto detectVirtualizationMacOS() -> VirtualizationResult {
    VirtualizationResult result;
    result.platform = "Non-macOS";
    result.is_virtual = false;
    result.confidence = 0.0;
    return result;
}

auto checkSysctlVirtualization() -> std::vector<std::string> {
    return {};
}

auto checkIOKitVirtualization() -> std::vector<std::string> {
    return {};
}

auto checkVirtualizationProcesses() -> std::vector<std::string> {
    return {};
}

auto checkSystemProfiler() -> std::vector<std::string> {
    return {};
}

auto checkHypervisorFramework() -> bool {
    return false;
}

auto getMacOSVirtualizationType() -> std::string {
    return "Not macOS";
}

auto checkMacOSContainerization() -> bool {
    return false;
}

#endif

} // namespace atom::system::virtual_env::platform::macos_impl
