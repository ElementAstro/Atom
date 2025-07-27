#include "test_framework.hpp"
#include "../../atom/memory/memory.hpp"
#include "../../atom/memory/memory_pool.hpp"
#include <random>
#include <algorithm>

using namespace atom::memory::test;
using namespace atom::memory;

namespace {

/**
 * @brief Test basic memory pool functionality
 */
void testBasicMemoryPool() {
    MemoryPool<1024> pool;
    
    // Test basic allocation
    void* ptr1 = pool.allocate(64);
    ASSERT_TRUE(ptr1 != nullptr);
    ASSERT_TRUE(pool.owns(ptr1));
    
    void* ptr2 = pool.allocate(128);
    ASSERT_TRUE(ptr2 != nullptr);
    ASSERT_TRUE(pool.owns(ptr2));
    ASSERT_NE(ptr1, ptr2);
    
    // Test deallocation
    pool.deallocate(ptr1, 64);
    pool.deallocate(ptr2, 128);
    
    // Test reallocation after deallocation
    void* ptr3 = pool.allocate(64);
    ASSERT_TRUE(ptr3 != nullptr);
    pool.deallocate(ptr3, 64);
}

/**
 * @brief Test memory pool with different allocation strategies
 */
void testAllocationStrategies() {
    // Test FirstFit strategy
    {
        MemoryPool<1024, 8, true, AllocationStrategy::FirstFit> pool;
        std::vector<void*> ptrs;
        
        // Allocate several blocks
        for (int i = 0; i < 10; ++i) {
            void* ptr = pool.allocate(64);
            ASSERT_TRUE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Deallocate every other block to create fragmentation
        for (size_t i = 1; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i], 64);
        }
        
        // Try to allocate again - should use first fit
        void* new_ptr = pool.allocate(64);
        ASSERT_TRUE(new_ptr != nullptr);
        
        // Cleanup
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i], 64);
        }
        pool.deallocate(new_ptr, 64);
    }
    
    // Test BestFit strategy
    {
        MemoryPool<1024, 8, true, AllocationStrategy::BestFit> pool;
        
        void* ptr1 = pool.allocate(100);
        void* ptr2 = pool.allocate(200);
        void* ptr3 = pool.allocate(50);
        
        ASSERT_TRUE(ptr1 && ptr2 && ptr3);
        
        pool.deallocate(ptr1, 100);
        pool.deallocate(ptr2, 200);
        pool.deallocate(ptr3, 50);
    }
}

/**
 * @brief Test memory pool performance monitoring
 */
void testPerformanceMonitoring() {
    MemoryPool<2048> pool;
    
    // Get initial stats
    auto initial_stats = pool.getStats();
    ASSERT_EQ(initial_stats.totalAllocations.load(), 0);
    ASSERT_EQ(initial_stats.currentAllocations.load(), 0);
    
    // Perform some allocations
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate(64);
        ASSERT_TRUE(ptr != nullptr);
        ptrs.push_back(ptr);
    }
    
    // Check stats after allocations
    auto after_alloc_stats = pool.getStats();
    ASSERT_EQ(after_alloc_stats.totalAllocations.load(), 10);
    ASSERT_EQ(after_alloc_stats.currentAllocations.load(), 10);
    ASSERT_GT(after_alloc_stats.currentBytesAllocated.load(), 0);
    
    // Deallocate half
    for (size_t i = 0; i < ptrs.size() / 2; ++i) {
        pool.deallocate(ptrs[i], 64);
    }
    
    // Check stats after partial deallocation
    auto after_dealloc_stats = pool.getStats();
    ASSERT_EQ(after_dealloc_stats.totalAllocations.load(), 10);
    ASSERT_EQ(after_dealloc_stats.currentAllocations.load(), 5);
    
    // Cleanup remaining
    for (size_t i = ptrs.size() / 2; i < ptrs.size(); ++i) {
        pool.deallocate(ptrs[i], 64);
    }
}

/**
 * @brief Test memory pool thread safety
 */
void testThreadSafety() {
    MemoryPool<4096> pool;
    const size_t num_threads = 8;
    const size_t allocations_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_allocations{0};
    std::atomic<size_t> successful_deallocations{0};
    
    // Launch threads that allocate and deallocate
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool, &successful_allocations, &successful_deallocations, allocations_per_thread]() {
            std::vector<void*> local_ptrs;
            
            // Allocate
            for (size_t j = 0; j < allocations_per_thread; ++j) {
                void* ptr = pool.allocate(32);
                if (ptr) {
                    local_ptrs.push_back(ptr);
                    successful_allocations.fetch_add(1);
                }
            }
            
            // Deallocate
            for (void* ptr : local_ptrs) {
                pool.deallocate(ptr, 32);
                successful_deallocations.fetch_add(1);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify results
    ASSERT_GT(successful_allocations.load(), 0);
    ASSERT_EQ(successful_allocations.load(), successful_deallocations.load());
    
    // Pool should be empty now
    auto final_stats = pool.getStats();
    ASSERT_EQ(final_stats.currentAllocations.load(), 0);
}

/**
 * @brief Benchmark memory pool allocation performance
 */
BenchmarkResult benchmarkAllocationPerformance() {
    MemoryPool<8192> pool;
    const size_t iterations = 10000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> ptrs;
    ptrs.reserve(iterations);
    
    // Allocation phase
    for (size_t i = 0; i < iterations; ++i) {
        void* ptr = pool.allocate(64);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }
    
    // Deallocation phase
    for (void* ptr : ptrs) {
        pool.deallocate(ptr, 64);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    BenchmarkResult result;
    result.name = "Memory Pool Allocation/Deallocation";
    result.duration = duration;
    result.iterations = iterations * 2; // Both allocation and deallocation
    result.memory_used = ptrs.size() * 64;
    result.operations_per_second = (result.iterations * 1e9) / duration.count();
    result.additional_info = "Pool size: 8192 bytes, Block size: 64 bytes";
    
    return result;
}

/**
 * @brief Test fragmentation analysis
 */
void testFragmentationAnalysis() {
    MemoryPool<2048> pool;
    
    // Create fragmentation pattern
    std::vector<void*> ptrs;
    
    // Allocate many small blocks
    for (int i = 0; i < 20; ++i) {
        void* ptr = pool.allocate(64);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }
    
    // Deallocate every other block to create fragmentation
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i], 64);
        ptrs[i] = nullptr;
    }
    
    // Get fragmentation metrics
    auto metrics = pool.getPerformanceMetrics();
    double fragmentation_ratio = std::get<2>(metrics); // fragmentation_ratio
    
    // Should have some fragmentation
    ASSERT_GT(fragmentation_ratio, 0.0);
    
    // Try to allocate a larger block - might fail due to fragmentation
    void* large_ptr = pool.allocate(256);
    // Note: This might fail due to fragmentation, which is expected
    
    if (large_ptr) {
        pool.deallocate(large_ptr, 256);
    }
    
    // Cleanup remaining blocks
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        if (ptrs[i]) {
            pool.deallocate(ptrs[i], 64);
        }
    }
}

/**
 * @brief Test memory pool with custom configuration
 */
void testCustomConfiguration() {
    MemoryPoolConfig config;
    config.enable_stats = true;
    config.enable_debugging = true;
    config.enable_prefetching = true;
    config.enable_coalescing = true;
    
    MemoryPool<1024> pool(config);
    
    // Test that configuration is applied
    ASSERT_EQ(pool.getConfig().enable_stats, true);
    ASSERT_EQ(pool.getConfig().enable_debugging, true);
    
    // Test basic functionality with custom config
    void* ptr = pool.allocate(128);
    ASSERT_TRUE(ptr != nullptr);
    
    // Test performance metrics are available
    auto metrics = pool.getPerformanceMetrics();
    // Should have valid metrics due to enabled stats
    
    pool.deallocate(ptr, 128);
}

/**
 * @brief Test memory leak detection
 */
LeakDetectionResult testMemoryLeakDetection() {
    MemoryUsageTracker::reset();
    
    {
        MemoryPool<1024> pool;
        
        // Allocate some memory
        void* ptr1 = pool.allocate(64);
        void* ptr2 = pool.allocate(128);
        
        MemoryUsageTracker::recordAllocation(ptr1, 64);
        MemoryUsageTracker::recordAllocation(ptr2, 128);
        
        // Deallocate only one - simulating a leak
        pool.deallocate(ptr1, 64);
        MemoryUsageTracker::recordDeallocation(ptr1);
        
        // ptr2 is "leaked" (not deallocated)
        // In real scenario, this would be caught by the pool's destructor
        pool.deallocate(ptr2, 128); // Clean up for test
    }
    
    return MemoryUsageTracker::checkForLeaks();
}

} // anonymous namespace

/**
 * @brief Register all memory pool tests
 */
void registerMemoryPoolTests(TestFramework& framework) {
    framework.addTest("BasicMemoryPool", "Test basic memory pool allocation and deallocation", 
                      testBasicMemoryPool);
    
    framework.addTest("AllocationStrategies", "Test different allocation strategies (FirstFit, BestFit)", 
                      testAllocationStrategies);
    
    framework.addTest("PerformanceMonitoring", "Test memory pool performance monitoring and statistics", 
                      testPerformanceMonitoring);
    
    framework.addTest("ThreadSafety", "Test memory pool thread safety with concurrent access", 
                      testThreadSafety);
    
    framework.addTest("FragmentationAnalysis", "Test memory fragmentation analysis and metrics", 
                      testFragmentationAnalysis);
    
    framework.addTest("CustomConfiguration", "Test memory pool with custom configuration options", 
                      testCustomConfiguration);
    
    framework.addBenchmark("AllocationPerformance", "Benchmark memory pool allocation/deallocation performance", 
                           benchmarkAllocationPerformance);
    
    framework.addLeakTest("MemoryLeakDetection", "Test memory leak detection capabilities", 
                          testMemoryLeakDetection);
}
