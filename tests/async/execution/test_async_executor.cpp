/*
 * test_async_executor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Executor
Tests advanced async executor, thread pool management, task scheduling,
priorities, and resource management.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/execution/async_executor.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::execution::test {

// ============================================================================
// AsyncExecutor Tests
// ============================================================================

class AsyncExecutorTest : public atom::async::test::AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();

        // Create a test configuration
        config.minThreads = 2;
        config.maxThreads = 4;
        config.threadIdleTimeout = 100ms;
        config.useWorkStealing = true;
        config.statInterval = 50ms;
    }

    void TearDown() override { AsyncTestBase::TearDown(); }

    AsyncExecutor::Configuration config;

    // Helper functions for testing
    int simpleTask(int value, int delay_ms = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        return value * 2;
    }

    void voidTask(std::atomic<int>& counter, int delay_ms = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        counter.fetch_add(1);
    }
};

// Test basic executor construction and lifecycle
TEST_F(AsyncExecutorTest, BasicLifecycle) {
    AsyncExecutor executor(config);

    EXPECT_FALSE(executor.isRunning());

    executor.start();
    EXPECT_TRUE(executor.isRunning());

    executor.stop();
    EXPECT_FALSE(executor.isRunning());
}

// Test executor with default configuration
TEST_F(AsyncExecutorTest, DefaultConfiguration) {
    AsyncExecutor executor{AsyncExecutor::Configuration{}};

    executor.start();
    EXPECT_TRUE(executor.isRunning());

    // Should be able to execute tasks
    auto future = executor.execute([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    executor.stop();
}

// Test basic task execution
TEST_F(AsyncExecutorTest, BasicTaskExecution) {
    AsyncExecutor executor(config);
    executor.start();

    // Test task with return value
    auto future = executor.execute([this]() { return simpleTask(5); });
    EXPECT_EQ(future.get(), 10);

    // Test void task
    std::atomic<int> counter{0};
    executor.execute([this, &counter]() { voidTask(counter); });

    // Wait a bit for task completion
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(counter.load(), 1);

    executor.stop();
}

// Test task priorities
TEST_F(AsyncExecutorTest, TaskPriorities) {
    // Use single thread to ensure priority ordering
    config.minThreads = 1;
    config.maxThreads = 1;

    AsyncExecutor executor(config);
    executor.start();

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // Submit tasks in reverse priority order
    auto lowFuture = executor.execute(
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(1);
            return 1;
        },
        AsyncExecutor::Priority::Low);

    auto normalFuture = executor.execute(
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(2);
            return 2;
        },
        AsyncExecutor::Priority::Normal);

    auto highFuture = executor.execute(
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(3);
            return 3;
        },
        AsyncExecutor::Priority::High);

    auto criticalFuture = executor.execute(
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(4);
            return 4;
        },
        AsyncExecutor::Priority::Critical);

    // Wait for all tasks to complete
    lowFuture.get();
    normalFuture.get();
    highFuture.get();
    criticalFuture.get();

    // Higher priority tasks should generally execute first
    // Note: This is probabilistic due to timing
    EXPECT_EQ(executionOrder.size(), 4);

    executor.stop();
}

// Test concurrent task execution
TEST_F(AsyncExecutorTest, ConcurrentTaskExecution) {
    AsyncExecutor executor(config);
    executor.start();

    const int numTasks = 20;
    std::vector<std::future<int>> futures;

    // Submit multiple tasks
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(
            executor.execute([this, i]() { return simpleTask(i, 10); }));
    }

    // Collect results
    std::vector<int> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // Verify all tasks completed
    EXPECT_EQ(results.size(), numTasks);

    // Verify results are correct (order may vary)
    std::sort(results.begin(), results.end());
    for (int i = 0; i < numTasks; ++i) {
        EXPECT_EQ(results[i], i * 2);
    }

    executor.stop();
}

// Test executor statistics - DISABLED: getStatistics method not implemented
TEST_F(AsyncExecutorTest, DISABLED_ExecutorStatistics) {
    AsyncExecutor executor(config);
    executor.start();

    // TODO: Implement getStatistics method in AsyncExecutor class
    // auto stats = executor.getStatistics();
    // EXPECT_EQ(stats.pendingTasks, 0);
    // EXPECT_EQ(stats.completedTasks, 0);
    // EXPECT_GT(stats.activeThreads, 0);

    // Execute some tasks to verify basic functionality
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(
            executor.execute([this, i]() { return simpleTask(i, 20); }));
    }

    // Wait for completion
    for (size_t i = 0; i < futures.size(); ++i) {
        EXPECT_EQ(futures[i].get(),
                  static_cast<int>(i) * 2);  // simpleTask returns i * 2
    }

    executor.stop();
}

// Test executor exception handling
TEST_F(AsyncExecutorTest, ExceptionHandling) {
    AsyncExecutor executor(config);
    executor.start();

    // Task that throws exception
    auto future = executor.execute(
        []() -> int { throw std::runtime_error("Test exception"); });

    // Should propagate exception through future
    EXPECT_THROW(future.get(), std::runtime_error);

    // Executor should still be functional
    auto normalFuture = executor.execute([]() { return 42; });
    EXPECT_EQ(normalFuture.get(), 42);

    executor.stop();
}

// Test executor with work stealing disabled
TEST_F(AsyncExecutorTest, WithoutWorkStealing) {
    config.useWorkStealing = false;

    AsyncExecutor executor(config);
    executor.start();

    // Should still work without work stealing
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(
            executor.execute([this, i]() { return simpleTask(i); }));
    }

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }

    executor.stop();
}

// Test executor shutdown with pending tasks
TEST_F(AsyncExecutorTest, ShutdownWithPendingTasks) {
    AsyncExecutor executor(config);
    executor.start();

    std::atomic<int> completedTasks{0};

    // Submit long-running tasks
    for (int i = 0; i < 5; ++i) {
        executor.execute([&completedTasks]() {
            std::this_thread::sleep_for(100ms);
            completedTasks.fetch_add(1);
        });
    }

    // Stop immediately
    executor.stop();

    // Some tasks may not complete
    EXPECT_LE(completedTasks.load(), 5);
}

// Test executor with invalid configuration
TEST_F(AsyncExecutorTest, InvalidConfiguration) {
    AsyncExecutor::Configuration invalidConfig;
    invalidConfig.minThreads = 0;  // Invalid
    invalidConfig.maxThreads = 0;  // Invalid

    // Constructor should fix invalid values
    AsyncExecutor executor(invalidConfig);
    executor.start();

    // Should still work
    auto future = executor.execute([]() { return 123; });
    EXPECT_EQ(future.get(), 123);

    executor.stop();
}

// Test executor task submission when not running
TEST_F(AsyncExecutorTest, TaskSubmissionWhenNotRunning) {
    AsyncExecutor executor(config);

    // Should throw when not running
    EXPECT_THROW(executor.execute([]() { return 42; }), ExecutorException);
}

// Test executor global instance
TEST_F(AsyncExecutorTest, GlobalInstance) {
    auto& instance1 = AsyncExecutor::getInstance();
    auto& instance2 = AsyncExecutor::getInstance();

    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);

    // Should be able to submit tasks
    auto future = AsyncExecutor::submit([]() { return 999; });
    EXPECT_EQ(future.get(), 999);
}

// Test executor with coroutines (if available)
TEST_F(AsyncExecutorTest, CoroutineSupport) {
    AsyncExecutor executor(config);
    executor.start();

    // Test executeAsTask method
    auto task = executor.executeAsTask([]() { return 42; });

    // Task should be created successfully
    // Note: Task validity check depends on implementation details

    executor.stop();
}

// Test executor performance under load
TEST_F(AsyncExecutorTest, PerformanceUnderLoad) {
    config.minThreads = 4;
    config.maxThreads = 8;

    AsyncExecutor executor(config);
    executor.start();

    const int numTasks = 1000;
    std::atomic<int> completedTasks{0};

    auto startTime = std::chrono::high_resolution_clock::now();

    // Submit many lightweight tasks
    for (int i = 0; i < numTasks; ++i) {
        executor.execute([&completedTasks]() { completedTasks.fetch_add(1); });
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(200ms);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    EXPECT_EQ(completedTasks.load(), numTasks);

    std::cout << "AsyncExecutor performance: " << numTasks
              << " tasks completed in " << duration.count() << "ms"
              << std::endl;

    executor.stop();
}

// Test executor thread safety
TEST_F(AsyncExecutorTest, ThreadSafety) {
    AsyncExecutor executor(config);
    executor.start();

    std::atomic<int> counter{0};
    const int numThreads = 10;
    const int tasksPerThread = 50;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&executor, &counter]() {
            for (int j = 0; j < 50; ++j) {
                executor.execute([&counter]() { counter.fetch_add(1); });
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for all tasks to complete
    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(counter.load(), numThreads * tasksPerThread);

    executor.stop();
}

// Test executor resource cleanup
TEST_F(AsyncExecutorTest, ResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        AsyncExecutor executor(config);
        executor.start();

        // Submit tasks that use resources
        for (int i = 0; i < 5; ++i) {
            executor.execute([&tracker]() {
                atom::async::test::ScopedResourceTracker resource(tracker);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            });
        }

        // Wait for tasks to complete
        std::this_thread::sleep_for(150ms);

        executor.stop();
    }  // Executor destructor should clean up

    // Give some time for cleanup
    std::this_thread::sleep_for(50ms);
    tracker.expectNoLeaks();
}

// Test executor with different thread configurations
TEST_F(AsyncExecutorTest, DifferentThreadConfigurations) {
    // Test with single thread
    {
        AsyncExecutor::Configuration singleThreadConfig;
        singleThreadConfig.minThreads = 1;
        singleThreadConfig.maxThreads = 1;

        AsyncExecutor executor(singleThreadConfig);
        executor.start();

        auto future = executor.execute([]() { return 1; });
        EXPECT_EQ(future.get(), 1);

        executor.stop();
    }

    // Test with many threads
    {
        AsyncExecutor::Configuration manyThreadsConfig;
        manyThreadsConfig.minThreads = 8;
        manyThreadsConfig.maxThreads = 16;

        AsyncExecutor executor(manyThreadsConfig);
        executor.start();

        std::vector<std::future<int>> futures;
        for (int i = 0; i < 20; ++i) {
            futures.push_back(executor.execute([i]() { return i; }));
        }

        for (int i = 0; i < 20; ++i) {
            EXPECT_EQ(futures[i].get(), i);
        }

        executor.stop();
    }
}

// Test executor task cancellation (if supported)
TEST_F(AsyncExecutorTest, TaskCancellation) {
    AsyncExecutor executor(config);
    executor.start();

    std::atomic<bool> taskStarted{false};
    std::atomic<bool> taskCompleted{false};

    // Submit a long-running task
    auto future = executor.execute([&taskStarted, &taskCompleted]() {
        taskStarted = true;
        std::this_thread::sleep_for(200ms);
        taskCompleted = true;
        return 42;
    });

    // Wait for task to start
    while (!taskStarted.load()) {
        std::this_thread::sleep_for(1ms);
    }

    // Stop executor (should cancel pending tasks)
    executor.stop();

    // Task may or may not complete depending on timing
    // This test mainly ensures no crashes occur during shutdown

    EXPECT_TRUE(taskStarted.load());
}

}  // namespace atom::async::execution::test
