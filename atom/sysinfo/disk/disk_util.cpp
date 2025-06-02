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

#include <sstream>

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

#include <spdlog/spdlog.h>

namespace atom::system {

namespace {
#ifdef __linux__ || __ANDROID__
struct FilesystemTypeInfo {
    uint32_t magic;
    const char* name;
};

constexpr FilesystemTypeInfo FILESYSTEM_TYPES[] = {
    {0xEF53, "ext4"},          {0x6969, "nfs"},        {0xFF534D42, "cifs"},
    {0x4d44, "vfat"},          {0x5346544E, "ntfs"},   {0x52654973, "reiserfs"},
    {0x01021994, "tmpfs"},     {0x58465342, "xfs"},    {0xF15F, "ecryptfs"},
    {0x65735546, "fuse"},      {0x9123683E, "btrfs"},  {0x73717368, "squashfs"},
    {0x794c7630, "overlayfs"}, {0x72b6, "jffs2"},      {0x24051905, "ubifs"},
    {0x47504653, "gpfs"},      {0x64626720, "debugfs"}};

const char* getFilesystemName(uint32_t fsType) {
    for (const auto& info : FILESYSTEM_TYPES) {
        if (info.magic == fsType) {
            return info.name;
        }
    }
    return nullptr;
}

std::string getFilesystemFromProcMounts(const std::string& path) {
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        return "Unknown";
    }

    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(path) != std::string::npos) {
            std::istringstream iss(line);
            std::string devicePath, mountPoint, fsType;
            iss >> devicePath >> mountPoint >> fsType;

            if (mountPoint == path && !fsType.empty()) {
                return fsType;
            }
        }
    }
    return "Unknown";
}
#endif
}  // namespace

double calculateDiskUsagePercentage(uint64_t totalSpace, uint64_t freeSpace) {
    if (totalSpace == 0) {
        spdlog::warn("Total space is zero, returning 0% usage");
        return 0.0;
    }

    if (freeSpace > totalSpace) {
        spdlog::warn(
            "Free space ({} bytes) exceeds total space ({} bytes), returning "
            "0% usage",
            freeSpace, totalSpace);
        return 0.0;
    }

    const uint64_t usedSpace = totalSpace - freeSpace;
    return (static_cast<double>(usedSpace) / static_cast<double>(totalSpace)) *
           100.0;
}

std::string getFileSystemType(const std::string& path) {
#ifdef _WIN32
    char fileSystemNameBuffer[MAX_PATH] = {0};

    std::string rootPath = path;
    if (rootPath.back() != '\\') {
        rootPath += '\\';
    }

    if (!GetVolumeInformationA(rootPath.c_str(), nullptr, 0, nullptr, nullptr,
                               nullptr, fileSystemNameBuffer,
                               sizeof(fileSystemNameBuffer))) {
        spdlog::error("Failed to get file system type for {}: {}", path,
                      GetLastError());
        return "Unknown";
    }

    const std::string result(fileSystemNameBuffer);
    spdlog::debug("File system type for {}: {}", path, result);
    return result;

#elif __linux__ || __ANDROID__
    struct statfs buffer{};
    if (statfs(path.c_str(), &buffer) != 0) {
        spdlog::error("Failed to get file system type for {}: {}", path,
                      strerror(errno));
        return "Unknown";
    }

    if (const char* fsName = getFilesystemName(buffer.f_type)) {
        spdlog::debug("File system type for {}: {}", path, fsName);
        return fsName;
    }

    const std::string result = getFilesystemFromProcMounts(path);
    if (result != "Unknown") {
        spdlog::debug("File system type for {} from /proc/mounts: {}", path,
                      result);
        return result;
    }

    spdlog::warn("Unknown file system type for {} (magic: 0x{:x})", path,
                 buffer.f_type);
    return "Unknown";

#elif __APPLE__
    struct statfs buffer{};
    if (statfs(path.c_str(), &buffer) != 0) {
        spdlog::error("Failed to get file system type for {}: {}", path,
                      strerror(errno));
        return "Unknown";
    }

    const std::string result(buffer.f_fstypename);
    spdlog::debug("File system type for {}: {}", path, result);
    return result;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs buffer{};
    if (statfs(path.c_str(), &buffer) != 0) {
        spdlog::error("Failed to get file system type for {}: {}", path,
                      strerror(errno));
        return "Unknown";
    }

    const std::string result(buffer.f_fstypename);
    spdlog::debug("File system type for {}: {}", path, result);
    return result;

#else
    struct statvfs buffer{};
    if (statvfs(path.c_str(), &buffer) != 0) {
        spdlog::error("Failed to get file system type for {}: {}", path,
                      strerror(errno));
        return "Unknown";
    }

    spdlog::warn(
        "File system type detection not fully implemented for this platform");
    return "Unknown";
#endif
}

}  // namespace atom::system
