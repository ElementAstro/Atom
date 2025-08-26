/*
 * test_fixtures.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Common test fixtures for Atom Async tests
Provides base test fixtures and specialized fixtures for different component types.

**************************************************/

#ifndef ATOM_ASYNC_TEST_FIXTURES_HPP
#define ATOM_ASYNC_TEST_FIXTURES_HPP

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

#include "test_utils.hpp"

namespace atom::async::test {

// ============================================================================
// Base Test Fixture
// ============================================================================

/**
 * @brief Base test fixture for all async tests
 */
class AsyncTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all async tests
        setupStartTime_ = std::chrono::steady_clock::now();
        resourceTracker_ = std::make_unique<ResourceTracker>();
    }

    void TearDown() override {
        // Common cleanup for all async tests
        threadManager_.joinAll();

        // Check for resource leaks
        if (resourceTracker_) {
            resourceTracker_->expectNoLeaks();
        }

        // Log test duration
        auto duration = std::chrono::steady_clock::now() - setupStartTime_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        if (ms.count() > 1000) { // Log if test takes more than 1 second
            std::cout << "[SLOW TEST] " << ::testing::UnitTest::GetInstance()
                         ->current_test_info()->name()
                      << " took " << ms.count() << "ms" << std::endl;
        }
    }

    // Helper methods available to all test fixtures
    template<typename Func>
    void addTestThread(Func&& func) {
        threadManager_.addThread(std::forward<Func>(func));
    }

    void joinAllThreads() {
        threadManager_.joinAll();
    }

    ResourceTracker& getResourceTracker() {
        return *resourceTracker_;
    }

    TimingHelper createTimer() {
        return TimingHelper{};
    }

    template<typename Duration>
    void expectTimingRange(const Duration& actual,
                          const Duration& min_duration,
                          const Duration& max_duration) {
        EXPECT_GE(actual, min_duration)
            << "Duration " << actual.count() << " is less than minimum "
            << min_duration.count();
        EXPECT_LE(actual, max_duration)
            << "Duration " << actual.count() << " is greater than maximum "
            << max_duration.count();
    }

private:
    std::chrono::steady_clock::time_point setupStartTime_;
    ThreadManager threadManager_;
    std::unique_ptr<ResourceTracker> resourceTracker_;
};

// ============================================================================
// Threading Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for threading-related tests
 */
class ThreadingTestFixture : public AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();

        // Threading-specific setup
        maxThreads_ = std::thread::hardware_concurrency();
        if (maxThreads_ == 0) maxThreads_ = 4; // Fallback
    }

    void TearDown() override {
        // Threading-specific cleanup
        AsyncTestBase::TearDown();
    }

    size_t getMaxThreads() const {
        return maxThreads_;
    }

    template<typename Func>
    void runConcurrentTest(size_t numThreads, Func&& func) {
        atom::async::test::runConcurrentTest(numThreads, std::forward<Func>(func));
    }

    template<typename Func>
    void runStressTest(size_t numThreads, size_t operationsPerThread, Func&& func) {
        atom::async::test::runStressTest(numThreads, operationsPerThread, std::forward<Func>(func));
    }

private:
    size_t maxThreads_;
};

// ============================================================================
// Synchronization Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for synchronization primitive tests
 */
class SynchronizationTestFixture : public ThreadingTestFixture {
protected:
    void SetUp() override {
        ThreadingTestFixture::SetUp();

        // Synchronization-specific setup
        defaultTimeout_ = std::chrono::milliseconds(1000);
    }

    std::chrono::milliseconds getDefaultTimeout() const {
        return defaultTimeout_;
    }

    template<typename SyncPrimitive, typename Func>
    void testBasicSynchronization(SyncPrimitive& sync, Func&& func) {
        std::atomic<int> counter{0};
        std::atomic<bool> ready{false};

        const size_t numThreads = 10;
        const size_t incrementsPerThread = 100;

        for (size_t i = 0; i < numThreads; ++i) {
            addTestThread([&sync, &counter, &ready, incrementsPerThread, func = std::forward<Func>(func)]() {
                while (!ready.load()) {
                    std::this_thread::yield();
                }

                for (size_t j = 0; j < incrementsPerThread; ++j) {
                    func(sync, counter);
                }
            });
        }

        ready.store(true);
        joinAllThreads();

        EXPECT_EQ(counter.load(), numThreads * incrementsPerThread);
    }

private:
    std::chrono::milliseconds defaultTimeout_;
};

// ============================================================================
// Timer Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for timer-related tests
 */
class TimerTestFixture : public AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();

        // Timer-specific setup
        defaultTimerDelay_ = std::chrono::milliseconds(50);
        timerTolerance_ = std::chrono::milliseconds(20);
    }

    std::chrono::milliseconds getDefaultDelay() const {
        return defaultTimerDelay_;
    }

    std::chrono::milliseconds getTolerance() const {
        return timerTolerance_;
    }

    template<typename Duration>
    void expectTimerAccuracy(const Duration& actual, const Duration& expected) {
        auto tolerance = std::chrono::duration_cast<Duration>(timerTolerance_);
        expectTimingRange(actual, expected - tolerance, expected + tolerance);
    }

private:
    std::chrono::milliseconds defaultTimerDelay_;
    std::chrono::milliseconds timerTolerance_;
};

// ============================================================================
// Async Execution Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for async execution tests
 */
class AsyncExecutionTestFixture : public ThreadingTestFixture {
protected:
    void SetUp() override {
        ThreadingTestFixture::SetUp();

        // Async execution specific setup
        defaultExecutionTimeout_ = std::chrono::seconds(5);
    }

    std::chrono::seconds getDefaultExecutionTimeout() const {
        return defaultExecutionTimeout_;
    }

    template<typename Future>
    auto waitForFuture(Future& future) -> decltype(future.get()) {
        auto status = future.wait_for(defaultExecutionTimeout_);
        EXPECT_EQ(status, std::future_status::ready)
            << "Future did not complete within timeout";
        return future.get();
    }

    template<typename Future>
    bool waitForFutureWithTimeout(Future& future, std::chrono::milliseconds timeout) {
        auto status = future.wait_for(timeout);
        return status == std::future_status::ready;
    }

private:
    std::chrono::seconds defaultExecutionTimeout_;
};

// ============================================================================
// Message Passing Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for message passing tests
 */
class MessagePassingTestFixture : public ThreadingTestFixture {
protected:
    void SetUp() override {
        ThreadingTestFixture::SetUp();

        // Message passing specific setup
        messageTimeout_ = std::chrono::milliseconds(500);
    }

    std::chrono::milliseconds getMessageTimeout() const {
        return messageTimeout_;
    }

    template<typename MessageQueue, typename Message>
    void testProducerConsumerPattern(MessageQueue& queue,
                                   const std::vector<Message>& messages) {
        std::vector<Message> receivedMessages;
        std::mutex receivedMutex;
        std::atomic<bool> producerDone{false};

        // Consumer thread
        addTestThread([&queue, &receivedMessages, &receivedMutex, &producerDone, this]() {
            while (!producerDone.load() || !queue.empty()) {
                Message msg;
                if (queue.tryReceive(msg, messageTimeout_)) {
                    std::lock_guard<std::mutex> lock(receivedMutex);
                    receivedMessages.push_back(std::move(msg));
                }
            }
        });

        // Producer thread
        addTestThread([&queue, &messages, &producerDone]() {
            for (const auto& msg : messages) {
                queue.send(msg);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            producerDone.store(true);
        });

        joinAllThreads();

        EXPECT_EQ(receivedMessages.size(), messages.size());

        // Verify all messages were received (order may vary)
        std::sort(receivedMessages.begin(), receivedMessages.end());
        auto sortedOriginal = messages;
        std::sort(sortedOriginal.begin(), sortedOriginal.end());
        EXPECT_EQ(receivedMessages, sortedOriginal);
    }

private:
    std::chrono::milliseconds messageTimeout_;
};

// ============================================================================
// Performance Test Fixture
// ============================================================================

/**
 * @brief Specialized fixture for performance tests
 */
class PerformanceTestFixture : public AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();

        // Performance test specific setup
        performanceTimer_ = std::make_unique<TimingHelper>();
    }

    void startPerformanceTimer() {
        performanceTimer_->reset();
    }

    TimingHelper::Duration getPerformanceTime() const {
        return performanceTimer_->elapsed();
    }

    template<typename Func>
    TimingHelper::Duration measurePerformance(Func&& func) {
        TimingHelper timer;
        func();
        return timer.elapsed();
    }

    void expectPerformanceWithin(TimingHelper::Duration maxDuration) {
        auto actual = getPerformanceTime();
        EXPECT_LE(actual, maxDuration)
            << "Performance test took " << actual.count()
            << " microseconds, expected at most " << maxDuration.count();
    }

private:
    std::unique_ptr<TimingHelper> performanceTimer_;
};

}  // namespace atom::async::test

#endif  // ATOM_ASYNC_TEST_FIXTURES_HPP
