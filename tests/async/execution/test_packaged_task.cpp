#include <gtest/gtest.h>
#include <stdexcept>

#include "atom/async/packaged_task.hpp"

using namespace atom::async;

TEST(EnhancedPackagedTaskTest, Initialization) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    auto future = task.getEnhancedFuture();
    EXPECT_FALSE(future.isReady());
}

TEST(EnhancedPackagedTaskTest, Execution) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    auto future = task.getEnhancedFuture();
    task(5);
    EXPECT_EQ(future.get(), 10);
}

TEST(EnhancedPackagedTaskTest, VoidExecution) {
    bool executed = false;
    EnhancedPackagedTask<void> task([&executed]() { executed = true; });
    auto future = task.getEnhancedFuture();
    task();
    future.get();  // Ensure the task has completed
    EXPECT_TRUE(executed);
}

TEST(EnhancedPackagedTaskTest, Callbacks) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    bool callbackCalled = false;
    task.onComplete([&callbackCalled](int result) {
        callbackCalled = true;
        EXPECT_EQ(result, 10);
    });
    task(5);
    EXPECT_TRUE(callbackCalled);
}

TEST(EnhancedPackagedTaskTest, VoidCallbacks) {
    EnhancedPackagedTask<void> task([]() {});
    bool callbackCalled = false;
    task.onComplete([&callbackCalled]() { callbackCalled = true; });
    task();
    EXPECT_TRUE(callbackCalled);
}

TEST(EnhancedPackagedTaskTest, Cancellation) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    task.cancel();
    auto future = task.getEnhancedFuture();
    task(5);
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_TRUE(task.isCancelled());
}

TEST(EnhancedPackagedTaskTest, VoidCancellation) {
    EnhancedPackagedTask<void> task([]() {});
    task.cancel();
    auto future = task.getEnhancedFuture();
    task();
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_TRUE(task.isCancelled());
}

TEST(EnhancedPackagedTaskTest, ExceptionHandling) {
    EnhancedPackagedTask<int, int> task(
        [](int) -> int { throw std::runtime_error("error"); });
    auto future = task.getEnhancedFuture();
    task(5);
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(EnhancedPackagedTaskTest, VoidExceptionHandling) {
    EnhancedPackagedTask<void> task(
        []() { throw std::runtime_error("error"); });
    auto future = task.getEnhancedFuture();
    task();
    EXPECT_THROW(future.get(), std::runtime_error);
}

// =============================================================================
// Additional EnhancedPackagedTask Tests
// =============================================================================

TEST(EnhancedPackagedTaskTest, MultipleArguments) {
    EnhancedPackagedTask<int, int, int, int> task(
        [](int a, int b, int c) { return a + b + c; });
    auto future = task.getEnhancedFuture();
    task(1, 2, 3);
    EXPECT_EQ(future.get(), 6);
}

TEST(EnhancedPackagedTaskTest, StringResult) {
    EnhancedPackagedTask<std::string, std::string, std::string> task(
        [](const std::string& a, const std::string& b) { return a + b; });
    auto future = task.getEnhancedFuture();
    task("Hello, ", "World!");
    EXPECT_EQ(future.get(), "Hello, World!");
}

TEST(EnhancedPackagedTaskTest, VectorResult) {
    EnhancedPackagedTask<std::vector<int>, int> task([](int n) {
        std::vector<int> result;
        for (int i = 0; i < n; ++i) {
            result.push_back(i);
        }
        return result;
    });
    auto future = task.getEnhancedFuture();
    task(5);
    auto result = future.get();
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[4], 4);
}

TEST(EnhancedPackagedTaskTest, MultipleCallbacks) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    std::atomic<int> callbackCount{0};

    task.onComplete([&callbackCount](int) { callbackCount++; });
    task.onComplete([&callbackCount](int) { callbackCount++; });
    task.onComplete([&callbackCount](int) { callbackCount++; });

    task(5);

    // Give callbacks time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(callbackCount.load(), 3);
}

TEST(EnhancedPackagedTaskTest, CallbackWithException) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    std::atomic<bool> callbackCalled{false};

    task.onComplete([&callbackCalled](int) {
        callbackCalled = true;
        throw std::runtime_error("Callback exception");
    });

    // Task execution should not throw even if callback throws
    EXPECT_NO_THROW(task(5));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(callbackCalled.load());
}

TEST(EnhancedPackagedTaskTest, CancelBeforeExecution) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });

    task.cancel();
    EXPECT_TRUE(task.isCancelled());

    auto future = task.getEnhancedFuture();
    task(5);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(EnhancedPackagedTaskTest, CancelAfterExecution) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    auto future = task.getEnhancedFuture();

    task(5);

    // Cancelling after execution should have no effect on result
    task.cancel();

    EXPECT_EQ(future.get(), 10);
}

TEST(EnhancedPackagedTaskTest, MoveSemantics) {
    EnhancedPackagedTask<int, int> task1([](int x) { return x * 2; });

    EnhancedPackagedTask<int, int> task2 = std::move(task1);

    auto future = task2.getEnhancedFuture();
    task2(5);

    EXPECT_EQ(future.get(), 10);
}

TEST(EnhancedPackagedTaskTest, LambdaWithCapture) {
    int multiplier = 3;
    EnhancedPackagedTask<int, int> task(
        [multiplier](int x) { return x * multiplier; });
    auto future = task.getEnhancedFuture();
    task(5);
    EXPECT_EQ(future.get(), 15);
}

TEST(EnhancedPackagedTaskTest, ReferenceCapture) {
    std::atomic<int> counter{0};
    EnhancedPackagedTask<void> task([&counter]() { counter.fetch_add(1); });
    auto future = task.getEnhancedFuture();
    task();
    future.get();
    EXPECT_EQ(counter.load(), 1);
}

TEST(EnhancedPackagedTaskTest, DoubleResult) {
    EnhancedPackagedTask<double, double, double> task(
        [](double a, double b) { return a / b; });
    auto future = task.getEnhancedFuture();
    task(10.0, 4.0);
    EXPECT_NEAR(future.get(), 2.5, 1e-10);
}

TEST(EnhancedPackagedTaskTest, VoidWithMultipleArgs) {
    std::atomic<int> sum{0};
    EnhancedPackagedTask<void, int, int, int> task(
        [&sum](int a, int b, int c) { sum = a + b + c; });
    auto future = task.getEnhancedFuture();
    task(1, 2, 3);
    future.get();
    EXPECT_EQ(sum.load(), 6);
}

TEST(EnhancedPackagedTaskTest, ConcurrentExecution) {
    std::vector<std::thread> threads;
    std::atomic<int> completedCount{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&completedCount, i]() {
            EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
            auto future = task.getEnhancedFuture();
            task(i);
            EXPECT_EQ(future.get(), i * 2);
            completedCount.fetch_add(1);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completedCount.load(), 10);
}

TEST(EnhancedPackagedTaskTest, FutureIsReadyAfterExecution) {
    EnhancedPackagedTask<int, int> task([](int x) { return x * 2; });
    auto future = task.getEnhancedFuture();

    EXPECT_FALSE(future.isReady());

    task(5);

    // Give time for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(future.isReady());
    EXPECT_EQ(future.get(), 10);
}

TEST(EnhancedPackagedTaskTest, CustomExceptionType) {
    class CustomException : public std::exception {
    public:
        const char* what() const noexcept override { return "Custom error"; }
    };

    EnhancedPackagedTask<int, int> task(
        [](int) -> int { throw CustomException(); });
    auto future = task.getEnhancedFuture();
    task(5);

    EXPECT_THROW(future.get(), CustomException);
}
