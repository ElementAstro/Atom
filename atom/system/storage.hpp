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
     * @brief Registers a callback function.
     *
     * @param callback The callback function to be triggered when storage space
     * changes.
     */
    void registerCallback(std::function<void(const std::string &)> callback);

    /**
     * @brief Starts storage space monitoring.
     *
     * @return True if monitoring started successfully, otherwise false.
     */
    ATOM_NODISCARD auto startMonitoring() -> bool;

    /**
     * @brief Stops storage space monitoring.
     */
    void stopMonitoring();

    /**
     * @brief Checks if monitoring is running.
     *
     * @return True if monitoring is running, otherwise false.
     */
    ATOM_NODISCARD auto isRunning() const -> bool;

    /**
     * @brief Triggers all registered callback functions.
     *
     * @param path The storage space path.
     */
    void triggerCallbacks(const std::string &path);

    /**
     * @brief Checks if new storage media is inserted at the specified path.
     *
     * @param path The storage space path.
     * @return True if new storage media is inserted, otherwise false.
     */
    ATOM_NODISCARD auto isNewMediaInserted(const std::string &path) -> bool;

    /**
     * @brief Lists all mounted storage spaces.
     */
    void listAllStorage();

    /**
     * @brief Lists the files in the specified path.
     *
     * @param path The storage space path.
     */
    void listFiles(const std::string &path);

    /**
     * @brief Dynamically adds a storage path.
     *
     * @param path The storage path to add.
     */
    void addStoragePath(const std::string &path);

    /**
     * @brief Dynamically removes a storage path.
     *
     * @param path The storage path to remove.
     */
    void removeStoragePath(const std::string &path);

    /**
     * @brief Gets the current storage status.
     *
     * @return A string representation of the storage status.
     */
    std::string getStorageStatus();

private:
    std::vector<std::string>
        m_storagePaths;  ///< All mounted storage space paths.
    std::unordered_map<std::string, std::pair<uintmax_t, uintmax_t>>
        m_storageStats;  ///< Storage statistics.
    std::mutex m_mutex;  ///< Mutex for thread-safe access to data structures.
    std::vector<std::function<void(const std::string &)>>
        m_callbacks;              ///< List of registered callback functions.
    bool m_isRunning;             ///< Flag indicating if monitoring is running.
    std::thread m_monitorThread;  ///< Monitoring thread.
    std::condition_variable
        m_cv;  ///< Condition variable for thread synchronization.
};

#ifdef _WIN32
static void monitorUdisk();
#else
void monitorUdisk(atom::system::StorageMonitor &monitor);
#endif

}  // namespace atom::system

#endif