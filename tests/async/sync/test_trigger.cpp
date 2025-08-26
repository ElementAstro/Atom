/*
 * test_trigger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Event Triggers
Tests event triggers, synchronization patterns, edge cases, and error handling.

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>

#include "atom/async/sync/trigger.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

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
    Trigger trigger;
    
    EXPECT_FALSE(trigger.isTriggered());
    
    trigger.trigger();
    EXPECT_TRUE(trigger.isTriggered());
    
    trigger.reset();
    EXPECT_FALSE(trigger.isTriggered());
}

TEST_F(TriggerTest, WaitForTrigger) {
    Trigger trigger;
    std::atomic<bool> triggerSet{false};
    
    std::thread waiter([&trigger, &triggerSet]() {
        trigger.wait();
        triggerSet = true;
    });
    
    // Give waiter time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(triggerSet);
    
    trigger.trigger();
    waiter.join();
    
    EXPECT_TRUE(triggerSet);
    EXPECT_TRUE(trigger.isTriggered());
}

TEST_F(TriggerTest, WaitWithTimeout) {
    Trigger trigger;
    
    // Test timeout when trigger is not set
    auto start = std::chrono::steady_clock::now();
    bool result = trigger.waitFor(100ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(result);
    EXPECT_GE(elapsed, 90ms);
    EXPECT_LT(elapsed, 150ms);
    
    // Test successful wait within timeout
    trigger.trigger();
    result = trigger.waitFor(100ms);
    
    EXPECT_TRUE(result);
}

TEST_F(TriggerTest, MultipleWaiters) {
    Trigger trigger;
    std::atomic<int> waiterCount{0};
    
    std::vector<std::thread> waiters;
    const int numWaiters = 5;
    
    for (int i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&trigger, &waiterCount]() {
            trigger.wait();
            waiterCount.fetch_add(1);
        });
    }
    
    // Give waiters time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(waiterCount.load(), 0);
    
    // Trigger should wake up all waiters
    trigger.trigger();
    
    for (auto& waiter : waiters) {
        waiter.join();
    }
    
    EXPECT_EQ(waiterCount.load(), numWaiters);
}

TEST_F(TriggerTest, AutoResetTrigger) {
    AutoResetTrigger trigger;
    
    EXPECT_FALSE(trigger.isTriggered());
    
    trigger.trigger();
    EXPECT_TRUE(trigger.isTriggered());
    
    // First wait should succeed and auto-reset
    bool result = trigger.waitFor(10ms);
    EXPECT_TRUE(result);
    EXPECT_FALSE(trigger.isTriggered()); // Should be auto-reset
    
    // Second wait should timeout
    result = trigger.waitFor(50ms);
    EXPECT_FALSE(result);
}

TEST_F(TriggerTest, AutoResetWithMultipleWaiters) {
    AutoResetTrigger trigger;
    std::atomic<int> successCount{0};
    std::atomic<int> timeoutCount{0};
    
    std::vector<std::thread> waiters;
    const int numWaiters = 5;
    
    for (int i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&trigger, &successCount, &timeoutCount]() {
            if (trigger.waitFor(200ms)) {
                successCount.fetch_add(1);
            } else {
                timeoutCount.fetch_add(1);
            }
        });
    }
    
    // Give waiters time to start waiting
    std::this_thread::sleep_for(50ms);
    
    // Trigger once - should only wake up one waiter
    trigger.trigger();
    
    for (auto& waiter : waiters) {
        waiter.join();
    }
    
    EXPECT_EQ(successCount.load(), 1); // Only one should succeed
    EXPECT_EQ(timeoutCount.load(), numWaiters - 1); // Others should timeout
}

TEST_F(TriggerTest, CountdownTrigger) {
    const int countdownValue = 3;
    CountdownTrigger trigger(countdownValue);
    
    EXPECT_FALSE(trigger.isTriggered());
    EXPECT_EQ(trigger.getCount(), countdownValue);
    
    // First two countdowns should not trigger
    trigger.countdown();
    EXPECT_FALSE(trigger.isTriggered());
    EXPECT_EQ(trigger.getCount(), countdownValue - 1);
    
    trigger.countdown();
    EXPECT_FALSE(trigger.isTriggered());
    EXPECT_EQ(trigger.getCount(), countdownValue - 2);
    
    // Third countdown should trigger
    trigger.countdown();
    EXPECT_TRUE(trigger.isTriggered());
    EXPECT_EQ(trigger.getCount(), 0);
}

TEST_F(TriggerTest, CountdownTriggerWithWaiters) {
    CountdownTrigger trigger(3);
    std::atomic<bool> triggered{false};
    
    std::thread waiter([&trigger, &triggered]() {
        trigger.wait();
        triggered = true;
    });
    
    // Give waiter time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(triggered);
    
    // Countdown twice - should not trigger yet
    trigger.countdown();
    trigger.countdown();
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(triggered);
    
    // Final countdown should trigger
    trigger.countdown();
    waiter.join();
    
    EXPECT_TRUE(triggered);
}

TEST_F(TriggerTest, ConcurrentCountdown) {
    const int numThreads = 10;
    CountdownTrigger trigger(numThreads);
    std::atomic<int> completedThreads{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&trigger, &completedThreads]() {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(10 + rand() % 50));
            
            trigger.countdown();
            completedThreads.fetch_add(1);
        });
    }
    
    // Wait for trigger to be set
    trigger.wait();
    
    // All threads should have completed
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completedThreads.load(), numThreads);
    EXPECT_TRUE(trigger.isTriggered());
    EXPECT_EQ(trigger.getCount(), 0);
}

TEST_F(TriggerTest, TriggerWithPredicate) {
    PredicateTrigger<int> trigger([](int value) { return value > 100; });
    
    EXPECT_FALSE(trigger.isTriggered());
    
    // Values <= 100 should not trigger
    trigger.update(50);
    EXPECT_FALSE(trigger.isTriggered());
    
    trigger.update(100);
    EXPECT_FALSE(trigger.isTriggered());
    
    // Value > 100 should trigger
    trigger.update(150);
    EXPECT_TRUE(trigger.isTriggered());
}

TEST_F(TriggerTest, PredicateTriggerWithWaiter) {
    PredicateTrigger<std::string> trigger([](const std::string& s) { 
        return s.length() >= 10; 
    });
    
    std::atomic<bool> triggered{false};
    
    std::thread waiter([&trigger, &triggered]() {
        trigger.wait();
        triggered = true;
    });
    
    // Give waiter time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(triggered);
    
    // Short strings should not trigger
    trigger.update("short");
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(triggered);
    
    // Long string should trigger
    trigger.update("this is a long string");
    waiter.join();
    
    EXPECT_TRUE(triggered);
}

TEST_F(TriggerTest, TimedTrigger) {
    TimedTrigger trigger(100ms);
    
    EXPECT_FALSE(trigger.isTriggered());
    
    auto start = std::chrono::steady_clock::now();
    trigger.wait();
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_TRUE(trigger.isTriggered());
    EXPECT_GE(elapsed, 90ms);
    EXPECT_LT(elapsed, 150ms);
}

TEST_F(TriggerTest, RepeatingTimedTrigger) {
    RepeatingTimedTrigger trigger(50ms);
    std::atomic<int> triggerCount{0};
    
    std::thread counter([&trigger, &triggerCount]() {
        for (int i = 0; i < 5; ++i) {
            trigger.wait();
            triggerCount.fetch_add(1);
            trigger.reset(); // Reset for next iteration
        }
    });
    
    counter.join();
    
    EXPECT_EQ(triggerCount.load(), 5);
}

TEST_F(TriggerTest, TriggerChain) {
    Trigger trigger1;
    Trigger trigger2;
    Trigger trigger3;
    
    // Chain triggers: trigger1 -> trigger2 -> trigger3
    std::thread chain([&trigger1, &trigger2, &trigger3]() {
        trigger1.wait();
        trigger2.trigger();
        
        trigger2.wait();
        trigger3.trigger();
    });
    
    std::atomic<bool> finalTriggered{false};
    std::thread finalWaiter([&trigger3, &finalTriggered]() {
        trigger3.wait();
        finalTriggered = true;
    });
    
    // Start the chain
    trigger1.trigger();
    
    chain.join();
    finalWaiter.join();
    
    EXPECT_TRUE(finalTriggered);
}

TEST_F(TriggerTest, TriggerExceptionSafety) {
    struct ThrowingPredicate {
        bool shouldThrow = false;
        
        bool operator()(int value) {
            if (shouldThrow && value == 999) {
                throw std::runtime_error("Predicate exception");
            }
            return value > 100;
        }
    };
    
    ThrowingPredicate predicate;
    PredicateTrigger<int> trigger(predicate);
    
    // Normal operation
    trigger.update(50);
    EXPECT_FALSE(trigger.isTriggered());
    
    trigger.update(150);
    EXPECT_TRUE(trigger.isTriggered());
    
    trigger.reset();
    
    // Exception in predicate
    predicate.shouldThrow = true;
    EXPECT_THROW(trigger.update(999), std::runtime_error);
    EXPECT_FALSE(trigger.isTriggered()); // Should remain unchanged
}

TEST_F(TriggerTest, HighConcurrencyStressTest) {
    const int numTriggers = 100;
    std::vector<std::unique_ptr<Trigger>> triggers;
    std::atomic<int> completedWaiters{0};
    
    // Create triggers
    for (int i = 0; i < numTriggers; ++i) {
        triggers.push_back(std::make_unique<Trigger>());
    }
    
    // Create waiters
    std::vector<std::thread> waiters;
    for (int i = 0; i < numTriggers; ++i) {
        waiters.emplace_back([&triggers, &completedWaiters, i]() {
            triggers[i]->wait();
            completedWaiters.fetch_add(1);
        });
    }
    
    // Give waiters time to start
    std::this_thread::sleep_for(100ms);
    
    // Trigger all
    for (auto& trigger : triggers) {
        trigger->trigger();
    }
    
    for (auto& waiter : waiters) {
        waiter.join();
    }
    
    EXPECT_EQ(completedWaiters.load(), numTriggers);
}

// Test trigger performance characteristics
TEST_F(TriggerTest, PerformanceCharacteristics) {
    Trigger trigger;
    const int numOperations = 1000;

    auto timer = createTimer();

    for (int i = 0; i < numOperations; ++i) {
        trigger.trigger();
        trigger.reset();
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(), 100000); // Less than 100ms for 1000 operations

    std::cout << "Trigger performance: "
              << elapsed.count() / numOperations << " microseconds per operation" << std::endl;
}

// Test trigger with resource cleanup
TEST_F(TriggerTest, ResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        Trigger trigger;

        std::thread waiter([&trigger, &tracker]() {
            atom::async::test::ScopedResourceTracker resource(tracker);
            trigger.wait();
            // Resource should be cleaned up when thread exits
        });

        std::this_thread::sleep_for(50ms);
        trigger.trigger();
        waiter.join();
    }

    // Give some time for cleanup
    std::this_thread::sleep_for(10ms);
    tracker.expectNoLeaks();
}

// Test trigger with spurious wakeups handling
TEST_F(TriggerTest, SpuriousWakeupsHandling) {
    Trigger trigger;
    std::atomic<bool> correctWakeup{false};
    std::atomic<int> wakeupCount{0};

    std::thread waiter([&trigger, &correctWakeup, &wakeupCount]() {
        // Simulate checking for spurious wakeups
        while (!trigger.isTriggered()) {
            if (trigger.waitFor(10ms)) {
                wakeupCount.fetch_add(1);
                if (trigger.isTriggered()) {
                    correctWakeup = true;
                    break;
                }
            }
        }
    });

    // Let waiter start
    std::this_thread::sleep_for(50ms);

    // Trigger the event
    trigger.trigger();
    waiter.join();

    EXPECT_TRUE(correctWakeup);
    EXPECT_GT(wakeupCount.load(), 0);
}

// Test trigger with multiple trigger types interaction
TEST_F(TriggerTest, MultipleTriggerTypesInteraction) {
    Trigger basicTrigger;
    AutoResetTrigger autoResetTrigger;
    CountdownTrigger countdownTrigger(2);

    std::atomic<int> basicTriggered{0};
    std::atomic<int> autoResetTriggered{0};
    std::atomic<int> countdownTriggered{0};

    // Waiters for each trigger type
    std::thread basicWaiter([&basicTrigger, &basicTriggered]() {
        basicTrigger.wait();
        basicTriggered.fetch_add(1);
    });

    std::thread autoResetWaiter([&autoResetTrigger, &autoResetTriggered]() {
        autoResetTrigger.wait();
        autoResetTriggered.fetch_add(1);
    });

    std::thread countdownWaiter([&countdownTrigger, &countdownTriggered]() {
        countdownTrigger.wait();
        countdownTriggered.fetch_add(1);
    });

    // Trigger all
    basicTrigger.trigger();
    autoResetTrigger.trigger();
    countdownTrigger.countdown();
    countdownTrigger.countdown();

    basicWaiter.join();
    autoResetWaiter.join();
    countdownWaiter.join();

    EXPECT_EQ(basicTriggered.load(), 1);
    EXPECT_EQ(autoResetTriggered.load(), 1);
    EXPECT_EQ(countdownTriggered.load(), 1);
}

// Test trigger with complex predicate logic
TEST_F(TriggerTest, ComplexPredicateLogic) {
    struct ComplexState {
        int value1;
        int value2;
        std::string status;

        ComplexState() : value1(0), value2(0), status("init") {}
    };

    PredicateTrigger<ComplexState> trigger([](const ComplexState& state) {
        return state.value1 > 100 && state.value2 > 50 && state.status == "ready";
    });

    std::atomic<bool> triggered{false};

    std::thread waiter([&trigger, &triggered]() {
        trigger.wait();
        triggered = true;
    });

    // Update state gradually
    ComplexState state;

    state.value1 = 150;
    trigger.update(state);
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(triggered.load());

    state.value2 = 75;
    trigger.update(state);
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(triggered.load());

    state.status = "ready";
    trigger.update(state);

    waiter.join();
    EXPECT_TRUE(triggered.load());
}

// Test trigger with timeout precision
TEST_F(TriggerTest, TimeoutPrecision) {
    Trigger trigger;

    const auto timeout = 100ms;
    const auto tolerance = 20ms;

    auto timer = createTimer();
    bool result = trigger.waitFor(timeout);
    auto elapsed = timer.elapsed();

    EXPECT_FALSE(result);
    expectTimingRange(std::chrono::duration_cast<std::chrono::microseconds>(elapsed),
                     std::chrono::duration_cast<std::chrono::microseconds>(timeout - tolerance),
                     std::chrono::duration_cast<std::chrono::microseconds>(timeout + tolerance));
}

// Test trigger with cascading triggers
TEST_F(TriggerTest, CascadingTriggers) {
    const int numTriggers = 5;
    std::vector<std::unique_ptr<Trigger>> triggers;
    std::vector<std::atomic<bool>> triggered(numTriggers);

    // Create triggers
    for (int i = 0; i < numTriggers; ++i) {
        triggers.push_back(std::make_unique<Trigger>());
    }

    // Create cascade chain
    std::vector<std::thread> threads;
    for (int i = 0; i < numTriggers; ++i) {
        threads.emplace_back([&triggers, &triggered, i, numTriggers]() {
            triggers[i]->wait();
            triggered[i] = true;

            // Trigger next in chain
            if (i + 1 < numTriggers) {
                triggers[i + 1]->trigger();
            }
        });
    }

    // Start the cascade
    triggers[0]->trigger();

    for (auto& thread : threads) {
        thread.join();
    }

    // All should be triggered
    for (int i = 0; i < numTriggers; ++i) {
        EXPECT_TRUE(triggered[i].load()) << "Trigger " << i << " was not triggered";
    }
}

// Test trigger with concurrent reset operations
TEST_F(TriggerTest, ConcurrentResetOperations) {
    Trigger trigger;
    std::atomic<int> triggerCount{0};
    std::atomic<int> resetCount{0};
    std::atomic<int> waitCount{0};

    const size_t numThreads = getMaxThreads();

    runConcurrentTest(numThreads, [&](size_t threadId) {
        for (int i = 0; i < 20; ++i) {
            switch (threadId % 3) {
                case 0: // Trigger
                    trigger.trigger();
                    triggerCount.fetch_add(1);
                    break;

                case 1: // Reset
                    trigger.reset();
                    resetCount.fetch_add(1);
                    break;

                case 2: // Wait with timeout
                    if (trigger.waitFor(1ms)) {
                        waitCount.fetch_add(1);
                    }
                    break;
            }

            std::this_thread::yield();
        }
    });

    // Verify operations completed without crashes
    std::cout << "Concurrent operations - Triggers: " << triggerCount.load()
              << ", Resets: " << resetCount.load()
              << ", Successful waits: " << waitCount.load() << std::endl;
}

// Test trigger with memory ordering guarantees
TEST_F(TriggerTest, MemoryOrderingGuarantees) {
    Trigger trigger;
    std::atomic<int> sharedData{0};
    std::atomic<int> observedValue{0};

    std::thread writer([&trigger, &sharedData]() {
        sharedData.store(42, std::memory_order_release);
        trigger.trigger();
    });

    std::thread reader([&trigger, &sharedData, &observedValue]() {
        trigger.wait();
        observedValue.store(sharedData.load(std::memory_order_acquire));
    });

    writer.join();
    reader.join();

    // The trigger should provide proper synchronization
    EXPECT_EQ(observedValue.load(), 42);
}

}  // namespace atom::async::sync::test
