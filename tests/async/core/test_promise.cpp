#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
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
    EXPECT_THROW(
        promise.setException(std::make_exception_ptr(std::runtime_error("second"))),
        PromiseCancelledException
    );
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
    EXPECT_THROW(
        promise.setException(std::make_exception_ptr(std::runtime_error("error"))),
        PromiseCancelledException
    );
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

    EXPECT_THROW(
        promise.setException(std::make_exception_ptr(std::runtime_error("error"))),
        PromiseCancelledException
    );
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

}  // namespace atom::async::test
