/*
 * test_common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Common Test Infrastructure and Utilities for Atom Test Suite
Provides shared test utilities, fixtures, macros, and helper functions
that can be used across all test modules to ensure consistency.

**************************************************/

#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <fstream>
#include <filesystem>

namespace atom::test {

// ============================================================================
// Test Configuration Constants
// ============================================================================

constexpr size_t DEFAULT_TEST_TIMEOUT_MS = 5000;
constexpr size_t PERFORMANCE_TEST_ITERATIONS = 1000;
constexpr size_t STRESS_TEST_ITERATIONS = 10000;
constexpr size_t DEFAULT_THREAD_COUNT = 4;

// ============================================================================
// Test Data Generators
// ============================================================================

class TestDataGenerator {
public:
    static std::string generateRandomString(size_t length) {
        const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, charset.size() - 1);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += charset[dis(gen)];
        }
        return result;
    }
    
    static std::vector<uint8_t> generateRandomBytes(size_t count) {
        std::vector<uint8_t> result(count);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(0, 255);
        
        for (auto& byte : result) {
            byte = dis(gen);
        }
        return result;
    }
    
    static std::vector<int> generateRandomIntegers(size_t count, int min = 0, int max = 1000) {
        std::vector<int> result(count);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        
        for (auto& value : result) {
            value = dis(gen);
        }
        return result;
    }
};

// ============================================================================
// Performance Testing Utilities
// ============================================================================

class PerformanceTimer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }
    
    double getElapsedMilliseconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_);
        return duration.count() / 1000.0;
    }
    
    double getElapsedSeconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
        return duration.count() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

// ============================================================================
// Memory Testing Utilities
// ============================================================================

class MemoryTracker {
public:
    static size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage tracking
        // This is a placeholder - implement platform-specific code
        return 0;
    }
    
    static bool detectMemoryLeaks() {
        // Memory leak detection logic
        // This is a placeholder - implement actual leak detection
        return false;
    }
};

// ============================================================================
// File System Test Utilities
// ============================================================================

class TestFileManager {
public:
    TestFileManager(const std::string& test_dir = "test_temp") 
        : test_directory_(test_dir) {
        createTestDirectory();
    }
    
    ~TestFileManager() {
        cleanupTestDirectory();
    }
    
    std::string createTestFile(const std::string& filename, const std::string& content = "") {
        std::string filepath = test_directory_ + "/" + filename;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
            created_files_.push_back(filepath);
        }
        return filepath;
    }
    
    std::string getTestDirectory() const {
        return test_directory_;
    }
    
    void cleanupTestDirectory() {
        try {
            if (std::filesystem::exists(test_directory_)) {
                std::filesystem::remove_all(test_directory_);
            }
        } catch (const std::exception&) {
            // Ignore cleanup errors in tests
        }
    }

private:
    void createTestDirectory() {
        try {
            std::filesystem::create_directories(test_directory_);
        } catch (const std::exception&) {
            // Handle directory creation failure
        }
    }
    
    std::string test_directory_;
    std::vector<std::string> created_files_;
};

// ============================================================================
// Thread Testing Utilities
// ============================================================================

class ThreadTestHelper {
public:
    template<typename Func>
    static void runConcurrentTest(Func&& func, size_t thread_count = DEFAULT_THREAD_COUNT) {
        std::vector<std::thread> threads;
        std::vector<std::exception_ptr> exceptions(thread_count);
        
        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&func, &exceptions, i]() {
                try {
                    func();
                } catch (...) {
                    exceptions[i] = std::current_exception();
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Check for exceptions
        for (const auto& exception : exceptions) {
            if (exception) {
                std::rethrow_exception(exception);
            }
        }
    }
    
    static void sleep(size_t milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
};

// ============================================================================
// Test Macros
// ============================================================================

#define EXPECT_PERFORMANCE_BETTER_THAN(code, max_time_ms) \
    do { \
        atom::test::PerformanceTimer timer; \
        timer.start(); \
        { code } \
        timer.stop(); \
        EXPECT_LT(timer.getElapsedMilliseconds(), max_time_ms) \
            << "Performance test failed: took " << timer.getElapsedMilliseconds() \
            << "ms, expected less than " << max_time_ms << "ms"; \
    } while(0)

#define EXPECT_NO_MEMORY_LEAKS(code) \
    do { \
        size_t initial_memory = atom::test::MemoryTracker::getCurrentMemoryUsage(); \
        { code } \
        size_t final_memory = atom::test::MemoryTracker::getCurrentMemoryUsage(); \
        EXPECT_FALSE(atom::test::MemoryTracker::detectMemoryLeaks()) \
            << "Memory leak detected"; \
    } while(0)

#define EXPECT_THREAD_SAFE(code, thread_count) \
    do { \
        EXPECT_NO_THROW({ \
            atom::test::ThreadTestHelper::runConcurrentTest([&]() { \
                code \
            }, thread_count); \
        }) << "Thread safety test failed"; \
    } while(0)

// ============================================================================
// Common Test Fixtures
// ============================================================================

class AtomTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all Atom tests
        file_manager_ = std::make_unique<TestFileManager>();
        timer_ = std::make_unique<PerformanceTimer>();
    }
    
    void TearDown() override {
        // Common cleanup for all Atom tests
        file_manager_.reset();
        timer_.reset();
    }
    
    std::unique_ptr<TestFileManager> file_manager_;
    std::unique_ptr<PerformanceTimer> timer_;
};

} // namespace atom::test
