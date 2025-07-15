// filepath: /home/max/Atom-1/atom/utils/test_stopwatcher.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "stopwatcher.hpp"

// Mock for LOG_F to avoid actual logging during tests
#define LOG_F(level, ...) ((void)0)

using namespace atom::utils;
using namespace std::chrono_literals;

// Helper function to check formatted time string
bool isFormattedTimeValid(const std::string& formatted) {
    // Format should be "HH:MM:SS.mmm"
    if (formatted.size() != 12)
        return false;
    if (formatted[2] != ':' || formatted[5] != ':' || formatted[8] != '.')
        return false;

    // Check that all other positions are digits
    for (size_t i = 0; i < formatted.size(); ++i) {
        if (i != 2 && i != 5 && i != 8) {
            if (!std::isdigit(formatted[i]))
                return false;
        }
    }

    return true;
}

class StopWatcherTest : public ::testing::Test {
protected:
    void SetUp() override { stopwatcher = std::make_unique<StopWatcher>(); }

    void TearDown() override { stopwatcher.reset(); }

    // Helper to sleep with high precision
    void preciseSleep(std::chrono::milliseconds duration) {
        auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::high_resolution_clock::now() - start < duration) {
            // Busy wait to get more precise timing
            std::this_thread::yield();
        }
    }

    std::unique_ptr<StopWatcher> stopwatcher;
};

// Test constructor and initial state
TEST_F(StopWatcherTest, Constructor) {
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);
    EXPECT_FALSE(stopwatcher->isRunning());
    EXPECT_EQ(stopwatcher->getLapCount(), 0);
    EXPECT_EQ(stopwatcher->elapsedMilliseconds(), 0.0);
    EXPECT_EQ(stopwatcher->elapsedSeconds(), 0.0);
}

// Test start method
TEST_F(StopWatcherTest, Start) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);
    EXPECT_TRUE(stopwatcher->isRunning());
}

// Test start when already running
TEST_F(StopWatcherTest, StartWhenRunning) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    EXPECT_THROW((void)stopwatcher->start(),
                 std::runtime_error);  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);
}

// Test stop method
TEST_F(StopWatcherTest, Stop) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);
    EXPECT_TRUE(stopwatcher->stop());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Stopped);
    EXPECT_FALSE(stopwatcher->isRunning());

    // Check that time has elapsed
    double elapsed = stopwatcher->elapsedMilliseconds();
    EXPECT_GE(elapsed, 50.0);
    EXPECT_LT(elapsed, 150.0);  // Allow some margin for test execution
}

// Test stop when not running
TEST_F(StopWatcherTest, StopWhenNotRunning) {
    EXPECT_FALSE(stopwatcher->stop());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);

    (void)stopwatcher->start();  // Suppress nodiscard warning
    (void)stopwatcher->stop();   // Suppress nodiscard warning
    EXPECT_FALSE(stopwatcher->stop());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Stopped);
}

// Test pause and resume
TEST_F(StopWatcherTest, PauseAndResume) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);
    EXPECT_TRUE(stopwatcher->pause());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Paused);

    double pausedTime = stopwatcher->elapsedMilliseconds();
    EXPECT_GE(pausedTime, 50.0);
    EXPECT_LT(pausedTime, 150.0);

    // Time should not advance while paused
    preciseSleep(50ms);
    double stillPausedTime = stopwatcher->elapsedMilliseconds();
    EXPECT_NEAR(pausedTime, stillPausedTime, 1.0);

    // Resume and check time advances again
    EXPECT_TRUE(stopwatcher->resume());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);
    preciseSleep(50ms);

    double finalTime = stopwatcher->elapsedMilliseconds();
    EXPECT_GE(finalTime, pausedTime + 50.0);
    EXPECT_LT(finalTime, pausedTime + 150.0);
}

// Test pause when not running
TEST_F(StopWatcherTest, PauseWhenNotRunning) {
    EXPECT_FALSE(stopwatcher->pause());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);

    (void)stopwatcher->start();  // Suppress nodiscard warning
    (void)stopwatcher->stop();   // Suppress nodiscard warning
    EXPECT_FALSE(stopwatcher->pause());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Stopped);
}

// Test resume when not paused
TEST_F(StopWatcherTest, ResumeWhenNotPaused) {
    EXPECT_FALSE(stopwatcher->resume());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);

    (void)stopwatcher->start();  // Suppress nodiscard warning
    EXPECT_FALSE(stopwatcher->resume());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);

    (void)stopwatcher->stop();  // Suppress nodiscard warning
    EXPECT_FALSE(stopwatcher->resume());
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Stopped);
}

// Test reset method
TEST_F(StopWatcherTest, Reset) {
    // Start, run, and record a lap
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);
    (void)stopwatcher->lap();  // Suppress nodiscard warning
    preciseSleep(50ms);

    // Reset and verify state
    stopwatcher->reset();
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);
    EXPECT_FALSE(stopwatcher->isRunning());
    EXPECT_EQ(stopwatcher->getLapCount(), 0);
    EXPECT_EQ(stopwatcher->elapsedMilliseconds(), 0.0);
    EXPECT_EQ(stopwatcher->elapsedSeconds(), 0.0);

    // Should be able to start again
    EXPECT_NO_THROW((void)stopwatcher->start());  // Suppress nodiscard warning
}

// Test lap method
TEST_F(StopWatcherTest, Lap) {
    (void)stopwatcher->start();  // Suppress nodiscard warning

    // Record multiple laps
    preciseSleep(50ms);
    double lap1 =
        stopwatcher->lap().value();  // Correctly get value from expected
    EXPECT_GE(lap1, 50.0);
    EXPECT_LT(lap1, 150.0);

    preciseSleep(75ms);
    double lap2 =
        stopwatcher->lap().value();  // Correctly get value from expected
    EXPECT_GE(lap2, lap1 + 75.0);
    EXPECT_LT(lap2, lap1 + 175.0);

    // Check lap counts and values
    EXPECT_EQ(stopwatcher->getLapCount(), 2);
    auto lapTimes = stopwatcher->getLapTimes();
    EXPECT_EQ(lapTimes.size(), 2);
    EXPECT_NEAR(lapTimes[0], lap1, 1.0);
    EXPECT_NEAR(lapTimes[1], lap2, 1.0);
}

// Test lap when not running
TEST_F(StopWatcherTest, LapWhenNotRunning) {
    EXPECT_THROW((void)stopwatcher->lap(),
                 std::runtime_error);  // Suppress nodiscard warning

    (void)stopwatcher->start();  // Suppress nodiscard warning
    (void)stopwatcher->stop();   // Suppress nodiscard warning
    EXPECT_THROW((void)stopwatcher->lap(),
                 std::runtime_error);  // Suppress nodiscard warning

    (void)stopwatcher->start();  // Suppress nodiscard warning
    (void)stopwatcher->pause();  // Suppress nodiscard warning
    EXPECT_THROW((void)stopwatcher->lap(),
                 std::runtime_error);  // Suppress nodiscard warning
}

// Test elapsedMilliseconds and elapsedSeconds
TEST_F(StopWatcherTest, ElapsedTime) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(100ms);

    double milliseconds = stopwatcher->elapsedMilliseconds();
    double seconds = stopwatcher->elapsedSeconds();

    EXPECT_GE(milliseconds, 100.0);
    EXPECT_LT(milliseconds, 200.0);

    // Verify seconds is milliseconds / 1000
    EXPECT_NEAR(seconds, milliseconds / 1000.0, 0.001);
}

// Test elapsedFormatted
TEST_F(StopWatcherTest, ElapsedFormatted) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(1234ms);        // 1.234 seconds

    std::string formatted = stopwatcher->elapsedFormatted();
    EXPECT_TRUE(isFormattedTimeValid(formatted));

    // For 1.234 seconds, should be close to "00:00:01.234"
    // But allow some margin for test execution time
    EXPECT_EQ(formatted.substr(0, 8), "00:00:01");
}

// Test getAverageLapTime
TEST_F(StopWatcherTest, GetAverageLapTime) {
    // No laps
    EXPECT_EQ(stopwatcher->getAverageLapTime(), 0.0);

    // With laps
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(100ms);
    (void)stopwatcher->lap();  // ~100ms // Suppress nodiscard warning
    preciseSleep(200ms);
    (void)stopwatcher->lap();  // ~300ms // Suppress nodiscard warning
    preciseSleep(300ms);
    (void)stopwatcher->lap();  // ~600ms // Suppress nodiscard warning

    // Average should be around (100 + 300 + 600) / 3 = 333.33ms
    double avg = stopwatcher->getAverageLapTime();
    EXPECT_GE(avg, 300.0);
    EXPECT_LE(avg, 400.0);
}

// Test multiple start-stop cycles
TEST_F(StopWatcherTest, MultipleStartStopCycles) {
    // First cycle
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);
    (void)stopwatcher->stop();  // Suppress nodiscard warning
    double time1 = stopwatcher->elapsedMilliseconds();

    // Second cycle - should reset time
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(100ms);
    (void)stopwatcher->stop();  // Suppress nodiscard warning
    double time2 = stopwatcher->elapsedMilliseconds();

    // time2 should reflect only the second interval, not cumulative
    EXPECT_GE(time2, 100.0);
    EXPECT_LT(time2, 200.0);

    // time2 should be independent of time1
    EXPECT_NE(time1, time2);
}

// Test callback registration and execution
TEST_F(StopWatcherTest, Callbacks) {
    std::atomic<bool> callbackExecuted = false;

    // Register a callback to execute after 50ms
    (void)stopwatcher->registerCallback(  // Suppress nodiscard warning
        [&callbackExecuted]() { callbackExecuted = true; }, 50);

    // Run for 100ms
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(100ms);
    (void)stopwatcher->stop();  // Suppress nodiscard warning

    // Callback should have executed
    EXPECT_TRUE(callbackExecuted);
}

// Test callback with invalid interval
TEST_F(StopWatcherTest, CallbackInvalidInterval) {
    EXPECT_THROW((void)stopwatcher->registerCallback(
                     []() {}, -10),  // Suppress nodiscard warning
                 std::invalid_argument);
}

// Test multiple callbacks
TEST_F(StopWatcherTest, MultipleCallbacks) {
    std::atomic<int> callbacksExecuted = 0;

    // Register callbacks at different times
    (void)stopwatcher->registerCallback(  // Suppress nodiscard warning
        [&callbacksExecuted]() { callbacksExecuted++; }, 50);

    (void)stopwatcher->registerCallback(  // Suppress nodiscard warning
        [&callbacksExecuted]() { callbacksExecuted++; }, 150);

    // Callbacks that won't execute
    (void)stopwatcher->registerCallback(  // Suppress nodiscard warning
        [&callbacksExecuted]() { callbacksExecuted++; }, 250);

    // Run for 200ms - only the first two should execute
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(200ms);
    (void)stopwatcher->stop();  // Suppress nodiscard warning

    EXPECT_EQ(callbacksExecuted, 2);
}

// Test move operations
TEST_F(StopWatcherTest, MoveOperations) {
    // Start the original stopwatcher
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);

    // Move construct
    StopWatcher movedConstructor(std::move(*stopwatcher));
    EXPECT_TRUE(movedConstructor.isRunning());
    EXPECT_GE(movedConstructor.elapsedMilliseconds(), 50.0);

    // Create a new stopwatcher
    stopwatcher = std::make_unique<StopWatcher>();
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(50ms);

    // Move assign
    StopWatcher movedAssignment;
    movedAssignment = std::move(*stopwatcher);
    EXPECT_TRUE(movedAssignment.isRunning());
    EXPECT_GE(movedAssignment.elapsedMilliseconds(), 50.0);
}

// Test thread safety
TEST_F(StopWatcherTest, ThreadSafety) {
    (void)stopwatcher->start();  // Suppress nodiscard warning

    std::vector<std::thread> threads;
    std::atomic<int> successCount = 0;

    const int numThreads = 10;

    // Create threads that call methods concurrently
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                if (i % 4 == 0) {
                    // Just read elapsed time
                    double time = stopwatcher->elapsedMilliseconds();
                    if (time >= 0.0)
                        successCount++;
                } else if (i % 4 == 1) {
                    // Try to record a lap
                    try {
                        // Access the value from std::expected
                        (void)stopwatcher->lap();  // Suppress nodiscard warning
                        successCount++;
                    } catch (const std::exception&) {
                        // Might fail if stopwatch is stopped by another thread
                    }
                } else if (i % 4 == 2) {
                    // Pause/resume
                    if (stopwatcher->pause()) {
                        std::this_thread::sleep_for(5ms);
                        if (stopwatcher->resume())
                            successCount++;
                    }
                } else {
                    // Stop
                    if (stopwatcher->stop())
                        successCount++;
                }
            } catch (...) {
                // Ignore any exceptions from concurrent access
            }
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // At least some operations should have succeeded
    EXPECT_GT(successCount, 0);
}

// Test state transitions
TEST_F(StopWatcherTest, StateTransitions) {
    // Idle -> Running
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);
    (void)stopwatcher->start();  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);

    // Running -> Paused
    (void)stopwatcher->pause();  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Paused);

    // Paused -> Running
    (void)stopwatcher->resume();  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Running);

    // Running -> Stopped
    (void)stopwatcher->stop();  // Suppress nodiscard warning
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Stopped);

    // Stopped -> Idle (via reset)
    stopwatcher->reset();
    EXPECT_EQ(stopwatcher->getState(), StopWatcherState::Idle);
}

// Test elapsed time accuracy
TEST_F(StopWatcherTest, TimeAccuracy) {
    (void)stopwatcher->start();  // Suppress nodiscard warning

    // Sleep for 1 second
    std::this_thread::sleep_for(1000ms);

    double elapsed = stopwatcher->elapsedMilliseconds();

    // Allow 5% margin for sleep inaccuracy
    EXPECT_GE(elapsed, 950.0);
    EXPECT_LE(elapsed, 1050.0);
}

// Test multiple pauses
TEST_F(StopWatcherTest, MultiplePauses) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(100ms);

    // First pause
    (void)stopwatcher->pause();  // Suppress nodiscard warning
    double time1 = stopwatcher->elapsedMilliseconds();
    preciseSleep(50ms);  // Should not count

    // Resume
    (void)stopwatcher->resume();  // Suppress nodiscard warning
    preciseSleep(100ms);

    // Second pause
    (void)stopwatcher->pause();  // Suppress nodiscard warning
    double time2 = stopwatcher->elapsedMilliseconds();

    // time2 should be about 200ms (100ms before first pause + 100ms after
    // resume)
    EXPECT_GE(time2, time1 + 100.0);
    EXPECT_LE(time2, time1 + 150.0);
}

// Test with very short intervals
TEST_F(StopWatcherTest, VeryShortIntervals) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(1ms);
    double time = stopwatcher->elapsedMilliseconds();

    // Even with very short intervals, time should be positive
    EXPECT_GT(time, 0.0);
    EXPECT_LT(time, 50.0);  // Allow generous margin for test execution
}

// Test with long running operations
TEST_F(StopWatcherTest, LongRunning) {
    (void)stopwatcher->start();  // Suppress nodiscard warning
    preciseSleep(2000ms);        // 2 seconds

    double milliseconds = stopwatcher->elapsedMilliseconds();
    double seconds = stopwatcher->elapsedSeconds();
    std::string formatted = stopwatcher->elapsedFormatted();

    EXPECT_GE(milliseconds, 2000.0);
    EXPECT_LE(milliseconds, 2200.0);
    EXPECT_NEAR(seconds, milliseconds / 1000.0, 0.001);

    // Formatted time should start with "00:00:02"
    EXPECT_EQ(formatted.substr(0, 8), "00:00:02");
}
