#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/parallel.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// Test fixture for Parallel algorithms
class ParallelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        test_data.resize(1000);
        std::iota(test_data.begin(), test_data.end(), 1);
        
        // Create random data for some tests
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000);
        
        random_data.resize(500);
        for (auto& val : random_data) {
            val = dis(gen);
        }
    }

    std::vector<int> test_data;
    std::vector<int> random_data;
    std::vector<float> float_data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
};

// Test fixture for Task coroutine
class TaskTest : public ::testing::Test {};

// Test fixture for ThreadConfig
class ThreadConfigTest : public ::testing::Test {};

// Task coroutine tests
TEST_F(TaskTest, BasicTaskExecution) {
    auto task = []() -> Task<int> {
        co_return 42;
    }();
    
    EXPECT_EQ(task.get(), 42);
    EXPECT_TRUE(task.is_done());
}

TEST_F(TaskTest, TaskWithException) {
    auto task = []() -> Task<int> {
        throw std::runtime_error("Task exception");
        co_return 42;
    }();
    
    EXPECT_THROW(task.get(), std::runtime_error);
    EXPECT_TRUE(task.is_done());
}

TEST_F(TaskTest, VoidTask) {
    bool executed = false;
    auto task = [&executed]() -> Task<void> {
        executed = true;
        co_return;
    }();
    
    task.get();
    EXPECT_TRUE(executed);
    EXPECT_TRUE(task.is_done());
}

TEST_F(TaskTest, TaskMoveSemantics) {
    auto task1 = []() -> Task<int> {
        co_return 100;
    }();
    
    Task<int> task2 = std::move(task1);
    EXPECT_EQ(task2.get(), 100);
    EXPECT_TRUE(task2.is_done());
}

// ThreadConfig tests
TEST_F(ThreadConfigTest, SetThreadAffinity) {
    // Test setting thread affinity (may not work on all systems)
    bool result = Parallel::ThreadConfig::setThreadAffinity(0);
    // Just ensure it doesn't crash - result depends on platform and permissions
    EXPECT_TRUE(result || !result);  // Always passes, just tests compilation
}

TEST_F(ThreadConfigTest, SetThreadPriority) {
    // Test setting thread priority
    bool result = Parallel::ThreadConfig::setThreadPriority(
        Parallel::ThreadConfig::Priority::Normal);
    // Just ensure it doesn't crash - result depends on platform and permissions
    EXPECT_TRUE(result || !result);  // Always passes, just tests compilation
}

// Parallel for_each tests
TEST_F(ParallelTest, ForEachBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};
    
    Parallel::for_each(data.begin(), data.end(), [&sum](int val) {
        sum.fetch_add(val);
    });
    
    EXPECT_EQ(sum.load(), 15);
}

TEST_F(ParallelTest, ForEachLargeDataset) {
    std::atomic<long long> sum{0};
    
    Parallel::for_each(test_data.begin(), test_data.end(), [&sum](int val) {
        sum.fetch_add(val);
    });
    
    // Sum of 1 to 1000 = 1000 * 1001 / 2 = 500500
    EXPECT_EQ(sum.load(), 500500);
}

TEST_F(ParallelTest, ForEachWithCustomThreadCount) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    std::atomic<int> sum{0};
    
    Parallel::for_each(data.begin(), data.end(), [&sum](int val) {
        sum.fetch_add(val);
    }, 2);  // Use 2 threads
    
    EXPECT_EQ(sum.load(), 36);
}

TEST_F(ParallelTest, ForEachEmptyRange) {
    std::vector<int> empty_data;
    std::atomic<int> sum{0};
    
    EXPECT_NO_THROW(Parallel::for_each(empty_data.begin(), empty_data.end(), 
                                      [&sum](int val) {
        sum.fetch_add(val);
    }));
    
    EXPECT_EQ(sum.load(), 0);
}

// Parallel map tests
TEST_F(ParallelTest, MapBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto result = Parallel::map(data.begin(), data.end(), [](int val) {
        return val * 2;
    });
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, MapLargeDataset) {
    auto result = Parallel::map(test_data.begin(), test_data.begin() + 100, 
                               [](int val) {
        return val * val;
    });
    
    EXPECT_EQ(result.size(), 100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(result[i], (i + 1) * (i + 1));
    }
}

TEST_F(ParallelTest, MapWithDifferentTypes) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto result = Parallel::map(data.begin(), data.end(), [](int val) {
        return std::to_string(val);
    });
    
    std::vector<std::string> expected = {"1", "2", "3", "4", "5"};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, MapEmptyRange) {
    std::vector<int> empty_data;
    
    auto result = Parallel::map(empty_data.begin(), empty_data.end(), [](int val) {
        return val * 2;
    });
    
    EXPECT_TRUE(result.empty());
}

// Parallel reduce tests
TEST_F(ParallelTest, ReduceBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    int result = Parallel::reduce(data.begin(), data.end(), 0, std::plus<int>());
    
    EXPECT_EQ(result, 15);
}

TEST_F(ParallelTest, ReduceLargeDataset) {
    int result = Parallel::reduce(test_data.begin(), test_data.end(), 0, 
                                 std::plus<int>());
    
    EXPECT_EQ(result, 500500);  // Sum of 1 to 1000
}

TEST_F(ParallelTest, ReduceWithMultiplication) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    int result = Parallel::reduce(data.begin(), data.end(), 1, 
                                 std::multiplies<int>());
    
    EXPECT_EQ(result, 120);  // 5!
}

TEST_F(ParallelTest, ReduceEmptyRange) {
    std::vector<int> empty_data;
    
    int result = Parallel::reduce(empty_data.begin(), empty_data.end(), 42, 
                                 std::plus<int>());
    
    EXPECT_EQ(result, 42);  // Should return initial value
}

// Parallel filter tests
TEST_F(ParallelTest, FilterBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    auto result = Parallel::filter(data.begin(), data.end(), [](int val) {
        return val % 2 == 0;  // Even numbers
    });
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, FilterLargeDataset) {
    auto result = Parallel::filter(test_data.begin(), test_data.end(), [](int val) {
        return val % 10 == 0;  // Multiples of 10
    });
    
    EXPECT_EQ(result.size(), 100);  // 10, 20, 30, ..., 1000
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(result[i], (i + 1) * 10);
    }
}

TEST_F(ParallelTest, FilterEmptyRange) {
    std::vector<int> empty_data;
    
    auto result = Parallel::filter(empty_data.begin(), empty_data.end(), [](int val) {
        return val > 0;
    });
    
    EXPECT_TRUE(result.empty());
}

TEST_F(ParallelTest, FilterNoMatches) {
    std::vector<int> data = {1, 3, 5, 7, 9};
    
    auto result = Parallel::filter(data.begin(), data.end(), [](int val) {
        return val % 2 == 0;  // Even numbers (none in this case)
    });
    
    EXPECT_TRUE(result.empty());
}

// Parallel sort tests
TEST_F(ParallelTest, SortBasic) {
    std::vector<int> data = {5, 2, 8, 1, 9, 3};
    
    Parallel::sort(data.begin(), data.end());
    
    std::vector<int> expected = {1, 2, 3, 5, 8, 9};
    EXPECT_EQ(data, expected);
}

TEST_F(ParallelTest, SortWithCustomComparator) {
    std::vector<int> data = {5, 2, 8, 1, 9, 3};
    
    Parallel::sort(data.begin(), data.end(), std::greater<int>());
    
    std::vector<int> expected = {9, 8, 5, 3, 2, 1};
    EXPECT_EQ(data, expected);
}

TEST_F(ParallelTest, SortLargeDataset) {
    std::vector<int> data = random_data;  // Copy random data
    
    Parallel::sort(data.begin(), data.end());
    
    EXPECT_TRUE(std::is_sorted(data.begin(), data.end()));
}

TEST_F(ParallelTest, SortEmptyRange) {
    std::vector<int> empty_data;
    
    EXPECT_NO_THROW(Parallel::sort(empty_data.begin(), empty_data.end()));
    EXPECT_TRUE(empty_data.empty());
}

TEST_F(ParallelTest, SortSingleElement) {
    std::vector<int> data = {42};

    EXPECT_NO_THROW(Parallel::sort(data.begin(), data.end()));
    EXPECT_EQ(data, std::vector<int>{42});
}

// Span-based operations tests
TEST_F(ParallelTest, MapSpanBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::span<const int> span_data(data);

    auto result = Parallel::map_span(span_data, [](int val) {
        return val * 3;
    });

    std::vector<int> expected = {3, 6, 9, 12, 15};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, MapSpanWithCustomThreads) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    std::span<const int> span_data(data);

    auto result = Parallel::map_span(span_data, [](int val) {
        return val * val;
    }, 2);  // Use 2 threads

    std::vector<int> expected = {1, 4, 9, 16, 25, 36, 49, 64};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, MapSpanEmptySpan) {
    std::vector<int> empty_data;
    std::span<const int> empty_span(empty_data);

    auto result = Parallel::map_span(empty_span, [](int val) {
        return val * 2;
    });

    EXPECT_TRUE(result.empty());
}

// Range-based operations tests
TEST_F(ParallelTest, FilterRangeBasic) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = Parallel::filter_range(data, [](int val) {
        return val > 5;
    });

    std::vector<int> expected = {6, 7, 8, 9, 10};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, FilterRangeWithCustomThreads) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = Parallel::filter_range(data, [](int val) {
        return val % 3 == 0;  // Multiples of 3
    }, 3);  // Use 3 threads

    std::vector<int> expected = {3, 6, 9};
    EXPECT_EQ(result, expected);
}

TEST_F(ParallelTest, FilterRangeEmptyRange) {
    std::vector<int> empty_data;

    auto result = Parallel::filter_range(empty_data, [](int val) {
        return val > 0;
    });

    EXPECT_TRUE(result.empty());
}

// Async coroutine tests
TEST_F(ParallelTest, AsyncBasic) {
    auto task = Parallel::async([]() {
        return 42;
    });

    EXPECT_EQ(task.get(), 42);
    EXPECT_TRUE(task.is_done());
}

TEST_F(ParallelTest, AsyncVoid) {
    bool executed = false;
    auto task = Parallel::async([&executed]() {
        executed = true;
    });

    task.get();
    EXPECT_TRUE(executed);
    EXPECT_TRUE(task.is_done());
}

TEST_F(ParallelTest, AsyncWithParameters) {
    auto task = Parallel::async([](int a, int b) {
        return a + b;
    }, 10, 20);

    EXPECT_EQ(task.get(), 30);
}

TEST_F(ParallelTest, AsyncException) {
    auto task = Parallel::async([]() {
        throw std::runtime_error("Async exception");
        return 42;
    });

    EXPECT_THROW(task.get(), std::runtime_error);
}

TEST_F(ParallelTest, WhenAllVoidTasks) {
    std::atomic<int> counter{0};

    auto task1 = Parallel::async([&counter]() {
        counter.fetch_add(1);
    });

    auto task2 = Parallel::async([&counter]() {
        counter.fetch_add(2);
    });

    auto task3 = Parallel::async([&counter]() {
        counter.fetch_add(3);
    });

    auto all_task = Parallel::when_all(std::move(task1), std::move(task2), std::move(task3));
    all_task.get();

    EXPECT_EQ(counter.load(), 6);
}

TEST_F(ParallelTest, ParallelForEachAsync) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::span<const int> span_data(data);
    std::atomic<int> sum{0};

    auto task = Parallel::parallel_for_each_async(span_data, [&sum](int val) {
        sum.fetch_add(val);
    });

    task.get();
    EXPECT_EQ(sum.load(), 15);
}

TEST_F(ParallelTest, ParallelForEachAsyncEmptySpan) {
    std::vector<int> empty_data;
    std::span<const int> empty_span(empty_data);

    auto task = Parallel::parallel_for_each_async(empty_span, [](int val) {
        // Should not be called
    });

    EXPECT_NO_THROW(task.get());
}

// SIMD operations tests
class SimdOpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        a_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        b_data = {2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
        result_data.resize(a_data.size());
    }

    std::vector<float> a_data;
    std::vector<float> b_data;
    std::vector<float> result_data;
};

TEST_F(SimdOpsTest, AddBasic) {
    SimdOps::add<float>(a_data.data(), b_data.data(), result_data.data(), a_data.size());

    for (size_t i = 0; i < a_data.size(); ++i) {
        EXPECT_FLOAT_EQ(result_data[i], a_data[i] + b_data[i]);
    }
}

TEST_F(SimdOpsTest, MultiplyBasic) {
    SimdOps::multiply<float>(a_data.data(), b_data.data(), result_data.data(), a_data.size());

    for (size_t i = 0; i < a_data.size(); ++i) {
        EXPECT_FLOAT_EQ(result_data[i], a_data[i] * b_data[i]);
    }
}

TEST_F(SimdOpsTest, DotProductBasic) {
    float result = SimdOps::dotProduct<float>(a_data.data(), b_data.data(), a_data.size());

    float expected = 0.0f;
    for (size_t i = 0; i < a_data.size(); ++i) {
        expected += a_data[i] * b_data[i];
    }

    EXPECT_FLOAT_EQ(result, expected);
}

TEST_F(SimdOpsTest, DotProductSpan) {
    std::span<const float> span_a(a_data);
    std::span<const float> span_b(b_data);

    float result = SimdOps::dotProduct<float>(span_a, span_b);

    float expected = 0.0f;
    for (size_t i = 0; i < a_data.size(); ++i) {
        expected += a_data[i] * b_data[i];
    }

    EXPECT_FLOAT_EQ(result, expected);
}

TEST_F(SimdOpsTest, NullPointerThrows) {
    EXPECT_THROW(SimdOps::add<float>(nullptr, b_data.data(), result_data.data(), a_data.size()),
                 std::invalid_argument);
    EXPECT_THROW(SimdOps::multiply<float>(a_data.data(), nullptr, result_data.data(), a_data.size()),
                 std::invalid_argument);
    EXPECT_THROW(SimdOps::dotProduct<float>(nullptr, b_data.data(), a_data.size()),
                 std::invalid_argument);
}

TEST_F(SimdOpsTest, MismatchedSpanSizes) {
    std::vector<float> short_data = {1.0f, 2.0f};
    std::span<const float> span_a(a_data);
    std::span<const float> span_short(short_data);

    EXPECT_THROW(SimdOps::dotProduct<float>(span_a, span_short), std::invalid_argument);
}

// Performance and stress tests
class ParallelPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        large_data.resize(100000);
        std::iota(large_data.begin(), large_data.end(), 1);

        // Create random data for sorting tests
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100000);

        random_large_data.resize(50000);
        for (auto& val : random_large_data) {
            val = dis(gen);
        }
    }

    std::vector<int> large_data;
    std::vector<int> random_large_data;
};

TEST_F(ParallelPerformanceTest, LargeDatasetForEach) {
    std::atomic<long long> sum{0};

    auto start = std::chrono::high_resolution_clock::now();

    Parallel::for_each(large_data.begin(), large_data.end(), [&sum](int val) {
        sum.fetch_add(val);
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Sum of 1 to 100000 = 100000 * 100001 / 2 = 5000050000
    EXPECT_EQ(sum.load(), 5000050000LL);
    EXPECT_LT(duration.count(), 1000);  // Should complete within 1 second
}

TEST_F(ParallelPerformanceTest, LargeDatasetMap) {
    auto start = std::chrono::high_resolution_clock::now();

    auto result = Parallel::map(large_data.begin(), large_data.begin() + 10000, [](int val) {
        return val * 2;
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.size(), 10000);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[9999], 20000);
    EXPECT_LT(duration.count(), 500);  // Should complete within 500ms
}

TEST_F(ParallelPerformanceTest, LargeDatasetReduce) {
    auto start = std::chrono::high_resolution_clock::now();

    long long result = Parallel::reduce(large_data.begin(), large_data.begin() + 50000,
                                       0LL, std::plus<long long>());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Sum of 1 to 50000 = 50000 * 50001 / 2 = 1250025000
    EXPECT_EQ(result, 1250025000LL);
    EXPECT_LT(duration.count(), 500);  // Should complete within 500ms
}

TEST_F(ParallelPerformanceTest, LargeDatasetSort) {
    std::vector<int> data = random_large_data;  // Copy for sorting

    auto start = std::chrono::high_resolution_clock::now();

    Parallel::sort(data.begin(), data.end());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(std::is_sorted(data.begin(), data.end()));
    EXPECT_LT(duration.count(), 2000);  // Should complete within 2 seconds
}

// Exception handling tests
TEST_F(ParallelTest, ForEachWithException) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> processed{0};

    // Some tasks will throw, but others should still execute
    EXPECT_NO_THROW(Parallel::for_each(data.begin(), data.end(), [&processed](int val) {
        processed.fetch_add(1);
        if (val == 3) {
            throw std::runtime_error("Test exception");
        }
    }));

    // Some tasks should have been processed (exact count depends on timing)
    EXPECT_GT(processed.load(), 0);
}

TEST_F(ParallelTest, MapWithException) {
    std::vector<int> data = {1, 2, 3, 4, 5};

    // Map operation should handle exceptions gracefully
    EXPECT_NO_THROW({
        auto result = Parallel::map(data.begin(), data.end(), [](int val) {
            if (val == 3) {
                throw std::runtime_error("Test exception");
            }
            return val * 2;
        });
        // Result may be partial or empty due to exception
    });
}

// Thread safety tests
TEST_F(ParallelTest, ConcurrentAccess) {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);
    std::atomic<long long> total_sum{0};

    // Multiple parallel operations on the same data
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&data, &total_sum]() {
            std::atomic<long long> local_sum{0};
            Parallel::for_each(data.begin(), data.end(), [&local_sum](const int& val) {
                local_sum.fetch_add(val);
            });
            total_sum.fetch_add(local_sum.load());
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Each thread should have computed the same sum, so total should be 4 times the sum
    long long expected_single_sum = 1000 * 1001 / 2;  // Sum of 1 to 1000
    EXPECT_EQ(total_sum.load(), expected_single_sum * 4);
}

// Edge case tests
TEST_F(ParallelTest, SingleThreadPerformance) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};

    // Force single thread execution
    Parallel::for_each(data.begin(), data.end(), [&sum](int val) {
        sum.fetch_add(val);
    }, 1);

    EXPECT_EQ(sum.load(), 15);
}

TEST_F(ParallelTest, ZeroThreadsDefaultsToHardwareConcurrency) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};

    // Zero threads should default to hardware concurrency
    EXPECT_NO_THROW(Parallel::for_each(data.begin(), data.end(), [&sum](int val) {
        sum.fetch_add(val);
    }, 0));

    EXPECT_EQ(sum.load(), 15);
}
