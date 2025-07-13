// filepath: atom/async/test_limiter.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <numeric>  // For std::iota
#include <string>
#include <thread>
#include <vector>

#include "atom/async/limiter.hpp"
#include "atom/error/exception.hpp"  // For THROW_INVALID_ARGUMENT

// Include spdlog for potential logging in tests, if needed for debugging
#include <spdlog/spdlog.h>

using namespace atom::async;
using namespace std::chrono_literals;
using ::testing::Ge;
using ::testing::Le;

// Helper to run a coroutine and get its result (if any)
// For testing purposes, we'll just resume it directly or use std::async
// A simple coroutine that yields nothing, just for testing await_suspend/resume
struct TestCoroutine {
    struct promise_type {
        TestCoroutine get_return_object() { return {}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        void return_void() {}
    };
};

// Test fixture for RateLimiter
class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the singleton instance for each test to ensure isolation
        // This is a hack for singletons, but necessary for unit testing.
        // In a real scenario, you might inject the limiter or use a
        // non-singleton. For this test, we'll just create a new RateLimiter
        // object directly. The RateLimiterSingleton::instance() will always
        // return the same one, so we test the direct class. However, the
        // provided RateLimiterSingleton uses a static local variable, which
        // means it's initialized once. To truly reset it, we'd need to modify
        // the singleton pattern or use a test-specific build. For now, we'll
        // test the RateLimiter class directly.
        limiter_ = std::make_unique<RateLimiter>();
        spdlog::set_level(
            spdlog::level::off);  // Suppress spdlog output during tests
    }

    void TearDown() override {
        limiter_->resume();  // Ensure any paused state is cleared
        limiter_.reset();    // Destroy the limiter
    }

    std::unique_ptr<RateLimiter> limiter_;

    // Helper to run a coroutine that acquires a limit
    std::future<bool> run_acquire_coroutine(RateLimiter& limiter,
                                            std::string func_name) {
        return std::async(std::launch::async, [&limiter, func_name]() -> bool {
            try {
                co_await limiter.acquire(func_name);
                return true;  // Acquired successfully
            } catch (const RateLimitExceededException& e) {
                spdlog::debug(
                    "Coroutine for {} caught RateLimitExceededException: {}",
                    func_name, e.what());
                return false;  // Rejected
            } catch (const std::exception& e) {
                spdlog::error(
                    "Coroutine for {} caught unexpected exception: {}",
                    func_name, e.what());
                return false;
            }
        });
    }
};

// Test 1: Basic Rate Limiting - Allow within limit, reject when exceeded
TEST_F(RateLimiterTest, BasicRateLimiting) {
    std::string func_name = "test_func";
    limiter_->setFunctionLimit(func_name, 2, 1s);  // 2 requests per second

    // First request: should be allowed
    auto f1 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f1.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);

    // Second request: should be allowed
    auto f2 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f2.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);

    // Third request: should be rejected
    auto f3 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f3.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);

    // Wait for time window to pass
    std::this_thread::sleep_for(1s);

    // Fourth request: should be allowed again
    auto f4 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f4.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name),
              1);  // Rejected count doesn't reset automatically
}

// Test 2: Time Window Cleanup
TEST_F(RateLimiterTest, TimeWindowCleanup) {
    std::string func_name = "cleanup_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);  // 1 request per second

    auto f1 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f1.get());

    auto f2 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f2.get());  // Rejected

    std::this_thread::sleep_for(1100ms);  // Wait slightly more than 1 second

    auto f3 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f3.get());  // Should be allowed now due to cleanup
}

// Test 3: setFunctionLimit - Valid and Invalid Parameters
TEST_F(RateLimiterTest, SetFunctionLimit) {
    std::string func_name = "set_limit_func";

    // Valid settings
    EXPECT_NO_THROW(limiter_->setFunctionLimit(func_name, 10, 5s));
    // Check if settings are applied (indirectly by trying to acquire)
    limiter_->setFunctionLimit(func_name, 1, 1s);
    auto f = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f.get());
    f = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f.get());

    // Invalid max_requests (0)
    EXPECT_THROW(limiter_->setFunctionLimit(func_name, 0, 1s),
                 atom::error::InvalidArgumentException);

    // Invalid time_window (0s)
    EXPECT_THROW(limiter_->setFunctionLimit(func_name, 1, 0s),
                 atom::error::InvalidArgumentException);

    // Invalid time_window (negative, though chrono::seconds doesn't allow
    // negative directly) This is covered by <= 0 check.
}

// Test 4: setFunctionLimits - Batch Setting
TEST_F(RateLimiterTest, SetFunctionLimitsBatch) {
    std::vector<std::pair<std::string_view, RateLimiter::Settings>> settings = {
        {"func_A", RateLimiter::Settings(1, 1s)},
        {"func_B", RateLimiter::Settings(5, 10s)},
        {"func_C", RateLimiter::Settings(2, 2s)}};

    EXPECT_NO_THROW(limiter_->setFunctionLimits(settings));

    // Verify settings for func_A
    auto fA1 = run_acquire_coroutine(*limiter_, "func_A");
    EXPECT_TRUE(fA1.get());
    auto fA2 = run_acquire_coroutine(*limiter_, "func_A");
    EXPECT_FALSE(fA2.get());

    // Verify settings for func_C
    auto fC1 = run_acquire_coroutine(*limiter_, "func_C");
    EXPECT_TRUE(fC1.get());
    auto fC2 = run_acquire_coroutine(*limiter_, "func_C");
    EXPECT_TRUE(fC2.get());
    auto fC3 = run_acquire_coroutine(*limiter_, "func_C");
    EXPECT_FALSE(fC3.get());

    // Invalid settings in batch
    std::vector<std::pair<std::string_view, RateLimiter::Settings>>
        invalid_settings = {
            {"func_D", RateLimiter::Settings(1, 1s)},
            {"func_E", RateLimiter::Settings(0, 1s)}  // Invalid
        };
    EXPECT_THROW(limiter_->setFunctionLimits(invalid_settings),
                 atom::error::InvalidArgumentException);
}

// Test 5: acquireBatch
TEST_F(RateLimiterTest, AcquireBatch) {
    std::string func_name = "batch_func";
    limiter_->setFunctionLimit(func_name, 3, 1s);

    std::vector<std::string> func_names = {func_name, func_name, func_name,
                                           func_name};
    auto awaiters = limiter_->acquireBatch(func_names);

    std::vector<std::future<bool>> futures;
    for (auto& aw : awaiters) {
        futures.push_back(std::async(std::launch::async, [&aw]() -> bool {
            try {
                co_await aw;
                return true;
            } catch (const RateLimitExceededException&) {
                return false;
            }
        }));
    }

    int allowed_count = 0;
    int rejected_count = 0;
    for (auto& f : futures) {
        if (f.get()) {
            allowed_count++;
        } else {
            rejected_count++;
        }
    }

    EXPECT_EQ(allowed_count, 3);
    EXPECT_EQ(rejected_count, 1);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);
}

// Test 6: Pause and Resume
TEST_F(RateLimiterTest, PauseResume) {
    std::string func_name = "pause_resume_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);

    // Acquire one, then pause
    auto f1 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f1.get());

    limiter_->pause();

    // Subsequent requests should be rejected while paused
    auto f2 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f2.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);

    auto f3 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f3.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 2);

    // Resume the limiter
    limiter_->resume();

    // After resume, the next request should still be rejected if within time
    // window and no cleanup happened yet. The resume() call itself processes
    // waiters. Let's re-test the scenario where requests are queued and then
    // resumed.
    limiter_->resetAll();  // Clear state for a clean test
    limiter_->setFunctionLimit(func_name, 1, 1s);

    limiter_->pause();

    // These should be rejected and queued
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(run_acquire_coroutine(*limiter_, func_name));
    }

    // All should be rejected initially
    for (auto& f : futures) {
        // We expect them to be rejected because the limiter is paused
        // The await_resume will throw, so f.get() will return false
        EXPECT_FALSE(f.get());
    }
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 5);

    // Resume the limiter. This should process the queued requests.
    // Only one should be allowed, others remain rejected.
    limiter_->resume();

    // The behavior here depends on how `processWaiters` is implemented.
    // If it tries to resume all, only the first one will succeed.
    // The current implementation of `processWaiters` will try to resume as many
    // as possible up to the limit. Since the limit is 1, only one will be
    // allowed. The futures were already resolved as false because they were
    // rejected when `await_suspend` was called. The `resume` call will try to
    // re-evaluate the conditions for the *next* set of requests. This test
    // needs to be re-thought for the exact behavior of `resume` and
    // `await_suspend`.

    // Let's re-design this test to verify that `resume` allows new requests.
    limiter_->resetAll();
    limiter_->setFunctionLimit(func_name, 1, 1s);

    limiter_->pause();
    // No requests made while paused, so no rejected count yet.

    // Now resume.
    limiter_->resume();

    // A request after resume should be allowed.
    auto f_after_resume = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f_after_resume.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);  // Still 0 rejected
}

// Test 7: Exception Handling (RateLimitExceededException)
TEST_F(RateLimiterTest, RateLimitExceededException) {
    std::string func_name = "exception_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);

    auto f1 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f1.get());

    // This one should throw
    auto f2 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f2.get());  // f.get() returns false if exception was caught by
                             // async lambda
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);
}

// Test 8: getRejectedRequests
TEST_F(RateLimiterTest, GetRejectedRequests) {
    std::string func_name = "rejected_count_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);

    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);
    EXPECT_EQ(limiter_->getRejectedRequests("non_existent_func"), 0);

    auto f1 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_TRUE(f1.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);

    auto f2 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f2.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);

    auto f3 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f3.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 2);
}

// Test 9: resetFunction and resetAll
TEST_F(RateLimiterTest, ResetFunctions) {
    std::string func_name1 = "reset_func1";
    std::string func_name2 = "reset_func2";

    limiter_->setFunctionLimit(func_name1, 1, 1s);
    limiter_->setFunctionLimit(func_name2, 1, 1s);

    // Make some requests to get rejected counts
    run_acquire_coroutine(*limiter_, func_name1).get();  // Allowed
    run_acquire_coroutine(*limiter_, func_name1).get();  // Rejected
    run_acquire_coroutine(*limiter_, func_name2).get();  // Allowed
    run_acquire_coroutine(*limiter_, func_name2).get();  // Rejected

    EXPECT_EQ(limiter_->getRejectedRequests(func_name1), 1);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name2), 1);

    // Reset func_name1
    limiter_->resetFunction(func_name1);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name1), 0);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name2),
              1);  // func_name2 unaffected

    // func_name1 should now allow a new request
    auto f1_new = run_acquire_coroutine(*limiter_, func_name1);
    EXPECT_TRUE(f1_new.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name1),
              0);  // Still 0 rejected

    // Reset all
    limiter_->resetAll();
    EXPECT_EQ(limiter_->getRejectedRequests(func_name1), 0);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name2), 0);

    // Both should now allow new requests
    auto f1_after_all = run_acquire_coroutine(*limiter_, func_name1);
    EXPECT_TRUE(f1_after_all.get());
    auto f2_after_all = run_acquire_coroutine(*limiter_, func_name2);
    EXPECT_TRUE(f2_after_all.get());
}

// Test 10: Concurrency
TEST_F(RateLimiterTest, ConcurrentAcquire) {
    std::string func_name = "concurrent_func";
    limiter_->setFunctionLimit(func_name, 10, 1s);  // 10 requests per second

    const int num_requests = 100;
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(run_acquire_coroutine(*limiter_, func_name));
    }

    int allowed_count = 0;
    int rejected_count = 0;
    for (auto& f : futures) {
        if (f.get()) {
            allowed_count++;
        } else {
            rejected_count++;
        }
    }

    // In a 1-second window, only 10 should be allowed.
    // The rest should be rejected.
    EXPECT_EQ(allowed_count, 10);
    EXPECT_EQ(rejected_count, num_requests - 10);
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), num_requests - 10);
}

// Test 11: Move Semantics
TEST_F(RateLimiterTest, MoveConstructor) {
    std::string func_name = "move_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);
    run_acquire_coroutine(*limiter_, func_name).get();  // Allowed
    run_acquire_coroutine(*limiter_, func_name).get();  // Rejected

    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);

    RateLimiter moved_limiter = std::move(*limiter_);

    // Original limiter should be in a valid but unspecified state (or null if
    // unique_ptr) For raw object, it's moved-from state. Test the moved_limiter
    EXPECT_EQ(moved_limiter.getRejectedRequests(func_name), 1);

    // New request on moved_limiter should still be rejected within the window
    auto f = run_acquire_coroutine(moved_limiter, func_name);
    EXPECT_FALSE(f.get());
    EXPECT_EQ(moved_limiter.getRejectedRequests(func_name), 2);
}

TEST_F(RateLimiterTest, MoveAssignment) {
    std::string func_name1 = "move_assign_func1";
    std::string func_name2 = "move_assign_func2";

    limiter_->setFunctionLimit(func_name1, 1, 1s);
    run_acquire_coroutine(*limiter_, func_name1).get();  // Allowed
    run_acquire_coroutine(*limiter_, func_name1).get();  // Rejected
    EXPECT_EQ(limiter_->getRejectedRequests(func_name1), 1);

    RateLimiter other_limiter;
    other_limiter.setFunctionLimit(func_name2, 1, 1s);
    run_acquire_coroutine(other_limiter, func_name2).get();  // Allowed
    run_acquire_coroutine(other_limiter, func_name2).get();  // Rejected
    EXPECT_EQ(other_limiter.getRejectedRequests(func_name2), 1);

    *limiter_ = std::move(other_limiter);  // Move assignment

    // limiter_ should now have func_name2's state
    EXPECT_EQ(limiter_->getRejectedRequests(func_name1),
              0);  // func_name1 state should be gone or reset
    EXPECT_EQ(limiter_->getRejectedRequests(func_name2), 1);

    // New request on limiter_ for func_name2 should still be rejected
    auto f = run_acquire_coroutine(*limiter_, func_name2);
    EXPECT_FALSE(f.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name2), 2);
}

// Test 12: Destructor Behavior - Ensure pending coroutines are resumed
TEST_F(RateLimiterTest, DestructorResumesPending) {
    std::string func_name = "destructor_func";
    limiter_->setFunctionLimit(func_name, 0,
                               1s);  // Set limit to 0 to ensure rejection

    // These coroutines will be suspended
    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(run_acquire_coroutine(*limiter_, func_name));
    }

    // At this point, all futures should be waiting (suspended)
    // The destructor of limiter_ will be called in TearDown, which should
    // resume them. They will then throw RateLimitExceededException, and the
    // lambda will return false.

    // Explicitly destroy the limiter here to observe behavior before TearDown
    limiter_.reset();

    // Now check if futures completed (they should have, due to destructor
    // resuming)
    for (auto& f : futures) {
        // They should have been resumed and then rejected
        EXPECT_FALSE(f.get());
    }
}

// Test 13: Platform-specific optimizedProcessWaiters (indirectly tested via
// resume) These tests rely on the `resume()` method calling the correct
// optimized version. We can't directly test the `optimizedProcessWaiters`
// private methods. The `resume()` test above covers this to some extent. To
// make sure the correct path is taken, we'd need to mock or inspect internal
// state, which is beyond typical unit testing scope for public API. Assuming
// the build system correctly defines ATOM_PLATFORM_WINDOWS/MACOS/LINUX and
// ATOM_USE_ASIO, the `resume()` call will use the appropriate implementation.

// Test with no limits set (default behavior)
TEST_F(RateLimiterTest, NoLimitsSet) {
    std::string func_name = "no_limit_func";
    // No setFunctionLimit called, so default settings (5 req/1s) apply
    // implicitly when the function_name is first encountered in await_suspend.

    std::vector<std::future<bool>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(run_acquire_coroutine(*limiter_, func_name));
    }
    // All 5 should be allowed
    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);

    // 6th request should be rejected
    auto f6 = run_acquire_coroutine(*limiter_, func_name);
    EXPECT_FALSE(f6.get());
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 1);
}

// Test RateLimiter::Settings validation
TEST(RateLimiterSettingsTest, Validation) {
    // Valid settings
    EXPECT_NO_THROW(RateLimiter::Settings(1, 1s));
    EXPECT_NO_THROW(RateLimiter::Settings(100, 60s));

    // Invalid maxRequests
    EXPECT_THROW(RateLimiter::Settings(0, 1s), std::invalid_argument);

    // Invalid timeWindow
    EXPECT_THROW(RateLimiter::Settings(1, 0s), std::invalid_argument);
    EXPECT_THROW(RateLimiter::Settings(1, -1s),
                 std::invalid_argument);  // Should be caught by chrono type
                                          // system, but good to check
}

// Test RateLimiterSingleton
TEST(RateLimiterSingletonTest, IsSingleton) {
    RateLimiter& instance1 = RateLimiterSingleton::instance();
    RateLimiter& instance2 = RateLimiterSingleton::instance();

    // Both instances should be the same object
    EXPECT_EQ(&instance1, &instance2);

    // Test a basic operation to ensure it's functional
    instance1.setFunctionLimit("singleton_func", 1, 1s);
    auto f1 = std::async(std::launch::async, [&instance1]() -> bool {
        try {
            co_await instance1.acquire("singleton_func");
            return true;
        } catch (const RateLimitExceededException&) {
            return false;
        }
    });
    EXPECT_TRUE(f1.get());

    auto f2 = std::async(std::launch::async, [&instance2]() -> bool {
        try {
            co_await instance2.acquire("singleton_func");
            return true;
        } catch (const RateLimitExceededException&) {
            return false;
        }
    });
    EXPECT_FALSE(f2.get());  // Should be rejected as it's the same limiter
}
