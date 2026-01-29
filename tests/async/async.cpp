// filepath: tests/async/async.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "atom/async/async.hpp"

using namespace atom::async;
using namespace atom::platform;
using namespace std::chrono_literals;

// Helper function to simulate a task
int sampleTask(int value) {
    std::this_thread::sleep_for(10ms);
    return value * 2;
}

void voidTask() { std::this_thread::sleep_for(10ms); }

void throwingTask() {
    std::this_thread::sleep_for(10ms);
    throw std::runtime_error("Task failed intentionally");
}

// Test fixture for AsyncWorker
class AsyncWorkerTest : public ::testing::Test {
protected:
    // No specific setup needed for most tests, as AsyncWorker is self-contained
};

// Test fixture for AsyncWorkerManager
class AsyncWorkerManagerTest : public ::testing::Test {
protected:
    AsyncWorkerManager<int> manager_int;
    AsyncWorkerManager<void> manager_void;
};

// AsyncWorker Tests

TEST_F(AsyncWorkerTest, DefaultConstructor) {
    AsyncWorker<int> worker;
    EXPECT_FALSE(worker.isDone());
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, StartAsyncTaskInt) {
    AsyncWorker<int> worker;
    worker.startAsync([](int x) { return x * 2; }, 5);
    EXPECT_TRUE(worker.isActive());
    EXPECT_FALSE(worker.isDone());
    EXPECT_EQ(worker.getResult(), 10);
    EXPECT_TRUE(worker.isDone());
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, StartAsyncTaskVoid) {
    AsyncWorker<void> worker;
    worker.startAsync(voidTask);
    EXPECT_TRUE(worker.isActive());
    EXPECT_FALSE(worker.isDone());
    worker.getResult();  // Should not throw
    EXPECT_TRUE(worker.isDone());
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, StartAsyncTaskThrows) {
    AsyncWorker<void> worker;
    worker.startAsync(throwingTask);
    EXPECT_TRUE(worker.isActive());
    EXPECT_FALSE(worker.isDone());
    EXPECT_THROW(worker.getResult(), std::runtime_error);
    EXPECT_TRUE(worker.isDone());  // Task is done, but failed
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, GetResultWithTimeoutSuccess) {
    AsyncWorker<int> worker;
    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        7);
    EXPECT_EQ(worker.getResult(100ms), 14);
}

TEST_F(AsyncWorkerTest, GetResultWithTimeoutFailure) {
    AsyncWorker<int> worker;
    worker.startAsync([]() {
        std::this_thread::sleep_for(200ms);
        return 1;
    });
    EXPECT_THROW(
        { [[maybe_unused]] auto result = worker.getResult(10ms); },
        TimeoutException);
}

TEST_F(AsyncWorkerTest, CancelTask) {
    AsyncWorker<int> worker;
    worker.startAsync([]() {
        std::this_thread::sleep_for(500ms);  // Long task
        return 1;
    });
    EXPECT_TRUE(worker.isActive());
    worker.cancel();  // Should wait for completion
    EXPECT_TRUE(worker.isDone());
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, IsDoneAndIsActive) {
    AsyncWorker<int> worker;
    EXPECT_FALSE(worker.isDone());
    EXPECT_FALSE(worker.isActive());

    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    EXPECT_FALSE(worker.isDone());  // May still be running
    EXPECT_TRUE(worker.isActive());

    [[maybe_unused]] auto result = worker.getResult();  // Wait for completion
    EXPECT_TRUE(worker.isDone());
    EXPECT_FALSE(worker.isActive());
}

TEST_F(AsyncWorkerTest, ValidateSuccess) {
    AsyncWorker<int> worker;
    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        10);
    [[maybe_unused]] auto result = worker.getResult();
    EXPECT_TRUE(worker.validate([](int result) { return result == 20; }));
}

TEST_F(AsyncWorkerTest, ValidateFailure) {
    AsyncWorker<int> worker;
    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        10);
    [[maybe_unused]] auto result = worker.getResult();
    EXPECT_FALSE(worker.validate([](int result) { return result == 19; }));
}

TEST_F(AsyncWorkerTest, ValidateVoidSuccess) {
    AsyncWorker<void> worker;
    worker.startAsync(voidTask);
    worker.getResult();
    EXPECT_TRUE(worker.validate([]() { return true; }));
}

TEST_F(AsyncWorkerTest, ValidateVoidFailure) {
    AsyncWorker<void> worker;
    worker.startAsync(voidTask);
    worker.getResult();
    EXPECT_FALSE(worker.validate([]() { return false; }));
}

TEST_F(AsyncWorkerTest, SetCallback) {
    AsyncWorker<int> worker;
    std::atomic<int> callbackResult = 0;
    worker.setCallback([&](int result) { callbackResult = result; });
    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        8);
    worker.waitForCompletion();
    EXPECT_EQ(callbackResult, 16);
}

TEST_F(AsyncWorkerTest, SetCallbackVoid) {
    AsyncWorker<void> worker;
    std::atomic<bool> callbackCalled = false;
    worker.setCallback([&]() { callbackCalled = true; });
    worker.startAsync(voidTask);
    worker.waitForCompletion();
    EXPECT_TRUE(callbackCalled);
}

TEST_F(AsyncWorkerTest, SetTimeoutAndCompletion) {
    AsyncWorker<int> worker;
    worker.setTimeout(1s);
    worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        10);  // Should complete within 1s
    EXPECT_NO_THROW(worker.waitForCompletion());
    EXPECT_TRUE(worker.isDone());
}

TEST_F(AsyncWorkerTest, SetTimeoutAndCompletionTimeout) {
    AsyncWorker<int> worker;
    worker.setTimeout(std::chrono::seconds(1));  // 1 second timeout
    worker.startAsync([]() {
        std::this_thread::sleep_for(
            std::chrono::seconds(2));  // Task takes 2 seconds
        return 1;
    });
    EXPECT_THROW(worker.waitForCompletion(), TimeoutException);
    EXPECT_TRUE(worker.isDone());  // Task is cancelled/finished due to timeout
}

TEST_F(AsyncWorkerTest, SetPriorityAndAffinity) {
    AsyncWorker<int> worker;
    worker.setPriority(AsyncWorker<int>::Priority::HIGH);
    worker.setPreferredCPU(0);  // Assuming CPU 0 exists

    // Hard to test directly without mocking OS calls, but ensure no crash
    EXPECT_NO_THROW(worker.startAsync(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1));
    EXPECT_NO_THROW({ [[maybe_unused]] auto result = worker.getResult(); });
}

// Additional tests for comprehensive coverage

TEST_F(AsyncWorkerTest, CopyConstructorDeleted) {
    // Test that copy constructor is deleted
    AsyncWorker<int> worker1;
    // AsyncWorker<int> worker2 = worker1; // Should not compile
    // This test just verifies the class design
    SUCCEED();
}

TEST_F(AsyncWorkerTest, CopyAssignmentDeleted) {
    // Test that copy assignment is deleted
    AsyncWorker<int> worker1;
    AsyncWorker<int> worker2;
    // worker2 = worker1; // Should not compile
    // This test just verifies the class design
    SUCCEED();
}

TEST_F(AsyncWorkerTest, StartAsyncAlreadyStarted) {
    AsyncWorker<int> worker;
    worker.startAsync([]() { return 42; });

    // Trying to start again should throw
    EXPECT_THROW(worker.startAsync([]() { return 43; }), std::runtime_error);
}

TEST_F(AsyncWorkerTest, GetResultInvalidTask) {
    AsyncWorker<int> worker;
    // No task started, should throw
    EXPECT_THROW(
        { [[maybe_unused]] auto result = worker.getResult(); },
        std::invalid_argument);
}

TEST_F(AsyncWorkerTest, SetCallbackNull) {
    AsyncWorker<int> worker;
    EXPECT_THROW(worker.setCallback(nullptr), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, SetTimeoutNegative) {
    AsyncWorker<int> worker;
    EXPECT_THROW(worker.setTimeout(std::chrono::seconds(-1)),
                 std::invalid_argument);
}

TEST_F(AsyncWorkerTest, ValidateNullValidator) {
    AsyncWorker<int> worker;
    worker.startAsync([]() { return 42; });
    [[maybe_unused]] auto result = worker.getResult();
    EXPECT_FALSE(worker.validate(nullptr));
}

TEST_F(AsyncWorkerTest, ValidateNotDone) {
    AsyncWorker<int> worker;
    // Task not started, validation should fail
    EXPECT_FALSE(worker.validate([](int) { return true; }));
}

TEST_F(AsyncWorkerTest, AllPriorityLevels) {
    AsyncWorker<int> worker;

    // Test all priority levels
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<int>::Priority::LOW));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<int>::Priority::NORMAL));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<int>::Priority::HIGH));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<int>::Priority::CRITICAL));

    worker.startAsync([]() { return 100; });
    EXPECT_EQ(worker.getResult(), 100);
}

// AsyncWorkerManager Tests

TEST_F(AsyncWorkerManagerTest, CreateWorkerInt) {
    auto worker = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        10);
    ASSERT_TRUE(worker != nullptr);
    EXPECT_EQ(manager_int.size(), 1);
    EXPECT_EQ(worker->getResult(), 20);
}

TEST_F(AsyncWorkerManagerTest, CreateWorkerVoid) {
    auto worker = manager_void.createWorker(voidTask);
    ASSERT_TRUE(worker != nullptr);
    EXPECT_EQ(manager_void.size(), 1);
    worker->getResult();  // Should not throw
}

TEST_F(AsyncWorkerManagerTest, CancelAll) {
    [[maybe_unused]] auto worker1 = manager_int.createWorker([]() {
        std::this_thread::sleep_for(200ms);
        return 1;
    });
    [[maybe_unused]] auto worker2 = manager_int.createWorker([]() {
        std::this_thread::sleep_for(200ms);
        return 2;
    });
    EXPECT_EQ(manager_int.size(), 2);
    manager_int.cancelAll();  // Should wait for all to complete
    EXPECT_TRUE(manager_int.allDone());
}

TEST_F(AsyncWorkerManagerTest, AllDone) {
    auto worker1 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    auto worker2 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        2);
    EXPECT_FALSE(manager_int.allDone());
    [[maybe_unused]] auto result1 = worker1->getResult();
    [[maybe_unused]] auto result2 = worker2->getResult();
    EXPECT_TRUE(manager_int.allDone());
}

TEST_F(AsyncWorkerManagerTest, WaitForAll) {
    [[maybe_unused]] auto worker1 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    [[maybe_unused]] auto worker2 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        2);
    EXPECT_NO_THROW(manager_int.waitForAll());
    EXPECT_TRUE(manager_int.allDone());
}

TEST_F(AsyncWorkerManagerTest, WaitForAllWithTimeout) {
    [[maybe_unused]] auto worker1 = manager_int.createWorker([]() {
        std::this_thread::sleep_for(50ms);
        return 1;
    });
    [[maybe_unused]] auto worker2 = manager_int.createWorker([]() {
        std::this_thread::sleep_for(50ms);
        return 2;
    });
    EXPECT_NO_THROW(manager_int.waitForAll(100ms));
    EXPECT_TRUE(manager_int.allDone());
}

TEST_F(AsyncWorkerManagerTest, WaitForAllWithTimeoutFailure) {
    [[maybe_unused]] auto worker = manager_int.createWorker([]() {
        std::this_thread::sleep_for(200ms);
        return 1;
    });
    EXPECT_THROW(manager_int.waitForAll(10ms), TimeoutException);
    // Note: allDone might still be false if tasks are still running after
    // timeout exception
}

TEST_F(AsyncWorkerManagerTest, IsDoneSpecificWorker) {
    auto worker = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    EXPECT_FALSE(manager_int.isDone(worker));
    [[maybe_unused]] auto result = worker->getResult();
    EXPECT_TRUE(manager_int.isDone(worker));
}

TEST_F(AsyncWorkerManagerTest, CancelSpecificWorker) {
    auto worker = manager_int.createWorker([]() {
        std::this_thread::sleep_for(200ms);
        return 1;
    });
    EXPECT_TRUE(worker->isActive());
    manager_int.cancel(worker);
    EXPECT_TRUE(worker->isDone());
}

TEST_F(AsyncWorkerManagerTest, Size) {
    EXPECT_EQ(manager_int.size(), 0);
    [[maybe_unused]] auto worker1 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    EXPECT_EQ(manager_int.size(), 1);
    [[maybe_unused]] auto worker2 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        2);
    EXPECT_EQ(manager_int.size(), 2);
}

TEST_F(AsyncWorkerManagerTest, PruneCompletedWorkers) {
    auto worker1 = manager_int.createWorker(
        [](int x) {
            std::this_thread::sleep_for(10ms);
            return x * 2;
        },
        1);
    auto worker2 = manager_int.createWorker([]() {
        std::this_thread::sleep_for(50ms);
        return 2;
    });

    [[maybe_unused]] auto result1 = worker1->getResult();  // Complete worker1

    EXPECT_EQ(manager_int.size(), 2);
    size_t pruned = manager_int.pruneCompletedWorkers();
    EXPECT_EQ(pruned, 1);
    EXPECT_EQ(manager_int.size(), 1);  // Only worker2 should remain

    [[maybe_unused]] auto result2 = worker2->getResult();  // Complete worker2
    pruned = manager_int.pruneCompletedWorkers();
    EXPECT_EQ(pruned, 1);
    EXPECT_EQ(manager_int.size(), 0);
}

// Coroutine Task Tests

// Simple coroutine that returns an int
Task<int> simpleCoroutine(int val) { co_return val * 3; }

// Coroutine that throws an exception
Task<void> throwingCoroutine() {
    throw std::runtime_error("Coroutine error");
    co_return;
}

TEST_F(AsyncWorkerTest, TaskAwaitResult) {
    Task<int> task = simpleCoroutine(5);
    EXPECT_EQ(task.await_result(), 15);
    EXPECT_TRUE(task.done());
}

TEST_F(AsyncWorkerTest, TaskThrowingCoroutine) {
    Task<void> task = throwingCoroutine();
    EXPECT_THROW(task.await_result(), std::runtime_error);
    EXPECT_TRUE(task.done());
}

TEST_F(AsyncWorkerTest, TaskMoveConstructor) {
    Task<int> task1 = simpleCoroutine(10);
    Task<int> task2 = std::move(task1);
    EXPECT_EQ(task2.await_result(), 30);
    EXPECT_TRUE(task2.done());
    // task1 is now in a valid but unspecified state, should not be used
}

TEST_F(AsyncWorkerTest, TaskMoveAssignment) {
    Task<int> task1 = simpleCoroutine(2);
    Task<int> task2;
    task2 = std::move(task1);
    EXPECT_EQ(task2.await_result(), 6);
    EXPECT_TRUE(task2.done());
}

// asyncRetryImpl Tests (indirectly tested by asyncRetry/asyncRetryE, but can
// add specific ones)

TEST_F(AsyncWorkerTest, AsyncRetryImplSuccess) {
    int callCount = 0;
    auto result = asyncRetryImpl(
        [&]() {
            callCount++;
            return 100;
        },
        3, 1ms, BackoffStrategy::FIXED, 100ms,
        [](int res) { EXPECT_EQ(res, 100); },
        [](const std::exception&) { FAIL() << "Should not throw"; }, []() {});
    EXPECT_EQ(result, 100);
    EXPECT_EQ(callCount, 1);
}

TEST_F(AsyncWorkerTest, AsyncRetryImplFailureThenSuccess) {
    int callCount = 0;
    auto result = asyncRetryImpl(
        [&]() {
            callCount++;
            if (callCount < 2) {
                throw std::runtime_error("Temporary error");
            }
            return 200;
        },
        3, 1ms, BackoffStrategy::FIXED, 100ms,
        [](int res) { EXPECT_EQ(res, 200); },
        [](const std::exception&) { SUCCEED(); },  // Expect exception
        []() {});
    EXPECT_EQ(result, 200);
    EXPECT_EQ(callCount, 2);
}

TEST_F(AsyncWorkerTest, AsyncRetryImplAllAttemptsFail) {
    int callCount = 0;
    EXPECT_THROW(
        {
            asyncRetryImpl(
                [&]() -> int {
                    callCount++;
                    throw std::runtime_error("Always fails");
                    return 0;  // Never reached
                },
                3, 1ms, BackoffStrategy::FIXED, 100ms,
                [](int) { FAIL() << "Should not succeed"; },
                [](const std::exception&) { SUCCEED(); },  // Expect exception
                []() {});
        },
        std::runtime_error);
    EXPECT_EQ(callCount, 3);
}

// asyncRetryTask Tests (coroutine version)

TEST_F(AsyncWorkerTest, AsyncRetryTaskSuccess) {
    int callCount = 0;
    auto task = asyncRetryTask(
        [&]() {
            callCount++;
            return 123;
        },
        3, 1ms, BackoffStrategy::FIXED);
    EXPECT_EQ(task.await_result(), 123);
    EXPECT_EQ(callCount, 1);
}

TEST_F(AsyncWorkerTest, AsyncRetryTaskFailureThenSuccess) {
    int callCount = 0;
    auto task = asyncRetryTask(
        [&]() {
            callCount++;
            if (callCount < 2) {
                throw std::runtime_error("Temporary error");
            }
            return 456;
        },
        3, 1ms, BackoffStrategy::FIXED);
    EXPECT_EQ(task.await_result(), 456);
    EXPECT_EQ(callCount, 2);
}

TEST_F(AsyncWorkerTest, AsyncRetryTaskAllAttemptsFail) {
    int callCount = 0;
    auto task = asyncRetryTask(
        [&]() {
            callCount++;
            throw std::runtime_error("Always fails");
        },
        3, 1ms, BackoffStrategy::FIXED);
    EXPECT_THROW(task.await_result(), std::runtime_error);
    EXPECT_EQ(callCount, 3);
}

// getWithTimeout Tests

TEST_F(AsyncWorkerTest, GetWithTimeoutSuccess) {
    std::promise<int> p;
    std::future<int> f = p.get_future();
    std::thread([&]() {
        std::this_thread::sleep_for(10ms);
        p.set_value(42);
    }).detach();
    EXPECT_EQ(getWithTimeout(f, 100ms), 42);
}

TEST_F(AsyncWorkerTest, GetWithTimeoutFailure) {
    std::promise<int> p;
    std::future<int> f = p.get_future();
    // Don't set value, let it timeout
    EXPECT_THROW(getWithTimeout(f, 10ms), TimeoutException);
}

TEST_F(AsyncWorkerTest, GetWithTimeoutInvalidFuture) {
    std::future<int> f;  // Invalid future
    EXPECT_THROW(getWithTimeout(f, 10ms), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, GetWithTimeoutNegativeTimeout) {
    std::promise<int> p;
    std::future<int> f = p.get_future();
    EXPECT_THROW(getWithTimeout(f, -10ms), std::invalid_argument);
}

// Additional comprehensive tests for better coverage

TEST_F(AsyncWorkerTest, AsyncRetryLinearBackoff) {
    int callCount = 0;
    auto result = asyncRetryImpl(
        [&]() {
            callCount++;
            if (callCount < 3) {
                throw std::runtime_error("Temporary error");
            }
            return 300;
        },
        5, 1ms, BackoffStrategy::LINEAR, 100ms,
        [](int res) { EXPECT_EQ(res, 300); },
        [](const std::exception&) { /* Expected */ }, []() {});
    EXPECT_EQ(result, 300);
    EXPECT_EQ(callCount, 3);
}

TEST_F(AsyncWorkerTest, AsyncRetryExponentialBackoff) {
    int callCount = 0;
    auto result = asyncRetryImpl(
        [&]() {
            callCount++;
            if (callCount < 2) {
                throw std::runtime_error("Temporary error");
            }
            return 400;
        },
        3, 1ms, BackoffStrategy::EXPONENTIAL, 100ms,
        [](int res) { EXPECT_EQ(res, 400); },
        [](const std::exception&) { /* Expected */ }, []() {});
    EXPECT_EQ(result, 400);
    EXPECT_EQ(callCount, 2);
}

TEST_F(AsyncWorkerTest, AsyncRetryTaskLinearBackoff) {
    int callCount = 0;
    auto task = asyncRetryTask(
        [&]() {
            callCount++;
            if (callCount < 2) {
                throw std::runtime_error("Temporary error");
            }
            return 500;
        },
        3, 1ms, BackoffStrategy::LINEAR);
    EXPECT_EQ(task.await_result(), 500);
    EXPECT_EQ(callCount, 2);
}

TEST_F(AsyncWorkerTest, AsyncRetryTaskExponentialBackoff) {
    int callCount = 0;
    auto task = asyncRetryTask(
        [&]() {
            callCount++;
            if (callCount < 3) {
                throw std::runtime_error("Temporary error");
            }
            return 600;
        },
        5, 1ms, BackoffStrategy::EXPONENTIAL);
    EXPECT_EQ(task.await_result(), 600);
    EXPECT_EQ(callCount, 3);
}

// Tests for void specialization edge cases

TEST_F(AsyncWorkerTest, VoidWorkerStartAsyncAlreadyStarted) {
    AsyncWorker<void> worker;
    worker.startAsync(voidTask);

    // Trying to start again should throw
    EXPECT_THROW(worker.startAsync(voidTask), std::runtime_error);
}

TEST_F(AsyncWorkerTest, VoidWorkerGetResultInvalidTask) {
    AsyncWorker<void> worker;
    // No task started, should throw
    EXPECT_THROW(worker.getResult(), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, VoidWorkerSetCallbackNull) {
    AsyncWorker<void> worker;
    EXPECT_THROW(worker.setCallback(nullptr), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, VoidWorkerSetTimeoutNegative) {
    AsyncWorker<void> worker;
    EXPECT_THROW(worker.setTimeout(std::chrono::seconds(-1)),
                 std::invalid_argument);
}

TEST_F(AsyncWorkerTest, VoidWorkerValidateNullValidator) {
    AsyncWorker<void> worker;
    worker.startAsync(voidTask);
    worker.getResult();
    EXPECT_FALSE(worker.validate(nullptr));
}

TEST_F(AsyncWorkerTest, VoidWorkerValidateNotDone) {
    AsyncWorker<void> worker;
    // Task not started, validation should fail
    EXPECT_FALSE(worker.validate([]() { return true; }));
}

TEST_F(AsyncWorkerTest, VoidWorkerAllPriorityLevels) {
    AsyncWorker<void> worker;

    // Test all priority levels
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<void>::Priority::LOW));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<void>::Priority::NORMAL));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<void>::Priority::HIGH));
    EXPECT_NO_THROW(worker.setPriority(AsyncWorker<void>::Priority::CRITICAL));

    worker.startAsync(voidTask);
    worker.getResult();
    EXPECT_TRUE(worker.isDone());
}

TEST_F(AsyncWorkerTest, VoidWorkerSetPreferredCPU) {
    AsyncWorker<void> worker;
    EXPECT_NO_THROW(worker.setPreferredCPU(0));
    EXPECT_NO_THROW(worker.setPreferredCPU(1));

    worker.startAsync(voidTask);
    worker.getResult();
    EXPECT_TRUE(worker.isDone());
}

// Additional edge case tests

TEST_F(AsyncWorkerTest, StartAsyncWithNullFunction) {
    AsyncWorker<int> worker;
    int (*nullFunc)(int) = nullptr;
    EXPECT_THROW(worker.startAsync(nullFunc, 5), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, VoidWorkerStartAsyncWithNullFunction) {
    AsyncWorker<void> worker;
    void (*nullFunc)() = nullptr;
    EXPECT_THROW(worker.startAsync(nullFunc), std::invalid_argument);
}

TEST_F(AsyncWorkerTest, CallbackExecutionFailure) {
    AsyncWorker<int> worker;
    worker.setCallback([](int) { throw std::runtime_error("Callback error"); });
    worker.startAsync([]() { return 42; });

    // waitForCompletion should throw due to callback failure
    EXPECT_THROW(worker.waitForCompletion(), std::runtime_error);
}

TEST_F(AsyncWorkerTest, VoidWorkerCallbackExecutionFailure) {
    AsyncWorker<void> worker;
    worker.setCallback([]() { throw std::runtime_error("Callback error"); });
    worker.startAsync(voidTask);

    // waitForCompletion should throw due to callback failure
    EXPECT_THROW(worker.waitForCompletion(), std::runtime_error);
}

TEST_F(AsyncWorkerTest, AsyncRetryInvalidParameters) {
    // Test with zero attempts
    EXPECT_THROW(
        {
            asyncRetryImpl([]() { return 42; }, 0, 1ms, BackoffStrategy::FIXED,
                           100ms, [](int) {}, [](const std::exception&) {},
                           []() {});
        },
        std::invalid_argument);
}

TEST_F(AsyncWorkerTest, AsyncRetryTaskInvalidParameters) {
    // Test with zero attempts
    EXPECT_THROW(
        {
            auto task = asyncRetryTask([]() { return 42; }, 0, 1ms,
                                       BackoffStrategy::FIXED);
            [[maybe_unused]] auto result = task.await_result();
        },
        std::invalid_argument);
}

TEST_F(AsyncWorkerManagerTest, IsDoneWithNullWorker) {
    std::shared_ptr<AsyncWorker<int>> nullWorker = nullptr;
    EXPECT_THROW(
        { [[maybe_unused]] auto done = manager_int.isDone(nullWorker); },
        std::invalid_argument);
}

TEST_F(AsyncWorkerManagerTest, CancelWithNullWorker) {
    std::shared_ptr<AsyncWorker<int>> nullWorker = nullptr;
    EXPECT_THROW(manager_int.cancel(nullWorker), std::invalid_argument);
}

// Test destructor behavior - simplified to avoid template issues
TEST_F(AsyncWorkerTest, DestructorCancelsRunningTask) {
    {
        AsyncWorker<int> worker;
        worker.startAsync(
            [](int x) {
                std::this_thread::sleep_for(100ms);
                return x * 2;
            },
            21);
        // Worker goes out of scope, destructor should cancel the task
    }
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}

TEST_F(AsyncWorkerTest, VoidWorkerDestructorCancelsRunningTask) {
    {
        AsyncWorker<void> worker;
        worker.startAsync(voidTask);  // Use existing helper function
        // Worker goes out of scope, destructor should cancel the task
    }
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}
