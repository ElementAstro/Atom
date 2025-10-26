/**
 * @file storage_basic.cpp
 * @brief Basic example demonstrating fundamental storage monitoring
 *
 * This example provides a gentle introduction to storage monitoring using
 * the Atom System module. It covers:
 * - Basic storage monitor setup
 * - Simple callback registration
 * - Storage listing operations
 * - Basic monitoring operations
 *
 * @note This is a beginner-friendly example
 * @author Atom Framework
 * @date 2024
 */

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include "atom/system/storage.hpp"

using namespace atom::system;

/**
 * @brief Simple callback function for storage changes
 */
void storageChangeCallback(const std::string& path) {
    std::cout << "[STORAGE CHANGE] Detected change at: " << path << std::endl;

    // Try to show basic space information
    try {
        if (std::filesystem::exists(path)) {
            auto space = std::filesystem::space(path);
            double freeGB =
                static_cast<double>(space.free) / (1024 * 1024 * 1024);
            double totalGB =
                static_cast<double>(space.capacity) / (1024 * 1024 * 1024);

            std::cout << "  Available: " << std::fixed << std::setprecision(1)
                      << freeGB << " GB of " << totalGB << " GB" << std::endl;
        }
    } catch (const std::exception&) {
        // Ignore errors for simplicity in basic example
    }
}

int main() {
    try {
        std::cout << "=== Basic Storage Monitoring Example ===" << std::endl;
        std::cout << "Learning fundamental storage monitoring\n" << std::endl;

        // 1. Create Storage Monitor
        std::cout << "[1. Storage Monitor Creation]" << std::endl;
        StorageMonitor monitor;
        std::cout << "Created StorageMonitor instance" << std::endl;

        // 2. Register Callback
        std::cout << "\n[2. Callback Registration]" << std::endl;
        monitor.registerCallback(storageChangeCallback);
        std::cout << "Registered storage change callback" << std::endl;

        // 3. List All Storage
        std::cout << "\n[3. Storage Listing]" << std::endl;
        std::cout << "Listing all mounted storage spaces:" << std::endl;
        monitor.listAllStorage();

        // 4. Add Storage Paths
        std::cout << "\n[4. Adding Storage Paths]" << std::endl;

        // Add platform-appropriate paths
#ifdef _WIN32
        std::vector<std::string> testPaths = {"C:\\"};
        if (std::filesystem::exists("D:\\")) {
            testPaths.push_back("D:\\");
        }
#else
        std::vector<std::string> testPaths = {"/"};
        if (std::filesystem::exists("/home")) {
            testPaths.push_back("/home");
        }
#endif

        for (const auto& path : testPaths) {
            if (std::filesystem::exists(path)) {
                std::cout << "Adding storage path: " << path << std::endl;
                monitor.addStoragePath(path);
            } else {
                std::cout << "Skipping non-existent path: " << path
                          << std::endl;
            }
        }

        // 5. Check for New Media
        std::cout << "\n[5. New Media Detection]" << std::endl;
        for (const auto& path : testPaths) {
            if (std::filesystem::exists(path)) {
                bool hasNewMedia = monitor.isNewMediaInserted(path);
                std::cout << "New media at " << path << ": "
                          << (hasNewMedia ? "Yes" : "No") << std::endl;
            }
        }

        // 6. Get Storage Status
        std::cout << "\n[6. Storage Status]" << std::endl;
        std::string status = monitor.getStorageStatus();
        std::cout << "Current storage status: " << status << std::endl;

        // 7. List Files in a Directory
        std::cout << "\n[7. File Listing]" << std::endl;

        // Find a suitable directory to list
        std::string listPath;
#ifdef _WIN32
        if (std::filesystem::exists("C:\\Windows")) {
            listPath = "C:\\Windows";
        } else {
            listPath = "C:\\";
        }
#else
        if (std::filesystem::exists("/usr")) {
            listPath = "/usr";
        } else {
            listPath = "/";
        }
#endif

        if (!listPath.empty() && std::filesystem::exists(listPath)) {
            std::cout << "Listing files in: " << listPath << std::endl;
            monitor.listFiles(listPath);
        }

        // 8. Start Monitoring
        std::cout << "\n[8. Start Monitoring]" << std::endl;
        std::cout << "Starting storage monitoring..." << std::endl;

        bool started = monitor.startMonitoring();
        std::cout << "Monitoring started: "
                  << (started ? "Successfully" : "Failed") << std::endl;

        if (!started) {
            std::cout << "Failed to start monitoring. This may be normal on "
                         "some systems."
                      << std::endl;
            std::cout << "Continuing with other demonstrations..." << std::endl;
        } else {
            // Check if monitoring is running
            bool running = monitor.isRunning();
            std::cout << "Is monitoring running: " << (running ? "Yes" : "No")
                      << std::endl;

            // 9. Monitor for a Short Time
            std::cout << "\n[9. Short Monitoring Period]" << std::endl;
            std::cout << "Monitoring for 5 seconds..." << std::endl;
            std::cout
                << "(Try creating or deleting a file to see change detection)"
                << std::endl;

            for (int i = 5; i > 0; i--) {
                std::cout << "  " << i << " seconds remaining..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            std::cout << "Monitoring period complete" << std::endl;
        }

        // 10. Manual Callback Test
        std::cout << "\n[10. Manual Callback Test]" << std::endl;
        std::cout << "Testing manual callback trigger..." << std::endl;

        if (!testPaths.empty() && std::filesystem::exists(testPaths[0])) {
            monitor.triggerCallbacks(testPaths[0]);
        }

        // 11. Path Management
        std::cout << "\n[11. Path Management]" << std::endl;

        // Add a temporary path
        std::string tempPath;
#ifdef _WIN32
        tempPath = "C:\\Windows\\Temp";
#else
        tempPath = "/tmp";
#endif

        if (std::filesystem::exists(tempPath)) {
            std::cout << "Adding temporary path: " << tempPath << std::endl;
            monitor.addStoragePath(tempPath);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::cout << "Removing temporary path: " << tempPath << std::endl;
            monitor.removeStoragePath(tempPath);
        }

        // 12. Final Status
        std::cout << "\n[12. Final Status]" << std::endl;
        std::string finalStatus = monitor.getStorageStatus();
        std::cout << "Final storage status: " << finalStatus << std::endl;

        bool stillRunning = monitor.isRunning();
        std::cout << "Monitoring still running: "
                  << (stillRunning ? "Yes" : "No") << std::endl;

        // 13. Cleanup
        std::cout << "\n[13. Cleanup]" << std::endl;
        if (monitor.isRunning()) {
            std::cout << "Stopping monitoring..." << std::endl;
            monitor.stopMonitoring();
            std::cout << "Monitoring stopped" << std::endl;
        } else {
            std::cout << "Monitoring was not running, no cleanup needed"
                      << std::endl;
        }

        std::cout << "\n=== Basic Storage Monitoring Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Storage monitor creation and setup" << std::endl;
        std::cout << "- Callback registration for change detection"
                  << std::endl;
        std::cout << "- Storage and file listing operations" << std::endl;
        std::cout << "- Storage path management" << std::endl;
        std::cout << "- New media detection" << std::endl;
        std::cout << "- Basic monitoring operations" << std::endl;
        std::cout << "- Status reporting and cleanup" << std::endl;
        std::cout << "\nNext steps: Try the advanced storage example for more "
                     "features!"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Storage monitoring error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient permissions (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Storage subsystem not available" << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        return 1;
    }

    return 0;
}
