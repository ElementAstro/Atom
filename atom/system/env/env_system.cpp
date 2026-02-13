/*
 * env_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: System information and directories implementation

**************************************************/

#include "env_system.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
// Note: Some Windows headers may not be available in all environments
#else
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "env_core.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

// Static member initializations
SystemInfoCache EnvSystem::sCache;
std::mutex EnvSystem::sCacheMutex;
std::atomic<bool> EnvSystem::sCachingEnabled{false};
std::atomic<int> EnvSystem::sCacheTtlSeconds{300};
std::atomic<size_t> EnvSystem::sCacheHits{0};
std::atomic<size_t> EnvSystem::sCacheMisses{0};

auto EnvSystem::getHomeDir() -> String {
    spdlog::debug("Getting home directory");
    String homePath;

#ifdef _WIN32
    homePath = EnvCore::getEnv("USERPROFILE", "");
    if (homePath.empty()) {
        String homeDrive = EnvCore::getEnv("HOMEDRIVE", "");
        String homePath2 = EnvCore::getEnv("HOMEPATH", "");
        if (!homeDrive.empty() && !homePath2.empty()) {
            homePath = homeDrive + homePath2;
        }
    }
#else
    homePath = EnvCore::getEnv("HOME", "");
    if (homePath.empty()) {
        // Try to get from passwd
        uid_t uid = geteuid();
        struct passwd* pw = getpwuid(uid);
        if (pw && pw->pw_dir) {
            homePath = pw->pw_dir;
        }
    }
#endif

    if (homePath.empty()) {
        spdlog::error("Failed to determine home directory");
    } else {
        spdlog::debug("Home directory: {}", homePath);
    }

    return homePath;
}

auto EnvSystem::getTempDir() -> String {
    spdlog::debug("Getting temporary directory");
    String tempPath;

#ifdef _WIN32
    tempPath = EnvCore::getEnv("TEMP", "");
    if (tempPath.empty()) {
        tempPath = EnvCore::getEnv("TMP", "");
    }
    if (tempPath.empty()) {
        tempPath = "C:\\Windows\\Temp";
    }
#else
    tempPath = EnvCore::getEnv("TMPDIR", "");
    if (tempPath.empty()) {
        tempPath = "/tmp";
    }
#endif

    spdlog::debug("Temporary directory: {}", tempPath);
    return tempPath;
}

auto EnvSystem::getConfigDir() -> String {
    spdlog::debug("Getting configuration directory");
    String configPath;

#ifdef _WIN32
    configPath = EnvCore::getEnv("APPDATA", "");
    if (configPath.empty()) {
        String userProfile = getHomeDir();
        if (!userProfile.empty()) {
            configPath = userProfile + "\\AppData\\Roaming";
        }
    }
#elif defined(__APPLE__)
    String home = getHomeDir();
    if (!home.empty()) {
        configPath = home + "/Library/Application Support";
    }
#else
    configPath = EnvCore::getEnv("XDG_CONFIG_HOME", "");
    if (configPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            configPath = home + "/.config";
        }
    }
#endif

    spdlog::debug("Configuration directory: {}", configPath);
    return configPath;
}

auto EnvSystem::getDataDir() -> String {
    spdlog::debug("Getting data directory");
    String dataPath;

#ifdef _WIN32
    dataPath = EnvCore::getEnv("LOCALAPPDATA", "");
    if (dataPath.empty()) {
        String userProfile = getHomeDir();
        if (!userProfile.empty()) {
            dataPath = userProfile + "\\AppData\\Local";
        }
    }
#elif defined(__APPLE__)
    String home = getHomeDir();
    if (!home.empty()) {
        dataPath = home + "/Library/Application Support";
    }
#else
    dataPath = EnvCore::getEnv("XDG_DATA_HOME", "");
    if (dataPath.empty()) {
        String home = getHomeDir();
        if (!home.empty()) {
            dataPath = home + "/.local/share";
        }
    }
#endif

    spdlog::debug("Data directory: {}", dataPath);
    return dataPath;
}

auto EnvSystem::getSystemName() -> String {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__unix__)
    return "Unix";
#else
    return "Unknown";
#endif
}

auto EnvSystem::getSystemArch() -> String {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

auto EnvSystem::getCurrentUser() -> String {
    String username;

#ifdef _WIN32
    DWORD size = 256;
    char buffer[256];
    if (GetUserNameA(buffer, &size)) {
        username = String(buffer);
    } else {
        spdlog::error("getCurrentUser: GetUserNameA failed with error {}",
                      GetLastError());
        username = EnvCore::getEnv("USERNAME", "unknown");
    }
#else
    username = EnvCore::getEnv("USER", "");
    if (username.empty()) {
        username = EnvCore::getEnv("LOGNAME", "");
    }

    if (username.empty()) {
        // Try to get from passwd
        uid_t uid = geteuid();
        struct passwd* pw = getpwuid(uid);
        if (pw) {
            username = pw->pw_name;
        } else {
            username = "unknown";
        }
    }
#endif

    spdlog::info("getCurrentUser returning: {}", username);
    return username;
}

auto EnvSystem::getHostName() -> String {
    spdlog::info("getHostName called");

    String hostname;

#ifdef _WIN32
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    if (GetComputerNameA(buffer, &size)) {
        hostname = String(buffer, size);
    } else {
        spdlog::error("getHostName: GetComputerNameA failed with error {}",
                      GetLastError());

        hostname = EnvCore::getEnv("COMPUTERNAME", "unknown");
    }
#else
    char buffer[HOST_NAME_MAX + 1];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        hostname = buffer;
    } else {
        spdlog::error("getHostName: gethostname failed with error {}", errno);
        hostname = EnvCore::getEnv("HOSTNAME", "unknown");
    }
#endif

    spdlog::info("getHostName returning: {}", hostname);
    return hostname;
}

// ========== NEW ENHANCED FUNCTIONALITY ==========

auto EnvSystem::getOperatingSystem() -> OperatingSystem {
#ifdef _WIN32
    return OperatingSystem::WINDOWS;
#elif defined(__APPLE__)
    return OperatingSystem::MACOS;
#elif defined(__linux__)
    return OperatingSystem::LINUX;
#elif defined(__FreeBSD__)
    return OperatingSystem::FREEBSD;
#else
    return OperatingSystem::UNKNOWN;
#endif
}

auto EnvSystem::getSystemArchitecture() -> SystemArchitecture {
    String arch = getSystemArch();
    if (arch == "x86_64" || arch == "amd64") {
        return SystemArchitecture::X86_64;
    } else if (arch == "arm64" || arch == "aarch64") {
        return SystemArchitecture::ARM64;
    } else if (arch == "x86" || arch == "i386" || arch == "i686") {
        return SystemArchitecture::X86;
    } else if (arch == "arm") {
        return SystemArchitecture::ARM;
    } else {
        return SystemArchitecture::UNKNOWN;
    }
}

auto EnvSystem::getSystemInfo() -> HashMap<String, String> {
    HashMap<String, String> info;

    info["os"] = getSystemName();
    info["architecture"] = getSystemArch();
    info["hostname"] = getHostName();
    info["username"] = getCurrentUser();
    info["home_dir"] = getHomeDir();
    info["temp_dir"] = getTempDir();
    info["config_dir"] = getConfigDir();
    info["data_dir"] = getDataDir();
    info["cwd"] = getCurrentWorkingDirectory();
    info["executable_dir"] = getExecutableDirectory();
    info["timezone"] = getTimezone();
    info["elevated"] = isElevated() ? "true" : "false";
    info["process_id"] = std::to_string(getCurrentProcessId());
    info["parent_process_id"] = std::to_string(getParentProcessId());

    auto uptime = getSystemUptime();
    if (uptime >= 0) {
        info["uptime_seconds"] = std::to_string(uptime);
    }

    return info;
}

auto EnvSystem::getSystemUptime() -> long long {
#ifdef _WIN32
    return static_cast<long long>(GetTickCount64() / 1000);
#else
    std::ifstream uptime_file("/proc/uptime");
    if (uptime_file.is_open()) {
        double uptime;
        uptime_file >> uptime;
        return static_cast<long long>(uptime);
    }
    return -1;
#endif
}

auto EnvSystem::getMemoryInfo() -> HashMap<String, size_t> {
    HashMap<String, size_t> memInfo;

#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        memInfo["total"] = static_cast<size_t>(memStatus.ullTotalPhys);
        memInfo["available"] = static_cast<size_t>(memStatus.ullAvailPhys);
        memInfo["used"] = memInfo["total"] - memInfo["available"];
    }
#else
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                size_t value = std::stoull(line.substr(line.find_last_of(' ') + 1)) * 1024;
                memInfo["total"] = value;
            } else if (line.find("MemAvailable:") == 0) {
                size_t value = std::stoull(line.substr(line.find_last_of(' ') + 1)) * 1024;
                memInfo["available"] = value;
            }
        }
        if (memInfo.find("total") != memInfo.end() && memInfo.find("available") != memInfo.end()) {
            memInfo["used"] = memInfo["total"] - memInfo["available"];
        }
    }
#endif

    return memInfo;
}

auto EnvSystem::getCpuInfo() -> HashMap<String, String> {
    HashMap<String, String> cpuInfo;

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    cpuInfo["cores"] = std::to_string(sysInfo.dwNumberOfProcessors);
    cpuInfo["architecture"] = getSystemArch();
#else
    cpuInfo["cores"] = std::to_string(sysconf(_SC_NPROCESSORS_ONLN));

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    cpuInfo["model"] = line.substr(pos + 2);
                    break;
                }
            }
        }
    }
#endif

    return cpuInfo;
}

auto EnvSystem::getDiskSpaceInfo(const String& path) -> HashMap<String, size_t> {
    HashMap<String, size_t> diskInfo;

#ifdef _WIN32
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        diskInfo["total"] = static_cast<size_t>(totalNumberOfBytes.QuadPart);
        diskInfo["free"] = static_cast<size_t>(totalNumberOfFreeBytes.QuadPart);
        diskInfo["used"] = diskInfo["total"] - diskInfo["free"];
    }
#else
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) == 0) {
        diskInfo["total"] = static_cast<size_t>(stat.f_blocks * stat.f_frsize);
        diskInfo["free"] = static_cast<size_t>(stat.f_bavail * stat.f_frsize);
        diskInfo["used"] = diskInfo["total"] - diskInfo["free"];
    }
#endif

    return diskInfo;
}

auto EnvSystem::getCurrentWorkingDirectory() -> String {
    try {
        return String(std::filesystem::current_path().string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to get current working directory: {}", e.what());
        return "";
    }
}

auto EnvSystem::getExecutableDirectory() -> String {
    try {
#ifdef _WIN32
        char path[MAX_PATH];
        if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
            std::filesystem::path exePath(path);
            return String(exePath.parent_path().string());
        }
#else
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            std::filesystem::path exePath(path);
            return String(exePath.parent_path().string());
        }
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to get executable directory: {}", e.what());
    }
    return "";
}

auto EnvSystem::getTimezone() -> String {
#ifdef _WIN32
    TIME_ZONE_INFORMATION tzInfo;
    DWORD result = GetTimeZoneInformation(&tzInfo);
    if (result != TIME_ZONE_ID_INVALID) {
        // Convert wide string to regular string
        char tzName[256];
        WideCharToMultiByte(CP_UTF8, 0, tzInfo.StandardName, -1, tzName, sizeof(tzName), nullptr, nullptr);
        return String(tzName);
    }
#else
    const char* tz = getenv("TZ");
    if (tz) {
        return String(tz);
    }

    // Try to read from /etc/timezone
    std::ifstream tzFile("/etc/timezone");
    if (tzFile.is_open()) {
        std::string timezone;
        std::getline(tzFile, timezone);
        return String(timezone);
    }
#endif
    return "Unknown";
}

auto EnvSystem::isElevated() -> bool {
#ifdef _WIN32
    BOOL isElevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return isElevated != FALSE;
#else
    return geteuid() == 0;
#endif
}

auto EnvSystem::getCurrentProcessId() -> int {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

auto EnvSystem::getParentProcessId() -> int {
#ifdef _WIN32
    // This requires more complex Windows API calls
    return -1;  // Not implemented for Windows
#else
    return static_cast<int>(getppid());
#endif
}

void EnvSystem::setCachingEnabled(bool enabled, int ttl_seconds) {
    sCachingEnabled.store(enabled);
    sCacheTtlSeconds.store(ttl_seconds);
    spdlog::debug("System info caching {}, TTL: {} seconds",
                  enabled ? "enabled" : "disabled", ttl_seconds);

    if (!enabled) {
        clearCache();
    }
}

void EnvSystem::clearCache() {
    std::lock_guard<std::mutex> lock(sCacheMutex);
    sCache = SystemInfoCache();
    sCacheHits.store(0);
    sCacheMisses.store(0);
    spdlog::debug("System info cache cleared");
}

auto EnvSystem::getCacheStats() -> HashMap<String, size_t> {
    HashMap<String, size_t> stats;
    stats["hits"] = sCacheHits.load();
    stats["misses"] = sCacheMisses.load();
    stats["enabled"] = sCachingEnabled.load() ? 1 : 0;
    stats["ttl_seconds"] = static_cast<size_t>(sCacheTtlSeconds.load());
    stats["valid"] = (sCachingEnabled.load() && isCacheValid()) ? 1 : 0;
    return stats;
}

// Helper method implementations
auto EnvSystem::isCacheValid() -> bool {
    if (!sCache.isValid) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - sCache.timestamp);
    return elapsed.count() < sCacheTtlSeconds.load();
}

auto EnvSystem::getNetworkInterfaces() -> Vector<HashMap<String, String>> {
    Vector<HashMap<String, String>> interfaces;

#ifdef _WIN32
    // Windows implementation would require additional headers
    spdlog::warn("Network interface enumeration not fully implemented on Windows");
#else
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("Failed to get network interfaces");
        return interfaces;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        HashMap<String, String> interface;
        interface["name"] = String(ifa->ifa_name);

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr_str[INET_ADDRSTRLEN];
            struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &(addr_in->sin_addr), addr_str, INET_ADDRSTRLEN);
            interface["ipv4"] = String(addr_str);
            interface["family"] = "IPv4";
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char addr_str[INET6_ADDRSTRLEN];
            struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), addr_str, INET6_ADDRSTRLEN);
            interface["ipv6"] = String(addr_str);
            interface["family"] = "IPv6";
        }

        interfaces.push_back(interface);
    }

    freeifaddrs(ifaddr);
#endif

    return interfaces;
}

auto EnvSystem::getSystemPathVariables() -> HashMap<String, String> {
    HashMap<String, String> pathVars;

    const Vector<String> systemPaths = {
        "PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "PYTHONPATH",
        "CLASSPATH", "JAVA_HOME", "HOME", "TEMP", "TMP", "TMPDIR"
    };

    for (const auto& varName : systemPaths) {
        String value = EnvCore::getEnv(varName, "");
        if (!value.empty()) {
            pathVars[varName] = value;
        }
    }

    return pathVars;
}

auto EnvSystem::getLocaleInfo() -> HashMap<String, String> {
    HashMap<String, String> localeInfo;

    const Vector<String> localeVars = {
        "LANG", "LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME",
        "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES", "LC_PAPER",
        "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE", "LC_MEASUREMENT",
        "LC_IDENTIFICATION"
    };

    for (const auto& varName : localeVars) {
        String value = EnvCore::getEnv(varName, "");
        if (!value.empty()) {
            localeInfo[varName] = value;
        }
    }

    return localeInfo;
}

auto EnvSystem::validateSystemRequirements(const HashMap<String, String>& requirements)
    -> HashMap<String, bool> {
    HashMap<String, bool> results;

    for (const auto& [requirement, expectedValue] : requirements) {
        bool isValid = false;

        if (requirement == "os") {
            isValid = (getSystemName() == expectedValue);
        } else if (requirement == "architecture") {
            isValid = (getSystemArch() == expectedValue);
        } else if (requirement == "min_memory_gb") {
            auto memInfo = getMemoryInfo();
            if (memInfo.find("total") != memInfo.end()) {
                size_t totalGB = memInfo["total"] / (1024 * 1024 * 1024);
                size_t requiredGB = std::stoull(std::string(expectedValue.c_str()));
                isValid = (totalGB >= requiredGB);
            }
        } else if (requirement == "min_disk_space_gb") {
            auto diskInfo = getDiskSpaceInfo(".");
            if (diskInfo.find("free") != diskInfo.end()) {
                size_t freeGB = diskInfo["free"] / (1024 * 1024 * 1024);
                size_t requiredGB = std::stoull(std::string(expectedValue.c_str()));
                isValid = (freeGB >= requiredGB);
            }
        } else if (requirement == "elevated") {
            bool requireElevated = (expectedValue == "true");
            isValid = (isElevated() == requireElevated);
        } else {
            // Check environment variable
            String actualValue = EnvCore::getEnv(requirement, "");
            isValid = (actualValue == expectedValue);
        }

        results[requirement] = isValid;
    }

    return results;
}

// Stub implementations for missing helper methods
auto EnvSystem::getCachedValue(const String& key) -> std::optional<String> {
    (void)key;  // Suppress unused parameter warning
    // Implementation would depend on specific caching strategy
    return std::nullopt;
}

void EnvSystem::setCachedValue(const String& key, const String& value) {
    (void)key;    // Suppress unused parameter warning
    (void)value;  // Suppress unused parameter warning
    // Implementation would depend on specific caching strategy
}

void EnvSystem::updateCache() {
    std::lock_guard<std::mutex> lock(sCacheMutex);
    sCache.systemName = getSystemName();
    sCache.architecture = getSystemArch();
    sCache.hostname = getHostName();
    sCache.username = getCurrentUser();
    sCache.homeDir = getHomeDir();
    sCache.tempDir = getTempDir();
    sCache.configDir = getConfigDir();
    sCache.dataDir = getDataDir();
    sCache.timestamp = std::chrono::steady_clock::now();
    sCache.isValid = true;
}

}  // namespace atom::utils
