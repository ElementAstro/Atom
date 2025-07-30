#include "test_framework.hpp"
#include "../../atom/memory/memory_pool.hpp"
#include <random>
#include <algorithm>

using namespace atom::memory::test;

namespace {

/**
 * @brief Test basic memory pool functionality
 */
void testBasicMemoryPool() {
    atom::memory::MemoryPool<64> pool;

    // Test basic allocation
    void* ptr1 = pool.allocate();
    ASSERT_TRUE(ptr1 != nullptr);

    void* ptr2 = pool.allocate();
    ASSERT_TRUE(ptr2 != nullptr);
    ASSERT_NE(ptr1, ptr2);

    // Test deallocation
    pool.deallocate(ptr1);
    pool.deallocate(ptr2);

    // Test reallocation after deallocation
    void* ptr3 = pool.allocate();
    ASSERT_TRUE(ptr3 != nullptr);
    pool.deallocate(ptr3);
}

/**
 * @brief Test memory pool with different configurations
 */
void testAllocationStrategies() {
    // Test with lock-free enabled
    {
        atom::memory::MemoryPool<64, 1024, true> pool;
        std::vector<void*> ptrs;

        // Allocate several blocks
        for (int i = 0; i < 10; ++i) {
            void* ptr = pool.allocate();
            ASSERT_TRUE(ptr != nullptr);
            ptrs.push_back(ptr);
        }

        // Deallocate every other block to create fragmentation
        for (size_t i = 1; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i]);
        }

        // Try to allocate again
        void* new_ptr = pool.allocate();
        ASSERT_TRUE(new_ptr != nullptr);

        // Cleanup
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i]);
        }
        pool.deallocate(new_ptr);
    }

    // Test with lock-free disabled
    {
        atom::memory::MemoryPool<128, 512, false> pool;

        void* ptr1 = pool.allocate();
        void* ptr2 = pool.allocate();
        void* ptr3 = pool.allocate();

        ASSERT_TRUE(ptr1 && ptr2 && ptr3);

        pool.deallocate(ptr1);
        pool.deallocate(ptr2);
        pool.deallocate(ptr3);
    }
}

/**
 * @brief Test memory pool performance monitoring
 */
void testPerformanceMonitoring() {
    atom::memory::MemoryPool<64> pool;

    // Get initial stats
    atom::memory::FixedPoolStats initial_stats;
    pool.getDetailedStats(initial_stats);
    ASSERT_EQ(initial_stats.total_allocations.load(), 0);
    ASSERT_EQ(initial_stats.current_allocations.load(), 0);

    // Perform some allocations
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate();
        ASSERT_TRUE(ptr != nullptr);
        ptrs.push_back(ptr);
    }

    // Check stats after allocations
    atom::memory::FixedPoolStats after_alloc_stats;
    pool.getDetailedStats(after_alloc_stats);
    ASSERT_EQ(after_alloc_stats.total_allocations.load(), 10);
    ASSERT_EQ(after_alloc_stats.current_allocations.load(), 10);

    // Deallocate half
    for (size_t i = 0; i < ptrs.size() / 2; ++i) {
        pool.deallocate(ptrs[i]);
    }

    // Check stats after partial deallocation
    atom::memory::FixedPoolStats after_dealloc_stats;
    pool.getDetailedStats(after_dealloc_stats);
    ASSERT_EQ(after_dealloc_stats.total_allocations.load(), 10);
    ASSERT_EQ(after_dealloc_stats.current_allocations.load(), 5);

    // Cleanup remaining
    for (size_t i = ptrs.size() / 2; i < ptrs.size(); ++i) {
        pool.deallocate(ptrs[i]);
    }
}

/**
 * @brief Test memory pool thread safety
 */
void testThreadSafety() {
    atom::memory::MemoryPool<64, 1024, true> pool;  // Enable lock-free for better thread safety
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
                void* ptr = pool.allocate();
                if (ptr) {
                    local_ptrs.push_back(ptr);
                    successful_allocations.fetch_add(1);
                }
            }

            // Deallocate
            for (void* ptr : local_ptrs) {
                pool.deallocate(ptr);
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
    atom::memory::FixedPoolStats final_stats;
    pool.getDetailedStats(final_stats);
    ASSERT_EQ(final_stats.current_allocations.load(), 0);
}

/**
 * @brief Benchmark memory pool allocation performance
 */
BenchmarkResult benchmarkAllocationPerformance() {
    atom::memory::MemoryPool<64> pool;
    const size_t iterations = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    ptrs.reserve(iterations);

    // Allocation phase
    for (size_t i = 0; i < iterations; ++i) {
        void* ptr = pool.allocate();
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    // Deallocation phase
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    BenchmarkResult result;
    result.name = "Memory Pool Allocation/Deallocation";
    result.duration = duration;
    result.iterations = iterations * 2; // Both allocation and deallocation
    result.memory_used = ptrs.size() * 64;
    result.operations_per_second = (result.iterations * 1e9) / duration.count();
    result.additional_info = "Block size: 64 bytes";

    return result;
}

/**
 * @brief Test fragmentation analysis
 */
void testFragmentationAnalysis() {
    atom::memory::MemoryPool<64> pool;

    // Create fragmentation pattern
    std::vector<void*> ptrs;

    // Allocate many small blocks
    for (int i = 0; i < 20; ++i) {
        void* ptr = pool.allocate();
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    // Deallocate every other block to create fragmentation
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }

    // Get utilization metrics
    auto metrics = pool.getUtilizationStats();
    double utilization_ratio = std::get<0>(metrics); // utilization_ratio
    size_t current_allocations = std::get<2>(metrics);

    // Should have some allocations
    ASSERT_GT(current_allocations, 0);

    // Try to allocate another block
    void* new_ptr = pool.allocate();
    // Should succeed since we have free blocks

    if (new_ptr) {
        pool.deallocate(new_ptr);
    }

    // Cleanup remaining blocks
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        if (ptrs[i]) {
            pool.deallocate(ptrs[i]);
        }
    }
}

/**
 * @brief Test memory pool with custom configuration
 */
void testCustomConfiguration() {
    // Test with different block sizes and configurations
    atom::memory::MemoryPool<128, 512, true> pool_lockfree;
    atom::memory::MemoryPool<32, 2048, false> pool_mutex;

    // Test basic functionality with lock-free pool
    void* ptr1 = pool_lockfree.allocate();
    ASSERT_TRUE(ptr1 != nullptr);
    pool_lockfree.deallocate(ptr1);

    // Test basic functionality with mutex-based pool
    void* ptr2 = pool_mutex.allocate();
    ASSERT_TRUE(ptr2 != nullptr);
    pool_mutex.deallocate(ptr2);

    // Test stats are being collected
    atom::memory::FixedPoolStats stats;
    pool_lockfree.getDetailedStats(stats);
    ASSERT_GT(stats.total_allocations.load(), 0);
}

/**
 * @brief Test memory leak detection
 */
LeakDetectionResult testMemoryLeakDetection() {
    LeakDetectionResult result;
    result.leaked_allocations = 0;
    result.leaked_bytes = 0;

    {
        atom::memory::MemoryPool<64> pool;

        // Allocate some memory
        void* ptr1 = pool.allocate();
        void* ptr2 = pool.allocate();

        // Get initial stats
        atom::memory::FixedPoolStats initial_stats;
        pool.getDetailedStats(initial_stats);

        // Deallocate both properly
        pool.deallocate(ptr1);
        pool.deallocate(ptr2);

        // Get final stats
        atom::memory::FixedPoolStats final_stats;
        pool.getDetailedStats(final_stats);

        // Check that allocations and deallocations match
        result.leaked_allocations = final_stats.current_allocations.load();
        result.leaked_bytes = result.leaked_allocations * 64; // block size
    }

    return result;
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
