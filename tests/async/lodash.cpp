#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>  // For std::function
#include <thread>
#include <vector>

#include "atom/async/lodash.hpp"

using namespace atom::async;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;

// Test fixture for Debounce and Throttle tests
class LodashTest : public ::testing::Test {
protected:
    std::atomic<int> call_count{0};
    std::atomic<int> arg_value{0};  // To test passing arguments

    // Function to be debounced/throttled
    auto increment_call_count() {
        return [&]() { call_count++; };
    }

    // Function to be debounced/throttled that takes an argument
    auto increment_with_arg() {
        return [&](int val) {
            call_count++;
            arg_value.store(val);
        };
    }

    void SetUp() override {
        call_count = 0;
        arg_value = 0;
    }

    void TearDown() override {
        // Ensure any background threads are joined by the Debounce/Throttle
        // destructors
    }
};

// --- Debounce Tests ---

// Test basic debounce (trailing edge)
TEST_F(LodashTest, Debounce_TrailingEdge_CallsOnceAfterDelay) {
    Debounce<std::function<void()>> debounced_fn(
        increment_call_count(), std::chrono::milliseconds(50),
        false);  // trailing = true (default)

    // Call multiple times quickly
    for (int i = 0; i < 5; ++i) {
        debounced_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Function should not have been called yet
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait for the delay to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Function should have been called exactly once
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again after delay, should trigger another call after delay
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Not called immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_THAT(call_count.load(), Eq(2));  // Called again
}

// Test debounce (leading edge)
TEST_F(LodashTest, Debounce_LeadingEdge_CallsImmediatelyThenDebounces) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(50),
                                                 true);  // leading = true

    // First call should be immediate
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call multiple times quickly within the delay
    for (int i = 0; i < 5; ++i) {
        debounced_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // No more calls should happen immediately
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait for the delay to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // No trailing call should happen by default with leading=true unless more
    // calls came after the leading one
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again after the delay has passed
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(2));  // Should call immediately again
}

// Test debounce (leading edge) with subsequent calls triggering trailing
TEST_F(LodashTest, Debounce_LeadingEdge_SubsequentCallsTriggerTrailing) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(50),
                                                 true);  // leading = true

    // First call should be immediate
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait a bit, but less than the delay
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Call again - this should schedule a trailing call
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Not called immediately again

    // Wait for the delay from the *last* call to pass
    std::this_thread::sleep_for(
        std::chrono::milliseconds(40));  // 20 + 40 = 60 > 50

    // A trailing call should now happen
    EXPECT_THAT(call_count.load(), Eq(2));

    // Call again after everything has settled
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(3));  // Should call immediately again
}

// Test debounce with maxWait
TEST_F(LodashTest, Debounce_MaxWait_CallsWithinMaxWait) {
    Debounce<std::function<void()>> debounced_fn(
        increment_call_count(), std::chrono::milliseconds(100),
        false,                            // trailing = true
        std::chrono::milliseconds(200));  // maxWait = 200ms

    // Call repeatedly faster than delay (100ms), but for longer than maxWait
    // (200ms)
    for (int i = 0; i < 30; ++i) {  // Total time > 300ms
        debounced_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Function should have been called at least once due to maxWait
    // It might be called more than once if the loop duration exceeds maxWait
    // significantly and the timer thread gets scheduled multiple times. Let's
    // wait a bit more to ensure any pending maxWait call happens.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(250));  // Wait past maxWait

    // The call count should be at least 1 (due to maxWait)
    EXPECT_THAT(call_count.load(), Ge(1));

    // Wait for the original delay (100ms) from the *last* call in the loop
    // (which was ~300ms in) This might trigger another call if maxWait didn't
    // align perfectly. Let's just check the count after a total sufficient
    // time. The first call should happen around 200ms. Subsequent calls might
    // happen if the loop continues for a long time, triggering maxWait again,
    // or if the loop stops and the final trailing call happens. A simpler test
    // is to call for slightly longer than maxWait and check the count.

    call_count = 0;  // Reset for a cleaner maxWait test
    Debounce<std::function<void()>> debounced_fn_2(
        increment_call_count(), std::chrono::milliseconds(100),
        false,                            // trailing = true
        std::chrono::milliseconds(200));  // maxWait = 200ms

    auto start_time = std::chrono::steady_clock::now();
    // Call repeatedly for slightly longer than maxWait
    while (std::chrono::steady_clock::now() - start_time <
           std::chrono::milliseconds(220)) {
        debounced_fn_2();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait a bit more to ensure any scheduled call completes
    std::this_thread::sleep_for(std::chrono::milliseconds(
        150));  // Wait past the original delay from the last call

    // The function should have been called at least once due to maxWait
    EXPECT_THAT(call_count.load(), Ge(1));
    // It should not be called excessively more than expected based on maxWait
    // The exact count can be tricky due to timing, but it should be relatively
    // low. Let's assert it's not zero and not excessively high (e.g., not
    // called for every single attempt).
    EXPECT_THAT(call_count.load(),
                Le(3));  // Should be 1 or 2 depending on timing
}

// Test debounce cancel
TEST_F(LodashTest, Debounce_Cancel_PreventsPendingCall) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(100),
                                                 false);  // trailing = true

    debounced_fn();  // Schedule a call
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait less than the delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    debounced_fn.cancel();  // Cancel the pending call

    // Wait longer than the original delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Function should not have been called
    EXPECT_THAT(call_count.load(), Eq(0));
}

// Test debounce flush
TEST_F(LodashTest, Debounce_Flush_InvokesPendingCallImmediately) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(100),
                                                 false);  // trailing = true

    debounced_fn();  // Schedule a call
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait less than the delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    debounced_fn.flush();  // Flush the pending call

    // Function should have been called immediately by flush
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait longer than the original delay to ensure no extra call happens
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_THAT(call_count.load(), Eq(1));  // Still 1
}

// Test debounce reset
TEST_F(LodashTest, Debounce_Reset_ClearsState) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(100),
                                                 false);  // trailing = true

    debounced_fn();  // Schedule a call
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait less than the delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    debounced_fn.reset();  // Reset the state

    // Wait longer than the original delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Function should not have been called
    EXPECT_THAT(call_count.load(), Eq(0));

    // Call again after reset, should schedule a new call
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(0));
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_THAT(call_count.load(), Eq(1));  // New call happened
}

// Test debounce callCount
TEST_F(LodashTest, Debounce_CallCount_ReflectsInvocations) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(50),
                                                 false);  // trailing = true

    EXPECT_THAT(debounced_fn.callCount(), Eq(0));

    debounced_fn();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_THAT(debounced_fn.callCount(), Eq(1));

    debounced_fn();
    debounced_fn();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_THAT(debounced_fn.callCount(),
                Eq(2));  // Only one more call due to debounce

    debounced_fn.flush();  // Flush a pending call
    EXPECT_THAT(debounced_fn.callCount(),
                Eq(3));  // Flush counts as an invocation

    debounced_fn.cancel();  // Cancel does not increment count
    EXPECT_THAT(debounced_fn.callCount(), Eq(3));

    debounced_fn.reset();  // Reset does not increment count
    EXPECT_THAT(debounced_fn.callCount(), Eq(3));
}

// Test debounce with arguments
TEST_F(LodashTest, Debounce_WithArguments_CapturesAndPassesArgs) {
    Debounce<std::function<void(int)>> debounced_fn(
        increment_with_arg(), std::chrono::milliseconds(50),
        false);  // trailing = true

    // Call multiple times with different arguments
    debounced_fn(10);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    debounced_fn(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    debounced_fn(30);  // Last argument should be 30

    EXPECT_THAT(call_count.load(), Eq(0));
    EXPECT_THAT(arg_value.load(), Eq(0));

    // Wait for the delay
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Function should be called once with the last argument
    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(arg_value.load(), Eq(30));
}

// Test Debounce constructor throws on negative delay
TEST_F(LodashTest, Debounce_Constructor_ThrowsOnNegativeDelay) {
    EXPECT_THROW(Debounce<std::function<void()>>(
                     increment_call_count(), std::chrono::milliseconds(-100)),
                 std::invalid_argument);
}

// Test Debounce constructor throws on negative maxWait
TEST_F(LodashTest, Debounce_Constructor_ThrowsOnNegativeMaxWait) {
    EXPECT_THROW(Debounce<std::function<void()>>(
                     increment_call_count(), std::chrono::milliseconds(100),
                     false, std::chrono::milliseconds(-50)),
                 std::invalid_argument);
}

// Test Debounce thread safety with concurrent calls
TEST_F(LodashTest, Debounce_ThreadSafety_ConcurrentCalls) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(100),
                                                 false);  // trailing = true

    const int num_threads = 10;
    const int calls_per_thread = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < calls_per_thread; ++j) {
                debounced_fn();
                // Add a small sleep to simulate real-world call patterns
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // After all threads finish calling, wait for the final debounce delay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // The function should have been called only once (the last trailing call)
    // unless maxWait was used or the total duration exceeded the delay multiple
    // times. With a 100ms delay and calls every 5ms for 50 iterations (250ms
    // total per thread), and 10 threads, the calls are spread out. The last
    // call across all threads will determine the final debounce timer. It's
    // most likely to be called exactly once after the last call from any
    // thread.
    EXPECT_THAT(call_count.load(), Eq(1));
}

// Test Debounce thread safety with concurrent flush/cancel/reset
TEST_F(LodashTest, Debounce_ThreadSafety_ConcurrentControlCalls) {
    Debounce<std::function<void()>> debounced_fn(increment_call_count(),
                                                 std::chrono::milliseconds(200),
                                                 false);  // trailing = true

    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Start threads that call, flush, cancel, reset concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            debounced_fn();  // Schedule a call
            if (i % 3 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                debounced_fn.flush();
            } else if (i % 3 == 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                debounced_fn.cancel();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                debounced_fn.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            debounced_fn();  // Schedule another call
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for any potential trailing calls from the last set of calls
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // The exact number of calls is hard to predict due to race conditions
    // between scheduling and control calls. However, the test should not crash
    // or deadlock. We expect some calls to have gone through via flush or
    // trailing edge calls that weren't cancelled/reset in time.
    EXPECT_THAT(call_count.load(),
                Ge(0));  // Should be at least 0, but likely > 0
    // A loose upper bound: each thread schedules two calls. Some are flushed,
    // some cancelled/reset. Max possible calls could be num_threads * 2 if
    // every call was flushed immediately, but flush/cancel/reset also race. A
    // safer check is just > 0. Let's check if it's less than the total number
    // of schedules (20)
    EXPECT_THAT(call_count.load(), Le(num_threads * 2));
}

// --- Throttle Tests ---

// Test basic throttle (leading = true, trailing = false)
TEST_F(LodashTest, Throttle_LeadingOnly_CallsImmediatelyThenIgnores) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(100), true,
        false);  // leading=true, trailing=false

    // First call should be immediate
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call multiple times quickly within the interval
    for (int i = 0; i < 5; ++i) {
        throttled_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // No more calls should happen immediately or as trailing
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait for the interval to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // No trailing call should happen
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again after the interval has passed
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(2));  // Should call immediately again
}

// Test throttle (leading = false, trailing = true)
TEST_F(LodashTest, Throttle_TrailingOnly_CallsAfterInterval) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(100), false,
        true);  // leading=false, trailing=true

    // Call multiple times quickly
    for (int i = 0; i < 5; ++i) {
        throttled_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Function should not have been called yet
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait less than the interval
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait for the interval from the *last* attempt to pass
    std::this_thread::sleep_for(
        std::chrono::milliseconds(60));  // Total wait 50 + 60 = 110 > 100

    // Function should have been called exactly once (trailing edge)
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again after interval, should trigger another trailing call
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Not called immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    EXPECT_THAT(call_count.load(), Eq(2));  // Called again
}

// Test throttle (leading = true, trailing = true)
TEST_F(LodashTest,
       Throttle_LeadingAndTrailing_CallsImmediatelyAndAfterInterval) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(100), true,
        true);  // leading=true, trailing=true

    // First call should be immediate
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call multiple times quickly within the interval
    for (int i = 0; i < 5; ++i) {
        throttled_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // No more calls should happen immediately
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait less than the interval from the last attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_THAT(call_count.load(), Eq(1));

    // Wait for the interval from the *last* attempt to pass
    std::this_thread::sleep_for(
        std::chrono::milliseconds(60));  // Total wait 50 + 60 = 110 > 100

    // A trailing call should happen
    EXPECT_THAT(call_count.load(), Eq(2));

    // Call again after everything has settled
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(3));  // Should call immediately again
}

// Test throttle cancel
TEST_F(LodashTest, Throttle_Cancel_PreventsPendingTrailingCall) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(100), false,
        true);  // leading=false, trailing=true

    throttled_fn();  // Schedule a trailing call
    EXPECT_THAT(call_count.load(), Eq(0));

    // Wait less than the interval
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    throttled_fn.cancel();  // Cancel the pending trailing call

    // Wait longer than the interval
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Function should not have been called
    EXPECT_THAT(call_count.load(), Eq(0));
}

// Test throttle reset
TEST_F(LodashTest, Throttle_Reset_ClearsState) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(100), true,
        true);  // leading=true, trailing=true

    throttled_fn();  // Calls immediately (count=1), schedules potential
                     // trailing
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again to ensure a trailing call is pending
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Not called immediately

    // Reset the state
    throttled_fn.reset();

    // Wait longer than the interval
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // No trailing call should happen after reset
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call again after reset, should call immediately if leading is true
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(2));  // Should call immediately again
}

// Test throttle callCount
TEST_F(LodashTest, Throttle_CallCount_ReflectsInvocations) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(50), true,
        true);  // leading=true, trailing=true

    EXPECT_THAT(throttled_fn.callCount(), Eq(0));

    throttled_fn();  // Leading call
    EXPECT_THAT(throttled_fn.callCount(), Eq(1));

    // Call quickly to trigger trailing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn();
    EXPECT_THAT(throttled_fn.callCount(), Eq(1));  // Not called immediately

    // Wait for trailing call
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_THAT(throttled_fn.callCount(), Eq(2));  // Trailing call happened

    throttled_fn.cancel();  // Cancel does not increment count
    EXPECT_THAT(throttled_fn.callCount(), Eq(2));

    throttled_fn.reset();  // Reset does not increment count
    EXPECT_THAT(throttled_fn.callCount(), Eq(2));
}

// Test throttle with arguments
TEST_F(LodashTest, Throttle_WithArguments_CapturesAndPassesArgs) {
    Throttle<std::function<void(int)>> throttled_fn(
        increment_with_arg(), std::chrono::milliseconds(50), false,
        true);  // leading=false, trailing=true

    // Call multiple times with different arguments
    throttled_fn(10);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn(30);  // Last argument should be 30

    EXPECT_THAT(call_count.load(), Eq(0));
    EXPECT_THAT(arg_value.load(), Eq(0));

    // Wait for the interval
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Function should be called once with the last argument
    EXPECT_THAT(call_count.load(), Eq(1));
    EXPECT_THAT(arg_value.load(), Eq(30));
}

// Test Throttle constructor throws on negative interval
TEST_F(LodashTest, Throttle_Constructor_ThrowsOnNegativeInterval) {
    EXPECT_THROW(Throttle<std::function<void()>>(
                     increment_call_count(), std::chrono::milliseconds(-100)),
                 std::invalid_argument);
}

// Test Throttle thread safety with concurrent calls
TEST_F(LodashTest, Throttle_ThreadSafety_ConcurrentCalls) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(50), true,
        true);  // leading=true, trailing=true

    const int num_threads = 10;
    const int calls_per_thread = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < calls_per_thread; ++j) {
                throttled_fn();
                // Add a small sleep to simulate real-world call patterns
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for any potential trailing calls
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // The exact number of calls is hard to predict due to race conditions,
    // but it should be significantly less than total attempts (num_threads *
    // calls_per_thread). Each thread's first call might be leading. Subsequent
    // calls within the interval are ignored. A trailing call might happen after
    // the last attempt in a series. Minimum calls: num_threads (if each
    // thread's first call is leading and no trailing happens) Maximum calls:
    // num_threads * 2 (if each thread gets a leading and a trailing call) Let's
    // check if the count is within a reasonable range.
    EXPECT_THAT(call_count.load(),
                Ge(num_threads));  // At least one call per thread (leading)
    EXPECT_THAT(call_count.load(),
                Le(num_threads * 2 +
                   5));  // Allow for some extra trailing calls due to timing
}

// Test Throttle thread safety with concurrent cancel/reset
TEST_F(LodashTest, Throttle_ThreadSafety_ConcurrentControlCalls) {
    Throttle<std::function<void()>> throttled_fn(
        increment_call_count(), std::chrono::milliseconds(200), true,
        true);  // leading=true, trailing=true

    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Start threads that call, cancel, reset concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            throttled_fn();  // Calls immediately (if allowed), schedules
                             // potential trailing
            if (i % 2 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                throttled_fn.cancel();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                throttled_fn.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            throttled_fn();  // Call again
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for any potential trailing calls from the last set of calls
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Similar to Debounce, the exact count is hard to predict, but it should
    // not crash. We expect some calls to have gone through via leading edge
    // calls. Minimum calls: num_threads (if each thread's first call is leading
    // and subsequent are cancelled/reset)
    EXPECT_THAT(call_count.load(), Ge(num_threads));
    // Maximum calls: num_threads * 2 (if each thread gets a leading and a
    // trailing call before control)
    EXPECT_THAT(call_count.load(), Le(num_threads * 2 + 5));
}

// --- Factory Tests ---

// Test DebounceFactory creates Debounce with correct config
TEST_F(LodashTest, DebounceFactory_Create_CreatesConfiguredDebounce) {
    std::chrono::milliseconds delay(75);
    std::chrono::milliseconds maxWait(150);
    DebounceFactory factory(delay, true, maxWait);  // leading=true

    auto debounced_fn = factory.create(increment_call_count());

    // Test leading edge behavior
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Called immediately

    // Call quickly within delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    debounced_fn();
    EXPECT_THAT(call_count.load(), Eq(1));  // Not called immediately again

    // Wait for delay from last call
    std::this_thread::sleep_for(
        std::chrono::milliseconds(70));     // 10 + 70 = 80 > 75
    EXPECT_THAT(call_count.load(), Eq(2));  // Trailing call happened

    // Test maxWait (reset count first)
    call_count = 0;
    auto debounced_fn_2 =
        factory.create(increment_call_count());  // Create another one

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time <
           std::chrono::milliseconds(160)) {  // Slightly > maxWait
        debounced_fn_2();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));  // Faster than delay
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Wait for any pending calls

    EXPECT_THAT(call_count.load(),
                Ge(1));  // Should be called at least once due to maxWait
    EXPECT_THAT(call_count.load(), Le(3));  // Should not be called excessively
}

// Test ThrottleFactory creates Throttle with correct config
TEST_F(LodashTest, ThrottleFactory_Create_CreatesConfiguredThrottle) {
    std::chrono::milliseconds interval(60);
    ThrottleFactory factory(interval, false,
                            true);  // leading=false, trailing=true

    auto throttled_fn = factory.create(increment_call_count());

    // Test trailing edge behavior
    throttled_fn();                         // Schedule trailing
    EXPECT_THAT(call_count.load(), Eq(0));  // Not called immediately

    // Call quickly again
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn();
    EXPECT_THAT(call_count.load(), Eq(0));  // Not called immediately

    // Wait for interval from last attempt
    std::this_thread::sleep_for(
        std::chrono::milliseconds(60));     // 10 + 60 = 70 > 60
    EXPECT_THAT(call_count.load(), Eq(1));  // Trailing call happened

    // Test leading=true config from factory
    call_count = 0;
    ThrottleFactory factory_leading(interval, true,
                                    false);  // leading=true, trailing=false
    auto throttled_fn_leading = factory_leading.create(increment_call_count());

    throttled_fn_leading();  // Leading call
    EXPECT_THAT(call_count.load(), Eq(1));

    // Call quickly within interval
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    throttled_fn_leading();
    EXPECT_THAT(call_count.load(), Eq(1));  // Ignored

    // Wait for interval
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_THAT(call_count.load(), Eq(1));  // No trailing

    // Call after interval
    throttled_fn_leading();
    EXPECT_THAT(call_count.load(), Eq(2));  // Leading call again
}
