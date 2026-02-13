/*
 * test_thread_wrapper.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Thread Wrapper
Tests thread lifecycle, exception handling, stop tokens, and platform-specific
behavior.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/threading/thread_wrapper.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::threading::test {

// ============================================================================
// Thread Wrapper Tests
// ============================================================================

class ThreadWrapperTest : public atom::async::test::ThreadingTestFixture {
protected:
    void SetUp() override {
        ThreadingTestFixture::SetUp();
        // Additional thread wrapper specific setup
    }

    void TearDown() override {
        // Thread wrapper specific cleanup
        ThreadingTestFixture::TearDown();
    }
};

TEST_F(ThreadWrapperTest, BasicStartAndJoin) {
    Thread thread;
    std::atomic<bool> executed{false};

    thread.start([&executed] { executed = true; });

    EXPECT_TRUE(thread.running());
    thread.join();
    EXPECT_FALSE(thread.running());
    EXPECT_TRUE(executed);
}

TEST_F(ThreadWrapperTest, StartWithArguments) {
    Thread thread;
    std::atomic<int> result{0};

    thread.start([&result](int a, int b) { result = a + b; }, 5, 10);

    thread.join();
    EXPECT_EQ(result.load(), 15);
}

TEST_F(ThreadWrapperTest, StartWithStopToken) {
    Thread thread;
    std::atomic<bool> stopRequested{false};

    thread.start([&stopRequested](std::stop_token token) {
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(10ms);
        }
        stopRequested = true;
    });

    EXPECT_TRUE(thread.running());

    // Request stop and wait
    thread.requestStop();
    thread.join();

    EXPECT_TRUE(stopRequested);
    EXPECT_FALSE(thread.running());
}

TEST_F(ThreadWrapperTest, ExceptionHandling) {
    Thread thread;
    const std::string errorMessage = "Test exception";

    thread.start([&errorMessage] { throw std::runtime_error(errorMessage); });

    EXPECT_THROW(thread.join(), std::runtime_error);
}

TEST_F(ThreadWrapperTest, ThreadNaming) {
    Thread thread;
    const std::string threadName = "TestThread";

    thread.setThreadName(threadName);
    EXPECT_EQ(thread.getThreadName(), threadName);

    std::atomic<bool> executed{false};
    thread.start([&executed] { executed = true; });

    thread.join();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadWrapperTest, ThreadTimeout) {
    Thread thread;
    std::atomic<bool> timedOut{false};

    thread.start([&timedOut] {
        std::this_thread::sleep_for(200ms);
        timedOut = true;
    });

    // Set a short timeout
    bool joinedInTime = thread.joinFor(50ms);

    EXPECT_FALSE(joinedInTime);  // Should timeout
    EXPECT_TRUE(thread.running());

    // Wait for actual completion
    thread.join();
    EXPECT_TRUE(timedOut);
}

// Test optimized tryJoinFor with condition variable
TEST_F(ThreadWrapperTest, OptimizedTryJoinFor) {
    Thread thread;
    std::atomic<bool> completed{false};

    // Test immediate join when thread completes quickly
    thread.start([&completed] {
        std::this_thread::sleep_for(20ms);
        completed = true;
    });

    auto start = std::chrono::steady_clock::now();
    bool joined = thread.tryJoinFor(500ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(joined);
    EXPECT_TRUE(completed);
    // Should complete well before timeout
    EXPECT_LT(elapsed, 200ms);
}

// Test tryJoinFor timeout efficiency
TEST_F(ThreadWrapperTest, TryJoinForTimeoutEfficiency) {
    Thread thread;

    thread.start([] { std::this_thread::sleep_for(500ms); });

    auto start = std::chrono::steady_clock::now();
    bool joined = thread.tryJoinFor(50ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(joined);
    // Should timeout close to requested duration (not spin-waiting excessively)
    EXPECT_GE(elapsed, 45ms);
    EXPECT_LT(elapsed, 150ms);

    // Clean up
    thread.join();
}

TEST_F(ThreadWrapperTest, ThreadPriority) {
    Thread thread;

    // Test setting different priorities
    thread.setPriority(Thread::Priority::LOW);
    EXPECT_EQ(thread.getPriority(), Thread::Priority::LOW);

    thread.setPriority(Thread::Priority::HIGH);
    EXPECT_EQ(thread.getPriority(), Thread::Priority::HIGH);

    std::atomic<bool> executed{false};
    thread.start([&executed] { executed = true; });

    thread.join();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadWrapperTest, CPUAffinity) {
    Thread thread;

    // Test setting CPU affinity
    thread.setPreferredCPU(0);
    EXPECT_EQ(thread.getPreferredCPU(), 0);

    std::atomic<bool> executed{false};
    thread.start([&executed] { executed = true; });

    thread.join();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadWrapperTest, MultipleStartCalls) {
    Thread thread;
    std::atomic<int> counter{0};

    // First start
    thread.start([&counter] { counter++; });
    thread.join();
    EXPECT_EQ(counter.load(), 1);

    // Second start - should work
    thread.start([&counter] { counter++; });
    thread.join();
    EXPECT_EQ(counter.load(), 2);
}

TEST_F(ThreadWrapperTest, ThreadId) {
    Thread thread;
    std::atomic<std::thread::id> threadId{};

    thread.start([&threadId] { threadId = std::this_thread::get_id(); });

    thread.join();

    EXPECT_NE(threadId.load(), std::thread::id{});
    EXPECT_EQ(thread.getId(), threadId.load());
}

TEST_F(ThreadWrapperTest, Detach) {
    Thread thread;
    std::atomic<bool> executed{false};

    thread.start([&executed] {
        std::this_thread::sleep_for(50ms);
        executed = true;
    });

    thread.detach();
    EXPECT_FALSE(
        thread.running());  // After detach, running() should return false

    // Wait for execution to complete
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(executed);
}

// Removed JoinableCheck test as Thread class doesn't expose joinable() method

TEST_F(ThreadWrapperTest, StopTokenWithTimeout) {
    Thread thread;
    std::atomic<bool> completed{false};

    thread.start([&completed](std::stop_token token) {
        auto start = std::chrono::steady_clock::now();
        while (!token.stop_requested() &&
               std::chrono::steady_clock::now() - start < 200ms) {
            std::this_thread::sleep_for(10ms);
        }
        completed = true;
    });

    std::this_thread::sleep_for(50ms);
    thread.requestStop();
    thread.join();

    EXPECT_TRUE(completed);
}

TEST_F(ThreadWrapperTest, ExceptionInStopTokenFunction) {
    Thread thread;
    const std::string errorMessage = "Stop token exception";

    thread.start([&errorMessage](std::stop_token /*token*/) {
        std::this_thread::sleep_for(10ms);
        throw std::runtime_error(errorMessage);
    });

    EXPECT_THROW(thread.join(), std::runtime_error);
}

TEST_F(ThreadWrapperTest, ConcurrentOperations) {
    const int numThreads = 10;
    std::vector<std::unique_ptr<Thread>> threads;
    std::atomic<int> counter{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<Thread>());
        threads.back()->start([&counter] {
            for (int j = 0; j < 100; ++j) {
                counter.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread->join();
    }

    EXPECT_EQ(counter.load(), numThreads * 100);
}

// Test thread pool-like behavior
TEST_F(ThreadWrapperTest, ThreadPoolBehavior) {
    const size_t poolSize = getMaxThreads();
    std::vector<std::unique_ptr<Thread>> threadPool;
    std::atomic<int> completedTasks{0};

    // Create thread pool
    for (size_t i = 0; i < poolSize; ++i) {
        threadPool.push_back(std::make_unique<Thread>());
    }

    // Submit tasks to pool
    const int numTasks = poolSize * 3;
    for (int taskId = 0; taskId < numTasks; ++taskId) {
        size_t threadIndex = taskId % poolSize;

        // Wait for previous task on this thread to complete
        if (taskId >= static_cast<int>(poolSize)) {
            threadPool[threadIndex]->join();
        }

        threadPool[threadIndex]->start([&completedTasks, taskId] {
            // Simulate work
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10 + taskId % 20));
            completedTasks.fetch_add(1);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threadPool) {
        thread->join();
    }

    EXPECT_EQ(completedTasks.load(), numTasks);
}

// Test thread exception propagation with custom exception types
TEST_F(ThreadWrapperTest, CustomExceptionPropagation) {
    class CustomException : public std::exception {
    public:
        const char* what() const noexcept override {
            return "Custom thread exception";
        }
    };

    Thread thread;
    thread.start([]() { throw CustomException(); });

    EXPECT_THROW(thread.join(), CustomException);
}

// Test thread with complex return type
TEST_F(ThreadWrapperTest, ComplexReturnType) {
    struct ComplexResult {
        int value;
        std::string message;
        std::vector<int> data;

        ComplexResult(int v, std::string m, std::vector<int> d)
            : value(v), message(std::move(m)), data(std::move(d)) {}
    };

    Thread thread;
    std::promise<ComplexResult> resultPromise;
    auto resultFuture = resultPromise.get_future();

    thread.start([&resultPromise]() {
        ComplexResult result(42, "test message", {1, 2, 3, 4, 5});
        resultPromise.set_value(std::move(result));
    });

    thread.join();

    auto result = resultFuture.get();
    EXPECT_EQ(result.value, 42);
    EXPECT_EQ(result.message, "test message");
    EXPECT_EQ(result.data.size(), 5);
    EXPECT_EQ(result.data[0], 1);
    EXPECT_EQ(result.data[4], 5);
}

// Test thread resource cleanup
TEST_F(ThreadWrapperTest, ResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        Thread thread;
        thread.start([&tracker]() {
            atom::async::test::ScopedResourceTracker resource(tracker);
            std::this_thread::sleep_for(50ms);
        });

        thread.join();
    }  // Thread destructor should not affect resource cleanup

    // Resource should be properly cleaned up
    tracker.expectNoLeaks();
}

// Test thread interruption patterns
TEST_F(ThreadWrapperTest, InterruptionPatterns) {
    Thread thread;
    std::atomic<bool> wasInterrupted{false};
    std::atomic<bool> cleanupCompleted{false};

    thread.start([&wasInterrupted, &cleanupCompleted](std::stop_token token) {
        try {
            while (!token.stop_requested()) {
                std::this_thread::sleep_for(10ms);
            }
            wasInterrupted = true;

            // Simulate cleanup work
            std::this_thread::sleep_for(20ms);
            cleanupCompleted = true;
        } catch (...) {
            // Handle any exceptions during cleanup
        }
    });

    // Let thread run for a bit
    std::this_thread::sleep_for(50ms);

    // Request stop
    thread.requestStop();
    thread.join();

    EXPECT_TRUE(wasInterrupted);
    EXPECT_TRUE(cleanupCompleted);
}

// Test thread performance characteristics
TEST_F(ThreadWrapperTest, PerformanceCharacteristics) {
    const int numIterations = 1000;
    auto timer = createTimer();

    for (int i = 0; i < numIterations; ++i) {
        Thread thread;
        thread.start([]() {
            // Minimal work
            volatile int dummy = 42;
            (void)dummy;
        });
        thread.join();
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable (this is a rough check)
    // Creating and joining 1000 threads should take less than 10 seconds
    EXPECT_LT(elapsed.count(), 10000000);  // 10 seconds in microseconds

    std::cout << "Thread creation/join performance: "
              << elapsed.count() / numIterations << " microseconds per thread"
              << std::endl;
}

// Test thread with move-only types
TEST_F(ThreadWrapperTest, MoveOnlyTypes) {
    Thread thread;
    std::atomic<bool> executed{false};

    auto uniquePtr = std::make_unique<int>(42);

    thread.start([&executed, ptr = std::move(uniquePtr)]() {
        EXPECT_EQ(*ptr, 42);
        executed = true;
    });

    thread.join();
    EXPECT_TRUE(executed);
}

// Test thread state transitions
TEST_F(ThreadWrapperTest, StateTransitions) {
    Thread thread;

    // Initial state
    EXPECT_FALSE(thread.running());

    // After start
    thread.start([]() { std::this_thread::sleep_for(50ms); });

    EXPECT_TRUE(thread.running());

    // After join
    thread.join();

    EXPECT_FALSE(thread.running());
}

// Test thread with lambda capture variations
TEST_F(ThreadWrapperTest, LambdaCaptureVariations) {
    Thread thread;

    int localVar = 10;
    std::string localString = "test";
    std::atomic<bool> executed{false};

    thread.start(
        [localVar, localString = std::move(localString), &executed]() mutable {
            localVar += 5;
            localString += "_modified";

            EXPECT_EQ(localVar, 15);
            EXPECT_EQ(localString, "test_modified");
            executed = true;
        });

    thread.join();
    EXPECT_TRUE(executed);
    EXPECT_EQ(localVar, 10);  // Original should be unchanged
}

}  // namespace atom::async::threading::test
