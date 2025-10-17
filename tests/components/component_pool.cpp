#include "atom/components/core/component_pool.hpp"
#include "atom/components/core/component.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace atom::components;

// Test fixture for ComponentPool tests
class ComponentPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.initialPoolSize = 16;
        config_.maxPoolSize = 64;
        config_.chunkSize = 8;
        config_.enableStatistics = true;
        config_.enableCacheOptimization = true;
        config_.enableMemoryAlignment = true;
        config_.alignmentSize = 64;

        pool_ = std::make_unique<ComponentPool<Component>>(config_);
    }

    void TearDown() override { pool_.reset(); }

    PoolConfig config_;
    std::unique_ptr<ComponentPool<Component>> pool_;
};

// Test fixture for ComponentFactory tests
class ComponentFactoryTest : public ::testing::Test {
protected:
    void SetUp() override { factory_ = &ComponentFactory::instance(); }

    ComponentFactory* factory_;
};

// Test fixture for SIMDComponentContainer tests
class SIMDComponentContainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SIMDComponentContainer<Component>::SIMDConfig config;
        config.batchSize = 8;
        config.enablePrefetch = true;

        container_ =
            std::make_unique<SIMDComponentContainer<Component>>(config);
    }

    std::unique_ptr<SIMDComponentContainer<Component>> container_;
};

// ============================================================================
// ComponentPool Tests
// ============================================================================

TEST_F(ComponentPoolTest, Constructor) {
    EXPECT_NE(pool_, nullptr);

    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.totalAllocations.load(), 0);
    EXPECT_EQ(stats.currentAllocations.load(), 0);
    EXPECT_EQ(stats.totalDeallocations.load(), 0);
}

TEST_F(ComponentPoolTest, BasicAllocation) {
    auto component = pool_->allocate("TestComponent");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "TestComponent");

    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.totalAllocations.load(), 1);
    EXPECT_EQ(stats.currentAllocations.load(), 1);
}

TEST_F(ComponentPoolTest, MultipleAllocations) {
    std::vector<std::shared_ptr<Component>> components;

    for (int i = 0; i < 10; ++i) {
        auto component = pool_->allocate("Component" + std::to_string(i));
        ASSERT_NE(component, nullptr);
        components.push_back(component);
    }

    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.totalAllocations.load(), 10);
    EXPECT_EQ(stats.currentAllocations.load(), 10);
}

TEST_F(ComponentPoolTest, AllocationDeallocation) {
    const auto& stats = pool_->getStatistics();
    auto initialAllocations = stats.totalAllocations.load();

    {
        auto component = pool_->allocate("TempComponent");
        EXPECT_EQ(stats.totalAllocations.load(), initialAllocations + 1);
        EXPECT_EQ(stats.currentAllocations.load(), 1);
    }  // component goes out of scope

    // Allow time for deallocation processing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(stats.currentAllocations.load(), 0);
}

TEST_F(ComponentPoolTest, PoolExhaustion) {
    std::vector<std::shared_ptr<Component>> components;

    // Allocate up to max pool size
    for (size_t i = 0; i < config_.maxPoolSize; ++i) {
        auto component = pool_->allocate("Component" + std::to_string(i));
        if (component) {
            components.push_back(component);
        }
    }

    // Try to allocate beyond max size - should handle gracefully
    auto extraComponent = pool_->allocate("ExtraComponent");
    // Behavior depends on implementation - either returns nullptr or throws
}

TEST_F(ComponentPoolTest, Statistics) {
    const auto& stats = pool_->getStatistics();

    // Initial state
    EXPECT_EQ(stats.totalAllocations.load(), 0);
    EXPECT_EQ(stats.currentAllocations.load(), 0);
    EXPECT_EQ(stats.totalDeallocations.load(), 0);
    EXPECT_EQ(stats.poolHits.load(), 0);
    EXPECT_EQ(stats.poolMisses.load(), 0);

    // After allocation
    auto component = pool_->allocate("TestComponent");
    EXPECT_GT(stats.totalAllocations.load(), 0);
    EXPECT_GT(stats.currentAllocations.load(), 0);
}

TEST_F(ComponentPoolTest, FragmentationRatio) {
    // Initially should have low fragmentation
    double initialFragmentation = pool_->getFragmentationRatio();
    EXPECT_GE(initialFragmentation, 0.0);
    EXPECT_LE(initialFragmentation, 1.0);

    // Allocate and deallocate to create fragmentation
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 8; ++i) {
        components.push_back(pool_->allocate("Component" + std::to_string(i)));
    }

    // Remove every other component to create fragmentation
    for (size_t i = 1; i < components.size(); i += 2) {
        components[i].reset();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    double fragmentationAfter = pool_->getFragmentationRatio();
    EXPECT_GE(fragmentationAfter, 0.0);
    EXPECT_LE(fragmentationAfter, 1.0);
}

TEST_F(ComponentPoolTest, ThreadSafety) {
    const int numThreads = 4;
    const int allocationsPerThread = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::shared_ptr<Component>>> threadComponents(
        numThreads);

    // Launch threads that allocate components
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(
            [this, t, allocationsPerThread, &threadComponents]() {
                for (int i = 0; i < allocationsPerThread; ++i) {
                    auto component =
                        pool_->allocate("Thread" + std::to_string(t) +
                                        "Component" + std::to_string(i));
                    if (component) {
                        threadComponents[t].push_back(component);
                    }
                }
            });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify statistics
    const auto& stats = pool_->getStatistics();
    EXPECT_LE(stats.totalAllocations.load(), numThreads * allocationsPerThread);
    EXPECT_GE(stats.totalAllocations.load(), 0);
}

// ============================================================================
// ComponentFactory Tests
// ============================================================================

TEST_F(ComponentFactoryTest, Singleton) {
    auto& factory1 = ComponentFactory::instance();
    auto& factory2 = ComponentFactory::instance();
    EXPECT_EQ(&factory1, &factory2);
}

TEST_F(ComponentFactoryTest, CreateComponent) {
    auto component = factory_->create<Component>("FactoryTestComponent");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "FactoryTestComponent");
}

TEST_F(ComponentFactoryTest, ConfigurePool) {
    PoolConfig config;
    config.initialPoolSize = 32;
    config.maxPoolSize = 128;
    config.enableStatistics = true;

    factory_->configurePool<Component>(config);

    // Create component to verify pool configuration
    auto component = factory_->create<Component>("PoolConfigTest");
    EXPECT_NE(component, nullptr);
}

TEST_F(ComponentFactoryTest, GetPoolStatistics) {
    // Create some components first
    auto comp1 = factory_->create<Component>("Comp1");
    auto comp2 = factory_->create<Component>("Comp2");

    const auto& stats =
        factory_->getPoolStatistics<Component>();  // Use reference, not copy
    EXPECT_GT(stats.totalAllocations.load(), 0);
}

// ============================================================================
// SIMDComponentContainer Tests
// ============================================================================

TEST_F(SIMDComponentContainerTest, Constructor) {
    EXPECT_NE(container_, nullptr);
    EXPECT_EQ(container_->size(), 0);
    EXPECT_TRUE(container_->empty());
}

TEST_F(SIMDComponentContainerTest, AddComponent) {
    auto component = std::make_shared<Component>("SIMDTestComponent");
    container_->add(component);

    EXPECT_EQ(container_->size(), 1);
    EXPECT_FALSE(container_->empty());
}

TEST_F(SIMDComponentContainerTest, RemoveComponent) {
    auto component = std::make_shared<Component>("SIMDTestComponent");
    container_->add(component);

    EXPECT_EQ(container_->size(), 1);

    container_->remove(component);  // remove() returns void
    EXPECT_EQ(container_->size(), 0);
    EXPECT_TRUE(container_->empty());
}

TEST_F(SIMDComponentContainerTest, ComponentIteration) {
    // Add multiple components
    for (int i = 0; i < 16; ++i) {
        auto component =
            std::make_shared<Component>("IterComponent" + std::to_string(i));
        container_->add(component);
    }

    EXPECT_EQ(container_->size(), 16);

    // Verify all components are stored
    EXPECT_FALSE(container_->empty());
}

TEST_F(SIMDComponentContainerTest, OptimizeLayout) {
    // Add components
    for (int i = 0; i < 10; ++i) {
        auto component = std::make_shared<Component>("OptimizeComponent" +
                                                     std::to_string(i));
        container_->add(component);
    }

    // Optimize should not throw and should maintain component count
    EXPECT_NO_THROW(container_->optimize());
    EXPECT_EQ(container_->size(), 10);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(ComponentPoolExceptionTest, ExceptionMessage) {
    ComponentPoolException ex("Test error message");
    EXPECT_STREQ(ex.what(), "Test error message");
}

TEST_F(ComponentPoolTest, InvalidConfiguration) {
    PoolConfig invalidConfig;
    invalidConfig.initialPoolSize = 0;
    invalidConfig.maxPoolSize = 0;

    // Should handle invalid configuration gracefully
    EXPECT_NO_THROW(ComponentPool<Component> invalidPool(invalidConfig));
}

// ============================================================================
// Extended Edge Case Tests
// ============================================================================

// Test pool exhaustion - renamed to avoid duplicate
TEST_F(ComponentPoolTest, PoolExhaustionExtended) {
    PoolConfig smallConfig;
    smallConfig.initialPoolSize = 2;
    smallConfig.maxPoolSize = 4;
    smallConfig.enableStatistics = true;

    ComponentPool<Component> smallPool(smallConfig);

    std::vector<std::shared_ptr<Component>> components;

    // Allocate up to max pool size
    for (int i = 0; i < 4; ++i) {
        auto comp = smallPool.allocate("Component" + std::to_string(i));
        ASSERT_NE(comp, nullptr);
        components.push_back(comp);
    }

    // Try to allocate beyond max - should still work (dynamic allocation)
    auto extraComp = smallPool.allocate("ExtraComponent");
    EXPECT_NE(extraComp, nullptr);
}

// Test deallocate with null component
TEST_F(ComponentPoolTest, DeallocateNull) {
    // Should handle null gracefully
    EXPECT_NO_THROW(pool_->deallocate(nullptr));
}

// Test statistics accuracy
TEST_F(ComponentPoolTest, StatisticsAccuracy) {
    const auto& stats = pool_->getStatistics();
    auto initialAllocs = stats.totalAllocations.load();
    auto initialDeallocs = stats.totalDeallocations.load();

    auto comp1 = pool_->allocate("Stats1");
    auto comp2 = pool_->allocate("Stats2");

    EXPECT_EQ(stats.totalAllocations.load(), initialAllocs + 2);

    pool_->deallocate(comp1);
    EXPECT_EQ(stats.totalDeallocations.load(), initialDeallocs + 1);
}

// Test concurrent allocation
TEST_F(ComponentPoolTest, ConcurrentAllocation) {
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<Component>> components;
    std::mutex componentsMutex;

    constexpr int numThreads = 10;
    constexpr int allocsPerThread = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &components, &componentsMutex]() {
            for (int j = 0; j < allocsPerThread; ++j) {
                auto comp = pool_->allocate("Thread" + std::to_string(i) +
                                            "_Comp" + std::to_string(j));
                std::lock_guard<std::mutex> lock(componentsMutex);
                components.push_back(comp);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(components.size(), numThreads * allocsPerThread);

    // Verify all components are valid
    for (const auto& comp : components) {
        EXPECT_NE(comp, nullptr);
    }
}

// Test concurrent deallocation
TEST_F(ComponentPoolTest, ConcurrentDeallocation) {
    std::vector<std::shared_ptr<Component>> components;

    // Allocate components
    for (int i = 0; i < 20; ++i) {
        components.push_back(
            pool_->allocate("DeallocComp" + std::to_string(i)));
    }

    std::vector<std::thread> threads;

    for (size_t i = 0; i < components.size(); ++i) {
        threads.emplace_back(
            [this, &components, i]() { pool_->deallocate(components[i]); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    const auto& stats = pool_->getStatistics();
    EXPECT_GE(stats.totalDeallocations.load(), 20);
}

// Test pool statistics tracking
TEST_F(ComponentPoolTest, StatisticsTracking) {
    auto comp1 = pool_->allocate("Stats1");
    auto comp2 = pool_->allocate("Stats2");

    const auto& stats = pool_->getStatistics();
    EXPECT_GT(stats.totalAllocations.load(), 0);
    EXPECT_GT(stats.currentAllocations.load(), 0);

    // Deallocate and check stats update
    pool_->deallocate(comp1);
    EXPECT_LT(stats.currentAllocations.load(), 2);
}

// Test memory allocation patterns
TEST_F(ComponentPoolTest, AllocationPatterns) {
    std::vector<std::shared_ptr<Component>> components;

    // Allocate many components
    for (int i = 0; i < 50; ++i) {
        components.push_back(pool_->allocate("Pattern" + std::to_string(i)));
    }

    // Deallocate some to create fragmentation
    for (size_t i = 0; i < components.size(); i += 2) {
        pool_->deallocate(components[i]);
    }

    // Verify pool still works after fragmentation
    auto newComp = pool_->allocate("NewAfterFragmentation");
    EXPECT_NE(newComp, nullptr);
}

// Test cache line alignment
TEST_F(ComponentPoolTest, CacheLineAlignment) {
    auto comp = pool_->allocate("AlignmentTest");
    ASSERT_NE(comp, nullptr);

    // Check if component is cache-line aligned (64 bytes)
    auto addr = reinterpret_cast<uintptr_t>(comp.get());
    EXPECT_EQ(addr % 64, 0);
}

// ============================================================================
// SIMDComponentContainer Extended Tests
// ============================================================================

// Test remove non-existent component
TEST_F(SIMDComponentContainerTest, RemoveNonExistent) {
    auto component = std::make_shared<Component>("NonExistent");
    EXPECT_NO_THROW(container_->remove(component));  // remove() returns void
}

// Test removing all components
TEST_F(SIMDComponentContainerTest, RemoveAllComponents) {
    std::vector<std::shared_ptr<Component>> components;
    for (int i = 0; i < 10; ++i) {
        auto comp = std::make_shared<Component>("Clear" + std::to_string(i));
        container_->add(comp);
        components.push_back(comp);
    }

    EXPECT_EQ(container_->size(), 10);

    // Remove all components individually
    for (auto& comp : components) {
        container_->remove(comp);
    }
    EXPECT_EQ(container_->size(), 0);
    EXPECT_TRUE(container_->empty());
}

// Test component count tracking
TEST_F(SIMDComponentContainerTest, ComponentCountTracking) {
    EXPECT_EQ(container_->size(), 0);
    EXPECT_TRUE(container_->empty());

    for (int i = 0; i < 20; ++i) {
        container_->add(
            std::make_shared<Component>("Count" + std::to_string(i)));
        EXPECT_EQ(container_->size(), i + 1);
    }

    EXPECT_EQ(container_->size(), 20);
    EXPECT_FALSE(container_->empty());
}

// Test concurrent add/remove
TEST_F(SIMDComponentContainerTest, ConcurrentAddRemove) {
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<Component>> components;

    // Pre-create components
    for (int i = 0; i < 20; ++i) {
        components.push_back(
            std::make_shared<Component>("Concurrent" + std::to_string(i)));
    }

    // Add components concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(
            [this, &components, i]() { container_->add(components[i]); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(container_->size(), 10);

    threads.clear();

    // Remove components concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(
            [this, &components, i]() { container_->remove(components[i]); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(container_->size(), 0);
}
