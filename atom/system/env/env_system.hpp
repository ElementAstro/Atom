/*
 * env_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: System information and directories

**************************************************/

#ifndef ATOM_SYSTEM_ENV_SYSTEM_HPP
#define ATOM_SYSTEM_ENV_SYSTEM_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Operating system enumeration
 */
enum class OperatingSystem {
    WINDOWS,
    LINUX,
    MACOS,
    FREEBSD,
    UNKNOWN
};

/**
 * @brief System architecture enumeration
 */
enum class SystemArchitecture {
    X86_64,
    ARM64,
    X86,
    ARM,
    UNKNOWN
};

/**
 * @brief System information cache entry
 */
struct SystemInfoCache {
    String systemName;
    String architecture;
    String hostname;
    String username;
    String homeDir;
    String tempDir;
    String configDir;
    String dataDir;
    std::chrono::steady_clock::time_point timestamp;
    bool isValid;

    SystemInfoCache() : isValid(false) {}
};

/**
 * @brief System information and directories
 */
class EnvSystem {
public:
    /**
     * @brief Gets the user home directory
     * @return The path to the user home directory
     */
    ATOM_NODISCARD static auto getHomeDir() -> String;

    /**
     * @brief Gets the system temporary directory
     * @return The path to the system temporary directory
     */
    ATOM_NODISCARD static auto getTempDir() -> String;

    /**
     * @brief Gets the system configuration directory
     * @return The path to the system configuration directory
     */
    ATOM_NODISCARD static auto getConfigDir() -> String;

    /**
     * @brief Gets the user data directory
     * @return The path to the user data directory
     */
    ATOM_NODISCARD static auto getDataDir() -> String;

    /**
     * @brief Gets the system name
     * @return System name (e.g., "Windows", "Linux", "macOS")
     */
    ATOM_NODISCARD static auto getSystemName() -> String;

    /**
     * @brief Gets the system architecture
     * @return System architecture (e.g., "x86_64", "arm64")
     */
    ATOM_NODISCARD static auto getSystemArch() -> String;

    /**
     * @brief Gets the current username
     * @return Current username
     */
    ATOM_NODISCARD static auto getCurrentUser() -> String;

    /**
     * @brief Gets the hostname
     * @return Hostname
     */
    ATOM_NODISCARD static auto getHostName() -> String;

    // ========== NEW ENHANCED FEATURES ==========

    /**
     * @brief Gets the operating system type
     * @return Operating system enumeration
     */
    ATOM_NODISCARD static auto getOperatingSystem() -> OperatingSystem;

    /**
     * @brief Gets the system architecture type
     * @return System architecture enumeration
     */
    ATOM_NODISCARD static auto getSystemArchitecture() -> SystemArchitecture;

    /**
     * @brief Gets detailed system information
     * @return Map containing comprehensive system information
     */
    ATOM_NODISCARD static auto getSystemInfo() -> HashMap<String, String>;

    /**
     * @brief Gets system uptime in seconds
     * @return System uptime in seconds, or -1 if unavailable
     */
    ATOM_NODISCARD static auto getSystemUptime() -> long long;

    /**
     * @brief Gets available memory information
     * @return Map containing memory statistics (total, available, used)
     */
    ATOM_NODISCARD static auto getMemoryInfo() -> HashMap<String, size_t>;

    /**
     * @brief Gets CPU information
     * @return Map containing CPU details (cores, model, frequency)
     */
    ATOM_NODISCARD static auto getCpuInfo() -> HashMap<String, String>;

    /**
     * @brief Gets disk space information for a path
     * @param path Path to check disk space for
     * @return Map containing disk space info (total, free, used)
     */
    ATOM_NODISCARD static auto getDiskSpaceInfo(const String& path) -> HashMap<String, size_t>;

    /**
     * @brief Gets network interface information
     * @return Vector of network interface details
     */
    ATOM_NODISCARD static auto getNetworkInterfaces() -> Vector<HashMap<String, String>>;

    /**
     * @brief Gets environment variables related to system paths
     * @return Map of system path environment variables
     */
    ATOM_NODISCARD static auto getSystemPathVariables() -> HashMap<String, String>;

    /**
     * @brief Gets the current working directory
     * @return Current working directory path
     */
    ATOM_NODISCARD static auto getCurrentWorkingDirectory() -> String;

    /**
     * @brief Gets the executable directory
     * @return Directory containing the current executable
     */
    ATOM_NODISCARD static auto getExecutableDirectory() -> String;

    /**
     * @brief Gets system locale information
     * @return Map containing locale settings
     */
    ATOM_NODISCARD static auto getLocaleInfo() -> HashMap<String, String>;

    /**
     * @brief Gets timezone information
     * @return Current timezone string
     */
    ATOM_NODISCARD static auto getTimezone() -> String;

    /**
     * @brief Checks if running with elevated privileges
     * @return True if running as administrator/root
     */
    ATOM_NODISCARD static auto isElevated() -> bool;

    /**
     * @brief Gets the process ID of the current process
     * @return Current process ID
     */
    ATOM_NODISCARD static auto getCurrentProcessId() -> int;

    /**
     * @brief Gets the parent process ID
     * @return Parent process ID
     */
    ATOM_NODISCARD static auto getParentProcessId() -> int;

    /**
     * @brief Enables or disables system information caching
     * @param enabled Whether to enable caching
     * @param ttl_seconds Cache time-to-live in seconds (default: 300)
     */
    static void setCachingEnabled(bool enabled, int ttl_seconds = 300);

    /**
     * @brief Clears the system information cache
     */
    static void clearCache();

    /**
     * @brief Gets cache statistics
     * @return Map containing cache statistics
     */
    static auto getCacheStats() -> HashMap<String, size_t>;

    /**
     * @brief Validates system requirements
     * @param requirements Map of requirement checks
     * @return Map of validation results
     */
    static auto validateSystemRequirements(const HashMap<String, String>& requirements)
        -> HashMap<String, bool>;

private:
    // Caching system
    static SystemInfoCache sCache;
    static std::mutex sCacheMutex;
    static std::atomic<bool> sCachingEnabled;
    static std::atomic<int> sCacheTtlSeconds;
    static std::atomic<size_t> sCacheHits;
    static std::atomic<size_t> sCacheMisses;

    // Helper methods
    static auto getCachedValue(const String& key) -> std::optional<String>;
    static void setCachedValue(const String& key, const String& value);
    static auto isCacheValid() -> bool;
    static void updateCache();

    // Platform-specific implementations
    static auto getSystemNameImpl() -> String;
    static auto getSystemArchImpl() -> String;
    static auto getHostNameImpl() -> String;
    static auto getCurrentUserImpl() -> String;
    static auto getUptimeImpl() -> long long;
    static auto getMemoryInfoImpl() -> HashMap<String, size_t>;
    static auto getCpuInfoImpl() -> HashMap<String, String>;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_SYSTEM_HPP
