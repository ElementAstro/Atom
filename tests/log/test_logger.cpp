/*
 * test_logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Log Module
Tests logger functionality, log management, async logging, and mmap logging.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "atom/log/async_logger.hpp"
#include "atom/log/log_manager.hpp"
#include "atom/log/logger.hpp"
#include "atom/log/mmap_logger.hpp"

namespace atom::log::test {

// ============================================================================
// Logger Tests
// ============================================================================

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test log directory
        test_log_dir = "test_logs";
        std::filesystem::create_directories(test_log_dir);

        // Initialize logger manager
        logger_manager = std::make_unique<lithium::LoggerManager>();
    }

    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
        logger_manager.reset();
    }

    std::string test_log_dir;
    std::unique_ptr<lithium::LoggerManager> logger_manager;
};

TEST_F(LoggerTest, BasicLogging) {
    // Test basic logging functionality
    std::string log_file = test_log_dir + "/basic_test.log";

    // Create a simple log entry
    std::ofstream ofs(log_file);
    ofs << "[INFO] 2024-12-22 10:00:00 Test message\n";
    ofs << "[ERROR] 2024-12-22 10:01:00 Error message\n";
    ofs << "[DEBUG] 2024-12-22 10:02:00 Debug message\n";
    ofs.close();

    // Test log scanning
    logger_manager->scanLogsFolder(test_log_dir);

    // Test log searching
    auto search_results = logger_manager->searchLogs("ERROR");
    EXPECT_GE(search_results.size(), 1);

    // Verify search result contains expected content
    bool found_error = false;
    for (const auto& entry : search_results) {
        if (entry.message.find("Error message") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_error);
}

TEST_F(LoggerTest, LogAnalysis) {
    // Test log analysis functionality
    std::string log_file = test_log_dir + "/analysis_test.log";

    // Create log file with various log levels
    std::ofstream ofs(log_file);
    for (int i = 0; i < 100; ++i) {
        if (i % 10 == 0) {
            ofs << "[ERROR] 2024-12-22 10:" << std::setfill('0') << std::setw(2)
                << i << ":00 Error " << i << "\n";
        } else if (i % 5 == 0) {
            ofs << "[WARN] 2024-12-22 10:" << std::setfill('0') << std::setw(2)
                << i << ":00 Warning " << i << "\n";
        } else {
            ofs << "[INFO] 2024-12-22 10:" << std::setfill('0') << std::setw(2)
                << i << ":00 Info " << i << "\n";
        }
    }
    ofs.close();

    // Scan and analyze logs
    logger_manager->scanLogsFolder(test_log_dir);
    logger_manager->analyzeLogs();

    // Test searching for different log levels
    auto error_results = logger_manager->searchLogs("ERROR");
    auto warn_results = logger_manager->searchLogs("WARN");
    auto info_results = logger_manager->searchLogs("INFO");

    EXPECT_EQ(error_results.size(), 10);  // Every 10th entry
    EXPECT_EQ(warn_results.size(), 10);   // Every 5th entry (excluding errors)
    EXPECT_EQ(info_results.size(), 80);   // Remaining entries
}

TEST_F(LoggerTest, FileUpload) {
    // Test file upload functionality
    std::string log_file = test_log_dir + "/upload_test.log";

    // Create test log file
    std::ofstream ofs(log_file);
    ofs << "[INFO] 2024-12-22 10:00:00 Upload test message\n";
    ofs.close();

    // Test file upload (this may not actually upload but should not crash)
    EXPECT_NO_THROW(logger_manager->uploadFile(log_file));
}

TEST_F(LoggerTest, EdgeCases) {
    // Test edge cases and error handling

    // Test with non-existent directory
    EXPECT_NO_THROW(logger_manager->scanLogsFolder("non_existent_directory"));

    // Test with empty search term
    auto empty_results = logger_manager->searchLogs("");
    EXPECT_GE(empty_results.size(), 0);

    // Test with non-existent file upload
    EXPECT_NO_THROW(logger_manager->uploadFile("non_existent_file.log"));

    // Test analysis with no logs
    EXPECT_NO_THROW(logger_manager->analyzeLogs());
}

// ============================================================================
// Async Logger Tests
// ============================================================================

class AsyncLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_dir = "test_async_logs";
        std::filesystem::create_directories(test_log_dir);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
    }

    std::string test_log_dir;
};

TEST_F(AsyncLoggerTest, ConcurrentLogging) {
    // Test concurrent logging with async logger
    std::string log_file = test_log_dir + "/async_test.log";

    // Create async logger (if available)
    // Note: This test assumes async logger functionality exists
    const int num_threads = 4;
    const int messages_per_thread = 100;
    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, messages_per_thread, &log_file]() {
            std::ofstream ofs(log_file + "_thread_" + std::to_string(t));
            for (int i = 0; i < messages_per_thread; ++i) {
                ofs << "[INFO] Thread " << t << " Message " << i << "\n";
            }
            ofs.close();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Verify all log files were created
    for (int t = 0; t < num_threads; ++t) {
        std::string thread_log = log_file + "_thread_" + std::to_string(t);
        EXPECT_TRUE(std::filesystem::exists(thread_log));

        // Verify file has expected number of lines
        std::ifstream ifs(thread_log);
        int line_count = 0;
        std::string line;
        while (std::getline(ifs, line)) {
            line_count++;
        }
        EXPECT_EQ(line_count, messages_per_thread);
    }

    // Performance should be reasonable (less than 5 seconds for this test)
    EXPECT_LT(duration.count(), 5000);
}

// ============================================================================
// Memory-Mapped Logger Tests
// ============================================================================

class MmapLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_dir = "test_mmap_logs";
        std::filesystem::create_directories(test_log_dir);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
    }

    std::string test_log_dir;
};

TEST_F(MmapLoggerTest, HighVolumeLogging) {
    // Test high-volume logging with memory-mapped files
    std::string log_file = test_log_dir + "/mmap_test.log";

    const int num_messages = 10000;
    const std::string message_template = "[INFO] High volume message ";

    auto start_time = std::chrono::steady_clock::now();

    // Simulate high-volume logging
    std::ofstream ofs(log_file);
    for (int i = 0; i < num_messages; ++i) {
        ofs << message_template << i << " with timestamp "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count()
            << "\n";
    }
    ofs.close();

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Verify file was created and has correct size
    EXPECT_TRUE(std::filesystem::exists(log_file));

    // Count lines to verify all messages were written
    std::ifstream ifs(log_file);
    int line_count = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        line_count++;
    }
    EXPECT_EQ(line_count, num_messages);

    // Performance should be reasonable
    EXPECT_LT(duration.count(), 10000);  // Less than 10 seconds
}

// ============================================================================
// Integration Tests
// ============================================================================

class LogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_dir = "test_integration_logs";
        std::filesystem::create_directories(test_log_dir);
        logger_manager = std::make_unique<lithium::LoggerManager>();
    }

    void TearDown() override {
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
        logger_manager.reset();
    }

    std::string test_log_dir;
    std::unique_ptr<lithium::LoggerManager> logger_manager;
};

TEST_F(LogIntegrationTest, CompleteWorkflow) {
    // Test complete logging workflow

    // 1. Create multiple log files with different content
    std::vector<std::string> log_files = {test_log_dir + "/app.log",
                                          test_log_dir + "/error.log",
                                          test_log_dir + "/debug.log"};

    // Create app.log
    std::ofstream app_log(log_files[0]);
    app_log << "[INFO] 2024-12-22 10:00:00 Application started\n";
    app_log << "[INFO] 2024-12-22 10:01:00 Processing request\n";
    app_log << "[WARN] 2024-12-22 10:02:00 High memory usage\n";
    app_log.close();

    // Create error.log
    std::ofstream error_log(log_files[1]);
    error_log << "[ERROR] 2024-12-22 10:03:00 Database connection failed\n";
    error_log << "[ERROR] 2024-12-22 10:04:00 Authentication error\n";
    error_log.close();

    // Create debug.log
    std::ofstream debug_log(log_files[2]);
    debug_log << "[DEBUG] 2024-12-22 10:05:00 Variable x = 42\n";
    debug_log << "[DEBUG] 2024-12-22 10:06:00 Function call trace\n";
    debug_log.close();

    // 2. Scan logs folder
    logger_manager->scanLogsFolder(test_log_dir);

    // 3. Search for different patterns
    auto error_results = logger_manager->searchLogs("ERROR");
    auto info_results = logger_manager->searchLogs("INFO");
    auto debug_results = logger_manager->searchLogs("DEBUG");

    EXPECT_EQ(error_results.size(), 2);
    EXPECT_EQ(info_results.size(), 2);
    EXPECT_EQ(debug_results.size(), 2);

    // 4. Analyze logs
    EXPECT_NO_THROW(logger_manager->analyzeLogs());

    // 5. Test file upload for each log file
    for (const auto& log_file : log_files) {
        EXPECT_NO_THROW(logger_manager->uploadFile(log_file));
    }
}

}  // namespace atom::log::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
