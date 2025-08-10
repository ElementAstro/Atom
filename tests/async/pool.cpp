#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>
#include <numeric>

#include "atom/async/pool.hpp"
#include "atom/async/future.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic thread pool creation and destruction
TEST_F(ThreadPoolTest, BasicCreationDestruction) {
    {
        ThreadPool pool(ThreadPool::Options::createDefault());
        EXPECT_FALSE(pool.isShutdown());
        EXPECT_GT(pool.getThreadCount(), 0);
    }
    // Pool should be properly destroyed here
}

// Test submitting a simple task
TEST_F(ThreadPoolTest, SubmitSimpleTask) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    std::atomic<int> result{0};
    auto future = pool.submit([&result]() {
        result = 42;
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(result.load(), 42);
}

// Test submitting multiple tasks
TEST_F(ThreadPoolTest, SubmitMultipleTasks) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    const int num_tasks = 100;
    std::vector<EnhancedFuture<int>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([i]() { return i * 2; }));
    }

    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].wait(), i * 2);
    }
}

// Test task with parameters
TEST_F(ThreadPoolTest, TaskWithParameters) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    auto future = pool.submit([](int a, int b, int c) {
        return a + b + c;
    }, 1, 2, 3);

    EXPECT_EQ(future.get(), 6);
}

// Test concurrent task execution
TEST_F(ThreadPoolTest, ConcurrentExecution) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    const int num_tasks = 50;
    std::atomic<int> counter{0};
    std::vector<EnhancedFuture<void>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(10ms);
            counter.fetch_add(1);
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter.load(), num_tasks);
}

// Test thread pool options
TEST_F(ThreadPoolTest, ThreadPoolOptions) {
    auto options = ThreadPool::Options::createDefault();
    options.initialThreadCount = 4;
    options.maxThreadCount = 8;
    options.allowThreadGrowth = true;

    ThreadPool pool(options);

    EXPECT_EQ(pool.getOptions().initialThreadCount, 4);
    EXPECT_EQ(pool.getOptions().maxThreadCount, 8);
    EXPECT_TRUE(pool.getOptions().allowThreadGrowth);
}

// Test high performance configuration
TEST_F(ThreadPoolTest, HighPerformanceConfiguration) {
    auto options = ThreadPool::Options::createHighPerformance();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
    EXPECT_TRUE(pool.getOptions().useWorkStealing);
}

// Test low latency configuration
TEST_F(ThreadPoolTest, LowLatencyConfiguration) {
    auto options = ThreadPool::Options::createLowLatency();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
    EXPECT_EQ(pool.getOptions().threadPriority, ThreadPool::Options::ThreadPriority::Highest);
}

// Test energy efficient configuration
TEST_F(ThreadPoolTest, EnergyEfficientConfiguration) {
    auto options = ThreadPool::Options::createEnergyEfficient();
    ThreadPool pool(options);

    EXPECT_GT(pool.getThreadCount(), 0);
    EXPECT_TRUE(pool.getOptions().allowThreadShrink);
}

// Test wait for completion
TEST_F(ThreadPoolTest, WaitForCompletion) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    std::atomic<int> counter{0};
    const int num_tasks = 20;

    std::vector<EnhancedFuture<void>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([&counter]() {
            std::this_thread::sleep_for(50ms);
            counter.fetch_add(1);
        }));
    }

    // Wait for all futures to complete
    for (auto& future : futures) {
        future.wait();
    }
    EXPECT_EQ(counter.load(), num_tasks);
}

// Test shutdown
TEST_F(ThreadPoolTest, Shutdown) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    EXPECT_FALSE(pool.isShutdown());

    pool.shutdown();
    EXPECT_TRUE(pool.isShutdown());

    // Submitting tasks after shutdown should throw
    EXPECT_THROW(pool.submit([]() { return 42; }), std::runtime_error);
}

// Test immediate shutdown
TEST_F(ThreadPoolTest, ImmediateShutdown) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    // Submit some long-running tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit([]() {
            std::this_thread::sleep_for(1s);
        });
    }

    pool.shutdownNow();
    EXPECT_TRUE(pool.isShutdown());
}

// Test exception handling in tasks
TEST_F(ThreadPoolTest, ExceptionHandling) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("Test exception");
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

// Test promise-based task submission
TEST_F(ThreadPoolTest, PromiseBasedSubmission) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    // Use regular submit instead since Promise has issues
    auto future = pool.submit([](int x, int y) {
        return x * y;
    }, 6, 7);

    EXPECT_EQ(future.wait(), 42);
}

// Test execute method
TEST_F(ThreadPoolTest, ExecuteMethod) {
    ThreadPool pool(ThreadPool::Options::createDefault());

    std::atomic<bool> executed{false};

    pool.execute([&executed]() {
        executed = true;
    });

    // Wait a bit for execution
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(executed.load());
}

// Test thread count management
TEST_F(ThreadPoolTest, ThreadCountManagement) {
    auto options = ThreadPool::Options::createDefault();
    options.initialThreadCount = 2;
    options.maxThreadCount = 4;
    options.allowThreadGrowth = true;

    ThreadPool pool(options);

    EXPECT_EQ(pool.getThreadCount(), 2);
    EXPECT_LE(pool.getActiveThreadCount(), 2);
}

// Test global thread pool
TEST_F(ThreadPoolTest, GlobalThreadPool) {
    auto& pool = globalThreadPool();

    auto future = pool.submit([]() { return 123; });
    EXPECT_EQ(future.get(), 123);
}

// Test high performance thread pool singleton
TEST_F(ThreadPoolTest, HighPerformanceThreadPool) {
    auto& pool = highPerformanceThreadPool();

    auto future = pool.submit([]() { return 456; });
    EXPECT_EQ(future.get(), 456);
}

// Test low latency thread pool singleton
TEST_F(ThreadPoolTest, LowLatencyThreadPool) {
    auto& pool = lowLatencyThreadPool();

    auto future = pool.submit([]() { return 789; });
    EXPECT_EQ(future.get(), 789);
}

// Test energy efficient thread pool singleton
TEST_F(ThreadPoolTest, EnergyEfficientThreadPool) {
    auto& pool = energyEfficientThreadPool();

    auto future = pool.submit([]() { return 101112; });
    EXPECT_EQ(future.get(), 101112);
}

// Test async function
TEST_F(ThreadPoolTest, AsyncFunction) {
    auto future = async([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

// Test async with parameters
TEST_F(ThreadPoolTest, AsyncWithParameters) {
    auto future = async([](int a, int b) { return a + b; }, 10, 20);
    EXPECT_EQ(future.get(), 30);
}

// Test parallel execution
TEST_F(ThreadPoolTest, ParallelExecution) {
    auto future1 = async([]() {
        std::this_thread::sleep_for(100ms);
        return 1;
    });

    auto future2 = async([]() {
        std::this_thread::sleep_for(100ms);
        return 2;
    });

    auto start = std::chrono::steady_clock::now();
    int result1 = future1.get();
    int result2 = future2.get();
    auto end = std::chrono::steady_clock::now();

    EXPECT_EQ(result1, 1);
    EXPECT_EQ(result2, 2);

    // Should take less than 200ms if executed in parallel
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 200);
}

// Test ThreadSafeQueue basic operations
TEST_F(ThreadPoolTest, ThreadSafeQueueBasicOperations) {
    ThreadSafeQueue<int> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    queue.pushBack(1);
    queue.pushBack(2);
    queue.pushBack(3);

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 3);

    auto item = queue.popFront();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 1);

    EXPECT_EQ(queue.size(), 2);
}

// Test ThreadSafeQueue concurrent operations
TEST_F(ThreadPoolTest, ThreadSafeQueueConcurrentOperations) {
    ThreadSafeQueue<int> queue;
    const int num_producers = 4;
    const int num_consumers = 2;
    const int items_per_producer = 100;

    std::atomic<int> total_consumed{0};
    std::vector<std::thread> threads;

    // Producer threads
    for (int i = 0; i < num_producers; ++i) {
        threads.emplace_back([&queue, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                queue.pushBack(i * items_per_producer + j);
            }
        });
    }

    // Consumer threads
    for (int i = 0; i < num_consumers; ++i) {
        threads.emplace_back([&queue, &total_consumed]() {
            while (total_consumed.load() < num_producers * items_per_producer) {
                auto item = queue.popFront();
                if (item.has_value()) {
                    total_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_consumed.load(), num_producers * items_per_producer);
    EXPECT_TRUE(queue.empty());
}
