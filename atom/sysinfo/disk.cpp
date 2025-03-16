/*
 * disk.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk

**************************************************/

#include "atom/sysinfo/disk.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#include <winioctl.h>
#pragma comment(lib, "setupapi.lib")
#elif __linux__
#include <blkid/blkid.h>
#include <dirent.h>
#include <libudev.h>
#include <limits.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <mntent.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/disk.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/ucred.h>
#endif

#include "atom/log/loguru.hpp"

namespace fs = std::filesystem;

namespace atom::system {

// Global mutex for whitelisted devices
static std::mutex g_whitelistMutex;
static std::unordered_set<std::string> g_whitelistedDevices = {"SD1234",
                                                               "SD5678"};

// Cache map to store drive info with expiration time
static std::mutex g_cacheMutex;
static std::unordered_map<
    std::string, std::pair<DiskInfo, std::chrono::steady_clock::time_point>>
    g_diskInfoCache;
static const auto CACHE_EXPIRATION = std::chrono::minutes(5);

// Atomic flag for device monitoring
static std::atomic_bool g_monitoringActive = false;

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
        info.model = getDriveModel(volumeName);
    }
#elif __linux__
    struct statfs stats {};
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
        std::istringstream iss(line);
        std::string device, mountpoint, fstype;
        iss >> device >> mountpoint;

        if (mountpoint == path) {
            info.devicePath = device;
            break;
        }
    }

    // Check if removable and get model
    if (!info.devicePath.empty()) {
        std::string sysPath =
            "/sys/block/" +
            info.devicePath.substr(info.devicePath.find_last_of('/') + 1);

        std::ifstream removable(sysPath + "/removable");
        char isRemovable = '0';
        if (removable >> isRemovable) {
            info.isRemovable = (isRemovable == '1');
        }

        info.model = getDriveModel(info.devicePath);
    }
#elif __APPLE__
    struct statfs stats {};
    if (statfs(path.c_str(), &stats) == 0) {
        info.totalSpace = static_cast<uint64_t>(stats.f_blocks) * stats.f_bsize;
        info.freeSpace = static_cast<uint64_t>(stats.f_bfree) * stats.f_bsize;
        info.usagePercent = static_cast<float>(
            calculateDiskUsagePercentage(info.totalSpace, info.freeSpace));
        info.devicePath = stats.f_mntfromname;
    }

    // Get model and check if removable using IOKit
    if (!info.devicePath.empty()) {
        info.model = getDriveModel(info.devicePath);

        // Check if removable using IOKit (simplified)
        io_service_t service = IOServiceGetMatchingService(
            kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0,
                                                    info.devicePath.c_str()));

        if (service) {
            CFTypeRef property = IORegistryEntryCreateCFProperty(
                service, CFSTR(kIOPropertyRemovableKey), kCFAllocatorDefault,
                0);

            if (property) {
                info.isRemovable = CFBooleanGetValue((CFBooleanRef)property);
                CFRelease(property);
            }

            IOObjectRelease(service);
        }
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs stats {};
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
        char volumePath[MAX_PATH] = {0};
        if (GetVolumeNameForVolumeMountPointA((drivePath + "\\").c_str(),
                                              volumePath, MAX_PATH)) {
            physicalDrivePath = volumePath;
        }
    }

    HANDLE hDevice = CreateFileA(physicalDrivePath.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);

    if (hDevice != INVALID_HANDLE_VALUE) {
        STORAGE_PROPERTY_QUERY query = {};
        std::array<char, 1024> buffer = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        DWORD bytesReturned = 0;

        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                            sizeof(query), buffer.data(), buffer.size(),
                            &bytesReturned, nullptr)) {
            auto desc =
                reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

            // Only access offsets that are valid
            std::string vendorId, productId, productRevision;

            if (desc->VendorIdOffset != 0) {
                vendorId = buffer.data() + desc->VendorIdOffset;
            }

            if (desc->ProductIdOffset != 0) {
                productId = buffer.data() + desc->ProductIdOffset;
            }

            if (desc->ProductRevisionOffset != 0) {
                productRevision = buffer.data() + desc->ProductRevisionOffset;
            }

            // Trim whitespace
            auto trim = [](std::string& s) {
                s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
                s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
            };

            trim(vendorId);
            trim(productId);
            trim(productRevision);

            model = vendorId;
            if (!model.empty() && !productId.empty())
                model += " ";
            model += productId;
            if (!model.empty() && !productRevision.empty())
                model += " ";
            model += productRevision;

            // If everything failed, use the drive letter
            if (model.empty()) {
                model = "Drive " + physicalDrivePath.substr(0, 1);
            }
        }
        CloseHandle(hDevice);
    }
#elif __APPLE__
    // For Apple, use IOKit to get drive model
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (session != nullptr) {
        // Create a URL from the drive path
        CFStringRef pathString = CFStringCreateWithCString(
            kCFAllocatorDefault, drivePath.c_str(), kCFStringEncodingUTF8);

        if (pathString != nullptr) {
            CFURLRef url = CFURLCreateWithFileSystemPath(
                kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false);

            CFRelease(pathString);

            if (url != nullptr) {
                // Get BSD name from the URL
                char bsdName[MAXPATHLEN] = {0};
                if (CFURLGetFileSystemRepresentation(url, true, (UInt8*)bsdName,
                                                     MAXPATHLEN)) {
                    DADiskRef disk = DADiskCreateFromBSDName(
                        kCFAllocatorDefault, session, bsdName);

                    if (disk != nullptr) {
                        CFDictionaryRef desc = DADiskCopyDescription(disk);
                        if (desc != nullptr) {
                            CFStringRef modelRef =
                                (CFStringRef)CFDictionaryGetValue(
                                    desc, kDADiskDescriptionDeviceModelKey);

                            if (modelRef != nullptr) {
                                char buffer[256] = {0};
                                if (CFStringGetCString(modelRef, buffer,
                                                       sizeof(buffer),
                                                       kCFStringEncodingUTF8)) {
                                    model = buffer;
                                }
                            }
                            CFRelease(desc);
                        }
                        CFRelease(disk);
                    }
                }
                CFRelease(url);
            }
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
    }
    // If that fails, try using lsblk
    else {
        std::string command =
            "lsblk -no MODEL /dev/" + deviceName + " 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                model = buffer;
                // Trim newline
                model.erase(model.find_last_not_of(" \t\n\r\f\v") + 1);
            }
            pclose(pipe);
        }
    }

    // If still empty, try vendor + model
    if (model.empty()) {
        std::string vendorPath = "/sys/block/" + deviceName + "/device/vendor";
        std::ifstream vendorFile(vendorPath);
        std::string vendor;
        if (vendorFile.is_open() && std::getline(vendorFile, vendor)) {
            vendor.erase(vendor.find_last_not_of(" \t\n\r\f\v") + 1);
            if (!vendor.empty()) {
                model = vendor;

                // Try again for model
                modelFile.close();
                modelFile.open(modelPath);
                std::string modelPart;
                if (modelFile.is_open() && std::getline(modelFile, modelPart)) {
                    modelPart.erase(modelPart.find_last_not_of(" \t\n\r\f\v") +
                                    1);
                    if (!modelPart.empty()) {
                        model += " " + modelPart;
                    }
                }
            }
        }
    }

    // If still empty, use device name
    if (model.empty()) {
        model = "Device " + deviceName;
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
// For BSD systems, use camcontrol (FreeBSD)
#ifdef __FreeBSD__
    std::string command =
        "camcontrol identify " + drivePath + " 2>/dev/null | grep 'model'";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string output = buffer;
            // Extract model from output
            size_t pos = output.find("model");
            if (pos != std::string::npos) {
                pos = output.find("\"", pos);
                if (pos != std::string::npos) {
                    size_t end = output.find("\"", pos + 1);
                    if (end != std::string::npos) {
                        model = output.substr(pos + 1, end - pos - 1);
                    }
                }
            }
        }
        pclose(pipe);
    }
#endif

    // If model is still empty, use device name
    if (model.empty()) {
        model = "Device " + drivePath;
    }
#endif

    // If model is still empty after all attempts
    if (model.empty()) {
        model = "Unknown Device";
    }

    return model;
}

std::vector<StorageDevice> getStorageDevices(bool includeRemovable) {
    std::vector<StorageDevice> devices;

#ifdef _WIN32
    // For Windows, use SetupDi functions to enumerate storage devices
    HDEVINFO hDevInfo =
        SetupDiGetClassDevs(&GUID_DEVCLASS_DISKDRIVE, 0, 0, DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to get device info set.");
        return devices;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        char devicePath[MAX_PATH] = {0};
        char deviceDesc[256] = {0};
        DWORD propertyBufferSize = 0;

        // Get device path
        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, NULL,
            (PBYTE)devicePath, sizeof(devicePath), &propertyBufferSize);

        // Get device description/model
        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)deviceDesc,
            sizeof(deviceDesc), &propertyBufferSize);

        // Create device entry
        StorageDevice device;
        device.devicePath = devicePath;
        device.model = deviceDesc;

        // Check if removable
        DWORD capabilities = 0;
        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_CAPABILITIES, NULL,
            (PBYTE)&capabilities, sizeof(capabilities), NULL);

        device.isRemovable = (capabilities & CM_DEVCAP_REMOVABLE) != 0;

        // Get size
        char instanceId[256] = {0};
        SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, instanceId,
                                    sizeof(instanceId), NULL);

        // We'd need to query the disk geometry to get the size
        // This is a simplified version
        device.sizeBytes = 0;

        // Add device if it matches removable filter
        if (includeRemovable || !device.isRemovable) {
            devices.push_back(device);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

#elif __linux__
    // For Linux, use libudev to enumerate block devices
    struct udev* udev = udev_new();
    if (!udev) {
        LOG_F(ERROR, "Failed to create udev context.");
        return devices;
    }

    struct udev_enumerate* enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devices_list =
        udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry;

    udev_list_entry_foreach(entry, devices_list) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(udev, path);

        if (dev) {
            const char* devnode = udev_device_get_devnode(dev);
            if (devnode) {
                StorageDevice device;
                device.devicePath = devnode;

                // Get model
                const char* model =
                    udev_device_get_property_value(dev, "ID_MODEL");
                if (model) {
                    device.model = model;
                } else {
                    device.model = "Unknown";
                }

                // Get serial
                const char* serial =
                    udev_device_get_property_value(dev, "ID_SERIAL");
                if (serial) {
                    device.serialNumber = serial;
                }

                // Check if removable
                const char* removable =
                    udev_device_get_sysattr_value(dev, "removable");
                device.isRemovable = removable && std::string(removable) == "1";

                // Get size
                const char* size = udev_device_get_sysattr_value(dev, "size");
                if (size) {
                    try {
                        uint64_t blocks = std::stoull(size);
                        device.sizeBytes =
                            blocks * 512;  // Typically 512 bytes per block
                    } catch (...) {
                        device.sizeBytes = 0;
                    }
                }

                // Add device if it matches removable filter
                if (includeRemovable || !device.isRemovable) {
                    devices.push_back(device);
                }
            }
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

#elif __APPLE__
    // For macOS, use IOKit to enumerate storage devices
    CFMutableDictionaryRef matchingDict = IOServiceMatching("IOMedia");
    if (matchingDict) {
        // Add property to match whole disks
        CFDictionarySetValue(matchingDict, CFSTR(kIOMediaWholeKey),
                             kCFBooleanTrue);

        io_iterator_t iter;
        if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                         &iter) == KERN_SUCCESS) {
            io_service_t service;
            while ((service = IOIteratorNext(iter))) {
                StorageDevice device;

                // Get BSD name
                CFTypeRef bsdNameRef = IORegistryEntryCreateCFProperty(
                    service, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);

                if (bsdNameRef) {
                    char buffer[128] = {0};
                    CFStringGetCString((CFStringRef)bsdNameRef, buffer,
                                       sizeof(buffer), kCFStringEncodingUTF8);

                    device.devicePath = "/dev/";
                    device.devicePath += buffer;
                    CFRelease(bsdNameRef);
                }

                // Get model
                CFTypeRef modelRef = IORegistryEntrySearchCFProperty(
                    service, kIOServicePlane, CFSTR(kIOPropertyProductNameKey),
                    kCFAllocatorDefault, kIORegistryIterateRecursively);

                if (modelRef) {
                    char buffer[128] = {0};
                    CFStringGetCString((CFStringRef)modelRef, buffer,
                                       sizeof(buffer), kCFStringEncodingUTF8);

                    device.model = buffer;
                    CFRelease(modelRef);
                } else {
                    device.model = "Unknown";
                }

                // Check if removable
                CFTypeRef removableRef = IORegistryEntrySearchCFProperty(
                    service, kIOServicePlane, CFSTR(kIOPropertyRemovableKey),
                    kCFAllocatorDefault, kIORegistryIterateRecursively);

                if (removableRef) {
                    device.isRemovable =
                        CFBooleanGetValue((CFBooleanRef)removableRef);
                    CFRelease(removableRef);
                }

                // Get size
                CFTypeRef sizeRef = IORegistryEntryCreateCFProperty(
                    service, CFSTR(kIOMediaSizeKey), kCFAllocatorDefault, 0);

                if (sizeRef) {
                    CFNumberGetValue((CFNumberRef)sizeRef, kCFNumberSInt64Type,
                                     &device.sizeBytes);
                    CFRelease(sizeRef);
                }

                // Add device if it matches removable filter
                if (includeRemovable || !device.isRemovable) {
                    devices.push_back(device);
                }

                IOObjectRelease(service);
            }
            IOObjectRelease(iter);
        }
    }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
// For BSD, get disks from sysctl or geom
#ifdef __FreeBSD__
    // Use FreeBSD GEOM framework
    FILE* pipe = popen("geom disk list", "r");
    if (pipe) {
        char buffer[1024];
        StorageDevice* currentDevice = nullptr;

        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);

            // New disk entry
            if (line.find("Geom name:") != std::string::npos) {
                if (currentDevice) {
                    // Add completed device if it matches filter
                    if (includeRemovable || !currentDevice->isRemovable) {
                        devices.push_back(*currentDevice);
                    }
                    delete currentDevice;
                }

                currentDevice = new StorageDevice();
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string name = line.substr(pos + 1);
                    name.erase(0, name.find_first_not_of(" \t\r\n"));
                    name.erase(name.find_last_not_of(" \t\r\n") + 1);

                    currentDevice->devicePath = "/dev/" + name;
                }
            }
            // Device properties
            else if (currentDevice) {
                if (line.find("descr:") != std::string::npos) {
                    size_t pos = line.find(':');
                    if (pos != std::string::npos) {
                        std::string descr = line.substr(pos + 1);
                        descr.erase(0, descr.find_first_not_of(" \t\r\n"));
                        descr.erase(descr.find_last_not_of(" \t\r\n") + 1);

                        currentDevice->model = descr;
                    }
                } else if (line.find("Mediasize:") != std::string::npos) {
                    size_t pos = line.find(':');
                    if (pos != std::string::npos) {
                        std::string size = line.substr(pos + 1);
                        pos = size.find('(');
                        if (pos != std::string::npos) {
                            size_t end = size.find(' ', pos);
                            if (end != std::string::npos) {
                                std::string sizeStr =
                                    size.substr(pos + 1, end - pos - 1);
                                try {
                                    currentDevice->sizeBytes =
                                        std::stoull(sizeStr);
                                } catch (...) {
                                    currentDevice->sizeBytes = 0;
                                }
                            }
                        }
                    }
                }
                // Simple check for removable based on device name
                currentDevice->isRemovable =
                    currentDevice->devicePath.find("da") == 5 ||
                    currentDevice->devicePath.find("cd") == 5;
            }
        }

        // Add last device if it exists
        if (currentDevice) {
            if (includeRemovable || !currentDevice->isRemovable) {
                devices.push_back(*currentDevice);
            }
            delete currentDevice;
        }

        pclose(pipe);
    }
#endif
#endif

    return devices;
}

std::vector<std::pair<std::string, std::string>> getStorageDeviceModels() {
    std::vector<std::pair<std::string, std::string>> result;

    // Use the new StorageDevice structure and convert to the old format
    auto devices = getStorageDevices(true);

    for (const auto& device : devices) {
        result.emplace_back(device.devicePath, device.model);
    }

    return result;
}

std::vector<std::string> getAvailableDrives(bool includeRemovable) {
    std::vector<std::string> drives;

#ifdef _WIN32
    DWORD drivesBitMask = GetLogicalDrives();
    for (char i = 'A'; i <= 'Z'; ++i) {
        if (drivesBitMask & 1) {
            std::string drive(1, i);
            drive += ":\\";

            // Check if drive is removable if filtering is requested
            if (!includeRemovable) {
                UINT driveType = GetDriveTypeA(drive.c_str());
                if (driveType == DRIVE_REMOVABLE) {
                    drivesBitMask >>= 1;
                    continue;
                }
            }

            drives.push_back(drive);
        }
        drivesBitMask >>= 1;
    }
#elif __linux__
    // Get mounted filesystems from /proc/mounts
    std::ifstream mountsFile("/proc/mounts");
    std::string line;

    while (std::getline(mountsFile, line)) {
        std::istringstream iss(line);
        std::string device, mountPoint, fsType;
        iss >> device >> mountPoint >> fsType;

        // Skip pseudo filesystems
        if (fsType == "proc" || fsType == "sysfs" || fsType == "devtmpfs" ||
            fsType == "devpts" || fsType == "tmpfs" || fsType == "debugfs" ||
            fsType == "securityfs" || fsType == "cgroup" ||
            fsType == "pstore" || fsType == "autofs" || fsType == "mqueue" ||
            fsType == "hugetlbfs" || fsType == "fusectl") {
            continue;
        }

        // Check if removable if filtering is requested
        if (!includeRemovable) {
            std::string sysDevicePath;

            // Extract device name (e.g., sda1 from /dev/sda1)
            std::string deviceName = device;
            size_t lastSlash = deviceName.find_last_of('/');
            if (lastSlash != std::string::npos) {
                deviceName = deviceName.substr(lastSlash + 1);
            }

            // Remove partition number to get base device
            std::string baseDevice = deviceName;
            while (!baseDevice.empty() && std::isdigit(baseDevice.back())) {
                baseDevice.pop_back();
            }

            // Check removable flag in sysfs
            std::string removablePath =
                "/sys/block/" + baseDevice + "/removable";
            std::ifstream removableFile(removablePath);
            char isRemovable = '0';
            if (removableFile >> isRemovable && isRemovable == '1') {
                continue;
            }
        }

        drives.push_back(mountPoint);
    }
#elif __APPLE__
    struct statfs* mounts;
    int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

    for (int i = 0; i < numMounts; ++i) {
        // Skip system filesystems
        if (strcmp(mounts[i].f_fstypename, "devfs") == 0 ||
            strcmp(mounts[i].f_fstypename, "autofs") == 0) {
            continue;
        }

        // Check if removable if filtering is requested
        if (!includeRemovable) {
            // Simple check for common removable media paths
            std::string mountPoint = mounts[i].f_mntonname;
            if (mountPoint.find("/Volumes/") == 0 &&
                mountPoint != "/Volumes/Macintosh HD") {
                // Check if it's a USB or external drive
                bool isRemovable = false;

                // Create a session
                DASessionRef session = DASessionCreate(kCFAllocatorDefault);
                if (session) {
                    // Create disk reference
                    DADiskRef disk = DADiskCreateFromBSDName(
                        kCFAllocatorDefault, session, mounts[i].f_mntfromname);

                    if (disk) {
                        // Get disk description
                        CFDictionaryRef descRef = DADiskCopyDescription(disk);
                        if (descRef) {
                            // Check if device is removable
                            CFBooleanRef removableRef =
                                (CFBooleanRef)CFDictionaryGetValue(
                                    descRef,
                                    kDADiskDescriptionMediaRemovableKey);

                            if (removableRef &&
                                CFBooleanGetValue(removableRef)) {
                                isRemovable = true;
                            }

                            CFRelease(descRef);
                        }
                        CFRelease(disk);
                    }
                    CFRelease(session);
                }

                if (isRemovable) {
                    continue;
                }
            }
        }

        drives.push_back(mounts[i].f_mntonname);
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs* mounts;
    int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

    for (int i = 0; i < numMounts; ++i) {
        // Skip pseudo filesystems
        if (strcmp(mounts[i].f_fstypename, "devfs") == 0 ||
            strcmp(mounts[i].f_fstypename, "procfs") == 0 ||
            strcmp(mounts[i].f_fstypename, "kernfs") == 0 ||
            strcmp(mounts[i].f_fstypename, "fdescfs") == 0) {
            continue;
        }

        // Check if removable if filtering is requested
        if (!includeRemovable) {
            std::string devicePath = mounts[i].f_mntfromname;

            // Simple check for removable devices
            bool isRemovable = devicePath.find("/dev/da") == 0 ||
                               devicePath.find("/dev/cd") == 0 ||
                               devicePath.find("/dev/acd") == 0 ||
                               devicePath.find("/dev/md") == 0;

            if (isRemovable) {
                continue;
            }
        }

        drives.push_back(mounts[i].f_mntonname);
    }
#endif

    return drives;
}

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
        DWORD error = GetLastError();
        LOG_F(ERROR, "Error retrieving filesystem information for %s: %lu",
              path.c_str(), error);
        return "Unknown";
    }
    return std::string(fileSystemNameBuffer);

#elif __linux__ || __ANDROID__
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Error retrieving filesystem information for %s: %s",
              path.c_str(), strerror(errno));
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
                std::istringstream iss(line);
                std::string device, mountpoint, fstype;
                iss >> device >> mountpoint;
                if (iss >> fstype) {
                    // Decode escaped spaces in mountpoint
                    std::string decodedPath;
                    for (size_t i = 0; i < mountpoint.length(); ++i) {
                        if (mountpoint[i] == '\\' &&
                            i + 3 < mountpoint.length() &&
                            mountpoint[i + 1] == '0' &&
                            mountpoint[i + 2] == '4' &&
                            mountpoint[i + 3] == '0') {
                            decodedPath += ' ';
                            i += 3;
                        } else {
                            decodedPath += mountpoint[i];
                        }
                    }

                    if (decodedPath == path) {
                        return fstype;
                    }
                }
            }
            return "Unknown";
    }

#elif __APPLE__
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Error retrieving filesystem information for %s: %s",
              path.c_str(), strerror(errno));
        return "Unknown";
    }

    // macOS returns filesystem type directly
    return buffer.f_fstypename;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs buffer;
    if (statfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Error retrieving filesystem information for %s: %s",
              path.c_str(), strerror(errno));
        return "Unknown";
    }

    return buffer.f_fstypename;

#else
    // Generic fallback for other Unix systems
    struct statvfs buffer;
    if (statvfs(path.c_str(), &buffer) != 0) {
        LOG_F(ERROR, "Error retrieving filesystem information for %s: %s",
              path.c_str(), strerror(errno));
        return "Unknown";
    }

    // Unfortunately statvfs doesn't provide filesystem type directly
    // We would need to check /etc/mtab or similar
    return "Unknown";
#endif
}

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

std::optional<std::string> getDeviceSerialNumber(
    const std::string& devicePath) {
#ifdef _WIN32
    // For Windows, get the serial number from setupapi
    HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                 OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to open device %s: %lu", devicePath.c_str(),
              GetLastError());
        return std::nullopt;
    }

    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    STORAGE_DESCRIPTOR_HEADER header = {0};
    DWORD bytesReturned = 0;

    // First get the necessary size
    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                         sizeof(query), &header, sizeof(header), &bytesReturned,
                         NULL)) {
        LOG_F(ERROR, "Failed to query device properties: %lu", GetLastError());
        CloseHandle(hDevice);
        return std::nullopt;
    }

    // Allocate buffer for the full descriptor
    std::vector<char> buffer(header.Size);

    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                         sizeof(query), buffer.data(), buffer.size(),
                         &bytesReturned, NULL)) {
        LOG_F(ERROR, "Failed to query device properties: %lu", GetLastError());
        CloseHandle(hDevice);
        return std::nullopt;
    }

    CloseHandle(hDevice);

    // Extract serial number
    auto desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

    if (desc->SerialNumberOffset == 0) {
        LOG_F(INFO, "Device %s has no serial number", devicePath.c_str());
        return std::nullopt;
    }

    std::string serialNumber = buffer.data() + desc->SerialNumberOffset;

    // Trim whitespace
    serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") + 1);
    serialNumber.erase(0, serialNumber.find_first_not_of(" \t\n\r\f\v"));

    if (serialNumber.empty()) {
        return std::nullopt;
    }

    return serialNumber;

#elif __linux__
    // For Linux, try multiple sources for the serial number

    // Extract device name
    std::string deviceName = devicePath;
    size_t lastSlash = deviceName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        deviceName = deviceName.substr(lastSlash + 1);
    }

    // First try: /sys/block/{device}/device/serial
    std::string serialPath = "/sys/block/" + deviceName + "/device/serial";
    std::ifstream serialFile(serialPath);
    std::string serialNumber;

    if (serialFile.is_open() && std::getline(serialFile, serialNumber)) {
        // Trim whitespace
        serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") + 1);
        serialNumber.erase(0, serialNumber.find_first_not_of(" \t\n\r\f\v"));

        if (!serialNumber.empty()) {
            return serialNumber;
        }
    }

    // Second try: Use udev to get the serial
    struct udev* udev = udev_new();
    if (udev) {
        struct udev_device* dev = udev_device_new_from_subsystem_sysname(
            udev, "block", deviceName.c_str());

        if (dev) {
            const char* serial =
                udev_device_get_property_value(dev, "ID_SERIAL");
            if (serial) {
                serialNumber = serial;
                udev_device_unref(dev);
                udev_unref(udev);
                return serialNumber;
            }
            udev_device_unref(dev);
        }
        udev_unref(udev);
    }

    // Third try: Use lsblk
    std::string command =
        "lsblk -no SERIAL /dev/" + deviceName + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            serialNumber = buffer;
            // Trim newline
            serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") +
                               1);
            serialNumber.erase(0,
                               serialNumber.find_first_not_of(" \t\n\r\f\v"));

            if (!serialNumber.empty()) {
                pclose(pipe);
                return serialNumber;
            }
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Could not find serial number for device %s",
          devicePath.c_str());
    return std::nullopt;

#elif __APPLE__
    // For macOS, use IOKit to get serial number
    io_service_t service = 0;

    // Extract disk identifier (e.g., 'disk1' from '/dev/disk1')
    std::string diskName = devicePath;
    size_t lastSlash = diskName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        diskName = diskName.substr(lastSlash + 1);
    }

    // Match BSD name
    CFMutableDictionaryRef matchingDict =
        IOBSDNameMatching(kIOMasterPortDefault, 0, diskName.c_str());

    if (matchingDict) {
        service =
            IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
        // Matchingdict is consumed by IOServiceGetMatchingService
    }

    if (!service) {
        LOG_F(ERROR, "Could not find IO service for %s", devicePath.c_str());
        return std::nullopt;
    }

    // Need to get the parent device that has the serial number
    io_service_t parentService = 0;
    kern_return_t kr =
        IORegistryEntryGetParentEntry(service, kIOServicePlane, &parentService);

    IOObjectRelease(service);

    if (kr != KERN_SUCCESS || !parentService) {
        LOG_F(ERROR, "Could not find parent IO service for %s",
              devicePath.c_str());
        return std::nullopt;
    }

    // Get the serial number property
    CFTypeRef serialRef = IORegistryEntryCreateCFProperty(
        parentService, CFSTR(kIOPropertySerialNumberKey), kCFAllocatorDefault,
        0);

    IOObjectRelease(parentService);

    if (!serialRef) {
        LOG_F(INFO, "Device %s has no serial number property",
              devicePath.c_str());
        return std::nullopt;
    }

    if (CFGetTypeID(serialRef) != CFStringGetTypeID()) {
        LOG_F(ERROR, "Serial number property is not a string");
        CFRelease(serialRef);
        return std::nullopt;
    }

    char buffer[256] = {0};
    if (!CFStringGetCString((CFStringRef)serialRef, buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        LOG_F(ERROR, "Failed to convert serial number to string");
        CFRelease(serialRef);
        return std::nullopt;
    }

    CFRelease(serialRef);

    std::string serialNumber = buffer;
    if (serialNumber.empty()) {
        return std::nullopt;
    }

    return serialNumber;

#elif defined(__FreeBSD__)
    // For FreeBSD, try camcontrol
    std::string command =
        "camcontrol identify " + devicePath + " | grep serial";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string output = buffer;

            // Extract serial number (format varies by device)
            std::regex serialRegex("serial\\s+[\"']?([^\"'\\s]+)[\"']?");
            std::smatch match;
            if (std::regex_search(output, match, serialRegex) &&
                match.size() > 1) {
                pclose(pipe);
                return match[1].str();
            }
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Could not find serial number for device %s",
          devicePath.c_str());
    return std::nullopt;
#else
    // Generic fallback that likely won't work
    LOG_F(WARNING, "Serial number retrieval not implemented for this platform");
    return std::nullopt;
#endif
}

bool setDiskReadOnly(const std::string& path) {
#ifdef _WIN32
    // For Windows, we use either drive letter or volume GUID path
    std::string targetPath = path;

    // If it's a drive letter, make sure it has the correct format
    if (path.length() == 1) {
        targetPath = path + ":\\";
    } else if (path.length() == 2 && path[1] == ':') {
        targetPath = path + "\\";
    }

    // Create a path in the format \\.\X: for DeviceIoControl
    std::string devicePath = R"(\\.\)";
    devicePath += targetPath[0];
    devicePath += ":";

    HANDLE hDevice = CreateFileA(
        devicePath.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to open device %s: %lu", devicePath.c_str(),
              GetLastError());
        return false;
    }

    // Try to lock the volume to make it read-only
    DWORD bytesReturned;
    bool success = DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0,
                                   &bytesReturned, NULL) != 0;

    if (success) {
        LOG_F(INFO, "Successfully locked volume %s as read-only", path.c_str());

        // We intentionally don't close the handle as that would unlock the
        // volume This means the handle will be leaked, but the volume will
        // remain locked In a real application, we should store this handle for
        // later cleanup
        return true;
    } else {
        LOG_F(ERROR, "Failed to lock volume %s: %lu", path.c_str(),
              GetLastError());
        CloseHandle(hDevice);
        return false;
    }

#elif __linux__
    // For Linux, we remount the filesystem as read-only
    std::string command = "mount -o remount,ro " + path + " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "Failed to execute remount command");
        return false;
    }

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status == 0) {
        LOG_F(INFO, "Successfully remounted %s as read-only", path.c_str());
        return true;
    } else {
        LOG_F(ERROR, "Failed to remount %s as read-only: %s", path.c_str(),
              result.c_str());
        return false;
    }

#elif __APPLE__
    // For macOS, we use diskutil

    // Extract disk identifier (e.g., 'disk1s1' from '/dev/disk1s1')
    std::string diskName = path;
    size_t lastSlash = diskName.find_last_of('/');
    if (lastSlash != std::string::npos) {
        diskName = diskName.substr(lastSlash + 1);
    }

    std::string command = "diskutil mount readOnly " + diskName + " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "Failed to execute diskutil command");
        return false;
    }

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status == 0) {
        LOG_F(INFO, "Successfully mounted %s as read-only", path.c_str());
        return true;
    } else {
        LOG_F(ERROR, "Failed to mount %s as read-only: %s", path.c_str(),
              result.c_str());
        return false;
    }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    // For BSD systems, use mount -u
    std::string command = "mount -u -o ro " + path + " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        LOG_F(ERROR, "Failed to execute mount command");
        return false;
    }

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status == 0) {
        LOG_F(INFO, "Successfully remounted %s as read-only", path.c_str());
        return true;
    } else {
        LOG_F(ERROR, "Failed to remount %s as read-only: %s", path.c_str(),
              result.c_str());
        return false;
    }
#else
    LOG_F(ERROR, "Setting disk read-only not supported on this platform");
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
        std::function<void(const fs::path&, int)> scanDirectory =
            [&](const fs::path& directory, int currentDepth) {
                if (scanDepth > 0 && currentDepth > scanDepth) {
                    return;
                }

                for (const auto& entry : fs::directory_iterator(directory)) {
                    try {
                        // Check if file or directory is hidden
                        bool isHidden = false;
                        std::string filename = entry.path().filename().string();

#ifdef _WIN32
                        DWORD attributes =
                            GetFileAttributesA(entry.path().string().c_str());
                        isHidden = (attributes != INVALID_FILE_ATTRIBUTES) &&
                                   (attributes & FILE_ATTRIBUTE_HIDDEN);
#else
                        isHidden = (filename[0] == '.');
#endif

                        if (entry.is_regular_file()) {
                            // Check extension
                            std::string extension =
                                entry.path().extension().string();
                            std::transform(extension.begin(), extension.end(),
                                           extension.begin(), ::tolower);

                            bool isSuspicious =
                                suspiciousExtensions.find(extension) !=
                                suspiciousExtensions.end();

                            // Check filename against patterns
                            for (const auto& pattern : suspiciousPatterns) {
                                if (std::regex_search(filename,
                                                      pattern.second)) {
                                    isSuspicious = true;
                                    LOG_F(WARNING,
                                          "Suspicious file pattern (%s): %s",
                                          pattern.first.c_str(),
                                          entry.path().string().c_str());
                                    break;
                                }
                            }

                            // Additional check for hidden executables
                            if (isHidden && isSuspicious) {
                                LOG_F(WARNING, "Hidden suspicious file: %s",
                                      entry.path().string().c_str());
                                suspiciousCount++;
                            } else if (isSuspicious) {
                                LOG_F(WARNING, "Suspicious file: %s",
                                      entry.path().string().c_str());
                                suspiciousCount++;
                            }

                            // For real threat detection, we would scan file
                            // contents, check for signatures, etc.
                        } else if (entry.is_directory()) {
                            // Recursively scan subdirectories
                            scanDirectory(entry.path(), currentDepth + 1);
                        }
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Error scanning %s: %s",
                              entry.path().string().c_str(), e.what());
                        success = false;
                    }
                }
            };

        // Start scanning from root path
        scanDirectory(fs::path(path), 0);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error scanning %s: %s", path.c_str(), e.what());
        success = false;
    }

    LOG_F(INFO, "Scan completed for %s. Found %d suspicious files.",
          path.c_str(), suspiciousCount);

    return {success, suspiciousCount};
}

// Device monitoring implementation
std::future<void> startDeviceMonitoring(
    std::function<void(const StorageDevice&)> callback,
    SecurityPolicy securityPolicy) {
    // Create atomic flag to control monitoring thread
    g_monitoringActive = true;

    // Create the future that will run the monitoring
    return std::async(std::launch::async, [callback, securityPolicy]() {
        LOG_F(INFO, "Starting device monitoring with security policy %d",
              static_cast<int>(securityPolicy));

#ifdef _WIN32
        // Windows-specific device monitoring
        while (g_monitoringActive) {
            // Get current devices
            static std::unordered_set<std::string> knownDevices;
            std::vector<StorageDevice> currentDevices = getStorageDevices(true);
            std::unordered_set<std::string> currentPaths;

            for (const auto& device : currentDevices) {
                currentPaths.insert(device.devicePath);

                // Check if this is a new device
                if (knownDevices.find(device.devicePath) ==
                    knownDevices.end()) {
                    LOG_F(INFO, "New device detected: %s (%s)",
                          device.devicePath.c_str(), device.model.c_str());

                    // Apply security policy
                    if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                        auto serialOpt =
                            getDeviceSerialNumber(device.devicePath);
                        std::string identifier =
                            serialOpt.value_or(device.devicePath);

                        if (!isDeviceInWhitelist(identifier)) {
                            LOG_F(WARNING, "Non-whitelisted device blocked: %s",
                                  device.devicePath.c_str());
                            continue;
                        }
                    }

                    if (securityPolicy == SecurityPolicy::READ_ONLY) {
                        // For removable devices, set read-only
                        if (device.isRemovable) {
                            // Get drive letter
                            char driveLetter = 0;

                            // Simple heuristic to find the drive letter
                            for (char letter = 'A'; letter <= 'Z'; letter++) {
                                std::string drive(1, letter);
                                drive += ":\\";

                                UINT driveType = GetDriveTypeA(drive.c_str());
                                if (driveType == DRIVE_REMOVABLE) {
                                    // This is a rough guess, we'd need more
                                    // precise matching in a real implementation
                                    driveLetter = letter;
                                    break;
                                }
                            }

                            if (driveLetter != 0) {
                                std::string drivePath(1, driveLetter);
                                setDiskReadOnly(drivePath);
                            }
                        }
                    }

                    if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                        // Get all drives and try to match to the device
                        auto drives = getAvailableDrives(true);
                        for (const auto& drive : drives) {
                            // Simple filter for removable drives
                            UINT driveType = GetDriveTypeA(drive.c_str());
                            if (driveType == DRIVE_REMOVABLE) {
                                auto [success, threats] =
                                    scanDiskForThreats(drive, 2);
                                if (threats > 0) {
                                    LOG_F(WARNING,
                                          "Threats detected on %s - setting "
                                          "read-only",
                                          drive.c_str());
                                    setDiskReadOnly(drive);
                                }
                            }
                        }
                    }

                    // Call the callback
                    callback(device);

                    // Add to known devices
                    knownDevices.insert(device.devicePath);
                }
            }

            // Remove devices that are no longer present
            for (auto it = knownDevices.begin(); it != knownDevices.end();) {
                if (currentPaths.find(*it) == currentPaths.end()) {
                    LOG_F(INFO, "Device removed: %s", it->c_str());
                    it = knownDevices.erase(it);
                } else {
                    ++it;
                }
            }

            // Sleep to avoid high CPU usage
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

#elif __linux__
        // Linux-specific device monitoring with inotify and udev
        struct udev* udev = udev_new();
        if (!udev) {
            LOG_F(ERROR, "Failed to create udev context.");
            return;
        }
        
        struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", NULL);
        udev_monitor_enable_receiving(monitor);
        
        int fd = udev_monitor_get_fd(monitor);
        
        while (g_monitoringActive) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            
            // Set timeout to 2 seconds
            struct timeval tv;
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            
            int ret = select(fd + 1, &fds, NULL, NULL, &tv);
            
            if (ret > 0 && FD_ISSET(fd, &fds)) {
                struct udev_device* dev = udev_monitor_receive_device(monitor);
                if (dev) {
                    const char* action = udev_device_get_action(dev);
                    const char* devnode = udev_device_get_devnode(dev);
                    
                    if (action && devnode && strcmp(action, "add") == 0) {
                        LOG_F(INFO, "New device detected: %s", devnode);
                        
                        // Create a StorageDevice object
                        StorageDevice device;
                        device.devicePath = devnode;
                        
                        // Get model
                        const char* model = udev_device_get_property_value(dev, "ID_MODEL");
                        device.model = model ? model : "Unknown";
                        
                        // Get serial
                        const char* serial = udev_device_get_property_value(dev, "ID_SERIAL");
                        device.serialNumber = serial ? serial : "";
                        
                        // Check if removable
                        const char* removable = udev_device_get_sysattr_value(dev, "removable");
                        device.isRemovable = removable && std::string(removable) == "1";
                        
                        // Apply security policy
                        if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                            if (!isDeviceInWhitelist(device.serialNumber)) {
                                LOG_F(WARNING, "Non-whitelisted device blocked: %s", 
                                     device.devicePath.c_str());
                                udev_device_unref(dev);
                                continue;
                            }
                        }
                        
                        // For removable devices, set read-only when policy requires it
                        if (device.isRemovable && 
                            (securityPolicy == SecurityPolicy::READ_ONLY)) {
                            
                            // Wait for the device to be mounted
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            
                            // Find the mount point
                            std::string mountPoint;
                            std::ifstream mounts("/proc/mounts");
                            std::string line;
                            
                            while (std::getline(mounts, line)) {
                                if (line.find(device.devicePath) != std::string::npos) {
                                    std::istringstream iss(line);
                                    std::string dev, mnt;
                                    iss >> dev >> mnt;
                                    mountPoint = mnt;
                                    break;
                                }
                            }
                            
                            if (!mountPoint.empty()) {
                                LOG_F(INFO, "Setting %s as read-only", mountPoint.c_str());
                                setDiskReadOnly(mountPoint);
                            }
                        }
                        
                        if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                            // Wait for the device to be mounted
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            
                            // Find the mount point
                            std::string mountPoint;
                            std::ifstream mounts("/proc/mounts");
                            std::string line;
                            
                            while (std::getline(mounts, line)) {
                                if (line.find(device.devicePath) != std::string::npos) {
                                    std::istringstream iss(line);
                                    std::string dev, mnt;
                                    iss >> dev >> mnt;
                                    mountPoint = mnt;
                                    break;
                                }
                            }
                            
                            if (!mountPoint.empty()) {
                                auto [success, threats] = scanDiskForThreats(mountPoint, 2);
                                if (threats > 0) {
                                    LOG_F(WARNING, "Threats detected on %s - setting read-only", 
                                         mountPoint.c_str());
                                    setDiskReadOnly(mountPoint);
                                }
                            }
                        }
                        
                        // Call the callback
                        callback(device);
                    }
                    
                    udev_device_unref(dev);
                }
            }
            
            // Also check for timeout to handle cancellation
            if (ret == 0) {
                // Timeout occurred, check if we should continue
                if (!g_monitoringActive) {
                    break;
                }
            }
        }
        
        udev_monitor_unref(monitor);
        udev_unref(udev);

#elif __APPLE__
        // macOS-specific device monitoring
        DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (!session) {
            LOG_F(ERROR, "Failed to create DiskArbitration session");
            return;
        }
        
        // Set up dispatch queue
        dispatch_queue_t queue = dispatch_queue_create("com.atom.system.diskmonitor", NULL);
        DASessionSetDispatchQueue(session, queue);
        
        // Known devices set
        static std::unordered_set<std::string> knownDevices;
        
        // Define the disk appeared callback
        DADiskAppearedCallback diskAppearedCallback = ^(DADiskRef disk) {
            CFDictionaryRef descRef = DADiskCopyDescription(disk);
            if (descRef) {
                CFStringRef bsdNameRef = (CFStringRef)CFDictionaryGetValue(
                    descRef, kDADiskDescriptionMediaBSDNameKey);
                    
                if (bsdNameRef) {
                    char bsdName[256] = {0};
                    CFStringGetCString(
                        bsdNameRef, bsdName, sizeof(bsdName), kCFStringEncodingUTF8);
                        
                    std::string devicePath = "/dev/";
                    devicePath += bsdName;
                    
                    // Check if this is a new device
                    if (knownDevices.find(devicePath) == knownDevices.end()) {
                        LOG_F(INFO, "New device detected: %s", devicePath.c_str());
                        
                        // Create a StorageDevice object
                        StorageDevice device;
                        device.devicePath = devicePath;
                        
                        // Get model
                        CFStringRef modelRef = (CFStringRef)CFDictionaryGetValue(
                            descRef, kDADiskDescriptionDeviceModelKey);
                            
                        if (modelRef) {
                            char model[256] = {0};
                            CFStringGetCString(
                                modelRef, model, sizeof(model), kCFStringEncodingUTF8);
                            device.model = model;
                        } else {
                            device.model = "Unknown";
                        }
                        
                        // Check if removable
                        CFBooleanRef removableRef = (CFBooleanRef)CFDictionaryGetValue(
                            descRef, kDADiskDescriptionMediaRemovableKey);
                            
                        device.isRemovable = removableRef && CFBooleanGetValue(removableRef);
                        
                        // Get serial (not always available via DiskArbitration)
                        auto serialOpt = getDeviceSerialNumber(devicePath);
                        if (serialOpt) {
                            device.serialNumber = *serialOpt;
                        }
                        
                        // Apply security policy
                        if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                            std::string identifier = device.serialNumber.empty() ? 
                                                    devicePath : device.serialNumber;
                                                    
                            if (!isDeviceInWhitelist(identifier)) {
                                LOG_F(WARNING, "Non-whitelisted device blocked: %s", 
                                     devicePath.c_str());
                                knownDevices.insert(devicePath);
                                CFRelease(descRef);
                                return;
                            }
                        }
                        
                        // Get mount point (if available)
                        CFURLRef mountURLRef = (CFURLRef)CFDictionaryGetValue(
                            descRef, kDADiskDescriptionVolumePathKey);
                            
                        if (mountURLRef) {
                            char mountPath[PATH_MAX] = {0};
                            if (CFURLGetFileSystemRepresentation(
                                    mountURLRef, true, (UInt8*)mountPath, PATH_MAX)) {
                                    
                                // Apply security policies that need mount points
                                if (device.isRemovable) {
                                    if (securityPolicy == SecurityPolicy::READ_ONLY) {
                                        setDiskReadOnly(mountPath);
                                    } else if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                                        auto [success, threats] = scanDiskForThreats(mountPath, 2);
                                        if (threats > 0) {
                                            LOG_F(WARNING, "Threats detected on %s - setting read-only", 
                                                 mountPath);
                                            setDiskReadOnly(mountPath);
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Call the callback
                        callback(device);
                        knownDevices.insert(devicePath);
                    }
                }
                
                CFRelease(descRef);
            }
        };
        
        // Register for disk appeared notifications
        DARegisterDiskAppearedCallback(
            session,
            NULL,
            diskAppearedCallback,
            NULL);
            
        // Register for disk disappeared notifications (to remove from known devices)
        DARegisterDiskDisappearedCallback(
            session,
            NULL,
            ^(DADiskRef disk) {
                CFDictionaryRef descRef = DADiskCopyDescription(disk);
                if (descRef) {
                    CFStringRef bsdNameRef = (CFStringRef)CFDictionaryGetValue(
                        descRef, kDADiskDescriptionMediaBSDNameKey);
                        
                    if (bsdNameRef) {
                        char bsdName[256] = {0};
                        CFStringGetCString(
                            bsdNameRef, bsdName, sizeof(bsdName), kCFStringEncodingUTF8);
                            
                        std::string devicePath = "/dev/";
                        devicePath += bsdName;
                        
                        knownDevices.erase(devicePath);
                        LOG_F(INFO, "Device removed: %s", devicePath.c_str());
                    }
                    
                    CFRelease(descRef);
                }
            },
            NULL);
            
        // Run until cancelled
        while (g_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Cleanup
        DASessionSetDispatchQueue(session, NULL);
        CFRelease(session);
        dispatch_release(queue);

#elif defined(__FreeBSD__)
        // FreeBSD-specific device monitoring (simplified)
        
        // Known devices set
        static std::unordered_set<std::string> knownDevices;
        
        // Poll for changes
        while (g_monitoringActive) {
            // Get current devices
            std::vector<StorageDevice> currentDevices = getStorageDevices(true);
            std::unordered_set<std::string> currentPaths;
            
            for (const auto& device : currentDevices) {
                currentPaths.insert(device.devicePath);
                
                // Check if this is a new device
                if (knownDevices.find(device.devicePath) == knownDevices.end()) {
                    LOG_F(INFO, "New device detected: %s (%s)", 
                         device.devicePath.c_str(), device.model.c_str());
                    
                    // Apply security policy
                    if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                        auto serialOpt = getDeviceSerialNumber(device.devicePath);
                        std::string identifier = serialOpt.value_or(device.devicePath);
                        
                        if (!isDeviceInWhitelist(identifier)) {
                            LOG_F(WARNING, "Non-whitelisted device blocked: %s", 
                                 device.devicePath.c_str());
                            continue;
                        }
                    }
                    
                    // For removable devices, check mount points
                    if (device.isRemovable) {
                        // Find mount point
                        std::string mountPoint;
                        std::array<char, 1024> buffer;
                        
                        FILE* pipe = popen("mount | grep " + device.devicePath, "r");
                        if (pipe) {
                            if (fgets(buffer.data(), buffer.size(), pipe)) {
                                std::string output = buffer.data();
                                std::istringstream iss(output);
                                std::string dev, on, mnt;
                                iss >> dev >> on >> mnt;
                                
                                if (!mnt.empty()) {
                                    mountPoint = mnt;
                                }
                            }
                            pclose(pipe);
                        }
                        
                        if (!mountPoint.empty()) {
                            if (securityPolicy == SecurityPolicy::READ_ONLY) {
                                LOG_F(INFO, "Setting %s as read-only", mountPoint.c_str());
                                setDiskReadOnly(mountPoint);
                            } else if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                                auto [success, threats] = scanDiskForThreats(mountPoint, 2);
                                if (threats > 0) {
                                    LOG_F(WARNING, "Threats detected on %s - setting read-only", 
                                         mountPoint.c_str());
                                    setDiskReadOnly(mountPoint);
                                }
                            }
                        }
                    }
                    
                    // Call the callback
                    callback(device);
                    
                    // Add to known devices
                    knownDevices.insert(device.devicePath);
                }
            }
            
            // Remove devices that are no longer present
            for (auto it = knownDevices.begin(); it != knownDevices.end();) {
                if (currentPaths.find(*it) == currentPaths.end()) {
                    LOG_F(INFO, "Device removed: %s", it->c_str());
                    it = knownDevices.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Sleep to avoid high CPU usage
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
#else
        LOG_F(WARNING, "Device monitoring not fully implemented for this platform");
        while (g_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
#endif

        LOG_F(INFO, "Device monitoring stopped");
    });
}

std::variant<int, std::string> getDiskHealth(const std::string& devicePath) {
#ifdef _WIN32
    // For Windows, use SMART commands via DeviceIoControl
    HANDLE hDevice = CreateFileA(
        devicePath.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        return std::string("Failed to open device: Error " +
                           std::to_string(GetLastError()));
    }

    // This implementation is simplified - a real implementation would use
    // SMART commands and analyze more health indicators

    // A complete implementation would use ATA_PASS_THROUGH or
    // STORAGE_PROPERTY_QUERY with IOCTL_ATA_PASS_THROUGH or
    // IOCTL_STORAGE_QUERY_PROPERTY

    // For this example, we return a dummy health value based on device
    // characteristics
    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    std::array<char, 1024> buffer = {};
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                         sizeof(query), buffer.data(), buffer.size(),
                         &bytesReturned, NULL)) {
        CloseHandle(hDevice);
        return std::string("Failed to query device properties: Error " +
                           std::to_string(GetLastError()));
    }

    CloseHandle(hDevice);

    // Return a dummy health percentage (in a real app, this would be calculated
    // from SMART data)
    return 85;

#elif __linux__
    // For Linux, we'd use smartctl or the ioctl interface to read SMART data

    // Using smartctl for simplicity
    std::string command =
        "smartctl -H " + devicePath + " | grep 'SMART overall-health'";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return std::string("Failed to execute smartctl command");
    }

    std::array<char, 256> buffer;
    std::string result;

    if (fgets(buffer.data(), buffer.size(), pipe)) {
        result = buffer.data();
    }

    pclose(pipe);

    if (result.find("PASSED") != std::string::npos) {
        // For passed status, get more detailed health metrics
        command =
            "smartctl -A " + devicePath + " | grep 'Remaining_Lifetime_Perc'";
        pipe = popen(command.c_str(), "r");

        if (pipe) {
            if (fgets(buffer.data(), buffer.size(), pipe)) {
                std::string output = buffer.data();
                std::istringstream iss(output);
                std::string id, attr, flag, val, worst, thresh, type, updated,
                    failed, raw;

                // Skip to the health value (usually 9th column)
                iss >> id >> attr >> flag >> val >> worst >> thresh >> type >>
                    updated >> failed;

                if (!val.empty()) {
                    try {
                        return std::stoi(val);
                    } catch (...) {
                        // Fall back to a good health score
                        return 100;
                    }
                }
            }
            pclose(pipe);
        }

        // If we couldn't get a specific percentage, return a good score
        return 90;
    } else if (result.find("FAILED") != std::string::npos) {
        return 10;  // Critical health level
    } else {
        return std::string("Health status could not be determined");
    }

#elif __APPLE__
    // For macOS, we can use smartctl or IOKit

    // Using smartctl for simplicity
    std::string command =
        "smartctl -H " + devicePath + " 2>&1 | grep 'SMART overall-health'";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return std::string("Failed to execute smartctl command");
    }

    std::array<char, 256> buffer;
    std::string result;

    if (fgets(buffer.data(), buffer.size(), pipe)) {
        result = buffer.data();
    }

    pclose(pipe);

    if (result.find("PASSED") != std::string::npos) {
        return 90;  // Good health
    } else if (result.find("FAILED") != std::string::npos) {
        return 10;  // Critical health
    } else {
        // Try IOKit
        io_service_t service = IOServiceGetMatchingService(
            kIOMasterPortDefault,
            IOBSDNameMatching(kIOMasterPortDefault, 0, devicePath.c_str()));

        if (service) {
            // In a real app, we'd traverse to the parent
            // IOMedia/IOBlockStorageDriver and extract SMART attributes through
            // IOKit

            // For this example, return a dummy health value
            IOObjectRelease(service);
            return 85;
        }

        return std::string("Health status could not be determined");
    }

#elif defined(__FreeBSD__)
    // For FreeBSD, use smartctl
    std::string command =
        "smartctl -H " + devicePath + " | grep 'SMART overall-health'";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return std::string("Failed to execute smartctl command");
    }

    std::array<char, 256> buffer;
    std::string result;

    if (fgets(buffer.data(), buffer.size(), pipe)) {
        result = buffer.data();
    }

    pclose(pipe);

    if (result.find("PASSED") != std::string::npos) {
        // Try to get more detailed info
        command =
            "smartctl -A " + devicePath + " | grep 'Remaining_Lifetime_Perc'";
        pipe = popen(command.c_str(), "r");

        if (pipe) {
            if (fgets(buffer.data(), buffer.size(), pipe)) {
                std::string output = buffer.data();
                std::istringstream iss(output);
                std::string id, attr, flag, val, worst, thresh, type, updated,
                    failed, raw;

                // Skip to the health value (usually 9th column)
                iss >> id >> attr >> flag >> val >> worst >> thresh >> type >>
                    updated >> failed;

                if (!val.empty()) {
                    try {
                        return std::stoi(val);
                    } catch (...) {
                        // Fall back to a good health score
                        return 90;
                    }
                }
            }
            pclose(pipe);
        }

        return 90;  // Good health
    } else if (result.find("FAILED") != std::string::npos) {
        return 10;  // Critical health
    } else {
        return std::string("Health status could not be determined");
    }

#else
    return std::string(
        "Disk health checking not implemented for this platform");
#endif
}

}  // namespace atom::system