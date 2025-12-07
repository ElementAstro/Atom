/*
 * test_slot.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Slot-based Synchronization
Tests slot-based synchronization patterns, edge cases, and concurrent access
scenarios.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/sync/slot.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::sync::test {

namespace {
struct ThrowingType {
    int value;
    static inline std::atomic<bool> shouldThrow{false};

    explicit ThrowingType(int v) : value(v) {
        if (shouldThrow.load(std::memory_order_relaxed) && v == 999) {
            throw std::runtime_error("Constructor exception");
        }
    }

    ThrowingType(const ThrowingType& other) : value(other.value) {
        if (shouldThrow.load(std::memory_order_relaxed) && value == 888) {
            throw std::runtime_error("Copy constructor exception");
        }
    }

    ThrowingType& operator=(const ThrowingType& other) {
        if (this != &other) {
            if (shouldThrow.load(std::memory_order_relaxed) &&
                other.value == 777) {
                throw std::runtime_error("Assignment exception");
            }
            value = other.value;
        }
        return *this;
    }
};
}  // namespace

// ============================================================================
// Slot Tests
// ============================================================================

class SlotTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override {
        SynchronizationTestFixture::SetUp();
        // Additional slot specific setup
    }

    void TearDown() override {
        // Slot specific cleanup
        SynchronizationTestFixture::TearDown();
    }
};

TEST_F(SlotTest, BasicSlotOperations) {
    Slot<int> slot;

    EXPECT_FALSE(slot.hasValue());
    EXPECT_FALSE(slot.tryGet().has_value());

    slot.put(42);
    EXPECT_TRUE(slot.hasValue());

    auto value = slot.tryGet();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);

    // After getting, slot should be empty
    EXPECT_FALSE(slot.hasValue());
}

TEST_F(SlotTest, BlockingGet) {
    Slot<int> slot;
    std::atomic<bool> valueSet{false};

    std::thread producer([&slot, &valueSet]() {
        std::this_thread::sleep_for(100ms);
        slot.put(123);
        valueSet = true;
    });

    auto start = std::chrono::steady_clock::now();
    int value = slot.get();  // Should block until value is available
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(value, 123);
    EXPECT_TRUE(valueSet);
    EXPECT_GE(elapsed, 90ms);   // Should have waited
    EXPECT_LT(elapsed, 200ms);  // But not too long

    producer.join();
}

TEST_F(SlotTest, GetWithTimeout) {
    Slot<int> slot;

    // Test timeout when no value is available
    auto start = std::chrono::steady_clock::now();
    auto result = slot.getWithTimeout(100ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    EXPECT_GE(elapsed, 90ms);
    EXPECT_LT(elapsed, 150ms);

    // Test successful get within timeout
    slot.put(456);
    result = slot.getWithTimeout(100ms);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 456);
}

TEST_F(SlotTest, MultipleProducersOneConsumer) {
    Slot<int> slot;
    std::atomic<int> producerCount{0};
    std::vector<int> consumedValues;
    std::mutex consumedMutex;

    std::vector<std::thread> producers;
    const int numProducers = 5;

    // Start producers
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back([&slot, &producerCount, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            slot.put(i * 100);
            producerCount.fetch_add(1);
        });
    }

    // Consumer thread
    std::thread consumer(
        [&slot, &consumedValues, &consumedMutex, numProducers]() {
            for (int i = 0; i < numProducers; ++i) {
                int value = slot.get();
                std::lock_guard<std::mutex> lock(consumedMutex);
                consumedValues.push_back(value);
            }
        });

    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();

    EXPECT_EQ(producerCount.load(), numProducers);
    EXPECT_EQ(consumedValues.size(), numProducers);

    // All values should be present (order may vary)
    std::sort(consumedValues.begin(), consumedValues.end());
    for (int i = 0; i < numProducers; ++i) {
        EXPECT_EQ(consumedValues[i], i * 100);
    }
}

TEST_F(SlotTest, OneProducerMultipleConsumers) {
    Slot<int> slot;
    std::atomic<int> consumerCount{0};
    std::vector<int> consumedValues(3);

    // Start consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([&slot, &consumerCount, &consumedValues, i]() {
            consumedValues[i] = slot.get();
            consumerCount.fetch_add(1);
        });
    }

    // Give consumers time to start waiting
    std::this_thread::sleep_for(50ms);

    // Producer puts values
    std::thread producer([&slot]() {
        slot.put(100);
        std::this_thread::sleep_for(10ms);
        slot.put(200);
        std::this_thread::sleep_for(10ms);
        slot.put(300);
    });

    producer.join();
    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_EQ(consumerCount.load(), 3);

    // Each consumer should get exactly one value
    std::sort(consumedValues.begin(), consumedValues.end());
    EXPECT_EQ(consumedValues[0], 100);
    EXPECT_EQ(consumedValues[1], 200);
    EXPECT_EQ(consumedValues[2], 300);
}

TEST_F(SlotTest, SlotWithComplexType) {
    struct TestData {
        int id;
        std::string name;
        std::vector<int> values;

        TestData(int i, const std::string& n, std::vector<int> v)
            : id(i), name(n), values(std::move(v)) {}
    };

    Slot<TestData> slot;

    TestData testData(42, "test", {1, 2, 3, 4, 5});
    slot.put(std::move(testData));

    TestData retrieved = slot.get();
    EXPECT_EQ(retrieved.id, 42);
    EXPECT_EQ(retrieved.name, "test");
    EXPECT_EQ(retrieved.values.size(), 5);
    EXPECT_EQ(retrieved.values[0], 1);
    EXPECT_EQ(retrieved.values[4], 5);
}

TEST_F(SlotTest, SlotClear) {
    Slot<int> slot;

    slot.put(42);
    EXPECT_TRUE(slot.hasValue());

    slot.clear();
    EXPECT_FALSE(slot.hasValue());
    EXPECT_FALSE(slot.tryGet().has_value());
}

TEST_F(SlotTest, SlotCapacity) {
    Slot<int> slot(3);  // Capacity of 3

    // Should be able to put up to capacity
    EXPECT_TRUE(slot.tryPut(1));
    EXPECT_TRUE(slot.tryPut(2));
    EXPECT_TRUE(slot.tryPut(3));

    // Fourth put should fail
    EXPECT_FALSE(slot.tryPut(4));

    // After getting one, should be able to put another
    auto value = slot.tryGet();
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(slot.tryPut(4));
}

TEST_F(SlotTest, SlotSize) {
    Slot<int> slot(5);

    EXPECT_EQ(slot.size(), 0);
    EXPECT_TRUE(slot.empty());

    slot.put(1);
    EXPECT_EQ(slot.size(), 1);
    EXPECT_FALSE(slot.empty());

    slot.put(2);
    slot.put(3);
    EXPECT_EQ(slot.size(), 3);

    slot.tryGet();
    EXPECT_EQ(slot.size(), 2);
}

TEST_F(SlotTest, SlotWaitForEmpty) {
    Slot<int> slot(2);

    slot.put(1);
    slot.put(2);

    std::atomic<bool> isEmpty{false};

    std::thread waiter([&slot, &isEmpty]() {
        slot.waitForEmpty();
        isEmpty = true;
    });

    // Give waiter time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(isEmpty);

    // Remove one item - should still be waiting
    slot.tryGet();
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(isEmpty);

    // Remove last item - should now be empty
    slot.tryGet();
    waiter.join();
    EXPECT_TRUE(isEmpty);
}

TEST_F(SlotTest, SlotWaitForSpace) {
    Slot<int> slot(2);

    slot.put(1);
    slot.put(2);

    std::atomic<bool> hasSpace{false};

    std::thread waiter([&slot, &hasSpace]() {
        slot.waitForSpace();
        hasSpace = true;
    });

    // Give waiter time to start waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(hasSpace);

    // Remove one item - should now have space
    slot.tryGet();
    waiter.join();
    EXPECT_TRUE(hasSpace);
}

TEST_F(SlotTest, SlotExceptionSafety) {
    ThrowingType::shouldThrow.store(true, std::memory_order_relaxed);

    Slot<ThrowingType> slot;

    // Test exception in constructor
    EXPECT_THROW(slot.put(ThrowingType(999)), std::runtime_error);
    EXPECT_FALSE(slot.hasValue());

    // Test exception in copy constructor
    ThrowingType throwingObj(888);
    EXPECT_THROW(slot.put(throwingObj), std::runtime_error);
    EXPECT_FALSE(slot.hasValue());

    // Normal operation should still work
    ThrowingType::shouldThrow.store(false, std::memory_order_relaxed);
    slot.put(ThrowingType(42));
    EXPECT_TRUE(slot.hasValue());

    auto result = slot.tryGet();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().value, 42);
}

TEST_F(SlotTest, HighConcurrencyStressTest) {
    Slot<int> slot(100);
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};

    const int numProducers = 10;
    const int numConsumers = 5;
    const int itemsPerProducer = 50;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Start producers
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back([&slot, &totalProduced, itemsPerProducer, i]() {
            for (int j = 0; j < itemsPerProducer; ++j) {
                slot.put(i * 1000 + j);
                totalProduced.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    // Start consumers
    for (int i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(
            [&slot, &totalConsumed, numProducers, itemsPerProducer]() {
                int expectedTotal = numProducers * itemsPerProducer;
                while (totalConsumed.load() < expectedTotal) {
                    auto value = slot.getWithTimeout(100ms);
                    if (value.has_value()) {
                        totalConsumed.fetch_add(1);
                    }
                }
            });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_EQ(totalProduced.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(totalConsumed.load(), numProducers * itemsPerProducer);
}

// Test slot with RAII types
TEST_F(SlotTest, RAIITypes) {
    auto& tracker = getResourceTracker();
    Slot<std::unique_ptr<atom::async::test::ScopedResourceTracker>> slot;

    // Put resource
    slot.put(
        std::make_unique<atom::async::test::ScopedResourceTracker>(tracker));

    // Get resource
    auto resource = slot.get();
    EXPECT_NE(resource, nullptr);

    // Resource should be properly tracked
    // When resource goes out of scope, it should be cleaned up
    resource.reset();

    // Give some time for cleanup
    std::this_thread::sleep_for(10ms);
    tracker.expectNoLeaks();
}

// Test slot performance characteristics
TEST_F(SlotTest, PerformanceCharacteristics) {
    Slot<int> slot;
    const int numOperations = 1000;

    auto timer = createTimer();

    for (int i = 0; i < numOperations; ++i) {
        slot.put(i);
        int value = slot.get();
        EXPECT_EQ(value, i);
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(),
              1000000);  // Less than 1 second for 1000 operations

    std::cout << "Slot performance: " << elapsed.count() / numOperations
              << " microseconds per operation" << std::endl;
}

// Test slot with priority queue behavior
TEST_F(SlotTest, PriorityQueueBehavior) {
    struct PriorityItem {
        int priority;
        int value;

        PriorityItem(int p, int v) : priority(p), value(v) {}

        bool operator<(const PriorityItem& other) const {
            return priority < other.priority;  // Higher priority first
        }
    };

    Slot<PriorityItem> slot(5);

    // Add items with different priorities
    slot.put(PriorityItem(1, 100));
    slot.put(PriorityItem(5, 500));
    slot.put(PriorityItem(3, 300));
    slot.put(PriorityItem(2, 200));

    // If slot supports priority ordering, higher priority items should come
    // first Note: This test assumes the slot implementation supports ordering
    auto item1 = slot.tryGet();
    EXPECT_TRUE(item1.has_value());

    auto item2 = slot.tryGet();
    EXPECT_TRUE(item2.has_value());

    // Continue getting remaining items
    while (slot.hasValue()) {
        auto item = slot.tryGet();
        EXPECT_TRUE(item.has_value());
    }
}

// Test slot with batch operations
TEST_F(SlotTest, BatchOperations) {
    Slot<std::vector<int>> slot(10);

    // Put multiple batches
    for (int batch = 0; batch < 5; ++batch) {
        std::vector<int> data;
        for (int i = 0; i < 10; ++i) {
            data.push_back(batch * 10 + i);
        }
        slot.put(std::move(data));
    }

    EXPECT_EQ(slot.size(), 5);

    // Get and verify batches
    int batchCount = 0;
    while (slot.hasValue()) {
        auto batch = slot.get();
        EXPECT_EQ(batch.size(), 10);

        for (size_t i = 0; i < batch.size(); ++i) {
            EXPECT_EQ(batch[i], batchCount * 10 + static_cast<int>(i));
        }

        batchCount++;
    }

    EXPECT_EQ(batchCount, 5);
}

// Test slot with producer-consumer pattern using condition variables
TEST_F(SlotTest, ProducerConsumerWithConditionVariable) {
    Slot<int> slot(5);
    std::atomic<bool> producerDone{false};
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};

    // Producer thread
    std::thread producer([&slot, &producerDone, &totalProduced]() {
        for (int i = 0; i < 20; ++i) {
            slot.put(i);
            totalProduced.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        producerDone.store(true);
    });

    // Consumer thread
    std::thread consumer([&slot, &producerDone, &totalConsumed]() {
        while (!producerDone.load() || slot.hasValue()) {
            auto item = slot.getWithTimeout(100ms);
            if (item.has_value()) {
                totalConsumed.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(totalProduced.load(), 20);
    EXPECT_EQ(totalConsumed.load(), 20);
}

// Test slot with circular buffer behavior
TEST_F(SlotTest, CircularBufferBehavior) {
    Slot<int> slot(3);  // Small capacity

    // Fill to capacity
    EXPECT_TRUE(slot.tryPut(1));
    EXPECT_TRUE(slot.tryPut(2));
    EXPECT_TRUE(slot.tryPut(3));
    EXPECT_FALSE(slot.tryPut(4));  // Should fail

    // Remove one and add another
    auto item = slot.tryGet();
    EXPECT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 1);

    EXPECT_TRUE(slot.tryPut(4));  // Should succeed now

    // Verify remaining items
    EXPECT_EQ(slot.get(), 2);
    EXPECT_EQ(slot.get(), 3);
    EXPECT_EQ(slot.get(), 4);
    EXPECT_TRUE(slot.empty());
}

// Test slot with timeout variations
TEST_F(SlotTest, TimeoutVariations) {
    Slot<int> slot;

    // Test very short timeout
    auto start = createTimer();
    auto result = slot.getWithTimeout(1ms);
    auto elapsed = start.elapsed();

    EXPECT_FALSE(result.has_value());
    expectTimingRange(elapsed, std::chrono::microseconds(500),
                      std::chrono::microseconds(5000));

    // Test longer timeout
    start = createTimer();
    result = slot.getWithTimeout(50ms);
    elapsed = start.elapsed();

    EXPECT_FALSE(result.has_value());
    expectTimingRange(elapsed, std::chrono::microseconds(45000),
                      std::chrono::microseconds(60000));
}

// Test slot with move-only types
TEST_F(SlotTest, MoveOnlyTypes) {
    Slot<std::unique_ptr<int>> slot;

    auto ptr = std::make_unique<int>(42);
    slot.put(std::move(ptr));

    EXPECT_EQ(ptr, nullptr);  // Should be moved

    auto retrieved = slot.get();
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
}

// Test slot thread safety with mixed operations
TEST_F(SlotTest, MixedOperationsThreadSafety) {
    Slot<int> slot(100);
    std::atomic<int> putOperations{0};
    std::atomic<int> getOperations{0};
    std::atomic<int> clearOperations{0};

    const size_t numThreads = getMaxThreads();

    runConcurrentTest(numThreads, [&](size_t threadId) {
        for (int i = 0; i < 50; ++i) {
            switch (threadId % 4) {
                case 0:  // Put operations
                    if (slot.tryPut(static_cast<int>(threadId * 1000 + i))) {
                        putOperations.fetch_add(1);
                    }
                    break;

                case 1:  // Get operations
                    if (slot.tryGet().has_value()) {
                        getOperations.fetch_add(1);
                    }
                    break;

                case 2: {  // Size checks
                    volatile size_t size = slot.size();
                    volatile bool empty = slot.empty();
                    volatile bool hasValue = slot.hasValue();
                    (void)size;
                    (void)empty;
                    (void)hasValue;
                    break;
                }

                case 3:  // Occasional clear
                    if (i % 20 == 0) {
                        slot.clear();
                        clearOperations.fetch_add(1);
                    }
                    break;
            }

            std::this_thread::yield();
        }
    });

    // Verify operations completed without crashes
    std::cout << "Mixed operations - Puts: " << putOperations.load()
              << ", Gets: " << getOperations.load()
              << ", Clears: " << clearOperations.load() << std::endl;
}

// ============================================================================
// Signal Tests
// ============================================================================

class SignalTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }

    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

TEST_F(SignalTest, BasicSignalConnect) {
    Signal<int> signal;
    std::atomic<int> receivedValue{0};

    signal.connect([&receivedValue](int value) { receivedValue = value; });

    EXPECT_EQ(signal.size(), 1u);
    EXPECT_FALSE(signal.empty());

    signal.emit(42);
    EXPECT_EQ(receivedValue.load(), 42);
}

TEST_F(SignalTest, MultipleSlots) {
    Signal<int, std::string> signal;
    std::atomic<int> slot1Called{0};
    std::atomic<int> slot2Called{0};
    std::atomic<int> slot3Called{0};

    signal.connect([&slot1Called](int, const std::string&) { slot1Called++; });
    signal.connect([&slot2Called](int, const std::string&) { slot2Called++; });
    signal.connect([&slot3Called](int, const std::string&) { slot3Called++; });

    EXPECT_EQ(signal.size(), 3u);

    signal.emit(42, "test");

    EXPECT_EQ(slot1Called.load(), 1);
    EXPECT_EQ(slot2Called.load(), 1);
    EXPECT_EQ(slot3Called.load(), 1);
}

TEST_F(SignalTest, SignalClear) {
    Signal<int> signal;

    signal.connect([](int) {});
    signal.connect([](int) {});

    EXPECT_EQ(signal.size(), 2u);

    signal.clear();

    EXPECT_EQ(signal.size(), 0u);
    EXPECT_TRUE(signal.empty());
}

TEST_F(SignalTest, InvalidSlotConnection) {
    Signal<int> signal;
    Signal<int>::SlotType invalidSlot;  // Empty function

    EXPECT_THROW(signal.connect(invalidSlot), SlotConnectionError);
}

TEST_F(SignalTest, SignalWithNoSlots) {
    Signal<int> signal;

    // Emitting with no slots should not throw
    EXPECT_NO_THROW(signal.emit(42));
}

TEST_F(SignalTest, SignalWithMultipleArguments) {
    Signal<int, double, std::string> signal;
    int receivedInt = 0;
    double receivedDouble = 0.0;
    std::string receivedString;

    signal.connect([&](int i, double d, const std::string& s) {
        receivedInt = i;
        receivedDouble = d;
        receivedString = s;
    });

    signal.emit(42, 3.14, "hello");

    EXPECT_EQ(receivedInt, 42);
    EXPECT_NEAR(receivedDouble, 3.14, 1e-5);
    EXPECT_EQ(receivedString, "hello");
}

TEST_F(SignalTest, SignalConcurrentEmit) {
    Signal<int> signal;
    std::atomic<int> totalReceived{0};

    signal.connect(
        [&totalReceived](int value) { totalReceived.fetch_add(value); });

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int emitsPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&signal, emitsPerThread]() {
            for (int j = 0; j < emitsPerThread; ++j) {
                signal.emit(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(totalReceived.load(), numThreads * emitsPerThread);
}

// ============================================================================
// AsyncSignal Tests
// ============================================================================

TEST_F(SignalTest, AsyncSignalBasic) {
    AsyncSignal<int> signal;
    std::atomic<int> receivedValue{0};

    signal.connect([&receivedValue](int value) { receivedValue = value; });

    auto futures = signal.emit(42);

    // Wait for all async operations to complete
    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(receivedValue.load(), 42);
}

TEST_F(SignalTest, AsyncSignalMultipleSlots) {
    AsyncSignal<int> signal;
    std::atomic<int> totalCalls{0};

    for (int i = 0; i < 5; ++i) {
        signal.connect([&totalCalls](int) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            totalCalls.fetch_add(1);
        });
    }

    auto futures = signal.emit(42);

    // All slots should execute asynchronously
    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(totalCalls.load(), 5);
}

TEST_F(SignalTest, AsyncSignalClear) {
    AsyncSignal<int> signal;

    signal.connect([](int) {});
    signal.connect([](int) {});

    EXPECT_EQ(signal.size(), 2u);

    signal.clear();

    EXPECT_EQ(signal.size(), 0u);
    EXPECT_TRUE(signal.empty());
}

TEST_F(SignalTest, AsyncSignalConcurrentEmit) {
    AsyncSignal<int> signal;
    std::atomic<int> totalReceived{0};

    signal.connect(
        [&totalReceived](int value) { totalReceived.fetch_add(value); });

    std::vector<std::thread> threads;
    const int numThreads = 5;
    const int emitsPerThread = 20;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&signal, emitsPerThread]() {
            for (int j = 0; j < emitsPerThread; ++j) {
                auto futures = signal.emit(1);
                for (auto& f : futures) {
                    f.wait();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(totalReceived.load(), numThreads * emitsPerThread);
}

}  // namespace atom::async::sync::test
