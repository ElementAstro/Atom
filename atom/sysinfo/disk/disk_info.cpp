/*
 * disk_info.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Information

**************************************************/

#include "atom/sysinfo/disk/disk_info.hpp"
#include "atom/sysinfo/disk/disk_device.hpp"
#include "atom/sysinfo/disk/disk_util.hpp"

#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/stat.h>
#include <sys/statfs.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <sys/mount.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/disk.h>
#include <sys/mount.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

namespace {
std::mutex g_cacheMutex;
std::unordered_map<std::string,
                   std::pair<DiskInfo, std::chrono::steady_clock::time_point>>
    g_diskInfoCache;
constexpr auto CACHE_EXPIRATION = std::chrono::minutes(5);

void clearExpiredCache() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    const auto now = std::chrono::steady_clock::now();

    std::erase_if(g_diskInfoCache, [now](const auto& item) {
        return (now - item.second.second) > CACHE_EXPIRATION;
    });
}

std::string trimString(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

}  // anonymous namespace

DiskInfo getDiskInfoCached(const std::string& path) {
    clearExpiredCache();

    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        const auto it = g_diskInfoCache.find(path);
        if (it != g_diskInfoCache.end() &&
            (std::chrono::steady_clock::now() - it->second.second) <=
                CACHE_EXPIRATION) {
            spdlog::debug("Using cached disk info for path: {}", path);
            return it->second.first;
        }
    }

    spdlog::debug("Computing new disk info for path: {}", path);
    DiskInfo info;
    info.path = path;

    info.fsType = getFileSystemType(path);

#ifdef _WIN32
    ULARGE_INTEGER totalSpace, freeSpace;
    if (GetDiskFreeSpaceExA(path.c_str(), nullptr, &totalSpace, &freeSpace)) {
        info.totalSpace = totalSpace.QuadPart;
        info.freeSpace = freeSpace.QuadPart;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
    } else {
        spdlog::warn("Failed to get disk space for path: {}", path);
    }

    const UINT driveType = GetDriveTypeA(path.c_str());
    info.isRemovable = (driveType == DRIVE_REMOVABLE);

    char volumeName[MAX_PATH] = {0};
    if (GetVolumeNameForVolumeMountPointA(path.c_str(), volumeName, MAX_PATH)) {
        info.devicePath = volumeName;
        info.model = getDriveModel(path);
    }

#elif __linux__
    struct statfs stats{};
    if (statfs(path.c_str(), &stats) == 0) {
        info.totalSpace = static_cast<uint64_t>(stats.f_blocks) * stats.f_bsize;
        info.freeSpace = static_cast<uint64_t>(stats.f_bfree) * stats.f_bsize;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
    } else {
        spdlog::warn("Failed to get filesystem stats for path: {}", path);
    }

    std::ifstream mountInfo("/proc/mounts");
    if (mountInfo.is_open()) {
        std::string line;
        while (std::getline(mountInfo, line)) {
            if (line.find(path) != std::string::npos) {
                std::istringstream iss(line);
                iss >> info.devicePath;
                break;
            }
        }
    }

    if (!info.devicePath.empty()) {
        std::string deviceName = info.devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        const std::string removablePath =
            "/sys/block/" + deviceName + "/removable";
        std::ifstream removableFile(removablePath);
        std::string value;
        if (removableFile.is_open() && std::getline(removableFile, value)) {
            info.isRemovable = (value == "1");
        }

        info.model = getDriveModel(info.devicePath);
    }

#elif __APPLE__
    struct statfs stats{};
    if (statfs(path.c_str(), &stats) == 0) {
        info.totalSpace = static_cast<uint64_t>(stats.f_blocks) * stats.f_bsize;
        info.freeSpace = static_cast<uint64_t>(stats.f_bfree) * stats.f_bsize;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
        info.devicePath = stats.f_mntfromname;
    } else {
        spdlog::warn("Failed to get filesystem stats for path: {}", path);
    }

    if (!info.devicePath.empty()) {
        std::string diskName = info.devicePath;
        const size_t lastSlash = diskName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            diskName = diskName.substr(lastSlash + 1);
        }

        info.model = getDriveModel(info.devicePath);

        const DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (session) {
            const DADiskRef disk = DADiskCreateFromBSDName(
                kCFAllocatorDefault, session, diskName.c_str());
            if (disk) {
                const CFDictionaryRef diskDesc = DADiskCopyDescription(disk);
                if (diskDesc) {
                    const CFBooleanRef ejectable =
                        static_cast<CFBooleanRef>(CFDictionaryGetValue(
                            diskDesc, kDADiskDescriptionMediaEjectableKey));
                    const CFBooleanRef removable =
                        static_cast<CFBooleanRef>(CFDictionaryGetValue(
                            diskDesc, kDADiskDescriptionMediaRemovableKey));

                    info.isRemovable =
                        (ejectable && CFBooleanGetValue(ejectable)) ||
                        (removable && CFBooleanGetValue(removable));

                    CFRelease(diskDesc);
                }
                CFRelease(disk);
            }
            CFRelease(session);
        }
    }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs stats{};
    if (statfs(path.c_str(), &stats) == 0) {
        info.totalSpace = static_cast<uint64_t>(stats.f_blocks) * stats.f_bsize;
        info.freeSpace = static_cast<uint64_t>(stats.f_bfree) * stats.f_bsize;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
        info.devicePath = stats.f_mntfromname;
    } else {
        spdlog::warn("Failed to get filesystem stats for path: {}", path);
    }

    info.model = info.devicePath;
    info.isRemovable =
        (info.devicePath.find("da") == 0 || info.devicePath.find("cd") == 0 ||
         info.devicePath.find("md") == 0);
#endif

    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_diskInfoCache[path] = {info, std::chrono::steady_clock::now()};
    }

    spdlog::debug("Disk info computed for path: {}, model: {}, usage: {:.2f}%",
                  path, info.model, info.usagePercent);
    return info;
}

std::vector<DiskInfo> getDiskInfo(bool includeRemovable) {
    spdlog::debug("Getting disk info, includeRemovable: {}", includeRemovable);

    std::vector<DiskInfo> result;
    const auto drives = getAvailableDrives(true);
    result.reserve(drives.size());

    for (const auto& drive : drives) {
        DiskInfo info = getDiskInfoCached(drive);

        if (!includeRemovable && info.isRemovable) {
            spdlog::debug("Skipping removable drive: {}", drive);
            continue;
        }

        result.push_back(std::move(info));
    }

    spdlog::debug("Found {} disk(s)", result.size());
    return result;
}

std::vector<std::pair<std::string, float>> getDiskUsage() {
    spdlog::debug("Getting disk usage information");

    std::vector<std::pair<std::string, float>> diskUsage;
    const auto diskInfo = getDiskInfo(true);

    diskUsage.reserve(diskInfo.size());
    for (const auto& info : diskInfo) {
        diskUsage.emplace_back(info.path, info.usagePercent);
    }

    return diskUsage;
}

std::string getDriveModel(const std::string& drivePath) {
    spdlog::debug("Getting drive model for: {}", drivePath);

    std::string model;

#ifdef _WIN32
    std::string physicalDrivePath = drivePath;
    if (drivePath.size() == 2 && drivePath[1] == ':') {
        physicalDrivePath += '\\';
    }

    const HANDLE hDevice = CreateFileA(physicalDrivePath.c_str(), GENERIC_READ,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDevice != INVALID_HANDLE_VALUE) {
        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_DESCRIPTOR_HEADER header{};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                            sizeof(query), &header, sizeof(header),
                            &bytesReturned, nullptr)) {
            std::vector<char> buffer(header.Size);
            if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                                sizeof(query), buffer.data(), buffer.size(),
                                &bytesReturned, nullptr)) {
                const auto* desc =
                    reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

                std::string vendor, product;
                if (desc->VendorIdOffset != 0) {
                    vendor = trimString(buffer.data() + desc->VendorIdOffset);
                }
                if (desc->ProductIdOffset != 0) {
                    product = trimString(buffer.data() + desc->ProductIdOffset);
                }

                if (!vendor.empty() && !product.empty()) {
                    model = vendor + " " + product;
                } else if (!product.empty()) {
                    model = product;
                } else if (!vendor.empty()) {
                    model = vendor;
                }
            }
        }
        CloseHandle(hDevice);
    }

#elif __APPLE__
    const DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (session) {
        std::string diskName = drivePath;
        const size_t lastSlash = diskName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            diskName = diskName.substr(lastSlash + 1);
        }

        const DADiskRef disk = DADiskCreateFromBSDName(
            kCFAllocatorDefault, session, diskName.c_str());
        if (disk) {
            const CFDictionaryRef diskDesc = DADiskCopyDescription(disk);
            if (diskDesc) {
                const CFStringRef modelRef =
                    static_cast<CFStringRef>(CFDictionaryGetValue(
                        diskDesc, kDADiskDescriptionDeviceModelKey));
                if (modelRef) {
                    char modelBuffer[256];
                    if (CFStringGetCString(modelRef, modelBuffer,
                                           sizeof(modelBuffer),
                                           kCFStringEncodingUTF8)) {
                        model = trimString(modelBuffer);
                    }
                }
                CFRelease(diskDesc);
            }
            CFRelease(disk);
        }
        CFRelease(session);
    }

#elif __linux__
    std::string deviceName = drivePath;
    const size_t lastSlash = deviceName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        deviceName = deviceName.substr(lastSlash + 1);
    }

    std::string baseDevice = deviceName;
    for (int i = deviceName.length() - 1; i >= 0; --i) {
        if (!isdigit(deviceName[i])) {
            baseDevice = deviceName.substr(0, i + 1);
            break;
        }
    }

    const std::string modelPath = "/sys/block/" + baseDevice + "/device/model";
    std::ifstream modelFile(modelPath);
    if (modelFile.is_open() && std::getline(modelFile, model)) {
        model = trimString(model);
    }

    if (model.empty()) {
        const std::string vendorPath =
            "/sys/block/" + baseDevice + "/device/vendor";
        std::ifstream vendorFile(vendorPath);
        std::string vendor;
        if (vendorFile.is_open() && std::getline(vendorFile, vendor)) {
            vendor = trimString(vendor);

            modelFile.clear();
            modelFile.open(modelPath);
            if (modelFile.is_open() && std::getline(modelFile, model)) {
                model = trimString(model);
                model = vendor + " " + model;
            } else {
                model = vendor;
            }
        }
    }

    if (model.empty()) {
        model = deviceName;
    }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    std::string deviceName = drivePath;
    const size_t lastSlash = deviceName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        deviceName = deviceName.substr(lastSlash + 1);
    }
    model = deviceName;
#endif

    if (model.empty()) {
        model = "Unknown Device";
        spdlog::warn("Could not determine model for drive: {}", drivePath);
    } else {
        spdlog::debug("Drive model for {}: {}", drivePath, model);
    }

    return model;
}

}  // namespace atom::system
