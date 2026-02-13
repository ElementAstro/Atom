/*
 * disk_util.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Utilities

**************************************************/

#include "disk_util.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>

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
#if defined(__linux__) || defined(__ANDROID__)
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

std::string formatSize(uint64_t sizeBytes, bool useBinaryUnits) {
    if (sizeBytes == 0) {
        return "0 B";
    }

    const uint64_t base = useBinaryUnits ? 1024 : 1000;
    const std::array<const char*, 9> binaryUnits = {
        "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
    };
    const std::array<const char*, 9> decimalUnits = {
        "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
    };

    const auto& units = useBinaryUnits ? binaryUnits : decimalUnits;

    double size = static_cast<double>(sizeBytes);
    size_t unitIndex = 0;

    while (size >= base && unitIndex < units.size() - 1) {
        size /= base;
        ++unitIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}

namespace {
// Cache for filesystem types
std::mutex g_fsTypeCacheMutex;
std::unordered_map<std::string, std::pair<std::string, std::chrono::steady_clock::time_point>> g_fsTypeCache;
constexpr auto FS_CACHE_EXPIRATION = std::chrono::minutes(10);

void clearExpiredFsCache() {
    std::lock_guard<std::mutex> lock(g_fsTypeCacheMutex);
    const auto now = std::chrono::steady_clock::now();

    std::erase_if(g_fsTypeCache, [now](const auto& item) {
        return (now - item.second.second) > FS_CACHE_EXPIRATION;
    });
}
}  // anonymous namespace

std::future<std::string> getFileSystemTypeAsync(const std::string& path) {
    return std::async(std::launch::async, [path]() {
        return getFileSystemType(path);
    });
}

DiskPerformanceMetrics benchmarkDiskPerformance(const std::string& path, uint32_t testSizeMB) {
    DiskPerformanceMetrics metrics;

    try {
        const std::string testFile = path + "/disk_benchmark_test.tmp";
        const size_t testSize = static_cast<size_t>(testSizeMB) * 1024 * 1024;

        // Generate random test data
        std::vector<char> testData(testSize);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::generate(testData.begin(), testData.end(), [&]() {
            return static_cast<char>(dis(gen));
        });

        // Write test
        auto writeStart = std::chrono::high_resolution_clock::now();
        {
            std::ofstream file(testFile, std::ios::binary);
            if (file.is_open()) {
                file.write(testData.data(), testSize);
                file.flush();
                metrics.writeOperations = 1;
                metrics.writeBytes = testSize;
            }
        }
        auto writeEnd = std::chrono::high_resolution_clock::now();

        // Read test
        auto readStart = std::chrono::high_resolution_clock::now();
        {
            std::ifstream file(testFile, std::ios::binary);
            if (file.is_open()) {
                std::vector<char> readData(testSize);
                file.read(readData.data(), testSize);
                metrics.readOperations = 1;
                metrics.readBytes = testSize;
            }
        }
        auto readEnd = std::chrono::high_resolution_clock::now();

        // Calculate latencies
        auto writeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(writeEnd - writeStart);
        auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - readStart);

        metrics.writeLatencyMs = static_cast<uint32_t>(writeDuration.count());
        metrics.readLatencyMs = static_cast<uint32_t>(readDuration.count());

        // Clean up test file
        std::remove(testFile.c_str());

    } catch (const std::exception& e) {
        spdlog::error("Disk benchmark failed for path {}: {}", path, e.what());
    }

    return metrics;
}

std::vector<double> calculateDiskUsagePercentageBatch(const std::vector<DiskInfo>& diskInfos) {
    std::vector<double> results;
    results.reserve(diskInfos.size());

    // Simple vectorized calculation - could be enhanced with actual SIMD
    std::transform(diskInfos.begin(), diskInfos.end(), std::back_inserter(results),
        [](const DiskInfo& info) -> double {
            return calculateDiskUsagePercentage(info.totalSpace, info.freeSpace);
        });

    return results;
}

std::string getFileSystemTypeCached(const std::string& path) {
    clearExpiredFsCache();

    {
        std::lock_guard<std::mutex> lock(g_fsTypeCacheMutex);
        const auto it = g_fsTypeCache.find(path);
        if (it != g_fsTypeCache.end() &&
            (std::chrono::steady_clock::now() - it->second.second) <= FS_CACHE_EXPIRATION) {
            spdlog::debug("Using cached filesystem type for path: {}", path);
            return it->second.first;
        }
    }

    spdlog::debug("Computing new filesystem type for path: {}", path);
    const std::string fsType = getFileSystemType(path);

    {
        std::lock_guard<std::mutex> lock(g_fsTypeCacheMutex);
        g_fsTypeCache[path] = {fsType, std::chrono::steady_clock::now()};
    }

    return fsType;
}

}  // namespace atom::system
