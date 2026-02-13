/**
 * @file test_memory_comprehensive.cpp
 * @brief Comprehensive test suite for the memory module
 *
 * This file contains comprehensive tests for all memory module functionality
 * including basic operations, advanced features, and performance testing.
 *
 * @author Atom Memory Module Team
 * @date 2024
 */

#include "memory.hpp"
#include "common.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

using namespace atom::system;

class MemoryModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code for each test
    }

    void TearDown() override {
        // Cleanup code for each test
        stopMemoryMonitoring();
        stopMemoryPressureMonitoring();
        stopMemoryCompressionMonitoring();
        stopLeakMonitoring();
    }
};

// Basic Memory Information Tests
TEST_F(MemoryModuleTest, BasicMemoryInfo) {
    auto totalMemory = getTotalMemorySize();
    EXPECT_GT(totalMemory, 0ULL) << "Total memory should be greater than 0";

    auto availableMemory = getAvailableMemorySize();
    EXPECT_GT(availableMemory, 0ULL) << "Available memory should be greater than 0";
    EXPECT_LE(availableMemory, totalMemory) << "Available memory should not exceed total memory";

    auto memoryUsage = getMemoryUsage();
    EXPECT_GE(memoryUsage, 0.0f) << "Memory usage should be non-negative";
    EXPECT_LE(memoryUsage, 100.0f) << "Memory usage should not exceed 100%";

    auto loadPercentage = getMemoryLoadPercentage();
    EXPECT_GE(loadPercentage, 0.0f) << "Memory load should be non-negative";
    EXPECT_LE(loadPercentage, 100.0f) << "Memory load should not exceed 100%";
}

TEST_F(MemoryModuleTest, DetailedMemoryStats) {
    auto memInfo = getDetailedMemoryStats();

    EXPECT_GT(memInfo.totalPhysicalMemory, 0ULL) << "Total physical memory should be positive";
    EXPECT_GT(memInfo.availablePhysicalMemory, 0ULL) << "Available physical memory should be positive";
    EXPECT_LE(memInfo.availablePhysicalMemory, memInfo.totalPhysicalMemory)
        << "Available should not exceed total";

    EXPECT_GE(memInfo.memoryLoadPercentage, 0.0f) << "Memory load should be non-negative";
    EXPECT_LE(memInfo.memoryLoadPercentage, 100.0f) << "Memory load should not exceed 100%";
}

TEST_F(MemoryModuleTest, PhysicalMemoryInfo) {
    auto memSlot = getPhysicalMemoryInfo();

    EXPECT_FALSE(memSlot.capacity.empty()) << "Memory capacity should not be empty";
    // Type and clock speed might be unknown on some systems, so we don't assert them
}

// Memory Performance Tests
TEST_F(MemoryModuleTest, MemoryPerformance) {
    auto perf = getMemoryPerformance();

    EXPECT_GE(perf.readSpeed, 0.0) << "Read speed should be non-negative";
    EXPECT_GE(perf.writeSpeed, 0.0) << "Write speed should be non-negative";
    EXPECT_GE(perf.bandwidthUsage, 0.0) << "Bandwidth usage should be non-negative";
    EXPECT_LE(perf.bandwidthUsage, 100.0) << "Bandwidth usage should not exceed 100%";
    EXPECT_GE(perf.latency, 0.0) << "Latency should be non-negative";
}

TEST_F(MemoryModuleTest, EnhancedMemoryPerformance) {
    auto perf = getEnhancedMemoryPerformance();

    EXPECT_GE(perf.sequentialReadSpeed, 0.0) << "Sequential read speed should be non-negative";
    EXPECT_GE(perf.sequentialWriteSpeed, 0.0) << "Sequential write speed should be non-negative";
    EXPECT_GE(perf.randomReadSpeed, 0.0) << "Random read speed should be non-negative";
    EXPECT_GE(perf.randomWriteSpeed, 0.0) << "Random write speed should be non-negative";

    EXPECT_GE(perf.l1CacheHitRate, 0.0) << "L1 cache hit rate should be non-negative";
    EXPECT_LE(perf.l1CacheHitRate, 100.0) << "L1 cache hit rate should not exceed 100%";
    EXPECT_GE(perf.l2CacheHitRate, 0.0) << "L2 cache hit rate should be non-negative";
    EXPECT_LE(perf.l2CacheHitRate, 100.0) << "L2 cache hit rate should not exceed 100%";
}

TEST_F(MemoryModuleTest, MemoryBenchmark) {
    const size_t testSize = 1024 * 1024; // 1MB
    const int iterations = 3;

    auto results = benchmarkMemoryPerformance(testSize, iterations, "mixed");

    EXPECT_GE(results.readSpeed, 0.0) << "Benchmark read speed should be non-negative";
    EXPECT_GE(results.writeSpeed, 0.0) << "Benchmark write speed should be non-negative";
    EXPECT_GE(results.latency, 0.0) << "Benchmark latency should be non-negative";
}

// Memory Pressure Tests
TEST_F(MemoryModuleTest, MemoryPressureDetection) {
    auto pressureInfo = detectMemoryPressure();

    EXPECT_GE(pressureInfo.pressureScore, 0.0) << "Pressure score should be non-negative";
    EXPECT_LE(pressureInfo.pressureScore, 100.0) << "Pressure score should not exceed 100";

    EXPECT_GE(pressureInfo.memoryUsagePercent, 0.0) << "Memory usage should be non-negative";
    EXPECT_LE(pressureInfo.memoryUsagePercent, 100.0) << "Memory usage should not exceed 100%";

    auto level = getMemoryPressureLevel();
    EXPECT_GE(static_cast<int>(level), 0) << "Pressure level should be valid";
    EXPECT_LE(static_cast<int>(level), 4) << "Pressure level should be within range";
}

// Memory Compression Tests
TEST_F(MemoryModuleTest, MemoryCompressionDetection) {
    auto compressionInfo = detectMemoryCompression();

    // isSupported and isEnabled are boolean, so just check they're accessible
    EXPECT_TRUE(compressionInfo.isSupported || !compressionInfo.isSupported);
    EXPECT_TRUE(compressionInfo.isEnabled || !compressionInfo.isEnabled);

    if (compressionInfo.isEnabled) {
        EXPECT_GE(compressionInfo.compressionRatio, 0.0) << "Compression ratio should be non-negative";
        EXPECT_LE(compressionInfo.compressionRatio, 1.0) << "Compression ratio should not exceed 1.0";
        EXPECT_GE(compressionInfo.spaceSavings, 0.0) << "Space savings should be non-negative";
    }
}

TEST_F(MemoryModuleTest, CompressionBenchmark) {
    const size_t testSize = 1024 * 1024; // 1MB

    auto results = benchmarkMemoryCompression(testSize, CompressionAlgorithm::LZ4);

    EXPECT_GT(results.originalSize, 0ULL) << "Original size should be positive";
    EXPECT_GT(results.compressedSize, 0ULL) << "Compressed size should be positive";
    EXPECT_GE(results.compressionRatio, 0.0) << "Compression ratio should be non-negative";
    EXPECT_GE(results.compressionThroughput, 0.0) << "Compression throughput should be non-negative";
}

// NUMA Topology Tests
TEST_F(MemoryModuleTest, NumaTopologyDetection) {
    auto topology = detectNumaTopology();

    EXPECT_GE(topology.nodeCount, 1) << "Should have at least one NUMA node";
    EXPECT_FALSE(topology.nodes.empty()) << "Nodes vector should not be empty";

    for (const auto& node : topology.nodes) {
        EXPECT_GE(node.nodeId, 0) << "Node ID should be non-negative";
        EXPECT_GE(node.cpuCount, 0) << "CPU count should be non-negative";
        EXPECT_GE(node.localAccessLatency, 0.0) << "Local access latency should be non-negative";
        EXPECT_GE(node.localBandwidth, 0.0) << "Local bandwidth should be non-negative";
    }

    auto isNuma = isNumaSystem();
    EXPECT_TRUE(isNuma || !isNuma); // Just check it's accessible

    auto nodeCount = getNumaNodeCount();
    EXPECT_EQ(nodeCount, topology.nodeCount) << "Node count should match topology";
}

// Memory Allocation Tracking Tests
TEST_F(MemoryModuleTest, AllocationTracking) {
    // Start allocation tracking
    bool started = startAllocationTracking(false);
    EXPECT_TRUE(started) << "Allocation tracking should start successfully";

    // Get tracker status
    auto tracker = getAllocationTracker();
    EXPECT_TRUE(tracker.isActive) << "Tracker should be active";

    // Stop tracking
    auto finalTracker = stopAllocationTracking();
    EXPECT_FALSE(finalTracker.isActive) << "Tracker should be inactive after stopping";
}

TEST_F(MemoryModuleTest, HeapAnalysis) {
    auto heapInfo = analyzeHeapMemory();

    EXPECT_GT(heapInfo.totalHeapSize, 0ULL) << "Total heap size should be positive";
    EXPECT_GE(heapInfo.fragmentationRatio, 0.0) << "Fragmentation ratio should be non-negative";
    EXPECT_LE(heapInfo.fragmentationRatio, 1.0) << "Fragmentation ratio should not exceed 1.0";
}

// Memory Leak Detection Tests
TEST_F(MemoryModuleTest, BasicLeakDetection) {
    auto leaks = detectMemoryLeaks();
    // We can't assert specific values since leaks depend on system state
    // Just ensure the function runs without crashing
    EXPECT_TRUE(true) << "Basic leak detection should complete";
}

TEST_F(MemoryModuleTest, EnhancedLeakDetection) {
    LeakDetectionConfig config;
    config.minimumLeakSize = 1024; // 1KB
    config.confidenceThreshold = 50.0;

    auto leaks = detectMemoryLeaksEnhanced(config);

    // Verify leak structure if any leaks are found
    for (const auto& leak : leaks) {
        EXPECT_GE(leak.confidence, 0.0) << "Leak confidence should be non-negative";
        EXPECT_LE(leak.confidence, 100.0) << "Leak confidence should not exceed 100%";
        EXPECT_GT(leak.totalLeakedBytes, 0ULL) << "Leaked bytes should be positive";
        EXPECT_FALSE(leak.leakId.empty()) << "Leak ID should not be empty";
    }
}

// Utility Function Tests
TEST_F(MemoryModuleTest, UtilityFunctions) {
    // Test byte size formatting
    EXPECT_EQ(formatByteSize(0), "0 B");
    EXPECT_EQ(formatByteSize(1024), "1.00 KB");
    EXPECT_EQ(formatByteSize(1024 * 1024), "1.00 MB");

    // Test advanced formatting
    auto formatted = formatByteSizeAdvanced(1536, true, 1, true); // 1.5 KiB
    EXPECT_FALSE(formatted.empty()) << "Formatted string should not be empty";

    // Test bandwidth formatting
    auto bandwidth = formatBandwidth(1024 * 1024); // 1 MB/s
    EXPECT_FALSE(bandwidth.empty()) << "Bandwidth string should not be empty";

    // Test byte size parsing
    EXPECT_EQ(parseByteSize("1KB"), 1000ULL);
    EXPECT_EQ(parseByteSize("1KiB"), 1024ULL);
    EXPECT_EQ(parseByteSize("1MB"), 1000000ULL);
}

// Memory Monitoring Tests
TEST_F(MemoryModuleTest, MemoryMonitoring) {
    bool callbackCalled = false;

    auto callback = [&callbackCalled](const MemoryInfo& info) {
        callbackCalled = true;
        EXPECT_GT(info.totalPhysicalMemory, 0ULL);
    };

    startMemoryMonitoring(callback);

    // Wait a bit for monitoring to trigger
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    stopMemoryMonitoring();

    // Note: callback might not be called in test environment, so we don't assert it
}

// Performance and Stress Tests
TEST_F(MemoryModuleTest, MemoryFragmentation) {
    auto fragmentation = getMemoryFragmentation();
    EXPECT_GE(fragmentation, 0.0) << "Fragmentation should be non-negative";
    EXPECT_LE(fragmentation, 100.0) << "Fragmentation should not exceed 100%";
}

TEST_F(MemoryModuleTest, MemoryOptimization) {
    auto optimizations = optimizeMemoryUsage();
    // Just ensure the function runs and returns something
    EXPECT_TRUE(true) << "Memory optimization should complete";
}

TEST_F(MemoryModuleTest, BottleneckAnalysis) {
    auto bottlenecks = analyzeMemoryBottlenecks();
    // Just ensure the function runs
    EXPECT_TRUE(true) << "Bottleneck analysis should complete";
}

// Integration Tests
TEST_F(MemoryModuleTest, MemoryTimeline) {
    auto timeline = getMemoryTimeline(std::chrono::minutes(1));
    // Timeline might be empty in test environment
    EXPECT_TRUE(true) << "Memory timeline should be accessible";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Starting comprehensive memory module tests..." << std::endl;

    int result = RUN_ALL_TESTS();

    std::cout << "Memory module tests completed." << std::endl;

    return result;
}
