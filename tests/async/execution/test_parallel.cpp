#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <latch>
#include <numeric>
#include <random>
#include <span>
#include <vector>
#include "atom/async/execution/parallel.hpp"
#include "atom/async/execution/thread_utils.hpp"

using namespace atom::async;

class ParallelTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Setup test data
        test_vector.resize(1000);
        std::iota(test_vector.begin(), test_vector.end(), 1);

        random_vector.resize(1000);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100);
        std::generate(random_vector.begin(), random_vector.end(),
                      [&]() { return dis(gen); });
    }

    std::vector<int> test_vector;
    std::vector<int> random_vector;
};

TEST_F(ParallelTest, ForEach) {
    std::vector<int> output = test_vector;

    // Test basic for_each
    Parallel::for_each(output.begin(), output.end(), [](int& x) { x *= 2; });

    // Verify all elements were doubled
    for (size_t i = 0; i < output.size(); ++i) {
        EXPECT_EQ(output[i], test_vector[i] * 2);
    }
}

TEST_F(ParallelTest, ForEachWithThreads) {
    std::vector<int> output = test_vector;

    // Test with specific thread count
    Parallel::for_each(output.begin(), output.end(), [](int& x) { x *= 3; }, 4);

    // Verify all elements were tripled
    for (size_t i = 0; i < output.size(); ++i) {
        EXPECT_EQ(output[i], test_vector[i] * 3);
    }
}

TEST_F(ParallelTest, ForEachJThread) {
    std::vector<int> output = test_vector;

    // Test with jthread implementation
    Parallel::for_each_jthread(
        output.begin(), output.end(), [](int& x) { x += 10; }, 2);

    // Verify all elements had 10 added
    for (size_t i = 0; i < output.size(); ++i) {
        EXPECT_EQ(output[i], test_vector[i] + 10);
    }
}

TEST_F(ParallelTest, Map) {
    // Test map function
    auto result = Parallel::map(test_vector.begin(), test_vector.end(),
                                [](int x) { return x * x; });

    ASSERT_EQ(result.size(), test_vector.size());

    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], test_vector[i] * test_vector[i]);
    }
}

TEST_F(ParallelTest, MapSpan) {
    // Test map_span function
    std::span<const int> span(test_vector);
    auto result = Parallel::map_span(span, [](int x) { return x + 100; });

    ASSERT_EQ(result.size(), test_vector.size());

    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], test_vector[i] + 100);
    }
}

TEST_F(ParallelTest, Reduce) {
    // Test reduce function (sum)
    int sum = Parallel::reduce(test_vector.begin(), test_vector.end(), 0,
                               std::plus<int>());

    int expected_sum =
        std::accumulate(test_vector.begin(), test_vector.end(), 0);
    EXPECT_EQ(sum, expected_sum);

    // Test reduce function (product of first 10 elements)
    std::vector<int> first_10(test_vector.begin(), test_vector.begin() + 10);
    int product = Parallel::reduce(first_10.begin(), first_10.end(), 1,
                                   std::multiplies<int>());

    int expected_product = std::accumulate(first_10.begin(), first_10.end(), 1,
                                           std::multiplies<int>());
    EXPECT_EQ(product, expected_product);
}

TEST_F(ParallelTest, Filter) {
    // Test filter function (even numbers)
    auto result = Parallel::filter(test_vector.begin(), test_vector.end(),
                                   [](int x) { return x % 2 == 0; });

    // Verify all results are even
    for (int val : result) {
        EXPECT_EQ(val % 2, 0);
    }

    // Count how many even numbers should be there
    size_t expected_even = std::count_if(test_vector.begin(), test_vector.end(),
                                         [](int x) { return x % 2 == 0; });
    EXPECT_EQ(result.size(), expected_even);
}

TEST_F(ParallelTest, FilterRange) {
    // Test filter_range function
    std::vector<int> large_vector(2000);
    std::iota(large_vector.begin(), large_vector.end(), 1);

    auto result =
        Parallel::filter_range(large_vector, [](int x) { return x > 1000; });

    // Verify all results are > 1000
    for (int val : result) {
        EXPECT_GT(val, 1000);
    }

    EXPECT_EQ(result.size(), 1000);  // Numbers 1001-2000
}

TEST_F(ParallelTest, Sort) {
    std::vector<int> unsorted = random_vector;
    std::vector<int> expected = unsorted;

    // Sort expected vector sequentially
    std::sort(expected.begin(), expected.end());

    // Sort using parallel algorithm
    Parallel::sort(unsorted.begin(), unsorted.end());

    // Verify results match
    EXPECT_EQ(unsorted, expected);
}

TEST_F(ParallelTest, SortWithCustomComparator) {
    std::vector<int> unsorted = random_vector;

    // Sort in descending order
    Parallel::sort(unsorted.begin(), unsorted.end(), std::greater<int>());

    // Verify vector is sorted in descending order
    for (size_t i = 1; i < unsorted.size(); ++i) {
        EXPECT_GE(unsorted[i - 1], unsorted[i]);
    }
}

TEST_F(ParallelTest, Partition) {
    std::vector<int> data = random_vector;

    // Partition into evens and odds
    auto partition_point = Parallel::partition(
        data.begin(), data.end(), [](int x) { return x % 2 == 0; });

    // Verify all elements before partition point are even
    for (auto it = data.begin(); it != partition_point; ++it) {
        EXPECT_EQ(*it % 2, 0);
    }

    // Verify all elements after partition point are odd
    for (auto it = partition_point; it != data.end(); ++it) {
        EXPECT_EQ(*it % 2, 1);
    }
}

TEST_F(ParallelTest, AsyncTask) {
    // Test async task creation
    auto task = Parallel::async([]() { return 42; });

    EXPECT_FALSE(task.is_done());

    int result = task.get();
    EXPECT_EQ(result, 42);
    EXPECT_TRUE(task.is_done());
}

TEST_F(ParallelTest, AsyncTaskWithParameters) {
    auto task = Parallel::async([](int a, int b) { return a + b; }, 20, 22);

    int result = task.get();
    EXPECT_EQ(result, 42);
}

TEST_F(ParallelTest, AsyncTaskVoid) {
    std::atomic<bool> executed{false};

    auto task = Parallel::async([&executed]() { executed = true; });

    task.get();
    EXPECT_TRUE(executed.load());
}

TEST_F(ParallelTest, AsyncTaskException) {
    auto task = Parallel::async([]() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    });

    EXPECT_THROW(task.get(), std::runtime_error);
}

TEST_F(ParallelTest, WhenAllVoidTasks) {
    std::atomic<int> counter{0};

    auto task1 = Parallel::async([&counter]() { counter += 1; });
    auto task2 = Parallel::async([&counter]() { counter += 2; });
    auto task3 = Parallel::async([&counter]() { counter += 3; });

    auto combined_task = Parallel::when_all(std::move(task1), std::move(task2),
                                            std::move(task3));

    combined_task.get();
    EXPECT_EQ(counter.load(), 6);
}

TEST_F(ParallelTest, SIMDOperations) {
    const size_t size = 1024;
    std::vector<float> a(size), b(size), result(size);

    // Initialize with test data
    for (size_t i = 0; i < size; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }

    // Test SIMD addition
    SimdOps::add(a.data(), b.data(), result.data(), size);

    for (size_t i = 0; i < size; ++i) {
        EXPECT_FLOAT_EQ(result[i], a[i] + b[i]);
    }

    // Test SIMD multiplication
    SimdOps::multiply(a.data(), b.data(), result.data(), size);

    for (size_t i = 0; i < size; ++i) {
        EXPECT_FLOAT_EQ(result[i], a[i] * b[i]);
    }

    // Test SIMD dot product
    float dot_result = SimdOps::dotProduct(a.data(), b.data(), size);

    float expected_dot = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        expected_dot += a[i] * b[i];
    }
    EXPECT_FLOAT_EQ(dot_result, expected_dot);
}

TEST_F(ParallelTest, SIMDOperationsWithSpan) {
    const size_t size = 512;
    std::vector<float> a(size), b(size);

    // Initialize with test data
    for (size_t i = 0; i < size; ++i) {
        a[i] = static_cast<float>(i + 1);
        b[i] = static_cast<float>(size - i);
    }

    std::span<const float> span_a(a);
    std::span<const float> span_b(b);

    float dot_result = SimdOps::dotProduct(span_a, span_b);

    float expected_dot = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        expected_dot += a[i] * b[i];
    }
    EXPECT_FLOAT_EQ(dot_result, expected_dot);
}

TEST_F(ParallelTest, ThreadConfiguration) {
    // Test thread affinity (may not work on all systems)
    bool affinity_result = ThreadUtils::setThreadAffinity(0);
    // Don't assert on this as it may not be supported
    (void)affinity_result;

    // Test thread priority
    bool priority_result =
        ThreadUtils::setThreadPriority(ThreadUtils::Priority::Normal);
    // Don't assert on this as it may not be supported
    (void)priority_result;
}

TEST_F(ParallelTest, EmptyRanges) {
    std::vector<int> empty;

    // Test operations on empty ranges
    Parallel::for_each(empty.begin(), empty.end(), [](int&) {});

    auto map_result =
        Parallel::map(empty.begin(), empty.end(), [](int x) { return x * 2; });
    EXPECT_TRUE(map_result.empty());

    auto filter_result =
        Parallel::filter(empty.begin(), empty.end(), [](int) { return true; });
    EXPECT_TRUE(filter_result.empty());

    int reduce_result =
        Parallel::reduce(empty.begin(), empty.end(), 0, std::plus<int>());
    EXPECT_EQ(reduce_result, 0);
}

TEST_F(ParallelTest, SingleElementRanges) {
    std::vector<int> single = {42};

    // Test operations on single element
    Parallel::for_each(single.begin(), single.end(), [](int& x) { x *= 2; });
    EXPECT_EQ(single[0], 84);

    auto map_result = Parallel::map(single.begin(), single.end(),
                                    [](int x) { return x + 1; });
    ASSERT_EQ(map_result.size(), 1);
    EXPECT_EQ(map_result[0], 43);

    auto filter_result = Parallel::filter(single.begin(), single.end(),
                                          [](int x) { return x > 40; });
    ASSERT_EQ(filter_result.size(), 1);
    EXPECT_EQ(filter_result[0], 42);
}

TEST_F(ParallelTest, PerformanceComparison) {
    const size_t large_size = 100000;
    std::vector<int> large_data(large_size);
    std::iota(large_data.begin(), large_data.end(), 1);

    // Sequential for timing
    auto start_sequential = std::chrono::high_resolution_clock::now();
    std::for_each(large_data.begin(), large_data.end(),
                  [](int& x) { x = x * x; });
    auto end_sequential = std::chrono::high_resolution_clock::now();
    auto sequential_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_sequential -
                                                              start_sequential);

    // Reset data
    std::iota(large_data.begin(), large_data.end(), 1);

    // Parallel for timing
    auto start_parallel = std::chrono::high_resolution_clock::now();
    Parallel::for_each(large_data.begin(), large_data.end(),
                       [](int& x) { x = x * x; });
    auto end_parallel = std::chrono::high_resolution_clock::now();
    auto parallel_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_parallel - start_parallel);

    spdlog::info("Sequential time: {} ms, Parallel time: {} ms",
                 sequential_time.count(), parallel_time.count());

    // Verify results are the same
    for (size_t i = 0; i < large_data.size(); ++i) {
        EXPECT_EQ(large_data[i], static_cast<int>((i + 1) * (i + 1)));
    }
}

TEST_F(ParallelTest, ThreadSafety) {
    const size_t num_threads = 10;
    const size_t operations_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> counter{0};
    std::vector<int> results(num_threads * operations_per_thread);

    // Launch multiple threads performing parallel operations
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::vector<int> local_data(operations_per_thread);
            std::iota(local_data.begin(), local_data.end(),
                      t * operations_per_thread);

            // Use parallel algorithms
            auto squares = Parallel::map(local_data.begin(), local_data.end(),
                                         [](int x) { return x * x; });

            // Store results
            for (size_t i = 0; i < squares.size(); ++i) {
                results[t * operations_per_thread + i] = squares[i];
            }

            counter.fetch_add(operations_per_thread);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter.load(), num_threads * operations_per_thread);

    // Verify results
    for (size_t t = 0; t < num_threads; ++t) {
        for (size_t i = 0; i < operations_per_thread; ++i) {
            int expected = static_cast<int>((t * operations_per_thread + i) *
                                            (t * operations_per_thread + i));
            EXPECT_EQ(results[t * operations_per_thread + i], expected);
        }
    }
}

TEST_F(ParallelTest, ParallelForEachAsync) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::atomic<int> sum{0};

    auto task = Parallel::parallel_for_each_async(std::span<const int>(data),
                                                  [&sum](int x) { sum += x; });

    task.get();

    int expected_sum = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(sum.load(), expected_sum);
}
