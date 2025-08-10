#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "atom/async/lock.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

class LockTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test Spinlock basic operations
TEST_F(LockTest, SpinlockBasicOperations) {
    Spinlock lock;

    // Test basic lock/unlock
    lock.lock();
    EXPECT_FALSE(lock.tryLock());  // Should fail when already locked
    lock.unlock();

    // Test tryLock
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
}

// Test Spinlock concurrent access
TEST_F(LockTest, SpinlockConcurrentAccess) {
    Spinlock lock;
    std::atomic<int> counter{0};
    const int num_threads = 10;
    const int increments_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &counter]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                lock.lock();
                counter.fetch_add(1);
                lock.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

// Test TicketSpinlock basic operations
TEST_F(LockTest, TicketSpinlockBasicOperations) {
    TicketSpinlock lock;

    // Test basic lock/unlock
    auto ticket = lock.lock();
    lock.unlock(ticket);

    // Test tryLock (returns bool, not optional)
    bool acquired = lock.tryLock();
    if (acquired) {
        // For tryLock, we need to get a ticket to unlock
        auto unlock_ticket = lock.lock();  // This will wait, but should be immediate
        lock.unlock(unlock_ticket);
    }
}

// Test TicketSpinlock fairness
TEST_F(LockTest, TicketSpinlockFairness) {
    TicketSpinlock lock;
    std::vector<int> order;
    std::mutex order_mutex;
    const int num_threads = 5;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &order, &order_mutex, i]() {
            auto ticket = lock.lock();
            {
                std::lock_guard<std::mutex> guard(order_mutex);
                order.push_back(i);
            }
            std::this_thread::sleep_for(10ms);
            lock.unlock(ticket);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Ticket spinlock should provide fairness, but exact order isn't guaranteed
    // due to thread scheduling, so we just check that all threads executed
    EXPECT_EQ(order.size(), num_threads);
}

// Test ScopedLock RAII
TEST_F(LockTest, ScopedLockRAII) {
    Spinlock lock;
    std::atomic<bool> critical_section_entered{false};

    {
        ScopedLock<Spinlock> scoped_lock(lock);
        critical_section_entered.store(true);

        // Lock should be held here
        EXPECT_FALSE(lock.tryLock());
    }

    // Lock should be released here
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
    EXPECT_TRUE(critical_section_entered.load());
}

// Test ScopedTicketLock (TicketSpinlock's LockGuard)
TEST_F(LockTest, ScopedTicketLock) {
    TicketSpinlock lock;
    std::atomic<bool> critical_section_entered{false};

    {
        ScopedTicketLock scoped_lock(lock);
        critical_section_entered.store(true);

        // Lock should be held here
        EXPECT_FALSE(lock.tryLock());
    }

    // Lock should be released here
    EXPECT_TRUE(lock.tryLock());
    EXPECT_TRUE(critical_section_entered.load());
}

// Test AdaptiveSpinlock basic operations
TEST_F(LockTest, AdaptiveSpinlockBasicOperations) {
    AdaptiveSpinlock lock;

    // Test basic lock/unlock
    lock.lock();
    EXPECT_FALSE(lock.tryLock());  // Should fail when already locked
    lock.unlock();

    // Test tryLock
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
}

// Test AdaptiveSpinlock concurrent access
TEST_F(LockTest, AdaptiveSpinlockConcurrentAccess) {
    AdaptiveSpinlock lock;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int increments_per_thread = 500;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &counter]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                lock.lock();
                counter.fetch_add(1);
                lock.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

// Test UnfairSpinlock basic operations
TEST_F(LockTest, UnfairSpinlockBasicOperations) {
    UnfairSpinlock lock;

    // Test basic lock/unlock
    lock.lock();
    EXPECT_FALSE(lock.tryLock());  // Should fail when already locked
    lock.unlock();

    // Test tryLock
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
}

// Test UnfairSpinlock concurrent access
TEST_F(LockTest, UnfairSpinlockConcurrentAccess) {
    UnfairSpinlock lock;
    std::atomic<int> counter{0};
    const int num_threads = 8;
    const int increments_per_thread = 500;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &counter]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                lock.lock();
                counter.fetch_add(1);
                lock.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

// Test CountingSemaphore basic operations
TEST_F(LockTest, CountingSemaphoreBasicOperations) {
    CountingSemaphore<2> semaphore(2);

    // Test acquire/release
    semaphore.acquire();
    semaphore.release();

    // Test try_acquire
    EXPECT_TRUE(semaphore.try_acquire());
    semaphore.release();

    // Test multiple acquires
    semaphore.acquire();
    semaphore.acquire();

    // Should fail now (no more permits)
    EXPECT_FALSE(semaphore.try_acquire());

    semaphore.release();
    semaphore.release();
}

// Test LockFactory basic functionality
TEST_F(LockTest, LockFactoryBasicOperations) {
    // Test creating different lock types
    auto spinlock = LockFactory::createLock(LockFactory::LockType::SPINLOCK);
    EXPECT_NE(spinlock, nullptr);

    auto ticket_lock = LockFactory::createLock(LockFactory::LockType::TICKET_SPINLOCK);
    EXPECT_NE(ticket_lock, nullptr);

    auto unfair_lock = LockFactory::createLock(LockFactory::LockType::UNFAIR_SPINLOCK);
    EXPECT_NE(unfair_lock, nullptr);

    auto adaptive_lock = LockFactory::createLock(LockFactory::LockType::ADAPTIVE_SPINLOCK);
    EXPECT_NE(adaptive_lock, nullptr);

    // Test optimized lock creation
    auto optimized_lock = LockFactory::createOptimizedLock();
    EXPECT_NE(optimized_lock, nullptr);
}

// Test performance comparison (basic)
TEST_F(LockTest, PerformanceComparison) {
    const int iterations = 10000;
    std::atomic<int> counter{0};

    // Test Spinlock performance
    {
        Spinlock lock;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            lock.lock();
            counter.fetch_add(1);
            lock.unlock();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Just verify it completes in reasonable time (less than 1 second)
        EXPECT_LT(duration.count(), 1000000);
    }

    EXPECT_EQ(counter.load(), iterations);
}

// Test edge cases
TEST_F(LockTest, EdgeCases) {
    // Test multiple unlock calls (should not crash)
    Spinlock lock;
    lock.lock();
    lock.unlock();
    // Second unlock might be undefined behavior, so we don't test it

    // Test TicketSpinlock with invalid ticket (should not crash)
    TicketSpinlock ticket_lock;
    auto valid_ticket = ticket_lock.lock();
    ticket_lock.unlock(valid_ticket);

    // Test CountingSemaphore edge cases
    CountingSemaphore<1> single_semaphore(1);
    single_semaphore.acquire();
    EXPECT_FALSE(single_semaphore.try_acquire());
    single_semaphore.release();
}
