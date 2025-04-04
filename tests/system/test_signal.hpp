#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "atom/system/signal.hpp"

using namespace std::chrono_literals;

// Test fixture for SignalHandlerRegistry
class SignalHandlerRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        registry = &SignalHandlerRegistry::getInstance();

        // Reset stats for clean testing
        registry->resetStats();

        // Use a test signal that won't interfere with normal operation
        // SIGUSR1 (10) and SIGUSR2 (12) are good for testing on POSIX
        // For Windows compatibility we'll use common signals across platforms
#if defined(_WIN32) || defined(_WIN64)
        testSignal = SIGBREAK;  // Windows-specific test signal (21)
#else
        testSignal = SIGUSR1;  // POSIX test signal (10)
#endif
        alternateTestSignal = SIGTERM;  // Another test signal (15)
    }

    void TearDown() override {
        // Clean up any registered handlers
        for (int handlerId : testHandlerIds) {
            registry->removeSignalHandlerById(handlerId);
        }
        testHandlerIds.clear();
    }

    // Helper to register a test handler and track its ID
    int registerTestHandler(SignalID signal, const SignalHandler& handler,
                            int priority = 0,
                            const std::string& name = "test") {
        int id = registry->setSignalHandler(signal, handler, priority, name);
        testHandlerIds.push_back(id);
        return id;
    }

    SignalHandlerRegistry* registry;
    SignalID testSignal;
    SignalID alternateTestSignal;
    std::vector<int> testHandlerIds;
};

// Test fixture for SafeSignalManager
class SafeSignalManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        manager = &SafeSignalManager::getInstance();

        // Reset stats and clear queue for clean testing
        manager->resetStats();
        manager->clearSignalQueue();

        // Use test signals that won't interfere with normal operation
#if defined(_WIN32) || defined(_WIN64)
        testSignal = SIGBREAK;  // Windows-specific test signal (21)
#else
        testSignal = SIGUSR1;  // POSIX test signal (10)
#endif
        alternateTestSignal = SIGTERM;  // Another test signal (15)
    }

    void TearDown() override {
        // Clean up any registered handlers
        for (int handlerId : testHandlerIds) {
            manager->removeSafeSignalHandlerById(handlerId);
        }
        testHandlerIds.clear();

        // Clear any remaining signals in the queue
        manager->clearSignalQueue();
    }

    // Helper to register a test handler and track its ID
    int registerTestHandler(SignalID signal, const SignalHandler& handler,
                            int priority = 0,
                            const std::string& name = "test") {
        int id = manager->addSafeSignalHandler(signal, handler, priority, name);
        testHandlerIds.push_back(id);
        return id;
    }

    // Helper method to wait for a condition with timeout
    template <typename Func>
    bool waitForCondition(Func condition,
                          std::chrono::milliseconds timeout = 1s) {
        auto start = std::chrono::steady_clock::now();
        while (!condition()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }

    SafeSignalManager* manager;
    SignalID testSignal;
    SignalID alternateTestSignal;
    std::vector<int> testHandlerIds;
};

// Tests for SignalHandlerRegistry

TEST_F(SignalHandlerRegistryTest, GetInstance) {
    // Test that we can get the singleton instance
    auto& instance1 = SignalHandlerRegistry::getInstance();
    auto& instance2 = SignalHandlerRegistry::getInstance();

    // Verify it's the same instance
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(SignalHandlerRegistryTest, SetSignalHandler) {
    bool handlerCalled = false;

    // Register a handler
    int handlerId = registerTestHandler(testSignal, [&](SignalID signal) {
        handlerCalled = true;
        EXPECT_EQ(signal, testSignal);
    });

    // Verify handler was registered
    EXPECT_GT(handlerId, 0);
    EXPECT_TRUE(registry->hasHandlersForSignal(testSignal));
}

TEST_F(SignalHandlerRegistryTest, RemoveSignalHandlerById) {
    // Register a handler
    int handlerId = registerTestHandler(testSignal, [](SignalID) {});

    // Verify handler was registered
    EXPECT_TRUE(registry->hasHandlersForSignal(testSignal));

    // Remove the handler
    bool removed = registry->removeSignalHandlerById(handlerId);

    // Verify handler was removed
    EXPECT_TRUE(removed);
    EXPECT_FALSE(registry->hasHandlersForSignal(testSignal));

    // Try to remove non-existent handler
    removed = registry->removeSignalHandlerById(999999);
    EXPECT_FALSE(removed);
}

TEST_F(SignalHandlerRegistryTest, RemoveSignalHandler) {
    // Create a handler
    auto handler = [](SignalID) {};

    // Register the handler
    registerTestHandler(testSignal, handler);

    // Verify handler was registered
    EXPECT_TRUE(registry->hasHandlersForSignal(testSignal));

    // Remove the handler
    bool removed = registry->removeSignalHandler(testSignal, handler);

    // Verify handler was removed
    EXPECT_TRUE(removed);
    EXPECT_FALSE(registry->hasHandlersForSignal(testSignal));

    // Try to remove non-existent handler
    removed = registry->removeSignalHandler(testSignal, handler);
    EXPECT_FALSE(removed);
}

TEST_F(SignalHandlerRegistryTest, SetStandardCrashHandlerSignals) {
    auto crashHandler = [](SignalID) {};

    // Register standard crash handlers
    std::vector<int> handlerIds =
        registry->setStandardCrashHandlerSignals(crashHandler);

    // Add these to our tracking for cleanup
    testHandlerIds.insert(testHandlerIds.end(), handlerIds.begin(),
                          handlerIds.end());

    // Verify handlers were registered
    EXPECT_FALSE(handlerIds.empty());

    // Check that at least SIGSEGV is handled
    EXPECT_TRUE(registry->hasHandlersForSignal(SIGSEGV));
}

TEST_F(SignalHandlerRegistryTest, HandlerPriority) {
    std::vector<int> executionOrder;

    // Register handlers with different priorities
    registerTestHandler(
        testSignal, [&](SignalID) { executionOrder.push_back(1); }, 10, "high");
    registerTestHandler(
        testSignal, [&](SignalID) { executionOrder.push_back(2); }, 5,
        "medium");
    registerTestHandler(
        testSignal, [&](SignalID) { executionOrder.push_back(3); }, 0, "low");

    // Manually process the signal
    int processed = registry->processAllPendingSignals(100ms);

    // Currently there are no pending signals to process
    EXPECT_EQ(processed, 0);

    // We can't easily test real signal dispatch in a unit test
    // but we can verify the handlers exist with proper priorities
    EXPECT_TRUE(registry->hasHandlersForSignal(testSignal));
}

TEST_F(SignalHandlerRegistryTest, GetSignalStats) {
    // Get stats for a signal
    const SignalStats& stats = registry->getSignalStats(testSignal);

    // Should be initialized to zeros
    EXPECT_EQ(stats.received, 0);
    EXPECT_EQ(stats.processed, 0);
    EXPECT_EQ(stats.dropped, 0);
    EXPECT_EQ(stats.handlerErrors, 0);
}

TEST_F(SignalHandlerRegistryTest, ResetStats) {
    // Register a handler so we have a signal to track
    registerTestHandler(testSignal, [](SignalID) {});

    // Reset stats
    registry->resetStats();

    // Stats should be cleared
    const SignalStats& stats = registry->getSignalStats(testSignal);
    EXPECT_EQ(stats.received, 0);
    EXPECT_EQ(stats.processed, 0);
}

TEST_F(SignalHandlerRegistryTest, SetHandlerTimeout) {
    // Set handler timeout
    auto timeout = 500ms;
    registry->setHandlerTimeout(timeout);

    // Just verify it doesn't crash, can't easily test the actual timeout value
    SUCCEED();
}

TEST_F(SignalHandlerRegistryTest, ExecuteHandlerWithTimeout) {
    // Quick handler
    auto quickHandler = [](SignalID) { std::this_thread::sleep_for(10ms); };

    // Slow handler
    auto slowHandler = [](SignalID) { std::this_thread::sleep_for(2s); };

    // Set timeout to 500ms
    registry->setHandlerTimeout(500ms);

    // Execute quick handler
    bool quickSuccess =
        registry->executeHandlerWithTimeout(quickHandler, testSignal);
    EXPECT_TRUE(quickSuccess);

    // Execute slow handler
    bool slowSuccess =
        registry->executeHandlerWithTimeout(slowHandler, testSignal);
    EXPECT_FALSE(slowSuccess);
}

// Tests for SafeSignalManager

TEST_F(SafeSignalManagerTest, GetInstance) {
    // Test that we can get the singleton instance
    auto& instance1 = SafeSignalManager::getInstance();
    auto& instance2 = SafeSignalManager::getInstance();

    // Verify it's the same instance
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(SafeSignalManagerTest, AddSafeSignalHandler) {
    bool handlerCalled = false;

    // Register a handler
    int handlerId = registerTestHandler(testSignal, [&](SignalID signal) {
        handlerCalled = true;
        EXPECT_EQ(signal, testSignal);
    });

    // Verify handler was registered
    EXPECT_GT(handlerId, 0);
}

TEST_F(SafeSignalManagerTest, RemoveSafeSignalHandlerById) {
    // Register a handler
    int handlerId = registerTestHandler(testSignal, [](SignalID) {});

    // Remove the handler
    bool removed = manager->removeSafeSignalHandlerById(handlerId);

    // Verify handler was removed
    EXPECT_TRUE(removed);

    // Try to remove non-existent handler
    removed = manager->removeSafeSignalHandlerById(999999);
    EXPECT_FALSE(removed);
}

TEST_F(SafeSignalManagerTest, RemoveSafeSignalHandler) {
    // Create a handler
    auto handler = [](SignalID) {};

    // Register the handler
    registerTestHandler(testSignal, handler);

    // Remove the handler
    bool removed = manager->removeSafeSignalHandler(testSignal, handler);

    // Verify handler was removed
    EXPECT_TRUE(removed);

    // Try to remove non-existent handler
    removed = manager->removeSafeSignalHandler(testSignal, handler);
    EXPECT_FALSE(removed);
}

TEST_F(SafeSignalManagerTest, QueueSignal) {
    std::atomic<bool> handlerCalled{false};

    // Register a handler
    registerTestHandler(testSignal, [&](SignalID signal) {
        handlerCalled = true;
        EXPECT_EQ(signal, testSignal);
    });

    // Queue the signal
    bool queued = manager->queueSignal(testSignal);
    EXPECT_TRUE(queued);

    // Wait for handler to be called
    EXPECT_TRUE(waitForCondition([&]() { return handlerCalled.load(); }));
}

TEST_F(SafeSignalManagerTest, GetQueueSize) {
    // Queue should start empty
    EXPECT_EQ(manager->getQueueSize(), 0);

    // Queue a signal without handlers to prevent immediate processing
    manager->queueSignal(alternateTestSignal);

    // Queue should now have 1 item
    EXPECT_EQ(manager->getQueueSize(), 1);

    // Clear the queue
    manager->clearSignalQueue();

    // Queue should be empty again
    EXPECT_EQ(manager->getQueueSize(), 0);
}

TEST_F(SafeSignalManagerTest, ClearSignalQueue) {
    // Queue some signals without handlers
    manager->queueSignal(testSignal);
    manager->queueSignal(alternateTestSignal);

    // Clear the queue
    int cleared = manager->clearSignalQueue();

    // Should have cleared 2 signals
    EXPECT_EQ(cleared, 2);

    // Queue should be empty
    EXPECT_EQ(manager->getQueueSize(), 0);
}

TEST_F(SafeSignalManagerTest, GetSignalStats) {
    // Get stats for a signal
    const SignalStats& stats = manager->getSignalStats(testSignal);

    // Should be initialized to zeros
    EXPECT_EQ(stats.received, 0);
    EXPECT_EQ(stats.processed, 0);
    EXPECT_EQ(stats.dropped, 0);
    EXPECT_EQ(stats.handlerErrors, 0);
}

TEST_F(SafeSignalManagerTest, ResetStats) {
    // Register a handler
    registerTestHandler(testSignal, [](SignalID) {});

    // Queue a signal to generate some stats
    manager->queueSignal(testSignal);

    // Wait a bit for processing
    std::this_thread::sleep_for(100ms);

    // Reset stats
    manager->resetStats();

    // Stats should be cleared
    const SignalStats& stats = manager->getSignalStats(testSignal);
    EXPECT_EQ(stats.received, 0);
    EXPECT_EQ(stats.processed, 0);
}

TEST_F(SafeSignalManagerTest, SetWorkerThreadCount) {
    // Set worker thread count
    bool success = manager->setWorkerThreadCount(2);

    // This should succeed
    EXPECT_TRUE(success);

    // Test an invalid thread count (0)
    success = manager->setWorkerThreadCount(0);

    // This should fail
    EXPECT_FALSE(success);
}

TEST_F(SafeSignalManagerTest, SetMaxQueueSize) {
    // Set max queue size
    manager->setMaxQueueSize(2000);

    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(SafeSignalManagerTest, MultipleConcurrentSignals) {
    std::atomic<int> handlerCallCount{0};

    // Register a handler
    registerTestHandler(testSignal, [&](SignalID) {
        handlerCallCount++;
        // Small delay to simulate work
        std::this_thread::sleep_for(50ms);
    });

    // Queue multiple signals
    const int signalCount = 5;
    for (int i = 0; i < signalCount; i++) {
        manager->queueSignal(testSignal);
    }

    // Wait for all handlers to be called
    EXPECT_TRUE(waitForCondition(
        [&]() { return handlerCallCount == signalCount; }, 2s));

    // Verify all signals were processed
    EXPECT_EQ(handlerCallCount, signalCount);
}

TEST_F(SafeSignalManagerTest, QueueFullBehavior) {
    // Set a small queue size
    manager->setMaxQueueSize(2);

    // Register a slow handler to ensure queue fills up
    registerTestHandler(testSignal,
                        [](SignalID) { std::this_thread::sleep_for(500ms); });

    // Queue signals until full
    EXPECT_TRUE(manager->queueSignal(testSignal));
    EXPECT_TRUE(manager->queueSignal(testSignal));

    // This should fail as queue is full
    EXPECT_FALSE(manager->queueSignal(testSignal));

    // Wait for queue to empty
    EXPECT_TRUE(
        waitForCondition([&]() { return manager->getQueueSize() == 0; }, 2s));

    // Now we should be able to queue again
    EXPECT_TRUE(manager->queueSignal(testSignal));
}

TEST_F(SafeSignalManagerTest, HandlerPriority) {
    std::atomic<int> executionCount{0};
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // Register handlers with different priorities
    registerTestHandler(
        testSignal,
        [&](SignalID) {
            {
                std::lock_guard<std::mutex> lock(orderMutex);
                executionOrder.push_back(1);
            }
            executionCount++;
        },
        10, "high");

    registerTestHandler(
        testSignal,
        [&](SignalID) {
            {
                std::lock_guard<std::mutex> lock(orderMutex);
                executionOrder.push_back(2);
            }
            executionCount++;
        },
        5, "medium");

    registerTestHandler(
        testSignal,
        [&](SignalID) {
            {
                std::lock_guard<std::mutex> lock(orderMutex);
                executionOrder.push_back(3);
            }
            executionCount++;
        },
        0, "low");

    // Queue a signal
    manager->queueSignal(testSignal);

    // Wait for all handlers to be called
    EXPECT_TRUE(waitForCondition([&]() { return executionCount == 3; }));

    // Verify handlers were called in priority order
    std::lock_guard<std::mutex> lock(orderMutex);
    ASSERT_EQ(executionOrder.size(), 3);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[1], 2);
    EXPECT_EQ(executionOrder[2], 3);
}

TEST_F(SafeSignalManagerTest, HandlerExceptionSafety) {
    // Register a handler that throws an exception
    registerTestHandler(testSignal, [](SignalID) {
        throw std::runtime_error("Test exception");
    });

    // Register a second handler that should still run
    std::atomic<bool> secondHandlerCalled{false};
    registerTestHandler(
        testSignal, [&](SignalID) { secondHandlerCalled = true; }, -10,
        "after_exception");

    // Queue the signal
    manager->queueSignal(testSignal);

    // Wait for second handler to be called
    EXPECT_TRUE(waitForCondition([&]() { return secondHandlerCalled.load(); }));

    // Verify second handler was called despite exception in first handler
    EXPECT_TRUE(secondHandlerCalled);

    // Check error count in stats
    const SignalStats& stats = manager->getSignalStats(testSignal);
    EXPECT_GE(stats.handlerErrors, 1);
}

TEST_F(SafeSignalManagerTest, SignalDispatcherIntegration) {
    // This test attempts to verify integration with the actual signal
    // dispatcher Note: This is a bit tricky as sending real signals in tests
    // can be problematic

    std::atomic<bool> handlerCalled{false};

    // Register a handler
    registerTestHandler(testSignal, [&](SignalID signal) {
        handlerCalled = true;
        EXPECT_EQ(signal, testSignal);
    });

    // Directly call the static dispatcher method
    // This simulates what would happen when a real signal is received
    SafeSignalManager::safeSignalDispatcher(testSignal);

    // Wait for handler to be called
    EXPECT_TRUE(waitForCondition([&]() { return handlerCalled.load(); }));
}

// Stress test for queuing many signals
TEST_F(SafeSignalManagerTest, DISABLED_StressTest) {
    // This is a stress test that might run for a while
    // It's disabled by default but can be enabled for performance testing

    const int signalCount = 1000;
    std::atomic<int> handlerCallCount{0};

    // Register a quick handler
    registerTestHandler(testSignal, [&](SignalID) { handlerCallCount++; });

    // Queue many signals rapidly
    for (int i = 0; i < signalCount; i++) {
        manager->queueSignal(testSignal);
    }

    // Wait for all handlers to be called
    EXPECT_TRUE(waitForCondition(
        [&]() { return handlerCallCount == signalCount; }, 10s));

    // Verify all signals were processed
    EXPECT_EQ(handlerCallCount, signalCount);

    // Check stats
    const SignalStats& stats = manager->getSignalStats(testSignal);
    EXPECT_EQ(stats.received, signalCount);
    EXPECT_EQ(stats.processed, signalCount);
}

// Testing global functions
TEST(SignalSystemGlobalTest, InitializeSignalSystem) {
    // Test that initialization works without crashing
    EXPECT_NO_THROW(initializeSignalSystem(2, 2000));
}

TEST(SignalSystemGlobalTest, InstallPlatformSpecificHandlers) {
    // Test that installation works without crashing
    EXPECT_NO_THROW(installPlatformSpecificHandlers());
}

// Test for the SignalHandlerWithPriority comparison operator
TEST(SignalSystemBasicTest, SignalHandlerWithPriorityComparison) {
    auto handler1 = [](SignalID) {};
    auto handler2 = [](SignalID) {};

    SignalHandlerWithPriority highPriority(handler1, 10, "high");
    SignalHandlerWithPriority lowPriority(handler2, 5, "low");

    // Higher priority should come before lower priority
    EXPECT_TRUE(highPriority < lowPriority);
    EXPECT_FALSE(lowPriority < highPriority);
}
