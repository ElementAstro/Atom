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

#include "atom/log/loguru.hpp"

namespace fs = std::filesystem;

namespace atom::system {

// Global mutex for whitelisted devices
static std::mutex g_whitelistMutex;
static std::unordered_set<std::string> g_whitelistedDevices = {"SD1234",
                                                               "SD5678"};

bool addDeviceToWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    // Check if already in whitelist
    if (g_whitelistedDevices.find(deviceIdentifier) !=
        g_whitelistedDevices.end()) {
        LOG_F(INFO, "Device %s is already in the whitelist",
              deviceIdentifier.c_str());
        return true;
    }

    // Add to whitelist
    g_whitelistedDevices.insert(deviceIdentifier);
    LOG_F(INFO, "Added device %s to whitelist", deviceIdentifier.c_str());

    // Here we could also persist the whitelist to disk for permanent storage

    return true;
}

bool removeDeviceFromWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    auto it = g_whitelistedDevices.find(deviceIdentifier);
    if (it == g_whitelistedDevices.end()) {
        LOG_F(WARNING, "Device %s is not in the whitelist",
              deviceIdentifier.c_str());
        return false;
    }

    g_whitelistedDevices.erase(it);
    LOG_F(INFO, "Removed device %s from whitelist", deviceIdentifier.c_str());

    // Here we could also update the persisted whitelist

    return true;
}

bool isDeviceInWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    bool result = g_whitelistedDevices.find(deviceIdentifier) !=
                  g_whitelistedDevices.end();

    if (result) {
        LOG_F(INFO, "Device %s is in the whitelist. Access granted.",
              deviceIdentifier.c_str());
    } else {
        LOG_F(ERROR, "Device %s is not in the whitelist. Access denied.",
              deviceIdentifier.c_str());
    }

    return result;
}

bool setDiskReadOnly(const std::string& path) {
#ifdef _WIN32
    // On Windows, use DeviceIOControl to set disk read-only
    HANDLE hDevice =
        CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_NO_BUFFERING, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to open device %s: %lu", path.c_str(),
              GetLastError());
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL result =
        DeviceIoControl(hDevice, FSCTL_SET_PERSISTENT_VOLUME_STATE, nullptr, 0,
                        nullptr, 0, &bytesReturned, nullptr);

    CloseHandle(hDevice);

    if (!result) {
        LOG_F(ERROR, "Failed to set disk read-only: %lu", GetLastError());
        return false;
    }

    LOG_F(INFO, "Successfully set disk %s to read-only mode", path.c_str());
    return true;

#elif __linux__
    // On Linux, remount with read-only flag
    int result = mount(path.c_str(), path.c_str(), nullptr,
                       MS_REMOUNT | MS_RDONLY, nullptr);

    if (result != 0) {
        LOG_F(ERROR, "Failed to set disk %s to read-only: %s", path.c_str(),
              strerror(errno));
        return false;
    }

    LOG_F(INFO, "Successfully set disk %s to read-only mode", path.c_str());
    return true;

#elif __APPLE__
    // On macOS, use mount with MNT_RDONLY flag
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        LOG_F(ERROR, "Failed to get mount info for %s: %s", path.c_str(),
              strerror(errno));
        return false;
    }

    int result = mount(statInfo.f_fstypename, path.c_str(),
                       MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        LOG_F(ERROR, "Failed to set disk %s to read-only: %s", path.c_str(),
              strerror(errno));
        return false;
    }

    LOG_F(INFO, "Successfully set disk %s to read-only mode", path.c_str());
    return true;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    // On BSD systems, use mount with MNT_RDONLY flag
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        LOG_F(ERROR, "Failed to get mount info for %s: %s", path.c_str(),
              strerror(errno));
        return false;
    }

    int result = mount(statInfo.f_fstypename, path.c_str(),
                       MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        LOG_F(ERROR, "Failed to set disk %s to read-only: %s", path.c_str(),
              strerror(errno));
        return false;
    }

    LOG_F(INFO, "Successfully set disk %s to read-only mode", path.c_str());
    return true;
#else
    LOG_F(ERROR,
          "Setting disk to read-only is not implemented for this platform");
    return false;
#endif
}

std::pair<bool, int> scanDiskForThreats(const std::string& path,
                                        int scanDepth) {
    LOG_F(INFO, "Scanning %s for malicious files (depth: %d)", path.c_str(),
          scanDepth);

    int suspiciousCount = 0;
    bool success = true;

    // Define suspicious file extensions and patterns
    const std::unordered_set<std::string> suspiciousExtensions = {
        ".exe", ".bat", ".cmd", ".ps1", ".vbs", ".js", ".jar", ".sh", ".py"};

    // Define suspicious file patterns (simplified)
    const std::vector<std::pair<std::string, std::regex>> suspiciousPatterns = {
        {"autorun.inf", std::regex("(?i)^autorun\\.inf$")},
        {"autorun", std::regex("(?i)^autorun$")},
        {"suspicious naming",
         std::regex("(?i)(virus|hack|crack|keygen|patch|warez)")}};

    // Implement a basic traversal
    try {
        std::function<void(const fs::path&, int)> scanDirectory;

        scanDirectory = [&](const fs::path& dirPath, int currentDepth) {
            // Check if we've reached the maximum depth
            if (scanDepth > 0 && currentDepth > scanDepth) {
                return;
            }

            for (const auto& entry : fs::directory_iterator(dirPath)) {
                try {
                    // Check if entry is a directory
                    if (fs::is_directory(entry)) {
                        scanDirectory(entry.path(), currentDepth + 1);
                    } else if (fs::is_regular_file(entry)) {
                        // Get the extension
                        std::string extension =
                            entry.path().extension().string();
                        std::string filename = entry.path().filename().string();

                        // Convert extension to lowercase for case-insensitive
                        // comparison
                        std::transform(extension.begin(), extension.end(),
                                       extension.begin(), [](unsigned char c) {
                                           return std::tolower(c);
                                       });

                        // Check if extension is suspicious
                        if (suspiciousExtensions.find(extension) !=
                            suspiciousExtensions.end()) {
                            LOG_F(WARNING,
                                  "Suspicious file extension found: %s",
                                  entry.path().string().c_str());
                            suspiciousCount++;
                        }

                        // Check filename against patterns
                        for (const auto& pattern : suspiciousPatterns) {
                            if (std::regex_search(filename, pattern.second)) {
                                LOG_F(WARNING,
                                      "Suspicious file pattern (%s) found: %s",
                                      pattern.first.c_str(),
                                      entry.path().string().c_str());
                                suspiciousCount++;
                                break;
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Error scanning %s: %s",
                          entry.path().string().c_str(), e.what());
                }
            }
        };

        scanDirectory(path, 0);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error scanning %s: %s", path.c_str(), e.what());
        success = false;
    }

    LOG_F(INFO, "Scan completed for %s. Found %d suspicious files.",
          path.c_str(), suspiciousCount);

    return {success, suspiciousCount};
}

}  // namespace atom::system
