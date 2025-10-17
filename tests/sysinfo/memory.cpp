#include "atom/sysinfo/memory.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Basic Memory Tests
// ============================================================================

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup memory tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(MemoryTest, GetTotalMemorySize) {
    // Test getting total memory size
    unsigned long long totalMemory = getTotalMemorySize();

    // Total memory should be positive
    EXPECT_GT(totalMemory, 0);

    // Should be at least 512MB (reasonable minimum)
    EXPECT_GT(totalMemory, 512ULL * 1024 * 1024);

    // Should be less than 1TB (reasonable maximum for most systems)
    EXPECT_LT(totalMemory, 1024ULL * 1024 * 1024 * 1024);
}

TEST_F(MemoryTest, GetAvailableMemorySize) {
    // Test getting available memory size
    unsigned long long availableMemory = getAvailableMemorySize();
    unsigned long long totalMemory = getTotalMemorySize();

    // Available memory should be non-negative
    EXPECT_GE(availableMemory, 0);

    // Available memory should not exceed total memory
    EXPECT_LE(availableMemory, totalMemory);
}

TEST_F(MemoryTest, GetMemoryUsage) {
    // Test getting memory usage percentage
    float memoryUsage = getMemoryUsage();

    // Memory usage should be between 0 and 100
    EXPECT_GE(memoryUsage, 0.0f);
    EXPECT_LE(memoryUsage, 100.0f);
}

TEST_F(MemoryTest, GetDetailedMemoryStats) {
    // Test getting detailed memory statistics
    MemoryInfo memInfo = getDetailedMemoryStats();

    // Validate basic fields
    EXPECT_GT(memInfo.totalPhysicalMemory, 0);
    EXPECT_GE(memInfo.availablePhysicalMemory, 0);
    EXPECT_LE(memInfo.availablePhysicalMemory, memInfo.totalPhysicalMemory);

    // Memory load percentage should be valid
    EXPECT_GE(memInfo.memoryLoadPercentage, 0.0f);
    EXPECT_LE(memInfo.memoryLoadPercentage, 100.0f);

    // Virtual memory should be reasonable
    EXPECT_GT(memInfo.virtualMemoryMax, 0);
    EXPECT_GE(memInfo.virtualMemoryUsed, 0);
    EXPECT_LE(memInfo.virtualMemoryUsed, memInfo.virtualMemoryMax);

    // Swap memory should be non-negative
    EXPECT_GE(memInfo.swapMemoryTotal, 0);
    EXPECT_GE(memInfo.swapMemoryUsed, 0);
    EXPECT_LE(memInfo.swapMemoryUsed, memInfo.swapMemoryTotal);
}

TEST_F(MemoryTest, GetVirtualMemoryMax) {
    // Test getting virtual memory maximum
    unsigned long long virtualMax = getVirtualMemoryMax();
    unsigned long long totalPhysical = getTotalMemorySize();

    // Virtual memory should be positive
    EXPECT_GT(virtualMax, 0);

    // Virtual memory should typically be >= physical memory
    EXPECT_GE(virtualMax, totalPhysical);
}

TEST_F(MemoryTest, GetVirtualMemoryUsed) {
    // Test getting virtual memory used
    unsigned long long virtualUsed = getVirtualMemoryUsed();
    unsigned long long virtualMax = getVirtualMemoryMax();

    // Virtual memory used should be non-negative
    EXPECT_GE(virtualUsed, 0);

    // Virtual memory used should not exceed maximum
    EXPECT_LE(virtualUsed, virtualMax);
}

TEST_F(MemoryTest, GetSwapMemoryTotal) {
    // Test getting swap memory total
    unsigned long long swapTotal = getSwapMemoryTotal();

    // Swap memory should be non-negative (can be 0 on some systems)
    EXPECT_GE(swapTotal, 0);
}

TEST_F(MemoryTest, GetSwapMemoryUsed) {
    // Test getting swap memory used
    unsigned long long swapUsed = getSwapMemoryUsed();
    unsigned long long swapTotal = getSwapMemoryTotal();

    // Swap memory used should be non-negative
    EXPECT_GE(swapUsed, 0);

    // Swap memory used should not exceed total
    EXPECT_LE(swapUsed, swapTotal);
}

TEST_F(MemoryTest, GetCommittedMemory) {
    // Test getting committed memory
    unsigned long long committed = getCommittedMemory();

    // Committed memory should be positive
    EXPECT_GT(committed, 0);

    // Committed memory should be reasonable
    unsigned long long totalMemory = getTotalMemorySize();
    EXPECT_LT(committed, totalMemory * 10); // Allow for overcommit
}

// ============================================================================
// Real System Tests
// ============================================================================

class RealMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup real memory tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(RealMemoryTest, MemoryConsistency) {
    // Test memory consistency across multiple calls
    MemoryInfo info1 = getDetailedMemoryStats();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    MemoryInfo info2 = getDetailedMemoryStats();

    // Static values should be the same
    EXPECT_EQ(info1.totalPhysicalMemory, info2.totalPhysicalMemory);
    EXPECT_EQ(info1.virtualMemoryMax, info2.virtualMemoryMax);
    EXPECT_EQ(info1.swapMemoryTotal, info2.swapMemoryTotal);

    // Dynamic values should be close
    float availableDiff = std::abs(static_cast<float>(info1.availablePhysicalMemory) -
                                 static_cast<float>(info2.availablePhysicalMemory));
    float tolerance = static_cast<float>(info1.totalPhysicalMemory) * 0.1f; // 10% tolerance
    EXPECT_LT(availableDiff, tolerance);
}

TEST_F(RealMemoryTest, MemoryUsageMonitoring) {
    // Test memory usage monitoring over time
    std::vector<float> usageHistory;

    for (int i = 0; i < 5; ++i) {
        float usage = getMemoryUsage();
        usageHistory.push_back(usage);

        // Each measurement should be valid
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Should have collected all measurements
    EXPECT_EQ(usageHistory.size(), 5);

    // Memory usage should be relatively stable over short periods
    for (size_t i = 1; i < usageHistory.size(); ++i) {
        float diff = std::abs(usageHistory[i] - usageHistory[i-1]);
        EXPECT_LT(diff, 50.0f); // Should not change by more than 50% quickly
    }
}

TEST_F(RealMemoryTest, MemoryCalculations) {
    // Test memory calculations and relationships
    MemoryInfo memInfo = getDetailedMemoryStats();

    // Used memory calculation
    unsigned long long usedMemory = memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory;
    EXPECT_LE(usedMemory, memInfo.totalPhysicalMemory);

    // Memory load percentage should match calculation
    float calculatedLoad = (static_cast<float>(usedMemory) / memInfo.totalPhysicalMemory) * 100.0f;
    EXPECT_NEAR(memInfo.memoryLoadPercentage, calculatedLoad, 5.0f); // 5% tolerance

    // Virtual memory relationships
    EXPECT_GE(memInfo.virtualMemoryMax, memInfo.totalPhysicalMemory);
    EXPECT_LE(memInfo.virtualMemoryUsed, memInfo.virtualMemoryMax);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(RealMemoryTest, NoThrowGuarantee) {
    // Test that all memory functions provide no-throw guarantee
    EXPECT_NO_THROW(getTotalMemorySize());
    EXPECT_NO_THROW(getAvailableMemorySize());
    EXPECT_NO_THROW(getMemoryUsage());
    EXPECT_NO_THROW(getDetailedMemoryStats());
    EXPECT_NO_THROW(getVirtualMemoryMax());
    EXPECT_NO_THROW(getVirtualMemoryUsed());
    EXPECT_NO_THROW(getSwapMemoryTotal());
    EXPECT_NO_THROW(getSwapMemoryUsed());
    EXPECT_NO_THROW(getCommittedMemory());
}

TEST_F(RealMemoryTest, BoundaryConditions) {
    // Test memory functions with boundary conditions

    // All memory sizes should be reasonable
    unsigned long long totalMemory = getTotalMemorySize();
    unsigned long long availableMemory = getAvailableMemorySize();
    unsigned long long virtualMax = getVirtualMemoryMax();
    unsigned long long virtualUsed = getVirtualMemoryUsed();
    unsigned long long swapTotal = getSwapMemoryTotal();
    unsigned long long swapUsed = getSwapMemoryUsed();
    unsigned long long committed = getCommittedMemory();

    // Test upper bounds (reasonable maximums)
    EXPECT_LT(totalMemory, 10000ULL * 1024 * 1024 * 1024); // Less than 10TB
    EXPECT_LT(virtualMax, 100000ULL * 1024 * 1024 * 1024); // Less than 100TB
    EXPECT_LT(committed, 10000ULL * 1024 * 1024 * 1024); // Less than 10TB

    // Test relationships
    EXPECT_LE(availableMemory, totalMemory);
    EXPECT_LE(virtualUsed, virtualMax);
    EXPECT_LE(swapUsed, swapTotal);

    // Memory usage percentage should be reasonable
    float memoryUsage = getMemoryUsage();
    EXPECT_GE(memoryUsage, 0.0f);
    EXPECT_LE(memoryUsage, 100.0f);
}

} // namespace atom::sysinfo::test
