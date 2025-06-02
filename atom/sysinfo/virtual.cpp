#include "virtual.hpp"
#include "cpu.hpp"
#include "memory.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
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

namespace atom::system {

namespace {
constexpr int CPUID_HYPERVISOR = 0x40000000;
constexpr int CPUID_FEATURES = 1;
constexpr int VENDOR_STRING_LENGTH = 12;
constexpr int BIOS_INFO_LENGTH = 256;
constexpr int HYPERVISOR_PRESENT_BIT = 31;
constexpr int TIME_DRIFT_UPPER_BOUND = 1050;
constexpr int TIME_DRIFT_LOWER_BOUND = 950;

/**
 * @brief Executes a system command and returns the output as a string
 * @param command The command to execute
 * @return Command output or empty string on failure
 */
auto executeCommand(std::string_view command) -> std::string {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.data(), "r"),
                                                  pclose);

    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return {};
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

/**
 * @brief Checks if a string contains any of the virtualization keywords
 * @param text The text to search
 * @return True if virtualization keywords are found
 */
auto containsVMKeywords(std::string_view text) -> bool {
    constexpr std::array<std::string_view, 8> vmKeywords = {
        "VMware", "VirtualBox", "QEMU",      "Xen",
        "KVM",    "Hyper-V",    "Parallels", "VirtIO"};

    std::string lowerText;
    lowerText.reserve(text.size());
    std::transform(text.begin(), text.end(), std::back_inserter(lowerText),
                   [](char c) { return std::tolower(c); });

    return std::any_of(
        vmKeywords.begin(), vmKeywords.end(),
        [&lowerText](std::string_view keyword) {
            std::string lowerKeyword;
            lowerKeyword.reserve(keyword.size());
            std::transform(keyword.begin(), keyword.end(),
                           std::back_inserter(lowerKeyword),
                           [](char c) { return std::tolower(c); });
            return lowerText.find(lowerKeyword) != std::string::npos;
        });
}
}  // namespace

auto getHypervisorVendor() -> std::string {
    spdlog::debug("Getting hypervisor vendor information");
    std::array<unsigned int, 4> cpuInfo = {0};

#ifdef _WIN32
    __cpuid(reinterpret_cast<int*>(cpuInfo.data()), CPUID_HYPERVISOR);
#else
    __get_cpuid(CPUID_HYPERVISOR, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2],
                &cpuInfo[3]);
#endif

    std::array<char, VENDOR_STRING_LENGTH + 1> vendor = {0};
    std::memcpy(vendor.data(), &cpuInfo[1], 4);
    std::memcpy(vendor.data() + 4, &cpuInfo[2], 4);
    std::memcpy(vendor.data() + 8, &cpuInfo[3], 4);

    std::string vendorStr(vendor.data());
    spdlog::debug("Hypervisor vendor: {}", vendorStr);
    return vendorStr;
}

auto isVirtualMachine() -> bool {
    spdlog::debug("Checking if running in virtual machine using CPUID");
    std::array<unsigned int, 4> cpuInfo = {0};

#ifdef _WIN32
    __cpuid(reinterpret_cast<int*>(cpuInfo.data()), CPUID_FEATURES);
#else
    __get_cpuid(CPUID_FEATURES, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2],
                &cpuInfo[3]);
#endif

    bool isVM = static_cast<bool>(cpuInfo[2] & (1u << HYPERVISOR_PRESENT_BIT));
    spdlog::debug("Virtual machine detected via CPUID: {}", isVM);
    return isVM;
}

auto checkBIOS() -> bool {
    spdlog::debug("Checking BIOS information for virtualization signs");

#ifdef _WIN32
    HKEY hKey;
    std::array<TCHAR, BIOS_INFO_LENGTH> biosInfo;
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

auto parseNetworkAdapterOutput(const std::string& output) -> bool {
    constexpr std::array<std::string_view, 5> vmNetKeywords = {
        "virbr", "vbox", "vmnet", "veth", "docker"};

    return std::any_of(vmNetKeywords.begin(), vmNetKeywords.end(),
                       [&output](std::string_view keyword) {
                           return output.find(keyword) != std::string::npos;
                       }) ||
           containsVMKeywords(output);
}

auto checkNetworkAdapter() -> bool {
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

auto checkDisk() -> bool {
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

auto checkGraphicsCard() -> bool {
    spdlog::debug("Checking graphics card for virtualization indicators");

#ifdef _WIN32
    std::string output =
        executeCommand("wmic path win32_videocontroller get caption");
#else
    std::string output = executeCommand("lspci | grep -i vga");
    if (output.empty()) {
        output = executeCommand(
            "cat /proc/driver/nvidia/cards 2>/dev/null || echo ''");
    }
#endif

    return containsVMKeywords(output);
}

auto checkProcesses() -> bool {
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

auto checkPCIBus() -> bool {
    spdlog::debug("Checking PCI bus for virtualization devices");

#ifdef _WIN32
    std::string output = executeCommand("wmic path Win32_PnPEntity get Name");
#else
    std::string output = executeCommand("lspci");
#endif

    return containsVMKeywords(output);
}

auto checkTimeDrift() -> bool {
    spdlog::debug("Checking for time drift anomalies");

    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    bool timeDrift =
        duration > TIME_DRIFT_UPPER_BOUND || duration < TIME_DRIFT_LOWER_BOUND;
    spdlog::debug("Time drift detected: {} (duration: {}ms)", timeDrift,
                  duration);
    return timeDrift;
}

auto isDockerContainer() -> bool {
    spdlog::debug("Checking for Docker container environment");

#ifdef _WIN32
    return false;
#else
    if (std::filesystem::exists("/.dockerenv")) {
        spdlog::debug("Docker environment file found");
        return true;
    }

    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos) {
                spdlog::debug("Docker container detected in cgroup");
                return true;
            }
        }
    }

    return false;
#endif
}

auto getVirtualizationConfidence() -> double {
    spdlog::debug("Calculating virtualization confidence score");

    struct Check {
        std::function<bool()> func;
        double weight;
        std::string name;
    };

    std::array<Check, 8> checks = {{{isVirtualMachine, 0.25, "CPUID"},
                                    {checkBIOS, 0.20, "BIOS"},
                                    {checkNetworkAdapter, 0.10, "Network"},
                                    {checkDisk, 0.15, "Disk"},
                                    {checkGraphicsCard, 0.10, "Graphics"},
                                    {checkProcesses, 0.05, "Processes"},
                                    {checkPCIBus, 0.10, "PCI Bus"},
                                    {checkTimeDrift, 0.05, "Time Drift"}}};

    double totalWeight = 0.0;
    double evidenceWeight = 0.0;

    for (const auto& check : checks) {
        totalWeight += check.weight;
        try {
            if (check.func()) {
                evidenceWeight += check.weight;
                spdlog::debug("Virtualization indicator found: {}", check.name);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error in {} check: {}", check.name, e.what());
        }
    }

    double confidence = evidenceWeight / totalWeight;
    spdlog::info("Virtualization confidence score: {:.2f}", confidence);
    return confidence;
}

auto getVirtualizationType() -> std::string {
    spdlog::debug("Determining virtualization type");

    std::string vendor = getHypervisorVendor();

    if (vendor.find("VMware") != std::string::npos) {
        return "VMware";
    }
    if (vendor.find("VBoxVBox") != std::string::npos) {
        return "VirtualBox";
    }
    if (vendor.find("Microsoft") != std::string::npos) {
        return "Hyper-V";
    }
    if (vendor.find("KVMKVMKVM") != std::string::npos) {
        return "KVM";
    }
    if (vendor.find("XenVMMXen") != std::string::npos) {
        return "Xen";
    }

    if (checkBIOS() || checkPCIBus()) {
        std::string output;
#ifdef _WIN32
        output = executeCommand("wmic computersystem get manufacturer,model");
#else
        output = executeCommand("cat /sys/class/dmi/id/product_name");
#endif

        if (containsVMKeywords(output)) {
            if (output.find("VMware") != std::string::npos)
                return "VMware";
            if (output.find("VirtualBox") != std::string::npos)
                return "VirtualBox";
            if (output.find("QEMU") != std::string::npos)
                return "QEMU/KVM";
            if (output.find("Xen") != std::string::npos)
                return "Xen";
        }
    }

    return "Unknown";
}

auto isContainer() -> bool {
    spdlog::debug("Checking for container environment");

    if (isDockerContainer()) {
        return true;
    }

#ifndef _WIN32
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("lxc") != std::string::npos ||
                line.find("docker") != std::string::npos ||
                line.find("kubepods") != std::string::npos) {
                return true;
            }
        }
    }

    return std::filesystem::exists("/run/.containerenv") ||
           std::filesystem::exists("/.dockerenv");
#endif

    return false;
}

auto getContainerType() -> std::string {
    spdlog::debug("Determining container type");

    if (!isContainer()) {
        return "";
    }

    if (isDockerContainer()) {
        return "Docker";
    }

#ifndef _WIN32
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("lxc") != std::string::npos) {
                return "LXC";
            }
            if (line.find("kubepods") != std::string::npos) {
                return "Kubernetes";
            }
        }
    }

    if (std::filesystem::exists("/run/.containerenv")) {
        return "Podman";
    }
#endif

    return "Unknown Container";
}

}  // namespace atom::system
