#include "atom/system/storage.hpp"

#include <iostream>

using namespace atom::system;

void storageCallback(const std::string& path) {
    std::cout << "Storage space changed at: " << path << std::endl;
}

int main() {
    // Create a StorageMonitor object
    StorageMonitor monitor;

    // Register a callback function
    monitor.registerCallback(storageCallback);
    std::cout << "Registered storage callback" << std::endl;

    // Start storage space monitoring
    bool started = monitor.startMonitoring();
    std::cout << "Started monitoring: " << std::boolalpha << started
              << std::endl;

    // Check if monitoring is running
    bool running = monitor.isRunning();
    std::cout << "Is monitoring running: " << std::boolalpha << running
              << std::endl;

    // Trigger all registered callback functions
    monitor.triggerCallbacks("/path/to/storage");
    std::cout << "Triggered callbacks for /path/to/storage" << std::endl;

    // Check if new storage media is inserted
    bool newMediaInserted = monitor.isNewMediaInserted("/path/to/storage");
    std::cout << "Is new media inserted: " << std::boolalpha << newMediaInserted
              << std::endl;

    // List all mounted storage spaces
    monitor.listAllStorage();
    std::cout << "Listed all storage spaces" << std::endl;

    // List the files in the specified path
    monitor.listFiles("/path/to/storage");
    std::cout << "Listed files in /path/to/storage" << std::endl;

    // Dynamically add a storage path
    monitor.addStoragePath("/new/storage/path");
    std::cout << "Added storage path: /new/storage/path" << std::endl;

    // Dynamically remove a storage path
    monitor.removeStoragePath("/new/storage/path");
    std::cout << "Removed storage path: /new/storage/path" << std::endl;

    // Get the current storage status
    std::string status = monitor.getStorageStatus();
    std::cout << "Current storage status: " << status << std::endl;

    // Stop storage space monitoring
    monitor.stopMonitoring();
    std::cout << "Stopped monitoring" << std::endl;

    return 0;
}