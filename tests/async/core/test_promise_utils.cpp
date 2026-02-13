/**
 * @file test_promise_utils.cpp
 * @brief Tests for promise_utils.hpp utility functions
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "atom/async/core/promise_utils.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

class PromiseUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// makeResolvedPromise Tests
// ============================================================================

TEST_F(PromiseUtilsTest, MakeResolvedPromise_Int) {
    auto promise = makeResolvedPromise(42);
    auto future = promise.getFuture();
    EXPECT_EQ(future.get(), 42);
}

TEST_F(PromiseUtilsTest, MakeResolvedPromise_String) {
    auto promise = makeResolvedPromise(std::string("Hello"));
    auto future = promise.getFuture();
    EXPECT_EQ(future.get(), "Hello");
}

TEST_F(PromiseUtilsTest, MakeResolvedPromise_Vector) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto promise = makeResolvedPromise(vec);
    auto future = promise.getFuture();
    auto result = future.get();
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 1);
}

// ============================================================================
// makeRejectedPromise Tests
// ============================================================================

TEST_F(PromiseUtilsTest, MakeRejectedPromise_RuntimeError) {
    auto promise = makeRejectedPromise<int>(
        std::make_exception_ptr(std::runtime_error("Test error")));
    auto future = promise.getFuture();
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(PromiseUtilsTest, MakeRejectedPromise_InvalidArgument) {
    auto promise = makeRejectedPromise<std::string>(
        std::make_exception_ptr(std::invalid_argument("Invalid arg")));
    auto future = promise.getFuture();
    EXPECT_THROW(future.get(), std::invalid_argument);
}

// ============================================================================
// whenAny Tests
// ============================================================================

TEST_F(PromiseUtilsTest, WhenAny_FirstCompletes) {
    std::vector<Promise<int>> promises;
    promises.emplace_back();
    promises.emplace_back();
    promises.emplace_back();

    auto resultPromise = whenAny(promises);

    // Complete the second promise first
    promises[1].setValue(42);

    // Give time for callback to execute
    std::this_thread::sleep_for(50ms);

    // The result promise should have the value from the first completed promise
    auto future = resultPromise->getFuture();
    EXPECT_EQ(future.get(), 42);
}

TEST_F(PromiseUtilsTest, WhenAny_EmptyVector) {
    std::vector<Promise<int>> promises;
    auto resultPromise = whenAny(promises);
    auto future = resultPromise->getFuture();
    EXPECT_THROW(future.get(), std::invalid_argument);
}

TEST_F(PromiseUtilsTest, WhenAny_Void_FirstCompletes) {
    std::vector<Promise<void>> promises;
    promises.emplace_back();
    promises.emplace_back();

    auto resultPromise = whenAny(promises);

    promises[0].setValue();

    std::this_thread::sleep_for(50ms);
    auto future = resultPromise->getFuture();
    EXPECT_NO_THROW(future.get());
}

TEST_F(PromiseUtilsTest, WhenAny_Void_EmptyVector) {
    std::vector<Promise<void>> promises;
    auto resultPromise = whenAny(promises);
    auto future = resultPromise->getFuture();
    EXPECT_THROW(future.get(), std::invalid_argument);
}

// ============================================================================
// delay Tests
// ============================================================================

TEST_F(PromiseUtilsTest, Delay_CompletesAfterDuration) {
    auto start = std::chrono::steady_clock::now();
    auto promise = delay(50ms);
    auto future = promise.getFuture();

    future.get();  // Wait for completion

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, 50ms);
}

TEST_F(PromiseUtilsTest, Delay_ShortDuration) {
    auto promise = delay(10ms);
    auto future = promise.getFuture();
    EXPECT_NO_THROW(future.get());
}

// ============================================================================
// retry Tests
// ============================================================================

TEST_F(PromiseUtilsTest, Retry_SucceedsFirstAttempt) {
    std::atomic<int> attemptCount{0};

    auto promise = retry<int>(
        [&attemptCount]() {
            attemptCount++;
            return 42;
        },
        3, 10ms);

    auto future = promise.getFuture();
    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(attemptCount.load(), 1);
}

TEST_F(PromiseUtilsTest, Retry_SucceedsAfterRetries) {
    std::atomic<int> attemptCount{0};

    auto promise = retry<int>(
        [&attemptCount]() -> int {
            attemptCount++;
            if (attemptCount < 3) {
                throw std::runtime_error("Retry needed");
            }
            return 42;
        },
        5, 10ms);

    auto future = promise.getFuture();
    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(attemptCount.load(), 3);
}

TEST_F(PromiseUtilsTest, Retry_ExhaustsRetries) {
    std::atomic<int> attemptCount{0};

    auto promise = retry<int>(
        [&attemptCount]() -> int {
            attemptCount++;
            throw std::runtime_error("Always fails");
        },
        3, 10ms);

    auto future = promise.getFuture();
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_EQ(attemptCount.load(), 4);  // Initial + 3 retries
}

// ============================================================================
// retryVoid Tests
// ============================================================================

TEST_F(PromiseUtilsTest, RetryVoid_SucceedsFirstAttempt) {
    std::atomic<int> attemptCount{0};

    auto promise = retryVoid([&attemptCount]() { attemptCount++; }, 3, 10ms);

    auto future = promise.getFuture();
    EXPECT_NO_THROW(future.get());
    EXPECT_EQ(attemptCount.load(), 1);
}

TEST_F(PromiseUtilsTest, RetryVoid_SucceedsAfterRetries) {
    std::atomic<int> attemptCount{0};

    auto promise = retryVoid(
        [&attemptCount]() {
            attemptCount++;
            if (attemptCount < 2) {
                throw std::runtime_error("Retry needed");
            }
        },
        3, 10ms);

    auto future = promise.getFuture();
    EXPECT_NO_THROW(future.get());
    EXPECT_EQ(attemptCount.load(), 2);
}

// ============================================================================
// withTimeout Tests
// ============================================================================

TEST_F(PromiseUtilsTest, WithTimeout_CompletesBeforeTimeout) {
    Promise<int> promise;
    auto timeoutPromise = withTimeout(promise, 100ms);

    // Complete before timeout
    promise.setValue(42);

    std::this_thread::sleep_for(50ms);

    auto future = timeoutPromise->getFuture();
    EXPECT_EQ(future.get(), 42);
}

TEST_F(PromiseUtilsTest, WithTimeout_TimesOut) {
    Promise<int> promise;
    auto timeoutPromise = withTimeout(promise, 20ms);

    // Don't set value - let it timeout

    std::this_thread::sleep_for(50ms);

    auto future = timeoutPromise->getFuture();
    EXPECT_THROW(future.get(), std::runtime_error);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(PromiseUtilsTest, CombinedUsage_DelayThenResolve) {
    auto delayPromise = delay(10ms);
    auto future = delayPromise.getFuture();

    future.get();

    auto resultPromise = makeResolvedPromise(100);
    auto resultFuture = resultPromise.getFuture();
    EXPECT_EQ(resultFuture.get(), 100);
}

TEST_F(PromiseUtilsTest, ConcurrentWhenAny) {
    std::vector<Promise<int>> promises;
    for (int i = 0; i < 5; ++i) {
        promises.emplace_back();
    }

    auto resultPromise = whenAny(promises);

    // Complete promises from different threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&promises, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            try {
                promises[i].setValue(i * 10);
            } catch (...) {
                // Ignore if already resolved
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Result should be set
    std::this_thread::sleep_for(100ms);
}
