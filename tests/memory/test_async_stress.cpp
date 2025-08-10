#include <gtest/gtest.h>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>
#include <exception>
#include <stdexcept>

#include "atom/memory/shared.hpp"
#include "atom/memory/memory_pool.hpp"

using namespace atom::connection;
using namespace atom::memory;

class AsyncStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_name_ = "AsyncStressTestSharedMemory";
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
        char buffer[128];
    };

    std::string shm_name_;
};

// Test extreme concurrency stress
TEST_F(AsyncStressTest, ExtremeConcurrencyStress) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_threads = std::thread::hardware_concurrency() * 2; // Oversubscribe
    const int operations_per_thread = 200;
    const auto test_duration = std::chrono::seconds(10);

    std::atomic<bool> stop_test{false};
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};
    std::atomic<int> timeout_operations{0};
    std::atomic<int> exception_operations{0};

    std::vector<std::thread> threads;

    // Start stress test
    auto start_time = std::chrono::steady_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dist(0, 2); // 0=write, 1=read, 2=read+write
            std::uniform_int_distribution<> delay_dist(0, 10); // 0-10ms delay

            int local_ops = 0;
            while (!stop_test.load() && local_ops < operations_per_thread) {
                try {
                    int operation = op_dist(gen);

                    if (operation == 0) {
                        // Async write
                        TestData data{t * 10000 + local_ops, static_cast<double>(t + local_ops), {}};
                        snprintf(data.buffer, sizeof(data.buffer), "stress_write_%d_%d", t, local_ops);

                        auto future = shm.writeAsync(data, std::chrono::milliseconds(50));
                        future.get();
                        successful_operations++;

                    } else if (operation == 1) {
                        // Async read
                        auto future = shm.readAsync(std::chrono::milliseconds(50));
                        [[maybe_unused]] auto data = future.get();
                        successful_operations++;

                    } else {
                        // Combined read+write
                        auto read_future = shm.readAsync(std::chrono::milliseconds(25));
                        TestData read_data = read_future.get();

                        read_data.value += 1.0;
                        snprintf(read_data.buffer, sizeof(read_data.buffer), "stress_modify_%d_%d", t, local_ops);

                        auto write_future = shm.writeAsync(read_data, std::chrono::milliseconds(25));
                        write_future.get();
                        successful_operations++;
                    }

                } catch (const std::future_error& e) {
                    // std::future_errc::timeout doesn't exist in standard library
                    // Check for timeout by error code value or treat all future_errors as timeouts
                    if (e.code() == std::future_errc::future_already_retrieved ||
                        e.code() == std::future_errc::promise_already_satisfied ||
                        e.code() == std::future_errc::no_state) {
                        failed_operations++;
                    } else {
                        // Assume other future errors are timeout-related
                        timeout_operations++;
                    }
                } catch (const SharedMemoryException& e) {
                    failed_operations++;
                } catch (const std::exception& e) {
                    exception_operations++;
                } catch (...) {
                    exception_operations++;
                }

                local_ops++;

                // Random small delay to vary timing
                if (delay_dist(gen) == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(test_duration);
    stop_test.store(true);

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    int total_operations = successful_operations.load() + failed_operations.load() +
                          timeout_operations.load() + exception_operations.load();

    std::cout << "Extreme concurrency stress test results:" << std::endl;
    std::cout << "  Test duration: " << actual_duration.count() << " ms" << std::endl;
    std::cout << "  Threads: " << num_threads << std::endl;
    std::cout << "  Total operations: " << total_operations << std::endl;
    std::cout << "  Successful: " << successful_operations.load() << std::endl;
    std::cout << "  Failed: " << failed_operations.load() << std::endl;
    std::cout << "  Timeouts: " << timeout_operations.load() << std::endl;
    std::cout << "  Exceptions: " << exception_operations.load() << std::endl;
    std::cout << "  Success rate: " << (100.0 * successful_operations.load() / total_operations) << "%" << std::endl;
    std::cout << "  Operations per second: " << (total_operations * 1000.0 / actual_duration.count()) << std::endl;

    // At least 50% of operations should succeed under extreme stress
    EXPECT_GT(successful_operations.load(), total_operations * 0.5);
    EXPECT_GT(total_operations, 1000); // Should have attempted many operations
}

// Test memory pool exhaustion and recovery
TEST_F(AsyncStressTest, MemoryPoolExhaustionStress) {
    atom::memory::MemoryPool<64, 128, true> small_pool; // Very limited pool

    const int num_threads = 8;
    const int max_allocations_per_thread = 50;

    std::atomic<int> allocation_successes{0};
    std::atomic<int> allocation_failures{0};
    std::atomic<int> deallocation_successes{0};
    std::atomic<int> recovery_successes{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::vector<void*> allocated_ptrs;
            allocated_ptrs.reserve(max_allocations_per_thread);

            // Phase 1: Allocate until exhaustion
            for (int i = 0; i < max_allocations_per_thread; ++i) {
                try {
                    void* ptr = small_pool.allocate();
                    if (ptr) {
                        allocated_ptrs.push_back(ptr);
                        allocation_successes++;
                    }
                } catch (const std::bad_alloc& e) {
                    allocation_failures++;
                    break; // Pool exhausted
                } catch (const std::exception& e) {
                    allocation_failures++;
                }
            }

            // Phase 2: Random deallocations and reallocations
            std::random_device rd;
            std::mt19937 gen(rd());

            for (int cycle = 0; cycle < 10; ++cycle) {
                // Deallocate some random pointers
                if (!allocated_ptrs.empty()) {
                    std::uniform_int_distribution<> dist(0, allocated_ptrs.size() - 1);
                    size_t num_to_free = std::min(allocated_ptrs.size(), static_cast<size_t>(5));

                    for (size_t i = 0; i < num_to_free; ++i) {
                        if (!allocated_ptrs.empty()) {
                            size_t idx = dist(gen) % allocated_ptrs.size();
                            small_pool.deallocate(allocated_ptrs[idx]);
                            allocated_ptrs.erase(allocated_ptrs.begin() + idx);
                            deallocation_successes++;
                        }
                    }
                }

                // Try to allocate again
                try {
                    void* ptr = small_pool.allocate();
                    if (ptr) {
                        allocated_ptrs.push_back(ptr);
                        recovery_successes++;
                    }
                } catch (const std::exception& e) {
                    // Expected when pool is still exhausted
                }

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }

            // Phase 3: Clean up remaining allocations
            for (void* ptr : allocated_ptrs) {
                small_pool.deallocate(ptr);
                deallocation_successes++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Memory pool exhaustion stress test results:" << std::endl;
    std::cout << "  Allocation successes: " << allocation_successes.load() << std::endl;
    std::cout << "  Allocation failures: " << allocation_failures.load() << std::endl;
    std::cout << "  Deallocation successes: " << deallocation_successes.load() << std::endl;
    std::cout << "  Recovery successes: " << recovery_successes.load() << std::endl;

    EXPECT_GT(allocation_successes.load(), 0);
    EXPECT_GT(allocation_failures.load(), 0); // Should hit exhaustion
    EXPECT_GT(recovery_successes.load(), 0); // Should recover after deallocations
    EXPECT_EQ(allocation_successes.load(), deallocation_successes.load()); // All allocated should be deallocated
}

// Test rapid callback registration/unregistration
TEST_F(AsyncStressTest, CallbackRegistrationStress) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int num_threads = 6;
    const int callbacks_per_thread = 50;
    const int write_operations = 100;

    std::atomic<int> callbacks_registered{0};
    std::atomic<int> callbacks_unregistered{0};
    std::atomic<int> callback_invocations{0};
    std::atomic<int> registration_failures{0};

    std::vector<std::thread> threads;

    // Callback registration/unregistration threads
    for (int t = 0; t < num_threads - 2; ++t) {
        threads.emplace_back([&]() {
            std::vector<size_t> callback_ids;
            callback_ids.reserve(callbacks_per_thread);

            for (int i = 0; i < callbacks_per_thread; ++i) {
                try {
                    auto callback = [&](const TestData& /*data*/) {
                        callback_invocations++;
                        // Simulate some processing
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    };

                    size_t id = shm.registerChangeCallback(callback);
                    callback_ids.push_back(id);
                    callbacks_registered++;

                    // Randomly unregister some callbacks
                    if (i > 10 && (i % 5 == 0) && !callback_ids.empty()) {
                        size_t unregister_id = callback_ids.back();
                        callback_ids.pop_back();

                        if (shm.unregisterChangeCallback(unregister_id)) {
                            callbacks_unregistered++;
                        }
                    }

                } catch (const std::exception& e) {
                    registration_failures++;
                }

                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            // Unregister remaining callbacks
            for (size_t id : callback_ids) {
                if (shm.unregisterChangeCallback(id)) {
                    callbacks_unregistered++;
                }
            }
        });
    }

    // Writer threads to trigger callbacks
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < write_operations; ++i) {
                try {
                    TestData data{t * 1000 + i, static_cast<double>(t + i), {}};
                    snprintf(data.buffer, sizeof(data.buffer), "callback_trigger_%d_%d", t, i);

                    shm.write(data);

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

                } catch (const std::exception& e) {
                    // Handle write failures
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Give callbacks time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "Callback registration stress test results:" << std::endl;
    std::cout << "  Callbacks registered: " << callbacks_registered.load() << std::endl;
    std::cout << "  Callbacks unregistered: " << callbacks_unregistered.load() << std::endl;
    std::cout << "  Callback invocations: " << callback_invocations.load() << std::endl;
    std::cout << "  Registration failures: " << registration_failures.load() << std::endl;

    EXPECT_GT(callbacks_registered.load(), 0);
    EXPECT_GT(callback_invocations.load(), 0);
    EXPECT_EQ(callbacks_registered.load(), callbacks_unregistered.load()); // All registered should be unregistered
    EXPECT_LT(registration_failures.load(), callbacks_registered.load() * 0.1); // Less than 10% failures
}

// Test system resource limits
TEST_F(AsyncStressTest, SystemResourceLimitsTest) {
    const int num_shared_memories = 10;
    const int operations_per_shm = 50;

    std::vector<std::unique_ptr<SharedMemory<TestData>>> shared_memories;
    std::atomic<int> successful_creations{0};
    std::atomic<int> failed_creations{0};
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};

    // Create multiple shared memory instances
    for (int i = 0; i < num_shared_memories; ++i) {
        try {
            std::string name = shm_name_ + "_" + std::to_string(i);
            auto shm = std::make_unique<SharedMemory<TestData>>(name, true);
            shared_memories.push_back(std::move(shm));
            successful_creations++;
        } catch (const std::exception& e) {
            failed_creations++;
        }
    }

    // Perform operations on all shared memories concurrently
    std::vector<std::thread> threads;

    for (size_t i = 0; i < shared_memories.size(); ++i) {
        threads.emplace_back([&, i]() {
            auto& shm = *shared_memories[i];

            for (int j = 0; j < operations_per_shm; ++j) {
                try {
                    TestData data{static_cast<int>(i * 1000 + j), static_cast<double>(i + j), {}};
                    snprintf(data.buffer, sizeof(data.buffer), "resource_test_%zu_%d", i, j);

                    auto write_future = shm.writeAsync(data);
                    write_future.get();

                    auto read_future = shm.readAsync();
                    [[maybe_unused]] auto read_data = read_future.get();

                    successful_operations++;

                } catch (const std::exception& e) {
                    failed_operations++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Clean up shared memories
    for (size_t i = 0; i < shared_memories.size(); ++i) {
        std::string name = shm_name_ + "_" + std::to_string(i);
#ifdef _WIN32
        HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
        if (h) {
            CloseHandle(h);
        }
#else
        shm_unlink(name.c_str());
#endif
    }

    std::cout << "System resource limits test results:" << std::endl;
    std::cout << "  Successful creations: " << successful_creations.load() << std::endl;
    std::cout << "  Failed creations: " << failed_creations.load() << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
    std::cout << "  Failed operations: " << failed_operations.load() << std::endl;

    EXPECT_GT(successful_creations.load(), 0);
    EXPECT_GT(successful_operations.load(), 0);

    // Most operations should succeed
    int total_operations = successful_operations.load() + failed_operations.load();
    if (total_operations > 0) {
        double success_rate = static_cast<double>(successful_operations.load()) / total_operations;
        EXPECT_GT(success_rate, 0.8); // At least 80% success rate
    }
}
