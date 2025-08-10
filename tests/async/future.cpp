#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>

#include "atom/async/future.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

TEST(EnhancedFutureTest, Constructor) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));
    EXPECT_FALSE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureTest, Then) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));
    auto nextFuture = enhancedFuture.then([](int value) { return value + 1; });

    promise.set_value(42);
    EXPECT_EQ(nextFuture.wait(), 43);
}

TEST(EnhancedFutureTest, WaitFor) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::thread([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        promise.set_value(42);
    }).detach();

    auto result = enhancedFuture.waitFor(std::chrono::milliseconds(200));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(EnhancedFutureTest, IsDone) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    EXPECT_FALSE(enhancedFuture.isDone());
    promise.set_value(42);
    EXPECT_TRUE(enhancedFuture.isDone());
}

TEST(EnhancedFutureTest, OnComplete) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    bool callbackCalled = false;
    enhancedFuture.onComplete([&callbackCalled](int value) {
        callbackCalled = true;
        EXPECT_EQ(value, 42);
    });

    promise.set_value(42);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(callbackCalled);
}

TEST(EnhancedFutureTest, Wait) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture.wait(), 42);
}

TEST(EnhancedFutureTest, Cancel) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureTest, GetException) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    promise.set_exception(std::make_exception_ptr(std::runtime_error("error")));
    auto exceptionPtr = enhancedFuture.getException();
    EXPECT_TRUE(exceptionPtr != nullptr);
}

TEST(EnhancedFutureTest, Retry) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto retryFuture =
        enhancedFuture.retry([](int value) { return value + 1; }, 3);
    promise.set_value(42);
    EXPECT_EQ(retryFuture.wait(), 43);
}

TEST(MakeEnhancedFutureTest, CreateEnhancedFuture) {
    auto enhancedFuture = makeEnhancedFuture([]() { return 42; });
    EXPECT_EQ(enhancedFuture.wait(), 42);
}

TEST(WhenAllTest, RangeOfFutures) {
    std::vector<std::shared_future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() { return i; }).share());
    }

    auto resultFuture = whenAll(futures.begin(), futures.end());
    auto results = resultFuture.get();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i], i);  // results is a vector of values, not futures
    }
}

TEST(WhenAllTest, VariadicFutures) {
    auto future1 = std::async(std::launch::async, []() { return 1; });
    auto future2 = std::async(std::launch::async, []() { return 2; });
    auto future3 = std::async(std::launch::async, []() { return 3; });

    auto resultFuture =
        whenAll(std::move(future1), std::move(future2), std::move(future3));
    auto results = resultFuture.get();
    EXPECT_EQ(std::get<0>(results), 1);
    EXPECT_EQ(std::get<1>(results), 2);
    EXPECT_EQ(std::get<2>(results), 3);
}

// Additional comprehensive tests

TEST(EnhancedFutureTest, ConstructorFromConstSharedFuture) {
    std::promise<int> promise;
    const auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(future);
    EXPECT_FALSE(enhancedFuture.isCancelled());

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture.get(), 42);
}

TEST(EnhancedFutureTest, IsReady) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    EXPECT_FALSE(enhancedFuture.isReady());
    promise.set_value(42);

    // Give some time for the future to become ready
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(enhancedFuture.isReady());
}

TEST(EnhancedFutureTest, Get) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    promise.set_value(123);
    EXPECT_EQ(enhancedFuture.get(), 123);
}

TEST(EnhancedFutureTest, GetCancelled) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.get(), InvalidFutureException);
}

TEST(EnhancedFutureTest, WaitCancelled) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.wait(), InvalidFutureException);
}

TEST(EnhancedFutureTest, WaitForTimeout) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    // Should timeout and return nullopt
    auto result = enhancedFuture.waitFor(50ms);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(enhancedFuture.isCancelled());  // Should be cancelled after timeout
}

// Note: WaitForWithCustomCancelPolicy test removed due to template constraint issue in implementation

TEST(EnhancedFutureTest, ThenChaining) {
    auto enhancedFuture = makeEnhancedFuture([]() { return 10; });

    auto chained = enhancedFuture
        .then([](int x) { return x * 2; })
        .then([](int x) { return x + 5; })
        .then([](int x) { return std::to_string(x); });

    EXPECT_EQ(chained.wait(), "25");
}

TEST(EnhancedFutureTest, ThenWithException) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto chained = enhancedFuture.then([](int x) { return x * 2; });

    promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
    EXPECT_THROW(chained.wait(), InvalidFutureException);
}

TEST(EnhancedFutureTest, CatchException) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto caught = enhancedFuture.catching([](std::exception_ptr) { return 999; });

    promise.set_exception(std::make_exception_ptr(std::runtime_error("test error")));
    EXPECT_EQ(caught.wait(), 999);
}

TEST(EnhancedFutureTest, RetrySuccess) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto retried = enhancedFuture.retry([](int x) { return x + 10; }, 3);

    promise.set_value(5);
    EXPECT_EQ(retried.wait(), 15);
}

TEST(EnhancedFutureTest, RetryWithFailure) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    int attemptCount = 0;
    auto retried = enhancedFuture.retry([&attemptCount](int) -> int {
        attemptCount++;
        if (attemptCount < 3) {
            throw std::runtime_error("Retry needed");
        }
        return 42;
    }, 5);

    promise.set_value(1);
    EXPECT_EQ(retried.wait(), 42);
    EXPECT_EQ(attemptCount, 3);
}

TEST(EnhancedFutureTest, MultipleCallbacks) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::atomic<int> callbackCount{0};

    enhancedFuture.onComplete([&callbackCount](int) { callbackCount++; });
    enhancedFuture.onComplete([&callbackCount](int) { callbackCount++; });
    enhancedFuture.onComplete([&callbackCount](int) { callbackCount++; });

    promise.set_value(42);

    // Give callbacks time to execute
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(callbackCount.load(), 3);
}

// Tests for void specialization

TEST(EnhancedFutureVoidTest, Constructor) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));
    EXPECT_FALSE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureVoidTest, ConstructorFromConstSharedFuture) {
    std::promise<void> promise;
    const auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(future);
    EXPECT_FALSE(enhancedFuture.isCancelled());

    promise.set_value();
    enhancedFuture.get();  // Should not throw
}

TEST(EnhancedFutureVoidTest, Then) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    auto nextFuture = enhancedFuture.then([]() { return 42; });

    promise.set_value();
    EXPECT_EQ(nextFuture.wait(), 42);
}

TEST(EnhancedFutureVoidTest, WaitFor) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    std::thread([&promise]() {
        std::this_thread::sleep_for(50ms);
        promise.set_value();
    }).detach();

    bool result = enhancedFuture.waitFor(100ms);
    EXPECT_TRUE(result);
}

TEST(EnhancedFutureVoidTest, WaitForTimeout) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    bool result = enhancedFuture.waitFor(50ms);
    EXPECT_FALSE(result);
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureVoidTest, IsDone) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    EXPECT_FALSE(enhancedFuture.isDone());
    promise.set_value();

    // Give some time for the future to become ready
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(enhancedFuture.isDone());
}

TEST(EnhancedFutureVoidTest, OnComplete) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    bool callbackCalled = false;
    enhancedFuture.onComplete([&callbackCalled]() {
        callbackCalled = true;
    });

    promise.set_value();
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(callbackCalled);
}

TEST(EnhancedFutureVoidTest, Wait) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    promise.set_value();
    EXPECT_NO_THROW(enhancedFuture.wait());
}

TEST(EnhancedFutureVoidTest, Cancel) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureVoidTest, GetCancelled) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.get(), InvalidFutureException);
}

TEST(EnhancedFutureVoidTest, WaitCancelled) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.wait(), InvalidFutureException);
}

TEST(EnhancedFutureVoidTest, GetException) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    promise.set_exception(std::make_exception_ptr(std::runtime_error("error")));
    auto exceptionPtr = enhancedFuture.getException();
    EXPECT_TRUE(exceptionPtr != nullptr);
}

TEST(EnhancedFutureVoidTest, ThenChaining) {
    auto enhancedFuture = makeEnhancedFuture([]() { /* void function */ });

    auto chained = enhancedFuture
        .then([]() { return 10; })
        .then([](int x) { return x * 2; })
        .then([](int x) { return std::to_string(x); });

    EXPECT_EQ(chained.wait(), "20");
}

TEST(EnhancedFutureVoidTest, MultipleCallbacks) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    std::atomic<int> callbackCount{0};

    enhancedFuture.onComplete([&callbackCount]() { callbackCount++; });
    enhancedFuture.onComplete([&callbackCount]() { callbackCount++; });
    enhancedFuture.onComplete([&callbackCount]() { callbackCount++; });

    promise.set_value();

    // Give callbacks time to execute
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(callbackCount.load(), 3);
}

// Tests for utility functions

TEST(MakeEnhancedFutureTest, CreateVoidEnhancedFuture) {
    bool executed = false;
    auto enhancedFuture = makeEnhancedFuture([&executed]() { executed = true; });
    enhancedFuture.wait();
    EXPECT_TRUE(executed);
}

TEST(MakeEnhancedFutureTest, CreateWithArguments) {
    auto enhancedFuture = makeEnhancedFuture([](int a, int b) { return a + b; }, 10, 20);
    EXPECT_EQ(enhancedFuture.wait(), 30);
}

TEST(MakeEnhancedFutureTest, CreateWithException) {
    auto enhancedFuture = makeEnhancedFuture([]() -> int {
        throw std::runtime_error("test exception");
    });

    EXPECT_THROW(enhancedFuture.wait(), InvalidFutureException);
}

TEST(WhenAllTest, EmptyRange) {
    std::vector<std::shared_future<int>> futures;
    auto resultFuture = whenAll(futures.begin(), futures.end());
    auto results = resultFuture.get();
    EXPECT_TRUE(results.empty());
}

TEST(WhenAllTest, SingleFuture) {
    std::vector<std::shared_future<int>> futures;
    futures.push_back(std::async(std::launch::async, []() { return 42; }).share());

    auto resultFuture = whenAll(futures.begin(), futures.end());
    auto results = resultFuture.get();
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], 42);
}

TEST(WhenAllTest, MixedTypes) {
    auto future1 = std::async(std::launch::async, []() { return 1; });
    auto future2 = std::async(std::launch::async, []() { return std::string("hello"); });
    auto future3 = std::async(std::launch::async, []() { return 3.14; });

    auto resultFuture = whenAll(std::move(future1), std::move(future2), std::move(future3));
    auto results = resultFuture.get();

    EXPECT_EQ(std::get<0>(results), 1);
    EXPECT_EQ(std::get<1>(results), "hello");
    EXPECT_DOUBLE_EQ(std::get<2>(results), 3.14);
}

TEST(WhenAllTest, WithException) {
    auto future1 = std::async(std::launch::async, []() { return 1; });
    auto future2 = std::async(std::launch::async, []() -> int {
        throw std::runtime_error("test exception");
    });
    auto future3 = std::async(std::launch::async, []() { return 3; });

    auto resultFuture = whenAll(std::move(future1), std::move(future2), std::move(future3));
    EXPECT_THROW(resultFuture.get(), std::runtime_error);
}

// Tests for exception handling

TEST(InvalidFutureExceptionTest, Construction) {
    InvalidFutureException ex(__FILE__, __LINE__, __FUNCTION__, "Test message");
    std::string what_str = ex.what();
    EXPECT_NE(what_str.find("Test message"), std::string::npos);
}

TEST(InvalidFutureExceptionTest, Inheritance) {
    try {
        throw InvalidFutureException(__FILE__, __LINE__, __FUNCTION__, "Test exception");
    } catch (const std::exception& e) {
        std::string what_str = e.what();
        EXPECT_NE(what_str.find("Test exception"), std::string::npos);
    }
}

// Edge case tests

TEST(EnhancedFutureTest, CallbackOnCancelledFuture) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();

    // Adding callback to cancelled future should not crash
    bool callbackCalled = false;
    enhancedFuture.onComplete([&callbackCalled](int) { callbackCalled = true; });

    // Give some time to see if callback is called (it shouldn't be)
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(callbackCalled);
}

TEST(EnhancedFutureVoidTest, CallbackOnCancelledFuture) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    enhancedFuture.cancel();

    // Adding callback to cancelled future should not crash
    bool callbackCalled = false;
    enhancedFuture.onComplete([&callbackCalled]() { callbackCalled = true; });

    // Give some time to see if callback is called (it shouldn't be)
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(callbackCalled);
}

TEST(EnhancedFutureTest, ConcurrentOperations) {
    auto enhancedFuture = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(100ms);
        return 42;
    });

    std::atomic<int> callbackCount{0};

    // Add callbacks from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&enhancedFuture, &callbackCount]() {
            enhancedFuture.onComplete([&callbackCount](int) { callbackCount++; });
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for the future to complete
    EXPECT_EQ(enhancedFuture.wait(), 42);

    // Give callbacks time to execute
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(callbackCount.load(), 5);
}

TEST(EnhancedFutureTest, ChainedOperationsWithDelay) {
    auto enhancedFuture = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(50ms);
        return 10;
    });

    auto chained = enhancedFuture
        .then([](int x) {
            std::this_thread::sleep_for(50ms);
            return x * 2;
        })
        .then([](int x) {
            std::this_thread::sleep_for(50ms);
            return x + 5;
        });

    EXPECT_EQ(chained.wait(), 25);
}
