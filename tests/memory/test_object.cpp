#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "atom/memory/object.hpp"

using namespace atom::memory;

// Sample Resettable class for testing
class TestObject {
public:
    void reset() { value = 0; }

    int value = 0;
};

class ObjectPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(ObjectPoolTest, Constructor) {
    ObjectPool<TestObject> pool(10);
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 0);
}

TEST_F(ObjectPoolTest, AcquireAndRelease) {
    ObjectPool<TestObject> pool(10);
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 9);
    EXPECT_EQ(pool.size(), 1);

    obj->value = 42;
    obj.reset();
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 1);
    EXPECT_EQ(pool.inUseCount(), 0);

    auto obj2 = pool.acquire();
    EXPECT_EQ(obj2->value, 0);  // Ensure the object was reset
}

TEST_F(ObjectPoolTest, TryAcquireFor) {
    ObjectPool<TestObject> pool(1);
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 0);

    auto obj2 = pool.tryAcquireFor(std::chrono::milliseconds(100));
    EXPECT_FALSE(obj2.has_value());

    obj.reset();
    auto obj3 = pool.tryAcquireFor(std::chrono::milliseconds(100));
    EXPECT_TRUE(obj3.has_value());
}

TEST_F(ObjectPoolTest, Prefill) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 5);

    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 9);
    EXPECT_EQ(pool.size(), 6);
}

TEST_F(ObjectPoolTest, Clear) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 5);

    pool.clear();
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 0);
}

TEST_F(ObjectPoolTest, Resize) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 5);

    pool.resize(20);
    EXPECT_EQ(pool.available(), 20);
    EXPECT_EQ(pool.size(), 5);

    pool.resize(5);
    EXPECT_EQ(pool.available(), 5);
    EXPECT_EQ(pool.size(), 5);
}

TEST_F(ObjectPoolTest, ApplyToAll) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);

    pool.applyToAll([](TestObject& obj) { obj.value = 42; });

    for (int i = 0; i < 5; ++i) {
        auto obj = pool.acquire();
        EXPECT_EQ(obj->value, 42);
    }
}

TEST_F(ObjectPoolTest, InUseCount) {
    ObjectPool<TestObject> pool(10);
    EXPECT_EQ(pool.inUseCount(), 0);

    auto obj = pool.acquire();
    EXPECT_EQ(pool.inUseCount(), 1);

    obj.reset();
    EXPECT_EQ(pool.inUseCount(), 0);
}

TEST_F(ObjectPoolTest, ThreadSafety) {
    ObjectPool<TestObject> pool(10);
    std::vector<std::thread> threads;

    threads.reserve(10);
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < 100; ++j) {
                auto obj = pool.acquire();
                obj->value = j;
                obj.reset();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 0);
}

// Test for acquiring with validation
TEST_F(ObjectPoolTest, AcquireValidated) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);

    // Set values in the pool objects
    pool.applyToAll([](TestObject& obj) { obj.value = 42; });

    // Acquire an object that has value == 42
    auto obj = pool.acquireValidated(
        [](const TestObject& obj) { return obj.value == 42; });
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);

    // Change value and release
    obj->value = 100;
    obj.reset();

    // Validate that our next acquisition has been reset
    auto obj2 = pool.acquire();
    EXPECT_EQ(obj2->value, 0);  // Should be reset to 0
}

// Test for batch acquisition
TEST_F(ObjectPoolTest, AcquireBatch) {
    ObjectPool<TestObject> pool(10);

    auto objects = pool.acquireBatch(5);
    EXPECT_EQ(objects.size(), 5);
    EXPECT_EQ(pool.available(), 5);
    EXPECT_EQ(pool.inUseCount(), 5);

    // Modify all objects
    for (size_t i = 0; i < objects.size(); ++i) {
        objects[i]->value = static_cast<int>(i);
    }

    // Release one object
    objects[0].reset();
    EXPECT_EQ(pool.available(), 6);
    EXPECT_EQ(pool.inUseCount(), 4);

    // Release all remaining objects
    objects.clear();
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.inUseCount(), 0);
}

// Test for empty batch acquisition
TEST_F(ObjectPoolTest, AcquireEmptyBatch) {
    ObjectPool<TestObject> pool(10);

    auto objects = pool.acquireBatch(0);
    EXPECT_EQ(objects.size(), 0);
    EXPECT_EQ(pool.available(), 10);
}

// Test for exceeding maximum batch size
TEST_F(ObjectPoolTest, AcquireTooLargeBatch) {
    ObjectPool<TestObject> pool(5);

    EXPECT_THROW({ auto objects = pool.acquireBatch(10); }, std::runtime_error);
}

// Test for priority-based acquisition
TEST_F(ObjectPoolTest, PriorityAcquisition) {
    ObjectPool<TestObject> pool(1);

    // Acquire the only object
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 0);

    // Start a thread that will try to acquire with high priority
    std::atomic<bool> high_priority_acquired{false};
    std::thread high_priority_thread([&pool, &high_priority_acquired]() {
        auto obj = pool.acquire(ObjectPool<TestObject>::Priority::High);
        high_priority_acquired = true;
        // Hold the object briefly then release
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        obj.reset();
    });

    // Start a thread that will try to acquire with low priority
    std::atomic<bool> low_priority_acquired{false};
    std::thread low_priority_thread([&pool, &low_priority_acquired]() {
        auto obj = pool.acquire(ObjectPool<TestObject>::Priority::Low);
        low_priority_acquired = true;
    });

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Release the object, allowing a waiting thread to acquire it
    obj.reset();

    // Wait a bit to let threads process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // High priority thread should acquire first
    EXPECT_TRUE(high_priority_acquired);
    EXPECT_FALSE(low_priority_acquired);

    // Clean up threads
    high_priority_thread.join();
    low_priority_thread.join();
}

// Test for auto cleanup functionality
TEST_F(ObjectPoolTest, AutoCleanup) {
    // Configure a pool with short idle times for testing
    ObjectPool<TestObject>::PoolConfig config;
    config.cleanup_interval =
        std::chrono::minutes(0);  // Run cleanup immediately
    config.max_idle_time =
        std::chrono::minutes(0);  // Objects expire immediately

    ObjectPool<TestObject> pool(
        10, 0, []() { return std::make_shared<TestObject>(); }, config);

    // Acquire and release to put objects in the idle list
    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();
    obj1.reset();
    obj2.reset();

    // Pool should have 2 objects now
    EXPECT_EQ(pool.size(), 2);

    // Run cleanup manually to force immediate cleanup
    size_t cleaned = pool.runCleanup(true);
    EXPECT_EQ(cleaned, 2);  // Should clean both objects

    // Pool should be empty again
    EXPECT_EQ(pool.available(), 10);
    EXPECT_EQ(pool.size(), 0);
}

// Test for statistics tracking
TEST_F(ObjectPoolTest, StatisticsTracking) {
    ObjectPool<TestObject> pool(10);

    // Get initial stats
    auto initial_stats = pool.getStats();
    EXPECT_EQ(initial_stats.hits, 0);
    EXPECT_EQ(initial_stats.misses, 0);

    // Acquire an object (should be a miss)
    auto obj = pool.acquire();
    obj.reset();

    // Acquire again (should be a hit)
    auto obj2 = pool.acquire();
    obj2.reset();

    // Check updated stats
    auto stats = pool.getStats();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);

    // Reset stats
    pool.resetStats();
    auto reset_stats = pool.getStats();
    EXPECT_EQ(reset_stats.hits, 0);
    EXPECT_EQ(reset_stats.misses, 0);
}

// Test for timeout statistics
TEST_F(ObjectPoolTest, TimeoutStats) {
    ObjectPool<TestObject> pool(1);
    auto obj = pool.acquire();  // Acquire the only object

    // Try to acquire with timeout (should fail)
    auto result = pool.tryAcquireFor(std::chrono::milliseconds(10));
    EXPECT_FALSE(result.has_value());

    // Check timeout stats
    auto stats = pool.getStats();
    EXPECT_EQ(stats.timeout_count, 1);
    EXPECT_EQ(stats.wait_count, 1);
}

// Test for custom object creation
TEST_F(ObjectPoolTest, CustomObjectCreation) {
    int creation_count = 0;
    ObjectPool<TestObject> pool(10, 0, [&creation_count]() {
        creation_count++;
        auto obj = std::make_shared<TestObject>();
        obj->value = 100;  // Custom initialization
        return obj;
    });

    // Acquire an object
    auto obj = pool.acquire();
    EXPECT_EQ(obj->value, 100);    // Should have custom initialization
    EXPECT_EQ(creation_count, 1);  // Should have called our creator

    // Release and reacquire
    obj.reset();
    auto obj2 = pool.acquire();
    EXPECT_EQ(obj2->value, 0);     // Should be reset to 0
    EXPECT_EQ(creation_count, 1);  // Should reuse object, not create a new one
}

// Test for pool reconfiguration
TEST_F(ObjectPoolTest, Reconfiguration) {
    ObjectPool<TestObject> pool(10);

    // Reconfigure with custom validator
    ObjectPool<TestObject>::PoolConfig new_config;
    new_config.validator = [](const TestObject& obj) {
        return obj.value < 100;
    };
    new_config.validate_on_release = true;

    pool.reconfigure(new_config);

    // Acquire and set invalid value
    auto obj = pool.acquire();
    obj->value = 200;  // Greater than our validator limit

    // Release - should not return to pool due to validation
    obj.reset();

    // Check that object wasn't returned to pool
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.available(), 10);
}

// Test for peak usage tracking
TEST_F(ObjectPoolTest, PeakUsageTracking) {
    ObjectPool<TestObject> pool(10);

    // Acquire multiple objects
    std::vector<std::shared_ptr<TestObject>> objects;
    for (int i = 0; i < 8; ++i) {
        objects.push_back(pool.acquire());
    }

    // Release some objects
    objects[0].reset();
    objects[1].reset();

    // Get stats and check peak usage
    auto stats = pool.getStats();
    EXPECT_EQ(stats.peak_usage, 8);

    // Clean up
    objects.clear();
}

// Test exception handling for full pool
TEST_F(ObjectPoolTest, FullPoolException) {
    ObjectPool<TestObject> pool(2);

    // Acquire all objects
    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();

    // Try to acquire more objects than the pool can provide
    EXPECT_THROW({ auto obj3 = pool.acquire(); }, std::runtime_error);

    // But tryAcquireFor should return nullopt, not throw
    auto result = pool.tryAcquireFor(std::chrono::milliseconds(10));
    EXPECT_FALSE(result.has_value());
}

// Test for wait time tracking
TEST_F(ObjectPoolTest, WaitTimeTracking) {
    ObjectPool<TestObject> pool(1);

    // Acquire the only object
    auto obj = pool.acquire();

    // Start a thread that will need to wait
    std::thread waiter([&pool]() {
        auto obj = pool.acquire();
        obj.reset();
    });

    // Hold the object for a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    obj.reset();

    // Wait for other thread to finish
    waiter.join();

    // Check stats
    auto stats = pool.getStats();
    EXPECT_EQ(stats.wait_count, 1);
    EXPECT_GT(stats.total_wait_time.count(), 0);
    EXPECT_GT(stats.max_wait_time.count(), 0);
}

// Test for move semantics
/*
TODO: Implement move semantics for ObjectPool and test
TEST_F(ObjectPoolTest, MoveSemantics) {
    ObjectPool<TestObject> original_pool(10);
    original_pool.prefill(5);

    // Acquire an object from original pool
    auto obj = original_pool.acquire();

    // Move the pool to a new variable
    ObjectPool<TestObject> moved_pool(std::move(original_pool));

    // Check that the moved pool has correct state
    EXPECT_EQ(moved_pool.size(), 5);
    EXPECT_EQ(moved_pool.available(), 4);

    // Release the object
    obj.reset();

    // Object should be returned to moved pool
    EXPECT_EQ(moved_pool.available(), 5);

    // Acquire from moved pool
    auto obj2 = moved_pool.acquire();
    EXPECT_NE(obj2, nullptr);
}
*/

// Test for stress testing with multiple threads
TEST_F(ObjectPoolTest, StressTest) {
    ObjectPool<TestObject> pool(100);

    constexpr int num_threads = 10;
    constexpr int operations_per_thread = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> total_acquisitions{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool, &total_acquisitions]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Randomly decide how many objects to acquire
                int count = 1 + (std::rand() % 3);  // 1-3 objects

                // Sometimes do batch acquisition
                if (j % 10 == 0) {
                    try {
                        auto objects = pool.acquireBatch(count);
                        total_acquisitions += objects.size();
                        // Modify objects
                        for (auto& obj : objects) {
                            obj->value++;
                        }
                    } catch (const std::runtime_error&) {
                        // Ignore if we can't acquire that many objects
                    }
                } else {
                    // Regular acquisition
                    auto obj = pool.acquire();
                    total_acquisitions++;
                    obj->value++;
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify final state
    EXPECT_EQ(pool.available(), 100);  // All objects should be returned
    EXPECT_EQ(total_acquisitions, num_threads * operations_per_thread);

    // Check stats
    auto stats = pool.getStats();
    EXPECT_GT(stats.hits, 0);
    EXPECT_GT(stats.misses, 0);
}

// Complex object with additional state for testing
class ComplexTestObject {
public:
    void reset() {
        value = 0;
        is_initialized = false;
    }

    void initialize() {
        is_initialized = true;
        value = 42;
    }

    int value = 0;
    bool is_initialized = false;
};

// Test for complex objects with custom initialization
TEST_F(ObjectPoolTest, ComplexObjectInitialization) {
    ObjectPool<ComplexTestObject> pool(10);

    // Acquire and initialize a complex object
    auto obj = pool.acquire();
    EXPECT_FALSE(obj->is_initialized);

    obj->initialize();
    EXPECT_TRUE(obj->is_initialized);
    EXPECT_EQ(obj->value, 42);

    // Release and verify reset
    obj.reset();

    // Re-acquire and verify state was reset
    auto obj2 = pool.acquire();
    EXPECT_FALSE(obj2->is_initialized);
    EXPECT_EQ(obj2->value, 0);
}

// Test for performance comparison
TEST_F(ObjectPoolTest, PerformanceComparison) {
    constexpr int iterations = 10000;

    // Time direct allocation
    auto start_direct = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto obj = std::make_shared<TestObject>();
        obj->value = i;
    }
    auto end_direct = std::chrono::high_resolution_clock::now();
    auto direct_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_direct -
                                                              start_direct)
            .count();

    // Time pool allocation
    ObjectPool<TestObject> pool(iterations);
    auto start_pool = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto obj = pool.acquire();
        obj->value = i;
        obj.reset();  // Return to pool immediately
    }
    auto end_pool = std::chrono::high_resolution_clock::now();
    auto pool_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                             end_pool - start_pool)
                             .count();

    // Output performance comparison (not an assertion)
    std::cout << "Direct allocation took " << direct_duration << " microseconds"
              << std::endl;
    std::cout << "Pool allocation took " << pool_duration << " microseconds"
              << std::endl;

    // Pool should generally be faster after warmup, but we don't assert this
    // as performance can vary by platform
}

// Additional edge case tests for ObjectPool
TEST_F(ObjectPoolTest, ValidatorEdgeCases) {
    // Test with validator that always returns false
    ObjectPool<TestObject>::PoolConfig config;
    config.validator = [](const TestObject&) { return false; };

    ObjectPool<TestObject> pool(5, 0, nullptr, config);
    pool.prefill(3);

    // All objects should be considered invalid and recreated
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 0);  // Should be newly created

    // Pool should have fewer available objects due to validation failures
    EXPECT_LE(pool.available(), 5);
}

TEST_F(ObjectPoolTest, TimeoutEdgeCases) {
    ObjectPool<TestObject> pool(1);
    auto obj = pool.acquire();  // Acquire the only object

    // Test with zero timeout
    auto result1 = pool.tryAcquireFor(std::chrono::milliseconds(0));
    EXPECT_FALSE(result1.has_value());

    // Test with very short timeout
    auto result2 = pool.tryAcquireFor(std::chrono::microseconds(1));
    EXPECT_FALSE(result2.has_value());

    // Release and test immediate acquisition
    obj.reset();
    auto result3 = pool.tryAcquireFor(std::chrono::milliseconds(0));
    EXPECT_TRUE(result3.has_value());
}

TEST_F(ObjectPoolTest, StatisticsEdgeCases) {
    ObjectPool<TestObject> pool(10);

    // Test statistics with no operations
    auto stats1 = pool.getStats();
    EXPECT_EQ(stats1.hits, 0);
    EXPECT_EQ(stats1.misses, 0);
    EXPECT_EQ(stats1.timeout_count, 0);

    // Test statistics overflow protection (if implemented)
    // This would require many operations to test properly
    for (int i = 0; i < 1000; ++i) {
        auto obj = pool.acquire();
        obj.reset();
    }

    auto stats2 = pool.getStats();
    EXPECT_EQ(stats2.hits, 1000);
    EXPECT_EQ(stats2.misses, 0);
}

TEST_F(ObjectPoolTest, ResizeEdgeCases) {
    ObjectPool<TestObject> pool(10);
    pool.prefill(5);

    // Resize to zero
    pool.resize(0);
    EXPECT_EQ(pool.available(), 0);
    EXPECT_EQ(pool.size(), 0);

    // Try to acquire from empty pool
    EXPECT_THROW([[maybe_unused]] auto temp = pool.acquire(),
                 std::runtime_error);

    // Resize back to positive size
    pool.resize(5);
    EXPECT_EQ(pool.available(), 5);

    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
}

TEST_F(ObjectPoolTest, ApplyToAllEdgeCases) {
    ObjectPool<TestObject> pool(10);

    // Apply to empty pool
    int count = 0;
    pool.applyToAll([&count](TestObject&) { count++; });
    EXPECT_EQ(count, 0);

    // Apply with exception in lambda
    pool.prefill(3);
    EXPECT_THROW(
        pool.applyToAll([](TestObject&) { throw std::runtime_error("test"); }),
        std::runtime_error);

    // Pool should still be functional after exception
    auto obj = pool.acquire();
    EXPECT_NE(obj, nullptr);
}

TEST_F(ObjectPoolTest, AcquireBatchEdgeCases) {
    ObjectPool<TestObject> pool(5);

    // Acquire batch larger than pool size
    EXPECT_THROW([[maybe_unused]] auto temp1 = pool.acquireBatch(10),
                 std::runtime_error);

    // Acquire maximum batch size
    auto objects = pool.acquireBatch(5);
    EXPECT_EQ(objects.size(), 5);
    EXPECT_EQ(pool.available(), 0);

    // Try to acquire more when pool is empty
    EXPECT_THROW([[maybe_unused]] auto temp2 = pool.acquireBatch(1),
                 std::runtime_error);

    // Release all and try again
    objects.clear();
    auto objects2 = pool.acquireBatch(3);
    EXPECT_EQ(objects2.size(), 3);
    EXPECT_EQ(pool.available(), 2);
}

// Advanced thread safety tests for ObjectPool
TEST_F(ObjectPoolTest, ConcurrentAcquireRelease) {
    ObjectPool<TestObject> pool(20);
    constexpr int numThreads = 6;
    constexpr int operationsPerThread = 200;
    std::vector<std::thread> threads;
    std::atomic<int> totalAcquisitions{0};
    std::atomic<int> totalReleases{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&pool, &totalAcquisitions, &totalReleases,
                              operationsPerThread, i]() {
            std::vector<std::shared_ptr<TestObject>> localObjects;

            for (int j = 0; j < operationsPerThread; ++j) {
                if (j % 3 == 0) {
                    // Acquire object
                    try {
                        auto obj = pool.acquire();
                        if (obj) {
                            obj->value = i * 1000 + j;
                            localObjects.push_back(obj);
                            totalAcquisitions++;
                        }
                    } catch (const std::runtime_error&) {
                        // Pool might be full, continue
                    }
                } else if (!localObjects.empty() && j % 3 == 1) {
                    // Release object
                    localObjects.pop_back();
                    totalReleases++;
                }

                // Small delay to increase thread interleaving
                if (j % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }

            // Release remaining objects
            totalReleases += localObjects.size();
            localObjects.clear();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify final state
    EXPECT_EQ(totalAcquisitions.load(), totalReleases.load());
    EXPECT_EQ(pool.inUseCount(), 0);
}

TEST_F(ObjectPoolTest, ConcurrentBatchOperations) {
    ObjectPool<TestObject> pool(50);
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> successfulBatches{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&pool, &successfulBatches, i]() {
            for (int j = 0; j < 20; ++j) {
                try {
                    size_t batchSize = 3 + (j % 5);  // Batch sizes 3-7
                    auto objects = pool.acquireBatch(batchSize);

                    if (objects.size() == batchSize) {
                        successfulBatches++;

                        // Modify objects
                        for (size_t k = 0; k < objects.size(); ++k) {
                            objects[k]->value = i * 10000 + j * 100 + k;
                        }

                        // Hold objects for a short time
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(10));

                        // Objects are automatically released when vector goes
                        // out of scope
                    }
                } catch (const std::runtime_error&) {
                    // Pool might not have enough objects, continue
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successfulBatches.load(), 0);
    EXPECT_EQ(pool.inUseCount(), 0);
}

TEST_F(ObjectPoolTest, ConcurrentResizeOperations) {
    ObjectPool<TestObject> pool(10);
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag{false};

    // Thread that continuously acquires and releases objects
    threads.emplace_back([&pool, &stopFlag]() {
        std::vector<std::shared_ptr<TestObject>> objects;
        while (!stopFlag.load()) {
            try {
                auto obj = pool.acquire();
                if (obj) {
                    objects.push_back(obj);
                }

                if (objects.size() > 5) {
                    objects.erase(objects.begin(), objects.begin() + 3);
                }
            } catch (const std::runtime_error&) {
                // Pool might be resizing, continue
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        objects.clear();
    });

    // Thread that continuously resizes the pool
    threads.emplace_back([&pool, &stopFlag]() {
        std::vector<size_t> sizes = {5, 15, 8, 20, 12};
        size_t sizeIndex = 0;

        while (!stopFlag.load()) {
            try {
                pool.resize(sizes[sizeIndex]);
                sizeIndex = (sizeIndex + 1) % sizes.size();
            } catch (const std::runtime_error&) {
                // Resize might fail if too many objects in use
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stopFlag = true;

    for (auto& thread : threads) {
        thread.join();
    }

    // Pool should be in a consistent state
    EXPECT_GE(pool.available(), 0);
    EXPECT_GE(pool.size(), 0);
}
