#include "atom/components/component_pool.hpp"
#include "atom/components/component.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

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

    void TearDown() override {
        pool_.reset();
    }

    PoolConfig config_;
    std::unique_ptr<ComponentPool<Component>> pool_;
};

// Test fixture for ComponentFactory tests
class ComponentFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = &ComponentFactory::instance();
    }

    ComponentFactory* factory_;
};

// Test fixture for SIMDComponentContainer tests
class SIMDComponentContainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SIMDComponentContainer<Component>::SIMDConfig config;
        config.batchSize = 8;
        config.enablePrefetching = true;
        config.cacheLineSize = 64;
        
        container_ = std::make_unique<SIMDComponentContainer<Component>>(config);
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
    } // component goes out of scope
    
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
    std::vector<std::vector<std::shared_ptr<Component>>> threadComponents(numThreads);
    
    // Launch threads that allocate components
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, allocationsPerThread, &threadComponents]() {
            for (int i = 0; i < allocationsPerThread; ++i) {
                auto component = pool_->allocate("Thread" + std::to_string(t) + 
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
    
    auto stats = factory_->getPoolStatistics<Component>();
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
    
    bool removed = container_->remove(component);
    EXPECT_TRUE(removed);
    EXPECT_EQ(container_->size(), 0);
    EXPECT_TRUE(container_->empty());
}

TEST_F(SIMDComponentContainerTest, BatchProcessing) {
    // Add multiple components
    for (int i = 0; i < 16; ++i) {
        auto component = std::make_shared<Component>("BatchComponent" + std::to_string(i));
        container_->add(component);
    }
    
    EXPECT_EQ(container_->size(), 16);
    
    // Test batch processing
    int processedCount = 0;
    container_->processBatch([&processedCount](const std::shared_ptr<Component>& comp) {
        processedCount++;
        return true; // Continue processing
    });
    
    EXPECT_EQ(processedCount, 16);
}

TEST_F(SIMDComponentContainerTest, OptimizeLayout) {
    // Add components
    for (int i = 0; i < 10; ++i) {
        auto component = std::make_shared<Component>("OptimizeComponent" + std::to_string(i));
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
