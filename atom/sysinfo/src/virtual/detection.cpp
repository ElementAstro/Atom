#include "detection.hpp"
#include "common.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <intrin.h>
#include <tchar.h>
#include <windows.h>
#else
#include <cpuid.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::detection {

namespace cpuid {
    auto isHypervisorPresent() -> bool {
        spdlog::debug("Checking if running in virtual machine using CPUID");
        auto cpuInfo = getCPUInfo(constants::CPUID_FEATURES);
        
        bool isVM = static_cast<bool>(cpuInfo[2] & (1u << constants::HYPERVISOR_PRESENT_BIT));
        spdlog::debug("Virtual machine detected via CPUID: {}", isVM);
        return isVM;
    }
    
    auto getHypervisorVendor() -> std::string {
        spdlog::debug("Getting hypervisor vendor information");
        auto cpuInfo = getCPUInfo(constants::CPUID_HYPERVISOR);
        
        std::array<char, constants::VENDOR_STRING_LENGTH + 1> vendor = {0};
        std::memcpy(vendor.data(), &cpuInfo[1], 4);
        std::memcpy(vendor.data() + 4, &cpuInfo[2], 4);
        std::memcpy(vendor.data() + 8, &cpuInfo[3], 4);
        
        std::string vendorStr(vendor.data());
        spdlog::debug("Hypervisor vendor: {}", vendorStr);
        return vendorStr;
    }
    
    auto getCPUVendor() -> std::string {
        auto cpuInfo = getCPUInfo(0);
        
        std::array<char, 13> vendor = {0};
        std::memcpy(vendor.data(), &cpuInfo[1], 4);
        std::memcpy(vendor.data() + 4, &cpuInfo[3], 4);
        std::memcpy(vendor.data() + 8, &cpuInfo[2], 4);
        
        return std::string(vendor.data());
    }
    
    auto getVirtualizationFeatures() -> std::vector<std::string> {
        std::vector<std::string> features;
        
        // Check for various virtualization-related CPU features
        auto cpuInfo = getCPUInfo(1);
        
        if (cpuInfo[2] & (1u << 31)) {
            features.push_back("Hypervisor Present");
        }
        
        // Check for VMX (Intel VT-x)
        if (cpuInfo[2] & (1u << 5)) {
            features.push_back("VMX (Intel VT-x)");
        }
        
        // Check for SVM (AMD-V)
        auto extendedInfo = getCPUInfo(0x80000001);
        if (extendedInfo[2] & (1u << 2)) {
            features.push_back("SVM (AMD-V)");
        }
        
        return features;
    }
}

namespace bios {
    auto checkBIOSInfo() -> bool {
        spdlog::debug("Checking BIOS information for virtualization signs");
        
#ifdef _WIN32
        HKEY hKey;
        std::array<TCHAR, constants::BIOS_INFO_LENGTH> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            struct RegKeyCloser {
                HKEY key;
                ~RegKeyCloser() { RegCloseKey(key); }
            } keyCloser{hKey};
            
            if (RegQueryValueEx(hKey, _T("SystemManufacturer"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(biosInfo.data()),
                                &bufSize) == ERROR_SUCCESS) {
                std::string bios(biosInfo.data());
                spdlog::debug("BIOS SystemManufacturer: {}", bios);
                return containsVMKeywords(bios);
            }
        }
#else
        std::ifstream file("/sys/class/dmi/id/product_name");
        if (file.is_open()) {
            std::string biosInfo;
            std::getline(file, biosInfo);
            spdlog::debug("BIOS product name: {}", biosInfo);
            return containsVMKeywords(biosInfo);
        }
#endif
        return false;
    }
    
    auto getBIOSManufacturer() -> std::string {
#ifdef _WIN32
        HKEY hKey;
        std::array<TCHAR, constants::BIOS_INFO_LENGTH> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            struct RegKeyCloser {
                HKEY key;
                ~RegKeyCloser() { RegCloseKey(key); }
            } keyCloser{hKey};
            
            if (RegQueryValueEx(hKey, _T("BIOSVendor"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(biosInfo.data()),
                                &bufSize) == ERROR_SUCCESS) {
                return std::string(biosInfo.data());
            }
        }
#else
        return readFileContent("/sys/class/dmi/id/bios_vendor");
#endif
        return {};
    }
    
    auto getSystemManufacturer() -> std::string {
#ifdef _WIN32
        HKEY hKey;
        std::array<TCHAR, constants::BIOS_INFO_LENGTH> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            struct RegKeyCloser {
                HKEY key;
                ~RegKeyCloser() { RegCloseKey(key); }
            } keyCloser{hKey};
            
            if (RegQueryValueEx(hKey, _T("SystemManufacturer"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(biosInfo.data()),
                                &bufSize) == ERROR_SUCCESS) {
                return std::string(biosInfo.data());
            }
        }
#else
        return readFileContent("/sys/class/dmi/id/sys_vendor");
#endif
        return {};
    }
    
    auto getProductName() -> std::string {
#ifdef _WIN32
        HKEY hKey;
        std::array<TCHAR, constants::BIOS_INFO_LENGTH> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            struct RegKeyCloser {
                HKEY key;
                ~RegKeyCloser() { RegCloseKey(key); }
            } keyCloser{hKey};
            
            if (RegQueryValueEx(hKey, _T("SystemProductName"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(biosInfo.data()),
                                &bufSize) == ERROR_SUCCESS) {
                return std::string(biosInfo.data());
            }
        }
#else
        return readFileContent("/sys/class/dmi/id/product_name");
#endif
        return {};
    }
    
    auto getBIOSVersion() -> std::string {
#ifdef _WIN32
        HKEY hKey;
        std::array<TCHAR, constants::BIOS_INFO_LENGTH> biosInfo;
        DWORD bufSize = sizeof(biosInfo);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("HARDWARE\\DESCRIPTION\\System\\BIOS"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            struct RegKeyCloser {
                HKEY key;
                ~RegKeyCloser() { RegCloseKey(key); }
            } keyCloser{hKey};
            
            if (RegQueryValueEx(hKey, _T("BIOSVersion"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(biosInfo.data()),
                                &bufSize) == ERROR_SUCCESS) {
                return std::string(biosInfo.data());
            }
        }
#else
        return readFileContent("/sys/class/dmi/id/bios_version");
#endif
        return {};
    }
}

namespace hardware {
    auto parseNetworkAdapterOutput(const std::string& output) -> bool {
        constexpr std::array<std::string_view, 5> vmNetKeywords = {
            "virbr", "vbox", "vmnet", "veth", "docker"};
        
        return std::any_of(vmNetKeywords.begin(), vmNetKeywords.end(),
                           [&output](std::string_view keyword) {
                               return output.find(keyword) != std::string::npos;
                           }) ||
               containsVMKeywords(output);
    }
    
    auto checkNetworkAdapters() -> bool {
        spdlog::debug("Checking network adapters for virtualization indicators");
        
#ifdef _WIN32
        std::string output = executeCommand("ipconfig /all");
#else
        std::string output = executeCommand("ip link show");
        if (output.empty()) {
            output = executeCommand("cat /proc/net/dev");
        }
#endif
        
        return parseNetworkAdapterOutput(output);
    }
    
    auto getNetworkAdapters() -> std::vector<std::string> {
        std::vector<std::string> adapters;
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path Win32_NetworkAdapter get Name");
#else
        std::string output = executeCommand("ip link show");
#endif
        
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.find("Name") == std::string::npos) {
                adapters.push_back(line);
            }
        }
        
        return adapters;
    }
    
    auto checkDiskInfo() -> bool {
        spdlog::debug("Checking disk information for virtualization signs");
        
#ifdef _WIN32
        std::string output = executeCommand("wmic diskdrive get caption,model");
#else
        std::string output = executeCommand("lsblk -o NAME,MODEL");
        if (output.empty()) {
            output = executeCommand("cat /proc/partitions");
        }
#endif
        
        return containsVMKeywords(output);
    }
    
    auto getDiskInfo() -> std::vector<std::string> {
        std::vector<std::string> disks;
        
#ifdef _WIN32
        std::string output = executeCommand("wmic diskdrive get caption,model");
#else
        std::string output = executeCommand("lsblk -o NAME,MODEL,SIZE");
#endif
        
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                disks.push_back(line);
            }
        }
        
        return disks;
    }
    
    auto checkGraphicsCard() -> bool {
        spdlog::debug("Checking graphics card for virtualization indicators");
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path win32_videocontroller get caption");
#else
        std::string output = executeCommand("lspci | grep -i vga");
        if (output.empty()) {
            output = executeCommand("cat /proc/driver/nvidia/cards 2>/dev/null || echo ''");
        }
#endif
        
        return containsVMKeywords(output);
    }
    
    auto getGraphicsInfo() -> std::vector<std::string> {
        std::vector<std::string> graphics;
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path win32_videocontroller get caption");
#else
        std::string output = executeCommand("lspci | grep -i 'vga\\|3d\\|display'");
#endif
        
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                graphics.push_back(line);
            }
        }
        
        return graphics;
    }
    
    auto checkPCIBus() -> bool {
        spdlog::debug("Checking PCI bus for virtualization devices");
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path Win32_PnPEntity get Name");
#else
        std::string output = executeCommand("lspci");
#endif
        
        return containsVMKeywords(output);
    }
    
    auto getPCIDevices() -> std::vector<std::string> {
        std::vector<std::string> devices;
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path Win32_PnPEntity get Name");
#else
        std::string output = executeCommand("lspci");
#endif
        
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                devices.push_back(line);
            }
        }
        
        return devices;
    }
    
    auto checkUSBDevices() -> bool {
#ifdef _WIN32
        std::string output = executeCommand("wmic path Win32_USBHub get Name");
#else
        std::string output = executeCommand("lsusb");
#endif
        
        return containsVMKeywords(output);
    }
    
    auto getUSBDevices() -> std::vector<std::string> {
        std::vector<std::string> devices;
        
#ifdef _WIN32
        std::string output = executeCommand("wmic path Win32_USBHub get Name");
#else
        std::string output = executeCommand("lsusb");
#endif
        
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                devices.push_back(line);
            }
        }
        
        return devices;
    }
}

namespace processes {
    auto checkVirtualizationProcesses() -> bool {
        spdlog::debug("Checking for virtualization-related processes");

#ifdef _WIN32
        std::string output = executeCommand("tasklist");
        constexpr std::array<std::string_view, 4> vmProcesses = {
            "vmtoolsd.exe", "VBoxService.exe", "qemu-ga", "xenservice"};
#else
        std::string output = executeCommand("ps aux");
        constexpr std::array<std::string_view, 4> vmProcesses = {
            "vmtoolsd", "VBoxService", "qemu-ga", "xenstore"};
#endif

        return std::any_of(vmProcesses.begin(), vmProcesses.end(),
                           [&output](std::string_view process) {
                               return output.find(process) != std::string::npos;
                           });
    }

    auto getRunningProcesses() -> std::vector<std::string> {
        std::vector<std::string> processes;

#ifdef _WIN32
        std::string output = executeCommand("tasklist /fo csv");
#else
        std::string output = executeCommand("ps -eo comm");
#endif

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                processes.push_back(line);
            }
        }

        return processes;
    }

    auto checkVirtualizationServices() -> bool {
#ifdef _WIN32
        std::string output = executeCommand("sc query");
        constexpr std::array<std::string_view, 4> vmServices = {
            "VMTools", "VBoxService", "QEMU", "XenService"};
#else
        std::string output = executeCommand("systemctl list-units --type=service");
        constexpr std::array<std::string_view, 4> vmServices = {
            "vmtoolsd", "vboxadd", "qemu-guest-agent", "xendomains"};
#endif

        return std::any_of(vmServices.begin(), vmServices.end(),
                           [&output](std::string_view service) {
                               return output.find(service) != std::string::npos;
                           });
    }

    auto getRunningServices() -> std::vector<std::string> {
        std::vector<std::string> services;

#ifdef _WIN32
        std::string output = executeCommand("sc query");
#else
        std::string output = executeCommand("systemctl list-units --type=service --state=running");
#endif

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                services.push_back(line);
            }
        }

        return services;
    }

    auto checkVirtualizationDrivers() -> bool {
#ifdef _WIN32
        std::string output = executeCommand("driverquery");
        constexpr std::array<std::string_view, 4> vmDrivers = {
            "vmci", "vmmouse", "vmxnet", "vboxguest"};
#else
        std::string output = executeCommand("lsmod");
        constexpr std::array<std::string_view, 6> vmDrivers = {
            "vmw_", "vbox", "virtio", "xen", "kvm", "qemu"};
#endif

        return std::any_of(vmDrivers.begin(), vmDrivers.end(),
                           [&output](std::string_view driver) {
                               return output.find(driver) != std::string::npos;
                           });
    }

    auto getLoadedDrivers() -> std::vector<std::string> {
        std::vector<std::string> drivers;

#ifdef _WIN32
        std::string output = executeCommand("driverquery /fo csv");
#else
        std::string output = executeCommand("lsmod");
#endif

        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                drivers.push_back(line);
            }
        }

        return drivers;
    }
}

namespace timing {
    auto checkTimeDrift() -> bool {
        spdlog::debug("Checking for time drift anomalies");

        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bool timeDrift = duration > constants::TIME_DRIFT_UPPER_BOUND ||
                        duration < constants::TIME_DRIFT_LOWER_BOUND;
        spdlog::debug("Time drift detected: {} (duration: {}ms)", timeDrift, duration);
        return timeDrift;
    }

    auto checkInstructionTiming() -> bool {
        // Measure CPUID instruction timing
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            getCPUInfo(0);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // In VMs, CPUID instructions typically take longer
        return duration > 10000; // 10ms for 1000 instructions suggests VM
    }

    auto checkPerformanceCharacteristics() -> bool {
        // Simple performance test that might indicate virtualization
        auto start = std::chrono::high_resolution_clock::now();

        volatile int sum = 0;
        for (int i = 0; i < 1000000; ++i) {
            sum += i;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // This is a very rough heuristic
        return duration > 50000; // 50ms suggests slower execution (VM)
    }

    auto checkMemoryAccessPatterns() -> bool {
        // Memory access pattern test
        constexpr size_t arraySize = 1024 * 1024; // 1MB
        std::vector<int> testArray(arraySize);

        auto start = std::chrono::high_resolution_clock::now();

        // Random memory access pattern
        for (size_t i = 0; i < arraySize; i += 64) { // Cache line size
            testArray[i] = i;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // VMs often have different memory access characteristics
        return duration > 10000; // 10ms suggests VM overhead
    }
}

namespace registry {
#ifdef _WIN32
    auto checkVirtualizationRegistry() -> bool {
        HKEY hKey;
        std::array<TCHAR, 256> regValue;
        DWORD bufSize = sizeof(regValue);

        // Check for VMware registry entries
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("SOFTWARE\\VMware, Inc.\\VMware Tools"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }

        // Check for VirtualBox registry entries
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         _T("SOFTWARE\\Oracle\\VirtualBox Guest Additions"), 0, KEY_READ,
                         &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }

        return false;
    }

    auto getVirtualizationRegistryEntries() -> std::vector<std::string> {
        std::vector<std::string> entries;

        // This would require more complex registry enumeration
        // For now, return known VM-related registry paths
        entries.push_back("SOFTWARE\\VMware, Inc.\\VMware Tools");
        entries.push_back("SOFTWARE\\Oracle\\VirtualBox Guest Additions");
        entries.push_back("SYSTEM\\ControlSet001\\Services\\VBoxGuest");
        entries.push_back("SYSTEM\\ControlSet001\\Services\\vmci");

        return entries;
    }

    auto checkVMRegistryKeys() -> bool {
        return checkVirtualizationRegistry();
    }
#else
    auto checkVirtualizationRegistry() -> bool {
        return false; // Registry is Windows-specific
    }

    auto getVirtualizationRegistryEntries() -> std::vector<std::string> {
        return {}; // Registry is Windows-specific
    }

    auto checkVMRegistryKeys() -> bool {
        return false; // Registry is Windows-specific
    }
#endif
}

namespace filesystem {
    auto checkVirtualizationFiles() -> bool {
        std::vector<std::string> vmFiles = {
            "/.dockerenv",
            "/run/.containerenv",
            "/proc/vz",
            "/proc/bc",
            "/sys/hypervisor/uuid",
            "/dev/vmci",
            "/proc/xen"
        };

        for (const auto& file : vmFiles) {
            if (fileExists(file)) {
                spdlog::debug("Found virtualization file: {}", file);
                return true;
            }
        }

        return false;
    }

    auto getVirtualizationFiles() -> std::vector<std::string> {
        std::vector<std::string> foundFiles;
        std::vector<std::string> vmFiles = {
            "/.dockerenv",
            "/run/.containerenv",
            "/proc/vz",
            "/proc/bc",
            "/sys/hypervisor/uuid",
            "/dev/vmci",
            "/proc/xen",
            "/sys/class/dmi/id/product_name",
            "/sys/class/dmi/id/sys_vendor"
        };

        for (const auto& file : vmFiles) {
            if (fileExists(file)) {
                foundFiles.push_back(file);
            }
        }

        return foundFiles;
    }

    auto checkVirtualizationMounts() -> bool {
#ifndef _WIN32
        std::string mountInfo = readFileContent("/proc/mounts");

        std::vector<std::string> vmMountTypes = {
            "9p", "virtiofs", "vboxsf", "vmhgfs", "fuse.vmware-vmblock"
        };

        for (const auto& mountType : vmMountTypes) {
            if (mountInfo.find(mountType) != std::string::npos) {
                return true;
            }
        }
#endif
        return false;
    }

    auto getVirtualizationMounts() -> std::vector<std::string> {
        std::vector<std::string> vmMounts;

#ifndef _WIN32
        std::string mountInfo = readFileContent("/proc/mounts");
        std::istringstream stream(mountInfo);
        std::string line;

        while (std::getline(stream, line)) {
            if (containsVMKeywords(line)) {
                vmMounts.push_back(line);
            }
        }
#endif

        return vmMounts;
    }
}

namespace environment {
    auto checkVirtualizationEnvironment() -> bool {
        std::vector<std::string> vmEnvVars = {
            "VMWARE_TOOLS_VERSION",
            "VBOX_VERSION",
            "KUBERNETES_SERVICE_HOST",
            "DOCKER_CONTAINER",
            "container"
        };

        for (const auto& envVar : vmEnvVars) {
            if (std::getenv(envVar.c_str()) != nullptr) {
                spdlog::debug("Found virtualization environment variable: {}", envVar);
                return true;
            }
        }

        return false;
    }

    auto getVirtualizationEnvironmentVars() -> std::vector<std::string> {
        std::vector<std::string> foundVars;
        std::vector<std::string> vmEnvVars = {
            "VMWARE_TOOLS_VERSION",
            "VBOX_VERSION",
            "KUBERNETES_SERVICE_HOST",
            "KUBERNETES_PORT",
            "DOCKER_CONTAINER",
            "container",
            "HOSTNAME",
            "USER"
        };

        for (const auto& envVar : vmEnvVars) {
            const char* value = std::getenv(envVar.c_str());
            if (value != nullptr) {
                foundVars.push_back(envVar + "=" + std::string(value));
            }
        }

        return foundVars;
    }
}

} // namespace atom::system::virtual_env::detection
