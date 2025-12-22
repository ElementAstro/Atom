/**
 * @file storage_advanced.cpp
 * @brief Comprehensive example demonstrating advanced storage monitoring
 *
 * This example showcases advanced storage monitoring capabilities including:
 * - Storage space monitoring and change detection
 * - Dynamic storage path management
 * - Storage media insertion detection
 * - File listing and storage analysis
 * - Callback-based event handling
 * - Real-time storage monitoring
 * - Cross-platform storage operations
 *
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @note May require elevated privileges for some operations
 * @author Atom Framework
 * @date 2024
 */

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/system/storage.hpp"

using namespace atom::system;

// Global variables for demonstrationstd::atomic<int> changeCount{0};
std::atomic<bool> monitoringActive{true};

/**
 * @brief Print a formatted section header
 */
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

/**
 * @brief Enhanced storage callback with detailed logging
 */
void enhancedStorageCallback(const std::string& path) {
    changeCount++;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "] "
              << "Storage change #" << changeCount << " detected at: " << path
              << std::endl;

    // Try to get additional information about the path
    try {
        if (std::filesystem::exists(path)) {
            auto space = std::filesystem::space(path);
            double freeGB =
                static_cast<double>(space.free) / (1024 * 1024 * 1024);
            double totalGB =
                static_cast<double>(space.capacity) / (1024 * 1024 * 1024);
            double usedPercent =
                ((static_cast<double>(space.capacity - space.free) /
                  space.capacity) *
                 100.0);

            std::cout << "  Space info: " << std::fixed << std::setprecision(2)
                      << freeGB << " GB free of " << totalGB << " GB total ("
                      << usedPercent << "% used)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  Could not get space info: " << e.what() << std::endl;
    }
}

/**
 * @brief Simple storage callback for basic monitoring
 */
void simpleStorageCallback(const std::string& path) {
    std::cout << "[CALLBACK] Storage space changed at: " << path << std::endl;
}

/**
 * @brief Get platform-appropriate storage paths for testing
 */
std::vector<std::string> getTestStoragePaths() {
#ifdef _WIN32
    return {"C:\\", "D:\\", "E:\\"};
#else
    return {"/", "/home", "/tmp", "/mnt", "/media"};
#endif
}

/**
 * @brief Demonstrate storage path management
 */
void demonstratePathManagement(StorageMonitor& monitor) {
    std::cout << "\n--- Storage Path Management ---" << std::endl;

    auto testPaths = getTestStoragePaths();
    std::cout << "Testing with platform-appropriate paths:" << std::endl;

    for (const auto& path : testPaths) {
        try {
            if (std::filesystem::exists(path)) {
                std::cout << "Adding storage path: " << path << std::endl;
                monitor.addStoragePath(path);

                // Check if new media is inserted
                bool hasNewMedia = monitor.isNewMediaInserted(path);
                std::cout << "  New media detected: "
                          << (hasNewMedia ? "Yes" : "No") << std::endl;
            } else {
                std::cout << "Skipping non-existent path: " << path
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error with path " << path << ": " << e.what()
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrate file listing capabilities
 */
void demonstrateFileListing(StorageMonitor& monitor) {
    std::cout << "\n--- File Listing Demonstration ---" << std::endl;

    auto testPaths = getTestStoragePaths();

    for (const auto& path : testPaths) {
        try {
            if (std::filesystem::exists(path) &&
                std::filesystem::is_directory(path)) {
                std::cout << "\nListing files in: " << path << std::endl;
                monitor.listFiles(path);
                break;  // Only list files for the first valid directory
            }
        } catch (const std::exception& e) {
            std::cout << "Error listing files in " << path << ": " << e.what()
                      << std::endl;
        }
    }
}

int main() {
    try {
        std::cout << "=== Advanced Storage Monitoring Example ===" << std::endl;
        std::cout
            << "Demonstrating comprehensive storage monitoring capabilities\n"
            << std::endl;

        // 1. Storage Monitor Initialization
        printSection("Storage Monitor Initialization");

        StorageMonitor monitor;
        std::cout << "Created StorageMonitor instance" << std::endl;

        // 2. Callback Registration
        printSection("Callback Registration");

        std::cout << "Registering enhanced storage callback..." << std::endl;
        monitor.registerCallback(enhancedStorageCallback);
        std::cout << "Enhanced callback registered successfully" << std::endl;

        // 3. Storage Path Management
        printSection("Storage Path Management");

        demonstratePathManagement(monitor);

        // 4. Storage Listing
        printSection("Storage Listing");

        std::cout << "Listing all mounted storage spaces:" << std::endl;
        monitor.listAllStorage();

        // 5. File Listing
        printSection("File Listing");

        demonstrateFileListing(monitor);

        // 6. Storage Status
        printSection("Storage Status");

        std::string status = monitor.getStorageStatus();
        std::cout << "Current storage status:" << std::endl;
        std::cout << status << std::endl;

        // 7. Monitoring Operations
        printSection("Storage Monitoring Operations");

        std::cout << "Starting storage monitoring..." << std::endl;
        bool started = monitor.startMonitoring();
        std::cout << "Monitoring started: "
                  << (started ? "Successfully" : "Failed") << std::endl;

        if (!started) {
            std::cout << "Failed to start monitoring. This may indicate:"
                      << std::endl;
            std::cout << "- Insufficient permissions" << std::endl;
            std::cout << "- Storage subsystem not available" << std::endl;
            std::cout << "- Platform limitations" << std::endl;
            return 1;
        }

        // Check monitoring status
        bool running = monitor.isRunning();
        std::cout << "Is monitoring running: " << (running ? "Yes" : "No")
                  << std::endl;

        // 8. Real-time Monitoring Demonstration
        printSection("Real-time Monitoring Demonstration");

        std::cout << "Monitoring storage changes for 10 seconds..."
                  << std::endl;
        std::cout << "Try creating/deleting files or inserting/removing "
                     "storage devices"
                  << std::endl;
        std::cout << "Monitoring will detect changes and trigger callbacks\n"
                  << std::endl;

        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(10);

        int lastChangeCount = changeCount.load();

        while (std::chrono::steady_clock::now() < endTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            int currentChangeCount = changeCount.load();
            if (currentChangeCount != lastChangeCount) {
                lastChangeCount = currentChangeCount;
                std::cout << "Total changes detected so far: "
                          << currentChangeCount << std::endl;
            }

            // Show a progress indicator
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime);
            std::cout << "\rMonitoring... " << elapsed.count() << "/10 seconds"
                      << std::flush;
        }

        std::cout << std::endl;
        std::cout << "Monitoring period complete. Total changes detected: "
                  << changeCount.load() << std::endl;

        // 9. Manual Callback Testing
        printSection("Manual Callback Testing");

        std::cout << "Testing manual callback triggering..." << std::endl;
        auto testPaths = getTestStoragePaths();

        for (const auto& path : testPaths) {
            if (std::filesystem::exists(path)) {
                std::cout << "Triggering callback for: " << path << std::endl;
                monitor.triggerCallbacks(path);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                break;  // Only test with the first valid path
            }
        }

        // 10. Dynamic Path Management
        printSection("Dynamic Path Management");

        std::cout << "Testing dynamic path addition and removal..."
                  << std::endl;

        std::string testPath;
#ifdef _WIN32
        testPath = "C:\\Windows\\Temp";
#else
        testPath = "/tmp";
#endif

        if (std::filesystem::exists(testPath)) {
            std::cout << "Adding test path: " << testPath << std::endl;
            monitor.addStoragePath(testPath);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            std::cout << "Removing test path: " << testPath << std::endl;
            monitor.removeStoragePath(testPath);
        } else {
            std::cout << "Test path not available: " << testPath << std::endl;
        }

        // 11. Final Status Check
        printSection("Final Status Check");

        std::cout << "Final monitoring status:" << std::endl;
        std::cout << "  Is running: " << (monitor.isRunning() ? "Yes" : "No")
                  << std::endl;
        std::cout << "  Total changes detected: " << changeCount.load()
                  << std::endl;

        std::string finalStatus = monitor.getStorageStatus();
        std::cout << "  Final storage status: " << finalStatus << std::endl;

        // 12. Cleanup
        printSection("Cleanup");

        std::cout << "Stopping storage monitoring..." << std::endl;
        monitor.stopMonitoring();
        std::cout << "Monitoring stopped successfully" << std::endl;

        // Verify monitoring has stopped
        bool stillRunning = monitor.isRunning();
        std::cout << "Monitoring still running: "
                  << (stillRunning ? "Yes" : "No") << std::endl;

        std::cout << "\n=== Advanced Storage Monitoring Complete ==="
                  << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Storage monitor initialization and configuration"
                  << std::endl;
        std::cout << "- Callback registration and event handling" << std::endl;
        std::cout << "- Dynamic storage path management" << std::endl;
        std::cout << "- Storage and file listing capabilities" << std::endl;
        std::cout << "- Real-time storage change monitoring" << std::endl;
        std::cout << "- Manual callback triggering" << std::endl;
        std::cout << "- Storage status reporting" << std::endl;
        std::cout << "- Proper cleanup and resource management" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Storage monitoring error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Storage subsystem not available" << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        std::cerr << "- File system access restrictions" << std::endl;
        return 1;
    }

    return 0;
}
