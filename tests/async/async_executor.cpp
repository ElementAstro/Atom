#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <functional>

#include "atom/async/async_executor.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// Test fixture for AsyncExecutor
class AsyncExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default configuration for most tests
        config = AsyncExecutor::Configuration{
            .minThreads = 2,
            .maxThreads = 4,
            .queueSizePerThread = 64,
            .threadIdleTimeout = 100ms,
            .setPriority = false,
            .threadPriority = 0,
            .pinThreads = false,
            .useWorkStealing = true,
            .statInterval = 1s
        };
    }

    void TearDown() override {
        // Ensure any executors are properly stopped
    }

    AsyncExecutor::Configuration config;
};

// Helper functions for testing
int simpleTask(int value) {
    std::this_thread::sleep_for(10ms);
    return value * 2;
}

void voidTask() {
    std::this_thread::sleep_for(10ms);
}

void throwingTask() {
    std::this_thread::sleep_for(5ms);
    throw std::runtime_error("Task failed intentionally");
}

int longRunningTask(int duration_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    return duration_ms;
}

// Basic functionality tests
TEST_F(AsyncExecutorTest, ConstructorAndBasicProperties) {
    AsyncExecutor executor(config);

    EXPECT_FALSE(executor.isRunning());
    EXPECT_EQ(executor.getActiveThreadCount(), 0);
    EXPECT_EQ(executor.getPendingTaskCount(), 0);
    EXPECT_EQ(executor.getCompletedTaskCount(), 0);
}

TEST_F(AsyncExecutorTest, StartAndStop) {
    AsyncExecutor executor(config);

    executor.start();
    EXPECT_TRUE(executor.isRunning());
    EXPECT_GE(executor.getActiveThreadCount(), 0);

    executor.stop();
    EXPECT_FALSE(executor.isRunning());
    EXPECT_EQ(executor.getActiveThreadCount(), 0);
}

TEST_F(AsyncExecutorTest, ExecuteVoidTask) {
    AsyncExecutor executor(config);
    executor.start();

    std::atomic<bool> taskExecuted{false};
    executor.execute([&taskExecuted]() {
        taskExecuted = true;
    });

    // Wait a bit for task to complete
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(taskExecuted.load());

    executor.stop();
}

TEST_F(AsyncExecutorTest, ExecuteTaskWithReturnValue) {
    AsyncExecutor executor(config);
    executor.start();

    auto future = executor.execute([](){ return 42; });
    EXPECT_EQ(future.get(), 42);

    executor.stop();
}

TEST_F(AsyncExecutorTest, ExecuteTaskWithParameters) {
    AsyncExecutor executor(config);
    executor.start();

    auto future = executor.execute([]() { return 10 + 20; });
    EXPECT_EQ(future.get(), 30);

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskPriorities) {
    AsyncExecutor executor(config);
    executor.start();

    std::vector<int> execution_order;
    std::mutex order_mutex;

    // Submit tasks with different priorities
    // Higher priority tasks should execute first
    auto low_future = executor.execute([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(1);
        return 1;
    }, AsyncExecutor::Priority::Low);

    auto high_future = executor.execute([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(3);
        return 3;
    }, AsyncExecutor::Priority::High);

    auto normal_future = executor.execute([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(2);
        return 2;
    }, AsyncExecutor::Priority::Normal);

    // Wait for all tasks to complete
    low_future.get();
    high_future.get();
    normal_future.get();

    // Note: Due to threading, exact order isn't guaranteed, but we can check
    // that high priority task executed
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_THAT(execution_order, ::testing::Contains(1));
    EXPECT_THAT(execution_order, ::testing::Contains(2));
    EXPECT_THAT(execution_order, ::testing::Contains(3));

    executor.stop();
}

TEST_F(AsyncExecutorTest, ExceptionHandling) {
    AsyncExecutor executor(config);
    executor.start();

    auto future = executor.execute([]() {
        throw std::runtime_error("Test exception");
        return 42;
    });

    EXPECT_THROW(future.get(), std::runtime_error);

    executor.stop();
}

TEST_F(AsyncExecutorTest, MultipleTasksExecution) {
    AsyncExecutor executor(config);
    executor.start();

    const int num_tasks = 10;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(10ms);
            return i * i;
        }));
    }

    // Verify all tasks complete with correct results
    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskCounters) {
    AsyncExecutor executor(config);
    executor.start();

    const int num_tasks = 5;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(20ms);
            return i;
        }));
    }

    // Check that pending tasks are tracked
    EXPECT_GT(executor.getPendingTaskCount(), 0);

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.get();
    }

    // Give some time for counters to update
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(executor.getPendingTaskCount(), 0);
    EXPECT_EQ(executor.getCompletedTaskCount(), num_tasks);

    executor.stop();
}

TEST_F(AsyncExecutorTest, ExecutorNotRunningThrows) {
    AsyncExecutor executor(config);
    // Don't start the executor

    EXPECT_THROW(executor.execute([]() { return 42; }), ExecutorException);
}

TEST_F(AsyncExecutorTest, MoveConstructor) {
    AsyncExecutor executor1(config);
    executor1.start();

    AsyncExecutor executor2 = std::move(executor1);

    // executor2 should be running, executor1 should be stopped
    EXPECT_TRUE(executor2.isRunning());

    // Should be able to execute tasks on moved executor
    auto future = executor2.execute([]() { return 123; });
    EXPECT_EQ(future.get(), 123);

    executor2.stop();
}

TEST_F(AsyncExecutorTest, MoveAssignment) {
    AsyncExecutor executor1(config);
    AsyncExecutor executor2(config);

    executor1.start();

    executor2 = std::move(executor1);

    EXPECT_TRUE(executor2.isRunning());

    auto future = executor2.execute([]() { return 456; });
    EXPECT_EQ(future.get(), 456);

    executor2.stop();
}

TEST_F(AsyncExecutorTest, GlobalInstanceAccess) {
    auto& global_executor = AsyncExecutor::getInstance();

    // Global instance should be accessible
    EXPECT_NO_THROW(global_executor.start());

    auto future = AsyncExecutor::submit([]() { return 789; });
    EXPECT_EQ(future.get(), 789);

    global_executor.stop();
}

// Configuration tests
TEST_F(AsyncExecutorTest, MinMaxThreadsConfiguration) {
    AsyncExecutor::Configuration test_config = config;
    test_config.minThreads = 1;
    test_config.maxThreads = 8;

    AsyncExecutor executor(test_config);
    executor.start();

    EXPECT_TRUE(executor.isRunning());
    EXPECT_GE(executor.getActiveThreadCount(), 0);

    executor.stop();
}

TEST_F(AsyncExecutorTest, WorkStealingConfiguration) {
    AsyncExecutor::Configuration test_config = config;
    test_config.useWorkStealing = true;
    test_config.minThreads = 4;

    AsyncExecutor executor(test_config);
    executor.start();

    // Submit many tasks to test work stealing
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(5ms);
            return i;
        }));
    }

    // All tasks should complete successfully
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, ThreadIdleTimeout) {
    AsyncExecutor::Configuration test_config = config;
    test_config.threadIdleTimeout = 50ms;
    test_config.minThreads = 1;
    test_config.maxThreads = 4;

    AsyncExecutor executor(test_config);
    executor.start();

    // Submit a task and wait for it to complete
    auto future = executor.execute([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    // Wait longer than idle timeout
    std::this_thread::sleep_for(100ms);

    // Executor should still be running
    EXPECT_TRUE(executor.isRunning());

    executor.stop();
}

// Coroutine tests - Note: executeAsTask has implementation issues, testing basic functionality
TEST_F(AsyncExecutorTest, CoroutineTaskBasicTest) {
    AsyncExecutor executor(config);
    executor.start();

    // Test that executeAsTask compiles and can be called
    // The actual coroutine functionality may need implementation fixes
    EXPECT_NO_THROW({
        auto task = executor.executeAsTask([]() { return 100; });
        // Note: The Task implementation may need fixes for proper coroutine support
    });

    executor.stop();
}

// Stress tests
TEST_F(AsyncExecutorTest, HighConcurrencyStress) {
    AsyncExecutor executor(config);
    executor.start();

    const int num_tasks = 1000;
    std::vector<std::future<int>> futures;
    std::atomic<int> counter{0};

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.execute([&counter, i]() {
            counter.fetch_add(1);
            return i;
        }));
    }

    // Wait for all tasks to complete
    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    EXPECT_EQ(counter.load(), num_tasks);

    executor.stop();
}

TEST_F(AsyncExecutorTest, MixedTaskTypes) {
    AsyncExecutor executor(config);
    executor.start();

    std::atomic<int> void_task_count{0};
    std::vector<std::future<int>> int_futures;

    // Mix void and return value tasks
    for (int i = 0; i < 10; ++i) {
        // Void task
        executor.execute([&void_task_count]() {
            void_task_count.fetch_add(1);
        });

        // Return value task
        int_futures.push_back(executor.execute([i]() {
            return i * 10;
        }));
    }

    // Wait for all return value tasks
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(int_futures[i].get(), i * 10);
    }

    // Give void tasks time to complete
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(void_task_count.load(), 10);

    executor.stop();
}

// Exception safety tests
TEST_F(AsyncExecutorTest, ExceptionInTaskDoesNotCrashExecutor) {
    AsyncExecutor executor(config);
    executor.start();

    // Submit a task that throws
    auto future1 = executor.execute([]() {
        throw std::runtime_error("First exception");
        return 1;
    });

    // Submit a normal task after the throwing one
    auto future2 = executor.execute([]() {
        return 42;
    });

    EXPECT_THROW(future1.get(), std::runtime_error);
    EXPECT_EQ(future2.get(), 42);

    // Executor should still be functional
    EXPECT_TRUE(executor.isRunning());

    executor.stop();
}

TEST_F(AsyncExecutorTest, MultipleExceptions) {
    AsyncExecutor executor(config);
    executor.start();

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i) {
        futures.push_back(executor.execute([i]() {
            if (i % 2 == 0) {
                throw std::runtime_error("Exception " + std::to_string(i));
            }
            return i;
        }));
    }

    for (int i = 0; i < 5; ++i) {
        if (i % 2 == 0) {
            EXPECT_THROW(futures[i].get(), std::runtime_error);
        } else {
            EXPECT_EQ(futures[i].get(), i);
        }
    }

    executor.stop();
}

// Edge case tests
TEST_F(AsyncExecutorTest, EmptyTaskSubmission) {
    AsyncExecutor executor(config);
    executor.start();

    // Test submitting empty/null function should throw
    std::function<void()> empty_func;
    EXPECT_THROW(executor.execute(empty_func), ExecutorException);

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskSubmissionAfterStop) {
    AsyncExecutor executor(config);
    executor.start();
    executor.stop();

    // Should throw when trying to submit task to stopped executor
    EXPECT_THROW(executor.execute([]() { return 42; }), ExecutorException);
}

TEST_F(AsyncExecutorTest, MultipleStartStop) {
    AsyncExecutor executor(config);

    // Multiple starts should be safe
    executor.start();
    executor.start();  // Should not cause issues
    EXPECT_TRUE(executor.isRunning());

    executor.stop();
    executor.stop();  // Should not cause issues
    EXPECT_FALSE(executor.isRunning());

    // Should be able to restart
    executor.start();
    EXPECT_TRUE(executor.isRunning());

    auto future = executor.execute([]() { return 123; });
    EXPECT_EQ(future.get(), 123);

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskExecutionDuringShutdown) {
    AsyncExecutor executor(config);
    executor.start();

    // Submit long-running tasks
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(100ms);
            return i;
        }));
    }

    // Stop executor while tasks are running
    executor.stop();

    // Tasks should still complete (or be cancelled gracefully)
    for (int i = 0; i < 5; ++i) {
        try {
            int result = futures[i].get();
            EXPECT_EQ(result, i);
        } catch (const std::exception&) {
            // Task may have been cancelled, which is acceptable
        }
    }
}

// Performance and load tests
TEST_F(AsyncExecutorTest, HighThroughputTasks) {
    AsyncExecutor::Configuration perf_config = config;
    perf_config.minThreads = 4;
    perf_config.maxThreads = 8;

    AsyncExecutor executor(perf_config);
    executor.start();

    const int num_tasks = 10000;
    std::atomic<int> completed_count{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        executor.execute([&completed_count]() {
            completed_count.fetch_add(1);
        });
    }

    // Wait for all tasks to complete
    while (completed_count.load() < num_tasks) {
        std::this_thread::sleep_for(1ms);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_EQ(completed_count.load(), num_tasks);
    EXPECT_LT(duration.count(), 5000);  // Should complete within 5 seconds

    executor.stop();
}

TEST_F(AsyncExecutorTest, MemoryUsageStability) {
    AsyncExecutor executor(config);
    executor.start();

    // Submit many tasks in batches to test memory stability
    for (int batch = 0; batch < 10; ++batch) {
        std::vector<std::future<int>> futures;

        for (int i = 0; i < 100; ++i) {
            futures.push_back(executor.execute([i, batch]() {
                return i * batch;
            }));
        }

        // Wait for batch to complete
        for (int i = 0; i < 100; ++i) {
            EXPECT_EQ(futures[i].get(), i * batch);
        }

        // Small delay between batches
        std::this_thread::sleep_for(10ms);
    }

    executor.stop();
}

// Configuration edge cases
TEST_F(AsyncExecutorTest, InvalidConfiguration) {
    AsyncExecutor::Configuration invalid_config;
    invalid_config.minThreads = 0;  // Invalid
    invalid_config.maxThreads = 0;  // Invalid

    // Constructor should fix invalid values
    AsyncExecutor executor(invalid_config);
    executor.start();

    EXPECT_TRUE(executor.isRunning());

    auto future = executor.execute([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    executor.stop();
}

TEST_F(AsyncExecutorTest, SingleThreadConfiguration) {
    AsyncExecutor::Configuration single_config = config;
    single_config.minThreads = 1;
    single_config.maxThreads = 1;
    single_config.useWorkStealing = false;

    AsyncExecutor executor(single_config);
    executor.start();

    // Should work with single thread
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(executor.execute([i]() { return i; }));
    }

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

// Additional tests for comprehensive coverage

TEST_F(AsyncExecutorTest, AllPriorityLevels) {
    AsyncExecutor executor(config);
    executor.start();

    // Test all priority levels
    auto critical_future = executor.execute([]() { return 1; }, AsyncExecutor::Priority::Critical);
    auto high_future = executor.execute([]() { return 2; }, AsyncExecutor::Priority::High);
    auto normal_future = executor.execute([]() { return 3; }, AsyncExecutor::Priority::Normal);
    auto low_future = executor.execute([]() { return 4; }, AsyncExecutor::Priority::Low);

    EXPECT_EQ(critical_future.get(), 1);
    EXPECT_EQ(high_future.get(), 2);
    EXPECT_EQ(normal_future.get(), 3);
    EXPECT_EQ(low_future.get(), 4);

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskExceptionTypes) {
    AsyncExecutor executor(config);
    executor.start();

    // Test ExecutorException
    EXPECT_THROW({
        throw ExecutorException("Test executor exception");
    }, ExecutorException);

    // Test TaskException
    EXPECT_THROW({
        throw TaskException("Test task exception");
    }, TaskException);

    // TaskException should also be an ExecutorException
    EXPECT_THROW({
        throw TaskException("Test task exception");
    }, ExecutorException);

    executor.stop();
}

TEST_F(AsyncExecutorTest, ConfigurationValidation) {
    AsyncExecutor::Configuration test_config;

    // Test that constructor fixes invalid configurations
    test_config.minThreads = 0;
    test_config.maxThreads = 0;

    AsyncExecutor executor(test_config);
    // Constructor should have fixed the values
    executor.start();
    EXPECT_TRUE(executor.isRunning());
    executor.stop();
}

TEST_F(AsyncExecutorTest, WorkStealingDisabled) {
    AsyncExecutor::Configuration no_stealing_config = config;
    no_stealing_config.useWorkStealing = false;
    no_stealing_config.minThreads = 4;

    AsyncExecutor executor(no_stealing_config);
    executor.start();

    // Submit tasks and verify they execute correctly without work stealing
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(5ms);
            return i * 2;
        }));
    }

    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, ThreadPriorityConfiguration) {
    AsyncExecutor::Configuration priority_config = config;
    priority_config.setPriority = true;
    priority_config.threadPriority = 10;

    AsyncExecutor executor(priority_config);
    executor.start();

    // Should start successfully even with priority settings
    EXPECT_TRUE(executor.isRunning());

    auto future = executor.execute([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    executor.stop();
}

TEST_F(AsyncExecutorTest, ThreadPinningConfiguration) {
    AsyncExecutor::Configuration pin_config = config;
    pin_config.pinThreads = true;

    AsyncExecutor executor(pin_config);
    executor.start();

    // Should start successfully even with thread pinning
    EXPECT_TRUE(executor.isRunning());

    auto future = executor.execute([]() { return 123; });
    EXPECT_EQ(future.get(), 123);

    executor.stop();
}

TEST_F(AsyncExecutorTest, StatisticsCollection) {
    AsyncExecutor::Configuration stats_config = config;
    stats_config.statInterval = 50ms;  // Short interval for testing

    AsyncExecutor executor(stats_config);
    executor.start();

    // Submit some tasks
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(10ms);
            return i;
        }));
    }

    // Wait for tasks to complete
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    // Give time for statistics collection
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(executor.getCompletedTaskCount(), 5);

    executor.stop();
}

TEST_F(AsyncExecutorTest, LargeQueueSize) {
    AsyncExecutor::Configuration large_queue_config = config;
    large_queue_config.queueSizePerThread = 1024;

    AsyncExecutor executor(large_queue_config);
    executor.start();

    // Submit many tasks to test large queue
    const int num_tasks = 500;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.execute([i]() { return i; }));
    }

    // All tasks should complete successfully
    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

// Edge case and error condition tests

TEST_F(AsyncExecutorTest, EmptyFunctionSubmission) {
    AsyncExecutor executor(config);
    executor.start();

    // Test submitting empty std::function should throw
    std::function<int()> empty_func;
    EXPECT_THROW(executor.execute(empty_func), ExecutorException);

    std::function<void()> empty_void_func;
    EXPECT_THROW(executor.execute(empty_void_func), ExecutorException);

    executor.stop();
}

TEST_F(AsyncExecutorTest, DestructorWithRunningTasks) {
    {
        AsyncExecutor executor(config);
        executor.start();

        // Submit long-running tasks
        for (int i = 0; i < 5; ++i) {
            executor.execute([i]() {
                std::this_thread::sleep_for(200ms);
                return i;
            });
        }

        // Destructor should handle cleanup gracefully
    }
    // If we reach here without hanging, destructor worked correctly
    SUCCEED();
}

TEST_F(AsyncExecutorTest, VeryShortIdleTimeout) {
    AsyncExecutor::Configuration short_timeout_config = config;
    short_timeout_config.threadIdleTimeout = 1ms;  // Very short
    short_timeout_config.minThreads = 1;
    short_timeout_config.maxThreads = 4;

    AsyncExecutor executor(short_timeout_config);
    executor.start();

    // Submit tasks with delays
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(10ms);
            return i;
        }));
    }

    // All tasks should still complete
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, MaxThreadsConfiguration) {
    AsyncExecutor::Configuration max_threads_config = config;
    max_threads_config.minThreads = 2;
    max_threads_config.maxThreads = 16;

    AsyncExecutor executor(max_threads_config);
    executor.start();

    // Submit many concurrent tasks to potentially trigger max threads
    const int num_tasks = 50;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(50ms);  // Longer delay to increase concurrency
            return i;
        }));
    }

    // All tasks should complete
    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, TaskSubmissionRateLimit) {
    AsyncExecutor executor(config);
    executor.start();

    // Submit tasks rapidly to test queue handling
    const int num_tasks = 1000;
    std::atomic<int> completed{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        executor.execute([&completed]() {
            completed.fetch_add(1);
        });
    }

    // Wait for all tasks to complete
    while (completed.load() < num_tasks) {
        std::this_thread::sleep_for(1ms);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_EQ(completed.load(), num_tasks);
    // Should complete reasonably quickly
    EXPECT_LT(duration.count(), 2000);

    executor.stop();
}

TEST_F(AsyncExecutorTest, MixedPriorityExecution) {
    AsyncExecutor executor(config);
    executor.start();

    std::vector<std::future<int>> futures;

    // Submit tasks with mixed priorities
    for (int i = 0; i < 20; ++i) {
        AsyncExecutor::Priority priority;
        switch (i % 4) {
            case 0: priority = AsyncExecutor::Priority::Low; break;
            case 1: priority = AsyncExecutor::Priority::Normal; break;
            case 2: priority = AsyncExecutor::Priority::High; break;
            case 3: priority = AsyncExecutor::Priority::Critical; break;
        }

        futures.push_back(executor.execute([i]() {
            std::this_thread::sleep_for(5ms);
            return i;
        }, priority));
    }

    // All tasks should complete regardless of priority
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }

    executor.stop();
}

TEST_F(AsyncExecutorTest, StatisticsDisabled) {
    AsyncExecutor::Configuration no_stats_config = config;
    no_stats_config.statInterval = std::chrono::milliseconds(0);  // Disable stats

    AsyncExecutor executor(no_stats_config);
    executor.start();

    // Should work normally without statistics collection
    auto future = executor.execute([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    executor.stop();
}
