#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "atom/async/promise.hpp"

using namespace std::chrono_literals;

namespace atom::async::test {

class EnhancedPromiseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, Initialization) {
    Promise<int> promise;
    EXPECT_FALSE(promise.isCancelled());
}

TEST_F(EnhancedPromiseTest, SetValue) {
    Promise<int> promise;
    auto future = promise.getFuture();
    promise.setValue(42);
    EXPECT_EQ(future.get(), 42);
}

TEST_F(EnhancedPromiseTest, SetException) {
    Promise<int> promise;
    auto future = promise.getFuture();
    promise.setException(std::make_exception_ptr(std::runtime_error("error")));
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(EnhancedPromiseTest, Callbacks) {
    Promise<int> promise;
    bool callbackCalled = false;
    promise.onComplete([&callbackCalled](int value) {
        callbackCalled = true;
        EXPECT_EQ(value, 42);
    });
    promise.setValue(42);
    EXPECT_TRUE(callbackCalled);
}

TEST_F(EnhancedPromiseTest, Cancellation) {
    Promise<int> promise;
    promise.cancel();
    EXPECT_TRUE(promise.isCancelled());
    EXPECT_THROW(promise.setValue(42), PromiseCancelledException);
}

TEST_F(EnhancedPromiseTest, VoidInitialization) {
    Promise<void> promise;
    EXPECT_FALSE(promise.isCancelled());
}

TEST_F(EnhancedPromiseTest, VoidSetValue) {
    Promise<void> promise;
    auto future = promise.getFuture();
    promise.setValue();
    future.get();  // Should not throw
}

TEST_F(EnhancedPromiseTest, VoidSetException) {
    Promise<void> promise;
    auto future = promise.getFuture();
    promise.setException(std::make_exception_ptr(std::runtime_error("error")));
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(EnhancedPromiseTest, VoidCallbacks) {
    Promise<void> promise;
    bool callbackCalled = false;
    promise.onComplete([&callbackCalled]() { callbackCalled = true; });
    promise.setValue();
    EXPECT_TRUE(callbackCalled);
}

TEST_F(EnhancedPromiseTest, VoidCancellation) {
    Promise<void> promise;
    promise.cancel();
    EXPECT_TRUE(promise.isCancelled());
    EXPECT_THROW(promise.setValue(), PromiseCancelledException);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, MoveConstructor) {
    Promise<int> promise1;
    auto future1 = promise1.getFuture();

    Promise<int> promise2(std::move(promise1));
    promise2.setValue(42);

    EXPECT_EQ(future1.get(), 42);
}

TEST_F(EnhancedPromiseTest, MoveAssignment) {
    Promise<int> promise1;
    auto future1 = promise1.getFuture();

    Promise<int> promise2;
    promise2 = std::move(promise1);
    promise2.setValue(42);

    EXPECT_EQ(future1.get(), 42);
}

TEST_F(EnhancedPromiseTest, VoidMoveConstructor) {
    Promise<void> promise1;
    auto future1 = promise1.getFuture();

    Promise<void> promise2(std::move(promise1));
    promise2.setValue();

    EXPECT_NO_THROW(future1.get());
}

TEST_F(EnhancedPromiseTest, VoidMoveAssignment) {
    Promise<void> promise1;
    auto future1 = promise1.getFuture();

    Promise<void> promise2;
    promise2 = std::move(promise1);
    promise2.setValue();

    EXPECT_NO_THROW(future1.get());
}

// ============================================================================
// Multiple Callbacks Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, MultipleCallbacks) {
    Promise<int> promise;
    std::atomic<int> callbackCount{0};

    promise.onComplete([&callbackCount](int value) {
        callbackCount++;
        EXPECT_EQ(value, 42);
    });

    promise.onComplete([&callbackCount](int value) {
        callbackCount++;
        EXPECT_EQ(value, 42);
    });

    promise.onComplete([&callbackCount](int value) {
        callbackCount++;
        EXPECT_EQ(value, 42);
    });

    promise.setValue(42);

    // Give callbacks time to execute
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(callbackCount.load(), 3);
}

TEST_F(EnhancedPromiseTest, VoidMultipleCallbacks) {
    Promise<void> promise;
    std::atomic<int> callbackCount{0};

    promise.onComplete([&callbackCount]() { callbackCount++; });
    promise.onComplete([&callbackCount]() { callbackCount++; });
    promise.onComplete([&callbackCount]() { callbackCount++; });

    promise.setValue();

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(callbackCount.load(), 3);
}

// ============================================================================
// Exception Handling Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, SetExceptionWithNullptr) {
    Promise<int> promise;
    auto future = promise.getFuture();

    // Setting null exception should be handled gracefully
    promise.setException(nullptr);

    EXPECT_THROW(future.get(), std::invalid_argument);
}

TEST_F(EnhancedPromiseTest, DoubleSetValue) {
    Promise<int> promise;
    promise.setValue(42);

    // Second setValue should throw
    EXPECT_THROW(promise.setValue(100), PromiseCancelledException);
}

TEST_F(EnhancedPromiseTest, DoubleSetException) {
    Promise<int> promise;
    promise.setException(std::make_exception_ptr(std::runtime_error("first")));

    // Second setException should throw
    EXPECT_THROW(promise.setException(
                     std::make_exception_ptr(std::runtime_error("second"))),
                 PromiseCancelledException);
}

TEST_F(EnhancedPromiseTest, SetValueAfterException) {
    Promise<int> promise;
    promise.setException(std::make_exception_ptr(std::runtime_error("error")));

    // setValue after setException should throw
    EXPECT_THROW(promise.setValue(42), PromiseCancelledException);
}

TEST_F(EnhancedPromiseTest, SetExceptionAfterValue) {
    Promise<int> promise;
    promise.setValue(42);

    // setException after setValue should throw
    EXPECT_THROW(promise.setException(
                     std::make_exception_ptr(std::runtime_error("error"))),
                 PromiseCancelledException);
}

// ============================================================================
// Cancellation Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, CancelBeforeCallback) {
    Promise<int> promise;
    promise.cancel();

    bool callbackCalled = false;
    promise.onComplete([&callbackCalled](int) { callbackCalled = true; });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(callbackCalled);
}

TEST_F(EnhancedPromiseTest, CancelReturnValue) {
    Promise<int> promise;

    // First cancel should return true
    EXPECT_TRUE(promise.cancel());

    // Second cancel should return false
    EXPECT_FALSE(promise.cancel());
}

TEST_F(EnhancedPromiseTest, SetExceptionAfterCancel) {
    Promise<int> promise;
    promise.cancel();

    EXPECT_THROW(promise.setException(
                     std::make_exception_ptr(std::runtime_error("error"))),
                 PromiseCancelledException);
}

// ============================================================================
// EnhancedFuture Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, GetEnhancedFuture) {
    Promise<int> promise;
    auto enhancedFuture = promise.getEnhancedFuture();

    promise.setValue(42);
    EXPECT_EQ(enhancedFuture.wait(), 42);
}

TEST_F(EnhancedPromiseTest, VoidGetEnhancedFuture) {
    Promise<void> promise;
    auto enhancedFuture = promise.getEnhancedFuture();

    promise.setValue();
    EXPECT_NO_THROW(enhancedFuture.wait());
}

// ============================================================================
// Stop Token and Cancellation Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, SetCancellableWithStopToken) {
    Promise<int> promise;
    std::stop_source stopSource;

    promise.setCancellable(stopSource.get_token());

    // Request stop
    stopSource.request_stop();

    // Give time for cancellation to propagate
    std::this_thread::sleep_for(50ms);

    EXPECT_TRUE(promise.isCancelled());
}

TEST_F(EnhancedPromiseTest, VoidSetCancellableWithStopToken) {
    Promise<void> promise;
    std::stop_source stopSource;

    promise.setCancellable(stopSource.get_token());

    stopSource.request_stop();
    std::this_thread::sleep_for(50ms);

    EXPECT_TRUE(promise.isCancelled());
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, ConcurrentCallbacks) {
    Promise<int> promise;
    std::atomic<int> callbackCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&promise, &callbackCount]() {
            promise.onComplete(
                [&callbackCount](int) { callbackCount.fetch_add(1); });
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    promise.setValue(42);
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(callbackCount.load(), 10);
}

TEST_F(EnhancedPromiseTest, ConcurrentCancellation) {
    Promise<int> promise;
    std::atomic<int> cancelCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&promise, &cancelCount]() {
            if (promise.cancel()) {
                cancelCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(promise.isCancelled());
    EXPECT_EQ(cancelCount.load(), 1);  // Only one thread should succeed
}

// ============================================================================
// PromiseAwaiter Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, GetAwaiter) {
    Promise<int> promise;
    auto awaiter = promise.getAwaiter();

    EXPECT_FALSE(awaiter.await_ready());

    promise.setValue(42);

    // After setting value, awaiter should be ready
    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_EQ(awaiter.await_resume(), 42);
}

TEST_F(EnhancedPromiseTest, VoidGetAwaiter) {
    Promise<void> promise;
    auto awaiter = promise.getAwaiter();

    EXPECT_FALSE(awaiter.await_ready());

    promise.setValue();

    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_resume());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, CallbackWithException) {
    Promise<int> promise;
    std::atomic<bool> exceptionThrown{false};

    promise.onComplete([&exceptionThrown](int) {
        exceptionThrown = true;
        throw std::runtime_error("Callback exception");
    });

    // Setting value should not throw even if callback throws
    EXPECT_NO_THROW(promise.setValue(42));

    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(exceptionThrown.load());
}

TEST_F(EnhancedPromiseTest, VoidCallbackWithException) {
    Promise<void> promise;
    std::atomic<bool> exceptionThrown{false};

    promise.onComplete([&exceptionThrown]() {
        exceptionThrown = true;
        throw std::runtime_error("Callback exception");
    });

    EXPECT_NO_THROW(promise.setValue());

    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(exceptionThrown.load());
}

TEST_F(EnhancedPromiseTest, SetValueWithDifferentTypes) {
    // Test with string
    {
        Promise<std::string> promise;
        auto future = promise.getFuture();
        promise.setValue("Hello, World!");
        EXPECT_EQ(future.get(), "Hello, World!");
    }

    // Test with vector
    {
        Promise<std::vector<int>> promise;
        auto future = promise.getFuture();
        promise.setValue(std::vector<int>{1, 2, 3, 4, 5});
        auto result = future.get();
        EXPECT_EQ(result.size(), 5u);
        EXPECT_EQ(result[0], 1);
    }

    // Test with double
    {
        Promise<double> promise;
        auto future = promise.getFuture();
        promise.setValue(3.14159);
        EXPECT_NEAR(future.get(), 3.14159, 1e-5);
    }
}

TEST_F(EnhancedPromiseTest, SetValueFromRvalue) {
    Promise<std::string> promise;
    auto future = promise.getFuture();

    std::string value = "Test String";
    promise.setValue(std::move(value));

    EXPECT_EQ(future.get(), "Test String");
}

TEST_F(EnhancedPromiseTest, SetValueFromLvalue) {
    Promise<int> promise;
    auto future = promise.getFuture();

    int value = 42;
    promise.setValue(value);

    EXPECT_EQ(future.get(), 42);
}

TEST_F(EnhancedPromiseTest, CustomExceptionType) {
    class CustomException : public std::exception {
    public:
        const char* what() const noexcept override {
            return "Custom exception";
        }
    };

    Promise<int> promise;
    auto future = promise.getFuture();

    promise.setException(std::make_exception_ptr(CustomException{}));

    EXPECT_THROW(future.get(), CustomException);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, ManyCallbacks) {
    Promise<int> promise;
    std::atomic<int> callbackCount{0};

    const int numCallbacks = 100;
    for (int i = 0; i < numCallbacks; ++i) {
        promise.onComplete(
            [&callbackCount](int) { callbackCount.fetch_add(1); });
    }

    promise.setValue(42);
    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(callbackCount.load(), numCallbacks);
}

TEST_F(EnhancedPromiseTest, RapidSetValueCalls) {
    // Test that rapid promise creation and value setting works correctly
    for (int i = 0; i < 100; ++i) {
        Promise<int> promise;
        auto future = promise.getFuture();
        promise.setValue(i);
        EXPECT_EQ(future.get(), i);
    }
}

// ============================================================================
// Memory Safety Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, PromiseDestroyedBeforeValue) {
    std::shared_future<int> future;

    {
        Promise<int> promise;
        future = promise.getFuture();
        // Promise destroyed without setting value
    }

    // Future should throw when trying to get value
    EXPECT_THROW(future.get(), PromiseCancelledException);
}

TEST_F(EnhancedPromiseTest, VoidPromiseDestroyedBeforeValue) {
    std::shared_future<void> future;

    {
        Promise<void> promise;
        future = promise.getFuture();
    }

    EXPECT_THROW(future.get(), PromiseCancelledException);
}

// ============================================================================
// RunAsync Tests
// ============================================================================

TEST_F(EnhancedPromiseTest, RunAsyncBasic) {
    Promise<int> promise;
    auto future = promise.getFuture();

    promise.runAsync(
        [](Promise<int>* p) {
            std::this_thread::sleep_for(10ms);
            p->setValue(42);
        },
        &promise);

    // Note: This test may need adjustment based on actual runAsync
    // implementation The promise should eventually be set
}

}  // namespace atom::async::test
