#include "linux.hpp"
#include "common.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::platform::linux_impl {

auto detectVirtualizationLinux() -> VirtualizationResult {
    VirtualizationResult result;
    result.platform = "Linux";

    // Check /proc/cpuinfo for hypervisor flag
    if (checkCPUInfoHypervisor()) {
        result.indicators.push_back("Hypervisor flag in /proc/cpuinfo");
        result.confidence += 0.3;
    }

    // Check DMI information
    auto dmiInfo = getDMIInfo();
    if (!dmiInfo.empty()) {
        for (const auto& info : dmiInfo) {
            if (containsVMKeywords(info.second)) {
                result.indicators.push_back("DMI: " + info.first + " = " + info.second);
                result.confidence += 0.2;
            }
        }
    }

    // Check for virtualization-specific files
    auto vmFiles = checkVirtualizationFiles();
    for (const auto& file : vmFiles) {
        result.indicators.push_back("VM file: " + file);
        result.confidence += 0.1;
    }

    // Check cgroups for container indicators
    auto cgroupInfo = checkCgroups();
    if (!cgroupInfo.empty()) {
        result.indicators.push_back("Container cgroups detected");
        result.confidence += 0.2;
        result.container_type = cgroupInfo;
    }

    // Check kernel modules
    auto vmModules = checkKernelModules();
    for (const auto& module : vmModules) {
        result.indicators.push_back("VM kernel module: " + module);
        result.confidence += 0.1;
    }

    // Check /proc/devices for VM devices
    auto vmDevices = checkVirtualDevices();
    for (const auto& device : vmDevices) {
        result.indicators.push_back("VM device: " + device);
        result.confidence += 0.1;
    }

    // Check systemd for container environment
    if (checkSystemdContainer()) {
        result.indicators.push_back("systemd container environment");
        result.confidence += 0.2;
    }

    // Limit confidence to 1.0
    result.confidence = std::min(result.confidence, 1.0);
    result.is_virtual = result.confidence > 0.3;

    return result;
}

auto checkCPUInfoHypervisor() -> bool {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("flags") != std::string::npos ||
            line.find("Features") != std::string::npos) {
            if (line.find("hypervisor") != std::string::npos) {
                spdlog::debug("Hypervisor flag found in /proc/cpuinfo");
                return true;
            }
        }
    }

    return false;
}

auto getDMIInfo() -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> dmiInfo;

    std::vector<std::pair<std::string, std::string>> dmiFiles = {
        {"sys_vendor", "/sys/class/dmi/id/sys_vendor"},
        {"product_name", "/sys/class/dmi/id/product_name"},
        {"product_version", "/sys/class/dmi/id/product_version"},
        {"bios_vendor", "/sys/class/dmi/id/bios_vendor"},
        {"bios_version", "/sys/class/dmi/id/bios_version"},
        {"board_vendor", "/sys/class/dmi/id/board_vendor"},
        {"board_name", "/sys/class/dmi/id/board_name"}
    };

    for (const auto& [key, path] : dmiFiles) {
        std::string content = readFileContent(path);
        if (!content.empty()) {
            // Remove trailing newline
            if (content.back() == '\n') {
                content.pop_back();
            }
            dmiInfo[key] = content;
            spdlog::debug("DMI {}: {}", key, content);
        }
    }

    return dmiInfo;
}

auto checkVirtualizationFiles() -> std::vector<std::string> {
    std::vector<std::string> foundFiles;

    std::vector<std::string> vmFiles = {
        "/.dockerenv",
        "/run/.containerenv",
        "/proc/vz",
        "/proc/bc",
        "/sys/hypervisor/uuid",
        "/sys/hypervisor/type",
        "/dev/vmci",
        "/proc/xen",
        "/sys/bus/pci/devices/0000:00:04.0", // VirtualBox
        "/sys/bus/pci/devices/0000:00:0f.0"  // VMware
    };

    for (const auto& file : vmFiles) {
        if (fileExists(file)) {
            foundFiles.push_back(file);
            spdlog::debug("Found virtualization file: {}", file);
        }
    }

    return foundFiles;
}

auto checkCgroups() -> std::string {
    std::ifstream cgroup("/proc/1/cgroup");
    if (!cgroup.is_open()) {
        return {};
    }

    std::string line;
    while (std::getline(cgroup, line)) {
        if (line.find("docker") != std::string::npos) {
            return "Docker";
        }
        if (line.find("lxc") != std::string::npos) {
            return "LXC";
        }
        if (line.find("kubepods") != std::string::npos) {
            return "Kubernetes";
        }
        if (line.find("libpod") != std::string::npos) {
            return "Podman";
        }
        if (line.find("systemd") != std::string::npos &&
            line.find("machine.slice") != std::string::npos) {
            return "systemd-nspawn";
        }
    }

    return {};
}

auto checkKernelModules() -> std::vector<std::string> {
    std::vector<std::string> vmModules;

    std::ifstream modules("/proc/modules");
    if (!modules.is_open()) {
        return vmModules;
    }

    std::vector<std::string> vmModuleNames = {
        "vmw_", "vbox", "virtio", "xen", "kvm", "qemu",
        "vmci", "vmxnet", "vmmouse", "vmwgfx"
    };

    std::string line;
    while (std::getline(modules, line)) {
        for (const auto& vmModule : vmModuleNames) {
            if (line.find(vmModule) != std::string::npos) {
                // Extract module name (first word)
                std::istringstream iss(line);
                std::string moduleName;
                iss >> moduleName;
                vmModules.push_back(moduleName);
                spdlog::debug("Found VM kernel module: {}", moduleName);
                break;
            }
        }
    }

    return vmModules;
}

auto checkVirtualDevices() -> std::vector<std::string> {
    std::vector<std::string> vmDevices;

    std::ifstream devices("/proc/devices");
    if (!devices.is_open()) {
        return vmDevices;
    }

    std::vector<std::string> vmDeviceNames = {
        "vmci", "vboxguest", "vboxuser", "vmware"
    };

    std::string line;
    while (std::getline(devices, line)) {
        for (const auto& vmDevice : vmDeviceNames) {
            if (line.find(vmDevice) != std::string::npos) {
                vmDevices.push_back(line);
                spdlog::debug("Found VM device: {}", line);
                break;
            }
        }
    }

    return vmDevices;
}

auto checkSystemdContainer() -> bool {
    // Check systemd container environment
    const char* container = std::getenv("container");
    if (container) {
        spdlog::debug("Container environment variable: {}", container);
        return true;
    }

    // Check systemd machine info
    std::string machineInfo = readFileContent("/run/systemd/container");
    if (!machineInfo.empty()) {
        spdlog::debug("systemd container info: {}", machineInfo);
        return true;
    }

    return false;
}

auto getLinuxVirtualizationType() -> std::string {
    // Check DMI information first
    auto dmiInfo = getDMIInfo();

    if (dmiInfo.count("sys_vendor")) {
        const std::string& vendor = dmiInfo["sys_vendor"];
        if (vendor.find("VMware") != std::string::npos) {
            return "VMware";
        }
        if (vendor.find("innotek") != std::string::npos ||
            vendor.find("Oracle") != std::string::npos) {
            return "VirtualBox";
        }
        if (vendor.find("Microsoft") != std::string::npos) {
            return "Hyper-V";
        }
        if (vendor.find("QEMU") != std::string::npos) {
            return "QEMU/KVM";
        }
        if (vendor.find("Xen") != std::string::npos) {
            return "Xen";
        }
    }

    // Check for container types
    std::string containerType = checkCgroups();
    if (!containerType.empty()) {
        return containerType;
    }

    // Check hypervisor type from /sys/hypervisor/type
    std::string hypervisorType = readFileContent("/sys/hypervisor/type");
    if (!hypervisorType.empty()) {
        if (hypervisorType.back() == '\n') {
            hypervisorType.pop_back();
        }
        return hypervisorType;
    }

    // Check kernel modules for hints
    auto modules = checkKernelModules();
    for (const auto& module : modules) {
        if (module.find("vmw") != std::string::npos) {
            return "VMware";
        }
        if (module.find("vbox") != std::string::npos) {
            return "VirtualBox";
        }
        if (module.find("virtio") != std::string::npos) {
            return "KVM/QEMU";
        }
        if (module.find("xen") != std::string::npos) {
            return "Xen";
        }
    }

    return "Unknown";
}

auto getLinuxContainerInfo() -> ContainerInfo {
    ContainerInfo info;

    // Check for Docker
    if (fileExists("/.dockerenv")) {
        info.type = "Docker";
        info.runtime = "Docker Engine";

        // Try to get container ID from cgroup
        std::ifstream cgroup("/proc/1/cgroup");
        if (cgroup.is_open()) {
            std::string line;
            while (std::getline(cgroup, line)) {
                if (line.find("docker") != std::string::npos) {
                    size_t lastSlash = line.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        std::string id = line.substr(lastSlash + 1);
                        if (id.length() >= 12) {
                            info.id = id.substr(0, 12);
                        }
                    }
                    break;
                }
            }
        }
    }
    // Check for Podman
    else if (fileExists("/run/.containerenv")) {
        info.type = "Podman";
        info.runtime = "Podman";

        std::string containerEnv = readFileContent("/run/.containerenv");
        // Parse container ID from .containerenv
        size_t idPos = containerEnv.find("id=");
        if (idPos != std::string::npos) {
            size_t start = idPos + 3;
            size_t end = containerEnv.find('\n', start);
            if (end != std::string::npos) {
                info.id = containerEnv.substr(start, end - start);
            }
        }
    }
    // Check for LXC/LXD
    else if (checkCgroups().find("lxc") != std::string::npos) {
        info.type = "LXC";
        info.runtime = "LXC";

        if (fileExists("/run/lxd_config")) {
            info.type = "LXD";
            info.runtime = "LXD";
        }
    }
    // Check for Kubernetes
    else if (fileExists("/var/run/secrets/kubernetes.io/serviceaccount/token")) {
        info.type = "Kubernetes Pod";
        info.runtime = "Kubernetes";

        const char* podName = std::getenv("HOSTNAME");
        if (podName) {
            info.id = std::string(podName);
        }

        std::string ns = readFileContent("/var/run/secrets/kubernetes.io/serviceaccount/namespace");
        if (!ns.empty() && ns.back() == '\n') {
            ns.pop_back();
        }
        info.namespace_name = ns;
    }

    return info;
}

} // namespace atom::system::virtual_env::platform::linux_impl
