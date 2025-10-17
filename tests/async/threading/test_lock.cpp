/*
 * test_lock.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Threading Lock Library
Tests various lock implementations, concurrent access patterns, and edge cases.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/threading/lock.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::threading::test {

// ============================================================================
// Lock Tests
// ============================================================================

class LockTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override {
        SynchronizationTestFixture::SetUp();
        // Additional lock-specific setup
    }

    void TearDown() override {
        // Lock-specific cleanup
        SynchronizationTestFixture::TearDown();
    }

    // Helper function to test basic lock functionality
    template <typename LockType>
    void testBasicLockFunctionality() {
        LockType lock;
        std::atomic<int> counter{0};
        std::atomic<bool> ready{false};

        std::vector<std::thread> threads;
        const int numThreads = 10;
        const int incrementsPerThread = 100;

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(
                [&lock, &counter, &ready, incrementsPerThread]() {
                    while (!ready.load()) {
                        std::this_thread::yield();
                    }

                    for (int j = 0; j < incrementsPerThread; ++j) {
                        std::lock_guard<LockType> guard(lock);
                        ++counter;
                    }
                });
        }

        ready.store(true);

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(counter.load(), numThreads * incrementsPerThread);
    }

    // Helper function to test tryLock functionality
    template <typename LockType>
    void testTryLockFunctionality() {
        LockType lock;
        std::atomic<bool> lockAcquired{false};

        std::thread t1([&lock, &lockAcquired]() {
            auto ticket = lock.lock();
            lockAcquired.store(true);
            std::this_thread::sleep_for(100ms);
            lock.unlock(ticket);
        });

        // Wait for first thread to acquire lock
        while (!lockAcquired.load()) {
            std::this_thread::yield();
        }

        // Try to acquire lock from another thread - should fail
        EXPECT_FALSE(lock.tryLock());

        t1.join();

        // Now should be able to acquire lock
        EXPECT_TRUE(lock.tryLock());
        lock.unlock();
    }
};

TEST_F(LockTest, SpinlockBasicFunctionality) {
    testBasicLockFunctionality<Spinlock>();
}

TEST_F(LockTest, SpinlockTryLock) { testTryLockFunctionality<Spinlock>(); }

TEST_F(LockTest, TicketSpinlockBasicFunctionality) {
    testBasicLockFunctionality<TicketSpinlock>();
}

// TEST_F(LockTest, TicketSpinlockTryLock) {
//     testTryLockFunctionality<TicketSpinlock>();
// }
// Note: TicketSpinlock API is incompatible with standard lock interface
// tryLock() doesn't return ticket needed for unlock()

TEST_F(LockTest, UnfairSpinlockBasicFunctionality) {
    testBasicLockFunctionality<UnfairSpinlock>();
}

TEST_F(LockTest, UnfairSpinlockTryLock) {
    testTryLockFunctionality<UnfairSpinlock>();
}

TEST_F(LockTest, AdaptiveSpinlockBasicFunctionality) {
    testBasicLockFunctionality<AdaptiveSpinlock>();
}

TEST_F(LockTest, AdaptiveSpinlockTryLock) {
    testTryLockFunctionality<AdaptiveSpinlock>();
}

#ifdef ATOM_HAS_ATOMIC_WAIT
TEST_F(LockTest, AtomicWaitLockBasicFunctionality) {
    testBasicLockFunctionality<AtomicWaitLock>();
}

TEST_F(LockTest, AtomicWaitLockTryLock) {
    testTryLockFunctionality<AtomicWaitLock>();
}
#endif

// Test CountingSemaphore
TEST_F(LockTest, CountingSemaphoreBasicFunctionality) {
    CountingSemaphore<5> semaphore(3);  // Allow 3 concurrent accesses
    std::atomic<int> activeCount{0};
    std::atomic<int> maxActiveCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&semaphore, &activeCount, &maxActiveCount]() {
            semaphore.acquire();

            int current = activeCount.fetch_add(1) + 1;
            int expected = maxActiveCount.load();
            while (current > expected &&
                   !maxActiveCount.compare_exchange_weak(expected, current)) {
                expected = maxActiveCount.load();
            }

            std::this_thread::sleep_for(10ms);

            activeCount.fetch_sub(1);
            semaphore.release();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should never have more than 3 active at once
    EXPECT_LE(maxActiveCount.load(), 3);
}

// Test LockFactory
TEST_F(LockTest, LockFactoryCreation) {
    auto spinlock = LockFactory::createLock(LockFactory::LockType::SPINLOCK);
    EXPECT_NE(spinlock, nullptr);

    auto ticketSpinlock =
        LockFactory::createLock(LockFactory::LockType::TICKET_SPINLOCK);
    EXPECT_NE(ticketSpinlock, nullptr);

    auto adaptiveSpinlock =
        LockFactory::createLock(LockFactory::LockType::ADAPTIVE_SPINLOCK);
    EXPECT_NE(adaptiveSpinlock, nullptr);
}

// Test lock performance characteristics
TEST_F(LockTest, LockPerformanceComparison) {
    const int numOperations = 10000;
    std::atomic<int> counter{0};

    // Test Spinlock performance
    {
        Spinlock lock;
        counter.store(0);

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&lock, &counter, numOperations]() {
                for (int j = 0; j < numOperations; ++j) {
                    std::lock_guard<Spinlock> guard(lock);
                    ++counter;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto spinlockTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        EXPECT_EQ(counter.load(), 4 * numOperations);
        EXPECT_GT(spinlockTime.count(), 0);  // Just ensure it took some time
    }
}

// Test edge cases
TEST_F(LockTest, EdgeCases) {
    // Test multiple lock/unlock cycles
    Spinlock lock;
    for (int i = 0; i < 1000; ++i) {
        lock.lock();
        lock.unlock();
    }

    // Test tryLock in tight loop
    for (int i = 0; i < 1000; ++i) {
        if (lock.tryLock()) {
            lock.unlock();
        }
    }
}

// Test platform-specific locks
#ifdef ATOM_PLATFORM_WINDOWS
TEST_F(LockTest, WindowsSpinlockBasicFunctionality) {
    testBasicLockFunctionality<WindowsSpinlock>();
}

TEST_F(LockTest, WindowsSpinlockTryLock) {
    testTryLockFunctionality<WindowsSpinlock>();
}

TEST_F(LockTest, WindowsSharedMutexBasicFunctionality) {
    WindowsSharedMutex mutex;
    std::atomic<int> readerCount{0};
    std::atomic<int> writerCount{0};

    // Test shared (read) locks
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i) {
        readers.emplace_back([&mutex, &readerCount]() {
            mutex.lockShared();
            readerCount.fetch_add(1);
            std::this_thread::sleep_for(50ms);
            readerCount.fetch_sub(1);
            mutex.unlockShared();
        });
    }

    // Test exclusive (write) lock
    std::thread writer([&mutex, &writerCount, &readerCount]() {
        std::this_thread::sleep_for(25ms);  // Let readers start
        mutex.lock();
        EXPECT_EQ(readerCount.load(), 0);  // No readers when writer has lock
        writerCount.fetch_add(1);
        std::this_thread::sleep_for(50ms);
        writerCount.fetch_sub(1);
        mutex.unlock();
    });

    for (auto& reader : readers) {
        reader.join();
    }
    writer.join();
}
#endif

#ifdef ATOM_PLATFORM_MACOS
TEST_F(LockTest, DarwinSpinlockBasicFunctionality) {
    testBasicLockFunctionality<DarwinSpinlock>();
}

TEST_F(LockTest, DarwinSpinlockTryLock) {
    testTryLockFunctionality<DarwinSpinlock>();
}
#endif

#ifdef ATOM_PLATFORM_LINUX
TEST_F(LockTest, LinuxFutexLockBasicFunctionality) {
    testBasicLockFunctionality<LinuxFutexLock>();
}

TEST_F(LockTest, LinuxFutexLockTryLock) {
    testTryLockFunctionality<LinuxFutexLock>();
}
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
TEST_F(LockTest, BoostSpinlockBasicFunctionality) {
    testBasicLockFunctionality<BoostSpinlock>();
}

TEST_F(LockTest, BoostSpinlockTryLock) {
    testTryLockFunctionality<BoostSpinlock>();
}
#endif

// Test lock contention scenarios
TEST_F(LockTest, HighContentionScenario) {
    Spinlock lock;
    std::atomic<int> counter{0};
    const int numThreads = getMaxThreads() * 2;  // Create high contention
    const int incrementsPerThread = 1000;

    runStressTest(
        numThreads, incrementsPerThread,
        [&lock, &counter](size_t /*threadId*/, size_t /*operationId*/) {
            std::lock_guard<Spinlock> guard(lock);
            ++counter;
            // Simulate some work while holding the lock
            volatile int dummy = 0;
            for (int i = 0; i < 10; ++i) {
                dummy += i;
            }
        });

    EXPECT_EQ(counter.load(), numThreads * incrementsPerThread);
}

/*
// Test lock fairness (best effort) - DISABLED due to TicketSpinlock API
incompatibility TEST_F(LockTest, LockFairness) { TicketSpinlock lock; // Ticket
spinlock should be more fair std::vector<std::atomic<int>> threadCounts(10);
    std::atomic<bool> stopTest{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&lock, &threadCounts, &stopTest, i]() {
            while (!stopTest.load()) {
                std::lock_guard<TicketSpinlock> guard(lock);
                threadCounts[i].fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    std::this_thread::sleep_for(200ms);
    stopTest.store(true);

    for (auto& thread : threads) {
        thread.join();
    }

    // Check that no thread was completely starved
    for (int i = 0; i < 10; ++i) {
        EXPECT_GT(threadCounts[i].load(), 0) << "Thread " << i << " was
starved";
    }

    // Check that the distribution is somewhat fair (no thread got more than 10x
another) int minCount = threadCounts[0].load(); int maxCount =
threadCounts[0].load(); for (int i = 1; i < 10; ++i) { minCount =
std::min(minCount, threadCounts[i].load()); maxCount = std::max(maxCount,
threadCounts[i].load());
    }

    if (minCount > 0) {
        EXPECT_LE(maxCount / minCount, 10) << "Lock fairness test failed:
max/min ratio too high";
    }
}
*/

// Test adaptive spinlock behavior
TEST_F(LockTest, AdaptiveSpinlockBehavior) {
    AdaptiveSpinlock lock;
    std::atomic<bool> longTaskRunning{false};
    std::atomic<bool> shortTaskCompleted{false};

    // Long-running task that holds the lock
    std::thread longTask([&lock, &longTaskRunning]() {
        lock.lock();
        longTaskRunning.store(true);
        std::this_thread::sleep_for(100ms);  // Hold lock for a while
        lock.unlock();
    });

    // Wait for long task to acquire lock
    while (!longTaskRunning.load()) {
        std::this_thread::yield();
    }

    // Short task that should adapt to blocking behavior
    auto start = std::chrono::steady_clock::now();
    std::thread shortTask([&lock, &shortTaskCompleted]() {
        lock.lock();
        shortTaskCompleted.store(true);
        lock.unlock();
    });

    longTask.join();
    shortTask.join();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(shortTaskCompleted);
    EXPECT_GE(elapsed, 90ms);  // Should have waited for the long task
}

}  // namespace atom::async::threading::test
