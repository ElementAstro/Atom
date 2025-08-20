/*
 * storage.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-5

Description: Storage Monitor

**************************************************/

#include "storage.hpp"

#include <atomic>
#include <filesystem>
#include <format>
#include <sstream>
#include <thread>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dbt.h>
// clang-format on
#elif __linux__
#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace atom::system {

StorageMonitor::StorageMonitor() : isRunning_(false) {
    storagePaths_.reserve(16);
    callbacks_.reserve(8);
    storageStats_.reserve(16);
}

StorageMonitor::~StorageMonitor() {
    spdlog::info("StorageMonitor destructor called");
    stopMonitoring();
}

void StorageMonitor::registerCallback(
    std::function<void(const std::string&)> callback) {
    spdlog::info("Registering callback");
    std::lock_guard lock(mutex_);
    callbacks_.emplace_back(std::move(callback));
    spdlog::info("Callback registered successfully, total callbacks: {}",
                 callbacks_.size());
}

auto StorageMonitor::startMonitoring() -> bool {
    std::lock_guard lock(mutex_);
    if (isRunning_) {
        spdlog::warn("Monitoring already running");
        return false;
    }

    spdlog::info("Starting storage monitoring");
    isRunning_ = true;

    try {
        monitorThread_ = std::thread(&StorageMonitor::monitorLoop, this);
        spdlog::info("Storage monitoring started successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start monitoring thread: {}", e.what());
        isRunning_ = false;
        return false;
    }
}

void StorageMonitor::stopMonitoring() {
    {
        std::lock_guard lock(mutex_);
        if (!isRunning_) {
            return;
        }
        spdlog::info("Stopping storage monitoring");
        isRunning_ = false;
    }

    cv_.notify_all();
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    spdlog::info("Storage monitoring stopped");
}

auto StorageMonitor::isRunning() const -> bool {
    std::lock_guard lock(mutex_);
    return isRunning_;
}

void StorageMonitor::triggerCallbacks(const std::string& path) {
    spdlog::info("Triggering callbacks for path: {}", path);
    std::lock_guard lock(mutex_);

    for (const auto& callback : callbacks_) {
        try {
            callback(path);
        } catch (const std::exception& e) {
            spdlog::error("Callback exception for path {}: {}", path, e.what());
        }
    }

    spdlog::info("Callbacks triggered successfully for path: {}", path);
}

auto StorageMonitor::isNewMediaInserted(const std::string& path) -> bool {
    try {
        auto currentSpace = fs::space(path);
        std::lock_guard lock(mutex_);
        auto& [lastCapacity, lastFree] = storageStats_[path];

        if (currentSpace.capacity != lastCapacity ||
            currentSpace.free != lastFree) {
            lastCapacity = currentSpace.capacity;
            lastFree = currentSpace.free;
            spdlog::info("Storage changed at path: {} (capacity: {}, free: {})",
                         path, currentSpace.capacity, currentSpace.free);
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error checking storage space for {}: {}", path,
                      e.what());
    }

    return false;
}

void StorageMonitor::listAllStorage() {
    spdlog::info("Listing all storage devices");

    try {
        std::lock_guard lock(mutex_);
        storagePaths_.clear();
        storageStats_.clear();

#ifdef _WIN32
        for (char drive = 'A'; drive <= 'Z'; ++drive) {
            std::string drivePath = std::format("{}:\\", drive);
            UINT driveType = GetDriveTypeA(drivePath.c_str());

            if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE) {
                storagePaths_.emplace_back(drivePath);
                storageStats_[drivePath] = {0, 0};
                updateStorageStats(drivePath);
                spdlog::info("Found storage device: {} (type: {})", drivePath,
                             driveType == DRIVE_FIXED ? "Fixed" : "Removable");
            }
        }
#else
        const std::vector<std::string> mountPoints = {"/", "/home", "/media",
                                                      "/mnt"};

        for (const auto& mountPoint : mountPoints) {
            if (fs::exists(mountPoint) && fs::is_directory(mountPoint)) {
                storagePaths_.emplace_back(mountPoint);
                storageStats_[mountPoint] = {0, 0};
                updateStorageStats(mountPoint);
                spdlog::info("Found storage device: {}", mountPoint);
            }
        }

        if (fs::exists("/media")) {
            for (const auto& entry : fs::directory_iterator("/media")) {
                if (entry.is_directory()) {
                    auto path = entry.path().string();
                    storagePaths_.emplace_back(path);
                    storageStats_[path] = {0, 0};
                    updateStorageStats(path);
                    spdlog::info("Found removable storage device: {}", path);
                }
            }
        }
#endif

        spdlog::info("Storage listing completed with {} devices found",
                     storagePaths_.size());
    } catch (const std::exception& e) {
        spdlog::error("Error listing storage: {}", e.what());
    }
}

void StorageMonitor::listFiles(const std::string& path) {
    spdlog::info("Listing files in path: {}", path);

    try {
        size_t fileCount = 0;
        for (const auto& entry : fs::directory_iterator(path)) {
            spdlog::debug("- {}", entry.path().filename().string());
            ++fileCount;

            if (fileCount > 100) {
                spdlog::info("... and {} more files (truncated)",
                             std::distance(fs::directory_iterator(path),
                                           fs::directory_iterator{}));
                break;
            }
        }
        spdlog::info("Listed {} files in path: {}", fileCount, path);
    } catch (const std::exception& e) {
        spdlog::error("Error listing files in {}: {}", path, e.what());
    }
}

void StorageMonitor::addStoragePath(const std::string& path) {
    std::lock_guard lock(mutex_);

    auto it = std::find(storagePaths_.begin(), storagePaths_.end(), path);
    if (it == storagePaths_.end()) {
        storagePaths_.emplace_back(path);
        storageStats_[path] = {0, 0};
        updateStorageStats(path);
        spdlog::info("Added new storage path: {}", path);
    } else {
        spdlog::warn("Storage path already exists: {}", path);
    }
}

void StorageMonitor::removeStoragePath(const std::string& path) {
    std::lock_guard lock(mutex_);

    auto it = std::remove(storagePaths_.begin(), storagePaths_.end(), path);
    if (it != storagePaths_.end()) {
        storagePaths_.erase(it, storagePaths_.end());
        storageStats_.erase(path);
        spdlog::info("Removed storage path: {}", path);
    } else {
        spdlog::warn("Storage path not found: {}", path);
    }
}

auto StorageMonitor::getStorageStatus() -> std::string {
    std::lock_guard lock(mutex_);
    std::stringstream ss;
    ss << "Storage Status:\n";

    for (const auto& path : storagePaths_) {
        auto it = storageStats_.find(path);
        if (it != storageStats_.end()) {
            const auto [capacity, free] = it->second;
            const auto used = capacity - free;
            const double usagePercent =
                capacity > 0 ? (static_cast<double>(used) / capacity) * 100.0
                             : 0.0;

            ss << std::format(
                "{}: Capacity={:.2f}GB, Used={:.2f}GB, Free={:.2f}GB, "
                "Usage={:.1f}%\n",
                path, static_cast<double>(capacity) / (1024 * 1024 * 1024),
                static_cast<double>(used) / (1024 * 1024 * 1024),
                static_cast<double>(free) / (1024 * 1024 * 1024), usagePercent);
        }
    }

    return ss.str();
}

auto StorageMonitor::getCallbackCount() const -> size_t {
    std::lock_guard lock(mutex_);
    return callbacks_.size();
}

void StorageMonitor::clearCallbacks() {
    std::lock_guard lock(mutex_);
    callbacks_.clear();
    spdlog::info("All callbacks cleared");
}

auto StorageMonitor::getStorageInfo(const std::string& path) -> std::string {
    try {
        auto spaceInfo = fs::space(path);
        std::lock_guard lock(mutex_);

        const auto capacity = spaceInfo.capacity;
        const auto free = spaceInfo.free;
        const auto available = spaceInfo.available;
        const auto used = capacity - free;
        const double usagePercent =
            capacity > 0 ? (static_cast<double>(used) / capacity) * 100.0 : 0.0;

        return std::format(
            "Storage Info for {}:\n"
            "  Capacity: {:.2f} GB\n"
            "  Used: {:.2f} GB\n"
            "  Free: {:.2f} GB\n"
            "  Available: {:.2f} GB\n"
            "  Usage: {:.1f}%\n",
            path, static_cast<double>(capacity) / (1024 * 1024 * 1024),
            static_cast<double>(used) / (1024 * 1024 * 1024),
            static_cast<double>(free) / (1024 * 1024 * 1024),
            static_cast<double>(available) / (1024 * 1024 * 1024),
            usagePercent);
    } catch (const std::exception& e) {
        spdlog::error("Error getting storage info for {}: {}", path, e.what());
        return std::format("Error getting storage info for {}: {}", path,
                           e.what());
    }
}

void StorageMonitor::monitorLoop() {
    spdlog::info("Storage monitor loop started");

    try {
        listAllStorage();

        while (true) {
            {
                std::unique_lock lk(mutex_);
                if (!isRunning_) {
                    break;
                }
            }

            std::vector<std::string> pathsCopy;
            {
                std::lock_guard lock(mutex_);
                pathsCopy = storagePaths_;
            }

            for (const auto& path : pathsCopy) {
                if (isNewMediaInserted(path)) {
                    triggerCallbacks(path);
                }
            }

            std::unique_lock lk(mutex_);
            cv_.wait_for(lk, std::chrono::seconds(5),
                         [this]() { return !isRunning_; });

            if (!isRunning_) {
                break;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in storage monitor loop: {}", e.what());
        std::lock_guard lk(mutex_);
        isRunning_ = false;
    }

    spdlog::info("Storage monitor loop ended");
}

void StorageMonitor::updateStorageStats(const std::string& path) {
    try {
        auto spaceInfo = fs::space(path);
        storageStats_[path] = {spaceInfo.capacity, spaceInfo.free};
    } catch (const std::exception& e) {
        spdlog::error("Failed to update storage stats for {}: {}", path,
                      e.what());
        storageStats_[path] = {0, 0};
    }
}

#ifdef _WIN32
void monitorUdisk() {
    spdlog::info("Starting Windows USB disk monitoring");

    DEV_BROADCAST_DEVICEINTERFACE devInterface{};
    devInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(
        GetConsoleWindow(), &devInterface, DEVICE_NOTIFY_WINDOW_HANDLE);

    if (hDevNotify == nullptr) {
        spdlog::error("Failed to register device notification: {}",
                      GetLastError());
        return;
    }

    spdlog::info("Device notification registered successfully");

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_DEVICECHANGE) {
            auto* hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(msg.lParam);
            if ((hdr != nullptr) && hdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                auto* volume = reinterpret_cast<PDEV_BROADCAST_VOLUME>(hdr);

                if (msg.wParam == DBT_DEVICEARRIVAL) {
                    DWORD unitmask = volume->dbcv_unitmask;
                    for (char driveLetter = 'A'; unitmask != 0U;
                         unitmask >>= 1, ++driveLetter) {
                        if ((unitmask & 1) != 0U) {
                            std::string drivePath =
                                std::format("{}:\\", driveLetter);
                            spdlog::info("USB disk inserted at drive: {}",
                                         drivePath);
                        }
                    }
                } else if (msg.wParam == DBT_DEVICEREMOVECOMPLETE) {
                    DWORD unitmask = volume->dbcv_unitmask;
                    for (char driveLetter = 'A'; unitmask != 0U;
                         unitmask >>= 1, ++driveLetter) {
                        if ((unitmask & 1) != 0U) {
                            std::string drivePath =
                                std::format("{}:\\", driveLetter);
                            spdlog::info("USB disk removed from drive: {}",
                                         drivePath);
                        }
                    }
                }
            }
        }
    }

    UnregisterDeviceNotification(hDevNotify);
    spdlog::info("Windows USB disk monitoring completed");
}
#else
void monitorUdisk(StorageMonitor& monitor) {
    spdlog::info("Starting Linux USB disk monitoring");

    struct udev* udev = udev_new();
    if (!udev) {
        spdlog::error("Failed to initialize udev");
        return;
    }

    struct udev_monitor* udevMon = udev_monitor_new_from_netlink(udev, "udev");
    if (!udevMon) {
        udev_unref(udev);
        spdlog::error("Failed to create udev monitor");
        return;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(udevMon, "block",
                                                        "disk") < 0) {
        spdlog::error("Failed to add udev filter");
        udev_monitor_unref(udevMon);
        udev_unref(udev);
        return;
    }

    if (udev_monitor_enable_receiving(udevMon) < 0) {
        spdlog::error("Failed to enable udev receiving");
        udev_monitor_unref(udevMon);
        udev_unref(udev);
        return;
    }

    int fd = udev_monitor_get_fd(udevMon);
    spdlog::info("USB disk monitoring started on fd: {}", fd);

    fd_set fds;
    struct timeval timeout;

    while (monitor.isRunning()) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(fd + 1, &fds, nullptr, nullptr, &timeout);
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            struct udev_device* dev = udev_monitor_receive_device(udevMon);
            if (dev) {
                const char* actionPtr = udev_device_get_action(dev);
                const char* devNodePtr = udev_device_get_devnode(dev);

                if (actionPtr && devNodePtr) {
                    std::string action(actionPtr);
                    std::string devNode(devNodePtr);

                    if (action == "add") {
                        spdlog::info("New USB disk detected: {}", devNode);
                        monitor.triggerCallbacks(devNode);
                    } else if (action == "remove") {
                        spdlog::info("USB disk removed: {}", devNode);
                    }
                }
                udev_device_unref(dev);
            }
        } else if (ret < 0) {
            spdlog::error("Error in select(): {}", strerror(errno));
            break;
        }
    }

    udev_monitor_unref(udevMon);
    udev_unref(udev);
    spdlog::info("Linux USB disk monitoring completed");
}
#endif

}  // namespace atom::system
