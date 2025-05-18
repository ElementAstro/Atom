/*
 * disk_device.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Devices

**************************************************/

#include "atom/sysinfo/disk/disk_device.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <devguid.h>
#include <setupapi.h>
#include <winioctl.h>
#include <cfgmgr32.h>
// clang-format on
#ifdef _MSV_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif  // _MSV_VER
#elif __linux__
#include <blkid/blkid.h>
#include <dirent.h>
#include <libudev.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

namespace atom::system {

std::vector<StorageDevice> getStorageDevices(bool includeRemovable) {
    std::vector<StorageDevice> devices;

#ifdef _WIN32
    // For Windows, use SetupDi functions to enumerate storage devices
    HDEVINFO hDevInfo =
        SetupDiGetClassDevs(&GUID_DEVCLASS_DISKDRIVE, 0, 0, DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        LOG_F(ERROR, "Failed to get device information set: %lu",
              GetLastError());
        return devices;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        DWORD dataType = 0;
        char buffer[4096] = {0};
        DWORD bufferSize = sizeof(buffer);

        // Get device friendly name
        if (SetupDiGetDeviceRegistryPropertyA(
                hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
                (PBYTE)buffer, bufferSize, NULL)) {
            StorageDevice device;
            device.model = buffer;

            // Get device path
            char devicePath[256] = {0};
            if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo, &devInfoData, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME,
                    &dataType, (PBYTE)devicePath, sizeof(devicePath), NULL)) {
                device.devicePath = devicePath;
            }

            // Check if removable
            DWORD capabilities = 0;
            if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo, &devInfoData, SPDRP_CAPABILITIES, &dataType,
                    (PBYTE)&capabilities, sizeof(capabilities), NULL)) {
                device.isRemovable = (capabilities & CM_DEVCAP_REMOVABLE) != 0;
            }

            // Get device size (requires getting the physical drive handle)
            if (!device.devicePath.empty()) {
                std::string physicalDrivePath = "\\\\.\\" + device.devicePath;
                HANDLE hDrive =
                    CreateFileA(physicalDrivePath.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, 0, NULL);
                if (hDrive != INVALID_HANDLE_VALUE) {
                    GET_LENGTH_INFORMATION lengthInfo;
                    DWORD bytesReturned = 0;
                    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO,
                                        NULL, 0, &lengthInfo,
                                        sizeof(lengthInfo), &bytesReturned,
                                        NULL)) {
                        device.sizeBytes = lengthInfo.Length.QuadPart;
                    }
                    CloseHandle(hDrive);
                }
            }

            // Get serial number
            auto serialOpt = getDeviceSerialNumber(device.devicePath);
            if (serialOpt) {
                device.serialNumber = *serialOpt;
            }

            // Add to list if it matches the removable filter
            if (includeRemovable || !device.isRemovable) {
                devices.push_back(device);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

#elif __linux__
    // For Linux, use libudev to enumerate block devices
    struct udev* udev = udev_new();
    if (!udev) {
        LOG_F(ERROR, "Failed to create udev context");
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
                // Check if this is a partition or whole disk
                const char* devtype = udev_device_get_devtype(dev);
                if (devtype && strcmp(devtype, "partition") != 0) {
                    StorageDevice device;
                    device.devicePath = devnode;

                    // Get device model and vendor
                    struct udev_device* parent =
                        udev_device_get_parent_with_subsystem_devtype(
                            dev, "block", "disk");
                    if (parent) {
                        const char* vendor =
                            udev_device_get_property_value(parent, "ID_VENDOR");
                        const char* model =
                            udev_device_get_property_value(parent, "ID_MODEL");

                        if (vendor && model) {
                            device.model = std::string(vendor) + " " + model;
                        } else if (model) {
                            device.model = model;
                        } else if (vendor) {
                            device.model = vendor;
                        } else {
                            // Fallback to device name
                            device.model = devnode;
                        }

                        // Get serial number
                        const char* serial =
                            udev_device_get_property_value(parent, "ID_SERIAL");
                        if (serial) {
                            device.serialNumber = serial;
                        }

                        // Check if device is removable
                        const char* removable =
                            udev_device_get_sysattr_value(parent, "removable");
                        if (removable) {
                            device.isRemovable = (strcmp(removable, "1") == 0);
                        }

                        // Get device size
                        const char* size =
                            udev_device_get_sysattr_value(parent, "size");
                        if (size) {
                            // Size is in 512-byte sectors
                            device.sizeBytes = std::stoull(size) * 512;
                        }
                    } else {
                        // Handle case where there's no parent device
                        device.model = devnode;

                        // Try to get removable attribute directly
                        const char* removable =
                            udev_device_get_sysattr_value(dev, "removable");
                        if (removable) {
                            device.isRemovable = (strcmp(removable, "1") == 0);
                        }

                        // Try to get size attribute directly
                        const char* size =
                            udev_device_get_sysattr_value(dev, "size");
                        if (size) {
                            // Size is in 512-byte sectors
                            device.sizeBytes = std::stoull(size) * 512;
                        }
                    }

                    // Add to list if it matches the removable filter
                    if (includeRemovable || !device.isRemovable) {
                        devices.push_back(device);
                    }
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
        // Add matching property for whole disks (not partitions)
        CFDictionarySetValue(matchingDict, CFSTR(kIOMediaWholeKey),
                             kCFBooleanTrue);

        io_iterator_t iter;
        if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                         &iter) == KERN_SUCCESS) {
            io_service_t service;
            while ((service = IOIteratorNext(iter)) != 0) {
                CFMutableDictionaryRef properties = NULL;
                if (IORegistryEntryCreateCFProperties(service, &properties,
                                                      kCFAllocatorDefault,
                                                      0) == KERN_SUCCESS) {
                    StorageDevice device;

                    // Get BSD name (e.g., disk0)
                    CFStringRef bsdNameRef = (CFStringRef)CFDictionaryGetValue(
                        properties, CFSTR(kIOBSDNameKey));
                    if (bsdNameRef) {
                        char bsdName[128];
                        if (CFStringGetCString(bsdNameRef, bsdName,
                                               sizeof(bsdName),
                                               kCFStringEncodingUTF8)) {
                            device.devicePath = "/dev/";
                            device.devicePath += bsdName;
                        }
                    }

                    // Get device size
                    CFNumberRef sizeRef = (CFNumberRef)CFDictionaryGetValue(
                        properties, CFSTR(kIOMediaSizeKey));
                    if (sizeRef) {
                        CFNumberGetValue(sizeRef, kCFNumberSInt64Type,
                                         &device.sizeBytes);
                    }

                    // Check if removable
                    CFBooleanRef removableRef =
                        (CFBooleanRef)CFDictionaryGetValue(
                            properties, CFSTR(kIOMediaRemovableKey));
                    if (removableRef) {
                        device.isRemovable = CFBooleanGetValue(removableRef);
                    }

                    // Get parent for model information
                    io_registry_entry_t parent;
                    kern_return_t kr = IORegistryEntryGetParentEntry(
                        service, kIOServicePlane, &parent);
                    if (kr == KERN_SUCCESS) {
                        CFMutableDictionaryRef parentProps = NULL;
                        if (IORegistryEntryCreateCFProperties(
                                parent, &parentProps, kCFAllocatorDefault, 0) ==
                            KERN_SUCCESS) {
                            // Get model
                            CFStringRef modelRef =
                                (CFStringRef)CFDictionaryGetValue(
                                    parentProps,
                                    CFSTR(kIOPropertyProductNameKey));
                            if (modelRef) {
                                char model[256];
                                if (CFStringGetCString(modelRef, model,
                                                       sizeof(model),
                                                       kCFStringEncodingUTF8)) {
                                    device.model = model;
                                }
                            }

                            // Get serial number
                            CFStringRef serialRef =
                                (CFStringRef)CFDictionaryGetValue(
                                    parentProps,
                                    CFSTR(kIOPropertySerialNumberKey));
                            if (serialRef) {
                                char serial[128];
                                if (CFStringGetCString(serialRef, serial,
                                                       sizeof(serial),
                                                       kCFStringEncodingUTF8)) {
                                    device.serialNumber = serial;
                                }
                            }

                            CFRelease(parentProps);
                        }
                        IOObjectRelease(parent);
                    }

                    CFRelease(properties);

                    // If we don't have a model yet, try using the device path
                    if (device.model.empty() && !device.devicePath.empty()) {
                        device.model = device.devicePath;
                    }

                    // Add to list if it matches the removable filter
                    if (!device.devicePath.empty() &&
                        (includeRemovable || !device.isRemovable)) {
                        devices.push_back(device);
                    }
                }
                IOObjectRelease(service);
            }
            IOObjectRelease(iter);
        }
    }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
// For BSD, get disks from sysctl or geom
#ifdef __FreeBSD__
    // For FreeBSD we would use libgeom or sysctl to get disk information
    // This is a simplified implementation
    DIR* dir = opendir("/dev");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // Look for disk devices (ada, da, etc.)
            if (strncmp(entry->d_name, "ada", 3) == 0 ||
                strncmp(entry->d_name, "da", 2) == 0 ||
                strncmp(entry->d_name, "cd", 2) == 0) {
                // Check if this is a whole disk, not a partition (no digits at
                // end)
                bool isPartition = false;
                for (const char* p = &entry->d_name[2]; *p; ++p) {
                    if (isdigit(*p)) {
                        isPartition = true;
                        break;
                    }
                }

                if (!isPartition) {
                    StorageDevice device;
                    device.devicePath = std::string("/dev/") + entry->d_name;

                    // Simple check for removable media
                    device.isRemovable =
                        (strncmp(entry->d_name, "da", 2) == 0 ||
                         strncmp(entry->d_name, "cd", 2) == 0);

                    // Get disk size
                    int fd = open(device.devicePath.c_str(), O_RDONLY);
                    if (fd >= 0) {
                        off_t size;
                        if (ioctl(fd, DIOCGMEDIASIZE, &size) == 0) {
                            device.sizeBytes = size;
                        }
                        close(fd);
                    }

                    // Set model to device name (for a real implementation, we
                    // would use CAMGET)
                    device.model = entry->d_name;

                    // Add to list if it matches the removable filter
                    if (includeRemovable || !device.isRemovable) {
                        devices.push_back(device);
                    }
                }
            }
        }
        closedir(dir);
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
        if (drivesBitMask & (1 << (i - 'A'))) {
            std::string drivePath = std::string(1, i) + ":";

            // Check if drive is removable
            UINT driveType = GetDriveTypeA((drivePath + "\\").c_str());

            // Add drive if it matches the removable filter
            if (includeRemovable || (driveType != DRIVE_REMOVABLE)) {
                drives.push_back(drivePath);
            }
        }
    }
#elif __linux__
    // Get mounted filesystems from /proc/mounts
    std::ifstream mountsFile("/proc/mounts");
    std::string line;

    while (std::getline(mountsFile, line)) {
        std::istringstream iss(line);
        std::string device, mountPoint, fsType;
        iss >> device >> mountPoint >> fsType;

        // Skip pseudo filesystems and check if drive exists
        if (fsType != "proc" && fsType != "sysfs" && fsType != "devtmpfs" &&
            fsType != "devpts" && fsType != "tmpfs" && fsType != "cgroup" &&
            std::filesystem::exists(mountPoint)) {
            // Check if device is removable
            bool isRemovable = false;
            std::string deviceName = device;
            if (deviceName.find("/dev/") == 0) {
                deviceName = deviceName.substr(5);  // Remove "/dev/"
                // Remove partition number if any
                for (int i = deviceName.length() - 1; i >= 0; --i) {
                    if (!isdigit(deviceName[i])) {
                        deviceName = deviceName.substr(0, i + 1);
                        break;
                    }
                }

                // Check if removable
                std::string removablePath =
                    "/sys/block/" + deviceName + "/removable";
                std::ifstream removableFile(removablePath);
                std::string value;
                if (removableFile.is_open() &&
                    std::getline(removableFile, value)) {
                    isRemovable = (value == "1");
                }
            }

            // Add to list if it matches the removable filter
            if (includeRemovable || !isRemovable) {
                drives.push_back(mountPoint);
            }
        }
    }
#elif __APPLE__
    struct statfs* mounts;
    int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

    for (int i = 0; i < numMounts; ++i) {
        // Check if filesystem is a local volume
        if (!(mounts[i].f_flags & MNT_LOCAL) &&
            !(mounts[i].f_flags & MNT_DONTBROWSE)) {
            continue;
        }

        // Get device name
        std::string devicePath = mounts[i].f_mntfromname;

        // Check if removable
        bool isRemovable = false;
        if (!devicePath.empty()) {
            DASessionRef session = DASessionCreate(kCFAllocatorDefault);
            if (session) {
                // Extract disk identifier (e.g., 'disk1' from '/dev/disk1')
                std::string diskName = devicePath;
                size_t lastSlash = diskName.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    diskName = diskName.substr(lastSlash + 1);
                }

                DADiskRef disk = DADiskCreateFromBSDName(
                    kCFAllocatorDefault, session, diskName.c_str());
                if (disk) {
                    CFDictionaryRef diskDesc = DADiskCopyDescription(disk);
                    if (diskDesc) {
                        // Check if ejectable/removable
                        CFBooleanRef ejectable =
                            (CFBooleanRef)CFDictionaryGetValue(
                                diskDesc, kDADiskDescriptionMediaEjectableKey);
                        CFBooleanRef removable =
                            (CFBooleanRef)CFDictionaryGetValue(
                                diskDesc, kDADiskDescriptionMediaRemovableKey);

                        isRemovable =
                            (ejectable && CFBooleanGetValue(ejectable)) ||
                            (removable && CFBooleanGetValue(removable));

                        CFRelease(diskDesc);
                    }
                    CFRelease(disk);
                }
                CFRelease(session);
            }
        }

        // Add mount point to the list if it matches the removable filter
        if (includeRemovable || !isRemovable) {
            drives.push_back(mounts[i].f_mntonname);
        }
    }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs* mounts;
    int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

    for (int i = 0; i < numMounts; ++i) {
        // Get device name
        std::string deviceName = mounts[i].f_mntfromname;

        // Simple check for removable media
        bool isRemovable = (deviceName.find("/dev/da") == 0 ||
                            deviceName.find("/dev/cd") == 0 ||
                            deviceName.find("/dev/md") == 0);

        // Add to list if it matches the removable filter
        if (includeRemovable || !isRemovable) {
            drives.push_back(mounts[i].f_mntonname);
        }
    }
#endif

    return drives;
}

std::optional<std::string> getDeviceSerialNumber(
    const std::string& devicePath) {
#ifdef _WIN32
    // For Windows, get the serial number from setupapi
    HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                 OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        LOG_F(WARNING, "Failed to open device %s: %lu", devicePath.c_str(),
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
        CloseHandle(hDevice);
        LOG_F(WARNING, "Failed to get storage descriptor size: %lu",
              GetLastError());
        return std::nullopt;
    }

    // Allocate buffer for the full descriptor
    std::vector<char> buffer(header.Size);

    if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                         sizeof(query), buffer.data(), buffer.size(),
                         &bytesReturned, NULL)) {
        CloseHandle(hDevice);
        LOG_F(WARNING, "Failed to get storage descriptor: %lu", GetLastError());
        return std::nullopt;
    }

    CloseHandle(hDevice);

    // Extract serial number
    auto desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

    if (desc->SerialNumberOffset == 0) {
        LOG_F(INFO, "No serial number available for device %s",
              devicePath.c_str());
        return std::nullopt;
    }

    std::string serialNumber = buffer.data() + desc->SerialNumberOffset;

    // Trim whitespace
    serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") + 1);
    serialNumber.erase(0, serialNumber.find_first_not_of(" \t\n\r\f\v"));

    if (serialNumber.empty()) {
        LOG_F(INFO, "Empty serial number for device %s", devicePath.c_str());
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
            struct udev_device* parent =
                udev_device_get_parent_with_subsystem_devtype(dev, "block",
                                                              "disk");
            if (parent) {
                const char* serial =
                    udev_device_get_property_value(parent, "ID_SERIAL");
                if (serial) {
                    serialNumber = serial;
                    udev_device_unref(dev);
                    udev_unref(udev);
                    return serialNumber;
                }
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
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            serialNumber = buffer;
            // Trim whitespace
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
        // matchingDict is consumed by IOServiceGetMatchingService
    }

    if (!service) {
        LOG_F(WARNING, "Failed to get IOService for disk %s", diskName.c_str());
        return std::nullopt;
    }

    // Need to get the parent device that has the serial number
    io_service_t parentService = 0;
    kern_return_t kr =
        IORegistryEntryGetParentEntry(service, kIOServicePlane, &parentService);

    IOObjectRelease(service);

    if (kr != KERN_SUCCESS || !parentService) {
        LOG_F(WARNING, "Failed to get parent service for disk %s",
              diskName.c_str());
        return std::nullopt;
    }

    // Get the serial number property
    CFTypeRef serialRef = IORegistryEntryCreateCFProperty(
        parentService, CFSTR(kIOPropertySerialNumberKey), kCFAllocatorDefault,
        0);

    IOObjectRelease(parentService);

    if (!serialRef) {
        LOG_F(INFO, "No serial number property for disk %s", diskName.c_str());
        return std::nullopt;
    }

    // Convert CFString to C++ string
    std::string serialNumber;
    if (CFGetTypeID(serialRef) == CFStringGetTypeID()) {
        CFStringRef serialStringRef = (CFStringRef)serialRef;
        char buffer[256];
        if (CFStringGetCString(serialStringRef, buffer, sizeof(buffer),
                               kCFStringEncodingUTF8)) {
            serialNumber = buffer;
        }
    }

    CFRelease(serialRef);

    if (serialNumber.empty()) {
        return std::nullopt;
    }

    return serialNumber;

#elif defined(__FreeBSD__)
    // For FreeBSD, we would use CAMGET to get device serial number
    // This is a simplified placeholder implementation
    LOG_F(INFO, "Serial number retrieval not fully implemented for FreeBSD");
    return std::nullopt;
#else
    LOG_F(INFO, "Serial number retrieval not implemented for this platform");
    return std::nullopt;
#endif
}

std::variant<int, std::string> getDiskHealth(const std::string& devicePath) {
#ifdef _WIN32
    // For Windows, we would use SMART data through DeviceIoControl
    // This requires elevated privileges and is a placeholder implementation
    LOG_F(INFO, "Disk health check not fully implemented for Windows");
    return "Not implemented for Windows yet";

#elif __linux__
    // For Linux, we would use smartctl or direct ATA commands
    // This is a simplified placeholder implementation
    std::string command = "smartctl -H " + devicePath + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");

    if (!pipe) {
        return "Failed to execute SMART health check";
    }

    char buffer[1024];
    std::string result;

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }

    pclose(pipe);

    if (result.find("PASSED") != std::string::npos) {
        return 100;  // Healthy
    } else if (result.find("FAILED") != std::string::npos) {
        return 0;  // Failed
    } else {
        return "Unable to determine disk health";
    }

#elif __APPLE__
    // For macOS, we would use IOKit to access SMART data
    // This is a placeholder implementation
    LOG_F(INFO, "Disk health check not fully implemented for macOS");
    return "Not implemented for macOS yet";

#elif defined(__FreeBSD__)
    // For FreeBSD, we would use CAMGET to access SMART data
    // This is a placeholder implementation
    LOG_F(INFO, "Disk health check not fully implemented for FreeBSD");
    return "Not implemented for FreeBSD yet";

#else
    LOG_F(INFO, "Disk health check not implemented for this platform");
    return "Not implemented for this platform";
#endif
}

}  // namespace atom::system
