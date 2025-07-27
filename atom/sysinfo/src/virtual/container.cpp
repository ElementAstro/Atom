#include "container.hpp"
#include "common.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env::container {

auto containerTypeToString(ContainerType type) -> std::string {
    switch (type) {
        case ContainerType::DOCKER: return "Docker";
        case ContainerType::LXC: return "LXC";
        case ContainerType::LXD: return "LXD";
        case ContainerType::PODMAN: return "Podman";
        case ContainerType::KUBERNETES_POD: return "Kubernetes Pod";
        case ContainerType::SYSTEMD_NSPAWN: return "systemd-nspawn";
        case ContainerType::RKT: return "rkt";
        case ContainerType::GARDEN: return "Garden";
        case ContainerType::CONTAINERD: return "containerd";
        case ContainerType::CRIO: return "CRI-O";
        case ContainerType::FIRECRACKER_MICROVM: return "Firecracker microVM";
        case ContainerType::KATA_CONTAINERS: return "Kata Containers";
        case ContainerType::GVISOR: return "gVisor";
        case ContainerType::WASM_CONTAINER: return "WebAssembly Container";
        case ContainerType::UNKNOWN:
        default: return "Unknown";
    }
}

auto containerRuntimeToString(ContainerRuntime runtime) -> std::string {
    switch (runtime) {
        case ContainerRuntime::DOCKER_ENGINE: return "Docker Engine";
        case ContainerRuntime::CONTAINERD: return "containerd";
        case ContainerRuntime::CRIO: return "CRI-O";
        case ContainerRuntime::PODMAN: return "Podman";
        case ContainerRuntime::RKT: return "rkt";
        case ContainerRuntime::SYSTEMD_NSPAWN: return "systemd-nspawn";
        case ContainerRuntime::LXC: return "LXC";
        case ContainerRuntime::LXD: return "LXD";
        case ContainerRuntime::GARDEN: return "Garden";
        case ContainerRuntime::KATA: return "Kata";
        case ContainerRuntime::GVISOR: return "gVisor";
        case ContainerRuntime::FIRECRACKER: return "Firecracker";
        case ContainerRuntime::WASM_RUNTIME: return "WebAssembly Runtime";
        case ContainerRuntime::UNKNOWN:
        default: return "Unknown";
    }
}

auto isContainer() -> bool {
    return docker::detect() || lxc::detect() || podman::detect() || 
           kubernetes::detect() || systemd_nspawn::detect();
}

auto detectContainerType() -> ContainerType {
    if (docker::detect()) return ContainerType::DOCKER;
    if (kubernetes::detect()) return ContainerType::KUBERNETES_POD;
    if (podman::detect()) return ContainerType::PODMAN;
    if (lxc::detectLXD()) return ContainerType::LXD;
    if (lxc::detect()) return ContainerType::LXC;
    if (systemd_nspawn::detect()) return ContainerType::SYSTEMD_NSPAWN;
    if (runtime::detectKataContainers()) return ContainerType::KATA_CONTAINERS;
    if (runtime::detectGVisor()) return ContainerType::GVISOR;
    if (runtime::detectFirecracker()) return ContainerType::FIRECRACKER_MICROVM;
    if (runtime::detectWASMContainer()) return ContainerType::WASM_CONTAINER;
    
    return ContainerType::UNKNOWN;
}

auto detectContainerRuntime() -> ContainerRuntime {
    if (runtime::detectContainerd()) return ContainerRuntime::CONTAINERD;
    if (runtime::detectCRIO()) return ContainerRuntime::CRIO;
    if (docker::detect()) return ContainerRuntime::DOCKER_ENGINE;
    if (podman::detect()) return ContainerRuntime::PODMAN;
    if (lxc::detectLXD()) return ContainerRuntime::LXD;
    if (lxc::detect()) return ContainerRuntime::LXC;
    if (systemd_nspawn::detect()) return ContainerRuntime::SYSTEMD_NSPAWN;
    if (runtime::detectKataContainers()) return ContainerRuntime::KATA;
    if (runtime::detectGVisor()) return ContainerRuntime::GVISOR;
    if (runtime::detectFirecracker()) return ContainerRuntime::FIRECRACKER;
    if (runtime::detectWASMContainer()) return ContainerRuntime::WASM_RUNTIME;
    
    return ContainerRuntime::UNKNOWN;
}

auto getContainerInfo() -> ContainerInfo {
    ContainerInfo info;
    info.type = detectContainerType();
    info.runtime = detectContainerRuntime();
    info.name = containerTypeToString(info.type);
    
    switch (info.type) {
        case ContainerType::DOCKER:
            info.id = docker::getContainerID();
            info.image = docker::getImageInfo();
            info.version = docker::getDockerVersion();
            info.environment = docker::getDockerEnvironment();
            info.detection_confidence = 0.95;
            break;
            
        case ContainerType::KUBERNETES_POD:
            info.id = kubernetes::getPodName();
            info.metadata["namespace"] = kubernetes::getNamespace();
            info.metadata["node"] = kubernetes::getNodeName();
            info.version = kubernetes::getKubernetesVersion();
            info.environment = kubernetes::getKubernetesEnvironment();
            info.detection_confidence = 0.90;
            break;
            
        case ContainerType::PODMAN:
            info.id = podman::getContainerID();
            info.version = podman::getPodmanVersion();
            info.detection_confidence = 0.85;
            break;
            
        case ContainerType::LXC:
            info.id = lxc::getContainerName();
            info.version = lxc::getLXCVersion();
            info.detection_confidence = 0.80;
            break;
            
        case ContainerType::LXD:
            info.id = lxc::getContainerName();
            info.version = lxc::getLXDVersion();
            info.detection_confidence = 0.80;
            break;
            
        case ContainerType::SYSTEMD_NSPAWN:
            info.id = systemd_nspawn::getContainerName();
            info.detection_confidence = 0.75;
            break;
            
        default:
            info.detection_confidence = 0.0;
            break;
    }
    
    return info;
}

auto getOrchestrationPlatform() -> std::string {
    if (kubernetes::detect()) {
        return "Kubernetes";
    }
    
    // Check for Docker Swarm
    const char* swarmNodeId = std::getenv("DOCKER_SWARM_NODE_ID");
    if (swarmNodeId) {
        return "Docker Swarm";
    }
    
    // Check for other orchestration platforms
    if (std::getenv("MESOS_TASK_ID")) {
        return "Apache Mesos";
    }
    
    if (std::getenv("NOMAD_TASK_NAME")) {
        return "HashiCorp Nomad";
    }
    
    return "None";
}

auto getNetworkingMode() -> std::string {
    // This is a simplified implementation
    // In practice, you'd need to check container runtime-specific configurations
    
    if (docker::detect()) {
        // Check Docker networking mode
        std::string networkInfo = executeCommand("cat /proc/1/net/route 2>/dev/null || echo ''");
        if (networkInfo.find("docker0") != std::string::npos) {
            return "bridge";
        }
        
        // Check for host networking
        std::string hostname = executeCommand("hostname");
        std::string hostHostname = executeCommand("cat /proc/sys/kernel/hostname 2>/dev/null || echo ''");
        if (hostname == hostHostname) {
            return "host";
        }
    }
    
    return "default";
}

auto getResourceLimits() -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> limits;
    
    // Check cgroup limits
    std::string memoryLimit = readFileContent("/sys/fs/cgroup/memory/memory.limit_in_bytes");
    if (!memoryLimit.empty() && memoryLimit != "9223372036854775807\n") {
        limits["memory"] = memoryLimit;
    }
    
    std::string cpuQuota = readFileContent("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
    if (!cpuQuota.empty() && cpuQuota != "-1\n") {
        limits["cpu_quota"] = cpuQuota;
    }
    
    std::string cpuPeriod = readFileContent("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
    if (!cpuPeriod.empty()) {
        limits["cpu_period"] = cpuPeriod;
    }
    
    return limits;
}

namespace docker {
    auto detect() -> bool {
        spdlog::debug("Checking for Docker container environment");
        
        if (hasDockerEnvFile()) {
            spdlog::debug("Docker environment file found");
            return true;
        }
        
        return checkCgroups();
    }
    
    auto hasDockerEnvFile() -> bool {
        return fileExists("/.dockerenv");
    }
    
    auto checkCgroups() -> bool {
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
    }
    
    auto getContainerID() -> std::string {
        std::ifstream cgroup("/proc/1/cgroup");
        if (cgroup.is_open()) {
            std::string line;
            while (std::getline(cgroup, line)) {
                if (line.find("docker") != std::string::npos) {
                    // Extract container ID from cgroup path
                    size_t lastSlash = line.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        std::string id = line.substr(lastSlash + 1);
                        if (id.length() >= 12) {
                            return id.substr(0, 12); // Return short ID
                        }
                    }
                }
            }
        }
        return "Unknown";
    }
    
    auto getImageInfo() -> std::string {
        // Try to get image info from environment variables
        const char* imageName = std::getenv("DOCKER_IMAGE");
        if (imageName) {
            return std::string(imageName);
        }
        
        // Try to get from hostname (often set to container ID)
        std::string hostname = executeCommand("hostname");
        if (hostname.length() == 12) {
            return "Unknown (ID: " + hostname + ")";
        }
        
        return "Unknown";
    }
    
    auto getDockerVersion() -> std::string {
        const char* version = std::getenv("DOCKER_VERSION");
        if (version) {
            return std::string(version);
        }
        return "Unknown";
    }
    
    auto getDockerEnvironment() -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> env;
        
        std::vector<std::string> dockerEnvVars = {
            "DOCKER_IMAGE", "DOCKER_VERSION", "DOCKER_CONTAINER",
            "HOSTNAME", "PATH", "HOME"
        };
        
        for (const auto& var : dockerEnvVars) {
            const char* value = std::getenv(var.c_str());
            if (value) {
                env[var] = std::string(value);
            }
        }
        
        return env;
    }
    
    auto checkDockerMounts() -> bool {
        std::string mounts = readFileContent("/proc/mounts");
        return mounts.find("overlay") != std::string::npos ||
               mounts.find("aufs") != std::string::npos ||
               mounts.find("devicemapper") != std::string::npos;
    }
}

namespace lxc {
    auto detect() -> bool {
        std::ifstream cgroup("/proc/1/cgroup");
        if (cgroup.is_open()) {
            std::string line;
            while (std::getline(cgroup, line)) {
                if (line.find("lxc") != std::string::npos) {
                    return true;
                }
            }
        }

        return checkLXCFiles();
    }

    auto detectLXD() -> bool {
        return fileExists("/run/lxd_config") ||
               std::getenv("LXD_DIR") != nullptr;
    }

    auto checkCgroups() -> bool {
        return detect();
    }

    auto getContainerName() -> std::string {
        const char* name = std::getenv("container");
        if (name) {
            return std::string(name);
        }

        // Try to extract from hostname
        std::string hostname = executeCommand("hostname");
        return hostname.empty() ? "Unknown" : hostname;
    }

    auto getLXCVersion() -> std::string {
        std::string output = executeCommand("lxc-info --version 2>/dev/null || echo ''");
        return output.empty() ? "Unknown" : output;
    }

    auto getLXDVersion() -> std::string {
        std::string output = executeCommand("lxd --version 2>/dev/null || echo ''");
        return output.empty() ? "Unknown" : output;
    }

    auto checkLXCFiles() -> bool {
        return fileExists("/proc/1/environ") &&
               readFileContent("/proc/1/environ").find("container=lxc") != std::string::npos;
    }
}

namespace podman {
    auto detect() -> bool {
        if (hasContainerEnvFile()) {
            return true;
        }

        return checkCgroups();
    }

    auto hasContainerEnvFile() -> bool {
        return fileExists("/run/.containerenv");
    }

    auto getContainerID() -> std::string {
        if (hasContainerEnvFile()) {
            std::string content = readFileContent("/run/.containerenv");
            // Parse container ID from .containerenv file
            size_t idPos = content.find("id=");
            if (idPos != std::string::npos) {
                size_t start = idPos + 3;
                size_t end = content.find('\n', start);
                if (end != std::string::npos) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "Unknown";
    }

    auto getPodmanVersion() -> std::string {
        std::string output = executeCommand("podman --version 2>/dev/null || echo ''");
        return output.empty() ? "Unknown" : output;
    }

    auto checkCgroups() -> bool {
        std::ifstream cgroup("/proc/1/cgroup");
        if (cgroup.is_open()) {
            std::string line;
            while (std::getline(cgroup, line)) {
                if (line.find("libpod") != std::string::npos) {
                    return true;
                }
            }
        }
        return false;
    }
}

namespace kubernetes {
    auto detect() -> bool {
        return hasServiceAccount() ||
               std::getenv("KUBERNETES_SERVICE_HOST") != nullptr ||
               checkKubernetesMounts();
    }

    auto hasServiceAccount() -> bool {
        return fileExists("/var/run/secrets/kubernetes.io/serviceaccount/token");
    }

    auto getPodName() -> std::string {
        const char* podName = std::getenv("HOSTNAME");
        if (podName) {
            return std::string(podName);
        }

        // Try to get from downward API
        std::string podNameFile = readFileContent("/etc/podinfo/name");
        return podNameFile.empty() ? "Unknown" : podNameFile;
    }

    auto getNamespace() -> std::string {
        const char* ns = std::getenv("POD_NAMESPACE");
        if (ns) {
            return std::string(ns);
        }

        std::string nsFile = readFileContent("/var/run/secrets/kubernetes.io/serviceaccount/namespace");
        return nsFile.empty() ? "default" : nsFile;
    }

    auto getNodeName() -> std::string {
        const char* nodeName = std::getenv("NODE_NAME");
        if (nodeName) {
            return std::string(nodeName);
        }

        return "Unknown";
    }

    auto getKubernetesVersion() -> std::string {
        const char* version = std::getenv("KUBERNETES_VERSION");
        if (version) {
            return std::string(version);
        }

        return "Unknown";
    }

    auto getKubernetesEnvironment() -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> env;

        std::vector<std::string> k8sEnvVars = {
            "KUBERNETES_SERVICE_HOST", "KUBERNETES_SERVICE_PORT",
            "POD_NAME", "POD_NAMESPACE", "NODE_NAME",
            "SERVICE_ACCOUNT", "HOSTNAME"
        };

        for (const auto& var : k8sEnvVars) {
            const char* value = std::getenv(var.c_str());
            if (value) {
                env[var] = std::string(value);
            }
        }

        return env;
    }

    auto checkKubernetesMounts() -> bool {
        std::string mounts = readFileContent("/proc/mounts");
        return mounts.find("kubernetes.io") != std::string::npos ||
               mounts.find("serviceaccount") != std::string::npos;
    }
}

namespace systemd_nspawn {
    auto detect() -> bool {
        const char* container = std::getenv("container");
        if (container && std::string(container) == "systemd-nspawn") {
            return true;
        }

        return checkEnvironment();
    }

    auto getContainerName() -> std::string {
        const char* name = std::getenv("SYSTEMD_NSPAWN_CONTAINER_NAME");
        if (name) {
            return std::string(name);
        }

        std::string hostname = executeCommand("hostname");
        return hostname.empty() ? "Unknown" : hostname;
    }

    auto checkEnvironment() -> bool {
        // Check for systemd-nspawn specific files
        return fileExists("/run/systemd/container") ||
               fileExists("/etc/machine-id");
    }
}

namespace runtime {
    auto detectContainerd() -> bool {
        std::string processes = executeCommand("ps aux | grep containerd || echo ''");
        return processes.find("containerd") != std::string::npos;
    }

    auto detectCRIO() -> bool {
        std::string processes = executeCommand("ps aux | grep crio || echo ''");
        return processes.find("crio") != std::string::npos ||
               fileExists("/var/run/crio/crio.sock");
    }

    auto detectKataContainers() -> bool {
        std::string processes = executeCommand("ps aux | grep kata || echo ''");
        return processes.find("kata") != std::string::npos ||
               fileExists("/opt/kata/bin/kata-runtime");
    }

    auto detectGVisor() -> bool {
        std::string processes = executeCommand("ps aux | grep runsc || echo ''");
        return processes.find("runsc") != std::string::npos;
    }

    auto detectFirecracker() -> bool {
        std::string processes = executeCommand("ps aux | grep firecracker || echo ''");
        return processes.find("firecracker") != std::string::npos ||
               (fileExists("/dev/kvm") &&
                executeCommand("dmesg | grep -i firecracker").find("firecracker") != std::string::npos);
    }

    auto detectWASMContainer() -> bool {
        // Check for WebAssembly runtime indicators
        std::string processes = executeCommand("ps aux | grep -E 'wasmtime|wasmer|wasm3' || echo ''");
        return !processes.empty() &&
               (processes.find("wasmtime") != std::string::npos ||
                processes.find("wasmer") != std::string::npos ||
                processes.find("wasm3") != std::string::npos);
    }

    auto getRuntimeInfo() -> std::string {
        if (detectContainerd()) {
            std::string version = executeCommand("containerd --version 2>/dev/null || echo ''");
            return "containerd " + version;
        }

        if (detectCRIO()) {
            std::string version = executeCommand("crio --version 2>/dev/null || echo ''");
            return "CRI-O " + version;
        }

        if (detectKataContainers()) {
            return "Kata Containers";
        }

        if (detectGVisor()) {
            return "gVisor";
        }

        if (detectFirecracker()) {
            return "Firecracker";
        }

        if (detectWASMContainer()) {
            return "WebAssembly Runtime";
        }

        return "Unknown";
    }
}

namespace security {
    auto isPrivileged() -> bool {
        // Check if running in privileged mode
        std::string capeff = readFileContent("/proc/1/status");

        // Look for CapEff line
        size_t capPos = capeff.find("CapEff:");
        if (capPos != std::string::npos) {
            std::string caps = capeff.substr(capPos + 7);
            size_t newlinePos = caps.find('\n');
            if (newlinePos != std::string::npos) {
                caps = caps.substr(0, newlinePos);
            }

            // If all capabilities are set, it's likely privileged
            return caps.find("ffffffff") != std::string::npos;
        }

        return false;
    }

    auto getSecurityProfiles() -> std::vector<std::string> {
        std::vector<std::string> profiles;

        // Check for AppArmor
        if (fileExists("/sys/kernel/security/apparmor")) {
            std::string apparmorStatus = readFileContent("/proc/1/attr/current");
            if (!apparmorStatus.empty() && apparmorStatus != "unconfined\n") {
                profiles.push_back("AppArmor: " + apparmorStatus);
            }
        }

        // Check for SELinux
        if (fileExists("/sys/fs/selinux")) {
            std::string selinuxStatus = readFileContent("/proc/1/attr/current");
            if (!selinuxStatus.empty()) {
                profiles.push_back("SELinux: " + selinuxStatus);
            }
        }

        return profiles;
    }

    auto getCapabilities() -> std::vector<std::string> {
        std::vector<std::string> capabilities;

        std::string capStatus = readFileContent("/proc/1/status");

        // Parse capability information
        std::istringstream stream(capStatus);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("Cap") == 0) {
                capabilities.push_back(line);
            }
        }

        return capabilities;
    }

    auto getNamespaceIsolation() -> std::unordered_map<std::string, bool> {
        std::unordered_map<std::string, bool> namespaces;

        // Check namespace isolation
        std::string nsInfo = readFileContent("/proc/1/ns");

        namespaces["pid"] = fileExists("/proc/1/ns/pid");
        namespaces["net"] = fileExists("/proc/1/ns/net");
        namespaces["mnt"] = fileExists("/proc/1/ns/mnt");
        namespaces["uts"] = fileExists("/proc/1/ns/uts");
        namespaces["ipc"] = fileExists("/proc/1/ns/ipc");
        namespaces["user"] = fileExists("/proc/1/ns/user");
        namespaces["cgroup"] = fileExists("/proc/1/ns/cgroup");

        return namespaces;
    }

    auto hasSeccompProfile() -> bool {
        std::string seccompStatus = readFileContent("/proc/1/status");

        size_t seccompPos = seccompStatus.find("Seccomp:");
        if (seccompPos != std::string::npos) {
            std::string seccomp = seccompStatus.substr(seccompPos + 8);
            size_t newlinePos = seccomp.find('\n');
            if (newlinePos != std::string::npos) {
                seccomp = seccomp.substr(0, newlinePos);
            }

            // 0 = disabled, 1 = strict, 2 = filter
            return seccomp.find("2") != std::string::npos ||
                   seccomp.find("1") != std::string::npos;
        }

        return false;
    }
}

} // namespace atom::system::virtual_env::container
