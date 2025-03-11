#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/tracker.hpp"

using namespace atom::memory;

// Mock implementation of StackTrace since we might not have access to the real
// one during testing
namespace atom::error {
#ifndef ATOM_ERROR_STACKTRACE_HPP
class StackTrace {
public:
    std::string toString() const {
        return "Frame 1: test_function()\nFrame 2: main()\n";
    }
};
#endif
}  // namespace atom::error

// Test fixture for MemoryTracker tests
class MemoryTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the memory tracker before each test
        MemoryTracker::instance().reset();

        // Configure with default settings but disable console output
        MemoryTrackerConfig config;
        config.logToConsole = false;
        config.logFilePath = "memory_tracker_test.log";
        MemoryTracker::instance().initialize(config);
    }

    void TearDown() override {
        // Clean up any test log file
        std::remove("memory_tracker_test.log");
    }

    // Helper method to allocate memory and register it
    void* allocateAndRegister(size_t size, const char* file = "test.cpp",
                              int line = 42) {
        void* ptr = malloc(size);
        MemoryTracker::instance().registerAllocation(ptr, size, file, line,
                                                     "test_function");
        return ptr;
    }

    // Helper method to check if a log file contains a specific string
    bool logFileContains(const std::string& filename, const std::string& text) {
        std::ifstream file(filename);
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// Test basic allocation and deallocation tracking
TEST_F(MemoryTrackerTest, BasicAllocationTracking) {
    void* ptr = allocateAndRegister(100);

    // Log should contain allocation info
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "ALLOC"));
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "100 bytes"));

    // Statistics should be updated
    MemoryTracker::instance().registerDeallocation(ptr);

    // Log should contain deallocation info
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "FREE"));

    free(ptr);
}

// Test tracking with source information
TEST_F(MemoryTrackerTest, SourceInfoTracking) {
    void* ptr = allocateAndRegister(100, "source_file.cpp", 123);

    // Log should contain source info
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "source_file.cpp:123"));

    MemoryTracker::instance().registerDeallocation(ptr);
    free(ptr);
}

// Test memory leak detection
TEST_F(MemoryTrackerTest, MemoryLeakDetection) {
    void* ptr1 = allocateAndRegister(100);
    void* ptr2 = allocateAndRegister(200);

    // Free only one of the pointers
    MemoryTracker::instance().registerDeallocation(ptr1);
    free(ptr1);

    // Report leaks - ptr2 should still be tracked as a leak
    MemoryTracker::instance().reportLeaks();

    // Log should contain leak info
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Detected 1 memory leaks"));
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "200 bytes"));

    // Clean up
    MemoryTracker::instance().registerDeallocation(ptr2);
    free(ptr2);
}

// Test memory statistics tracking
TEST_F(MemoryTrackerTest, MemoryStatistics) {
    // Allocate and register multiple blocks
    void* ptr1 = allocateAndRegister(100);
    void* ptr2 = allocateAndRegister(200);
    void* ptr3 = allocateAndRegister(300);

    // Free one block
    MemoryTracker::instance().registerDeallocation(ptr1);
    free(ptr1);

    // Report leaks should show statistics
    MemoryTracker::instance().reportLeaks();

    // Check for statistics in log
    EXPECT_TRUE(logFileContains("memory_tracker_test.log",
                                "Total allocations:       3"));
    EXPECT_TRUE(logFileContains("memory_tracker_test.log",
                                "Total deallocations:     1"));
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log",
                        "Peak memory usage:       600"));  // 100 + 200 + 300
    EXPECT_TRUE(logFileContains("memory_tracker_test.log",
                                "Largest single alloc:    300"));

    // Clean up
    MemoryTracker::instance().registerDeallocation(ptr2);
    MemoryTracker::instance().registerDeallocation(ptr3);
    free(ptr2);
    free(ptr3);
}

// Test resetting the tracker
TEST_F(MemoryTrackerTest, ResetTracker) {
    // Allocate and register a block
    void* ptr = allocateAndRegister(100);

    // Reset the tracker
    MemoryTracker::instance().reset();

    // Report leaks should show no leaks
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "No memory leaks detected"));

    // Check that stats were reset
    EXPECT_TRUE(logFileContains("memory_tracker_test.log",
                                "Total allocations:       0"));

    // Clean up (even though tracker doesn't track it anymore)
    free(ptr);
}

// Test tracking with stack trace
TEST_F(MemoryTrackerTest, StackTraceTracking) {
    // Configure tracker to track stack traces
    MemoryTrackerConfig config;
    config.logToConsole = false;
    config.trackStackTrace = true;
    config.logFilePath = "memory_tracker_test.log";
    MemoryTracker::instance().initialize(config);

    // Allocate and register a block
    void* ptr = allocateAndRegister(100);

    // Report leaks
    MemoryTracker::instance().reportLeaks();

    // Check that stack trace was included
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "Stack trace:"));
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "Frame 1"));

    // Clean up
    MemoryTracker::instance().registerDeallocation(ptr);
    free(ptr);
}

// Test minimum allocation size filter
TEST_F(MemoryTrackerTest, MinAllocationSizeFilter) {
    // Configure tracker with minimum allocation size
    MemoryTrackerConfig config;
    config.logToConsole = false;
    config.minAllocationSize = 150;
    config.logFilePath = "memory_tracker_test.log";
    MemoryTracker::instance().initialize(config);

    // Allocate and register blocks of different sizes
    void* small_ptr = allocateAndRegister(100);  // Below minimum
    void* large_ptr = allocateAndRegister(200);  // Above minimum

    // Report leaks
    MemoryTracker::instance().reportLeaks();

    // Check that only large allocation was tracked
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Detected 1 memory leaks"));
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "200 bytes"));

    // Clean up
    free(small_ptr);  // Not tracked, but still need to free
    MemoryTracker::instance().registerDeallocation(large_ptr);
    free(large_ptr);
}

// Test disabling the tracker
TEST_F(MemoryTrackerTest, DisableTracker) {
    // Configure tracker to be disabled
    MemoryTrackerConfig config;
    config.enabled = false;
    MemoryTracker::instance().initialize(config);

    // Allocate and register a block
    void* ptr = malloc(100);
    MemoryTracker::instance().registerAllocation(ptr, 100);

    // Report leaks
    MemoryTracker::instance().reportLeaks();

    // No leaks should be reported because tracking is disabled
    EXPECT_FALSE(
        logFileContains("memory_tracker_test.log", "Detected 1 memory leaks"));

    // Clean up
    free(ptr);
}

// Test custom error callback
TEST_F(MemoryTrackerTest, ErrorCallback) {
    std::string lastError;

    // Configure tracker with custom error callback
    MemoryTrackerConfig config;
    config.logToConsole = false;
    config.logFilePath =
        "invalid/path/that/will/fail.log";  // Invalid path to trigger an error
    config.errorCallback = [&lastError](const std::string& error) {
        lastError = error;
    };

    MemoryTracker::instance().initialize(config);

    // Error callback should have been called due to invalid log path
    EXPECT_FALSE(lastError.empty());
    EXPECT_TRUE(lastError.find("Failed to open log file") != std::string::npos);
}

// Test thread safety
TEST_F(MemoryTrackerTest, ThreadSafety) {
    constexpr int numThreads = 5;
    constexpr int allocsPerThread = 10;

    // Function for thread to allocate and free memory
    auto threadFunc = [](int threadId) {
        std::vector<void*> ptrs;

        // Allocate several blocks
        for (int i = 0; i < allocsPerThread; ++i) {
            void* ptr = malloc(100);
            MemoryTracker::instance().registerAllocation(
                ptr, 100, "thread_test.cpp", threadId * 1000 + i,
                "thread_func");
            ptrs.push_back(ptr);

            // Add some random delay
            std::this_thread::sleep_for(
                std::chrono::microseconds(rand() % 1000));
        }

        // Free half the blocks
        for (int i = 0; i < allocsPerThread / 2; ++i) {
            MemoryTracker::instance().registerDeallocation(ptrs[i]);
            free(ptrs[i]);

            // Add some random delay
            std::this_thread::sleep_for(
                std::chrono::microseconds(rand() % 1000));
        }

        // Return the remaining pointers for cleanup
        std::vector<void*> remainingPtrs;
        for (int i = allocsPerThread / 2; i < allocsPerThread; ++i) {
            remainingPtrs.push_back(ptrs[i]);
        }
        return remainingPtrs;
    };

    // Launch threads
    std::vector<std::future<std::vector<void*>>> futures;
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, threadFunc, i));
    }

    // Collect results and remaining pointers
    std::vector<void*> remainingPtrs;
    for (auto& future : futures) {
        auto ptrs = future.get();
        remainingPtrs.insert(remainingPtrs.end(), ptrs.begin(), ptrs.end());
    }

    // Report leaks
    MemoryTracker::instance().reportLeaks();

    // Should have detected the remaining half of allocations as leaks
    int expectedLeaks = numThreads * allocsPerThread / 2;
    EXPECT_TRUE(logFileContains(
        "memory_tracker_test.log",
        "Detected " + std::to_string(expectedLeaks) + " memory leaks"));

    // Clean up remaining pointers
    for (void* ptr : remainingPtrs) {
        MemoryTracker::instance().registerDeallocation(ptr);
        free(ptr);
    }
}

// Test handling of double-free and invalid free
TEST_F(MemoryTrackerTest, InvalidFreeDetection) {
    // Allocate and register a block
    void* ptr = allocateAndRegister(100);

    // Free it once (valid)
    MemoryTracker::instance().registerDeallocation(ptr);

    // Try to free it again (invalid)
    MemoryTracker::instance().registerDeallocation(ptr);

    // Log should contain warning about trying to free untracked memory
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log",
                        "WARNING: Attempting to free untracked memory"));

    // Also try with a completely invalid pointer
    MemoryTracker::instance().registerDeallocation((void*)0x12345);
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log",
                        "WARNING: Attempting to free untracked memory"));

    // Clean up
    free(ptr);
}

// Test the tracking of peak memory usage
TEST_F(MemoryTrackerTest, PeakMemoryUsage) {
    // Allocate blocks in a pattern to create a peak
    void* ptr1 = allocateAndRegister(100);
    void* ptr2 = allocateAndRegister(200);
    void* ptr3 = allocateAndRegister(300);  // Peak should be 600 here

    // Free one block
    MemoryTracker::instance().registerDeallocation(ptr1);
    free(ptr1);

    // Allocate a smaller block
    void* ptr4 = allocateAndRegister(50);  // Current usage 550, peak still 600

    // Report leaks
    MemoryTracker::instance().reportLeaks();

    // Check peak memory usage
    EXPECT_TRUE(logFileContains("memory_tracker_test.log",
                                "Peak memory usage:       600"));

    // Clean up
    MemoryTracker::instance().registerDeallocation(ptr2);
    MemoryTracker::instance().registerDeallocation(ptr3);
    MemoryTracker::instance().registerDeallocation(ptr4);
    free(ptr2);
    free(ptr3);
    free(ptr4);
}

// Test the MemoryStatistics operators
TEST_F(MemoryTrackerTest, MemoryStatisticsOperators) {
    MemoryStatistics stats1;
    stats1.currentAllocations = 10;
    stats1.currentMemoryUsage = 1000;
    stats1.peakMemoryUsage = 2000;

    MemoryStatistics stats2;
    stats2.currentAllocations = 5;
    stats2.currentMemoryUsage = 500;
    stats2.peakMemoryUsage = 1500;

    // Test addition operator
    stats1 += stats2;
    EXPECT_EQ(stats1.currentAllocations, 15);
    EXPECT_EQ(stats1.currentMemoryUsage, 1500);
    EXPECT_EQ(stats1.peakMemoryUsage, 2000);  // Should take the maximum

    // Test assignment operator
    MemoryStatistics stats3;
    stats3 = stats1;
    EXPECT_EQ(stats3.currentAllocations, stats1.currentAllocations);
    EXPECT_EQ(stats3.currentMemoryUsage, stats1.currentMemoryUsage);
    EXPECT_EQ(stats3.peakMemoryUsage, stats1.peakMemoryUsage);

    // Test equality operators
    EXPECT_TRUE(stats3 == stats1);
    EXPECT_FALSE(stats3 != stats1);
    EXPECT_FALSE(stats3 == stats2);
    EXPECT_TRUE(stats3 != stats2);
}

// Test custom configuration settings
TEST_F(MemoryTrackerTest, CustomConfiguration) {
    // Configure tracker with various settings
    MemoryTrackerConfig config;
    config.logToConsole = false;
    config.trackStackTrace = false;
    config.maxStackFrames = 5;
    config.autoReportLeaks = false;
    config.logFilePath = "memory_tracker_test.log";
    MemoryTracker::instance().initialize(config);

    // Verify configuration in log
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Track Stack Trace: No"));
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Auto Report Leaks: No"));
}

// Test the ATOM_TRACK macros
TEST_F(MemoryTrackerTest, TrackingMacros) {
#ifdef ATOM_MEMORY_TRACKING_ENABLED
    // Allocate memory
    void* ptr = malloc(100);

    // Use the tracking macros
    ATOM_TRACK_ALLOC(ptr, 100);

    // Report leaks
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Detected 1 memory leaks"));

    // Free memory
    ATOM_TRACK_FREE(ptr);
    free(ptr);

    // Report leaks again
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "No memory leaks detected"));
#else
    // If tracking is disabled, the test is trivially successful
    SUCCEED() << "ATOM_MEMORY_TRACKING_ENABLED not defined, skipping test";
#endif
}

// If tracking is enabled in the build, test the operator new/delete overloads
TEST_F(MemoryTrackerTest, OperatorOverloads) {
#ifdef ATOM_MEMORY_TRACKING_ENABLED
    // Reset tracker for clean state
    MemoryTracker::instance().reset();

    // Use new/delete which should automatically track
    int* p1 = new int(42);
    int* p2 = new int[10];

    // Report leaks
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Detected 2 memory leaks"));

    // Clean up
    delete p1;
    delete[] p2;

    // Report leaks again
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "No memory leaks detected"));

    // Test nothrow versions
    int* p3 = new (std::nothrow) int(42);
    int* p4 = new (std::nothrow) int[10];

    // Report leaks
    MemoryTracker::instance().reportLeaks();
    EXPECT_TRUE(
        logFileContains("memory_tracker_test.log", "Detected 2 memory leaks"));

    // Clean up
    delete p3;
    delete[] p4;
#else
    // If tracking is disabled, the test is trivially successful
    SUCCEED() << "ATOM_MEMORY_TRACKING_ENABLED not defined, skipping test";
#endif
}

// Test the pointer to string conversion
TEST_F(MemoryTrackerTest, PointerToString) {
    void* ptr = allocateAndRegister(100);

    // Report leaks to get pointer string in log
    MemoryTracker::instance().reportLeaks();

    // Log should contain pointer in hex format like 0x00a1b2c3
    EXPECT_TRUE(logFileContains("memory_tracker_test.log", "0x"));

    // Clean up
    MemoryTracker::instance().registerDeallocation(ptr);
    free(ptr);
}

// Test handling of large allocations that could overflow stats
TEST_F(MemoryTrackerTest, LargeAllocations) {
    // Only allocate if we're on a 64-bit platform where this is safe
    if (sizeof(size_t) >= 8) {
        // Try a very large allocation (but don't actually allocate)
        size_t largeSize = (size_t)1 << 40;  // 1 TB
        MemoryTracker::instance().registerAllocation((void*)0x12345, largeSize);

        // Report leaks
        MemoryTracker::instance().reportLeaks();

        // Largest allocation should be updated
        EXPECT_TRUE(logFileContains(
            "memory_tracker_test.log",
            "Largest single alloc:    " + std::to_string(largeSize)));

        // Clean up
        MemoryTracker::instance().registerDeallocation((void*)0x12345);
    } else {
        SUCCEED() << "Skipping large allocation test on 32-bit platform";
    }
}
