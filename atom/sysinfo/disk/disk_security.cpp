/*
 * disk_security.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Security

**************************************************/

#include "atom/sysinfo/disk/disk_security.hpp"

#include <filesystem>
#include <mutex>
#include <regex>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/mount.h>
#include <unistd.h>
#elif __APPLE__
#include <sys/mount.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/mount.h>
#endif

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace atom::system {

namespace {
std::mutex g_whitelistMutex;
std::unordered_set<std::string> g_whitelistedDevices = {"SD1234", "SD5678"};

const std::unordered_set<std::string> SUSPICIOUS_EXTENSIONS = {
    ".exe", ".bat", ".cmd", ".ps1", ".vbs", ".js",  ".jar", ".sh", ".py",
    ".scr", ".pif", ".com", ".msi", ".dll", ".hta", ".wsf", ".lnk"};

const std::vector<std::pair<std::string, std::regex>> SUSPICIOUS_PATTERNS = {
    {"autorun.inf", std::regex("(?i)^autorun\\.inf$")},
    {"autorun", std::regex("(?i)^autorun$")},
    {"suspicious naming",
     std::regex("(?i)(virus|hack|crack|keygen|patch|warez|trojan|malware)")},
    {"hidden system", std::regex("(?i)^\\.")},
    {"temp files", std::regex("(?i)\\.(tmp|temp)$")}};

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}
}  // namespace

bool addDeviceToWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    if (g_whitelistedDevices.find(deviceIdentifier) !=
        g_whitelistedDevices.end()) {
        spdlog::info("Device {} is already in the whitelist", deviceIdentifier);
        return true;
    }

    g_whitelistedDevices.insert(deviceIdentifier);
    spdlog::info("Added device {} to whitelist", deviceIdentifier);

    return true;
}

bool removeDeviceFromWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    const auto it = g_whitelistedDevices.find(deviceIdentifier);
    if (it == g_whitelistedDevices.end()) {
        spdlog::warn("Device {} is not in the whitelist", deviceIdentifier);
        return false;
    }

    g_whitelistedDevices.erase(it);
    spdlog::info("Removed device {} from whitelist", deviceIdentifier);

    return true;
}

bool isDeviceInWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    const bool result = g_whitelistedDevices.find(deviceIdentifier) !=
                        g_whitelistedDevices.end();

    if (result) {
        spdlog::info("Device {} is in the whitelist. Access granted.",
                     deviceIdentifier);
    } else {
        spdlog::error("Device {} is not in the whitelist. Access denied.",
                      deviceIdentifier);
    }

    return result;
}

bool setDiskReadOnly(const std::string& path) {
#ifdef _WIN32
    const HANDLE hDevice =
        CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_NO_BUFFERING, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to open device {}: {}", path, GetLastError());
        return false;
    }

    DWORD bytesReturned = 0;
    const BOOL result =
        DeviceIoControl(hDevice, FSCTL_SET_PERSISTENT_VOLUME_STATE, nullptr, 0,
                        nullptr, 0, &bytesReturned, nullptr);

    CloseHandle(hDevice);

    if (!result) {
        spdlog::error("Failed to set disk read-only: {}", GetLastError());
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif __linux__
    const int result = mount(path.c_str(), path.c_str(), nullptr,
                             MS_REMOUNT | MS_RDONLY, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif __APPLE__
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        spdlog::error("Failed to get mount info for {}: {}", path,
                      strerror(errno));
        return false;
    }

    const int result = mount(statInfo.f_fstypename, path.c_str(),
                             MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        spdlog::error("Failed to get mount info for {}: {}", path,
                      strerror(errno));
        return false;
    }

    const int result = mount(statInfo.f_fstypename, path.c_str(),
                             MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;
#else
    spdlog::error(
        "Setting disk to read-only is not implemented for this platform");
    return false;
#endif
}

std::pair<bool, int> scanDiskForThreats(const std::string& path,
                                        int scanDepth) {
    spdlog::info("Scanning {} for malicious files (depth: {})", path,
                 scanDepth);

    int suspiciousCount = 0;
    bool success = true;

    try {
        std::function<void(const fs::path&, int)> scanDirectory;

        scanDirectory = [&](const fs::path& dirPath, int currentDepth) {
            if (scanDepth > 0 && currentDepth > scanDepth) {
                return;
            }

            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
                if (ec) {
                    spdlog::warn("Error accessing directory {}: {}",
                                 dirPath.string(), ec.message());
                    continue;
                }

                try {
                    if (fs::is_directory(entry, ec) && !ec) {
                        scanDirectory(entry.path(), currentDepth + 1);
                    } else if (fs::is_regular_file(entry, ec) && !ec) {
                        const std::string extension =
                            toLower(entry.path().extension().string());
                        const std::string filename =
                            entry.path().filename().string();

                        if (SUSPICIOUS_EXTENSIONS.find(extension) !=
                            SUSPICIOUS_EXTENSIONS.end()) {
                            spdlog::warn("Suspicious file extension found: {}",
                                         entry.path().string());
                            suspiciousCount++;
                        }

                        for (const auto& [patternName, pattern] :
                             SUSPICIOUS_PATTERNS) {
                            if (std::regex_search(filename, pattern)) {
                                spdlog::warn(
                                    "Suspicious file pattern ({}) found: {}",
                                    patternName, entry.path().string());
                                suspiciousCount++;
                                break;
                            }
                        }

                        if (fs::file_size(entry, ec) == 0 && !ec) {
                            spdlog::warn(
                                "Empty file found (potential placeholder): {}",
                                entry.path().string());
                        }

                        const auto fileTime = fs::last_write_time(entry, ec);
                        if (!ec) {
                            const auto now = std::chrono::file_clock::now();
                            const auto duration = now - fileTime;
                            if (duration < std::chrono::minutes(5)) {
                                spdlog::info(
                                    "Recently created file detected: {}",
                                    entry.path().string());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error scanning {}: {}",
                                  entry.path().string(), e.what());
                }
            }
        };

        scanDirectory(path, 0);
    } catch (const std::exception& e) {
        spdlog::error("Error scanning {}: {}", path, e.what());
        success = false;
    }

    spdlog::info("Scan completed for {}. Found {} suspicious files.", path,
                 suspiciousCount);

    return {success, suspiciousCount};
}

}  // namespace atom::system
