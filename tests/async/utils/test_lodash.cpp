/*
 * test_lodash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Lodash Utilities
Tests debounce, throttle, and functional utility patterns with edge cases and concurrent access.

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "atom/async/utils/lodash.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::utils::test {

// ============================================================================
// Lodash Tests
// ============================================================================

class LodashTest : public atom::async::test::AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();
        callCount = 0;
        lastCallValue = 0;
    }

    void TearDown() override {
        AsyncTestBase::TearDown();
    }

    // Helper variables for testing
    std::atomic<int> callCount{0};
    std::atomic<int> lastCallValue{0};

    // Helper functions for testing
    void incrementCounter() {
        callCount.fetch_add(1);
    }

    void setLastValue(int value) {
        lastCallValue.store(value);
        callCount.fetch_add(1);
    }

    int multiplyBy2(int value) {
        callCount.fetch_add(1);
        return value * 2;
    }
};

// ============================================================================
// Debounce Tests
// ============================================================================

// Test basic debounce functionality
TEST_F(LodashTest, DebounceBasicFunctionality) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 100ms);

    // Call multiple times quickly
    debounced();
    debounced();
    debounced();

    // Should not have been called yet
    EXPECT_EQ(callCount.load(), 0);

    // Wait for debounce delay
    std::this_thread::sleep_for(150ms);

    // Should have been called once
    EXPECT_EQ(callCount.load(), 1);
}

// Test debounce with arguments
TEST_F(LodashTest, DebounceWithArguments) {
    auto debounced = Debounce([this](int value) { setLastValue(value); }, 100ms);

    debounced(10);
    debounced(20);
    debounced(30);

    // Wait for debounce delay
    std::this_thread::sleep_for(150ms);

    // Should have been called once with the last value
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_EQ(lastCallValue.load(), 30);
}

// Test debounce leading edge
TEST_F(LodashTest, DebounceLeadingEdge) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 100ms, true);

    // First call should execute immediately
    debounced();
    EXPECT_EQ(callCount.load(), 1);

    // Subsequent calls should be debounced
    debounced();
    debounced();

    // Should still be 1
    EXPECT_EQ(callCount.load(), 1);

    // Wait for debounce delay
    std::this_thread::sleep_for(150ms);

    // Should still be 1 (no trailing call)
    EXPECT_EQ(callCount.load(), 1);
}

// Test debounce with maxWait
TEST_F(LodashTest, DebounceWithMaxWait) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 200ms, false, 100ms);

    // Call repeatedly to trigger maxWait
    for (int i = 0; i < 10; ++i) {
        debounced();
        std::this_thread::sleep_for(50ms);
    }

    // Should have been called due to maxWait
    EXPECT_GT(callCount.load(), 0);
}

// Test debounce flush
TEST_F(LodashTest, DebounceFlush) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 200ms);

    debounced();
    debounced();

    // Should not have been called yet
    EXPECT_EQ(callCount.load(), 0);

    // Flush should execute immediately
    debounced.flush();
    EXPECT_EQ(callCount.load(), 1);
}

// Test debounce reset
TEST_F(LodashTest, DebounceReset) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 100ms);

    debounced();
    debounced();

    // Reset should cancel pending calls
    debounced.reset();

    // Wait longer than debounce delay
    std::this_thread::sleep_for(150ms);

    // Should not have been called
    EXPECT_EQ(callCount.load(), 0);
}

// Test debounce call count
TEST_F(LodashTest, DebounceCallCount) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 50ms);

    EXPECT_EQ(debounced.callCount(), 0);

    debounced();
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(debounced.callCount(), 1);
    EXPECT_EQ(callCount.load(), 1);

    debounced();
    debounced();
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(debounced.callCount(), 2);
    EXPECT_EQ(callCount.load(), 2);
}

// ============================================================================
// Throttle Tests
// ============================================================================

// Test basic throttle functionality
TEST_F(LodashTest, ThrottleBasicFunctionality) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 100ms);

    // First call should execute immediately (leading edge)
    throttled();
    EXPECT_EQ(callCount.load(), 1);

    // Subsequent calls should be throttled
    throttled();
    throttled();
    EXPECT_EQ(callCount.load(), 1);

    // Wait for throttle interval
    std::this_thread::sleep_for(150ms);

    // Next call should execute
    throttled();
    EXPECT_EQ(callCount.load(), 2);
}

// Test throttle with arguments
TEST_F(LodashTest, ThrottleWithArguments) {
    auto throttled = Throttle([this](int value) { setLastValue(value); }, 100ms);

    throttled(10);
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_EQ(lastCallValue.load(), 10);

    throttled(20);
    throttled(30);
    EXPECT_EQ(callCount.load(), 1); // Still throttled

    std::this_thread::sleep_for(150ms);

    throttled(40);
    EXPECT_EQ(callCount.load(), 2);
    EXPECT_EQ(lastCallValue.load(), 40);
}

// Test throttle without leading edge
TEST_F(LodashTest, ThrottleNoLeading) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 100ms, false);

    // First call should not execute immediately
    throttled();
    EXPECT_EQ(callCount.load(), 0);

    // Wait for throttle interval
    std::this_thread::sleep_for(150ms);

    // Should execute now
    throttled();
    EXPECT_EQ(callCount.load(), 1);
}

// Test throttle with trailing edge
TEST_F(LodashTest, ThrottleWithTrailing) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 100ms, true, true);

    // First call executes immediately
    throttled();
    EXPECT_EQ(callCount.load(), 1);

    // Multiple calls during throttle period
    throttled();
    throttled();
    throttled();

    // Wait for trailing call
    std::this_thread::sleep_for(150ms);

    // Should have trailing call
    EXPECT_EQ(callCount.load(), 2);
}

// Test throttle cancel
TEST_F(LodashTest, ThrottleCancel) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 100ms, true, true);

    throttled();
    EXPECT_EQ(callCount.load(), 1);

    throttled();
    throttled();

    // Cancel should prevent trailing call
    throttled.cancel();

    std::this_thread::sleep_for(150ms);

    // Should not have trailing call
    EXPECT_EQ(callCount.load(), 1);
}

// Test throttle reset
TEST_F(LodashTest, ThrottleReset) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 100ms);

    throttled();
    EXPECT_EQ(callCount.load(), 1);

    // Reset should allow immediate call
    throttled.reset();
    throttled();
    EXPECT_EQ(callCount.load(), 2);
}

// Test throttle call count
TEST_F(LodashTest, ThrottleCallCount) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 50ms);

    EXPECT_EQ(throttled.callCount(), 0);

    throttled();
    EXPECT_EQ(throttled.callCount(), 1);
    EXPECT_EQ(callCount.load(), 1);

    std::this_thread::sleep_for(100ms);

    throttled();
    EXPECT_EQ(throttled.callCount(), 2);
    EXPECT_EQ(callCount.load(), 2);
}

// ============================================================================
// Factory Tests
// ============================================================================

// Test DebounceFactory
TEST_F(LodashTest, DebounceFactory) {
    DebounceFactory factory(100ms, false);

    auto debounced1 = factory.create([this]() { incrementCounter(); });
    auto debounced2 = factory.create([this](int value) { setLastValue(value); });

    debounced1();
    debounced2(42);

    std::this_thread::sleep_for(150ms);

    EXPECT_EQ(callCount.load(), 2); // Both should have been called
    EXPECT_EQ(lastCallValue.load(), 42);
}

// Test ThrottleFactory
TEST_F(LodashTest, ThrottleFactory) {
    ThrottleFactory factory(100ms, true, false);

    auto throttled1 = factory.create([this]() { incrementCounter(); });
    auto throttled2 = factory.create([this](int value) { setLastValue(value); });

    throttled1();
    throttled2(99);

    EXPECT_EQ(callCount.load(), 2); // Both should execute immediately
    EXPECT_EQ(lastCallValue.load(), 99);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

// Test debounce with negative delay
TEST_F(LodashTest, DebounceNegativeDelay) {
    EXPECT_THROW(
        Debounce([this]() { incrementCounter(); }, std::chrono::milliseconds(-100)),
        std::invalid_argument
    );
}

// Test debounce with negative maxWait
TEST_F(LodashTest, DebounceNegativeMaxWait) {
    EXPECT_THROW(
        Debounce([this]() { incrementCounter(); }, 100ms, false, std::chrono::milliseconds(-50)),
        std::invalid_argument
    );
}

// Test throttle with negative interval
TEST_F(LodashTest, ThrottleNegativeInterval) {
    EXPECT_THROW(
        Throttle([this]() { incrementCounter(); }, std::chrono::milliseconds(-100)),
        std::invalid_argument
    );
}

// Test debounce with zero delay
TEST_F(LodashTest, DebounceZeroDelay) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 0ms);

    debounced();
    debounced();

    // Should execute very quickly
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(callCount.load(), 1);
}

// Test throttle with zero interval
TEST_F(LodashTest, ThrottleZeroInterval) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 0ms);

    throttled();
    throttled();
    throttled();

    // All calls should execute
    EXPECT_EQ(callCount.load(), 3);
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

// Test debounce concurrent access
TEST_F(LodashTest, DebounceConcurrentAccess) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 50ms);

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&debounced]() {
            for (int j = 0; j < 10; ++j) {
                debounced();
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for final debounce
    std::this_thread::sleep_for(100ms);

    // Should have been called at least once
    EXPECT_GT(callCount.load(), 0);
    EXPECT_LT(callCount.load(), numThreads * 10); // But not for every call
}

// Test throttle concurrent access
TEST_F(LodashTest, ThrottleConcurrentAccess) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 50ms);

    std::vector<std::thread> threads;
    const int numThreads = 5;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&throttled]() {
            for (int j = 0; j < 20; ++j) {
                throttled();
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for any trailing calls
    std::this_thread::sleep_for(100ms);

    // Should have been called multiple times but not for every call
    EXPECT_GT(callCount.load(), 1);
    EXPECT_LT(callCount.load(), numThreads * 20);
}

// ============================================================================
// Performance Tests
// ============================================================================

// Test debounce performance
TEST_F(LodashTest, DebouncePerformance) {
    auto debounced = Debounce([this]() { incrementCounter(); }, 10ms);

    const int numCalls = 1000;
    auto timer = createTimer();

    for (int i = 0; i < numCalls; ++i) {
        debounced();
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(), 100000); // Less than 100ms for 1000 calls

    std::cout << "Debounce performance: " << numCalls << " calls in "
              << elapsed.count() << " microseconds" << std::endl;
}

// Test throttle performance
TEST_F(LodashTest, ThrottlePerformance) {
    auto throttled = Throttle([this]() { incrementCounter(); }, 1ms);

    const int numCalls = 1000;
    auto timer = createTimer();

    for (int i = 0; i < numCalls; ++i) {
        throttled();
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(), 100000); // Less than 100ms for 1000 calls

    std::cout << "Throttle performance: " << numCalls << " calls in "
              << elapsed.count() << " microseconds" << std::endl;
}

// ============================================================================
// Resource Management Tests
// ============================================================================

// Test debounce resource cleanup
TEST_F(LodashTest, DebounceResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        auto debounced = Debounce([&tracker]() {
            atom::async::test::ScopedResourceTracker resource(tracker);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }, 50ms);

        debounced();
        std::this_thread::sleep_for(100ms); // Let it execute
    } // Debounce destructor should clean up

    // Give some time for cleanup
    std::this_thread::sleep_for(50ms);
    tracker.expectNoLeaks();
}

// Test throttle resource cleanup
TEST_F(LodashTest, ThrottleResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        auto throttled = Throttle([&tracker]() {
            atom::async::test::ScopedResourceTracker resource(tracker);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }, 50ms, true, true);

        throttled();
        throttled(); // Should trigger trailing call
        std::this_thread::sleep_for(100ms); // Let trailing call execute
    } // Throttle destructor should clean up

    // Give some time for cleanup
    std::this_thread::sleep_for(50ms);
    tracker.expectNoLeaks();
}

// ============================================================================
// Complex Scenarios
// ============================================================================

// Test debounce with exception in function
TEST_F(LodashTest, DebounceExceptionHandling) {
    auto debounced = Debounce([]() {
        throw std::runtime_error("Test exception");
    }, 50ms);

    // Should not throw from operator()
    EXPECT_NO_THROW(debounced());

    // Wait for execution
    std::this_thread::sleep_for(100ms);

    // Should still be able to use debounce after exception
    auto debounced2 = Debounce([this]() { incrementCounter(); }, 50ms);
    debounced2();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(callCount.load(), 1);
}

// Test throttle with exception in function
TEST_F(LodashTest, ThrottleExceptionHandling) {
    auto throttled = Throttle([]() {
        throw std::runtime_error("Test exception");
    }, 50ms);

    // Should not throw from operator()
    EXPECT_NO_THROW(throttled());

    // Should still be able to use throttle after exception
    auto throttled2 = Throttle([this]() { incrementCounter(); }, 50ms);
    throttled2();
    EXPECT_EQ(callCount.load(), 1);
}

}  // namespace atom::async::utils::test
