/*
 * disk_util.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Utilities

**************************************************/

#include "atom/sysinfo/disk/disk_util.hpp"

#ifdef _WIN32
#include <windows.h>
#elif __linux__ || __ANDROID__
#include <sys/statfs.h>
#include <fstream>
#elif __APPLE__
#include <sys/mount.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {

double calculateDiskUsagePercentage(uint64_t totalSpace, uint64_t freeSpace) {
    // Guard against division by zero
    if (totalSpace == 0) {
        return 0.0;
    }

    uint64_t usedSpace = totalSpace - freeSpace;
    return (static_cast<double>(usedSpace) / static_cast<double>(totalSpace)) *
           100.0;
}

std::string getFileSystemType(const std::string& path) {
#ifdef _WIN32
    char fileSystemNameBuffer[MAX_PATH] = {0};

    // Make sure the path ends with a backslash
    std::string rootPath = path;
    if (rootPath.back() != '\\') {
        rootPath += '\\';
    }

    if (!GetVolumeInformationA(rootPath.c_str(), NULL, 0, NULL, NULL, NULL,
                               fileSystemNameBuffer,
                               sizeof(fileSystemNameBuffer))) {
        LOG_F(ERROR, "Failed to get file system type for %s: %d", path.c_str(),
              GetLastError());
        return "Unknown";
    }
    return std::string(fileSystemNameBuffer);

#elif __linux__ || __ANDROID__
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Failed to get file system type for %s", path.c_str());
        return "Unknown";
    }

    // Map filesystem type to string
    switch (buffer.f_type) {
        case 0xEF53:
            return "ext4";
        case 0x6969:
            return "nfs";
        case 0xFF534D42:
            return "cifs";
        case 0x4d44:
            return "vfat";
        case 0x5346544E:
            return "ntfs";
        case 0x52654973:
            return "reiserfs";
        case 0x01021994:
            return "tmpfs";
        case 0x58465342:
            return "xfs";
        case 0xF15F:
            return "ecryptfs";
        case 0x65735546:
            return "fuse";
        case 0x9123683E:
            return "btrfs";
        default:
            // Try to get from /proc/mounts for unrecognized types
            std::ifstream mounts("/proc/mounts");
            std::string line;
            while (std::getline(mounts, line)) {
                if (line.find(path) != std::string::npos) {
                    std::string devicePath, mountPoint, fsType;
                    std::istringstream iss(line);
                    iss >> devicePath >> mountPoint >> fsType;
                    if (!fsType.empty()) {
                        return fsType;
                    }
                }
            }
            return "Unknown";
    }

#elif __APPLE__
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Failed to get file system type for %s", path.c_str());
        return "Unknown";
    }

    // macOS returns filesystem type directly
    return buffer.f_fstypename;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Failed to get file system type for %s", path.c_str());
        return "Unknown";
    }

    return buffer.f_fstypename;

#else
    // Generic fallback for other Unix systems
    struct statvfs buffer;
    if (statvfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Failed to get file system type for %s", path.c_str());
        return "Unknown";
    }

    // Unfortunately statvfs doesn't provide filesystem type directly
    // We would need to check /etc/mtab or similar
    return "Unknown";
#endif
}

}  // namespace atom::system
