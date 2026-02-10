#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <bitset>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>
#include "atom/algorithm/snowflake.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

constexpr uint64_t TEST_EPOCH = 1577836800000;  // 2020-01-01

class SnowflakeEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    void waitForMillisecond() {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < 2ms) {
            std::this_thread::yield();
        }
    }

    // Helper to verify ID structure
    void verifyIdStructure(uint64_t id, uint64_t expectedWorkerId,
                           uint64_t expectedDatacenterId) {
        Snowflake<TEST_EPOCH> snowflake(expectedWorkerId, expectedDatacenterId);
        uint64_t timestamp, datacenterId, workerId, sequence;
        snowflake.parseId(id, timestamp, datacenterId, workerId, sequence);

        EXPECT_EQ(workerId, expectedWorkerId);
        EXPECT_EQ(datacenterId, expectedDatacenterId);
        EXPECT_GE(timestamp, TEST_EPOCH);
        EXPECT_LT(sequence, (1ULL << Snowflake<TEST_EPOCH>::SEQUENCE_BITS));
    }
};

// Test high-performance ID generation
TEST_F(SnowflakeEnhancedTest, HighPerformanceGeneration) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);
    const size_t numIds = 1000000;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::unordered_set<uint64_t> generatedIds;
    generatedIds.reserve(numIds);

    for (size_t i = 0; i < numIds; ++i) {
        auto ids = snowflake.nextid<1>();
        auto id = ids[0];

        // Verify uniqueness
        EXPECT_TRUE(generatedIds.insert(id).second)
            << "Duplicate ID generated: " << id;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    double idsPerSecond =
        static_cast<double>(numIds) / (duration.count() / 1000.0);
    spdlog::info("Generated {} IDs in {} ms ({:.0f} IDs/sec)", numIds,
                 duration.count(), idsPerSecond);

    EXPECT_EQ(generatedIds.size(), numIds);
    EXPECT_GT(idsPerSecond, 100000);  // Should generate at least 100K IDs/sec
}

// Test batch generation efficiency
TEST_F(SnowflakeEnhancedTest, BatchGenerationEfficiency) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);
    constexpr size_t BATCH_SIZE = 10000;

    // Test different batch sizes
    std::vector<size_t> batchSizes = {1, 10, 100, 1000, BATCH_SIZE};

    for (size_t batchSize : batchSizes) {
        auto startTime = std::chrono::high_resolution_clock::now();

        std::unordered_set<uint64_t> allIds;
        size_t totalGenerated = 0;

        while (totalGenerated < BATCH_SIZE) {
            size_t currentBatch =
                std::min(batchSize, BATCH_SIZE - totalGenerated);

            if (currentBatch == 1) {
                auto ids = snowflake.nextid<1>();
                allIds.insert(ids[0]);
            } else if (currentBatch == 10) {
                auto ids = snowflake.nextid<10>();
                for (auto id : ids)
                    allIds.insert(id);
            } else if (currentBatch == 100) {
                auto ids = snowflake.nextid<100>();
                for (auto id : ids)
                    allIds.insert(id);
            } else if (currentBatch == 1000) {
                auto ids = snowflake.nextid<1000>();
                for (auto id : ids)
                    allIds.insert(id);
            } else {
                auto ids = snowflake.nextid<10000>();
                for (auto id : ids)
                    allIds.insert(id);
            }

            totalGenerated += currentBatch;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);

        spdlog::info("Batch size {}: {} μs for {} IDs", batchSize,
                     duration.count(), BATCH_SIZE);
        EXPECT_EQ(allIds.size(), BATCH_SIZE);
    }
}

// Test thread safety with high contention
TEST_F(SnowflakeEnhancedTest, HighContentionThreadSafety) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(5, 10);
    const int numThreads = 16;
    const int idsPerThread = 10000;

    std::atomic<size_t> totalGenerated{0};
    std::vector<std::future<std::vector<uint64_t>>> futures;

    // Launch threads
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(
            std::launch::async, [&snowflake, idsPerThread, &totalGenerated]() {
                std::vector<uint64_t> threadIds;
                threadIds.reserve(idsPerThread);

                for (int j = 0; j < idsPerThread; ++j) {
                    auto ids = snowflake.nextid<1>();
                    threadIds.push_back(ids[0]);
                    totalGenerated.fetch_add(1, std::memory_order_relaxed);
                }

                return threadIds;
            }));
    }

    // Collect all IDs
    std::unordered_set<uint64_t> allIds;
    for (auto& future : futures) {
        auto threadIds = future.get();
        for (auto id : threadIds) {
            EXPECT_TRUE(allIds.insert(id).second) << "Duplicate ID: " << id;
        }
    }

    EXPECT_EQ(allIds.size(), numThreads * idsPerThread);
    EXPECT_EQ(totalGenerated.load(), numThreads * idsPerThread);

    // Verify all IDs are unique and non-zero
    for (auto id : allIds) {
        EXPECT_GT(id, 0u);
    }
}

// Test statistics collection
TEST_F(SnowflakeEnhancedTest, StatisticsCollection) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);

    auto initialStats = snowflake.getStatistics();
    EXPECT_EQ(initialStats.total_ids_generated, 0);

    // Generate some IDs
    const size_t numIds = 1000;
    for (size_t i = 0; i < numIds; ++i) {
        auto ids = snowflake.nextid<1>();
        (void)ids;  // Suppress unused variable warning
    }

    auto stats = snowflake.getStatistics();
    EXPECT_EQ(stats.total_ids_generated, numIds);
    EXPECT_GE(stats.sequence_rollovers, 0);
    EXPECT_GE(stats.timestamp_wait_count, 0);

    spdlog::info("Generated {} IDs, {} rollovers, {} waits",
                 stats.total_ids_generated, stats.sequence_rollovers,
                 stats.timestamp_wait_count);
}

// Test sequence rollover handling
TEST_F(SnowflakeEnhancedTest, SequenceRolloverHandling) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);

    // Generate enough IDs to potentially trigger sequence rollover
    const size_t numIds = 5000;  // More than sequence mask (4096)
    std::vector<uint64_t> ids;
    ids.reserve(numIds);

    for (size_t i = 0; i < numIds; ++i) {
        auto nextIds = snowflake.nextid<1>();
        ids.push_back(nextIds[0]);
    }

    // Verify all IDs are unique
    std::unordered_set<uint64_t> uniqueIds(ids.begin(), ids.end());
    EXPECT_EQ(uniqueIds.size(), numIds);

    // Check statistics for rollovers
    auto stats = snowflake.getStatistics();
    if (stats.sequence_rollovers > 0) {
        spdlog::info("Sequence rollovers occurred: {}",
                     stats.sequence_rollovers);
    }
}

// Test different lock types performance
TEST_F(SnowflakeEnhancedTest, LockTypePerformance) {
    const size_t numIds = 100000;

    // Test std::mutex
    {
        Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < numIds; ++i) {
            auto ids = snowflake.nextid<1>();
            (void)ids;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        spdlog::info("std::mutex: {} μs for {} IDs", duration.count(), numIds);
    }

    // Test std::mutex
    {
        Snowflake<TEST_EPOCH, std::mutex> snowflake(0, 0);
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < numIds; ++i) {
            auto ids = snowflake.nextid<1>();
            (void)ids;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        spdlog::info("std::mutex: {} μs for {} IDs", duration.count(), numIds);
    }

    // Test SnowflakeNonLock (single-threaded)
    {
        Snowflake<TEST_EPOCH, SnowflakeNonLock> snowflake(0, 0);
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < numIds; ++i) {
            auto ids = snowflake.nextid<1>();
            (void)ids;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        spdlog::info("SnowflakeNonLock: {} μs for {} IDs", duration.count(),
                     numIds);
    }
}

// Test ID validation with various scenarios
TEST_F(SnowflakeEnhancedTest, IDValidationScenarios) {
    Snowflake<TEST_EPOCH, std::mutex> snowflake(15, 20);

    // Generate valid IDs
    std::vector<uint64_t> validIds;
    for (int i = 0; i < 100; ++i) {
        auto ids = snowflake.nextid<1>();
        validIds.push_back(ids[0]);
    }

    // All generated IDs should be valid
    for (auto id : validIds) {
        EXPECT_TRUE(snowflake.validateId(id));
    }

    // Test with different snowflake instance (different worker/datacenter)
    Snowflake<TEST_EPOCH, std::mutex> otherSnowflake(10, 15);
    for (auto id : validIds) {
        EXPECT_FALSE(otherSnowflake.validateId(id));
    }

    // Test with manually crafted invalid IDs
    uint64_t invalidId = 0;  // All zeros should be invalid
    EXPECT_FALSE(snowflake.validateId(invalidId));

    // Future timestamp should be invalid
    uint64_t futureTimestamp =
        snowflake.extractTimestamp(validIds[0]) + 1000000;
    uint64_t futureId = ((futureTimestamp - TEST_EPOCH)
                         << Snowflake<TEST_EPOCH>::TIMESTAMP_LEFT_SHIFT) |
                        (20ULL << Snowflake<TEST_EPOCH>::DATACENTER_ID_SHIFT) |
                        (15ULL << Snowflake<TEST_EPOCH>::WORKER_ID_SHIFT) | 123;
    EXPECT_FALSE(snowflake.validateId(futureId));
}

// Test serialization/deserialization under load
TEST_F(SnowflakeEnhancedTest, SerializationUnderLoad) {
    Snowflake<TEST_EPOCH, std::mutex> original(25, 30);

    // Generate some IDs to establish state
    for (int i = 0; i < 1000; ++i) {
        auto ids = original.nextid<1>();
        (void)ids;
    }

    // Serialize state
    std::string serialized = original.serialize();
    EXPECT_FALSE(serialized.empty());

    // Create new instance and deserialize
    Snowflake<TEST_EPOCH, std::mutex> restored;
    EXPECT_NO_THROW(restored.deserialize(serialized));

    // Verify worker and datacenter IDs
    EXPECT_EQ(restored.getWorkerId(), 25);
    EXPECT_EQ(restored.getDatacenterId(), 30);

    // Generate IDs from both instances
    auto originalIds = original.nextid<10>();
    auto restoredIds = restored.nextid<10>();

    // IDs should be different but valid
    for (size_t i = 0; i < originalIds.size(); ++i) {
        EXPECT_NE(originalIds[i], restoredIds[i]);
        EXPECT_TRUE(original.validateId(originalIds[i]));
        EXPECT_TRUE(restored.validateId(restoredIds[i]));
    }
}

// Test memory usage and cleanup
TEST_F(SnowflakeEnhancedTest, MemoryUsageAndCleanup) {
    // Test that repeated creation/destruction doesn't leak memory
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto snowflake = std::make_unique<Snowflake<TEST_EPOCH, std::mutex>>(
            iteration % 32, (iteration * 2) % 32);

        // Generate some IDs
        for (int i = 0; i < 100; ++i) {
            auto ids = snowflake->nextid<1>();
            (void)ids;
        }

        // Get statistics
        auto stats = snowflake->getStatistics();
        EXPECT_EQ(stats.total_ids_generated, 100);

        // Reset and generate more
        snowflake->reset();
        for (int i = 0; i < 50; ++i) {
            auto ids = snowflake->nextid<1>();
            (void)ids;
        }

        auto newStats = snowflake->getStatistics();
        EXPECT_EQ(newStats.total_ids_generated, 50);
    }

    // If we reach here without memory issues, the test passes
    SUCCEED();
}
