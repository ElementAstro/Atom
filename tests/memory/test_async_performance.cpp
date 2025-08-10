#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>
#include <algorithm>
#include <numeric>

#include "atom/memory/shared.hpp"
#include "atom/memory/memory_pool.hpp"

using namespace atom::connection;
using namespace atom::memory;

class AsyncPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_name_ = "AsyncPerfTestSharedMemory";
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

    // Performance measurement helper
    template<typename Func>
    auto measurePerformance(const std::string& test_name, Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = func();
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << test_name << " took: " << duration.count() << " μs" << std::endl;

        return std::make_pair(result, duration);
    }
};

// Test async shared memory throughput
TEST_F(AsyncPerformanceTest, SharedMemoryThroughputTest) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_operations = 1000;
    const int num_threads = 4;

    auto [operations_completed, duration] = measurePerformance("Async SharedMemory Throughput", [&]() {
        std::atomic<int> completed{0};
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < num_operations / num_threads; ++i) {
                    try {
                        TestData data{t * 1000 + i, static_cast<double>(t + i), {}};
                        snprintf(data.buffer, sizeof(data.buffer), "perf_%d_%d", t, i);

                        auto write_future = shm.writeAsync(data);
                        write_future.get();

                        auto read_future = shm.readAsync();
                        [[maybe_unused]] auto read_data = read_future.get();

                        completed++;
                    } catch (const std::exception& e) {
                        // Count failures too
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return completed.load();
    });

    double ops_per_second = (operations_completed * 1000000.0) / duration.count();
    std::cout << "Throughput: " << ops_per_second << " ops/sec" << std::endl;
    std::cout << "Operations completed: " << operations_completed << "/" << num_operations << std::endl;

    EXPECT_GT(operations_completed, num_operations * 0.8);
    EXPECT_GT(ops_per_second, 1000); // At least 1000 ops/sec
}

// Test memory pool allocation performance
TEST_F(AsyncPerformanceTest, MemoryPoolAllocationPerformance) {
    const int num_allocations = 10000;
    const int num_threads = 4;

    // Test lock-free pool
    auto [lock_free_ops, lock_free_duration] = measurePerformance("Lock-free MemoryPool", [&]() {
        atom::memory::MemoryPool<64, 2048, true> pool;
        std::atomic<int> completed{0};
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                std::vector<void*> ptrs;
                ptrs.reserve(num_allocations / num_threads);

                // Allocation phase
                for (int i = 0; i < num_allocations / num_threads; ++i) {
                    try {
                        void* ptr = pool.allocate();
                        if (ptr) {
                            ptrs.push_back(ptr);
                            completed++;
                        }
                    } catch (const std::exception& e) {
                        // Handle allocation failures
                    }
                }

                // Deallocation phase
                for (void* ptr : ptrs) {
                    pool.deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return completed.load();
    });

    // Test mutex-based pool
    auto [mutex_ops, mutex_duration] = measurePerformance("Mutex-based MemoryPool", [&]() {
        atom::memory::MemoryPool<64, 2048, false> pool;
        std::atomic<int> completed{0};
        std::vector<std::thread> threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                std::vector<void*> ptrs;
                ptrs.reserve(num_allocations / num_threads);

                // Allocation phase
                for (int i = 0; i < num_allocations / num_threads; ++i) {
                    try {
                        void* ptr = pool.allocate();
                        if (ptr) {
                            ptrs.push_back(ptr);
                            completed++;
                        }
                    } catch (const std::exception& e) {
                        // Handle allocation failures
                    }
                }

                // Deallocation phase
                for (void* ptr : ptrs) {
                    pool.deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return completed.load();
    });

    double lock_free_ops_per_sec = (lock_free_ops * 1000000.0) / lock_free_duration.count();
    double mutex_ops_per_sec = (mutex_ops * 1000000.0) / mutex_duration.count();

    std::cout << "Lock-free pool: " << lock_free_ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Mutex pool: " << mutex_ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Performance ratio: " << (lock_free_ops_per_sec / mutex_ops_per_sec) << std::endl;

    EXPECT_GT(lock_free_ops, num_allocations * 0.8);
    EXPECT_GT(mutex_ops, num_allocations * 0.8);
}

// Test latency distribution for async operations
TEST_F(AsyncPerformanceTest, LatencyDistributionTest) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_samples = 1000;
    std::vector<std::chrono::microseconds> latencies;
    latencies.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        TestData data{i, static_cast<double>(i), {}};
        snprintf(data.buffer, sizeof(data.buffer), "latency_%d", i);

        auto start = std::chrono::high_resolution_clock::now();

        try {
            auto write_future = shm.writeAsync(data);
            write_future.get();

            auto read_future = shm.readAsync();
            [[maybe_unused]] auto read_data = read_future.get();

            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            latencies.push_back(latency);

        } catch (const std::exception& e) {
            // Skip failed operations
        }
    }

    ASSERT_GT(latencies.size(), num_samples * 0.8);

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());

    auto min_latency = latencies.front();
    auto max_latency = latencies.back();
    auto median_latency = latencies[latencies.size() / 2];
    auto p95_latency = latencies[static_cast<size_t>(latencies.size() * 0.95)];
    auto p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];

    auto sum = std::accumulate(latencies.begin(), latencies.end(), std::chrono::microseconds{0});
    auto avg_latency = sum / latencies.size();

    std::cout << "Latency Distribution (μs):" << std::endl;
    std::cout << "  Min: " << min_latency.count() << std::endl;
    std::cout << "  Average: " << avg_latency.count() << std::endl;
    std::cout << "  Median: " << median_latency.count() << std::endl;
    std::cout << "  95th percentile: " << p95_latency.count() << std::endl;
    std::cout << "  99th percentile: " << p99_latency.count() << std::endl;
    std::cout << "  Max: " << max_latency.count() << std::endl;

    // Performance assertions
    EXPECT_LT(avg_latency.count(), 1000); // Average should be less than 1ms
    EXPECT_LT(p95_latency.count(), 5000); // 95th percentile should be less than 5ms
}

// Test scalability with increasing thread count
TEST_F(AsyncPerformanceTest, ScalabilityTest) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int operations_per_thread = 100;
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};

    for (int num_threads : thread_counts) {
        auto [ops_completed, duration] = measurePerformance(
            "Scalability test with " + std::to_string(num_threads) + " threads",
            [&]() {
                std::atomic<int> completed{0};
                std::vector<std::thread> threads;

                for (int t = 0; t < num_threads; ++t) {
                    threads.emplace_back([&, t]() {
                        for (int i = 0; i < operations_per_thread; ++i) {
                            try {
                                TestData data{t * 1000 + i, static_cast<double>(t + i), {}};
                                snprintf(data.buffer, sizeof(data.buffer), "scale_%d_%d", t, i);

                                auto write_future = shm.writeAsync(data);
                                write_future.get();

                                completed++;
                            } catch (const std::exception& e) {
                                // Handle errors
                            }
                        }
                    });
                }

                for (auto& thread : threads) {
                    thread.join();
                }

                return completed.load();
            });

        double ops_per_second = (ops_completed * 1000000.0) / duration.count();
        double ops_per_thread_per_second = ops_per_second / num_threads;

        std::cout << "  " << num_threads << " threads: " << ops_per_second
                  << " total ops/sec, " << ops_per_thread_per_second
                  << " ops/sec/thread" << std::endl;

        EXPECT_GT(ops_completed, (num_threads * operations_per_thread) * 0.8);
    }
}

// Test memory usage efficiency
TEST_F(AsyncPerformanceTest, MemoryEfficiencyTest) {
    const int num_pools = 3;
    const int allocations_per_pool = 1000;

    std::vector<std::unique_ptr<atom::memory::MemoryPool<128, 1024, true>>> pools;

    // Create multiple pools
    for (int i = 0; i < num_pools; ++i) {
        pools.push_back(std::make_unique<atom::memory::MemoryPool<128, 1024, true>>());
    }

    auto [total_allocations, duration] = measurePerformance("Memory Efficiency Test", [&]() {
        std::atomic<int> total{0};
        std::vector<std::thread> threads;

        for (int p = 0; p < num_pools; ++p) {
            threads.emplace_back([&, p]() {
                std::vector<void*> ptrs;
                ptrs.reserve(allocations_per_pool);

                // Allocate
                for (int i = 0; i < allocations_per_pool; ++i) {
                    try {
                        void* ptr = pools[p]->allocate();
                        if (ptr) {
                            ptrs.push_back(ptr);
                            total++;
                        }
                    } catch (const std::exception& e) {
                        // Handle allocation failures
                    }
                }

                // Use memory (write pattern)
                for (void* ptr : ptrs) {
                    memset(ptr, 0xAA, 64); // Write pattern to ensure memory is actually used
                }

                // Deallocate
                for (void* ptr : ptrs) {
                    pools[p]->deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return total.load();
    });

    double allocations_per_second = (total_allocations * 1000000.0) / duration.count();

    std::cout << "Memory efficiency results:" << std::endl;
    std::cout << "  Total allocations: " << total_allocations << std::endl;
    std::cout << "  Allocations per second: " << allocations_per_second << std::endl;
    std::cout << "  Average time per allocation: "
              << (duration.count() / static_cast<double>(total_allocations)) << " μs" << std::endl;

    EXPECT_GT(total_allocations, (num_pools * allocations_per_pool) * 0.9);
    EXPECT_GT(allocations_per_second, 10000); // At least 10k allocations per second
}
