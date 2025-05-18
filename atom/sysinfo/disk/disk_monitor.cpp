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
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {

// Atomic flag for device monitoring
static std::atomic_bool g_monitoringActive = false;

struct MonitorContext {
    SecurityPolicy securityPolicy;
    std::function<void(const StorageDevice&)> callback;
};

static LRESULT CALLBACK DeviceMonitorWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam) {
    // 获取关联的上下文数据
    MonitorContext* context = nullptr;
    if (msg == WM_CREATE) {
        // 窗口创建时保存上下文
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        context = reinterpret_cast<MonitorContext*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(context));
    } else {
        // 获取之前保存的上下文
        context = reinterpret_cast<MonitorContext*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (msg == WM_DEVICECHANGE && context) {
        if (wParam == DBT_DEVICEARRIVAL) {
            // A device was inserted
            DEV_BROADCAST_HDR* header =
                reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
            if (header->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                DEV_BROADCAST_VOLUME* vol =
                    reinterpret_cast<DEV_BROADCAST_VOLUME*>(header);

                // Convert the volume mask to a drive letter
                char driveLetter = 'A';
                DWORD mask = vol->dbcv_unitmask;
                while (!(mask & 1)) {
                    mask >>= 1;
                    driveLetter++;
                }

                // Create the drive path
                std::string drivePath = std::string(1, driveLetter) + ":\\";

                // Create a device object
                StorageDevice device;
                device.devicePath = drivePath;
                device.isRemovable =
                    (GetDriveTypeA(drivePath.c_str()) == DRIVE_REMOVABLE);

                // Apply security policy
                if (context->securityPolicy == SecurityPolicy::READ_ONLY) {
                    setDiskReadOnly(drivePath);
                } else if (context->securityPolicy ==
                           SecurityPolicy::SCAN_BEFORE_USE) {
                    scanDiskForThreats(drivePath);
                }

                // Invoke callback
                context->callback(device);
            }
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Device monitoring implementation
std::future<void> startDeviceMonitoring(
    std::function<void(const StorageDevice&)> callback,
    SecurityPolicy securityPolicy) {
    // Create atomic flag to control monitoring thread
    g_monitoringActive = true;

    // Create the future that will run the monitoring
    return std::async(std::launch::async, [callback, securityPolicy]() {
#ifdef _WIN32
        // Windows implementation using WM_DEVICECHANGE
        // This requires a window handle, so typically is done in the main UI
        // thread Here we create a hidden window for monitoring

        // First, register a window class
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = DeviceMonitorWndProc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "DeviceMonitorClass";

        if (!RegisterClassExA(&wc)) {
            LOG_F(ERROR, "Failed to register window class");
            return;
        }

        // Create a hidden window
        HWND hwnd = CreateWindowExA(0, "DeviceMonitorClass", "DeviceMonitor", 0,
                                    0, 0, 0, 0, HWND_MESSAGE, NULL,
                                    GetModuleHandleA(NULL), NULL);
        if (!hwnd) {
            LOG_F(ERROR, "Failed to create hidden window");
            return;
        }

        // Register for device notifications
        DEV_BROADCAST_DEVICEINTERFACE notificationFilter = {};
        notificationFilter.dbcc_size = sizeof(notificationFilter);
        notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

        HDEVNOTIFY hDevNotify = RegisterDeviceNotificationA(
            hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

        if (!hDevNotify) {
            LOG_F(ERROR, "Failed to register for device notifications");
            DestroyWindow(hwnd);
            return;
        }

        // Message loop
        MSG msg;
        while (g_monitoringActive && GetMessageA(&msg, hwnd, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Cleanup
        UnregisterDeviceNotification(hDevNotify);
        DestroyWindow(hwnd);
        UnregisterClassA("DeviceMonitorClass", GetModuleHandleA(NULL));

#elif __linux__
        // Linux implementation using libudev for monitoring device insertion
        struct udev* udev = udev_new();
        if (!udev) {
            LOG_F(ERROR, "Failed to create udev object");
            return;
        }
        
        // Create monitor for block devices
        struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", NULL);
        udev_monitor_enable_receiving(monitor);
        
        // Get the file descriptor for polling
        int fd = udev_monitor_get_fd(monitor);
        
        // Setup polling
        fd_set readfds;
        
        // Track existing devices to detect new ones
        std::unordered_set<std::string> existingDevices;
        
        // Scan for existing devices first
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
        
        // Monitor for new devices
        while (g_monitoringActive) {
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            
            // Use select with a timeout to check flag periodically
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            
            int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
            
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
                        
                        // This is a new device
                        existingDevices.insert(devnode);
                        
                        // Create device object
                        StorageDevice device;
                        device.devicePath = devnode;
                        
                        // Get device properties
                        struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(
                            dev, "block", "disk");
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
                            
                            const char* serial = udev_device_get_property_value(parent, "ID_SERIAL");
                            if (serial) {
                                device.serialNumber = serial;
                            }
                            
                            const char* removable = udev_device_get_sysattr_value(parent, "removable");
                            if (removable) {
                                device.isRemovable = (strcmp(removable, "1") == 0);
                            }
                            
                            const char* size = udev_device_get_sysattr_value(parent, "size");
                            if (size) {
                                device.sizeBytes = std::stoull(size) * 512;
                            }
                        }
                        
                        // Apply security policy
                        if (securityPolicy == SecurityPolicy::READ_ONLY) {
                            setDiskReadOnly(devnode);
                        } else if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                            if (!device.serialNumber.empty() && 
                                !isDeviceInWhitelist(device.serialNumber)) {
                                continue;  // Skip callback if not in whitelist
                            }
                        } else if (securityPolicy == SecurityPolicy::SCAN_BEFORE_USE) {
                            // Wait for device to be mounted
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            
                            // Find mount point
                            std::string mountPoint;
                            FILE* mtab = fopen("/proc/mounts", "r");
                            if (mtab) {
                                char line[256];
                                while (fgets(line, sizeof(line), mtab) != NULL) {
                                    if (strstr(line, devnode) != NULL) {
                                        char* token = strtok(line, " \t");
                                        if (token) token = strtok(NULL, " \t");  // Skip device
                                        if (token) {
                                            mountPoint = token;
                                            break;
                                        }
                                    }
                                }
                                fclose(mtab);
                            }
                            
                            if (!mountPoint.empty()) {
                                scanDiskForThreats(mountPoint);
                            }
                        }
                        
                        // Invoke callback
                        callback(device);
                    }
                    udev_device_unref(dev);
                }
            }
        }
        
        udev_monitor_unref(monitor);
        udev_unref(udev);

#elif __APPLE__
        // macOS implementation using DiskArbitration framework
        DASessionRef session = DASessionCreate(kCFAllocatorDefault);
        if (!session) {
            LOG_F(ERROR, "Failed to create DiskArbitration session");
            return;
        }
        
        // Set up the run loop
        DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        
        // Track existing devices to detect new ones
        std::unordered_set<std::string> existingDevices;
        
        // Register callbacks for disk appearance
        DARegisterDiskAppearedCallback(
            session, NULL,
            [](DADiskRef disk, void* context) {
                auto callback = reinterpret_cast<std::function<void(const StorageDevice&)>*>(context);
                
                // Get disk information
                CFDictionaryRef description = DADiskCopyDescription(disk);
                if (!description) return;
                
                // Get device path
                CFStringRef bsdNameRef = (CFStringRef)CFDictionaryGetValue(
                    description, kDADiskDescriptionMediaBSDNameKey);
                if (!bsdNameRef) {
                    CFRelease(description);
                    return;
                }
                
                char bsdName[128];
                if (!CFStringGetCString(bsdNameRef, bsdName, sizeof(bsdName), kCFStringEncodingUTF8)) {
                    CFRelease(description);
                    return;
                }
                
                std::string devicePath = "/dev/";
                devicePath += bsdName;
                
                // Check if this is a whole disk (not a partition)
                CFBooleanRef wholeMediaRef = (CFBooleanRef)CFDictionaryGetValue(
                    description, kDADiskDescriptionMediaWholeKey);
                if (!wholeMediaRef || !CFBooleanGetValue(wholeMediaRef)) {
                    CFRelease(description);
                    return;
                }
                
                // Create device object
                StorageDevice device;
                device.devicePath = devicePath;
                
                // Get model
                CFStringRef modelRef = (CFStringRef)CFDictionaryGetValue(
                    description, kDADiskDescriptionDeviceModelKey);
                if (modelRef) {
                    char model[256];
                    if (CFStringGetCString(modelRef, model, sizeof(model), kCFStringEncodingUTF8)) {
                        device.model = model;
                    }
                }
                
                // Get size
                CFNumberRef sizeRef = (CFNumberRef)CFDictionaryGetValue(
                    description, kDADiskDescriptionMediaSizeKey);
                if (sizeRef) {
                    CFNumberGetValue(sizeRef, kCFNumberSInt64Type, &device.sizeBytes);
                }
                
                // Check if removable
                CFBooleanRef removableRef = (CFBooleanRef)CFDictionaryGetValue(
                    description, kDADiskDescriptionMediaRemovableKey);
                if (removableRef) {
                    device.isRemovable = CFBooleanGetValue(removableRef);
                }
                
                // Get serial number
                CFStringRef serialRef = (CFStringRef)CFDictionaryGetValue(
                    description, kIOPropertySerialNumberKey);
                if (serialRef) {
                    char serial[128];
                    if (CFStringGetCString(serialRef, serial, sizeof(serial), kCFStringEncodingUTF8)) {
                        device.serialNumber = serial;
                    }
                }
                
                CFRelease(description);
                
                // Apply security policy
                auto secPolicy = *reinterpret_cast<SecurityPolicy*>(context + sizeof(callback));
                if (secPolicy == SecurityPolicy::READ_ONLY) {
                    setDiskReadOnly(devicePath);
                } else if (secPolicy == SecurityPolicy::WHITELIST_ONLY) {
                    if (!device.serialNumber.empty() && 
                        !isDeviceInWhitelist(device.serialNumber)) {
                        return;  // Skip callback if not in whitelist
                    }
                }
                
                // Invoke callback
                (*callback)(device);
            },
            &callback);
        
        // Run loop
        while (g_monitoringActive) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
        }
        
        // Clean up
        DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        CFRelease(session);

#else
        // Generic fallback implementation using polling
        std::unordered_map<std::string, StorageDevice> knownDevices;
        
        // Initialize with currently available devices
        auto currentDevices = getStorageDevices(true);
        for (const auto& device : currentDevices) {
            knownDevices[device.devicePath] = device;
        }
        
        // Poll periodically for new devices
        while (g_monitoringActive) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            currentDevices = getStorageDevices(true);
            for (const auto& device : currentDevices) {
                if (knownDevices.find(device.devicePath) == knownDevices.end()) {
                    // This is a new device
                    knownDevices[device.devicePath] = device;
                    
                    // Apply security policy
                    if (securityPolicy == SecurityPolicy::READ_ONLY) {
                        setDiskReadOnly(device.devicePath);
                    } else if (securityPolicy == SecurityPolicy::WHITELIST_ONLY) {
                        if (!device.serialNumber.empty() && 
                            !isDeviceInWhitelist(device.serialNumber)) {
                            continue;  // Skip callback if not in whitelist
                        }
                    }
                    
                    // Invoke callback
                    callback(device);
                }
            }
        }
#endif
        LOG_F(INFO, "Device monitoring stopped");
    });
}

}  // namespace atom::system
