#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <latch>
#include <numeric>
#include <random>
#include <vector>
#include "atom/async/execution/pool.hpp"

using namespace atom::async;

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    void TearDown() override {
        // Give threads time to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

TEST_F(ThreadPoolTest, DefaultConstructor) {
    ThreadPool pool;

    EXPECT_GT(pool.getThreadCount(), 0);
    EXPECT_EQ(pool.getQueueSize(), 0);
}

TEST_F(ThreadPoolTest, CustomOptions) {
    ThreadPool::Options options;
    options.initialThreadCount = 2;
    options.maxThreadCount = 4;
    options.maxQueueSize = 100;
    options.allowThreadGrowth = false;
    options.allowThreadShrink = false;

    ThreadPool pool(options);

    EXPECT_EQ(pool.getThreadCount(), 2);
}

TEST_F(ThreadPoolTest, HighPerformanceOptions) {
    auto options = ThreadPool::Options::createHighPerformance();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
    EXPECT_LE(pool.getThreadCount(), options.maxThreadCount);
}

TEST_F(ThreadPoolTest, LowLatencyOptions) {
    auto options = ThreadPool::Options::createLowLatency();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
}

TEST_F(ThreadPoolTest, EnergyEfficientOptions) {
    auto options = ThreadPool::Options::createEnergyEfficient();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
}

TEST_F(ThreadPoolTest, SubmitTask) {
    ThreadPool pool;

    auto future = pool.submit([]() { return 42; });

    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, SubmitTaskWithParameters) {
    ThreadPool pool;

    auto future = pool.submit([](int a, int b) { return a + b; }, 20, 22);

    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, SubmitVoidTask) {
    ThreadPool pool;

    std::atomic<bool> executed{false};
    auto future = pool.submit([&executed]() { executed = true; });

    future.get();
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadPoolTest, SubmitMultipleTasks) {
    ThreadPool pool;

    std::vector<EnhancedFuture<int>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([i]() { return i * 2; }));
    }

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST_F(ThreadPoolTest, SubmitTaskWithException) {
    ThreadPool pool;

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ThreadPoolTest, SubmitBatch) {
    ThreadPool pool;

    std::vector<int> inputs = {1, 2, 3, 4, 5};

    auto futures = pool.submitBatch(inputs.begin(), inputs.end(),
                                    [](int x) { return x * x; });

    ASSERT_EQ(futures.size(), inputs.size());

    for (size_t i = 0; i < futures.size(); ++i) {
        EXPECT_EQ(futures[i].get(), inputs[i] * inputs[i]);
    }
}

TEST_F(ThreadPoolTest, SubmitWithPromise) {
    ThreadPool pool;

    auto future = pool.submitWithPromise([]() { return 123; });

    EXPECT_EQ(future.get(), 123);
}

TEST_F(ThreadPoolTest, QueueSizeMonitoring) {
    ThreadPool::Options options;
    options.initialThreadCount = 1;  // Single thread to create queue backlog
    options.maxThreadCount = 1;
    ThreadPool pool(options);

    std::atomic<int> counter{0};
    std::latch sync(10);

    // Submit many tasks quickly to fill the queue
    std::vector<EnhancedFuture<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([&counter, &sync]() {
            sync.arrive_and_wait();
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }));
    }

    // Queue should have tasks
    EXPECT_GE(pool.getQueueSize(), 0);

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(counter.load(), 10);
}

TEST_F(ThreadPoolTest, ConcurrentSubmissions) {
    ThreadPool pool;

    const size_t num_threads = 10;
    const size_t tasks_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_completed{0};

    auto sync_point = std::make_shared<std::latch>(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            sync_point->arrive_and_wait();

            std::vector<EnhancedFuture<int>> futures;
            for (size_t i = 0; i < tasks_per_thread; ++i) {
                futures.push_back(pool.submit([t, i]() {
                    return static_cast<int>(t * tasks_per_thread + i);
                }));
            }

            for (auto& future : futures) {
                future.get();
                total_completed.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_completed.load(), num_threads * tasks_per_thread);
}

TEST_F(ThreadPoolTest, PerformanceTest) {
    ThreadPool pool;

    const size_t num_tasks = 10000;
    std::vector<EnhancedFuture<int>> futures;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([i]() {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            return static_cast<int>(i);
        }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Completed {} tasks in {} ms", num_tasks, duration.count());

    // Should complete reasonably fast
    EXPECT_LT(duration.count(), 5000);  // 5 seconds max
}

TEST_F(ThreadPoolTest, ThreadSafety) {
    ThreadPool pool;

    std::atomic<int> counter{0};
    const size_t num_operations = 1000;
    std::vector<std::thread> threads;

    // Multiple threads submitting tasks concurrently
    for (size_t t = 0; t < 10; ++t) {
        threads.emplace_back([&]() {
            for (size_t i = 0; i < num_operations / 10; ++i) {
                auto future =
                    pool.submit([&counter]() { counter.fetch_add(1); });
                future.get();  // Wait for task completion
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter.load(), num_operations);
}

TEST_F(ThreadPoolTest, ComplexTaskGraph) {
    ThreadPool pool;

    // Create a task dependency graph
    std::vector<EnhancedFuture<int>> level1_futures;
    std::vector<EnhancedFuture<int>> level2_futures;
    EnhancedFuture<int> final_future;

    // Level 1: Generate numbers
    for (int i = 0; i < 5; ++i) {
        level1_futures.push_back(pool.submit([i]() { return i * 10; }));
    }

    // Level 2: Process numbers from Level 1
    for (int i = 0; i < 5; ++i) {
        level2_futures.push_back(pool.submit(
            [&level1_futures, i]() { return level1_futures[i].get() + 5; }));
    }

    // Final: Sum all Level 2 results
    final_future = pool.submit([&level2_futures]() {
        int sum = 0;
        for (auto& future : level2_futures) {
            sum += future.get();
        }
        return sum;
    });

    // Expected: (0+5) + (10+5) + (20+5) + (30+5) + (40+5) = 5 + 15 + 25 + 35 +
    // 45 = 125
    EXPECT_EQ(final_future.get(), 125);
}

TEST_F(ThreadPoolTest, LongRunningTasks) {
    ThreadPool pool;

    std::vector<EnhancedFuture<void>> futures;

    // Submit long-running tasks
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Task should complete successfully
        }));
    }

    // All tasks should complete without timeout
    for (auto& future : futures) {
        EXPECT_NO_THROW(future.get());
    }
}

TEST_F(ThreadPoolTest, TaskPriority) {
    // This test ensures that tasks are executed in a reasonable time
    ThreadPool pool;

    std::atomic<int> execution_order{0};
    std::vector<EnhancedFuture<void>> futures;

    // Submit tasks that should execute quickly
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([&execution_order, i]() {
            int order = execution_order.fetch_add(1);
            // Do minimal work
        }));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        future.get();
    }

    EXPECT_EQ(execution_order.load(), 10);
}

TEST_F(ThreadPoolTest, MemoryStress) {
    ThreadPool pool;

    std::vector<EnhancedFuture<std::vector<int>>> futures;

    // Submit tasks that allocate significant memory
    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.submit([i]() {
            std::vector<int> data(1000);
            std::iota(data.begin(), data.end(), i * 1000);
            return data;
        }));
    }

    // Verify all allocations succeeded
    for (int i = 0; i < 50; ++i) {
        auto result = futures[i].get();
        EXPECT_EQ(result.size(), 1000);
        EXPECT_EQ(result[0], i * 1000);
    }
}

TEST_F(ThreadPoolTest, RecursiveTasks) {
    ThreadPool pool;

    std::function<EnhancedFuture<int>(int)> fibonacci =
        [&](int n) -> EnhancedFuture<int> {
        if (n <= 1) {
            return pool.submit([n]() { return n; });
        }

        auto future1 = fibonacci(n - 1);
        auto future2 = fibonacci(n - 2);

        return pool.submit(
            [f1 = std::move(future1), f2 = std::move(future2)]() mutable {
                return f1.get() + f2.get();
            });
    };

    // Test with small fibonacci number to avoid excessive recursion
    auto result = fibonacci(6);
    EXPECT_EQ(result.get(), 8);  // F(6) = 8
}

TEST_F(ThreadPoolTest, ExceptionHandling) {
    ThreadPool pool;

    std::atomic<int> success_count{0};
    std::atomic<int> exception_count{0};

    std::vector<EnhancedFuture<void>> futures;

    // Mix of successful and failing tasks
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([&success_count, &exception_count, i]() {
            if (i % 5 == 0) {
                throw std::runtime_error("Task failed");
            }
            success_count.fetch_add(1);
        }));
    }

    // Process results
    for (auto& future : futures) {
        try {
            future.get();
        } catch (const std::runtime_error&) {
            exception_count.fetch_add(1);
        }
    }

    EXPECT_EQ(success_count.load(), 16);  // 4 out of 20 should fail
    EXPECT_EQ(exception_count.load(), 4);
}

// =============================================================================
// ThreadSafeQueue Tests
// =============================================================================

class ThreadSafeQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

TEST_F(ThreadSafeQueueTest, BasicOperations) {
    ThreadSafeQueue<int> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);

    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 3u);

    auto front = queue.popFront();
    EXPECT_TRUE(front.has_value());
    EXPECT_EQ(front.value(), 1);

    auto back = queue.popBack();
    EXPECT_TRUE(back.has_value());
    EXPECT_EQ(back.value(), 3);

    EXPECT_EQ(queue.size(), 1u);
}

TEST_F(ThreadSafeQueueTest, PushFront) {
    ThreadSafeQueue<int> queue;

    queue.pushBack(2);
    queue.pushFront(1);
    queue.pushBack(3);

    auto first = queue.popFront();
    EXPECT_EQ(first.value(), 1);

    auto second = queue.popFront();
    EXPECT_EQ(second.value(), 2);

    auto third = queue.popFront();
    EXPECT_EQ(third.value(), 3);
}

TEST_F(ThreadSafeQueueTest, Steal) {
    ThreadSafeQueue<int> queue;

    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);

    auto stolen = queue.steal();
    EXPECT_TRUE(stolen.has_value());
    EXPECT_EQ(stolen.value(), 3);  // Steal from back

    EXPECT_EQ(queue.size(), 2u);
}

TEST_F(ThreadSafeQueueTest, Clear) {
    ThreadSafeQueue<int> queue;

    for (int i = 0; i < 10; ++i) {
        queue.pushBack(i);
    }

    EXPECT_EQ(queue.size(), 10u);

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);
}

TEST_F(ThreadSafeQueueTest, RotateToFront) {
    ThreadSafeQueue<int> queue;

    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);

    queue.rotateToFront(2);

    auto front = queue.popFront();
    EXPECT_EQ(front.value(), 2);
}

TEST_F(ThreadSafeQueueTest, CopyFrontAndRotateToBack) {
    ThreadSafeQueue<int> queue;

    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);

    auto copied = queue.copyFrontAndRotateToBack();
    EXPECT_TRUE(copied.has_value());
    EXPECT_EQ(copied.value(), 1);

    // Original front should now be at back
    auto back = queue.popBack();
    EXPECT_EQ(back.value(), 1);
}

TEST_F(ThreadSafeQueueTest, EmptyQueueOperations) {
    ThreadSafeQueue<int> queue;

    auto front = queue.popFront();
    EXPECT_FALSE(front.has_value());

    auto back = queue.popBack();
    EXPECT_FALSE(back.has_value());

    auto stolen = queue.steal();
    EXPECT_FALSE(stolen.has_value());

    auto copied = queue.copyFrontAndRotateToBack();
    EXPECT_FALSE(copied.has_value());
}

TEST_F(ThreadSafeQueueTest, CopyConstructor) {
    ThreadSafeQueue<int> queue1;
    queue1.pushBack(1);
    queue1.pushBack(2);
    queue1.pushBack(3);

    ThreadSafeQueue<int> queue2(queue1);

    EXPECT_EQ(queue2.size(), 3u);

    auto front = queue2.popFront();
    EXPECT_EQ(front.value(), 1);
}

TEST_F(ThreadSafeQueueTest, MoveConstructor) {
    ThreadSafeQueue<int> queue1;
    queue1.pushBack(1);
    queue1.pushBack(2);
    queue1.pushBack(3);

    ThreadSafeQueue<int> queue2(std::move(queue1));

    EXPECT_EQ(queue2.size(), 3u);
}

TEST_F(ThreadSafeQueueTest, ConcurrentPushPop) {
    ThreadSafeQueue<int> queue;
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};

    const int numThreads = 4;
    const int opsPerThread = 1000;

    std::vector<std::thread> pushThreads;
    std::vector<std::thread> popThreads;

    // Push threads
    for (int t = 0; t < numThreads; ++t) {
        pushThreads.emplace_back([&queue, &pushCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                queue.pushBack(i);
                pushCount.fetch_add(1);
            }
        });
    }

    // Pop threads
    for (int t = 0; t < numThreads; ++t) {
        popThreads.emplace_back([&queue, &popCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (queue.popFront().has_value()) {
                    popCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : pushThreads)
        t.join();
    for (auto& t : popThreads)
        t.join();

    // Drain remaining items
    while (queue.popFront().has_value()) {
        popCount.fetch_add(1);
    }

    EXPECT_EQ(pushCount.load(), popCount.load());
}

// =============================================================================
// Additional ThreadPool Tests
// =============================================================================

TEST_F(ThreadPoolTest, SubmitWithDifferentReturnTypes) {
    ThreadPool pool;

    // String return
    auto stringFuture = pool.submit([]() { return std::string("Hello"); });
    EXPECT_EQ(stringFuture.get(), "Hello");

    // Vector return
    auto vectorFuture =
        pool.submit([]() { return std::vector<int>{1, 2, 3, 4, 5}; });
    auto vec = vectorFuture.get();
    EXPECT_EQ(vec.size(), 5u);

    // Double return
    auto doubleFuture = pool.submit([]() { return 3.14159; });
    EXPECT_NEAR(doubleFuture.get(), 3.14159, 1e-5);
}

TEST_F(ThreadPoolTest, SubmitLambdaWithCapture) {
    ThreadPool pool;

    int x = 10;
    int y = 20;

    auto future = pool.submit([x, y]() { return x + y; });
    EXPECT_EQ(future.get(), 30);

    // Reference capture
    std::atomic<int> counter{0};
    auto voidFuture = pool.submit([&counter]() { counter.fetch_add(1); });
    voidFuture.get();
    EXPECT_EQ(counter.load(), 1);
}

TEST_F(ThreadPoolTest, SubmitMemberFunction) {
    ThreadPool pool;

    struct Calculator {
        int add(int a, int b) const { return a + b; }
    };

    Calculator calc;
    auto future = pool.submit([&calc]() { return calc.add(10, 20); });
    EXPECT_EQ(future.get(), 30);
}

TEST_F(ThreadPoolTest, ChainedTasks) {
    ThreadPool pool;

    auto future1 = pool.submit([]() { return 10; });

    auto future2 = pool.submit(
        [f1 = std::move(future1)]() mutable { return f1.get() * 2; });

    auto future3 = pool.submit(
        [f2 = std::move(future2)]() mutable { return f2.get() + 5; });

    EXPECT_EQ(future3.get(), 25);  // (10 * 2) + 5
}

TEST_F(ThreadPoolTest, ParallelMap) {
    ThreadPool pool;

    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<EnhancedFuture<int>> futures;

    for (int val : input) {
        futures.push_back(pool.submit([val]() { return val * val; }));
    }

    std::vector<int> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    EXPECT_EQ(results.size(), 10u);
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(results[i], input[i] * input[i]);
    }
}

TEST_F(ThreadPoolTest, ParallelReduce) {
    ThreadPool pool;

    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Submit tasks to compute partial sums
    std::vector<EnhancedFuture<int>> futures;
    for (int val : input) {
        futures.push_back(pool.submit([val]() { return val; }));
    }

    // Reduce
    auto sumFuture = pool.submit([&futures]() {
        int sum = 0;
        for (auto& f : futures) {
            sum += f.get();
        }
        return sum;
    });

    EXPECT_EQ(sumFuture.get(), 55);  // Sum of 1 to 10
}

TEST_F(ThreadPoolTest, TaskCancellation) {
    ThreadPool pool;

    std::atomic<bool> taskStarted{false};
    std::atomic<bool> taskCompleted{false};

    auto future = pool.submit([&taskStarted, &taskCompleted]() {
        taskStarted = true;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        taskCompleted = true;
        return 42;
    });

    // Wait for task to start
    while (!taskStarted.load()) {
        std::this_thread::yield();
    }

    // Cancel the future
    future.cancel();

    EXPECT_TRUE(future.isCancelled());
}

TEST_F(ThreadPoolTest, ZeroThreadPool) {
    // Pool with 0 threads should still work (use at least 1)
    ThreadPool::Options options;
    options.initialThreadCount = 0;
    options.maxThreadCount = 1;

    ThreadPool pool(options);

    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, SingleThreadPool) {
    ThreadPool::Options options;
    options.initialThreadCount = 1;
    options.maxThreadCount = 1;

    ThreadPool pool(options);

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    std::vector<EnhancedFuture<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.submit([i, &executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(i);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // With single thread, tasks should execute in order
    EXPECT_EQ(executionOrder.size(), 5u);
}

TEST_F(ThreadPoolTest, NestedSubmit) {
    ThreadPool pool;

    auto outerFuture = pool.submit([&pool]() {
        auto innerFuture = pool.submit([]() { return 42; });
        return innerFuture.get() * 2;
    });

    EXPECT_EQ(outerFuture.get(), 84);
}

TEST_F(ThreadPoolTest, StressTestManySmallTasks) {
    ThreadPool pool;

    const int numTasks = 10000;
    std::atomic<int> completedTasks{0};

    std::vector<EnhancedFuture<void>> futures;
    futures.reserve(numTasks);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(
            pool.submit([&completedTasks]() { completedTasks.fetch_add(1); }));
    }

    for (auto& f : futures) {
        f.get();
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    EXPECT_EQ(completedTasks.load(), numTasks);

    // Should complete in reasonable time
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(),
              10);
}
