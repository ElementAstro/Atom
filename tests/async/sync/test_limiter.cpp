/*
 * test_limiter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Rate Limiter
Tests rate limiting functionality, concurrent access, edge cases, and platform-specific optimizations.

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>

#include "atom/async/sync/limiter.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::sync::test {

// ============================================================================
// Rate Limiter Tests
// ============================================================================

class RateLimiterTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override {
        SynchronizationTestFixture::SetUp();
        // Additional rate limiter specific setup
    }

    void TearDown() override {
        // Rate limiter specific cleanup
        SynchronizationTestFixture::TearDown();
    }
};

TEST_F(RateLimiterTest, BasicRateLimiting) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test_function", 5, 1s);

    auto start = std::chrono::steady_clock::now();

    // Should allow first 5 requests immediately
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("test_function"));
    }

    // 6th request should be blocked
    EXPECT_FALSE(limiter.tryAcquire("test_function"));

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 100ms); // Should be very fast for first 5
}

TEST_F(RateLimiterTest, WaitForAvailability) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test_function", 2, 1s);

    // Use up the limit
    EXPECT_TRUE(limiter.tryAcquire("test_function"));
    EXPECT_TRUE(limiter.tryAcquire("test_function"));
    EXPECT_FALSE(limiter.tryAcquire("test_function"));

    auto start = std::chrono::steady_clock::now();

    // This should wait until the time window resets
    std::future<bool> future = std::async(std::launch::async, [&limiter]() {
        return limiter.waitForAvailability("test_function", 2s);
    });

    bool result = future.get();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(result);
    EXPECT_GE(elapsed, 900ms); // Should wait close to 1 second
    EXPECT_LT(elapsed, 1200ms); // But not too much longer
}

TEST_F(RateLimiterTest, MultipleFunction) {
    RateLimiter limiter;
    limiter.setFunctionLimit("function1", 3, 1s);
    limiter.setFunctionLimit("function2", 5, 1s);

    // Test function1 limit
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("function1"));
    }
    EXPECT_FALSE(limiter.tryAcquire("function1"));

    // Test function2 limit (should be independent)
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("function2"));
    }
    EXPECT_FALSE(limiter.tryAcquire("function2"));
}

TEST_F(RateLimiterTest, ConcurrentAccess) {
    RateLimiter limiter;
    limiter.setFunctionLimit("concurrent_test", 10, 1s);

    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 20;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &successCount, &failureCount]() {
            if (limiter.tryAcquire("concurrent_test")) {
                successCount.fetch_add(1);
            } else {
                failureCount.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), 10); // Should allow exactly 10
    EXPECT_EQ(failureCount.load(), 10); // Should reject exactly 10
}

TEST_F(RateLimiterTest, TimeWindowReset) {
    RateLimiter limiter;
    limiter.setFunctionLimit("reset_test", 2, 500ms);

    // Use up the limit
    EXPECT_TRUE(limiter.tryAcquire("reset_test"));
    EXPECT_TRUE(limiter.tryAcquire("reset_test"));
    EXPECT_FALSE(limiter.tryAcquire("reset_test"));

    // Wait for time window to reset
    std::this_thread::sleep_for(600ms);

    // Should be able to acquire again
    EXPECT_TRUE(limiter.tryAcquire("reset_test"));
    EXPECT_TRUE(limiter.tryAcquire("reset_test"));
    EXPECT_FALSE(limiter.tryAcquire("reset_test"));
}

TEST_F(RateLimiterTest, GetCurrentUsage) {
    RateLimiter limiter;
    limiter.setFunctionLimit("usage_test", 5, 1s);

    EXPECT_EQ(limiter.getCurrentUsage("usage_test"), 0);

    limiter.tryAcquire("usage_test");
    EXPECT_EQ(limiter.getCurrentUsage("usage_test"), 1);

    limiter.tryAcquire("usage_test");
    limiter.tryAcquire("usage_test");
    EXPECT_EQ(limiter.getCurrentUsage("usage_test"), 3);
}

TEST_F(RateLimiterTest, GetRemainingRequests) {
    RateLimiter limiter;
    limiter.setFunctionLimit("remaining_test", 5, 1s);

    EXPECT_EQ(limiter.getRemainingRequests("remaining_test"), 5);

    limiter.tryAcquire("remaining_test");
    EXPECT_EQ(limiter.getRemainingRequests("remaining_test"), 4);

    limiter.tryAcquire("remaining_test");
    limiter.tryAcquire("remaining_test");
    EXPECT_EQ(limiter.getRemainingRequests("remaining_test"), 2);
}

TEST_F(RateLimiterTest, ResetFunction) {
    RateLimiter limiter;
    limiter.setFunctionLimit("reset_function_test", 2, 1s);

    // Use up the limit
    EXPECT_TRUE(limiter.tryAcquire("reset_function_test"));
    EXPECT_TRUE(limiter.tryAcquire("reset_function_test"));
    EXPECT_FALSE(limiter.tryAcquire("reset_function_test"));

    // Reset the function
    limiter.resetFunction("reset_function_test");

    // Should be able to acquire again immediately
    EXPECT_TRUE(limiter.tryAcquire("reset_function_test"));
    EXPECT_TRUE(limiter.tryAcquire("reset_function_test"));
    EXPECT_FALSE(limiter.tryAcquire("reset_function_test"));
}

TEST_F(RateLimiterTest, RemoveFunction) {
    RateLimiter limiter;
    limiter.setFunctionLimit("remove_test", 2, 1s);

    EXPECT_TRUE(limiter.tryAcquire("remove_test"));

    limiter.removeFunction("remove_test");

    // After removal, function should not be limited
    EXPECT_EQ(limiter.getCurrentUsage("remove_test"), 0);
    EXPECT_EQ(limiter.getRemainingRequests("remove_test"), 0);
}

TEST_F(RateLimiterTest, EdgeCaseZeroLimit) {
    RateLimiter limiter;

    // Setting zero limit should throw
    EXPECT_THROW(limiter.setFunctionLimit("zero_test", 0, 1s), std::invalid_argument);
}

TEST_F(RateLimiterTest, EdgeCaseZeroTimeWindow) {
    RateLimiter limiter;

    // Setting zero time window should throw
    EXPECT_THROW(limiter.setFunctionLimit("zero_time_test", 5, 0s), std::invalid_argument);
}

TEST_F(RateLimiterTest, NonExistentFunction) {
    RateLimiter limiter;

    // Trying to acquire from non-existent function should return false
    EXPECT_FALSE(limiter.tryAcquire("non_existent"));
    EXPECT_EQ(limiter.getCurrentUsage("non_existent"), 0);
    EXPECT_EQ(limiter.getRemainingRequests("non_existent"), 0);
}

TEST_F(RateLimiterTest, BulkFunctionLimits) {
    RateLimiter limiter;

    std::vector<std::pair<std::string_view, RateLimiter::Settings>> settings = {
        {"func1", {3, 1s}},
        {"func2", {5, 2s}},
        {"func3", {10, 500ms}}
    };

    limiter.setFunctionLimits(settings);

    // Test each function's limit
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("func1"));
    }
    EXPECT_FALSE(limiter.tryAcquire("func1"));

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("func2"));
    }
    EXPECT_FALSE(limiter.tryAcquire("func2"));

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("func3"));
    }
    EXPECT_FALSE(limiter.tryAcquire("func3"));
}

TEST_F(RateLimiterTest, HighConcurrencyStressTest) {
    RateLimiter limiter;
    limiter.setFunctionLimit("stress_test", 100, 1s);

    std::atomic<int> totalSuccesses{0};
    std::atomic<int> totalFailures{0};

    std::vector<std::thread> threads;
    const int numThreads = 50;
    const int attemptsPerThread = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &totalSuccesses, &totalFailures, attemptsPerThread]() {
            for (int j = 0; j < attemptsPerThread; ++j) {
                if (limiter.tryAcquire("stress_test")) {
                    totalSuccesses.fetch_add(1);
                } else {
                    totalFailures.fetch_add(1);
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(totalSuccesses.load() + totalFailures.load(), numThreads * attemptsPerThread);
    EXPECT_LE(totalSuccesses.load(), 100); // Should not exceed the limit
    EXPECT_GE(totalSuccesses.load(), 90);  // Should be close to the limit
}

// Test rate limiter with burst capacity
TEST_F(RateLimiterTest, BurstCapacity) {
    RateLimiter limiter;
    limiter.setFunctionLimit("burst_test", 5, 1s, 10); // 5 per second, burst of 10

    // Should allow burst of 10 initially
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("burst_test")) << "Failed at burst request " << i;
    }

    // 11th request should fail
    EXPECT_FALSE(limiter.tryAcquire("burst_test"));

    // After time window, should allow more requests
    std::this_thread::sleep_for(1100ms);

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.tryAcquire("burst_test")) << "Failed at refill request " << i;
    }
}

// Test rate limiter with different time windows
TEST_F(RateLimiterTest, DifferentTimeWindows) {
    RateLimiter limiter;

    // Fast refill rate
    limiter.setFunctionLimit("fast", 2, 100ms);

    // Slow refill rate
    limiter.setFunctionLimit("slow", 2, 1s);

    // Use up both limits
    EXPECT_TRUE(limiter.tryAcquire("fast"));
    EXPECT_TRUE(limiter.tryAcquire("fast"));
    EXPECT_FALSE(limiter.tryAcquire("fast"));

    EXPECT_TRUE(limiter.tryAcquire("slow"));
    EXPECT_TRUE(limiter.tryAcquire("slow"));
    EXPECT_FALSE(limiter.tryAcquire("slow"));

    // Wait for fast to refill
    std::this_thread::sleep_for(150ms);

    EXPECT_TRUE(limiter.tryAcquire("fast")); // Should be available
    EXPECT_FALSE(limiter.tryAcquire("slow")); // Should still be limited
}

// Test rate limiter thread safety with mixed operations
TEST_F(RateLimiterTest, MixedOperationsThreadSafety) {
    RateLimiter limiter;
    limiter.setFunctionLimit("mixed_test", 100, 1s);

    std::atomic<int> acquireSuccesses{0};
    std::atomic<int> acquireFailures{0};
    std::atomic<int> resetCount{0};
    std::atomic<int> removeCount{0};

    const size_t numThreads = getMaxThreads();

    runConcurrentTest(numThreads, [&](size_t threadId) {
        for (int i = 0; i < 50; ++i) {
            switch (threadId % 4) {
                case 0: // Acquire
                    if (limiter.tryAcquire("mixed_test")) {
                        acquireSuccesses.fetch_add(1);
                    } else {
                        acquireFailures.fetch_add(1);
                    }
                    break;

                case 1: // Check usage
                    limiter.getCurrentUsage("mixed_test");
                    limiter.getRemainingRequests("mixed_test");
                    break;

                case 2: // Reset occasionally
                    if (i % 20 == 0) {
                        limiter.resetFunction("mixed_test");
                        resetCount.fetch_add(1);
                    }
                    break;

                case 3: // Re-add function occasionally
                    if (i % 25 == 0) {
                        limiter.removeFunction("mixed_test");
                        limiter.setFunctionLimit("mixed_test", 100, 1s);
                        removeCount.fetch_add(1);
                    }
                    break;
            }

            std::this_thread::yield();
        }
    });

    // Verify operations completed without crashes
    EXPECT_GT(acquireSuccesses.load() + acquireFailures.load(), 0);
    std::cout << "Mixed operations - Successes: " << acquireSuccesses.load()
              << ", Failures: " << acquireFailures.load()
              << ", Resets: " << resetCount.load()
              << ", Removes: " << removeCount.load() << std::endl;
}

// Test rate limiter performance under load
TEST_F(RateLimiterTest, PerformanceUnderLoad) {
    RateLimiter limiter;
    limiter.setFunctionLimit("perf_test", 10000, 1s); // High limit for performance test

    const int numOperations = 1000;
    auto timer = createTimer();

    for (int i = 0; i < numOperations; ++i) {
        limiter.tryAcquire("perf_test");
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(), 100000); // Less than 100ms for 1000 operations

    std::cout << "Rate limiter performance: "
              << elapsed.count() / numOperations << " microseconds per operation" << std::endl;
}

// Test rate limiter with very short time windows
TEST_F(RateLimiterTest, ShortTimeWindows) {
    RateLimiter limiter;
    limiter.setFunctionLimit("short_window", 1, 10ms);

    EXPECT_TRUE(limiter.tryAcquire("short_window"));
    EXPECT_FALSE(limiter.tryAcquire("short_window"));

    // Wait for very short window to reset
    std::this_thread::sleep_for(15ms);

    EXPECT_TRUE(limiter.tryAcquire("short_window"));
    EXPECT_FALSE(limiter.tryAcquire("short_window"));
}

// Test rate limiter with very long time windows
TEST_F(RateLimiterTest, LongTimeWindows) {
    RateLimiter limiter;
    limiter.setFunctionLimit("long_window", 2, 5s);

    EXPECT_TRUE(limiter.tryAcquire("long_window"));
    EXPECT_TRUE(limiter.tryAcquire("long_window"));
    EXPECT_FALSE(limiter.tryAcquire("long_window"));

    // Should still be limited after short wait
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(limiter.tryAcquire("long_window"));
}

// Test rate limiter with function name edge cases
TEST_F(RateLimiterTest, FunctionNameEdgeCases) {
    RateLimiter limiter;

    // Empty string
    EXPECT_THROW(limiter.setFunctionLimit("", 5, 1s), std::invalid_argument);

    // Very long function name
    std::string longName(1000, 'a');
    EXPECT_NO_THROW(limiter.setFunctionLimit(longName, 5, 1s));
    EXPECT_TRUE(limiter.tryAcquire(longName));

    // Special characters
    std::string specialName = "func!@#$%^&*()_+-={}[]|\\:;\"'<>?,./";
    EXPECT_NO_THROW(limiter.setFunctionLimit(specialName, 5, 1s));
    EXPECT_TRUE(limiter.tryAcquire(specialName));
}

// Test rate limiter memory usage with many functions
TEST_F(RateLimiterTest, ManyFunctions) {
    RateLimiter limiter;

    const int numFunctions = 1000;

    // Add many functions
    for (int i = 0; i < numFunctions; ++i) {
        std::string funcName = "func_" + std::to_string(i);
        limiter.setFunctionLimit(funcName, 5, 1s);
    }

    // Test a few of them
    EXPECT_TRUE(limiter.tryAcquire("func_0"));
    EXPECT_TRUE(limiter.tryAcquire("func_500"));
    EXPECT_TRUE(limiter.tryAcquire("func_999"));

    // Remove all functions
    for (int i = 0; i < numFunctions; ++i) {
        std::string funcName = "func_" + std::to_string(i);
        limiter.removeFunction(funcName);
    }
}

// Test rate limiter with concurrent function management
TEST_F(RateLimiterTest, ConcurrentFunctionManagement) {
    RateLimiter limiter;
    std::atomic<int> operationCount{0};

    const size_t numThreads = getMaxThreads();

    runConcurrentTest(numThreads, [&](size_t threadId) {
        std::string funcName = "func_" + std::to_string(threadId);

        // Add function
        limiter.setFunctionLimit(funcName, 10, 1s);
        operationCount.fetch_add(1);

        // Use function
        for (int i = 0; i < 5; ++i) {
            limiter.tryAcquire(funcName);
            operationCount.fetch_add(1);
        }

        // Reset function
        limiter.resetFunction(funcName);
        operationCount.fetch_add(1);

        // Remove function
        limiter.removeFunction(funcName);
        operationCount.fetch_add(1);
    });

    EXPECT_EQ(operationCount.load(), numThreads * 8); // 1 add + 5 acquire + 1 reset + 1 remove per thread
}

}  // namespace atom::async::sync::test
