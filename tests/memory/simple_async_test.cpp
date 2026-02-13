#include <iostream>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <cassert>
#include <cstring>
#include <mutex>

// Simple test framework
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "ASSERTION FAILED: " << #condition << " at line " << __LINE__ << std::endl; \
        return false; \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "ASSERTION FAILED: " << #a << " != " << #b << " (" << (a) << " != " << (b) << ") at line " << __LINE__ << std::endl; \
        return false; \
    }

#define ASSERT_GT(a, b) \
    if ((a) <= (b)) { \
        std::cerr << "ASSERTION FAILED: " << #a << " <= " << #b << " (" << (a) << " <= " << (b) << ") at line " << __LINE__ << std::endl; \
        return false; \
    }

// Simple async memory pool test
class SimpleMemoryPool {
private:
    std::vector<void*> free_blocks_;
    std::mutex mutex_;
    size_t block_size_;
    size_t total_blocks_;
    std::atomic<size_t> allocated_count_{0};

public:
    SimpleMemoryPool(size_t block_size, size_t num_blocks)
        : block_size_(block_size), total_blocks_(num_blocks) {
        // Pre-allocate blocks
        for (size_t i = 0; i < num_blocks; ++i) {
            void* block = std::aligned_alloc(64, block_size);
            if (block) {
                free_blocks_.push_back(block);
            }
        }
    }

    ~SimpleMemoryPool() {
        for (void* block : free_blocks_) {
            std::free(block);
        }
    }

    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_blocks_.empty()) {
            return nullptr;
        }
        void* block = free_blocks_.back();
        free_blocks_.pop_back();
        allocated_count_++;
        return block;
    }

    void deallocate(void* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        free_blocks_.push_back(ptr);
        allocated_count_--;
    }

    size_t getAllocatedCount() const {
        return allocated_count_.load();
    }
};

// Test functions
bool testBasicAsyncOperations() {
    std::cout << "Testing basic async operations..." << std::endl;

    SimpleMemoryPool pool(64, 100);

    const int num_operations = 50;
    std::atomic<int> successful_ops{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_operations; ++i) {
        auto future = std::async(std::launch::async, [&pool, &successful_ops, i]() {
            void* ptr = pool.allocate();
            if (ptr) {
                // Simulate some work
                memset(ptr, i % 256, 32);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                pool.deallocate(ptr);
                successful_ops++;
            }
        });
        futures.push_back(std::move(future));
    }

    // Wait for all operations to complete
    for (auto& future : futures) {
        future.get();
    }

    ASSERT_EQ(successful_ops.load(), num_operations);
    ASSERT_EQ(pool.getAllocatedCount(), 0);

    std::cout << "✓ Basic async operations test passed" << std::endl;
    return true;
}

bool testConcurrentAccess() {
    std::cout << "Testing concurrent access..." << std::endl;

    SimpleMemoryPool pool(128, 200);

    const int num_threads = 8;
    const int ops_per_thread = 25;
    std::atomic<int> total_ops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&pool, &total_ops, ops_per_thread]() {
            std::vector<void*> allocated_ptrs;

            // Allocation phase
            for (int i = 0; i < ops_per_thread; ++i) {
                void* ptr = pool.allocate();
                if (ptr) {
                    allocated_ptrs.push_back(ptr);
                    total_ops++;
                }
            }

            // Deallocation phase
            for (void* ptr : allocated_ptrs) {
                pool.deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_GT(total_ops.load(), 0);
    ASSERT_EQ(pool.getAllocatedCount(), 0);

    std::cout << "✓ Concurrent access test passed (ops: " << total_ops.load() << ")" << std::endl;
    return true;
}

bool testMemoryPoolExhaustion() {
    std::cout << "Testing memory pool exhaustion..." << std::endl;

    SimpleMemoryPool small_pool(64, 10); // Very small pool

    std::vector<void*> allocated_ptrs;
    int successful_allocations = 0;

    // Try to allocate more than available
    for (int i = 0; i < 20; ++i) {
        void* ptr = small_pool.allocate();
        if (ptr) {
            allocated_ptrs.push_back(ptr);
            successful_allocations++;
        }
    }

    ASSERT_EQ(successful_allocations, 10); // Should only get 10
    ASSERT_EQ(small_pool.getAllocatedCount(), 10);

    // Deallocate some and try again
    for (int i = 0; i < 5; ++i) {
        small_pool.deallocate(allocated_ptrs[i]);
    }
    allocated_ptrs.erase(allocated_ptrs.begin(), allocated_ptrs.begin() + 5);

    // Should be able to allocate again
    void* new_ptr = small_pool.allocate();
    ASSERT_TRUE(new_ptr != nullptr);
    allocated_ptrs.push_back(new_ptr);

    // Clean up
    for (void* ptr : allocated_ptrs) {
        small_pool.deallocate(ptr);
    }

    ASSERT_EQ(small_pool.getAllocatedCount(), 0);

    std::cout << "✓ Memory pool exhaustion test passed" << std::endl;
    return true;
}

bool testPerformanceUnderLoad() {
    std::cout << "Testing performance under load..." << std::endl;

    SimpleMemoryPool pool(256, 1000);

    const int num_threads = 4;
    const int ops_per_thread = 100;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::atomic<int> completed_ops{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&pool, &completed_ops, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                void* ptr = pool.allocate();
                if (ptr) {
                    // Simulate work
                    memset(ptr, i % 256, 128);
                    pool.deallocate(ptr);
                    completed_ops++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    double ops_per_second = (completed_ops.load() * 1000000.0) / duration.count();

    ASSERT_GT(completed_ops.load(), num_threads * ops_per_thread * 0.8); // At least 80% success
    ASSERT_EQ(pool.getAllocatedCount(), 0);

    std::cout << "✓ Performance test passed: " << ops_per_second << " ops/sec" << std::endl;
    return true;
}

bool testAsyncErrorHandling() {
    std::cout << "Testing async error handling..." << std::endl;

    SimpleMemoryPool pool(64, 5); // Very small pool to force errors

    const int num_futures = 20;
    std::vector<std::future<bool>> futures;
    std::atomic<int> successful_ops{0};
    std::atomic<int> failed_ops{0};

    for (int i = 0; i < num_futures; ++i) {
        auto future = std::async(std::launch::async, [&pool, &successful_ops, &failed_ops]() {
            void* ptr = pool.allocate();
            if (ptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                pool.deallocate(ptr);
                successful_ops++;
                return true;
            } else {
                failed_ops++;
                return false;
            }
        });
        futures.push_back(std::move(future));
    }

    // Wait for all futures
    for (auto& future : futures) {
        future.get();
    }

    ASSERT_GT(successful_ops.load(), 0);
    ASSERT_GT(failed_ops.load(), 0); // Should have some failures due to small pool
    ASSERT_EQ(successful_ops.load() + failed_ops.load(), num_futures);
    ASSERT_EQ(pool.getAllocatedCount(), 0);

    std::cout << "✓ Error handling test passed (success: " << successful_ops.load()
              << ", failed: " << failed_ops.load() << ")" << std::endl;
    return true;
}

int main() {
    std::cout << "Running comprehensive async memory tests..." << std::endl;
    std::cout << "=========================================" << std::endl;

    bool all_passed = true;

    all_passed &= testBasicAsyncOperations();
    all_passed &= testConcurrentAccess();
    all_passed &= testMemoryPoolExhaustion();
    all_passed &= testPerformanceUnderLoad();
    all_passed &= testAsyncErrorHandling();

    std::cout << "=========================================" << std::endl;
    if (all_passed) {
        std::cout << "✓ All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests FAILED!" << std::endl;
        return 1;
    }
}
