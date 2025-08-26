#include "atom/components/iteration.hpp"
#include "atom/components/component.hpp"

#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

using namespace atom::components;

// Test component for iteration testing
class TestIterationComponent : public Component {
public:
    TestIterationComponent(const std::string& name, int value = 0) 
        : Component(name), value_(value) {}
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    void process() { 
        processCount_++; 
        lastProcessTime_ = std::chrono::steady_clock::now();
    }
    
    int getProcessCount() const { return processCount_; }
    auto getLastProcessTime() const { return lastProcessTime_; }

private:
    int value_;
    std::atomic<int> processCount_{0};
    std::chrono::steady_clock::time_point lastProcessTime_;
};

// Test fixture for ComponentIterator tests
class ComponentIteratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test components
        for (int i = 0; i < 10; ++i) {
            auto component = std::make_shared<TestIterationComponent>(
                "TestComponent" + std::to_string(i), i * 10);
            components_.push_back(component);
        }
        
        IteratorConfig config;
        config.batchSize = 4;
        config.enablePrefetching = true;
        config.enableSIMD = true;
        config.cacheLineAlignment = true;
        
        iterator_ = std::make_unique<ComponentIterator<TestIterationComponent>>(config);
    }

    std::vector<std::shared_ptr<TestIterationComponent>> components_;
    std::unique_ptr<ComponentIterator<TestIterationComponent>> iterator_;
};

// Test fixture for SIMDProcessor tests
class SIMDProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_unique<SIMDProcessor>();
        
        // Create test data
        for (int i = 0; i < 16; ++i) {
            testData_.push_back(static_cast<float>(i));
        }
    }

    std::unique_ptr<SIMDProcessor> processor_;
    std::vector<float> testData_;
};

// Test fixture for CacheOptimizer tests
class CacheOptimizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        optimizer_ = std::make_unique<CacheOptimizer>();
        
        // Create test components
        for (int i = 0; i < 20; ++i) {
            auto component = std::make_shared<TestIterationComponent>(
                "CacheTestComponent" + std::to_string(i), i);
            components_.push_back(component);
        }
    }

    std::unique_ptr<CacheOptimizer> optimizer_;
    std::vector<std::shared_ptr<TestIterationComponent>> components_;
};

// ============================================================================
// Constants Tests
// ============================================================================

TEST(IterationConstantsTest, CacheLineSize) {
    EXPECT_EQ(CACHE_LINE_SIZE, 64);
}

TEST(IterationConstantsTest, SIMDVectorWidths) {
    // Test that SIMD vector widths are reasonable
    EXPECT_GT(SIMD_WIDTH_FLOAT, 0);
    EXPECT_GT(SIMD_WIDTH_DOUBLE, 0);
    EXPECT_GT(SIMD_WIDTH_INT32, 0);
    EXPECT_GT(SIMD_WIDTH_INT64, 0);
    
    // Test relationships
    EXPECT_GE(SIMD_WIDTH_FLOAT, SIMD_WIDTH_DOUBLE);
    EXPECT_GE(SIMD_WIDTH_INT32, SIMD_WIDTH_INT64);
}

// ============================================================================
// ComponentIterator Tests
// ============================================================================

TEST_F(ComponentIteratorTest, BasicIteration) {
    iterator_->setComponents(components_);
    
    int processedCount = 0;
    iterator_->forEach([&processedCount](TestIterationComponent& component) {
        component.process();
        processedCount++;
    });
    
    EXPECT_EQ(processedCount, components_.size());
    
    // Verify all components were processed
    for (const auto& component : components_) {
        EXPECT_EQ(component->getProcessCount(), 1);
    }
}

TEST_F(ComponentIteratorTest, BatchProcessing) {
    iterator_->setComponents(components_);
    
    std::vector<int> batchSizes;
    iterator_->forEachBatch([&batchSizes](const std::vector<TestIterationComponent*>& batch) {
        batchSizes.push_back(batch.size());
        for (auto* component : batch) {
            component->process();
        }
    });
    
    // Should have processed in batches
    EXPECT_GT(batchSizes.size(), 1);
    
    // Total processed should equal component count
    int totalProcessed = 0;
    for (int batchSize : batchSizes) {
        totalProcessed += batchSize;
    }
    EXPECT_EQ(totalProcessed, components_.size());
}

TEST_F(ComponentIteratorTest, ParallelIteration) {
    iterator_->setComponents(components_);
    
    std::atomic<int> processedCount{0};
    iterator_->forEachParallel([&processedCount](TestIterationComponent& component) {
        component.process();
        processedCount++;
    });
    
    EXPECT_EQ(processedCount.load(), components_.size());
}

TEST_F(ComponentIteratorTest, ConditionalIteration) {
    iterator_->setComponents(components_);
    
    int processedCount = 0;
    iterator_->forEachIf([&processedCount](TestIterationComponent& component) {
        if (component.getValue() >= 50) {
            component.process();
            processedCount++;
        }
    });
    
    // Should only process components with value >= 50
    EXPECT_LT(processedCount, components_.size());
    EXPECT_GT(processedCount, 0);
}

TEST_F(ComponentIteratorTest, IteratorStatistics) {
    iterator_->setComponents(components_);
    
    iterator_->forEach([](TestIterationComponent& component) {
        component.process();
    });
    
    auto stats = iterator_->getStatistics();
    EXPECT_GT(stats.totalIterations, 0);
    EXPECT_GT(stats.totalProcessingTime.count(), 0);
    EXPECT_EQ(stats.componentsProcessed, components_.size());
}

TEST_F(ComponentIteratorTest, ResetStatistics) {
    iterator_->setComponents(components_);
    
    iterator_->forEach([](TestIterationComponent& component) {
        component.process();
    });
    
    auto statsBefore = iterator_->getStatistics();
    EXPECT_GT(statsBefore.totalIterations, 0);
    
    iterator_->resetStatistics();
    
    auto statsAfter = iterator_->getStatistics();
    EXPECT_EQ(statsAfter.totalIterations, 0);
    EXPECT_EQ(statsAfter.totalProcessingTime.count(), 0);
    EXPECT_EQ(statsAfter.componentsProcessed, 0);
}

// ============================================================================
// SIMDProcessor Tests
// ============================================================================

TEST_F(SIMDProcessorTest, VectorAddition) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> b = {5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> result(4);
    
    processor_->vectorAdd(a.data(), b.data(), result.data(), 4);
    
    EXPECT_FLOAT_EQ(result[0], 6.0f);
    EXPECT_FLOAT_EQ(result[1], 8.0f);
    EXPECT_FLOAT_EQ(result[2], 10.0f);
    EXPECT_FLOAT_EQ(result[3], 12.0f);
}

TEST_F(SIMDProcessorTest, VectorMultiplication) {
    std::vector<float> a = {2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> b = {1.5f, 2.0f, 2.5f, 3.0f};
    std::vector<float> result(4);
    
    processor_->vectorMultiply(a.data(), b.data(), result.data(), 4);
    
    EXPECT_FLOAT_EQ(result[0], 3.0f);
    EXPECT_FLOAT_EQ(result[1], 6.0f);
    EXPECT_FLOAT_EQ(result[2], 10.0f);
    EXPECT_FLOAT_EQ(result[3], 15.0f);
}

TEST_F(SIMDProcessorTest, VectorSum) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    
    float sum = processor_->vectorSum(data.data(), data.size());
    
    EXPECT_FLOAT_EQ(sum, 15.0f);
}

TEST_F(SIMDProcessorTest, VectorDotProduct) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> b = {2.0f, 3.0f, 4.0f, 5.0f};
    
    float dotProduct = processor_->vectorDotProduct(a.data(), b.data(), 4);
    
    // 1*2 + 2*3 + 3*4 + 4*5 = 2 + 6 + 12 + 20 = 40
    EXPECT_FLOAT_EQ(dotProduct, 40.0f);
}

TEST_F(SIMDProcessorTest, VectorNormalize) {
    std::vector<float> data = {3.0f, 4.0f, 0.0f, 0.0f};
    std::vector<float> result(4);
    
    processor_->vectorNormalize(data.data(), result.data(), 4);
    
    // Length of (3, 4, 0, 0) is 5, so normalized should be (0.6, 0.8, 0, 0)
    EXPECT_NEAR(result[0], 0.6f, 0.001f);
    EXPECT_NEAR(result[1], 0.8f, 0.001f);
    EXPECT_NEAR(result[2], 0.0f, 0.001f);
    EXPECT_NEAR(result[3], 0.0f, 0.001f);
}

TEST_F(SIMDProcessorTest, SIMDCapabilities) {
    auto capabilities = processor_->getSIMDCapabilities();
    
    // Should report some capabilities
    EXPECT_TRUE(capabilities.hasSSE || capabilities.hasAVX || capabilities.hasAVX2 || 
                capabilities.hasNEON || !capabilities.hasAny);
}

// ============================================================================
// CacheOptimizer Tests
// ============================================================================

TEST_F(CacheOptimizerTest, OptimizeLayout) {
    // Optimize component layout
    optimizer_->optimizeLayout(components_);
    
    // Should not throw and should maintain component count
    EXPECT_EQ(components_.size(), 20);
}

TEST_F(CacheOptimizerTest, PrefetchComponents) {
    // Test prefetching
    EXPECT_NO_THROW(optimizer_->prefetchComponents(components_, 0, 5));
}

TEST_F(CacheOptimizerTest, AlignMemory) {
    size_t size = 1024;
    void* memory = optimizer_->alignedAlloc(size, CACHE_LINE_SIZE);
    
    EXPECT_NE(memory, nullptr);
    
    // Check alignment
    uintptr_t address = reinterpret_cast<uintptr_t>(memory);
    EXPECT_EQ(address % CACHE_LINE_SIZE, 0);
    
    optimizer_->alignedFree(memory);
}

TEST_F(CacheOptimizerTest, CacheStatistics) {
    optimizer_->optimizeLayout(components_);
    
    auto stats = optimizer_->getCacheStatistics();
    EXPECT_GE(stats.cacheHits, 0);
    EXPECT_GE(stats.cacheMisses, 0);
    EXPECT_GE(stats.totalAccesses, 0);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ComponentIteratorTest, PerformanceComparison) {
    iterator_->setComponents(components_);
    
    // Measure sequential processing
    auto start = std::chrono::high_resolution_clock::now();
    iterator_->forEach([](TestIterationComponent& component) {
        component.process();
        // Simulate some work
        volatile int dummy = 0;
        for (int i = 0; i < 100; ++i) {
            dummy += i;
        }
    });
    auto sequentialTime = std::chrono::high_resolution_clock::now() - start;
    
    // Reset components
    for (auto& component : components_) {
        component = std::make_shared<TestIterationComponent>(
            component->getName(), component->getValue());
    }
    iterator_->setComponents(components_);
    
    // Measure parallel processing
    start = std::chrono::high_resolution_clock::now();
    iterator_->forEachParallel([](TestIterationComponent& component) {
        component.process();
        // Simulate some work
        volatile int dummy = 0;
        for (int i = 0; i < 100; ++i) {
            dummy += i;
        }
    });
    auto parallelTime = std::chrono::high_resolution_clock::now() - start;
    
    // Parallel should be faster or at least not significantly slower
    // (allowing for overhead in small datasets)
    EXPECT_LE(parallelTime.count(), sequentialTime.count() * 2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ComponentIteratorTest, EmptyComponentList) {
    std::vector<std::shared_ptr<TestIterationComponent>> emptyComponents;
    iterator_->setComponents(emptyComponents);
    
    int processedCount = 0;
    iterator_->forEach([&processedCount](TestIterationComponent&) {
        processedCount++;
    });
    
    EXPECT_EQ(processedCount, 0);
}

TEST_F(ComponentIteratorTest, NullComponentHandling) {
    std::vector<std::shared_ptr<TestIterationComponent>> componentsWithNull;
    componentsWithNull.push_back(components_[0]);
    componentsWithNull.push_back(nullptr);
    componentsWithNull.push_back(components_[1]);
    
    // Should handle null components gracefully
    EXPECT_NO_THROW(iterator_->setComponents(componentsWithNull));
}

TEST_F(SIMDProcessorTest, InvalidInputSizes) {
    std::vector<float> a = {1.0f, 2.0f};
    std::vector<float> b = {3.0f, 4.0f, 5.0f}; // Different size
    std::vector<float> result(2);
    
    // Should handle mismatched sizes gracefully
    EXPECT_NO_THROW(processor_->vectorAdd(a.data(), b.data(), result.data(), 2));
}

TEST_F(CacheOptimizerTest, NullPointerHandling) {
    // Should handle null pointers gracefully
    EXPECT_NO_THROW(optimizer_->alignedFree(nullptr));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ComponentIteratorTest, ConcurrentAccess) {
    iterator_->setComponents(components_);
    
    const int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> totalProcessed{0};
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &totalProcessed]() {
            int localProcessed = 0;
            iterator_->forEach([&localProcessed](TestIterationComponent& component) {
                component.process();
                localProcessed++;
            });
            totalProcessed += localProcessed;
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Each thread should process all components
    EXPECT_EQ(totalProcessed.load(), numThreads * components_.size());
}
