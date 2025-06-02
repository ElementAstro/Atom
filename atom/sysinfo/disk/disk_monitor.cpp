/*
 * disk_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Monitoring

**************************************************/

#include "atom/sysinfo/disk/disk_monitor.hpp"
#include "atom/sysinfo/disk/disk_security.hpp"

#include <atomic>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dbt.h>
// clang-format on
#elif __linux__
#include <libudev.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

static std::atomic_bool g_monitoringActive{false};

struct MonitorContext {
    SecurityPolicy securityPolicy;
    std::function<void(const StorageDevice&)> callback;
};

#ifdef _WIN32
static LRESULT CALLBACK DeviceMonitorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MonitorContext* context = nullptr;
    if (msg == WM_CREATE) {
        const CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        context = reinterpret_cast<MonitorContext*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
    } else {
        context = reinterpret_cast<MonitorContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (msg == WM_DEVICECHANGE && context && wParam == DBT_DEVICEARRIVAL) {
        const DEV_BROADCAST_HDR* header = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
        if (header->dbch_devicetype == DBT_DEVTYP_VOLUME) {
            const DEV_BROADCAST_VOLUME* vol = reinterpret_cast<DEV_BROADCAST_VOLUME*>(lParam);

            char driveLetter = 'A';
            DWORD mask = vol->dbcv_unitmask;
            while (!(mask & 1)) {
                mask >>= 1;
                driveLetter++;
            }

            const std::string drivePath = std::string(1, driveLetter) + ":\\";

            StorageDevice device;
            device.devicePath = drivePath;
            device.isRemovable = (GetDriveTypeA(drivePath.c_str()) == DRIVE_REMOVABLE);

            try {
                if (context->securityPolicy == SecurityPolicy::READ_ONLY) {
                    setDiskReadOnly(drivePath);
                } else if (context->securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                    scanDiskForThreats(drivePath);
                }

                context->callback(device);
            } catch (const std::exception& e) {
                spdlog::error("Error processing device insertion for {}: {}", drivePath, e.what());
            }
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
#endif

std::future<void> startDeviceMonitoring(std::function<void(const StorageDevice&)> callback,
                                       SecurityPolicy securityPolicy) {
    g_monitoringActive = true;

    return std::async(std::launch::async, [callback = std::move(callback), securityPolicy]() {
        spdlog::info("Starting device monitoring with security policy: {}", static_cast<int>(securityPolicy));

#ifdef _WIN32
        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = DeviceMonitorWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "DeviceMonitorClass";

        if (!RegisterClassExA(&wc)) {
            spdlog::error("Failed to register window class: {}", GetLastError());
            return;
        }

        MonitorContext context{securityPolicy, callback};
        const HWND hwnd = CreateWindowExA(0, "DeviceMonitorClass", "DeviceMonitor", 0,
                                         0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                         GetModuleHandleA(nullptr), &context);
        if (!hwnd) {
            spdlog::error("Failed to create hidden window: {}", GetLastError());
            UnregisterClassA("DeviceMonitorClass", GetModuleHandleA(nullptr));
            return;
        }

        DEV_BROADCAST_DEVICEINTERFACE notificationFilter{};
        notificationFilter.dbcc_size = sizeof(notificationFilter);
        notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

        const HDEVNOTIFY hDevNotify = RegisterDeviceNotificationA(hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

        if (!hDevNotify) {
            spdlog::error("Failed to register for device notifications: {}", GetLastError());
            DestroyWindow(hwnd);
            UnregisterClassA("DeviceMonitorClass", GetModuleHandleA(nullptr));
            return;
        }

        MSG msg;
        while (g_monitoringActive && GetMessageA(&msg, hwnd, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        UnregisterDeviceNotification(hDevNotify);
        DestroyWindow(hwnd);
        UnregisterClassA("DeviceMonitorClass", GetModuleHandleA(nullptr));

#elif __linux__
        struct udev* udev = udev_new();
        if (!udev) {
            spdlog::error("Failed to create udev object");
            return;
        }

        struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", nullptr);
        udev_monitor_enable_receiving(monitor);

        const int fd = udev_monitor_get_fd(monitor);
        std::unordered_set<std::string> existingDevices;

        struct udev_enumerate* enumerate = udev_enumerate_new(udev);
        udev_enumerate_add_match_subsystem(enumerate, "block");
        udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
        udev_enumerate_scan_devices(enumerate);

        struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* entry;

        udev_list_entry_foreach(entry, devices) {
            const char* path = udev_list_entry_get_name(entry);
            struct udev_device* dev = udev_device_new_from_syspath(udev, path);

            if (dev) {
                const char* devnode = udev_device_get_devnode(dev);
                if (devnode) {
                    existingDevices.insert(devnode);
                }
                udev_device_unref(dev);
            }
        }
        udev_enumerate_unref(enumerate);

        fd_set readfds;
        while (g_monitoringActive) {
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            struct timeval tv{1, 0};
            const int ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);

            if (ret > 0 && FD_ISSET(fd, &readfds)) {
                struct udev_device* dev = udev_monitor_receive_device(monitor);
                if (dev) {
                    const char* action = udev_device_get_action(dev);
                    const char* devnode = udev_device_get_devnode(dev);
                    const char* devtype = udev_device_get_devtype(dev);

                    if (action && devnode && devtype && 
                        strcmp(action, "add") == 0 && 
                        strcmp(devtype, "disk") == 0 &&
                        existingDevices.find(devnode) == existingDevices.end()) {
                        
                        existingDevices.insert(devnode);

                        StorageDevice device;
                        device.devicePath = devnode;

                        struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
                        if (parent) {
                            const char* vendor = udev_device_get_property_value(parent, "ID_VENDOR");
                            const char* model = udev_device_get_property_value(parent, "ID_MODEL");

                            if (vendor && model) {
                                device.model = std::string(vendor) + " " + model;
                            } else if (model) {
                                device.model = model;
                            } else {
                                device.model = devnode;
                            }

                            if (const char* serial = udev_device_get_property_value(parent, "ID_SERIAL")) {
                                device.serialNumber = serial;
                            }

                            if (const char* removable = udev_device_get_sysattr_value(parent, "removable")) {
                                device.isRemovable = (strcmp(removable, "1") == 0);
                            }

                            if (const char* size = udev_device_get_sysattr_value(parent, "size")) {
                                try {
                                    device.sizeBytes = std::stoull(size) * 512;
                                } catch (const std::exception& e) {
                                    spdlog::warn("Failed to parse device size for {}: {}", devnode, e.what());
                                }
                            }
                        }

                        try {
                            if (securityPolicy == SecurityPolicy::READ_ONLY) {
                                setDiskReadOnly(devnode);
                            } else if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                                if (!device.serialNumber.empty() && !isDeviceInWhitelist(device.serialNumber)) {
                                    spdlog::info("Device {} not in whitelist, skipping", devnode);
                                    udev_device_unref(dev);
                                    continue;
                                }
                            } else if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                                std::this_thread::sleep_for(std::chrono::seconds(2));

                                std::string mountPoint;
                                std::ifstream mtab("/proc/mounts");
                                std::string line;
                                while (std::getline(mtab, line)) {
                                    if (line.find(devnode) != std::string::npos) {
                                        std::istringstream iss(line);
                                        std::string device, mount;
                                        iss >> device >> mount;
                                        if (device == devnode) {
                                            mountPoint = mount;
                                            break;
                                        }
                                    }
                                }

                                if (!mountPoint.empty()) {
                                    scanDiskForThreats(mountPoint);
                                }
                            }

                            callback(device);
                        } catch (const std::exception& e) {
                            spdlog::error("Error processing device insertion for {}: {}", devnode, e.what());
                        }
                    }
                    udev_device_unref(dev);
                }
            }
        }

        udev_monitor_unref(monitor);
        udev_unref(udev);

#elif __APPLE__
        const DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (!session) {
            spdlog::error("Failed to create DiskArbitration session");
            return;
        }

        DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

        struct CallbackContext {
            std::function<void(const StorageDevice&)> callback;
            SecurityPolicy securityPolicy;
        };

        auto contextPtr = std::make_unique<CallbackContext>(CallbackContext{callback, securityPolicy});

        DARegisterDiskAppearedCallback(
            session, nullptr,
            [](DADiskRef disk, void* context) {
                const auto* ctx = static_cast<CallbackContext*>(context);

                const CFDictionaryRef description = DADiskCopyDescription(disk);
                if (!description) return;

                const CFStringRef bsdNameRef = static_cast<CFStringRef>(
                    CFDictionaryGetValue(description, kDADiskDescriptionMediaBSDNameKey));
                if (!bsdNameRef) {
                    CFRelease(description);
                    return;
                }

                char bsdName[128];
                if (!CFStringGetCString(bsdNameRef, bsdName, sizeof(bsdName), kCFStringEncodingUTF8)) {
                    CFRelease(description);
                    return;
                }

                const CFBooleanRef wholeMediaRef = static_cast<CFBooleanRef>(
                    CFDictionaryGetValue(description, kDADiskDescriptionMediaWholeKey));
                if (!wholeMediaRef || !CFBooleanGetValue(wholeMediaRef)) {
                    CFRelease(description);
                    return;
                }

                StorageDevice device;
                device.devicePath = "/dev/" + std::string(bsdName);

                if (const CFStringRef modelRef = static_cast<CFStringRef>(
                    CFDictionaryGetValue(description, kDADiskDescriptionDeviceModelKey))) {
                    char model[256];
                    if (CFStringGetCString(modelRef, model, sizeof(model), kCFStringEncodingUTF8)) {
                        device.model = model;
                    }
                }

                if (const CFNumberRef sizeRef = static_cast<CFNumberRef>(
                    CFDictionaryGetValue(description, kDADiskDescriptionMediaSizeKey))) {
                    CFNumberGetValue(sizeRef, kCFNumberSInt64Type, &device.sizeBytes);
                }

                if (const CFBooleanRef removableRef = static_cast<CFBooleanRef>(
                    CFDictionaryGetValue(description, kDADiskDescriptionMediaRemovableKey))) {
                    device.isRemovable = CFBooleanGetValue(removableRef);
                }

                if (const CFStringRef serialRef = static_cast<CFStringRef>(
                    CFDictionaryGetValue(description, kIOPropertySerialNumberKey))) {
                    char serial[128];
                    if (CFStringGetCString(serialRef, serial, sizeof(serial), kCFStringEncodingUTF8)) {
                        device.serialNumber = serial;
                    }
                }

                CFRelease(description);

                try {
                    if (ctx->securityPolicy == SecurityPolicy::READ_ONLY) {
                        setDiskReadOnly(device.devicePath);
                    } else if (ctx->securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                        if (!device.serialNumber.empty() && !isDeviceInWhitelist(device.serialNumber)) {
                            return;
                        }
                    }

                    ctx->callback(device);
                } catch (const std::exception& e) {
                    spdlog::error("Error processing device insertion for {}: {}", device.devicePath, e.what());
                }
            },
            contextPtr.get());

        while (g_monitoringActive) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        }

        DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        CFRelease(session);

#else
        std::unordered_map<std::string, StorageDevice> knownDevices;

        auto currentDevices = getStorageDevices(true);
        for (const auto& device : currentDevices) {
            knownDevices[device.devicePath] = device;
        }

        while (g_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(2));

            currentDevices = getStorageDevices(true);
            for (const auto& device : currentDevices) {
                if (knownDevices.find(device.devicePath) == knownDevices.end()) {
                    knownDevices[device.devicePath] = device;

                    try {
                        if (securityPolicy == SecurityPolicy::READ_ONLY) {
                            setDiskReadOnly(device.devicePath);
                        } else if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                            if (!device.serialNumber.empty() && !isDeviceInWhitelist(device.serialNumber)) {
                                continue;
                            }
                        }

                        callback(device);
                    } catch (const std::exception& e) {
                        spdlog::error("Error processing device insertion for {}: {}", device.devicePath, e.what());
                    }
                }
            }
        }
#endif
        spdlog::info("Device monitoring stopped");
    });
}

}  // namespace atom::system
