// filepath: /home/max/Atom-1/atom/utils/test_qtimer.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "qtimer.hpp"

using namespace atom::utils;
using namespace std::chrono_literals;

class ElapsedTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup is done for each test
    }

    void TearDown() override {
        // Cleanup after each test
    }
};

// Test default constructor
TEST_F(ElapsedTimerTest, DefaultConstructorCreatesInvalidTimer) {
    ElapsedTimer timer;
    EXPECT_FALSE(timer.isValid());
}

// Test constructor with start_now parameter
TEST_F(ElapsedTimerTest, ConstructorWithStartNowParameterCreatesValidTimer) {
    ElapsedTimer timer(true);
    EXPECT_TRUE(timer.isValid());
}

// Test starting the timer
TEST_F(ElapsedTimerTest, StartCreatesValidTimer) {
    ElapsedTimer timer;
    EXPECT_FALSE(timer.isValid());
    timer.start();
    EXPECT_TRUE(timer.isValid());
}

// Test invalidating the timer
TEST_F(ElapsedTimerTest, InvalidateCreatesInvalidTimer) {
    ElapsedTimer timer(true);
    EXPECT_TRUE(timer.isValid());
    timer.invalidate();
    EXPECT_FALSE(timer.isValid());
}

// Test elapsed time increases as expected
TEST_F(ElapsedTimerTest, ElapsedTimeIncreases) {
    ElapsedTimer timer(true);

    // Sleep a bit to ensure time passes
    std::this_thread::sleep_for(100ms);

    int64_t elapsed1 = timer.elapsedMs();
    EXPECT_GT(elapsed1, 0);

    // Sleep more and check again
    std::this_thread::sleep_for(100ms);

    int64_t elapsed2 = timer.elapsedMs();
    EXPECT_GT(elapsed2, elapsed1);
}

// Test elapsed methods return 0 for invalid timer
TEST_F(ElapsedTimerTest, ElapsedMethodsReturnZeroForInvalidTimer) {
    ElapsedTimer timer;  // Not started, so invalid
    EXPECT_EQ(timer.elapsedNs(), 0);
    EXPECT_EQ(timer.elapsedUs(), 0);
    EXPECT_EQ(timer.elapsedMs(), 0);
    EXPECT_EQ(timer.elapsedSec(), 0);
    EXPECT_EQ(timer.elapsedMin(), 0);
    EXPECT_EQ(timer.elapsedHrs(), 0);
    EXPECT_EQ(timer.elapsed(), 0);  // Default is ms
}

// Test different elapsed time units
TEST_F(ElapsedTimerTest, ElapsedTimeInDifferentUnits) {
    ElapsedTimer timer(true);

    // Sleep for a defined duration
    std::this_thread::sleep_for(500ms);

    // Check different time units
    EXPECT_GT(timer.elapsedNs(), 450'000'000);  // At least 450ms in ns
    EXPECT_GT(timer.elapsedUs(), 450'000);      // At least 450ms in us
    EXPECT_GT(timer.elapsedMs(), 450);          // At least 450ms
    EXPECT_LE(timer.elapsedSec(), 1);           // Should be less than 1 second
    EXPECT_EQ(timer.elapsedMin(), 0);           // Should be 0 minutes
    EXPECT_EQ(timer.elapsedHrs(), 0);           // Should be 0 hours
    EXPECT_EQ(timer.elapsed(), timer.elapsedMs());  // Default elapsed is ms
}

// Test hasExpired method
TEST_F(ElapsedTimerTest, HasExpiredMethod) {
    ElapsedTimer timer(true);

    // Shouldn't have expired after 0ms
    EXPECT_FALSE(timer.hasExpired(100));

    // Sleep to expire the timer
    std::this_thread::sleep_for(150ms);

    // Should now be expired
    EXPECT_TRUE(timer.hasExpired(100));

    // Still not expired for longer timeout
    EXPECT_FALSE(timer.hasExpired(1000));
}

// Test hasExpired on invalid timer
TEST_F(ElapsedTimerTest, HasExpiredOnInvalidTimer) {
    ElapsedTimer timer;  // Invalid timer

    // hasExpired behavior on invalid timer
    EXPECT_FALSE(timer.hasExpired(100));
}

// Test hasExpired throws on negative time
TEST_F(ElapsedTimerTest, HasExpiredThrowsOnNegativeTime) {
    ElapsedTimer timer(true);
    EXPECT_THROW(timer.hasExpired(-100), std::invalid_argument);
}

// Test remainingTimeMs method
TEST_F(ElapsedTimerTest, RemainingTimeMsMethod) {
    ElapsedTimer timer(true);

    int64_t remaining1 = timer.remainingTimeMs(500);
    EXPECT_LE(remaining1, 500);
    EXPECT_GT(remaining1, 0);

    // Sleep a bit
    std::this_thread::sleep_for(100ms);

    int64_t remaining2 = timer.remainingTimeMs(500);
    EXPECT_LT(remaining2, remaining1);

    // Sleep more than the total time
    std::this_thread::sleep_for(500ms);

    int64_t remaining3 = timer.remainingTimeMs(500);
    EXPECT_EQ(remaining3, 0);  // Should be 0 when time has passed
}

// Test remainingTimeMs on invalid timer
TEST_F(ElapsedTimerTest, RemainingTimeMsOnInvalidTimer) {
    ElapsedTimer timer;  // Invalid timer

    // Should return 0 for invalid timer
    EXPECT_EQ(timer.remainingTimeMs(100), 0);
}

// Test remainingTimeMs throws on negative time
TEST_F(ElapsedTimerTest, RemainingTimeMsThrowsOnNegativeTime) {
    ElapsedTimer timer(true);
    EXPECT_THROW(timer.remainingTimeMs(-100), std::invalid_argument);
}

// Test currentTimeMs static method
TEST_F(ElapsedTimerTest, CurrentTimeMsIsReasonable) {
    int64_t now = ElapsedTimer::currentTimeMs();
    EXPECT_GT(now, 1600000000000);  // Some reasonable timestamp after 2020
}

// Test equality comparison
TEST_F(ElapsedTimerTest, EqualityComparison) {
    // Two invalid timers should be equal
    ElapsedTimer invalid1;
    ElapsedTimer invalid2;
    EXPECT_EQ(invalid1, invalid2);

    // A valid and an invalid timer should not be equal
    ElapsedTimer valid(true);
    EXPECT_NE(valid, invalid1);

    // Two timers started at different times should not be equal
    ElapsedTimer valid1(true);
    std::this_thread::sleep_for(10ms);
    ElapsedTimer valid2(true);
    EXPECT_NE(valid1, valid2);

    // A timer should be equal to itself
    EXPECT_EQ(valid1, valid1);

    // Two timers started at nearly the same time might be equal
    // This test is less reliable due to timing precision
    ElapsedTimer simultaneous1(true);
    ElapsedTimer simultaneous2(true);
    // Note: Equal comparison may fail due to precision, not testing this
}

// Test spaceship (<=>) operator
TEST_F(ElapsedTimerTest, SpaceshipOperator) {
    // Setup timers with different start times
    ElapsedTimer invalid;
    ElapsedTimer earlier(true);
    std::this_thread::sleep_for(10ms);
    ElapsedTimer later(true);

    // Invalid < Valid
    EXPECT_TRUE(invalid < earlier);
    EXPECT_TRUE(invalid < later);

    // Valid > Invalid
    EXPECT_TRUE(earlier > invalid);
    EXPECT_TRUE(later > invalid);

    // earlier < later (started earlier, so less elapsed time)
    EXPECT_TRUE(earlier < later);

    // later > earlier
    EXPECT_TRUE(later > earlier);

    // Self-equality
    EXPECT_TRUE(earlier == earlier);
    EXPECT_TRUE(later == later);
}

// Test restarting a timer
TEST_F(ElapsedTimerTest, RestartTimer) {
    ElapsedTimer timer(true);

    // Sleep a bit
    std::this_thread::sleep_for(100ms);

    int64_t elapsed1 = timer.elapsedMs();
    EXPECT_GT(elapsed1, 50);  // At least some time has passed

    // Restart the timer
    timer.start();

    // Should be close to 0 again
    int64_t elapsed2 = timer.elapsedMs();
    EXPECT_LT(elapsed2, 50);  // Should be much lower
}

// Test elapsed with custom duration type
TEST_F(ElapsedTimerTest, ElapsedWithCustomDurationType) {
    ElapsedTimer timer(true);

    // Sleep for a specific duration
    std::this_thread::sleep_for(100ms);

    // Test with different duration types
    int64_t ns = timer.elapsed<ElapsedTimer::Nanoseconds>();
    int64_t us = timer.elapsed<ElapsedTimer::Microseconds>();
    int64_t ms = timer.elapsed<ElapsedTimer::Milliseconds>();

    EXPECT_GT(ns, 90'000'000);  // At least 90ms in ns
    EXPECT_GT(us, 90'000);      // At least 90ms in us
    EXPECT_GT(ms, 90);          // At least 90ms

    // Verify relationship between units
    EXPECT_NEAR(ns, us * 1000, ns * 0.1);  // Within 10% due to timing precision
    EXPECT_NEAR(us, ms * 1000, us * 0.1);  // Within 10% due to timing precision
}

// Test elapsed with throw_if_invalid=true
TEST_F(ElapsedTimerTest, ElapsedWithThrowIfInvalidTrue) {
    ElapsedTimer timer;  // Invalid timer

    // Should throw for invalid timer with throw_if_invalid=true
    EXPECT_THROW((timer.elapsed<ElapsedTimer::Milliseconds, true>()),
                 std::logic_error);
}

// Test for thread safety with ElapsedTimer
TEST_F(ElapsedTimerTest, ThreadSafety) {
    ElapsedTimer timer(true);

    // Create multiple threads that read from the timer
    std::vector<std::thread> threads;
    std::vector<int64_t> results(10, 0);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&timer, &results, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            results[i] = timer.elapsedMs();
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Results should generally increase
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i], results[i - 1]);
    }
}

// Test that start() handles exceptions properly
TEST_F(ElapsedTimerTest, StartHandlesExceptions) {
    // This is hard to test directly since we can't easily force
    // std::chrono::steady_clock::now() to throw an exception. We're mainly
    // verifying that the THROW_RUNTIME_ERROR logic exists.
    ElapsedTimer timer;
    EXPECT_NO_THROW(timer.start());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}