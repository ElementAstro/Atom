/*
 * test_trigger_new.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for Atom Async Event Triggers
Tests the event-driven callback system with parameter support.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/sync/trigger.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::sync::test {

// ============================================================================
// Trigger Tests
// ============================================================================

class TriggerTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override {
        SynchronizationTestFixture::SetUp();
        // Additional trigger specific setup
    }

    void TearDown() override {
        // Trigger specific cleanup
        SynchronizationTestFixture::TearDown();
    }
};

TEST_F(TriggerTest, BasicTriggerOperations) {
    Trigger<int> trigger;
    std::atomic<int> callbackValue{0};
    std::atomic<bool> callbackCalled{false};

    // Register a callback
    auto callback = [&callbackValue, &callbackCalled](int value) {
        callbackValue = value;
        callbackCalled = true;
    };

    auto callbackId = trigger.registerCallback("test_event", callback);
    EXPECT_GT(callbackId, 0);

    // Trigger the event
    auto triggeredCount = trigger.trigger("test_event", 42);
    EXPECT_EQ(triggeredCount, 1);

    // Wait a bit for callback to execute
    std::this_thread::sleep_for(10ms);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(callbackValue, 42);

    // Test unregistering callback
    bool unregistered = trigger.unregisterCallback("test_event", callbackId);
    EXPECT_TRUE(unregistered);

    // Reset for next test
    callbackCalled = false;
    callbackValue = 0;

    // Trigger again - should not call callback
    triggeredCount = trigger.trigger("test_event", 100);
    EXPECT_EQ(triggeredCount, 0);

    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(callbackCalled);
    EXPECT_EQ(callbackValue, 0);
}

TEST_F(TriggerTest, MultipleCallbacks) {
    Trigger<std::string> trigger;
    std::atomic<int> callback1Called{0};
    std::atomic<int> callback2Called{0};
    std::string callback1Value, callback2Value;

    // Register multiple callbacks for the same event
    auto callback1 = [&callback1Called,
                      &callback1Value](const std::string& value) {
        callback1Value = value;
        callback1Called++;
    };

    auto callback2 = [&callback2Called,
                      &callback2Value](const std::string& value) {
        callback2Value = value;
        callback2Called++;
    };

    auto id1 = trigger.registerCallback("multi_event", callback1);
    auto id2 = trigger.registerCallback("multi_event", callback2);

    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_NE(id1, id2);

    // Trigger the event
    auto triggeredCount = trigger.trigger("multi_event", "test_data");
    EXPECT_EQ(triggeredCount, 2);

    // Wait for callbacks to execute
    std::this_thread::sleep_for(20ms);

    EXPECT_EQ(callback1Called, 1);
    EXPECT_EQ(callback2Called, 1);
    EXPECT_EQ(callback1Value, "test_data");
    EXPECT_EQ(callback2Value, "test_data");
}

TEST_F(TriggerTest, CallbackPriorities) {
    Trigger<int> trigger;
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // Register callbacks with different priorities
    auto highPriorityCallback = [&executionOrder, &orderMutex](int) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);  // High priority = 1
    };

    auto normalPriorityCallback = [&executionOrder, &orderMutex](int) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2);  // Normal priority = 2
    };

    auto lowPriorityCallback = [&executionOrder, &orderMutex](int) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(3);  // Low priority = 3
    };

    // Register in reverse priority order to test sorting
    [[maybe_unused]] auto id1 =
        trigger.registerCallback("priority_event", lowPriorityCallback,
                                 Trigger<int>::CallbackPriority::Low);
    [[maybe_unused]] auto id2 =
        trigger.registerCallback("priority_event", normalPriorityCallback,
                                 Trigger<int>::CallbackPriority::Normal);
    [[maybe_unused]] auto id3 =
        trigger.registerCallback("priority_event", highPriorityCallback,
                                 Trigger<int>::CallbackPriority::High);

    // Trigger the event
    auto triggeredCount = trigger.trigger("priority_event", 42);
    EXPECT_EQ(triggeredCount, 3);

    // Wait for callbacks to execute
    std::this_thread::sleep_for(50ms);

    // Check execution order (High=0, Normal=1, Low=2 in enum)
    EXPECT_EQ(executionOrder.size(), 3);
    EXPECT_EQ(executionOrder[0], 1);  // High priority first
    EXPECT_EQ(executionOrder[1], 2);  // Normal priority second
    EXPECT_EQ(executionOrder[2], 3);  // Low priority last
}

TEST_F(TriggerTest, EmptyEventName) {
    Trigger<int> trigger;

    // Test triggering empty event name
    auto triggeredCount = trigger.trigger("", 42);
    EXPECT_EQ(triggeredCount, 0);

    // Test registering callback with empty event name
    auto callback = [](int) {};
    EXPECT_THROW(trigger.registerCallback("", callback),
                 atom::async::TriggerException);
}

TEST_F(TriggerTest, NonExistentEvent) {
    Trigger<int> trigger;

    // Test triggering non-existent event
    auto triggeredCount = trigger.trigger("non_existent", 42);
    EXPECT_EQ(triggeredCount, 0);

    // Test unregistering from non-existent event
    bool unregistered = trigger.unregisterCallback("non_existent", 123);
    EXPECT_FALSE(unregistered);
}

TEST_F(TriggerTest, HasCallbacks) {
    Trigger<int> trigger;

    // Initially no callbacks
    EXPECT_FALSE(trigger.hasCallbacks("test_event"));
    EXPECT_EQ(trigger.callbackCount("test_event"), 0);

    // Register a callback
    auto callback = [](int) {};
    auto callbackId = trigger.registerCallback("test_event", callback);

    EXPECT_TRUE(trigger.hasCallbacks("test_event"));
    EXPECT_EQ(trigger.callbackCount("test_event"), 1);

    // Unregister callback
    trigger.unregisterCallback("test_event", callbackId);

    EXPECT_FALSE(trigger.hasCallbacks("test_event"));
    EXPECT_EQ(trigger.callbackCount("test_event"), 0);
}

TEST_F(TriggerTest, UnregisterAllCallbacks) {
    Trigger<int> trigger;

    // Register multiple callbacks
    auto callback1 = [](int) {};
    auto callback2 = [](int) {};
    auto callback3 = [](int) {};

    trigger.registerCallback("test_event", callback1);
    trigger.registerCallback("test_event", callback2);
    trigger.registerCallback("test_event", callback3);

    EXPECT_EQ(trigger.callbackCount("test_event"), 3);

    // Unregister all callbacks for the event
    auto unregisteredCount = trigger.unregisterAllCallbacks("test_event");
    EXPECT_EQ(unregisteredCount, 3);

    EXPECT_FALSE(trigger.hasCallbacks("test_event"));
    EXPECT_EQ(trigger.callbackCount("test_event"), 0);
}

}  // namespace atom::async::sync::test
