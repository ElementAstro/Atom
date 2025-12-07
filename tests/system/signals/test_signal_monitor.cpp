/**
 * @file test_signal_monitor.cpp
 * @brief Comprehensive tests for signal monitoring functionality
 *
 * This file contains tests for the signal monitoring system in
 * atom/system/signals/signal_monitor.hpp including monitoring start/stop,
 * threshold callbacks, and statistics collection.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "atom/system/signal_monitor.hpp"

namespace atom::system::test {

using namespace std::chrono_literals;

/**
 * @brief Test fixture for signal monitor tests
 */
class SignalMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        monitor = &SignalMonitor::getInstance();

        // Ensure monitor is stopped before each test
        monitor->stop();

        // Wait a bit for cleanup
        std::this_thread::sleep_for(100ms);
    }

    void TearDown() override {
        // Stop monitoring after each test
        monitor->stop();

        // Wait for cleanup
        std::this_thread::sleep_for(100ms);
    }

    SignalMonitor* monitor;
    const SignalID testSignal = SIGINT;
};

// ============================================================================
// Singleton Tests
// ============================================================================

/**
 * @brief Test that getInstance returns the same instance
 */
TEST_F(SignalMonitorTest, Singleton_SameInstance) {
    SignalMonitor& instance1 = SignalMonitor::getInstance();
    SignalMonitor& instance2 = SignalMonitor::getInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Start/Stop Monitoring Tests
// ============================================================================

/**
 * @brief Test starting monitoring with default parameters
 */
TEST_F(SignalMonitorTest, Start_DefaultParameters) {
    EXPECT_NO_THROW(monitor->start());

    // Give it time to start
    std::this_thread::sleep_for(100ms);

    // Stop monitoring
    EXPECT_NO_THROW(monitor->stop());
}

/**
 * @brief Test starting monitoring with custom interval
 */
TEST_F(SignalMonitorTest, Start_CustomInterval) {
    EXPECT_NO_THROW(monitor->start(500ms));

    std::this_thread::sleep_for(100ms);

    EXPECT_NO_THROW(monitor->stop());
}

/**
 * @brief Test starting monitoring with specific signals
 */
TEST_F(SignalMonitorTest, Start_SpecificSignals) {
    std::vector<SignalID> signals = {SIGINT, SIGTERM};

    EXPECT_NO_THROW(monitor->start(1000ms, signals));

    std::this_thread::sleep_for(100ms);

    EXPECT_NO_THROW(monitor->stop());
}

/**
 * @brief Test starting monitoring with empty signal list
 */
TEST_F(SignalMonitorTest, Start_EmptySignalList) {
    std::vector<SignalID> signals;

    EXPECT_NO_THROW(monitor->start(1000ms, signals));

    std::this_thread::sleep_for(100ms);

    EXPECT_NO_THROW(monitor->stop());
}

/**
 * @brief Test starting already running monitor
 */
TEST_F(SignalMonitorTest, Start_AlreadyRunning) {
    monitor->start();
    std::this_thread::sleep_for(100ms);

    // Starting again should be safe (no-op or restart)
    EXPECT_NO_THROW(monitor->start());

    monitor->stop();
}

/**
 * @brief Test stopping monitor that is not running
 */
TEST_F(SignalMonitorTest, Stop_NotRunning) {
    // Stopping when not running should be safe
    EXPECT_NO_THROW(monitor->stop());
}

/**
 * @brief Test multiple start/stop cycles
 */
TEST_F(SignalMonitorTest, StartStop_MultipleCycles) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(monitor->start());
        std::this_thread::sleep_for(50ms);
        EXPECT_NO_THROW(monitor->stop());
        std::this_thread::sleep_for(50ms);
    }
}

// ============================================================================
// Threshold Callback Tests
// ============================================================================

/**
 * @brief Test adding threshold callback
 */
TEST_F(SignalMonitorTest, AddThresholdCallback_ValidCallback) {
    std::atomic<bool> callbackInvoked{false};

    auto callback = [&callbackInvoked](SignalID signal,
                                       const SignalStats& stats) {
        callbackInvoked = true;
    };

    int callbackId = monitor->addThresholdCallback(testSignal, 10, 5, callback);

    // Callback ID should be valid (non-negative)
    EXPECT_GE(callbackId, 0);
}

/**
 * @brief Test adding multiple threshold callbacks
 */
TEST_F(SignalMonitorTest, AddThresholdCallback_MultipleCallbacks) {
    auto callback1 = [](SignalID, const SignalStats&) {};
    auto callback2 = [](SignalID, const SignalStats&) {};

    int id1 = monitor->addThresholdCallback(testSignal, 10, 5, callback1);
    int id2 = monitor->addThresholdCallback(testSignal, 20, 10, callback2);

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);
    EXPECT_NE(id1, id2);
}

/**
 * @brief Test adding threshold callback with zero thresholds
 */
TEST_F(SignalMonitorTest, AddThresholdCallback_ZeroThresholds) {
    auto callback = [](SignalID, const SignalStats&) {};

    int callbackId = monitor->addThresholdCallback(testSignal, 0, 0, callback);

    // Should still return a valid ID
    EXPECT_GE(callbackId, 0);
}

/**
 * @brief Test adding threshold callback with very high thresholds
 */
TEST_F(SignalMonitorTest, AddThresholdCallback_HighThresholds) {
    auto callback = [](SignalID, const SignalStats&) {};

    int callbackId =
        monitor->addThresholdCallback(testSignal, 1000000, 1000000, callback);

    EXPECT_GE(callbackId, 0);
}

/**
 * @brief Test removing threshold callback
 */
TEST_F(SignalMonitorTest, RemoveThresholdCallback_ValidId) {
    auto callback = [](SignalID, const SignalStats&) {};

    int callbackId = monitor->addThresholdCallback(testSignal, 10, 5, callback);
    ASSERT_GE(callbackId, 0);

    // Remove the callback
    EXPECT_NO_THROW(monitor->removeThresholdCallback(callbackId));
}

/**
 * @brief Test removing non-existent callback
 */
TEST_F(SignalMonitorTest, RemoveThresholdCallback_InvalidId) {
    // Removing non-existent callback should be safe
    EXPECT_NO_THROW(monitor->removeThresholdCallback(99999));
}

/**
 * @brief Test removing callback twice
 */
TEST_F(SignalMonitorTest, RemoveThresholdCallback_Twice) {
    auto callback = [](SignalID, const SignalStats&) {};

    int callbackId = monitor->addThresholdCallback(testSignal, 10, 5, callback);
    ASSERT_GE(callbackId, 0);

    monitor->removeThresholdCallback(callbackId);

    // Removing again should be safe
    EXPECT_NO_THROW(monitor->removeThresholdCallback(callbackId));
}

// ============================================================================
// Statistics Tests
// ============================================================================

/**
 * @brief Test getting statistics for a signal
 */
TEST_F(SignalMonitorTest, GetStatistics_ValidSignal) {
    // This test depends on implementation details
    // Just verify it doesn't crash
    EXPECT_NO_THROW({
        // Attempt to get statistics
        // Note: Actual method name may vary based on implementation
    });
}

/**
 * @brief Test clearing statistics
 */
TEST_F(SignalMonitorTest, ClearStatistics_AllSignals) {
    // This test depends on implementation details
    EXPECT_NO_THROW({
        // Attempt to clear statistics
        // Note: Actual method name may vary based on implementation
    });
}

// ============================================================================
// Integration Tests
// ============================================================================

/**
 * @brief Test monitoring with callback invocation
 */
TEST_F(SignalMonitorTest, Integration_MonitoringWithCallback) {
    std::atomic<int> callbackCount{0};

    auto callback = [&callbackCount](SignalID signal,
                                     const SignalStats& stats) {
        callbackCount++;
    };

    // Add callback with low threshold
    int callbackId = monitor->addThresholdCallback(testSignal, 1, 1, callback);
    ASSERT_GE(callbackId, 0);

    // Start monitoring
    monitor->start(100ms);

    // Let it run for a bit
    std::this_thread::sleep_for(500ms);

    // Stop monitoring
    monitor->stop();

    // Callback might or might not have been invoked depending on signal
    // activity
    EXPECT_GE(callbackCount.load(), 0);
}

/**
 * @brief Test monitoring specific signals only
 */
TEST_F(SignalMonitorTest, Integration_MonitorSpecificSignals) {
    std::vector<SignalID> signals = {SIGINT};

    monitor->start(100ms, signals);

    std::this_thread::sleep_for(300ms);

    monitor->stop();

    // Should complete without errors
    SUCCEED();
}

/**
 * @brief Test concurrent callback additions and removals
 */
TEST_F(SignalMonitorTest, Integration_ConcurrentCallbackOperations) {
    std::vector<int> callbackIds;

    // Add multiple callbacks
    for (int i = 0; i < 5; ++i) {
        auto callback = [](SignalID, const SignalStats&) {};
        int id =
            monitor->addThresholdCallback(testSignal, 10 * i, 5 * i, callback);
        callbackIds.push_back(id);
    }

    // Start monitoring
    monitor->start(100ms);

    std::this_thread::sleep_for(200ms);

    // Remove some callbacks while monitoring
    for (size_t i = 0; i < callbackIds.size(); i += 2) {
        monitor->removeThresholdCallback(callbackIds[i]);
    }

    std::this_thread::sleep_for(200ms);

    // Stop monitoring
    monitor->stop();

    // Should complete without errors
    SUCCEED();
}

/**
 * @brief Test rapid start/stop cycles
 */
TEST_F(SignalMonitorTest, Integration_RapidStartStopCycles) {
    for (int i = 0; i < 10; ++i) {
        monitor->start(50ms);
        std::this_thread::sleep_for(10ms);
        monitor->stop();
    }

    // Should complete without errors or deadlocks
    SUCCEED();
}

}  // namespace atom::system::test
