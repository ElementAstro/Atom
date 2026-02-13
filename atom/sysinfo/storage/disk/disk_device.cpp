/*
 * disk_device.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Devices

**************************************************/

#include "disk_device.hpp"
<<<<<<<<HEAD : atom / sysinfo / src / disk / components / disk_device.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
        == == == ==>>>>>>>> test -
    fixes / systematic -
    testing : atom / sysinfo / storage / disk /
              disk_device.cpp

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

#include <spdlog/spdlog.h>

              namespace atom::system {

    std::vector<StorageDevice> getStorageDevices(bool includeRemovable) {
        std::vector<StorageDevice> devices;
        devices.reserve(16);  // Reserve space for typical number of devices

#ifdef _WIN32
        const HDEVINFO hDevInfo = SetupDiGetClassDevs(
            &GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);

        if (hDevInfo == INVALID_HANDLE_VALUE) {
            spdlog::error("Failed to get device information set: {}",
                          GetLastError());
            return devices;
        }

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData);
             i++) {
            DWORD dataType = 0;
            char buffer[4096] = {0};
            constexpr DWORD bufferSize = sizeof(buffer);

            if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
                    reinterpret_cast<PBYTE>(buffer), bufferSize, nullptr)) {
                StorageDevice device;
                device.model = buffer;

                char devicePath[256] = {0};
                if (SetupDiGetDeviceRegistryPropertyA(
                        hDevInfo, &devInfoData,
                        SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, &dataType,
                        reinterpret_cast<PBYTE>(devicePath), sizeof(devicePath),
                        nullptr)) {
                    device.devicePath = devicePath;
                }

                DWORD capabilities = 0;
                if (SetupDiGetDeviceRegistryPropertyA(
                        hDevInfo, &devInfoData, SPDRP_CAPABILITIES, &dataType,
                        reinterpret_cast<PBYTE>(&capabilities),
                        sizeof(capabilities), nullptr)) {
                    device.isRemovable =
                        (capabilities & CM_DEVCAP_REMOVABLE) != 0;
                }

                if (!device.devicePath.empty()) {
                    const std::string physicalDrivePath =
                        "\\\\.\\" + device.devicePath;
                    const HANDLE hDrive =
                        CreateFileA(physicalDrivePath.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
                    if (hDrive != INVALID_HANDLE_VALUE) {
                        GET_LENGTH_INFORMATION lengthInfo;
                        DWORD bytesReturned = 0;
                        if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO,
                                            nullptr, 0, &lengthInfo,
                                            sizeof(lengthInfo), &bytesReturned,
                                            nullptr)) {
                            device.sizeBytes = lengthInfo.Length.QuadPart;
                        }
                        CloseHandle(hDrive);
                    }
                }

                if (const auto serialOpt =
                        getDeviceSerialNumber(device.devicePath)) {
                    device.serialNumber = *serialOpt;
                }

                if (includeRemovable || !device.isRemovable) {
                    devices.push_back(std::move(device));
                }
            }
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);

#elif __linux__
        struct udev* udev = udev_new();
        if (!udev) {
            spdlog::error("Failed to create udev context");
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
                    const char* devtype = udev_device_get_devtype(dev);
                    if (devtype && strcmp(devtype, "partition") != 0) {
                        StorageDevice device;
                        device.devicePath = devnode;

                        struct udev_device* parent =
                            udev_device_get_parent_with_subsystem_devtype(
                                dev, "block", "disk");
                        if (parent) {
                            const char* vendor = udev_device_get_property_value(
                                parent, "ID_VENDOR");
                            const char* model = udev_device_get_property_value(
                                parent, "ID_MODEL");

                            if (vendor && model) {
                                device.model =
                                    std::string(vendor) + " " + model;
                            } else if (model) {
                                device.model = model;
                            } else if (vendor) {
                                device.model = vendor;
                            } else {
                                device.model = devnode;
                            }

                            if (const char* serial =
                                    udev_device_get_property_value(
                                        parent, "ID_SERIAL")) {
                                device.serialNumber = serial;
                            }

                            if (const char* removable =
                                    udev_device_get_sysattr_value(
                                        parent, "removable")) {
                                device.isRemovable =
                                    (strcmp(removable, "1") == 0);
                            }

                            if (const char* size =
                                    udev_device_get_sysattr_value(parent,
                                                                  "size")) {
                                device.sizeBytes = std::stoull(size) * 512;
                            }
                        } else {
                            device.model = devnode;

                            if (const char* removable =
                                    udev_device_get_sysattr_value(
                                        dev, "removable")) {
                                device.isRemovable =
                                    (strcmp(removable, "1") == 0);
                            }

                            if (const char* size =
                                    udev_device_get_sysattr_value(dev,
                                                                  "size")) {
                                device.sizeBytes = std::stoull(size) * 512;
                            }
                        }

                        if (includeRemovable || !device.isRemovable) {
                            devices.push_back(std::move(device));
                        }
                    }
                }
                udev_device_unref(dev);
            }
        }

        udev_enumerate_unref(enumerate);
        udev_unref(udev);

#elif __APPLE__
        CFMutableDictionaryRef matchingDict = IOServiceMatching("IOMedia");
        if (matchingDict) {
            CFDictionarySetValue(matchingDict, CFSTR(kIOMediaWholeKey),
                                 kCFBooleanTrue);

            io_iterator_t iter;
            if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                             &iter) == KERN_SUCCESS) {
                io_service_t service;
                while ((service = IOIteratorNext(iter)) != 0) {
                    CFMutableDictionaryRef properties = nullptr;
                    if (IORegistryEntryCreateCFProperties(service, &properties,
                                                          kCFAllocatorDefault,
                                                          0) == KERN_SUCCESS) {
                        StorageDevice device;

                        if (const CFStringRef bsdNameRef =
                                static_cast<CFStringRef>(CFDictionaryGetValue(
                                    properties, CFSTR(kIOBSDNameKey)))) {
                            char bsdName[128];
                            if (CFStringGetCString(bsdNameRef, bsdName,
                                                   sizeof(bsdName),
                                                   kCFStringEncodingUTF8)) {
                                device.devicePath =
                                    "/dev/" + std::string(bsdName);
                            }
                        }

                        if (const CFNumberRef sizeRef =
                                static_cast<CFNumberRef>(CFDictionaryGetValue(
                                    properties, CFSTR(kIOMediaSizeKey)))) {
                            CFNumberGetValue(sizeRef, kCFNumberSInt64Type,
                                             &device.sizeBytes);
                        }

                        if (const CFBooleanRef removableRef =
                                static_cast<CFBooleanRef>(CFDictionaryGetValue(
                                    properties, CFSTR(kIOMediaRemovableKey)))) {
                            device.isRemovable =
                                CFBooleanGetValue(removableRef);
                        }

                        io_registry_entry_t parent;
                        if (IORegistryEntryGetParentEntry(
                                service, kIOServicePlane, &parent) ==
                            KERN_SUCCESS) {
                            CFMutableDictionaryRef parentProps = nullptr;
                            if (IORegistryEntryCreateCFProperties(
                                    parent, &parentProps, kCFAllocatorDefault,
                                    0) == KERN_SUCCESS) {
                                if (const CFStringRef modelRef = static_cast<
                                        CFStringRef>(CFDictionaryGetValue(
                                        parentProps,
                                        CFSTR(kIOPropertyProductNameKey)))) {
                                    char model[256];
                                    if (CFStringGetCString(
                                            modelRef, model, sizeof(model),
                                            kCFStringEncodingUTF8)) {
                                        device.model = model;
                                    }
                                }

                                if (const CFStringRef serialRef = static_cast<
                                        CFStringRef>(CFDictionaryGetValue(
                                        parentProps,
                                        CFSTR(kIOPropertySerialNumberKey)))) {
                                    char serial[128];
                                    if (CFStringGetCString(
                                            serialRef, serial, sizeof(serial),
                                            kCFStringEncodingUTF8)) {
                                        device.serialNumber = serial;
                                    }
                                }

                                CFRelease(parentProps);
                            }
                            IOObjectRelease(parent);
                        }

                        CFRelease(properties);

                        if (device.model.empty() &&
                            !device.devicePath.empty()) {
                            device.model = device.devicePath;
                        }

                        if (!device.devicePath.empty() &&
                            (includeRemovable || !device.isRemovable)) {
                            devices.push_back(std::move(device));
                        }
                    }
                    IOObjectRelease(service);
                }
                IOObjectRelease(iter);
            }
        }

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#ifdef __FreeBSD__
        DIR* dir = opendir("/dev");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strncmp(entry->d_name, "ada", 3) == 0 ||
                    strncmp(entry->d_name, "da", 2) == 0 ||
                    strncmp(entry->d_name, "cd", 2) == 0) {
                    bool isPartition = false;
                    for (const char* p = &entry->d_name[2]; *p; ++p) {
                        if (isdigit(*p)) {
                            isPartition = true;
                            break;
                        }
                    }

                    if (!isPartition) {
                        StorageDevice device;
                        device.devicePath =
                            std::string("/dev/") + entry->d_name;
                        device.isRemovable =
                            (strncmp(entry->d_name, "da", 2) == 0 ||
                             strncmp(entry->d_name, "cd", 2) == 0);

                        const int fd =
                            open(device.devicePath.c_str(), O_RDONLY);
                        if (fd >= 0) {
                            off_t size;
                            if (ioctl(fd, DIOCGMEDIASIZE, &size) == 0) {
                                device.sizeBytes = size;
                            }
                            close(fd);
                        }

                        device.model = entry->d_name;

                        if (includeRemovable || !device.isRemovable) {
                            devices.push_back(std::move(device));
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
        const auto devices = getStorageDevices(true);

        result.reserve(devices.size());
        for (const auto& device : devices) {
            result.emplace_back(device.devicePath, device.model);
        }

        return result;
    }

    std::vector<std::string> getAvailableDrives(bool includeRemovable) {
        std::vector<std::string> drives;
        drives.reserve(26);  // Reserve space for typical number of drives

#ifdef _WIN32
        const DWORD drivesBitMask = GetLogicalDrives();
        for (char i = 'A'; i <= 'Z'; ++i) {
            if (drivesBitMask & (1 << (i - 'A'))) {
                const std::string drivePath = std::string(1, i) + ":";
                const UINT driveType =
                    GetDriveTypeA((drivePath + "\\").c_str());

                if (includeRemovable || (driveType != DRIVE_REMOVABLE)) {
                    drives.push_back(drivePath);
                }
            }
        }
#elif __linux__
        std::ifstream mountsFile("/proc/mounts");
        std::string line;

        while (std::getline(mountsFile, line)) {
            std::istringstream iss(line);
            std::string device, mountPoint, fsType;
            iss >> device >> mountPoint >> fsType;

            static const std::unordered_set<std::string> excludedTypes{
                "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup"};

            if (excludedTypes.find(fsType) == excludedTypes.end() &&
                std::filesystem::exists(mountPoint)) {
                bool isRemovable = false;
                std::string deviceName = device;
                if (deviceName.find("/dev/") == 0) {
                    deviceName = deviceName.substr(5);
                    for (int i = deviceName.length() - 1; i >= 0; --i) {
                        if (!isdigit(deviceName[i])) {
                            deviceName = deviceName.substr(0, i + 1);
                            break;
                        }
                    }

                    const std::string removablePath =
                        "/sys/block/" + deviceName + "/removable";
                    std::ifstream removableFile(removablePath);
                    std::string value;
                    if (removableFile.is_open() &&
                        std::getline(removableFile, value)) {
                        isRemovable = (value == "1");
                    }
                }

                if (includeRemovable || !isRemovable) {
                    drives.push_back(mountPoint);
                }
            }
        }
#elif __APPLE__
        struct statfs* mounts;
        const int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

        for (int i = 0; i < numMounts; ++i) {
            if (!(mounts[i].f_flags & MNT_LOCAL) &&
                !(mounts[i].f_flags & MNT_DONTBROWSE)) {
                continue;
            }

            const std::string devicePath = mounts[i].f_mntfromname;
            bool isRemovable = false;

            if (!devicePath.empty()) {
                const DASessionRef session =
                    DASessionCreate(kCFAllocatorDefault);
                if (session) {
                    std::string diskName = devicePath;
                    const size_t lastSlash = diskName.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        diskName = diskName.substr(lastSlash + 1);
                    }

                    const DADiskRef disk = DADiskCreateFromBSDName(
                        kCFAllocatorDefault, session, diskName.c_str());
                    if (disk) {
                        const CFDictionaryRef diskDesc =
                            DADiskCopyDescription(disk);
                        if (diskDesc) {
                            const CFBooleanRef ejectable =
                                static_cast<CFBooleanRef>(CFDictionaryGetValue(
                                    diskDesc,
                                    kDADiskDescriptionMediaEjectableKey));
                            const CFBooleanRef removable =
                                static_cast<CFBooleanRef>(CFDictionaryGetValue(
                                    diskDesc,
                                    kDADiskDescriptionMediaRemovableKey));

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

            if (includeRemovable || !isRemovable) {
                drives.push_back(mounts[i].f_mntonname);
            }
        }
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        struct statfs* mounts;
        const int numMounts = getmntinfo(&mounts, MNT_NOWAIT);

        for (int i = 0; i < numMounts; ++i) {
            const std::string deviceName = mounts[i].f_mntfromname;
            const bool isRemovable = (deviceName.find("/dev/da") == 0 ||
                                      deviceName.find("/dev/cd") == 0 ||
                                      deviceName.find("/dev/md") == 0);

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
        const HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           nullptr, OPEN_EXISTING, 0, nullptr);

        if (hDevice == INVALID_HANDLE_VALUE) {
            spdlog::warn("Failed to open device {}: {}", devicePath,
                         GetLastError());
            return std::nullopt;
        }

        STORAGE_PROPERTY_QUERY query;
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_DESCRIPTOR_HEADER header = {};
        header.Size = 0;
        DWORD bytesReturned = 0;

        if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                             sizeof(query), &header, sizeof(header),
                             &bytesReturned, nullptr)) {
            CloseHandle(hDevice);
            spdlog::warn("Failed to get storage descriptor size: {}",
                         GetLastError());
            return std::nullopt;
        }

        std::vector<char> buffer(header.Size);

        if (!DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                             sizeof(query), buffer.data(), buffer.size(),
                             &bytesReturned, nullptr)) {
            CloseHandle(hDevice);
            spdlog::warn("Failed to get storage descriptor: {}",
                         GetLastError());
            return std::nullopt;
        }

        CloseHandle(hDevice);

        const auto desc =
            reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());

        if (desc->SerialNumberOffset == 0) {
            spdlog::info("No serial number available for device {}",
                         devicePath);
            return std::nullopt;
        }

        std::string serialNumber = buffer.data() + desc->SerialNumberOffset;
        serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") + 1);
        serialNumber.erase(0, serialNumber.find_first_not_of(" \t\n\r\f\v"));

        if (serialNumber.empty()) {
            spdlog::info("Empty serial number for device {}", devicePath);
            return std::nullopt;
        }

        return serialNumber;

#elif __linux__
        std::string deviceName = devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        const std::string serialPath =
            "/sys/block/" + deviceName + "/device/serial";
        std::ifstream serialFile(serialPath);
        std::string serialNumber;

        if (serialFile.is_open() && std::getline(serialFile, serialNumber)) {
            serialNumber.erase(serialNumber.find_last_not_of(" \t\n\r\f\v") +
                               1);
            serialNumber.erase(0,
                               serialNumber.find_first_not_of(" \t\n\r\f\v"));

            if (!serialNumber.empty()) {
                return serialNumber;
            }
        }

        struct udev* udev = udev_new();
        if (udev) {
            struct udev_device* dev = udev_device_new_from_subsystem_sysname(
                udev, "block", deviceName.c_str());
            if (dev) {
                struct udev_device* parent =
                    udev_device_get_parent_with_subsystem_devtype(dev, "block",
                                                                  "disk");
                if (parent) {
                    if (const char* serial = udev_device_get_property_value(
                            parent, "ID_SERIAL")) {
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

        const std::string command =
            "lsblk -no SERIAL /dev/" + deviceName + " 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                serialNumber = buffer;
                serialNumber.erase(
                    serialNumber.find_last_not_of(" \t\n\r\f\v") + 1);
                serialNumber.erase(
                    0, serialNumber.find_first_not_of(" \t\n\r\f\v"));

                if (!serialNumber.empty()) {
                    pclose(pipe);
                    return serialNumber;
                }
            }
            pclose(pipe);
        }

        spdlog::info("Could not find serial number for device {}", devicePath);
        return std::nullopt;

#elif __APPLE__
        std::string diskName = devicePath;
        const size_t lastSlash = diskName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            diskName = diskName.substr(lastSlash + 1);
        }

        const CFMutableDictionaryRef matchingDict =
            IOBSDNameMatching(kIOMasterPortDefault, 0, diskName.c_str());

        io_service_t service = 0;
        if (matchingDict) {
            service =
                IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
        }

        if (!service) {
            spdlog::warn("Failed to get IOService for disk {}", diskName);
            return std::nullopt;
        }

        io_service_t parentService = 0;
        const kern_return_t kr = IORegistryEntryGetParentEntry(
            service, kIOServicePlane, &parentService);

        IOObjectRelease(service);

        if (kr != KERN_SUCCESS || !parentService) {
            spdlog::warn("Failed to get parent service for disk {}", diskName);
            return std::nullopt;
        }

        const CFTypeRef serialRef = IORegistryEntryCreateCFProperty(
            parentService, CFSTR(kIOPropertySerialNumberKey),
            kCFAllocatorDefault, 0);

        IOObjectRelease(parentService);

        if (!serialRef) {
            spdlog::info("No serial number property for disk {}", diskName);
            return std::nullopt;
        }

        std::string serialNumber;
        if (CFGetTypeID(serialRef) == CFStringGetTypeID()) {
            const CFStringRef serialStringRef =
                static_cast<CFStringRef>(serialRef);
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
        spdlog::info(
            "Serial number retrieval not fully implemented for FreeBSD");
        return std::nullopt;
#else
        spdlog::info(
            "Serial number retrieval not implemented for this platform");
        return std::nullopt;
#endif
    }

    std::variant<int, std::string> getDiskHealth(
        const std::string& /*devicePath*/) {
#ifdef _WIN32
        spdlog::info("Disk health check not fully implemented for Windows");
        return "Not implemented for Windows yet";

#elif __linux__
        const std::string command =
            "smartctl -H " + devicePath + " 2>/dev/null";
        FILE* pipe = popen(command.c_str(), "r");

        if (!pipe) {
            return "Failed to execute SMART health check";
        }

        char buffer[1024];
        std::string result;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        pclose(pipe);

        if (result.find("PASSED") != std::string::npos) {
            return 100;
        } else if (result.find("FAILED") != std::string::npos) {
            return 0;
        } else {
            return "Unable to determine disk health";
        }

#elif __APPLE__
        spdlog::info("Disk health check not fully implemented for macOS");
        return "Not implemented for macOS yet";

#elif defined(__FreeBSD__)
        spdlog::info("Disk health check not fully implemented for FreeBSD");
        return "Not implemented for FreeBSD yet";

#else
        spdlog::info("Disk health check not implemented for this platform");
        return "Not implemented for this platform";
#endif
    }

    std::future<std::vector<StorageDevice>> getStorageDevicesAsync(
        bool includeRemovable) {
        return std::async(std::launch::async, [includeRemovable]() {
            return getStorageDevices(includeRemovable);
        });
    }

    std::vector<StorageDevice> getNVMeDevices() {
        std::vector<StorageDevice> nvmeDevices;

#ifdef __linux__
        // Look for NVMe devices in /dev/nvme*
        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            const std::string filename = entry.path().filename().string();
            if (filename.find("nvme") == 0 &&
                filename.find("n1") != std::string::npos) {
                StorageDevice device;
                device.devicePath = entry.path().string();
                device.interfaceType = DiskInterfaceType::NVME;
                device.diskType =
                    DiskType::SSD;  // NVMe devices are typically SSDs

                // Try to get more information
                const std::string modelPath =
                    "/sys/block/" + filename + "/device/model";
                std::ifstream modelFile(modelPath);
                if (modelFile.is_open()) {
                    std::getline(modelFile, device.model);
                }

                const std::string sizePath = "/sys/block/" + filename + "/size";
                std::ifstream sizeFile(sizePath);
                if (sizeFile.is_open()) {
                    uint64_t sectors;
                    if (sizeFile >> sectors) {
                        device.sizeBytes =
                            sectors * 512;  // Assuming 512-byte sectors
                    }
                }

                nvmeDevices.push_back(device);
            }
        }
#else
        spdlog::warn("NVMe device detection not implemented for this platform");
#endif

        return nvmeDevices;
    }

    DiskInterfaceType detectDeviceInterfaceType(const std::string& devicePath) {
#ifdef __linux__
        if (devicePath.find("nvme") != std::string::npos) {
            return DiskInterfaceType::NVME;
        } else if (devicePath.find("sd") != std::string::npos) {
            // Could be SATA or USB, need to check further
            std::string deviceName = devicePath;
            const size_t lastSlash = deviceName.find_last_of('/');
            if (lastSlash != std::string::npos) {
                deviceName = deviceName.substr(lastSlash + 1);
            }

            const std::string removablePath =
                "/sys/block/" + deviceName + "/removable";
            std::ifstream removableFile(removablePath);
            std::string value;
            if (removableFile.is_open() && std::getline(removableFile, value)) {
                if (value == "1") {
                    return DiskInterfaceType::USB;
                }
            }
            return DiskInterfaceType::SATA;
        } else if (devicePath.find("hd") != std::string::npos) {
            return DiskInterfaceType::IDE;
        }
#endif
        return DiskInterfaceType::UNKNOWN;
    }

    DiskType detectDeviceType(const std::string& devicePath) {
#ifdef __linux__
        std::string deviceName = devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        // Check if it's an SSD by looking at the rotational attribute
        const std::string rotationalPath =
            "/sys/block/" + deviceName + "/queue/rotational";
        std::ifstream rotationalFile(rotationalPath);
        std::string value;
        if (rotationalFile.is_open() && std::getline(rotationalFile, value)) {
            if (value == "0") {
                return DiskType::SSD;
            } else if (value == "1") {
                return DiskType::HDD;
            }
        }

        // Check if it's NVMe (which are typically SSDs)
        if (devicePath.find("nvme") != std::string::npos) {
            return DiskType::SSD;
        }
#endif
        return DiskType::UNKNOWN;
    }

    std::optional<float> getDeviceTemperature(const std::string& devicePath) {
#ifdef __linux__
        std::string deviceName = devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        // Try to find temperature in hwmon
        for (const auto& entry :
             std::filesystem::directory_iterator("/sys/class/hwmon")) {
            const std::string hwmonPath = entry.path().string();
            const std::string namePath = hwmonPath + "/name";

            std::ifstream nameFile(namePath);
            std::string hwmonName;
            if (nameFile.is_open() && std::getline(nameFile, hwmonName)) {
                if (hwmonName.find(deviceName) != std::string::npos ||
                    hwmonName.find("nvme") != std::string::npos) {
                    const std::string tempPath = hwmonPath + "/temp1_input";
                    std::ifstream tempFile(tempPath);
                    int tempMilliC;
                    if (tempFile.is_open() && (tempFile >> tempMilliC)) {
                        return static_cast<float>(tempMilliC) / 1000.0f;
                    }
                }
            }
        }
#endif
        return std::nullopt;
    }

    bool isDeviceHotpluggable(const std::string& devicePath) {
#ifdef __linux__
        std::string deviceName = devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        const std::string removablePath =
            "/sys/block/" + deviceName + "/removable";
        std::ifstream removableFile(removablePath);
        std::string value;
        if (removableFile.is_open() && std::getline(removableFile, value)) {
            return value == "1";
        }
#endif
        return false;
    }

    std::optional<StorageDevice> getEnhancedDeviceInfo(
        const std::string& devicePath) {
        StorageDevice device;
        device.devicePath = devicePath;

        // Get basic device information
        device.interfaceType = detectDeviceInterfaceType(devicePath);
        device.diskType = detectDeviceType(devicePath);
        device.isHotpluggable = isDeviceHotpluggable(devicePath);

        // Get temperature if available
        auto temp = getDeviceTemperature(devicePath);
        if (temp) {
            device.performance.temperature = *temp;
        }

#ifdef __linux__
        std::string deviceName = devicePath;
        const size_t lastSlash = deviceName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            deviceName = deviceName.substr(lastSlash + 1);
        }

        // Get model information
        const std::string modelPath =
            "/sys/block/" + deviceName + "/device/model";
        std::ifstream modelFile(modelPath);
        if (modelFile.is_open()) {
            std::getline(modelFile, device.model);
        }

        // Get serial number
        const std::string serialPath =
            "/sys/block/" + deviceName + "/device/serial";
        std::ifstream serialFile(serialPath);
        if (serialFile.is_open()) {
            std::getline(serialFile, device.serialNumber);
        }

        // Get size
        const std::string sizePath = "/sys/block/" + deviceName + "/size";
        std::ifstream sizeFile(sizePath);
        if (sizeFile.is_open()) {
            uint64_t sectors;
            if (sizeFile >> sectors) {
                device.sizeBytes = sectors * 512;  // Assuming 512-byte sectors
            }
        }

        // Get vendor
        const std::string vendorPath =
            "/sys/block/" + deviceName + "/device/vendor";
        std::ifstream vendorFile(vendorPath);
        if (vendorFile.is_open()) {
            std::getline(vendorFile, device.vendor);
        }

        return device;
#else
        spdlog::warn(
            "Enhanced device info not fully implemented for this platform");
        return std::nullopt;
#endif
    }

    std::vector<StorageDevice> getStorageDevicesFiltered(
        std::function<bool(const StorageDevice&)> filter, bool useCache) {
        static std::mutex cacheMutex;
        static std::vector<StorageDevice> cachedDevices;
        static std::chrono::steady_clock::time_point lastUpdate;
        static constexpr auto CACHE_DURATION = std::chrono::minutes(1);

        std::vector<StorageDevice> devices;

        if (useCache) {
            std::lock_guard<std::mutex> lock(cacheMutex);
            const auto now = std::chrono::steady_clock::now();
            if (!cachedDevices.empty() && (now - lastUpdate) < CACHE_DURATION) {
                devices = cachedDevices;
            } else {
                devices = getStorageDevices(true);
                cachedDevices = devices;
                lastUpdate = now;
            }
        } else {
            devices = getStorageDevices(true);
        }

        std::vector<StorageDevice> filteredDevices;
        std::copy_if(devices.begin(), devices.end(),
                     std::back_inserter(filteredDevices), filter);

        return filteredDevices;
    }

    std::future<void> startHotplugMonitoring(
        std::function<void(const StorageDevice&)> onDeviceAdded,
        std::function<void(const std::string&)> onDeviceRemoved) {
        return std::async(
            std::launch::async, [onDeviceAdded, onDeviceRemoved]() {
                std::unordered_set<std::string> knownDevices;

                // Initialize with current devices
                auto currentDevices = getStorageDevices(true);
                for (const auto& device : currentDevices) {
                    knownDevices.insert(device.devicePath);
                }

                while (true) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    auto newDevices = getStorageDevices(true);
                    std::unordered_set<std::string> newDevicePaths;

                    // Check for added devices
                    for (const auto& device : newDevices) {
                        newDevicePaths.insert(device.devicePath);
                        if (knownDevices.find(device.devicePath) ==
                            knownDevices.end()) {
                            onDeviceAdded(device);
                        }
                    }

                    // Check for removed devices
                    for (const auto& devicePath : knownDevices) {
                        if (newDevicePaths.find(devicePath) ==
                            newDevicePaths.end()) {
                            onDeviceRemoved(devicePath);
                        }
                    }

                    knownDevices = std::move(newDevicePaths);
                }
            });
    }

    // DeviceManager implementation
    class DeviceManager::Impl {
    public:
        mutable std::mutex cacheMutex;
        std::vector<StorageDevice> cachedDevices;
        std::chrono::steady_clock::time_point lastCacheUpdate;
        std::chrono::minutes cacheExpiration{std::chrono::minutes(2)};
        std::atomic<bool> monitoringActive{false};
        std::future<void> monitoringFuture;

        std::vector<StorageDevice> getStorageDevices(bool includeRemovable) {
            const auto now = std::chrono::steady_clock::now();

            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                if (!cachedDevices.empty() &&
                    (now - lastCacheUpdate) < cacheExpiration) {
                    std::vector<StorageDevice> result;
                    std::copy_if(
                        cachedDevices.begin(), cachedDevices.end(),
                        std::back_inserter(result),
                        [includeRemovable](const StorageDevice& device) {
                            return includeRemovable || !device.isRemovable;
                        });
                    return result;
                }
            }

            auto devices = atom::system::getStorageDevices(includeRemovable);

            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                cachedDevices = devices;
                lastCacheUpdate = now;
            }

            return devices;
        }

        void clearCache() {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cachedDevices.clear();
        }
    };

    DeviceManager::DeviceManager() : pImpl(std::make_unique<Impl>()) {}

    DeviceManager::~DeviceManager() { stopMonitoring(); }

    DeviceManager::DeviceManager(DeviceManager&&) noexcept = default;

    DeviceManager& DeviceManager::operator=(DeviceManager&&) noexcept = default;

    std::vector<StorageDevice> DeviceManager::getStorageDevices(
        bool includeRemovable) {
        return pImpl->getStorageDevices(includeRemovable);
    }

    void DeviceManager::startMonitoring(
        std::function<void(const StorageDevice&)> onDeviceAdded,
        std::function<void(const std::string&)> onDeviceRemoved) {
        if (pImpl->monitoringActive.load()) {
            return;  // Already monitoring
        }

        pImpl->monitoringActive = true;
        pImpl->monitoringFuture =
            startHotplugMonitoring(onDeviceAdded, onDeviceRemoved);
    }

    void DeviceManager::stopMonitoring() {
        pImpl->monitoringActive = false;
        // Note: In a real implementation, we'd need a way to signal the
        // monitoring thread to stop
    }

    void DeviceManager::clearCache() { pImpl->clearCache(); }

    std::optional<StorageDevice> DeviceManager::getDevice(
        const std::string& devicePath) {
        auto devices = getStorageDevices(true);
        auto it = std::find_if(devices.begin(), devices.end(),
                               [&devicePath](const StorageDevice& device) {
                                   return device.devicePath == devicePath;
                               });

        if (it != devices.end()) {
            return *it;
        }

        return std::nullopt;
    }

    bool DeviceManager::deviceExists(const std::string& devicePath) {
        return getDevice(devicePath).has_value();
    }

}  // namespace atom::system
