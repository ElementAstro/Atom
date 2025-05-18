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
#include <mutex>
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

#include "atom/log/loguru.hpp"

namespace atom::system {

// Cache map to store drive info with expiration time
static std::mutex g_cacheMutex;
static std::unordered_map<
    std::string, std::pair<DiskInfo, std::chrono::steady_clock::time_point>>
    g_diskInfoCache;
static const auto CACHE_EXPIRATION = std::chrono::minutes(5);

/**
 * @brief Clears expired cache entries
 */
void clearExpiredCache() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto now = std::chrono::steady_clock::now();

    // Use C++20 erase_if for cleaner removal
    std::erase_if(g_diskInfoCache, [now](const auto& item) {
        return (now - item.second.second) > CACHE_EXPIRATION;
    });
}

/**
 * @brief Gets disk information with caching
 */
DiskInfo getDiskInfoCached(const std::string& path) {
    clearExpiredCache();

    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_diskInfoCache.find(path);
        if (it != g_diskInfoCache.end() &&
            (std::chrono::steady_clock::now() - it->second.second) <=
                CACHE_EXPIRATION) {
            return it->second.first;
        }
    }

    // Cache miss - compute new info
    DiskInfo info;
    info.path = path;

    // Fill file system type
    info.fsType = getFileSystemType(path);

    // Get space information
#ifdef _WIN32
    ULARGE_INTEGER totalSpace, freeSpace;
    if (GetDiskFreeSpaceExA(path.c_str(), nullptr, &totalSpace, &freeSpace)) {
        info.totalSpace = totalSpace.QuadPart;
        info.freeSpace = freeSpace.QuadPart;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
    }

    // Check if removable
    UINT driveType = GetDriveTypeA(path.c_str());
    info.isRemovable = (driveType == DRIVE_REMOVABLE);

    // Get physical device path and model
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
    }

    // Try to find device path
    std::ifstream mountInfo("/proc/mounts");
    std::string line;
    while (std::getline(mountInfo, line)) {
        if (line.find(path) != std::string::npos) {
            std::istringstream iss(line);
            iss >> info.devicePath;
            break;
        }
    }

    // Check if removable and get model
    if (!info.devicePath.empty()) {
        // Extract device name
        std::string deviceName = info.devicePath;
        size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        // Check if removable by looking at /sys/block/<device>/removable
        std::string removablePath = "/sys/block/" + deviceName + "/removable";
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
    }

    // Get model and check if removable using IOKit
    if (!info.devicePath.empty()) {
        // Extract disk identifier (e.g., 'disk1' from '/dev/disk1')
        std::string diskName = info.devicePath;
        size_t lastSlash = diskName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            diskName = diskName.substr(lastSlash + 1);
        }

        info.model = getDriveModel(info.devicePath);

        // Check if removable
        DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (session) {
            DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault,
                                                     session, diskName.c_str());
            if (disk) {
                CFDictionaryRef diskDesc = DADiskCopyDescription(disk);
                if (diskDesc) {
                    // Check if ejectable/removable
                    CFBooleanRef ejectable = (CFBooleanRef)CFDictionaryGetValue(
                        diskDesc, kDADiskDescriptionMediaEjectableKey);
                    CFBooleanRef removable = (CFBooleanRef)CFDictionaryGetValue(
                        diskDesc, kDADiskDescriptionMediaRemovableKey);

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
    }

    // For BSD, we simplify by just setting the model to the device name
    // A more complete implementation would use ioctls to get media info
    info.model = info.devicePath;

    // Simple check for removable media (not comprehensive)
    info.isRemovable =
        (info.devicePath.find("da") == 0 || info.devicePath.find("cd") == 0 ||
         info.devicePath.find("md") == 0);
#endif

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_diskInfoCache[path] = {info, std::chrono::steady_clock::now()};
    }

    return info;
}

std::vector<DiskInfo> getDiskInfo(bool includeRemovable) {
    std::vector<DiskInfo> result;
    auto drives =
        getAvailableDrives(true);  // Get all drives including removable

    for (const auto& drive : drives) {
        DiskInfo info = getDiskInfoCached(drive);

        // Filter based on removable setting
        if (!includeRemovable && info.isRemovable) {
            continue;
        }

        result.push_back(info);
    }

    return result;
}

std::vector<std::pair<std::string, float>> getDiskUsage() {
    std::vector<std::pair<std::string, float>> diskUsage;

    // Get full disk info and convert to the simplified format
    auto diskInfo = getDiskInfo(true);

    for (const auto& info : diskInfo) {
        diskUsage.emplace_back(info.path, info.usagePercent);
    }

    return diskUsage;
}

std::string getDriveModel(const std::string& drivePath) {
    std::string model;

#ifdef _WIN32
    // Create physical drive path if it's a logical drive
    std::string physicalDrivePath = drivePath;
    if (drivePath.size() == 2 && drivePath[1] == ':') {
        physicalDrivePath += '\\';
    }

    HANDLE hDevice = CreateFileA(physicalDrivePath.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);

    if (hDevice != INVALID_HANDLE_VALUE) {
        // Get storage property
        STORAGE_PROPERTY_QUERY query;
        ZeroMemory(&query, sizeof(query));
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_DESCRIPTOR_HEADER header = {0};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                            sizeof(query), &header, sizeof(header),
                            &bytesReturned, nullptr)) {
            std::vector<char> buffer(header.Size);
            if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                                sizeof(query), buffer.data(), buffer.size(),
                                &bytesReturned, nullptr)) {
                auto* desc =
                    reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
                if (desc->VendorIdOffset != 0) {
                    std::string vendor = buffer.data() + desc->VendorIdOffset;
                    if (!vendor.empty()) {
                        model = vendor;
                    }
                }
                if (desc->ProductIdOffset != 0) {
                    std::string product = buffer.data() + desc->ProductIdOffset;
                    if (!product.empty()) {
                        if (!model.empty()) {
                            model += " ";
                        }
                        model += product;
                    }
                }
            }
        }
        CloseHandle(hDevice);
    }
#elif __APPLE__
    // For Apple, use IOKit to get drive model
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (session != nullptr) {
        // Extract disk identifier (e.g., 'disk1' from '/dev/disk1')
        std::string diskName = drivePath;
        size_t lastSlash = diskName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            diskName = diskName.substr(lastSlash + 1);
        }

        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
                                                 diskName.c_str());
        if (disk != nullptr) {
            CFDictionaryRef diskDesc = DADiskCopyDescription(disk);
            if (diskDesc != nullptr) {
                // Get model string
                CFStringRef modelRef = (CFStringRef)CFDictionaryGetValue(
                    diskDesc, kDADiskDescriptionDeviceModelKey);
                if (modelRef != nullptr) {
                    char modelBuffer[256];
                    if (CFStringGetCString(modelRef, modelBuffer,
                                           sizeof(modelBuffer),
                                           kCFStringEncodingUTF8)) {
                        model = modelBuffer;
                    }
                }
                CFRelease(diskDesc);
            }
            CFRelease(disk);
        }
        CFRelease(session);
    }
#elif __linux__
    // Extract device name from path
    std::string deviceName = drivePath;
    size_t lastSlash = deviceName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        deviceName = deviceName.substr(lastSlash + 1);
    }

    // First try /sys/block/{device}/device/model
    std::string modelPath = "/sys/block/" + deviceName + "/device/model";
    std::ifstream modelFile(modelPath);
    if (modelFile.is_open()) {
        std::getline(modelFile, model);
        // Trim whitespace
        model.erase(model.find_last_not_of(" \t\n\r\f\v") + 1);
        model.erase(0, model.find_first_not_of(" \t\n\r\f\v"));
    } else {
        // If we can't directly access by device name, try with partitions
        // (e.g., sda1 -> sda)
        std::string baseDevice = deviceName;
        for (int i = deviceName.length() - 1; i >= 0; --i) {
            if (!isdigit(deviceName[i])) {
                baseDevice = deviceName.substr(0, i + 1);
                break;
            }
        }
        if (baseDevice != deviceName) {
            modelPath = "/sys/block/" + baseDevice + "/device/model";
            std::ifstream baseModelFile(modelPath);
            if (baseModelFile.is_open()) {
                std::getline(baseModelFile, model);
                // Trim whitespace
                model.erase(model.find_last_not_of(" \t\n\r\f\v") + 1);
                model.erase(0, model.find_first_not_of(" \t\n\r\f\v"));
            }
        }
    }

    // If still empty, try vendor + model
    if (model.empty()) {
        std::string vendorPath = "/sys/block/" + deviceName + "/device/vendor";
        std::ifstream vendorFile(vendorPath);
        std::string vendor;
        if (vendorFile.is_open() && std::getline(vendorFile, vendor)) {
            // Trim whitespace
            vendor.erase(vendor.find_last_not_of(" \t\n\r\f\v") + 1);
            vendor.erase(0, vendor.find_first_not_of(" \t\n\r\f\v"));

            // Try again for model
            modelFile.open(modelPath);
            if (modelFile.is_open() && std::getline(modelFile, model)) {
                // Trim whitespace
                model.erase(model.find_last_not_of(" \t\n\r\f\v") + 1);
                model.erase(0, model.find_first_not_of(" \t\n\r\f\v"));

                model = vendor + " " + model;
            } else {
                model = vendor;
            }
        }
    }

    // If still empty, use device name
    if (model.empty()) {
        model = deviceName;
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
// For BSD systems, use camcontrol (FreeBSD)
#ifdef __FreeBSD__
    // We would use camcontrol here if we needed it
    // For simplicity, we're using just the device name
    std::string deviceName = drivePath;
    size_t lastSlash = deviceName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        deviceName = deviceName.substr(lastSlash + 1);
    }
    model = deviceName;
#endif

    // If model is still empty, use device name
    if (model.empty()) {
        std::string deviceName = drivePath;
        size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }
        model = deviceName;
    }
#endif

    // If model is still empty after all attempts
    if (model.empty()) {
        model = "Unknown Device";
    }

    return model;
}

}  // namespace atom::system
