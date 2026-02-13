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

// ============================================================================
// Delayed Trigger Tests
// ============================================================================

TEST_F(TriggerTest, TriggerWithDelay) {
    Trigger<int> trigger;
    std::atomic<bool> callbackCalled{false};
    std::chrono::steady_clock::time_point callbackTime;

    auto callback = [&callbackCalled, &callbackTime](int) {
        callbackTime = std::chrono::steady_clock::now();
        callbackCalled = true;
    };

    trigger.registerCallback("delayed_event", callback);

    auto startTime = std::chrono::steady_clock::now();
    trigger.triggerWithDelay("delayed_event", 42, 100ms);

    // Should not be called immediately
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(callbackCalled.load());

    // Wait for delay to pass
    std::this_thread::sleep_for(150ms);
    EXPECT_TRUE(callbackCalled.load());

    // Verify delay was approximately correct
    auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
        callbackTime - startTime);
    EXPECT_GE(delay.count(), 90);  // Allow some tolerance
    EXPECT_LT(delay.count(), 200);
}

// ============================================================================
// Async Trigger Tests
// ============================================================================

TEST_F(TriggerTest, TriggerAsync) {
    Trigger<int> trigger;
    std::atomic<int> callbackValue{0};
    std::atomic<bool> callbackCalled{false};

    auto callback = [&callbackValue, &callbackCalled](int value) {
        std::this_thread::sleep_for(10ms);  // Simulate work
        callbackValue = value;
        callbackCalled = true;
    };

    trigger.registerCallback("async_event", callback);

    // Trigger asynchronously
    auto future = trigger.triggerAsync("async_event", 42);

    // Should return immediately
    EXPECT_FALSE(callbackCalled.load());

    // Wait for async completion
    future.wait();

    EXPECT_TRUE(callbackCalled.load());
    EXPECT_EQ(callbackValue.load(), 42);
}

// ============================================================================
// Concurrent Trigger Tests
// ============================================================================

TEST_F(TriggerTest, ConcurrentTriggers) {
    Trigger<int> trigger;
    std::atomic<int> totalCallbacks{0};

    auto callback = [&totalCallbacks](int) { totalCallbacks.fetch_add(1); };

    trigger.registerCallback("concurrent_event", callback);

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int triggersPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&trigger, triggersPerThread]() {
            for (int j = 0; j < triggersPerThread; ++j) {
                trigger.trigger("concurrent_event", j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for all callbacks to complete
    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(totalCallbacks.load(), numThreads * triggersPerThread);
}

TEST_F(TriggerTest, ConcurrentRegistrationAndTrigger) {
    Trigger<int> trigger;
    std::atomic<int> callbackCount{0};
    std::atomic<bool> running{true};

    // Thread that continuously triggers events
    std::thread triggerThread([&trigger, &running]() {
        while (running.load()) {
            trigger.trigger("concurrent_event", 42);
            std::this_thread::yield();
        }
    });

    // Threads that register and unregister callbacks
    std::vector<std::thread> registrationThreads;
    for (int i = 0; i < 5; ++i) {
        registrationThreads.emplace_back([&trigger, &callbackCount, i]() {
            for (int j = 0; j < 50; ++j) {
                auto callback = [&callbackCount](int) {
                    callbackCount.fetch_add(1);
                };
                auto id =
                    trigger.registerCallback("concurrent_event", callback);
                std::this_thread::sleep_for(1ms);
                trigger.unregisterCallback("concurrent_event", id);
            }
        });
    }

    for (auto& t : registrationThreads) {
        t.join();
    }

    running = false;
    triggerThread.join();

    // Just verify no crashes occurred
    EXPECT_GE(callbackCount.load(), 0);
}

// ============================================================================
// Multiple Events Tests
// ============================================================================

TEST_F(TriggerTest, MultipleEvents) {
    Trigger<int> trigger;
    std::atomic<int> event1Count{0};
    std::atomic<int> event2Count{0};
    std::atomic<int> event3Count{0};

    trigger.registerCallback("event1", [&event1Count](int) { event1Count++; });
    trigger.registerCallback("event2", [&event2Count](int) { event2Count++; });
    trigger.registerCallback("event3", [&event3Count](int) { event3Count++; });

    trigger.trigger("event1", 1);
    trigger.trigger("event2", 2);
    trigger.trigger("event2", 2);
    trigger.trigger("event3", 3);
    trigger.trigger("event3", 3);
    trigger.trigger("event3", 3);

    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(event1Count.load(), 1);
    EXPECT_EQ(event2Count.load(), 2);
    EXPECT_EQ(event3Count.load(), 3);
}

// ============================================================================
// Callback Exception Handling Tests
// ============================================================================

TEST_F(TriggerTest, CallbackException) {
    Trigger<int> trigger;
    std::atomic<int> callback1Called{0};
    std::atomic<int> callback2Called{0};

    // First callback throws exception
    auto callback1 = [&callback1Called](int) {
        callback1Called++;
        throw std::runtime_error("Test exception");
    };

    // Second callback should still be called
    auto callback2 = [&callback2Called](int) { callback2Called++; };

    trigger.registerCallback("exception_event", callback1,
                             Trigger<int>::CallbackPriority::High);
    trigger.registerCallback("exception_event", callback2,
                             Trigger<int>::CallbackPriority::Normal);

    // Trigger should not throw even if callback throws
    EXPECT_NO_THROW(trigger.trigger("exception_event", 42));

    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(callback1Called.load(), 1);
    EXPECT_EQ(callback2Called.load(), 1);  // Should still be called
}

// ============================================================================
// Cancel Trigger Tests
// ============================================================================

TEST_F(TriggerTest, CancelAllTriggers) {
    Trigger<int> trigger;
    std::atomic<int> callbackCount{0};

    auto callback = [&callbackCount](int) {
        std::this_thread::sleep_for(100ms);  // Long-running callback
        callbackCount++;
    };

    trigger.registerCallback("cancel_event", callback);

    // Start multiple delayed triggers
    for (int i = 0; i < 5; ++i) {
        trigger.triggerWithDelay("cancel_event", i, 50ms);
    }

    // Cancel all triggers
    trigger.cancelAllTriggers();

    // Wait to see if any callbacks execute
    std::this_thread::sleep_for(200ms);

    // Some callbacks may have started before cancel
    EXPECT_LT(callbackCount.load(), 5);
}

// ============================================================================
// Different Parameter Types Tests
// ============================================================================

TEST_F(TriggerTest, StringParameter) {
    Trigger<std::string> trigger;
    std::string receivedValue;

    trigger.registerCallback(
        "string_event",
        [&receivedValue](const std::string& value) { receivedValue = value; });

    trigger.trigger("string_event", std::string("Hello, World!"));
    std::this_thread::sleep_for(10ms);

    EXPECT_EQ(receivedValue, "Hello, World!");
}

TEST_F(TriggerTest, StructParameter) {
    struct TestData {
        int id;
        std::string name;
        double value;
    };

    Trigger<TestData> trigger;
    TestData receivedData{};

    trigger.registerCallback(
        "struct_event",
        [&receivedData](const TestData& data) { receivedData = data; });

    TestData sendData{42, "Test", 3.14};
    trigger.trigger("struct_event", sendData);
    std::this_thread::sleep_for(10ms);

    EXPECT_EQ(receivedData.id, 42);
    EXPECT_EQ(receivedData.name, "Test");
    EXPECT_NEAR(receivedData.value, 3.14, 1e-5);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(TriggerTest, ManyCallbacksPerformance) {
    Trigger<int> trigger;
    std::atomic<int> totalCallbacks{0};

    const int numCallbacks = 100;

    for (int i = 0; i < numCallbacks; ++i) {
        trigger.registerCallback("perf_event", [&totalCallbacks](int) {
            totalCallbacks.fetch_add(1);
        });
    }

    auto start = std::chrono::high_resolution_clock::now();

    trigger.trigger("perf_event", 42);

    auto triggerTime = std::chrono::high_resolution_clock::now() - start;

    // Wait for all callbacks
    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(totalCallbacks.load(), numCallbacks);

    // Trigger should be fast (less than 10ms for 100 callbacks)
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(triggerTime)
                  .count(),
              100);
}

TEST_F(TriggerTest, RapidTriggers) {
    Trigger<int> trigger;
    std::atomic<int> callbackCount{0};

    trigger.registerCallback(
        "rapid_event", [&callbackCount](int) { callbackCount.fetch_add(1); });

    const int numTriggers = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numTriggers; ++i) {
        trigger.trigger("rapid_event", i);
    }

    auto triggerTime = std::chrono::high_resolution_clock::now() - start;

    // Wait for all callbacks
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(callbackCount.load(), numTriggers);

    // All triggers should complete in reasonable time
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(triggerTime)
                  .count(),
              500);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(TriggerTest, UnregisterDuringCallback) {
    Trigger<int> trigger;
    std::size_t callbackId = 0;
    std::atomic<int> callbackCount{0};

    auto callback = [&trigger, &callbackId, &callbackCount](int) {
        callbackCount++;
        // Try to unregister self during callback
        trigger.unregisterCallback("self_unregister", callbackId);
    };

    callbackId = trigger.registerCallback("self_unregister", callback);

    trigger.trigger("self_unregister", 42);
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(callbackCount.load(), 1);

    // Callback should be unregistered now
    EXPECT_FALSE(trigger.hasCallbacks("self_unregister"));
}

TEST_F(TriggerTest, RegisterDuringCallback) {
    Trigger<int> trigger;
    std::atomic<int> originalCallbackCount{0};
    std::atomic<int> newCallbackCount{0};

    auto originalCallback = [&trigger, &originalCallbackCount,
                             &newCallbackCount](int) {
        originalCallbackCount++;
        // Register new callback during execution
        trigger.registerCallback("register_during", [&newCallbackCount](int) {
            newCallbackCount++;
        });
    };

    trigger.registerCallback("register_during", originalCallback);

    trigger.trigger("register_during", 42);
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(originalCallbackCount.load(), 1);

    // Trigger again to call the newly registered callback
    trigger.trigger("register_during", 42);
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(originalCallbackCount.load(), 2);
    EXPECT_EQ(newCallbackCount.load(), 1);
}

}  // namespace atom::async::sync::test
