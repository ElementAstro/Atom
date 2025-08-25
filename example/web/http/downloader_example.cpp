/*
 * downloader_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom DownloadManager
 * class
 */

#include "atom/log/loguru.hpp"
#include "atom/web/downloader.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace atom::web;

// Global variables for tracking download progress
std::atomic<bool> downloadCompleted{false};
std::atomic<size_t> completedDownloads{0};
std::atomic<size_t> failedDownloads{0};

void demonstrateBasicDownload() {
    std::cout << "\n=== Basic Download Example ===\n";

    try {
        // Create download manager with task file
        DownloadManager dm("basic_downloads.json");

        // Add a simple download task
        std::string url =
            "https://httpbin.org/bytes/1024";  // Download 1KB of data
        std::string filepath = "downloads/basic_test.bin";

        // Create downloads directory if it doesn't exist
        std::filesystem::create_directories("downloads");

        std::cout << "Adding download task:\n";
        std::cout << "  URL: " << url << "\n";
        std::cout << "  File: " << filepath << "\n";

        dm.addTask(url, filepath, 10);  // High priority

        // Set up completion callback
        dm.onDownloadComplete([](size_t index, bool success) {
            std::cout << "Download " << index << " "
                      << (success ? "completed successfully" : "failed")
                      << "\n";
            downloadCompleted = true;
        });

        // Start download with single thread
        std::cout << "Starting download...\n";
        dm.start(1);

        // Wait for completion (with timeout)
        auto startTime = std::chrono::steady_clock::now();
        while (!downloadCompleted &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (downloadCompleted) {
            std::cout << "Basic download completed!\n";

            // Check if file exists and show size
            if (std::filesystem::exists(filepath)) {
                auto fileSize = std::filesystem::file_size(filepath);
                std::cout << "Downloaded file size: " << fileSize << " bytes\n";
            }
        } else {
            std::cout << "Download timed out or failed\n";
        }

        dm.stop();
        downloadCompleted = false;  // Reset for next example

    } catch (const std::exception& e) {
        std::cerr << "Error in basic download: " << e.what() << "\n";
    }
}

void demonstrateMultiThreadedDownload() {
    std::cout << "\n=== Multi-threaded Download Example ===\n";

    try {
        DownloadManager dm("multithreaded_downloads.json");

        // Add multiple download tasks
        std::vector<std::pair<std::string, std::string>> downloads = {
            {"https://httpbin.org/bytes/2048", "downloads/file1.bin"},
            {"https://httpbin.org/bytes/4096", "downloads/file2.bin"},
            {"https://httpbin.org/bytes/8192", "downloads/file3.bin"},
            {"https://httpbin.org/bytes/1024", "downloads/file4.bin"}};

        std::cout << "Adding " << downloads.size() << " download tasks:\n";

        for (size_t i = 0; i < downloads.size(); ++i) {
            const auto& [url, filepath] = downloads[i];
            std::cout << "  Task " << i << ": " << url << " -> " << filepath
                      << "\n";
            dm.addTask(
                url, filepath,
                static_cast<int>(downloads.size() - i));  // Varying priorities
        }

        // Set up callbacks
        dm.onDownloadComplete([&downloads](size_t index, bool success) {
            if (index < downloads.size()) {
                std::cout << "Download " << index << " ("
                          << downloads[index].second << ") "
                          << (success ? "completed" : "failed") << "\n";
            }

            if (success) {
                completedDownloads++;
            } else {
                failedDownloads++;
            }
        });

        dm.onProgressUpdate([&downloads](size_t index, double progress) {
            if (index < downloads.size()) {
                std::cout << "Download " << index << " ("
                          << downloads[index].second
                          << ") progress: " << std::fixed
                          << std::setprecision(1) << (progress * 100) << "%\n";
            }
        });

        dm.onError([&downloads](size_t index, const std::string& error) {
            if (index < downloads.size()) {
                std::cout << "Download " << index << " ("
                          << downloads[index].second << ") error: " << error
                          << "\n";
            }
        });

        // Start with multiple threads
        size_t threadCount = 3;
        std::cout << "Starting downloads with " << threadCount
                  << " threads...\n";
        dm.start(threadCount);

        // Monitor progress
        auto startTime = std::chrono::steady_clock::now();
        while ((completedDownloads + failedDownloads) < downloads.size() &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(60)) {
            std::cout << "Active tasks: " << dm.getActiveTaskCount()
                      << ", Total tasks: " << dm.getTotalTaskCount()
                      << ", Running: " << (dm.isRunning() ? "Yes" : "No")
                      << "\n";

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "Multi-threaded download completed!\n";
        std::cout << "Successful downloads: " << completedDownloads << "\n";
        std::cout << "Failed downloads: " << failedDownloads << "\n";

        dm.stop();

        // Reset counters
        completedDownloads = 0;
        failedDownloads = 0;

    } catch (const std::exception& e) {
        std::cerr << "Error in multi-threaded download: " << e.what() << "\n";
    }
}

void demonstrateDownloadControl() {
    std::cout << "\n=== Download Control Example ===\n";

    try {
        DownloadManager dm("control_downloads.json");

        // Add a larger download for testing control features
        std::string url = "https://httpbin.org/bytes/32768";  // 32KB
        std::string filepath = "downloads/control_test.bin";

        dm.addTask(url, filepath, 5);

        // Set up callbacks
        dm.onDownloadComplete([](size_t index, bool success) {
            std::cout << "Controlled download " << index << " "
                      << (success ? "completed" : "failed") << "\n";
            downloadCompleted = true;
        });

        dm.onProgressUpdate([](size_t /*index*/, double progress) {
            std::cout << "Progress: " << std::fixed << std::setprecision(1)
                      << (progress * 100) << "%\n";
        });

        // Start download
        std::cout << "Starting controlled download...\n";
        dm.start(1);

        // Let it run for a bit
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Pause the download
        std::cout << "Pausing download...\n";
        dm.pauseTask(0);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Resume the download
        std::cout << "Resuming download...\n";
        dm.resumeTask(0);

        // Wait for completion
        auto startTime = std::chrono::steady_clock::now();
        while (!downloadCompleted &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (downloadCompleted) {
            std::cout << "Controlled download completed successfully!\n";
        } else {
            std::cout << "Cancelling download due to timeout...\n";
            dm.cancelTask(0);
        }

        dm.stop();
        downloadCompleted = false;

    } catch (const std::exception& e) {
        std::cerr << "Error in download control: " << e.what() << "\n";
    }
}

void demonstrateDownloadConfiguration() {
    std::cout << "\n=== Download Configuration Example ===\n";

    try {
        DownloadManager dm("config_downloads.json");

        // Configure download manager
        dm.setMaxRetries(5);   // Retry failed downloads up to 5 times
        dm.setThreadCount(2);  // Use 2 threads

        std::cout << "Download manager configured:\n";
        std::cout << "  Max retries: 5\n";
        std::cout << "  Thread count: 2\n";

        // Add some downloads
        std::vector<std::string> urls = {
            "https://httpbin.org/bytes/1024",
            "https://httpbin.org/status/404",  // This will fail
            "https://httpbin.org/bytes/2048"};

        for (size_t i = 0; i < urls.size(); ++i) {
            std::string filepath =
                "downloads/config_test_" + std::to_string(i) + ".bin";
            dm.addTask(urls[i], filepath, static_cast<int>(i));
            std::cout << "Added task " << i << ": " << urls[i] << "\n";
        }

        // Set up error handling
        dm.onError([](size_t index, const std::string& error) {
            std::cout << "Task " << index << " error: " << error << "\n";
        });

        dm.onDownloadComplete([](size_t index, bool success) {
            std::cout << "Task " << index << " "
                      << (success ? "completed" : "failed after retries")
                      << "\n";

            if (success) {
                completedDownloads++;
            } else {
                failedDownloads++;
            }
        });

        // Start downloads with speed limit (1MB/s)
        size_t speedLimit = 1024 * 1024;  // 1 MB/s
        std::cout << "Starting downloads with speed limit: " << speedLimit
                  << " bytes/s\n";
        dm.start(2, speedLimit);

        // Wait for all downloads to complete
        auto startTime = std::chrono::steady_clock::now();
        while ((completedDownloads + failedDownloads) < urls.size() &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(60)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Configuration example completed!\n";
        std::cout << "Successful downloads: " << completedDownloads << "\n";
        std::cout << "Failed downloads: " << failedDownloads << "\n";

        dm.stop();

        // Reset counters
        completedDownloads = 0;
        failedDownloads = 0;

    } catch (const std::exception& e) {
        std::cerr << "Error in download configuration: " << e.what() << "\n";
    }
}

void demonstrateTaskManagement() {
    std::cout << "\n=== Task Management Example ===\n";

    try {
        DownloadManager dm("task_management.json");

        // Add several tasks
        std::vector<std::pair<std::string, std::string>> tasks = {
            {"https://httpbin.org/bytes/1024", "downloads/task1.bin"},
            {"https://httpbin.org/bytes/2048", "downloads/task2.bin"},
            {"https://httpbin.org/bytes/4096", "downloads/task3.bin"}};

        std::cout << "Adding tasks:\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            const auto& [url, filepath] = tasks[i];
            dm.addTask(url, filepath, static_cast<int>(i));
            std::cout << "  Task " << i << ": " << filepath << "\n";
        }

        std::cout << "Total tasks: " << dm.getTotalTaskCount() << "\n";

        // Remove a task
        std::cout << "Removing task 1...\n";
        if (dm.removeTask(1)) {
            std::cout << "Task 1 removed successfully\n";
        } else {
            std::cout << "Failed to remove task 1\n";
        }

        std::cout << "Total tasks after removal: " << dm.getTotalTaskCount()
                  << "\n";

        // Start remaining downloads
        dm.onDownloadComplete([](size_t index, bool success) {
            std::cout << "Task " << index << " "
                      << (success ? "completed" : "failed") << "\n";
            completedDownloads++;
        });

        dm.start(1);

        // Wait for completion
        auto startTime = std::chrono::steady_clock::now();
        while (completedDownloads <
                   2 &&  // Expecting 2 downloads (task 1 was removed)
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << "Task management example completed!\n";
        std::cout << "Completed downloads: " << completedDownloads << "\n";

        dm.stop();
        completedDownloads = 0;

    } catch (const std::exception& e) {
        std::cerr << "Error in task management: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("downloader_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "        ATOM DOWNLOAD MANAGER DEMO          \n";
    std::cout << "============================================\n";

    try {
        // Create downloads directory
        std::filesystem::create_directories("downloads");

        demonstrateBasicDownload();
        demonstrateMultiThreadedDownload();
        demonstrateDownloadControl();
        demonstrateDownloadConfiguration();
        demonstrateTaskManagement();

        std::cout << "\n============================================\n";
        std::cout << "     DOWNLOAD MANAGER DEMO COMPLETED       \n";
        std::cout << "============================================\n";

        // Cleanup downloaded files
        std::cout << "\nCleaning up downloaded files...\n";
        try {
            std::filesystem::remove_all("downloads");
            std::filesystem::remove("basic_downloads.json");
            std::filesystem::remove("multithreaded_downloads.json");
            std::filesystem::remove("control_downloads.json");
            std::filesystem::remove("config_downloads.json");
            std::filesystem::remove("task_management.json");
            std::cout << "Cleanup completed.\n";
        } catch (const std::exception& e) {
            std::cout << "Cleanup warning: " << e.what() << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
