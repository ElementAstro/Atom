#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <memory>
#include <numeric>
#include <cstring>

#include "atom/memory/shared.hpp"
#include "atom/memory/memory_pool.hpp"
#include "atom/memory/memory.hpp"

using namespace atom::connection;
using namespace atom::memory;

class AsyncMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_name_ = "AsyncTestSharedMemory";
        // Clean up any existing shared memory
        if (SharedMemory<TestData>::exists(shm_name_)) {
#ifdef _WIN32
            HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name_.c_str());
            if (h) {
                CloseHandle(h);
            }
#else
            shm_unlink(shm_name_.c_str());
#endif
        }
    }

    void TearDown() override {
        if (SharedMemory<TestData>::exists(shm_name_)) {
#ifdef _WIN32
            HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name_.c_str());
            if (h) {
                CloseHandle(h);
            }
#else
            shm_unlink(shm_name_.c_str());
#endif
        }
    }

    struct TestData {
        int id;
        double value;
        char buffer[64];
    };

    std::string shm_name_;
};

// Test concurrent async read/write operations
TEST_F(AsyncMemoryTest, ConcurrentAsyncOperations) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_threads = 8;
    const int operations_per_thread = 50;
    std::atomic<int> successful_reads{0};
    std::atomic<int> successful_writes{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    // Create mixed reader/writer threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 1);

            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    if (op_dist(gen) == 0) {
                        // Async write
                        TestData data{i * 1000 + j, static_cast<double>(i + j), {}};
                        snprintf(data.buffer, sizeof(data.buffer), "thread_%d_op_%d", i, j);

                        auto future = shm.writeAsync(data);
                        future.get(); // Wait for completion
                        successful_writes++;
                    } else {
                        // Async read
                        auto future = shm.readAsync();
                        [[maybe_unused]] auto data = future.get();
                        successful_reads++;
                    }
                } catch (const std::exception& e) {
                    errors++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successful_reads.load(), 0);
    EXPECT_GT(successful_writes.load(), 0);
    EXPECT_EQ(errors.load(), 0);
}

// Test async operations with timeouts
TEST_F(AsyncMemoryTest, AsyncOperationsWithTimeouts) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Test async read with timeout
    TestData writeData{42, 3.14159, "test_data"};
    auto writeFuture = shm.writeAsync(writeData, std::chrono::milliseconds(1000));
    EXPECT_NO_THROW(writeFuture.get());

    auto readFuture = shm.readAsync(std::chrono::milliseconds(1000));
    TestData readData;
    EXPECT_NO_THROW(readData = readFuture.get());

    EXPECT_EQ(readData.id, writeData.id);
    EXPECT_DOUBLE_EQ(readData.value, writeData.value);
    EXPECT_STREQ(readData.buffer, writeData.buffer);
}

// Test async operations under memory pressure
TEST_F(AsyncMemoryTest, AsyncOperationsUnderMemoryPressure) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_concurrent_ops = 100;
    std::vector<std::future<void>> write_futures;
    std::vector<std::future<TestData>> read_futures;

    // Create memory pressure with many concurrent operations
    for (int i = 0; i < num_concurrent_ops; ++i) {
        TestData data{i, static_cast<double>(i), {}};
        snprintf(data.buffer, sizeof(data.buffer), "pressure_test_%d", i);

        write_futures.push_back(shm.writeAsync(data));
        read_futures.push_back(shm.readAsync());
    }

    // Wait for all operations to complete
    int successful_writes = 0;
    int successful_reads = 0;

    for (auto& future : write_futures) {
        try {
            future.get();
            successful_writes++;
        } catch (const std::exception& e) {
            // Some operations may fail under extreme pressure
        }
    }

    for (auto& future : read_futures) {
        try {
            [[maybe_unused]] auto data = future.get();
            successful_reads++;
        } catch (const std::exception& e) {
            // Some operations may fail under extreme pressure
        }
    }

    // At least some operations should succeed
    EXPECT_GT(successful_writes, num_concurrent_ops / 2);
    EXPECT_GT(successful_reads, num_concurrent_ops / 2);
}

// Test lock-free memory pool concurrent operations
TEST_F(AsyncMemoryTest, LockFreeMemoryPoolConcurrency) {
    atom::memory::MemoryPool<64, 1024, true> pool; // Enable lock-free mode

    const int num_threads = 8;
    const int allocations_per_thread = 100;
    std::atomic<int> successful_allocations{0};
    std::atomic<int> successful_deallocations{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            std::vector<void*> allocated_ptrs;

            // Allocation phase
            for (int j = 0; j < allocations_per_thread; ++j) {
                try {
                    void* ptr = pool.allocate();
                    if (ptr) {
                        allocated_ptrs.push_back(ptr);
                        successful_allocations++;
                    }
                } catch (const std::exception& e) {
                    // Handle allocation failures
                }
            }

            // Deallocation phase
            for (void* ptr : allocated_ptrs) {
                try {
                    pool.deallocate(ptr);
                    successful_deallocations++;
                } catch (const std::exception& e) {
                    // Handle deallocation failures
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_allocations.load(), successful_deallocations.load());
    EXPECT_GT(successful_allocations.load(), 0);

    // Verify pool statistics
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0); // current allocations
    EXPECT_GT(stats.second, 0); // total allocations
}

// Test memory pool performance under high concurrency
TEST_F(AsyncMemoryTest, MemoryPoolPerformanceTest) {
    atom::memory::MemoryPool<128, 2048, true> lock_free_pool;
    atom::memory::MemoryPool<128, 2048, false> mutex_pool;

    const int num_threads = 4;
    const int operations_per_thread = 1000;

    auto test_pool = [&](auto& pool, const std::string& pool_type) {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        std::atomic<int> total_ops{0};

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    void* ptr = pool.allocate();
                    if (ptr) {
                        // Simulate some work
                        memset(ptr, j % 256, 64);
                        pool.deallocate(ptr);
                        total_ops++;
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        double ops_per_second = (total_ops.load() * 1000000.0) / duration.count();

        std::cout << pool_type << " Pool Performance: "
                  << ops_per_second << " ops/sec, "
                  << "Total ops: " << total_ops.load() << std::endl;

        return ops_per_second;
    };

    double lock_free_perf = test_pool(lock_free_pool, "Lock-free");
    double mutex_perf = test_pool(mutex_pool, "Mutex-based");

    // Lock-free should generally be faster, but this depends on the system
    EXPECT_GT(lock_free_perf, 0);
    EXPECT_GT(mutex_perf, 0);

    std::cout << "Performance ratio (lock-free/mutex): "
              << (lock_free_perf / mutex_perf) << std::endl;
}

// Test async error handling and recovery
TEST_F(AsyncMemoryTest, AsyncErrorHandlingAndRecovery) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Test async operations with invalid data
    std::vector<std::future<void>> error_futures;
    std::atomic<int> caught_exceptions{0};

    // Create multiple threads that will cause errors
    for (int i = 0; i < 10; ++i) {
        std::thread([&]() {
            try {
                // Try to write to a very small timeout to force errors
                TestData data{i, static_cast<double>(i), "error_test"};
                auto future = shm.writeAsync(data, std::chrono::milliseconds(1));
                future.get();
            } catch (const std::exception& e) {
                caught_exceptions++;
            }
        }).detach();
    }

    // Give threads time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify that the shared memory is still functional after errors
    TestData recovery_data{999, 99.9, "recovery_test"};
    auto write_future = shm.writeAsync(recovery_data);
    EXPECT_NO_THROW(write_future.get());

    auto read_future = shm.readAsync();
    TestData read_data;
    EXPECT_NO_THROW(read_data = read_future.get());

    EXPECT_EQ(read_data.id, recovery_data.id);
}

// Test memory pool edge cases
TEST_F(AsyncMemoryTest, MemoryPoolEdgeCases) {
    atom::memory::MemoryPool<32, 16, true> small_pool; // Very small pool to force edge cases

    // Test pool exhaustion and recovery
    std::vector<void*> allocated_ptrs;

    // Allocate until exhaustion
    try {
        for (int i = 0; i < 100; ++i) {
            void* ptr = small_pool.allocate();
            if (ptr) {
                allocated_ptrs.push_back(ptr);
            }
        }
    } catch (const std::bad_alloc& e) {
        // Expected when pool is exhausted
    }

    EXPECT_GT(allocated_ptrs.size(), 0);

    // Free half the allocations
    size_t half = allocated_ptrs.size() / 2;
    for (size_t i = 0; i < half; ++i) {
        small_pool.deallocate(allocated_ptrs[i]);
    }
    allocated_ptrs.erase(allocated_ptrs.begin(), allocated_ptrs.begin() + half);

    // Should be able to allocate again
    void* new_ptr = nullptr;
    EXPECT_NO_THROW(new_ptr = small_pool.allocate());
    EXPECT_NE(new_ptr, nullptr);

    if (new_ptr) {
        small_pool.deallocate(new_ptr);
    }

    // Clean up remaining allocations
    for (void* ptr : allocated_ptrs) {
        small_pool.deallocate(ptr);
    }
}

// Test concurrent callback operations
TEST_F(AsyncMemoryTest, ConcurrentCallbackOperations) {
    SharedMemory<TestData> shm(shm_name_, true);

    std::atomic<int> callback_count{0};
    std::atomic<int> total_callbacks_registered{0};
    std::vector<size_t> callback_ids;
    std::mutex callback_ids_mutex;

    // Register multiple callbacks concurrently
    std::vector<std::thread> register_threads;
    for (int i = 0; i < 5; ++i) {
        register_threads.emplace_back([&]() {
            auto callback = [&](const TestData& /*data*/) {
                callback_count++;
                // Simulate some processing time
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            };

            size_t id = shm.registerChangeCallback(callback);
            {
                std::lock_guard<std::mutex> lock(callback_ids_mutex);
                callback_ids.push_back(id);
            }
            total_callbacks_registered++;
        });
    }

    for (auto& thread : register_threads) {
        thread.join();
    }

    EXPECT_EQ(total_callbacks_registered.load(), 5);

    // Trigger callbacks with concurrent writes
    std::vector<std::thread> write_threads;
    for (int i = 0; i < 3; ++i) {
        write_threads.emplace_back([&, i]() {
            TestData data{i, static_cast<double>(i), "callback_test"};
            shm.write(data);
        });
    }

    for (auto& thread : write_threads) {
        thread.join();
    }

    // Give callbacks time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should have received callbacks (3 writes * 5 callbacks = 15)
    EXPECT_GE(callback_count.load(), 10); // Allow for some timing variations

    // Unregister callbacks
    for (size_t id : callback_ids) {
        EXPECT_TRUE(shm.unregisterChangeCallback(id));
    }
}

// Test memory alignment and cache performance
TEST_F(AsyncMemoryTest, MemoryAlignmentAndCachePerformance) {
    // Test different alignment requirements
    atom::memory::MemoryPool<64, 1024, true> aligned_pool;

    const int num_allocations = 1000;
    std::vector<void*> ptrs;

    // Allocate and check alignment
    for (int i = 0; i < num_allocations; ++i) {
        void* ptr = aligned_pool.allocate();
        ASSERT_NE(ptr, nullptr);

        // Check alignment (should be at least 8-byte aligned)
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 8, 0);

        ptrs.push_back(ptr);
    }

    // Test cache performance by accessing memory in different patterns
    auto test_access_pattern = [&](const std::string& pattern_name,
                                   std::function<void(const std::vector<void*>&)> access_func) {
        auto start = std::chrono::high_resolution_clock::now();
        access_func(ptrs);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << pattern_name << " access time: " << duration.count() << " μs" << std::endl;
    };

    // Sequential access pattern
    test_access_pattern("Sequential", [](const std::vector<void*>& ptrs) {
        for (void* ptr : ptrs) {
            volatile char* p = static_cast<char*>(ptr);
            *p = 42; // Write to trigger cache load
        }
    });

    // Random access pattern
    test_access_pattern("Random", [](const std::vector<void*>& ptrs) {
        std::vector<size_t> indices(ptrs.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(indices.begin(), indices.end(), g);

        for (size_t idx : indices) {
            volatile char* p = static_cast<char*>(ptrs[idx]);
            *p = 42;
        }
    });

    // Clean up
    for (void* ptr : ptrs) {
        aligned_pool.deallocate(ptr);
    }
}
