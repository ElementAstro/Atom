/*
 * storage.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-5

Description: Storage Monitor

**************************************************/

#ifndef ATOM_SYSTEM_STORAGE_HPP
#define ATOM_SYSTEM_STORAGE_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "atom/macro.hpp"

namespace atom::system {
/**
 * @brief Class for monitoring storage space changes.
 *
 * This class can monitor the storage space usage of all mounted devices and
 * trigger registered callback functions when storage space changes.
 */
class StorageMonitor {
public:
    /**
     * @brief Default constructor.
     */
    StorageMonitor();

    /**
     * @brief Destructor.
     */
    ~StorageMonitor();

    /**
     * @brief Copy constructor (deleted).
     */
    StorageMonitor(const StorageMonitor&) = delete;

    /**
     * @brief Copy assignment operator (deleted).
     */
    StorageMonitor& operator=(const StorageMonitor&) = delete;

    /**
     * @brief Move constructor (deleted for thread safety).
     */
    StorageMonitor(StorageMonitor&&) = delete;

    /**
     * @brief Move assignment operator (deleted for thread safety).
     */
    StorageMonitor& operator=(StorageMonitor&&) = delete;

    /**
     * @brief Registers a callback function.
     *
     * @param callback The callback function to be triggered when storage space
     * changes.
     */
    void registerCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Starts storage space monitoring.
     *
     * @return True if monitoring started successfully, otherwise false.
     */
    [[nodiscard]] auto startMonitoring() -> bool;

    /**
     * @brief Stops storage space monitoring.
     */
    void stopMonitoring();

    /**
     * @brief Checks if monitoring is running.
     *
     * @return True if monitoring is running, otherwise false.
     */
    [[nodiscard]] auto isRunning() const -> bool;

    /**
     * @brief Triggers all registered callback functions.
     *
     * @param path The storage space path.
     */
    void triggerCallbacks(const std::string& path);

    /**
     * @brief Checks if new storage media is inserted at the specified path.
     *
     * @param path The storage space path.
     * @return True if new storage media is inserted, otherwise false.
     */
    [[nodiscard]] auto isNewMediaInserted(const std::string& path) -> bool;

    /**
     * @brief Lists all mounted storage spaces.
     */
    void listAllStorage();

    /**
     * @brief Lists the files in the specified path.
     *
     * @param path The storage space path.
     */
    void listFiles(const std::string& path);

    /**
     * @brief Dynamically adds a storage path.
     *
     * @param path The storage path to add.
     */
    void addStoragePath(const std::string& path);

    /**
     * @brief Dynamically removes a storage path.
     *
     * @param path The storage path to remove.
     */
    void removeStoragePath(const std::string& path);

    /**
     * @brief Gets the current storage status.
     *
     * @return A string representation of the storage status.
     */
    [[nodiscard]] auto getStorageStatus() -> std::string;

    /**
     * @brief Gets the number of registered callbacks.
     *
     * @return The number of callbacks.
     */
    [[nodiscard]] auto getCallbackCount() const -> size_t;

    /**
     * @brief Clears all registered callbacks.
     */
    void clearCallbacks();

    /**
     * @brief Gets detailed information about a specific storage path.
     *
     * @param path The storage path to query.
     * @return A string containing detailed storage information.
     */
    [[nodiscard]] auto getStorageInfo(const std::string& path) -> std::string;

private:
    /**
     * @brief The main monitoring loop.
     */
    void monitorLoop();

    /**
     * @brief Updates storage statistics for a given path.
     *
     * @param path The storage path to update.
     */
    void updateStorageStats(const std::string& path);

    std::vector<std::string> storagePaths_;
    std::unordered_map<std::string, std::pair<uintmax_t, uintmax_t>>
        storageStats_;
    mutable std::mutex mutex_;
    std::vector<std::function<void(const std::string&)>> callbacks_;
    bool isRunning_;
    std::thread monitorThread_;
    std::condition_variable cv_;
};

#ifdef _WIN32
/**
 * @brief Monitor USB disk insertion and removal on Windows.
 */
void monitorUdisk();
#else
/**
 * @brief Monitor USB disk insertion and removal on Linux.
 *
 * @param monitor Reference to the storage monitor instance.
 */
void monitorUdisk(StorageMonitor& monitor);
#endif

}  // namespace atom::system

#endif
