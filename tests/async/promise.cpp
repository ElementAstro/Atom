#include <gtest/gtest.h>

#include "atom/async/promise.hpp"

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

}  // namespace atom::async::test
