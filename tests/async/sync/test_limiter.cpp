/*
 * test_limiter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Rate Limiter
Tests rate limiting functionality, concurrent access, edge cases, and
platform-specific optimizations.

Note: The RateLimiter uses a coroutine-based API with acquire() returning
an Awaiter. Tests are designed to work with this API pattern.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/sync/limiter.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::sync::test {

// ============================================================================
// Rate Limiter Tests
// ============================================================================

class RateLimiterTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }

    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

// ============================================================================
// Settings Tests
// ============================================================================

TEST_F(RateLimiterTest, SettingsValidConstruction) {
    EXPECT_NO_THROW({
        RateLimiter::Settings settings(5, 1s);
        EXPECT_EQ(settings.maxRequests, 5);
        EXPECT_EQ(settings.timeWindow, 1s);
    });
}

TEST_F(RateLimiterTest, SettingsDefaultValues) {
    RateLimiter::Settings settings;
    EXPECT_EQ(settings.maxRequests, 5);
    EXPECT_EQ(settings.timeWindow, 1s);
}

TEST_F(RateLimiterTest, SettingsZeroMaxRequestsThrows) {
    EXPECT_THROW(RateLimiter::Settings(0, 1s), std::invalid_argument);
}

TEST_F(RateLimiterTest, SettingsZeroTimeWindowThrows) {
    EXPECT_THROW(RateLimiter::Settings(5, 0s), std::invalid_argument);
}

TEST_F(RateLimiterTest, SettingsNegativeTimeWindowThrows) {
    EXPECT_THROW(RateLimiter::Settings(5, std::chrono::seconds(-1)),
                 std::invalid_argument);
}

// ============================================================================
// Basic RateLimiter Construction Tests
// ============================================================================

TEST_F(RateLimiterTest, DefaultConstruction) {
    EXPECT_NO_THROW({ RateLimiter limiter; });
}

TEST_F(RateLimiterTest, MoveConstruction) {
    RateLimiter limiter1;
    limiter1.setFunctionLimit("test", 5, 1s);

    RateLimiter limiter2(std::move(limiter1));
    // limiter2 should now own the state
    EXPECT_NO_THROW(limiter2.acquire("test"));
}

TEST_F(RateLimiterTest, MoveAssignment) {
    RateLimiter limiter1;
    limiter1.setFunctionLimit("test", 5, 1s);

    RateLimiter limiter2;
    limiter2 = std::move(limiter1);
    // limiter2 should now own the state
    EXPECT_NO_THROW(limiter2.acquire("test"));
}

// ============================================================================
// setFunctionLimit Tests
// ============================================================================

TEST_F(RateLimiterTest, SetFunctionLimitValid) {
    RateLimiter limiter;
    EXPECT_NO_THROW(limiter.setFunctionLimit("test_function", 5, 1s));
}

TEST_F(RateLimiterTest, SetFunctionLimitZeroMaxRequestsThrows) {
    RateLimiter limiter;
    EXPECT_THROW(limiter.setFunctionLimit("test", 0, 1s),
                 std::invalid_argument);
}

TEST_F(RateLimiterTest, SetFunctionLimitZeroTimeWindowThrows) {
    RateLimiter limiter;
    EXPECT_THROW(limiter.setFunctionLimit("test", 5, 0s),
                 std::invalid_argument);
}

TEST_F(RateLimiterTest, SetFunctionLimitMultipleFunctions) {
    RateLimiter limiter;
    EXPECT_NO_THROW({
        limiter.setFunctionLimit("func1", 3, 1s);
        limiter.setFunctionLimit("func2", 5, 2s);
        limiter.setFunctionLimit("func3", 10, 500ms);
    });
}

TEST_F(RateLimiterTest, SetFunctionLimitOverwrite) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);
    EXPECT_NO_THROW(limiter.setFunctionLimit("test", 10, 2s));
}

// ============================================================================
// setFunctionLimits (Batch) Tests
// ============================================================================

TEST_F(RateLimiterTest, SetFunctionLimitsBatch) {
    RateLimiter limiter;

    std::vector<std::pair<std::string_view, RateLimiter::Settings>> settings = {
        {"func1", RateLimiter::Settings(3, 1s)},
        {"func2", RateLimiter::Settings(5, 2s)},
        {"func3", RateLimiter::Settings(10, 500ms)}};

    EXPECT_NO_THROW(limiter.setFunctionLimits(settings));

    // Verify all functions are set by acquiring
    EXPECT_NO_THROW(limiter.acquire("func1"));
    EXPECT_NO_THROW(limiter.acquire("func2"));
    EXPECT_NO_THROW(limiter.acquire("func3"));
}

TEST_F(RateLimiterTest, SetFunctionLimitsEmptyBatch) {
    RateLimiter limiter;
    std::vector<std::pair<std::string_view, RateLimiter::Settings>> settings;
    EXPECT_NO_THROW(limiter.setFunctionLimits(settings));
}

// ============================================================================
// acquire Tests
// ============================================================================

TEST_F(RateLimiterTest, AcquireReturnsAwaiter) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    auto awaiter = limiter.acquire("test");
    // Awaiter should be valid (await_ready returns false to check rate limit)
    EXPECT_FALSE(awaiter.await_ready());
}

TEST_F(RateLimiterTest, AcquireMultipleTimes) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(limiter.acquire("test"));
    }
}

TEST_F(RateLimiterTest, AcquireNonExistentFunction) {
    RateLimiter limiter;
    // Acquiring non-existent function should still return an awaiter
    // The behavior depends on implementation - it may use default settings
    EXPECT_NO_THROW(limiter.acquire("non_existent"));
}

// ============================================================================
// acquireBatch Tests
// ============================================================================

TEST_F(RateLimiterTest, AcquireBatchMultipleFunctions) {
    RateLimiter limiter;
    limiter.setFunctionLimit("func1", 5, 1s);
    limiter.setFunctionLimit("func2", 5, 1s);
    limiter.setFunctionLimit("func3", 5, 1s);

    std::vector<std::string> funcNames = {"func1", "func2", "func3"};
    auto awaiters = limiter.acquireBatch(funcNames);

    EXPECT_EQ(awaiters.size(), 3);
}

TEST_F(RateLimiterTest, AcquireBatchEmptyRange) {
    RateLimiter limiter;
    std::vector<std::string> funcNames;
    auto awaiters = limiter.acquireBatch(funcNames);
    EXPECT_TRUE(awaiters.empty());
}

// ============================================================================
// Pause/Resume Tests
// ============================================================================

TEST_F(RateLimiterTest, PauseAndResume) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    EXPECT_NO_THROW(limiter.pause());
    EXPECT_NO_THROW(limiter.resume());
}

TEST_F(RateLimiterTest, MultiplePauseCalls) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    EXPECT_NO_THROW({
        limiter.pause();
        limiter.pause();  // Should be idempotent
    });
}

TEST_F(RateLimiterTest, MultipleResumeCalls) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    limiter.pause();
    EXPECT_NO_THROW({
        limiter.resume();
        limiter.resume();  // Should be idempotent
    });
}

TEST_F(RateLimiterTest, ResumeWithoutPause) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    EXPECT_NO_THROW(limiter.resume());  // Should be safe
}

// ============================================================================
// getRejectedRequests Tests
// ============================================================================

TEST_F(RateLimiterTest, GetRejectedRequestsInitiallyZero) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    EXPECT_EQ(limiter.getRejectedRequests("test"), 0);
}

TEST_F(RateLimiterTest, GetRejectedRequestsNonExistentFunction) {
    RateLimiter limiter;
    EXPECT_EQ(limiter.getRejectedRequests("non_existent"), 0);
}

// ============================================================================
// resetFunction Tests
// ============================================================================

TEST_F(RateLimiterTest, ResetFunctionBasic) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    // Acquire some
    for (int i = 0; i < 3; ++i) {
        limiter.acquire("test");
    }

    EXPECT_NO_THROW(limiter.resetFunction("test"));
}

TEST_F(RateLimiterTest, ResetFunctionNonExistent) {
    RateLimiter limiter;
    // Resetting non-existent function should not throw
    EXPECT_NO_THROW(limiter.resetFunction("non_existent"));
}

TEST_F(RateLimiterTest, ResetFunctionClearsRejectedCount) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    limiter.resetFunction("test");
    EXPECT_EQ(limiter.getRejectedRequests("test"), 0);
}

// ============================================================================
// resetAll Tests
// ============================================================================

TEST_F(RateLimiterTest, ResetAllBasic) {
    RateLimiter limiter;
    limiter.setFunctionLimit("func1", 5, 1s);
    limiter.setFunctionLimit("func2", 5, 1s);

    // Acquire some
    limiter.acquire("func1");
    limiter.acquire("func2");

    EXPECT_NO_THROW(limiter.resetAll());
}

TEST_F(RateLimiterTest, ResetAllEmptyLimiter) {
    RateLimiter limiter;
    EXPECT_NO_THROW(limiter.resetAll());
}

// ============================================================================
// processWaiters Tests
// ============================================================================

TEST_F(RateLimiterTest, ProcessWaitersBasic) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    EXPECT_NO_THROW(limiter.processWaiters());
}

TEST_F(RateLimiterTest, ProcessWaitersEmptyQueue) {
    RateLimiter limiter;
    EXPECT_NO_THROW(limiter.processWaiters());
}

// ============================================================================
// Awaiter Tests
// ============================================================================

TEST_F(RateLimiterTest, AwaiterAwaitReadyReturnsFalse) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 5, 1s);

    auto awaiter = limiter.acquire("test");
    // await_ready should return false to allow suspension and rate limit check
    EXPECT_FALSE(awaiter.await_ready());
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(RateLimiterTest, ConcurrentSetFunctionLimit) {
    RateLimiter limiter;
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &successCount, i]() {
            try {
                std::string funcName = "func_" + std::to_string(i);
                limiter.setFunctionLimit(funcName, 5, 1s);
                successCount.fetch_add(1);
            } catch (...) {
                // Should not throw
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}

TEST_F(RateLimiterTest, ConcurrentAcquire) {
    RateLimiter limiter;
    limiter.setFunctionLimit("concurrent_test", 100, 1s);

    std::atomic<int> acquireCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 20;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &acquireCount]() {
            for (int j = 0; j < 5; ++j) {
                try {
                    limiter.acquire("concurrent_test");
                    acquireCount.fetch_add(1);
                } catch (...) {
                    // May throw if rate limited
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(acquireCount.load(), 0);
}

TEST_F(RateLimiterTest, ConcurrentPauseResume) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 100, 1s);

    std::atomic<bool> running{true};

    std::thread pauseThread([&limiter, &running]() {
        while (running.load()) {
            limiter.pause();
            std::this_thread::yield();
        }
    });

    std::thread resumeThread([&limiter, &running]() {
        while (running.load()) {
            limiter.resume();
            std::this_thread::yield();
        }
    });

    std::thread acquireThread([&limiter, &running]() {
        while (running.load()) {
            try {
                limiter.acquire("test");
            } catch (...) {
            }
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(100ms);
    running.store(false);

    pauseThread.join();
    resumeThread.join();
    acquireThread.join();

    // Test passes if no crashes or deadlocks
    SUCCEED();
}

TEST_F(RateLimiterTest, ConcurrentResetFunction) {
    RateLimiter limiter;
    limiter.setFunctionLimit("test", 100, 1s);

    std::atomic<int> resetCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &resetCount]() {
            for (int j = 0; j < 10; ++j) {
                limiter.resetFunction("test");
                resetCount.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(resetCount.load(), numThreads * 10);
}

TEST_F(RateLimiterTest, ConcurrentResetAll) {
    RateLimiter limiter;
    limiter.setFunctionLimit("func1", 100, 1s);
    limiter.setFunctionLimit("func2", 100, 1s);

    std::atomic<int> resetCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &resetCount]() {
            for (int j = 0; j < 10; ++j) {
                limiter.resetAll();
                resetCount.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(resetCount.load(), numThreads * 10);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(RateLimiterTest, HighConcurrencyStressTest) {
    RateLimiter limiter;
    limiter.setFunctionLimit("stress_test", 1000, 1s);

    std::atomic<int> totalOperations{0};

    std::vector<std::thread> threads;
    const int numThreads = 50;
    const int operationsPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(
            [&limiter, &totalOperations, operationsPerThread]() {
                for (int j = 0; j < operationsPerThread; ++j) {
                    try {
                        limiter.acquire("stress_test");
                        totalOperations.fetch_add(1);
                    } catch (...) {
                    }
                    std::this_thread::yield();
                }
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(totalOperations.load(), 0);
}

TEST_F(RateLimiterTest, MixedOperationsStressTest) {
    RateLimiter limiter;
    limiter.setFunctionLimit("mixed_test", 100, 1s);

    std::atomic<int> operationCount{0};

    const size_t numThreads =
        std::min(static_cast<size_t>(8),
                 static_cast<size_t>(std::thread::hardware_concurrency()));

    std::vector<std::thread> threads;

    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([&limiter, &operationCount, i]() {
            for (int j = 0; j < 50; ++j) {
                switch (i % 4) {
                    case 0:  // Acquire
                        try {
                            limiter.acquire("mixed_test");
                            operationCount.fetch_add(1);
                        } catch (...) {
                        }
                        break;

                    case 1:  // Get rejected requests
                        limiter.getRejectedRequests("mixed_test");
                        operationCount.fetch_add(1);
                        break;

                    case 2:  // Reset occasionally
                        if (j % 20 == 0) {
                            limiter.resetFunction("mixed_test");
                            operationCount.fetch_add(1);
                        }
                        break;

                    case 3:  // Pause/Resume occasionally
                        if (j % 25 == 0) {
                            limiter.pause();
                            limiter.resume();
                            operationCount.fetch_add(1);
                        }
                        break;
                }

                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(operationCount.load(), 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(RateLimiterTest, VeryLongFunctionName) {
    RateLimiter limiter;
    std::string longName(1000, 'a');
    EXPECT_NO_THROW(limiter.setFunctionLimit(longName, 5, 1s));
    EXPECT_NO_THROW(limiter.acquire(longName));
}

TEST_F(RateLimiterTest, SpecialCharactersInFunctionName) {
    RateLimiter limiter;
    std::string specialName = "func!@#$%^&*()_+-={}[]|\\:;\"'<>?,./";
    EXPECT_NO_THROW(limiter.setFunctionLimit(specialName, 5, 1s));
    EXPECT_NO_THROW(limiter.acquire(specialName));
}

TEST_F(RateLimiterTest, UnicodeInFunctionName) {
    RateLimiter limiter;
    std::string unicodeName = "函数名_функция_関数";
    EXPECT_NO_THROW(limiter.setFunctionLimit(unicodeName, 5, 1s));
    EXPECT_NO_THROW(limiter.acquire(unicodeName));
}

TEST_F(RateLimiterTest, ManyFunctions) {
    RateLimiter limiter;

    const int numFunctions = 1000;

    // Add many functions
    for (int i = 0; i < numFunctions; ++i) {
        std::string funcName = "func_" + std::to_string(i);
        EXPECT_NO_THROW(limiter.setFunctionLimit(funcName, 5, 1s));
    }

    // Test a few of them
    EXPECT_NO_THROW(limiter.acquire("func_0"));
    EXPECT_NO_THROW(limiter.acquire("func_500"));
    EXPECT_NO_THROW(limiter.acquire("func_999"));
}

TEST_F(RateLimiterTest, VeryHighMaxRequests) {
    RateLimiter limiter;
    EXPECT_NO_THROW(limiter.setFunctionLimit("high_limit", 1000000, 1s));
}

TEST_F(RateLimiterTest, VeryLongTimeWindow) {
    RateLimiter limiter;
    EXPECT_NO_THROW(
        limiter.setFunctionLimit("long_window", 5, std::chrono::seconds(3600)));
}

TEST_F(RateLimiterTest, VeryShortTimeWindow) {
    RateLimiter limiter;
    // Minimum valid time window is 1 second based on Settings validation
    EXPECT_NO_THROW(limiter.setFunctionLimit("short_window", 5, 1s));
}

// ============================================================================
// Singleton Tests
// ============================================================================

TEST_F(RateLimiterTest, SingletonInstance) {
    RateLimiter& instance1 = RateLimiterSingleton::instance();
    RateLimiter& instance2 = RateLimiterSingleton::instance();

    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(RateLimiterTest, SingletonUsage) {
    RateLimiter& limiter = RateLimiterSingleton::instance();

    EXPECT_NO_THROW(limiter.setFunctionLimit("singleton_test", 5, 1s));
    EXPECT_NO_THROW(limiter.acquire("singleton_test"));

    // Clean up
    limiter.resetFunction("singleton_test");
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST_F(RateLimiterTest, RateLimitExceededExceptionMessage) {
    try {
        throw RateLimitExceededException("test message");
    } catch (const RateLimitExceededException& e) {
        std::string what = e.what();
        EXPECT_TRUE(what.find("test message") != std::string::npos);
        EXPECT_TRUE(what.find("Rate limit exceeded") != std::string::npos);
    }
}

TEST_F(RateLimiterTest, RateLimitExceededExceptionInheritance) {
    try {
        throw RateLimitExceededException("test");
    } catch (const std::runtime_error& e) {
        // Should be caught as runtime_error
        SUCCEED();
    } catch (...) {
        FAIL() << "RateLimitExceededException should inherit from "
                  "std::runtime_error";
    }
}

}  // namespace atom::async::sync::test
