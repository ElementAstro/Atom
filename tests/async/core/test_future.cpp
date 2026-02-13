#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>

#include "atom/async/future.hpp"

using namespace atom::async;

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
    std::vector<atom::async::EnhancedFuture<int>> futures;
    for (int i = 0; i < 5; ++i) {
        auto stdFuture = std::async(std::launch::async, [i]() { return i; });
        auto sharedFuture = stdFuture.share();
        futures.emplace_back(std::move(sharedFuture));
    }

    auto resultFuture = whenAll(futures.begin(), futures.end());
    auto results = resultFuture.get();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

TEST(WhenAllTest, VariadicFutures) {
    auto stdFuture1 = std::async(std::launch::async, []() { return 1; });
    auto stdFuture2 = std::async(std::launch::async, []() { return 2; });
    auto stdFuture3 = std::async(std::launch::async, []() { return 3; });

    auto future1 = atom::async::EnhancedFuture<int>(stdFuture1.share());
    auto future2 = atom::async::EnhancedFuture<int>(stdFuture2.share());
    auto future3 = atom::async::EnhancedFuture<int>(stdFuture3.share());

    auto resultFuture =
        whenAll(std::move(future1), std::move(future2), std::move(future3));
    auto results = resultFuture.get();
    EXPECT_EQ(std::get<0>(results), 1);
    EXPECT_EQ(std::get<1>(results), 2);
    EXPECT_EQ(std::get<2>(results), 3);
}

// =============================================================================
// Additional EnhancedFuture Tests
// =============================================================================

TEST(EnhancedFutureTest, DefaultConstructor) {
    EnhancedFuture<int> future;
    EXPECT_FALSE(future.isCancelled());
}

TEST(EnhancedFutureTest, CopyConstructor) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture1(std::move(future));
    EnhancedFuture<int> enhancedFuture2(enhancedFuture1);

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture1.wait(), 42);
    EXPECT_EQ(enhancedFuture2.wait(), 42);
}

TEST(EnhancedFutureTest, MoveConstructor) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture1(std::move(future));
    EnhancedFuture<int> enhancedFuture2(std::move(enhancedFuture1));

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture2.wait(), 42);
}

TEST(EnhancedFutureTest, CopyAssignment) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture1(std::move(future));
    EnhancedFuture<int> enhancedFuture2;
    enhancedFuture2 = enhancedFuture1;

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture1.wait(), 42);
    EXPECT_EQ(enhancedFuture2.wait(), 42);
}

TEST(EnhancedFutureTest, MoveAssignment) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture1(std::move(future));
    EnhancedFuture<int> enhancedFuture2;
    enhancedFuture2 = std::move(enhancedFuture1);

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture2.wait(), 42);
}

TEST(EnhancedFutureTest, WaitForTimeout) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto result = enhancedFuture.waitFor(std::chrono::milliseconds(10));
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureTest, WaitForSuccess) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::thread([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        promise.set_value(42);
    }).detach();

    auto result = enhancedFuture.waitFor(std::chrono::milliseconds(100));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(EnhancedFutureTest, IsReady) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    EXPECT_FALSE(enhancedFuture.isReady());
    promise.set_value(42);
    EXPECT_TRUE(enhancedFuture.isReady());
}

TEST(EnhancedFutureTest, Get) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    promise.set_value(42);
    EXPECT_EQ(enhancedFuture.get(), 42);
}

TEST(EnhancedFutureTest, GetAfterCancel) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.get(), InvalidFutureException);
}

TEST(EnhancedFutureTest, WaitAfterCancel) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_THROW(enhancedFuture.wait(), InvalidFutureException);
}

TEST(EnhancedFutureTest, ThenChaining) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto chainedFuture =
        enhancedFuture.then([](int value) { return value * 2; })
            .then([](int value) { return value + 10; });

    promise.set_value(5);
    EXPECT_EQ(chainedFuture.wait(), 20);  // (5 * 2) + 10 = 20
}

TEST(EnhancedFutureTest, Catching) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto catchingFuture = enhancedFuture.catching([](std::exception_ptr) {
        return -1;  // Default value on exception
    });

    promise.set_exception(std::make_exception_ptr(std::runtime_error("error")));
    EXPECT_EQ(catchingFuture.wait(), -1);
}

TEST(EnhancedFutureTest, RetrySuccess) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    auto retryFuture =
        enhancedFuture.retry([](int value) { return value + 1; }, 3);

    promise.set_value(42);
    EXPECT_EQ(retryFuture.wait(), 43);
}

TEST(EnhancedFutureTest, MultipleOnComplete) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::atomic<int> callCount{0};

    enhancedFuture.onComplete([&callCount](int) { callCount++; });
    enhancedFuture.onComplete([&callCount](int) { callCount++; });
    enhancedFuture.onComplete([&callCount](int) { callCount++; });

    promise.set_value(42);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(callCount.load(), 3);
}

TEST(EnhancedFutureTest, OnCompleteAfterCancel) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    enhancedFuture.cancel();

    std::atomic<bool> callbackCalled{false};
    enhancedFuture.onComplete(
        [&callbackCalled](int) { callbackCalled = true; });

    promise.set_value(42);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_FALSE(callbackCalled.load());
}

// =============================================================================
// EnhancedFuture<void> Tests
// =============================================================================

TEST(EnhancedFutureVoidTest, Constructor) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));
    EXPECT_FALSE(enhancedFuture.isCancelled());
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        promise.set_value();
    }).detach();

    bool result = enhancedFuture.waitFor(std::chrono::milliseconds(100));
    EXPECT_TRUE(result);
}

TEST(EnhancedFutureVoidTest, WaitForTimeout) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    bool result = enhancedFuture.waitFor(std::chrono::milliseconds(10));
    EXPECT_FALSE(result);
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureVoidTest, IsDone) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    EXPECT_FALSE(enhancedFuture.isDone());
    promise.set_value();
    EXPECT_TRUE(enhancedFuture.isDone());
}

TEST(EnhancedFutureVoidTest, Cancel) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    enhancedFuture.cancel();
    EXPECT_TRUE(enhancedFuture.isCancelled());
}

TEST(EnhancedFutureVoidTest, OnComplete) {
    std::promise<void> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<void> enhancedFuture(std::move(future));

    std::atomic<bool> callbackCalled{false};
    enhancedFuture.onComplete([&callbackCalled]() { callbackCalled = true; });

    promise.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(callbackCalled.load());
}

// =============================================================================
// makeEnhancedFuture Tests
// =============================================================================

TEST(MakeEnhancedFutureTest, WithLambda) {
    auto future = makeEnhancedFuture([]() { return std::string("Hello"); });
    EXPECT_EQ(future.wait(), "Hello");
}

TEST(MakeEnhancedFutureTest, WithParameters) {
    auto future =
        makeEnhancedFuture([](int a, int b) { return a + b; }, 10, 20);
    EXPECT_EQ(future.wait(), 30);
}

TEST(MakeEnhancedFutureTest, VoidReturn) {
    std::atomic<bool> executed{false};
    auto future = makeEnhancedFuture([&executed]() { executed = true; });
    future.wait();
    EXPECT_TRUE(executed.load());
}

TEST(MakeEnhancedFutureTest, WithException) {
    auto future = makeEnhancedFuture(
        []() -> int { throw std::runtime_error("Test error"); });

    EXPECT_THROW(future.wait(), std::exception);
}

// =============================================================================
// WhenAny Tests
// =============================================================================

TEST(WhenAnyTest, FirstCompletes) {
    auto fastFuture = std::async(std::launch::async, []() { return 1; });
    auto slowFuture = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 2;
    });

    auto future1 = EnhancedFuture<int>(fastFuture.share());
    auto future2 = EnhancedFuture<int>(slowFuture.share());

    auto result = whenAny(std::move(future1), std::move(future2));
    auto [index, value] = result.get();

    EXPECT_EQ(index, 0u);  // First future should complete first
    EXPECT_EQ(value, 1);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST(EnhancedFutureTest, ConcurrentOnComplete) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::atomic<int> callCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&enhancedFuture, &callCount]() {
            enhancedFuture.onComplete([&callCount](int) { callCount++; });
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    promise.set_value(42);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(callCount.load(), 10);
}

TEST(EnhancedFutureTest, ConcurrentCancel) {
    std::promise<int> promise;
    auto future = promise.get_future().share();
    EnhancedFuture<int> enhancedFuture(std::move(future));

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&enhancedFuture]() { enhancedFuture.cancel(); });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(enhancedFuture.isCancelled());
}
