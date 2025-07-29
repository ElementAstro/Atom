/**
 * @file container.hpp
 * @brief Container detection and identification
 *
 * This file contains specialized detection methods for identifying
 * container environments and their characteristics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_VIRTUAL_CONTAINER_HPP
#define ATOM_SYSINFO_VIRTUAL_CONTAINER_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace atom::system::virtual_env::container {

/**
 * @brief Container types
 */
enum class ContainerType {
    UNKNOWN,
    DOCKER,
    LXC,
    LXD,
    PODMAN,
    KUBERNETES_POD,
    SYSTEMD_NSPAWN,
    RKT,
    GARDEN,
    CONTAINERD,
    CRIO,
    FIRECRACKER_MICROVM,
    KATA_CONTAINERS,
    GVISOR,
    WASM_CONTAINER
};

/**
 * @brief Container runtime types
 */
enum class ContainerRuntime {
    UNKNOWN,
    DOCKER_ENGINE,
    CONTAINERD,
    CRIO,
    PODMAN,
    RKT,
    SYSTEMD_NSPAWN,
    LXC,
    LXD,
    GARDEN,
    KATA,
    GVISOR,
    FIRECRACKER,
    WASM_RUNTIME
};

/**
 * @brief Container information structure
 */
struct ContainerInfo {
    ContainerType type = ContainerType::UNKNOWN;
    ContainerRuntime runtime = ContainerRuntime::UNKNOWN;
    std::string name;
    std::string id;
    std::string image;
    std::string version;
    std::vector<std::string> features;
    std::unordered_map<std::string, std::string> metadata;
    std::unordered_map<std::string, std::string> environment;
    double detection_confidence = 0.0;
};

/**
 * @brief Docker-specific detection
 */
namespace docker {
    /**
     * @brief Detect Docker container
     * @return bool True if Docker container detected
     */
    auto detect() -> bool;

    /**
     * @brief Check for .dockerenv file
     * @return bool True if .dockerenv exists
     */
    auto hasDockerEnvFile() -> bool;

    /**
     * @brief Check Docker cgroup information
     * @return bool True if Docker cgroups detected
     */
    auto checkCgroups() -> bool;

    /**
     * @brief Get Docker container ID
     * @return std::string Container ID
     */
    auto getContainerID() -> std::string;

    /**
     * @brief Get Docker image information
     * @return std::string Image name and tag
     */
    auto getImageInfo() -> std::string;

    /**
     * @brief Get Docker version
     * @return std::string Docker version
     */
    auto getDockerVersion() -> std::string;

    /**
     * @brief Get Docker environment variables
     * @return std::unordered_map<std::string, std::string> Environment variables
     */
    auto getDockerEnvironment() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Check for Docker-specific mount points
     * @return bool True if Docker mounts detected
     */
    auto checkDockerMounts() -> bool;
}

/**
 * @brief LXC/LXD-specific detection
 */
namespace lxc {
    /**
     * @brief Detect LXC container
     * @return bool True if LXC container detected
     */
    auto detect() -> bool;

    /**
     * @brief Detect LXD container
     * @return bool True if LXD container detected
     */
    auto detectLXD() -> bool;

    /**
     * @brief Check LXC cgroup information
     * @return bool True if LXC cgroups detected
     */
    auto checkCgroups() -> bool;

    /**
     * @brief Get LXC container name
     * @return std::string Container name
     */
    auto getContainerName() -> std::string;

    /**
     * @brief Get LXC version
     * @return std::string LXC version
     */
    auto getLXCVersion() -> std::string;

    /**
     * @brief Get LXD version
     * @return std::string LXD version
     */
    auto getLXDVersion() -> std::string;

    /**
     * @brief Check for LXC-specific files
     * @return bool True if LXC files detected
     */
    auto checkLXCFiles() -> bool;
}

/**
 * @brief Podman-specific detection
 */
namespace podman {
    /**
     * @brief Detect Podman container
     * @return bool True if Podman container detected
     */
    auto detect() -> bool;

    /**
     * @brief Check for .containerenv file
     * @return bool True if .containerenv exists
     */
    auto hasContainerEnvFile() -> bool;

    /**
     * @brief Get Podman container ID
     * @return std::string Container ID
     */
    auto getContainerID() -> std::string;

    /**
     * @brief Get Podman version
     * @return std::string Podman version
     */
    auto getPodmanVersion() -> std::string;

    /**
     * @brief Check Podman cgroup information
     * @return bool True if Podman cgroups detected
     */
    auto checkCgroups() -> bool;
}

/**
 * @brief Kubernetes-specific detection
 */
namespace kubernetes {
    /**
     * @brief Detect Kubernetes pod
     * @return bool True if Kubernetes pod detected
     */
    auto detect() -> bool;

    /**
     * @brief Check for Kubernetes service account
     * @return bool True if service account detected
     */
    auto hasServiceAccount() -> bool;

    /**
     * @brief Get pod name
     * @return std::string Pod name
     */
    auto getPodName() -> std::string;

    /**
     * @brief Get namespace
     * @return std::string Namespace
     */
    auto getNamespace() -> std::string;

    /**
     * @brief Get node name
     * @return std::string Node name
     */
    auto getNodeName() -> std::string;

    /**
     * @brief Get Kubernetes version
     * @return std::string Kubernetes version
     */
    auto getKubernetesVersion() -> std::string;

    /**
     * @brief Get Kubernetes environment variables
     * @return std::unordered_map<std::string, std::string> Environment variables
     */
    auto getKubernetesEnvironment() -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Check for Kubernetes-specific mount points
     * @return bool True if Kubernetes mounts detected
     */
    auto checkKubernetesMounts() -> bool;
}

/**
 * @brief systemd-nspawn-specific detection
 */
namespace systemd_nspawn {
    /**
     * @brief Detect systemd-nspawn container
     * @return bool True if systemd-nspawn detected
     */
    auto detect() -> bool;

    /**
     * @brief Get container name
     * @return std::string Container name
     */
    auto getContainerName() -> std::string;

    /**
     * @brief Check systemd-nspawn environment
     * @return bool True if systemd-nspawn environment detected
     */
    auto checkEnvironment() -> bool;
}

/**
 * @brief Advanced container runtime detection
 */
namespace runtime {
    /**
     * @brief Detect containerd runtime
     * @return bool True if containerd detected
     */
    auto detectContainerd() -> bool;

    /**
     * @brief Detect CRI-O runtime
     * @return bool True if CRI-O detected
     */
    auto detectCRIO() -> bool;

    /**
     * @brief Detect Kata Containers
     * @return bool True if Kata Containers detected
     */
    auto detectKataContainers() -> bool;

    /**
     * @brief Detect gVisor
     * @return bool True if gVisor detected
     */
    auto detectGVisor() -> bool;

    /**
     * @brief Detect Firecracker microVM
     * @return bool True if Firecracker detected
     */
    auto detectFirecracker() -> bool;

    /**
     * @brief Detect WebAssembly container
     * @return bool True if WASM container detected
     */
    auto detectWASMContainer() -> bool;

    /**
     * @brief Get container runtime information
     * @return std::string Runtime name and version
     */
    auto getRuntimeInfo() -> std::string;
}

/**
 * @brief Container security and isolation detection
 */
namespace security {
    /**
     * @brief Check for privileged container
     * @return bool True if running in privileged mode
     */
    auto isPrivileged() -> bool;

    /**
     * @brief Check for security profiles (AppArmor, SELinux)
     * @return std::vector<std::string> Active security profiles
     */
    auto getSecurityProfiles() -> std::vector<std::string>;

    /**
     * @brief Check for capability restrictions
     * @return std::vector<std::string> Available capabilities
     */
    auto getCapabilities() -> std::vector<std::string>;

    /**
     * @brief Check for namespace isolation
     * @return std::unordered_map<std::string, bool> Namespace isolation status
     */
    auto getNamespaceIsolation() -> std::unordered_map<std::string, bool>;

    /**
     * @brief Check for seccomp profile
     * @return bool True if seccomp profile active
     */
    auto hasSeccompProfile() -> bool;
}

/**
 * @brief General container detection and identification
 */

/**
 * @brief Detect container type
 * @return ContainerType Detected container type
 */
auto detectContainerType() -> ContainerType;

/**
 * @brief Detect container runtime
 * @return ContainerRuntime Detected container runtime
 */
auto detectContainerRuntime() -> ContainerRuntime;

/**
 * @brief Get comprehensive container information
 * @return ContainerInfo Complete container details
 */
auto getContainerInfo() -> ContainerInfo;

/**
 * @brief Convert container type to string
 * @param type Container type
 * @return std::string String representation
 */
auto containerTypeToString(ContainerType type) -> std::string;

/**
 * @brief Convert container runtime to string
 * @param runtime Container runtime
 * @return std::string String representation
 */
auto containerRuntimeToString(ContainerRuntime runtime) -> std::string;

/**
 * @brief Check if running in any container
 * @return bool True if container detected
 */
auto isContainer() -> bool;

/**
 * @brief Get container orchestration platform
 * @return std::string Orchestration platform (e.g., "Kubernetes", "Docker Swarm")
 */
auto getOrchestrationPlatform() -> std::string;

/**
 * @brief Check for container networking mode
 * @return std::string Networking mode (e.g., "bridge", "host", "none")
 */
auto getNetworkingMode() -> std::string;

/**
 * @brief Get container resource limits
 * @return std::unordered_map<std::string, std::string> Resource limits
 */
auto getResourceLimits() -> std::unordered_map<std::string, std::string>;

} // namespace atom::system::virtual_env::container

#endif // ATOM_SYSINFO_VIRTUAL_CONTAINER_HPP
