#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>
#include <queue>
#include <condition_variable>

#include "atom/memory/shared.hpp"
#include "atom/memory/memory_pool.hpp"
#include "atom/memory/ring.hpp"

using namespace atom::connection;
using namespace atom::memory;

class AsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_name_ = "AsyncIntegrationTestSharedMemory";
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
        char buffer[32];
    };

    std::string shm_name_;
};

// Test integration between shared memory and memory pools
TEST_F(AsyncIntegrationTest, SharedMemoryWithMemoryPoolIntegration) {
    SharedMemory<TestData> shm(shm_name_, true);
    atom::memory::MemoryPool<64, 512, true> pool;

    const int num_operations = 100;
    std::atomic<int> successful_operations{0};
    std::vector<std::future<void>> futures;

    // Create async operations that use both shared memory and memory pool
    for (int i = 0; i < num_operations; ++i) {
        auto future = std::async(std::launch::async, [&, i]() {
            try {
                // Allocate from memory pool
                void* pool_memory = pool.allocate();
                if (!pool_memory) return;

                // Use pool memory for temporary data processing
                TestData temp_data{i, static_cast<double>(i * 2.5), {}};
                snprintf(temp_data.buffer, sizeof(temp_data.buffer), "temp_%d", i);

                // Copy to pool memory and process
                memcpy(pool_memory, &temp_data, sizeof(TestData));

                // Write to shared memory asynchronously
                auto write_future = shm.writeAsync(temp_data);
                write_future.get();

                // Read back from shared memory
                auto read_future = shm.readAsync();
                TestData read_data = read_future.get();

                // Verify data integrity
                if (read_data.id == temp_data.id &&
                    read_data.value == temp_data.value) {
                    successful_operations++;
                }

                // Clean up pool memory
                pool.deallocate(pool_memory);

            } catch (const std::exception& e) {
                // Handle errors gracefully
            }
        });

        futures.push_back(std::move(future));
    }

    // Wait for all operations to complete
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_GT(successful_operations.load(), num_operations * 0.8); // Allow some failures
}

// Test async operations with ring buffer coordination
TEST_F(AsyncIntegrationTest, AsyncRingBufferCoordination) {
    RingBuffer<int> ring_buffer(100);
    std::atomic<int> producer_count{0};
    std::atomic<int> consumer_count{0};
    std::atomic<bool> stop_test{false};

    const int num_producers = 3;
    const int num_consumers = 2;
    const int test_duration_ms = 1000;

    std::vector<std::thread> threads;

    // Producer threads
    for (int i = 0; i < num_producers; ++i) {
        threads.emplace_back([&, i]() {
            int value = i * 10000;
            while (!stop_test.load()) {
                if (ring_buffer.push(value++)) {
                    producer_count++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Consumer threads
    for (int i = 0; i < num_consumers; ++i) {
        threads.emplace_back([&]() {
            while (!stop_test.load()) {
                auto value = ring_buffer.pop();
                if (value.has_value()) {
                    consumer_count++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(150));
            }
        });
    }

    // Run test for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(test_duration_ms));
    stop_test.store(true);

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(producer_count.load(), 0);
    EXPECT_GT(consumer_count.load(), 0);

    // Consumer count should be close to producer count (allowing for timing)
    int diff = std::abs(producer_count.load() - consumer_count.load());
    EXPECT_LT(diff, producer_count.load() * 0.2); // Within 20% difference
}

// Test async memory operations under resource contention
TEST_F(AsyncIntegrationTest, ResourceContentionTest) {
    SharedMemory<TestData> shm(shm_name_, true);
    atom::memory::MemoryPool<128, 256, true> limited_pool; // Small pool to create contention

    const int num_threads = 8;
    const int operations_per_thread = 50;

    std::atomic<int> allocation_failures{0};
    std::atomic<int> shm_failures{0};
    std::atomic<int> successful_operations{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    // Try to allocate from limited pool
                    void* memory = nullptr;
                    try {
                        memory = limited_pool.allocate();
                    } catch (const std::bad_alloc& e) {
                        allocation_failures++;
                        continue;
                    }

                    if (!memory) {
                        allocation_failures++;
                        continue;
                    }

                    // Prepare test data
                    TestData data{i * 1000 + j, static_cast<double>(i + j), {}};
                    snprintf(data.buffer, sizeof(data.buffer), "thread_%d_%d", i, j);

                    // Try async shared memory operation
                    try {
                        auto future = shm.writeAsync(data, std::chrono::milliseconds(100));
                        future.get();
                        successful_operations++;
                    } catch (const std::exception& e) {
                        shm_failures++;
                    }

                    // Always clean up memory
                    limited_pool.deallocate(memory);

                } catch (const std::exception& e) {
                    // Handle any other errors
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    int total_attempts = num_threads * operations_per_thread;

    EXPECT_GT(successful_operations.load(), 0);
    EXPECT_LT(allocation_failures.load(), total_attempts);
    EXPECT_LT(shm_failures.load(), total_attempts);

    std::cout << "Resource contention test results:" << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
    std::cout << "  Allocation failures: " << allocation_failures.load() << std::endl;
    std::cout << "  Shared memory failures: " << shm_failures.load() << std::endl;
    std::cout << "  Success rate: " << (100.0 * successful_operations.load() / total_attempts) << "%" << std::endl;
}

// Test async operations with callback chains
TEST_F(AsyncIntegrationTest, AsyncCallbackChains) {
    SharedMemory<TestData> shm(shm_name_, true);

    std::atomic<int> callback_chain_completions{0};
    std::atomic<int> callback_errors{0};

    const int num_chains = 10;
    std::vector<std::future<void>> chain_futures;

    for (int i = 0; i < num_chains; ++i) {
        auto future = std::async(std::launch::async, [&, i]() {
            try {
                // Step 1: Write initial data
                TestData initial_data{i, static_cast<double>(i), {}};
                snprintf(initial_data.buffer, sizeof(initial_data.buffer), "initial_%d", i);

                auto write_future = shm.writeAsync(initial_data);
                write_future.get();

                // Step 2: Read and modify
                auto read_future = shm.readAsync();
                TestData read_data = read_future.get();

                read_data.value *= 2.0;
                snprintf(read_data.buffer, sizeof(read_data.buffer), "modified_%d", i);

                // Step 3: Write modified data
                auto write_future2 = shm.writeAsync(read_data);
                write_future2.get();

                // Step 4: Final verification read
                auto verify_future = shm.readAsync();
                TestData final_data = verify_future.get();

                if (final_data.value == initial_data.value * 2.0) {
                    callback_chain_completions++;
                }

            } catch (const std::exception& e) {
                callback_errors++;
            }
        });

        chain_futures.push_back(std::move(future));
    }

    // Wait for all chains to complete
    for (auto& future : chain_futures) {
        future.get();
    }

    EXPECT_GT(callback_chain_completions.load(), num_chains * 0.7); // Allow some failures
    EXPECT_LT(callback_errors.load(), num_chains * 0.3);
}

// Test async memory operations with timeout handling
TEST_F(AsyncIntegrationTest, TimeoutHandlingTest) {
    SharedMemory<TestData> shm(shm_name_, true);

    std::atomic<int> timeout_operations{0};
    std::atomic<int> successful_operations{0};

    const int num_operations = 20;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_operations; ++i) {
        auto future = std::async(std::launch::async, [&, i]() {
            TestData data{i, static_cast<double>(i), {}};
            snprintf(data.buffer, sizeof(data.buffer), "timeout_test_%d", i);

            try {
                // Use very short timeout to potentially trigger timeouts
                auto write_future = shm.writeAsync(data, std::chrono::milliseconds(1));
                write_future.get();

                auto read_future = shm.readAsync(std::chrono::milliseconds(1));
                [[maybe_unused]] auto read_data = read_future.get();

                successful_operations++;

            } catch (const std::exception& e) {
                timeout_operations++;
            }
        });

        futures.push_back(std::move(future));
    }

    for (auto& future : futures) {
        future.get();
    }

    // Either operations succeed or timeout, but total should equal num_operations
    EXPECT_EQ(successful_operations.load() + timeout_operations.load(), num_operations);

    std::cout << "Timeout handling test results:" << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
    std::cout << "  Timeout operations: " << timeout_operations.load() << std::endl;
}
