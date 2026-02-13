#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/web/http/downloader.hpp"

using namespace atom::web;
namespace fs = std::filesystem;

class DownloadManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests to reduce noise
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Create temporary directory for test files
        tempDir = fs::temp_directory_path() / "atom_downloader_test";
        fs::create_directories(tempDir);

        // Create task file path
        taskFile = tempDir / "tasks.json";

        // Create download directory
        downloadDir = tempDir / "downloads";
        fs::create_directories(downloadDir);

        // Reset callback tracking
        resetCallbackTracking();
    }

    void TearDown() override {
        try {
            fs::remove_all(tempDir);
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }

    void resetCallbackTracking() {
        downloadCompleteCount = 0;
        progressUpdateCount = 0;
        errorCount = 0;
        lastCompletedIndex = SIZE_MAX;
        lastCompletedSuccess = false;
        lastProgressIndex = SIZE_MAX;
        lastProgressPercent = -1.0;
        lastErrorIndex = SIZE_MAX;
        lastErrorMessage.clear();
    }

    void setupCallbacks(DownloadManager& dm) {
        dm.onDownloadComplete([this](size_t index, bool success) {
            downloadCompleteCount++;
            lastCompletedIndex = index;
            lastCompletedSuccess = success;
        });

        dm.onProgressUpdate([this](size_t index, double percent) {
            progressUpdateCount++;
            lastProgressIndex = index;
            lastProgressPercent = percent;
        });

        dm.onError([this](size_t index, const std::string& message) {
            errorCount++;
            lastErrorIndex = index;
            lastErrorMessage = message;
        });
    }

    // Helper method to wait for downloads to complete
    void waitForDownloads(DownloadManager& dm, int maxWaitSeconds = 30) {
        auto start = std::chrono::steady_clock::now();
        while (dm.getActiveTaskCount() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start)
                    .count() > maxWaitSeconds) {
                break;
            }
        }
    }

    // Test data
    fs::path tempDir;
    fs::path taskFile;
    fs::path downloadDir;

    // Callback tracking
    std::atomic<int> downloadCompleteCount{0};
    std::atomic<int> progressUpdateCount{0};
    std::atomic<int> errorCount{0};
    std::atomic<size_t> lastCompletedIndex{SIZE_MAX};
    std::atomic<bool> lastCompletedSuccess{false};
    std::atomic<size_t> lastProgressIndex{SIZE_MAX};
    std::atomic<double> lastProgressPercent{-1.0};
    std::atomic<size_t> lastErrorIndex{SIZE_MAX};
    std::string lastErrorMessage;

    // Test URLs - using httpbin.org for reliable testing
    const std::string TEST_SMALL_FILE_URL =
        "https://httpbin.org/bytes/1024";  // 1KB
    const std::string TEST_MEDIUM_FILE_URL =
        "https://httpbin.org/bytes/10240";  // 10KB
    const std::string TEST_LARGE_FILE_URL =
        "https://httpbin.org/bytes/102400";  // 100KB
    const std::string TEST_INVALID_URL =
        "https://invalid.nonexistent.domain/file.txt";
    const std::string TEST_404_URL = "https://httpbin.org/status/404";
};

// Constructor/Destructor Tests
TEST_F(DownloadManagerTest, ConstructorDestructor) {
    ASSERT_NO_THROW({ DownloadManager dm(taskFile.string()); });
}

TEST_F(DownloadManagerTest, ConstructorWithNonexistentDirectory) {
    fs::path nonexistentPath = tempDir / "nonexistent" / "tasks.json";

    // Should create directory structure
    ASSERT_NO_THROW({ DownloadManager dm(nonexistentPath.string()); });
}

// Task Management Tests
TEST_F(DownloadManagerTest, AddTask) {
    DownloadManager dm(taskFile.string());

    fs::path outputFile = downloadDir / "test1.bin";

    ASSERT_NO_THROW(dm.addTask(TEST_SMALL_FILE_URL, outputFile.string()));

    EXPECT_EQ(dm.getTotalTaskCount(), 1);
    EXPECT_FALSE(dm.isRunning());
}

TEST_F(DownloadManagerTest, AddTaskWithPriority) {
    DownloadManager dm(taskFile.string());

    fs::path outputFile1 = downloadDir / "test1.bin";
    fs::path outputFile2 = downloadDir / "test2.bin";

    ASSERT_NO_THROW(dm.addTask(TEST_SMALL_FILE_URL, outputFile1.string(), 5));
    ASSERT_NO_THROW(dm.addTask(TEST_MEDIUM_FILE_URL, outputFile2.string(), 10));

    EXPECT_EQ(dm.getTotalTaskCount(), 2);
}

TEST_F(DownloadManagerTest, AddTaskInvalidArguments) {
    DownloadManager dm(taskFile.string());

    // Empty URL should throw
    EXPECT_THROW(dm.addTask("", "output.txt"), std::invalid_argument);

    // Empty filepath should throw
    EXPECT_THROW(dm.addTask(TEST_SMALL_FILE_URL, ""), std::invalid_argument);
}

TEST_F(DownloadManagerTest, RemoveTask) {
    DownloadManager dm(taskFile.string());

    fs::path outputFile = downloadDir / "test1.bin";
    dm.addTask(TEST_SMALL_FILE_URL, outputFile.string());

    EXPECT_EQ(dm.getTotalTaskCount(), 1);

    EXPECT_TRUE(dm.removeTask(0));
    EXPECT_EQ(dm.getTotalTaskCount(), 0);

    // Removing non-existent task should return false
    EXPECT_FALSE(dm.removeTask(0));
}

// Basic Download Tests
TEST_F(DownloadManagerTest, SingleDownload) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "single_test.bin";
    dm.addTask(TEST_SMALL_FILE_URL, outputFile.string());

    ASSERT_NO_THROW(dm.start(1));  // Single thread
    EXPECT_TRUE(dm.isRunning());

    waitForDownloads(dm);

    dm.stop();
    EXPECT_FALSE(dm.isRunning());

    // Check if file was downloaded
    EXPECT_TRUE(fs::exists(outputFile));
    EXPECT_GT(fs::file_size(outputFile), 0);

    // Check callbacks
    EXPECT_GT(downloadCompleteCount.load(), 0);
    EXPECT_TRUE(lastCompletedSuccess);
}

TEST_F(DownloadManagerTest, MultipleDownloads) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    std::vector<fs::path> outputFiles;
    for (int i = 0; i < 3; ++i) {
        fs::path outputFile =
            downloadDir / ("multi_test_" + std::to_string(i) + ".bin");
        outputFiles.push_back(outputFile);
        dm.addTask(TEST_SMALL_FILE_URL, outputFile.string());
    }

    ASSERT_NO_THROW(dm.start(2));  // Two threads

    waitForDownloads(dm);
    dm.stop();

    // Check all files were downloaded
    for (const auto& file : outputFiles) {
        EXPECT_TRUE(fs::exists(file));
        EXPECT_GT(fs::file_size(file), 0);
    }

    EXPECT_EQ(downloadCompleteCount.load(), 3);
}

// Progress Tracking Tests
TEST_F(DownloadManagerTest, ProgressTracking) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "progress_test.bin";
    dm.addTask(TEST_MEDIUM_FILE_URL, outputFile.string());

    dm.start(1);

    waitForDownloads(dm);
    dm.stop();

    // Should have received progress updates
    EXPECT_GT(progressUpdateCount.load(), 0);
    EXPECT_GE(lastProgressPercent.load(), 0.0);
    EXPECT_LE(lastProgressPercent.load(), 100.0);
}

TEST_F(DownloadManagerTest, GetProgress) {
    DownloadManager dm(taskFile.string());

    fs::path outputFile = downloadDir / "get_progress_test.bin";
    dm.addTask(TEST_MEDIUM_FILE_URL, outputFile.string());

    // Before starting, progress should be 0
    EXPECT_EQ(dm.getProgress(0), 0.0);

    dm.start(1);

    // During download, progress should be between 0 and 100
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    double progress = dm.getProgress(0);
    EXPECT_GE(progress, 0.0);
    EXPECT_LE(progress, 100.0);

    waitForDownloads(dm);
    dm.stop();

    // After completion, progress should be 100 or -1 (if task is cleaned up)
    double finalProgress = dm.getProgress(0);
    EXPECT_TRUE(finalProgress == 100.0 || finalProgress == -1.0);
}

// Pause/Resume Tests
TEST_F(DownloadManagerTest, PauseResumeTask) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "pause_resume_test.bin";
    dm.addTask(TEST_LARGE_FILE_URL,
               outputFile.string());  // Larger file for testing

    dm.start(1);

    // Let it start downloading
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Pause the task
    ASSERT_NO_THROW(dm.pauseTask(0));

    // Wait a bit to ensure it's paused
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Resume the task
    ASSERT_NO_THROW(dm.resumeTask(0));

    waitForDownloads(dm);
    dm.stop();

    EXPECT_TRUE(fs::exists(outputFile));
    EXPECT_GT(fs::file_size(outputFile), 0);
}

TEST_F(DownloadManagerTest, CancelTask) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "cancel_test.bin";
    dm.addTask(TEST_LARGE_FILE_URL, outputFile.string());

    dm.start(1);

    // Let it start downloading
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Cancel the task
    ASSERT_NO_THROW(dm.cancelTask(0));

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    dm.stop();

    // File might exist but should be incomplete or not exist
    if (fs::exists(outputFile)) {
        // If file exists, it should be smaller than expected
        EXPECT_LT(fs::file_size(outputFile), 102400);  // Less than full size
    }
}

// Error Handling Tests
TEST_F(DownloadManagerTest, InvalidURL) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "invalid_url_test.bin";
    dm.addTask(TEST_INVALID_URL, outputFile.string());

    dm.start(1);

    waitForDownloads(dm);
    dm.stop();

    // Should have received error callback
    EXPECT_GT(errorCount.load(), 0);
    EXPECT_FALSE(lastErrorMessage.empty());

    // Download should have failed
    EXPECT_GT(downloadCompleteCount.load(), 0);
    EXPECT_FALSE(lastCompletedSuccess);
}

TEST_F(DownloadManagerTest, HTTP404Error) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "404_test.bin";
    dm.addTask(TEST_404_URL, outputFile.string());

    dm.start(1);

    waitForDownloads(dm);
    dm.stop();

    // Should complete but might not be successful depending on implementation
    EXPECT_GT(downloadCompleteCount.load(), 0);
}

// Thread Management Tests
TEST_F(DownloadManagerTest, SetThreadCount) {
    DownloadManager dm(taskFile.string());

    // Add multiple tasks
    for (int i = 0; i < 5; ++i) {
        fs::path outputFile =
            downloadDir / ("thread_test_" + std::to_string(i) + ".bin");
        dm.addTask(TEST_SMALL_FILE_URL, outputFile.string());
    }

    dm.start(2);  // Start with 2 threads

    // Change thread count while running
    ASSERT_NO_THROW(dm.setThreadCount(4));

    waitForDownloads(dm);
    dm.stop();
}

TEST_F(DownloadManagerTest, MaxRetries) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    // Set max retries
    ASSERT_NO_THROW(dm.setMaxRetries(3));

    fs::path outputFile = downloadDir / "retry_test.bin";
    dm.addTask(TEST_INVALID_URL, outputFile.string());

    dm.start(1);

    waitForDownloads(dm, 60);  // Longer wait for retries
    dm.stop();

    // Should have attempted retries
    EXPECT_GT(errorCount.load(), 0);
}

// Speed Control Tests
TEST_F(DownloadManagerTest, SpeedLimit) {
    DownloadManager dm(taskFile.string());
    setupCallbacks(dm);

    fs::path outputFile = downloadDir / "speed_test.bin";
    dm.addTask(TEST_MEDIUM_FILE_URL, outputFile.string());

    // Start with speed limit
    dm.start(1, 1024);  // 1KB/s limit

    auto start = std::chrono::steady_clock::now();

    waitForDownloads(dm, 60);  // Longer wait for speed-limited download
    dm.stop();

    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(end - start);

    EXPECT_TRUE(fs::exists(outputFile));

    // Download should take some time due to speed limiting
    EXPECT_GT(duration.count(),
              5);  // Should take at least 5 seconds for 10KB at 1KB/s
}

// File System Tests
TEST_F(DownloadManagerTest, DownloadToNonexistentDirectory) {
    DownloadManager dm(taskFile.string());

    fs::path nonexistentDir = tempDir / "nonexistent" / "subdir";
    fs::path outputFile = nonexistentDir / "test.bin";

    // Should create directory structure
    ASSERT_NO_THROW(dm.addTask(TEST_SMALL_FILE_URL, outputFile.string()));

    dm.start(1);
    waitForDownloads(dm);
    dm.stop();

    EXPECT_TRUE(fs::exists(outputFile));
}

TEST_F(DownloadManagerTest, OverwriteExistingFile) {
    DownloadManager dm(taskFile.string());

    fs::path outputFile = downloadDir / "overwrite_test.bin";

    // Create existing file
    std::ofstream existingFile(outputFile);
    existingFile << "existing content";
    existingFile.close();

    auto originalSize = fs::file_size(outputFile);

    dm.addTask(TEST_SMALL_FILE_URL, outputFile.string());

    dm.start(1);
    waitForDownloads(dm);
    dm.stop();

    EXPECT_TRUE(fs::exists(outputFile));
    // File should be overwritten with new content
    EXPECT_NE(fs::file_size(outputFile), originalSize);
}

// Task Persistence Tests
TEST_F(DownloadManagerTest, TaskPersistence) {
    // Create first manager and add tasks
    {
        DownloadManager dm1(taskFile.string());

        fs::path outputFile1 = downloadDir / "persist_test1.bin";
        fs::path outputFile2 = downloadDir / "persist_test2.bin";

        dm1.addTask(TEST_SMALL_FILE_URL, outputFile1.string());
        dm1.addTask(TEST_MEDIUM_FILE_URL, outputFile2.string());

        EXPECT_EQ(dm1.getTotalTaskCount(), 2);
    }

    // Create second manager with same task file
    {
        DownloadManager dm2(taskFile.string());

        // Should load existing tasks
        EXPECT_EQ(dm2.getTotalTaskCount(), 2);
    }
}
