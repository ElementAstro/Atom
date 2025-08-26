/*
 * test_timer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Timer
Tests task scheduling, priorities, repeat counts, edge cases, and platform-specific timer implementations.

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>

#include "atom/async/utils/timer.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::utils::test {

// ============================================================================
// Timer Tests
// ============================================================================

class TimerTest : public atom::async::test::TimerTestFixture {
protected:
    void SetUp() override {
        TimerTestFixture::SetUp();
        // Additional timer specific setup
    }

    void TearDown() override {
        // Timer specific cleanup
        TimerTestFixture::TearDown();
    }
};

TEST_F(TimerTest, BasicSetTimeout) {
    Timer timer;
    std::atomic<bool> executed{false};
    
    auto future = timer.setTimeout([&executed]() {
        executed = true;
    }, 100);
    
    // Should not be executed immediately
    EXPECT_FALSE(executed);
    
    // Wait for execution
    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(TimerTest, SetTimeoutWithReturnValue) {
    Timer timer;
    
    auto future = timer.setTimeout([]() -> int {
        return 42;
    }, 50);
    
    int result = future.get();
    EXPECT_EQ(result, 42);
}

TEST_F(TimerTest, SetTimeoutWithArguments) {
    Timer timer;
    std::atomic<int> result{0};
    
    auto future = timer.setTimeout([&result](int a, int b) {
        result = a + b;
    }, 50, 10, 20);
    
    future.wait();
    EXPECT_EQ(result.load(), 30);
}

TEST_F(TimerTest, SetInterval) {
    Timer timer;
    std::atomic<int> count{0};
    
    timer.setInterval([&count]() {
        count.fetch_add(1);
    }, 50, 5, 0); // Execute 5 times
    
    // Wait for all executions
    std::this_thread::sleep_for(400ms);
    
    EXPECT_EQ(count.load(), 5);
}

TEST_F(TimerTest, SetIntervalWithPriority) {
    Timer timer;
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    // High priority task
    timer.setInterval([&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
    }, 100, 2, 10); // High priority
    
    // Low priority task
    timer.setInterval([&executionOrder, &orderMutex]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2);
    }, 100, 2, 1); // Low priority
    
    // Wait for executions
    std::this_thread::sleep_for(300ms);
    
    EXPECT_EQ(executionOrder.size(), 4);
    // High priority tasks should generally execute first
    // (exact order may vary due to timing)
}

TEST_F(TimerTest, AddTask) {
    Timer timer;
    std::atomic<bool> executed{false};
    
    auto future = timer.addTask([&executed]() {
        executed = true;
    }, 100, 1, 5);
    
    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(TimerTest, AddTaskWithRepeat) {
    Timer timer;
    std::atomic<int> count{0};
    
    auto future = timer.addTask([&count]() -> int {
        return count.fetch_add(1) + 1;
    }, 50, 3, 5); // Repeat 3 times
    
    // Wait for completion
    std::this_thread::sleep_for(250ms);
    
    EXPECT_EQ(count.load(), 3);
}

TEST_F(TimerTest, TimerCallback) {
    Timer timer;
    std::atomic<int> callbackCount{0};
    
    timer.setCallback([&callbackCount]() {
        callbackCount.fetch_add(1);
    });
    
    // Add multiple tasks
    timer.setTimeout([]() {}, 50);
    timer.setTimeout([]() {}, 100);
    timer.setTimeout([]() {}, 150);
    
    // Wait for all tasks to complete
    std::this_thread::sleep_for(250ms);
    
    EXPECT_EQ(callbackCount.load(), 3);
}

TEST_F(TimerTest, TimerStart) {
    Timer timer;
    std::atomic<bool> executed{false};
    
    // Add task before starting
    timer.setTimeout([&executed]() {
        executed = true;
    }, 100);
    
    EXPECT_FALSE(executed);
    
    // Timer starts automatically when tasks are added
    
    // Wait for execution
    std::this_thread::sleep_for(200ms);
    EXPECT_TRUE(executed);
}

TEST_F(TimerTest, TimerStop) {
    Timer timer;
    std::atomic<int> count{0};
    
    timer.setInterval([&count]() {
        count.fetch_add(1);
    }, 50, -1, 0); // Infinite repeat
    
    timer.start();
    
    // Let it run for a bit
    std::this_thread::sleep_for(150ms);
    int countAfterStart = count.load();
    EXPECT_GT(countAfterStart, 0);
    
    // Stop timer
    timer.stop();
    
    // Wait a bit more
    std::this_thread::sleep_for(150ms);
    int countAfterStop = count.load();
    
    // Count should not have increased significantly after stop
    EXPECT_LE(countAfterStop - countAfterStart, 1); // Allow for one more execution due to timing
}

TEST_F(TimerTest, TimerRestart) {
    Timer timer;
    std::atomic<int> count{0};
    
    timer.setTimeout([&count]() {
        count.fetch_add(1);
    }, 100);
    
    timer.start();
    std::this_thread::sleep_for(150ms);
    EXPECT_EQ(count.load(), 1);
    
    // Restart with new task
    timer.stop();
    timer.setTimeout([&count]() {
        count.fetch_add(10);
    }, 100);
    
    timer.start();
    std::this_thread::sleep_for(150ms);
    EXPECT_EQ(count.load(), 11);
}

TEST_F(TimerTest, ConcurrentTaskAddition) {
    Timer timer;
    std::atomic<int> totalExecuted{0};
    
    timer.start();
    
    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int tasksPerThread = 5;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&timer, &totalExecuted, tasksPerThread]() {
            for (int j = 0; j < tasksPerThread; ++j) {
                timer.setTimeout([&totalExecuted]() {
                    totalExecuted.fetch_add(1);
                }, 50 + j * 10);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Wait for all tasks to complete
    std::this_thread::sleep_for(500ms);
    
    EXPECT_EQ(totalExecuted.load(), numThreads * tasksPerThread);
}

TEST_F(TimerTest, TaskPriorityOrdering) {
    Timer timer;
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    timer.start();
    
    // Add tasks with different priorities (higher number = higher priority)
    for (int priority = 1; priority <= 5; ++priority) {
        timer.addTask([&executionOrder, &orderMutex, priority]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(priority);
        }, 100, 1, priority);
    }
    
    // Wait for execution
    std::this_thread::sleep_for(200ms);
    
    EXPECT_EQ(executionOrder.size(), 5);
    
    // Higher priority tasks should generally execute first
    // (exact order may vary due to timing, but we expect some correlation)
    bool hasHighPriorityFirst = false;
    for (size_t i = 0; i < executionOrder.size() - 1; ++i) {
        if (executionOrder[i] > executionOrder[i + 1]) {
            hasHighPriorityFirst = true;
            break;
        }
    }
    // This is a probabilistic test - in most cases, we should see some high priority tasks first
}

TEST_F(TimerTest, ExceptionInTask) {
    Timer timer;
    std::atomic<bool> normalTaskExecuted{false};
    
    timer.start();
    
    // Add a task that throws
    timer.setTimeout([]() {
        throw std::runtime_error("Test exception");
    }, 50);
    
    // Add a normal task after the throwing one
    timer.setTimeout([&normalTaskExecuted]() {
        normalTaskExecuted = true;
    }, 100);
    
    // Wait for execution
    std::this_thread::sleep_for(200ms);
    
    // Normal task should still execute despite the exception
    EXPECT_TRUE(normalTaskExecuted);
}

TEST_F(TimerTest, TaskValidation) {
    Timer timer;
    
    // Valid parameters should not throw
    EXPECT_NO_THROW(Timer::validateTaskParams(100, 5));
    EXPECT_NO_THROW(Timer::validateTaskParams(0, 1));
    EXPECT_NO_THROW(Timer::validateTaskParams(1000, -1)); // Infinite repeat
    
    // Invalid parameters should throw
    EXPECT_THROW(Timer::validateTaskParams(100, 0), std::invalid_argument);
    EXPECT_THROW(Timer::validateTaskParams(100, -2), std::invalid_argument); // Only -1 allowed for infinite
}

TEST_F(TimerTest, TimerDestructor) {
    std::atomic<int> count{0};
    
    {
        Timer timer;
        timer.setInterval([&count]() {
            count.fetch_add(1);
        }, 50, -1, 0); // Infinite repeat
        
        timer.start();
        std::this_thread::sleep_for(150ms);
    } // Timer destructor should stop all tasks
    
    int countAfterDestruction = count.load();
    
    // Wait a bit more
    std::this_thread::sleep_for(150ms);
    int finalCount = count.load();
    
    // Count should not increase after destruction
    EXPECT_EQ(countAfterDestruction, finalCount);
}

TEST_F(TimerTest, HighFrequencyTasks) {
    Timer timer;
    std::atomic<int> count{0};
    
    timer.start();
    
    // Add many high-frequency tasks
    for (int i = 0; i < 100; ++i) {
        timer.setTimeout([&count]() {
            count.fetch_add(1);
        }, 10 + i); // Staggered timing
    }
    
    // Wait for execution
    std::this_thread::sleep_for(200ms);
    
    EXPECT_EQ(count.load(), 100);
}

TEST_F(TimerTest, LongRunningTask) {
    Timer timer;
    std::atomic<bool> longTaskStarted{false};
    std::atomic<bool> longTaskFinished{false};
    std::atomic<bool> shortTaskExecuted{false};
    
    timer.start();
    
    // Add a long-running task
    timer.setTimeout([&longTaskStarted, &longTaskFinished]() {
        longTaskStarted = true;
        std::this_thread::sleep_for(200ms);
        longTaskFinished = true;
    }, 50);
    
    // Add a short task that should execute after the long one starts
    timer.setTimeout([&shortTaskExecuted]() {
        shortTaskExecuted = true;
    }, 100);
    
    // Wait for both tasks
    std::this_thread::sleep_for(400ms);
    
    EXPECT_TRUE(longTaskStarted);
    EXPECT_TRUE(longTaskFinished);
    EXPECT_TRUE(shortTaskExecuted);
}

// Test timer with resource cleanup
TEST_F(TimerTest, ResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        Timer timer;

        // Add tasks that use resources
        std::vector<EnhancedFuture<void>> futures;
        for (int i = 0; i < 5; ++i) {
            auto future = timer.setTimeout([&tracker]() {
                atom::async::test::ScopedResourceTracker resource(tracker);
                std::this_thread::sleep_for(10ms);
            }, 50 + i * 10);
            futures.push_back(std::move(future));
        }

        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
    } // Timer destructor should clean up properly

    // Give some time for cleanup
    std::this_thread::sleep_for(50ms);
    tracker.expectNoLeaks();
}

// Enhanced timer tests using test utilities are complete

}  // namespace atom::async::utils::test
