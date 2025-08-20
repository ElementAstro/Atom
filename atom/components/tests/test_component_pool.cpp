/*
 * test_component_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Component Pool
Tests memory pool management including allocation/deallocation,
cache alignment, statistics tracking, fragmentation handling,
SIMD operations, and performance benchmarks.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../component_pool.hpp"

using namespace atom::components;

/**
 * @brief Test fixture for ComponentPool tests
 */
class ComponentPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        PoolConfig config;
        config.initialPoolSize = 32;
        config.maxPoolSize = 128;
        config.chunkSize = 8;
        config.enableStatistics = true;
        config.enableCacheOptimization = true;
        config.enableMemoryAlignment = true;
        config.alignmentSize = 64;

        pool_ = std::make_unique<ComponentPool<Component>>(config);
    }

    void TearDown() override { pool_.reset(); }

    std::unique_ptr<ComponentPool<Component>> pool_;
};

/**
 * @brief Test fixture for SIMD container tests
 */
class SIMDContainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SIMDComponentContainer<Component>::SIMDConfig config;
        config.batchSize = 32;
        config.prefetchDistance = 2;
        config.enablePrefetch = true;
        config.enableSIMD = true;

        container_ =
            std::make_unique<SIMDComponentContainer<Component>>(config);
    }

    void TearDown() override { container_.reset(); }

    std::unique_ptr<SIMDComponentContainer<Component>> container_;
};

/**
 * @brief Test fixture for performance benchmarks
 */
class ComponentPoolPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        PoolConfig config;
        config.initialPoolSize = 64;
        config.maxPoolSize = 512;
        config.chunkSize = 16;
        config.enableStatistics = true;
        config.enableCacheOptimization = true;

        pool_ = std::make_unique<ComponentPool<Component>>(config);
    }

    void TearDown() override { pool_.reset(); }

    std::unique_ptr<ComponentPool<Component>> pool_;
    static constexpr int BENCHMARK_ITERATIONS = 1000;
};

// ============================================================================
// Basic Pool Operations Tests
// ============================================================================

TEST_F(ComponentPoolTest, BasicAllocation) {
    auto component = pool_->allocate("TestComponent");

    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "TestComponent");
    EXPECT_EQ(component->getState(), ComponentState::Created);
}

TEST_F(ComponentPoolTest, MultipleAllocations) {
    std::vector<std::shared_ptr<Component>> components;
    const int numComponents = 16;

    for (int i = 0; i < numComponents; ++i) {
        auto component = pool_->allocate("Component" + std::to_string(i));
        ASSERT_NE(component, nullptr);
        EXPECT_EQ(component->getName(), "Component" + std::to_string(i));
        components.push_back(component);
    }

    EXPECT_EQ(components.size(), numComponents);

    // Verify all components are unique
    for (size_t i = 0; i < components.size(); ++i) {
        for (size_t j = i + 1; j < components.size(); ++j) {
            EXPECT_NE(components[i].get(), components[j].get());
        }
    }
}

TEST_F(ComponentPoolTest, AllocationAndDeallocation) {
    const auto& stats = pool_->getStatistics();
    auto initialAllocations = stats.totalAllocations.load();
    auto initialDeallocations = stats.totalDeallocations.load();

    {
        auto component = pool_->allocate("TempComponent");
        EXPECT_EQ(stats.totalAllocations.load(), initialAllocations + 1);
        EXPECT_EQ(stats.currentAllocations.load(), 1);
    }  // component goes out of scope and should be deallocated

    // Give some time for deallocation to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(stats.totalDeallocations.load(), initialDeallocations + 1);
    EXPECT_EQ(stats.currentAllocations.load(), 0);
}

// ============================================================================
// Pool Configuration Tests
// ============================================================================

TEST_F(ComponentPoolTest, PoolConfiguration) {
    PoolConfig config;
    config.initialPoolSize = 16;
    config.maxPoolSize = 64;
    config.chunkSize = 4;
    config.enableStatistics = true;
    config.enableCacheOptimization = false;
    config.enableMemoryAlignment = false;

    auto customPool = std::make_unique<ComponentPool<Component>>(config);

    auto component = customPool->allocate("ConfigTest");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "ConfigTest");
}

TEST_F(ComponentPoolTest, CacheAlignment) {
    auto component = pool_->allocate("AlignmentTest");

    // Check that the component is properly aligned
    uintptr_t address = reinterpret_cast<uintptr_t>(component.get());
    EXPECT_EQ(address % 64, 0) << "Component should be cache-line aligned";
}

// ============================================================================
// Statistics and Monitoring Tests
// ============================================================================

TEST_F(ComponentPoolTest, StatisticsTracking) {
    const auto& stats = pool_->getStatistics();

    // Initial state
    auto initialAllocations = stats.totalAllocations.load();
    auto initialHits = stats.poolHits.load();

    // Allocate some components
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 5; ++i) {
        components.push_back(pool_->allocate("StatsTest" + std::to_string(i)));
    }

    // Check statistics updated
    EXPECT_EQ(stats.totalAllocations.load(), initialAllocations + 5);
    EXPECT_EQ(stats.currentAllocations.load(), 5);
    EXPECT_GT(stats.poolHits.load(), initialHits);
}

TEST_F(ComponentPoolTest, HitRatio) {
    // Allocate and deallocate to build up pool
    for (int i = 0; i < 10; ++i) {
        auto component = pool_->allocate("HitRatioTest" + std::to_string(i));
    }

    const auto& stats = pool_->getStatistics();
    double hitRatio = stats.getHitRatio();

    EXPECT_GE(hitRatio, 0.0);
    EXPECT_LE(hitRatio, 1.0);
}

TEST_F(ComponentPoolTest, MemoryUsageTracking) {
    auto initialMemory = pool_->getMemoryUsage();

    // Allocate components
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 20; ++i) {
        components.push_back(pool_->allocate("MemoryTest" + std::to_string(i)));
    }

    auto currentMemory = pool_->getMemoryUsage();
    EXPECT_GT(currentMemory, initialMemory);
}

TEST_F(ComponentPoolTest, FragmentationRatio) {
    // Allocate many components to potentially cause fragmentation
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 50; ++i) {
        components.push_back(pool_->allocate("FragTest" + std::to_string(i)));
    }

    // Deallocate every other component
    for (size_t i = 1; i < components.size(); i += 2) {
        components[i].reset();
    }

    double fragRatio = pool_->getFragmentationRatio();
    EXPECT_GE(fragRatio, 0.0);
    EXPECT_LE(fragRatio, 1.0);
}

// ============================================================================
// Pool Limits and Overflow Tests
// ============================================================================

TEST_F(ComponentPoolTest, PoolSizeLimits) {
    PoolConfig config;
    config.initialPoolSize = 4;
    config.maxPoolSize = 8;
    config.chunkSize = 2;
    config.enableStatistics = true;

    auto limitedPool = std::make_unique<ComponentPool<Component>>(config);

    std::vector<std::shared_ptr<Component>> components;

    // Allocate up to the limit
    for (int i = 0; i < 8; ++i) {
        auto component = limitedPool->allocate("LimitTest" + std::to_string(i));
        ASSERT_NE(component, nullptr);
        components.push_back(component);
    }

    // Try to allocate beyond the limit
    // The behavior depends on implementation - might throw, return nullptr, or
    // expand
    auto overflowComponent = limitedPool->allocate("OverflowTest");
    // Just check that it doesn't crash
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ComponentPoolTest, ConcurrentAllocation) {
    const int numThreads = 4;
    const int allocationsPerThread = 25;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::shared_ptr<Component>>> threadComponents(
        numThreads);

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(
            [this, t, allocationsPerThread, &threadComponents]() {
                for (int i = 0; i < allocationsPerThread; ++i) {
                    std::string name = "ConcurrentTest_" + std::to_string(t) +
                                       "_" + std::to_string(i);
                    auto component = pool_->allocate(name);
                    if (component != nullptr) {
                        threadComponents[t].push_back(component);
                    }
                }
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Count total successful allocations
    int totalAllocations = 0;
    for (const auto& components : threadComponents) {
        totalAllocations += components.size();
    }

    EXPECT_GT(totalAllocations, 0);

    // Verify all components are unique
    std::vector<Component*> allPtrs;
    for (const auto& components : threadComponents) {
        for (const auto& comp : components) {
            allPtrs.push_back(comp.get());
        }
    }

    std::sort(allPtrs.begin(), allPtrs.end());
    auto uniqueEnd = std::unique(allPtrs.begin(), allPtrs.end());
    EXPECT_EQ(uniqueEnd, allPtrs.end())
        << "All allocated components should be unique";
}

TEST_F(ComponentPoolTest, ConcurrentAllocationDeallocation) {
    const int numThreads = 4;
    const int operationsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successfulAllocations{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, operationsPerThread,
                              &successfulAllocations]() {
            std::vector<std::shared_ptr<Component>> localComponents;

            for (int i = 0; i < operationsPerThread; ++i) {
                // Allocate
                std::string name = "ConcurrentAllocDealloc_" +
                                   std::to_string(t) + "_" + std::to_string(i);
                auto component = pool_->allocate(name);
                if (component != nullptr) {
                    localComponents.push_back(component);
                    successfulAllocations++;
                }

                // Occasionally deallocate some components
                if (i % 10 == 0 && !localComponents.empty()) {
                    localComponents.erase(
                        localComponents.begin(),
                        localComponents.begin() +
                            std::min(5, (int)localComponents.size()));
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successfulAllocations.load(), 0);

    // Check that pool statistics are consistent
    const auto& stats = pool_->getStatistics();
    EXPECT_GE(stats.totalAllocations.load(), successfulAllocations.load());
}

// ============================================================================
// SIMD Container Tests
// ============================================================================

TEST_F(SIMDContainerTest, BasicSIMDOperations) {
    // Add components to SIMD container
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 16; ++i) {
        auto component =
            std::make_shared<Component>("SIMDTest" + std::to_string(i));
        components.push_back(component);
        container_->addComponent(component);
    }

    EXPECT_EQ(container_->size(), 16);
}

TEST_F(SIMDContainerTest, BatchProcessing) {
    // Add components
    for (int i = 0; i < 32; ++i) {
        auto component =
            std::make_shared<Component>("BatchTest" + std::to_string(i));
        component->addVariable<int>("value", i);
        container_->addComponent(component);
    }

    // Process in batches
    int processedCount = 0;
    container_->processBatch(
        [&processedCount](
            const std::vector<std::shared_ptr<Component>>& batch) {
            for (const auto& comp : batch) {
                processedCount++;
                // Simulate some processing
                auto var = comp->getVariable<int>("value");
                var->set(var->get() * 2);
            }
        });

    EXPECT_EQ(processedCount, 32);
}

TEST_F(SIMDContainerTest, PrefetchOptimization) {
    // Add many components to test prefetching
    for (int i = 0; i < 64; ++i) {
        auto component =
            std::make_shared<Component>("PrefetchTest" + std::to_string(i));
        container_->addComponent(component);
    }

    // Enable prefetching and process
    container_->enablePrefetch(true);

    auto start = std::chrono::high_resolution_clock::now();

    container_->processBatch(
        [](const std::vector<std::shared_ptr<Component>>& batch) {
            for (const auto& comp : batch) {
                // Simulate memory access
                volatile auto name = comp->getName();
                (void)name;
            }
        });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Just ensure it completes without crashing
    EXPECT_GT(duration.count(), 0);
}

TEST_F(SIMDContainerTest, ComponentRemoval) {
    std::vector<std::shared_ptr<Component>> components;

    // Add components
    for (int i = 0; i < 10; ++i) {
        auto component =
            std::make_shared<Component>("RemovalTest" + std::to_string(i));
        components.push_back(component);
        container_->addComponent(component);
    }

    EXPECT_EQ(container_->size(), 10);

    // Remove some components
    for (int i = 0; i < 5; ++i) {
        container_->removeComponent(components[i]);
    }

    EXPECT_EQ(container_->size(), 5);
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

TEST_F(ComponentPoolPerformanceTest, AllocationPerformanceBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::shared_ptr<Component>> components;
    components.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        components.push_back(
            pool_->allocate("BenchmarkComponent" + std::to_string(i)));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Pool allocation benchmark: " << duration.count() << " μs for "
              << BENCHMARK_ITERATIONS << " allocations" << std::endl;
    std::cout << "Average per allocation: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be faster than standard allocation
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS,
              5);  // Less than 5μs per allocation
}

TEST_F(ComponentPoolPerformanceTest, PoolVsStandardAllocation) {
    const int iterations = 500;

    // Benchmark pool allocation
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::shared_ptr<Component>> poolComponents;
    for (int i = 0; i < iterations; ++i) {
        poolComponents.push_back(
            pool_->allocate("PoolBench" + std::to_string(i)));
    }
    auto poolTime = std::chrono::high_resolution_clock::now() - start;

    // Benchmark standard allocation
    start = std::chrono::high_resolution_clock::now();
    std::vector<std::shared_ptr<Component>> standardComponents;
    for (int i = 0; i < iterations; ++i) {
        standardComponents.push_back(
            std::make_shared<Component>("StandardBench" + std::to_string(i)));
    }
    auto standardTime = std::chrono::high_resolution_clock::now() - start;

    auto poolMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(poolTime).count();
    auto standardMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(standardTime)
            .count();

    std::cout << "Pool allocation: " << poolMicros << " μs" << std::endl;
    std::cout << "Standard allocation: " << standardMicros << " μs"
              << std::endl;
    std::cout << "Pool speedup: " << (double)standardMicros / poolMicros << "x"
              << std::endl;

    // Pool should be competitive (within 2x of standard allocation)
    EXPECT_LT(poolMicros, standardMicros * 2);
}

TEST_F(ComponentPoolPerformanceTest, MemoryLocalityBenchmark) {
    // Allocate many components
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 100; ++i) {
        auto component = pool_->allocate("LocalityTest" + std::to_string(i));
        component->addVariable<int>("value", i);
        components.push_back(component);
    }

    // Benchmark sequential access (should benefit from cache locality)
    auto start = std::chrono::high_resolution_clock::now();

    int sum = 0;
    for (const auto& component : components) {
        auto var = component->getVariable<int>("value");
        sum += var->get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Memory locality benchmark: " << duration.count() << " μs for "
              << components.size() << " accesses" << std::endl;

    // Verify correctness
    int expectedSum = (components.size() - 1) * components.size() / 2;
    EXPECT_EQ(sum, expectedSum);

    // Should be reasonably fast
    EXPECT_LT(duration.count(), 1000);  // Less than 1ms
}

// ============================================================================
// Memory Management and Cleanup Tests
// ============================================================================

TEST_F(ComponentPoolTest, MemoryLeakDetection) {
    auto initialMemory = pool_->getMemoryUsage();

    {
        std::vector<std::shared_ptr<Component>> components;
        for (int i = 0; i < 50; ++i) {
            components.push_back(
                pool_->allocate("LeakTest" + std::to_string(i)));
        }

        auto peakMemory = pool_->getMemoryUsage();
        EXPECT_GT(peakMemory, initialMemory);
    }  // All components should be deallocated here

    // Give time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Memory usage should return close to initial level
    auto finalMemory = pool_->getMemoryUsage();
    EXPECT_LE(finalMemory, initialMemory * 1.1);  // Allow 10% overhead
}

TEST_F(ComponentPoolTest, PoolCleanup) {
    // Allocate and deallocate to create fragmentation
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 30; ++i) {
        components.push_back(
            pool_->allocate("CleanupTest" + std::to_string(i)));
    }

    // Deallocate every other component
    for (size_t i = 1; i < components.size(); i += 2) {
        components[i].reset();
    }

    auto fragRatioBefore = pool_->getFragmentationRatio();

    // Trigger cleanup
    pool_->performCleanup();

    auto fragRatioAfter = pool_->getFragmentationRatio();

    // Fragmentation should be reduced or at least not increased
    EXPECT_LE(fragRatioAfter, fragRatioBefore * 1.1);
}

// ============================================================================
// Error Handling and Edge Cases
// ============================================================================

TEST_F(ComponentPoolTest, AllocationWithEmptyName) {
    // Test allocation with empty name
    EXPECT_THROW(pool_->allocate(""), std::invalid_argument);
}

TEST_F(ComponentPoolTest, ExtremelyLongComponentName) {
    std::string longName(10000, 'A');
    auto component = pool_->allocate(longName);

    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), longName);
}

TEST_F(ComponentPoolTest, RapidAllocationDeallocation) {
    // Rapidly allocate and deallocate to stress test the pool
    for (int cycle = 0; cycle < 10; ++cycle) {
        std::vector<std::shared_ptr<Component>> components;

        // Allocate
        for (int i = 0; i < 20; ++i) {
            components.push_back(pool_->allocate(
                "RapidTest" + std::to_string(cycle) + "_" + std::to_string(i)));
        }

        // Deallocate
        components.clear();
    }

    // Pool should remain stable
    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.currentAllocations.load(), 0);
}
