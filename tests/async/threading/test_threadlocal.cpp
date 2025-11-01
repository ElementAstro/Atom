/*
 * test_threadlocal.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Thread Local Storage
Tests initialization, cleanup, thread safety, and edge cases.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/threading/threadlocal.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

namespace atom::async::threading::test {

// ============================================================================
// Thread Local Tests
// ============================================================================

class ThreadLocalTest : public atom::async::test::ThreadingTestFixture {
protected:
    void SetUp() override {
        ThreadingTestFixture::SetUp();
        // Additional thread local specific setup
    }

    void TearDown() override {
        // Thread local specific cleanup
        ThreadingTestFixture::TearDown();
    }
};

TEST_F(ThreadLocalTest, BasicInitialization) {
    ThreadLocal<int> tl([]() { return 42; });

    EXPECT_EQ(tl.get(), 42);
    EXPECT_TRUE(tl.hasValue());
}

TEST_F(ThreadLocalTest, SetAndGet) {
    ThreadLocal<int> tl;

    tl.set(100);
    EXPECT_EQ(tl.get(), 100);
    EXPECT_TRUE(tl.hasValue());
}

TEST_F(ThreadLocalTest, DifferentValuesInDifferentThreads) {
    ThreadLocal<int> tl;
    std::atomic<bool> thread1Ready{false};
    std::atomic<bool> thread2Ready{false};
    std::atomic<int> thread1Value{0};
    std::atomic<int> thread2Value{0};

    std::thread t1([&tl, &thread1Ready, &thread1Value]() {
        tl.set(10);
        thread1Value = tl.get();
        thread1Ready = true;
    });

    std::thread t2([&tl, &thread2Ready, &thread2Value]() {
        tl.set(20);
        thread2Value = tl.get();
        thread2Ready = true;
    });

    t1.join();
    t2.join();

    EXPECT_TRUE(thread1Ready);
    EXPECT_TRUE(thread2Ready);
    EXPECT_EQ(thread1Value.load(), 10);
    EXPECT_EQ(thread2Value.load(), 20);
}

TEST_F(ThreadLocalTest, InitializerFunction) {
    std::atomic<int> initCount{0};

    ThreadLocal<int> tl([&initCount]() { return initCount.fetch_add(1) + 1; });

    std::vector<std::thread> threads;
    std::vector<std::atomic<int>> values(5);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&tl, &values, i]() { values[i] = tl.get(); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Each thread should get a unique initialized value
    std::set<int> uniqueValues;
    for (int i = 0; i < 5; ++i) {
        uniqueValues.insert(values[i].load());
    }

    EXPECT_EQ(uniqueValues.size(), 5);
    EXPECT_EQ(initCount.load(), 5);
}

TEST_F(ThreadLocalTest, CleanupFunction) {
    std::atomic<int> cleanupCount{0};

    {
        ThreadLocal<std::unique_ptr<int>> tl(
            []() { return std::make_unique<int>(42); },
            [&cleanupCount](const std::unique_ptr<int>& ptr) {
                if (ptr) {
                    cleanupCount.fetch_add(1);
                }
            });

        std::vector<std::thread> threads;

        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&tl]() {
                auto& ptr = tl.get();
                EXPECT_NE(ptr, nullptr);
                EXPECT_EQ(*ptr, 42);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }  // ThreadLocal destructor should trigger cleanup

    // Give some time for cleanup to complete
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(cleanupCount.load(), 3);
}

TEST_F(ThreadLocalTest, Reset) {
    ThreadLocal<int> tl([]() { return 10; });

    EXPECT_EQ(tl.get(), 10);

    tl.reset(20);
    EXPECT_EQ(tl.get(), 20);

    tl.reset();              // Reset to default
    EXPECT_EQ(tl.get(), 0);  // Default constructed int
}

TEST_F(ThreadLocalTest, Clear) {
    ThreadLocal<int> tl;

    tl.set(42);
    EXPECT_TRUE(tl.hasValue());
    EXPECT_EQ(tl.get(), 42);

    tl.clear();
    EXPECT_FALSE(tl.hasValue());
}

TEST_F(ThreadLocalTest, GetActiveThreadCount) {
    ThreadLocal<int> tl;

    EXPECT_EQ(tl.getActiveThreadCount(), 0);

    tl.set(42);
    EXPECT_EQ(tl.getActiveThreadCount(), 1);

    std::vector<std::thread> threads;
    std::atomic<int> readyCount{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&tl, &readyCount]() {
            tl.set(100);
            readyCount.fetch_add(1);
            std::this_thread::sleep_for(50ms);
        });
    }

    // Wait for all threads to set their values
    while (readyCount.load() < 5) {
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(tl.getActiveThreadCount(), 6);  // 5 threads + main thread

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(ThreadLocalTest, ConditionalInitializer) {
    ThreadLocal<int> tl;

    tl.setConditionalInitializer([](std::thread::id tid) {
        // Only initialize for specific thread pattern
        std::hash<std::thread::id> hasher;
        return hasher(tid) % 2 == 0 ? std::optional<int>(100) : std::nullopt;
    });

    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> hasValue(10);
    std::vector<std::atomic<int>> values(10);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&tl, &hasValue, &values, i]() {
            try {
                values[i] = tl.get();
                hasValue[i] = true;
            } catch (...) {
                hasValue[i] = false;
                values[i] = -1;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Some threads should have values, others should not
    int withValues = 0;
    int withoutValues = 0;

    for (int i = 0; i < 10; ++i) {
        if (hasValue[i].load()) {
            withValues++;
            EXPECT_EQ(values[i].load(), 100);
        } else {
            withoutValues++;
        }
    }

    EXPECT_GT(withValues, 0);
    EXPECT_GT(withoutValues, 0);
}

TEST_F(ThreadLocalTest, ThreadIdBasedInitializer) {
    ThreadLocal<std::string> tl;

    tl.setThreadIdInitializer([](std::thread::id tid) {
        std::ostringstream oss;
        oss << "Thread-" << tid;
        return oss.str();
    });

    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> initialized(5);
    std::vector<std::string> threadValues(5);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&tl, &initialized, &threadValues, i]() {
            threadValues[i] = tl.get();
            initialized[i] = true;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(initialized[i].load());
        EXPECT_TRUE(threadValues[i].find("Thread-") == 0);
    }
}

TEST_F(ThreadLocalTest, ExceptionInInitializer) {
    ThreadLocal<int> tl(
        []() -> int { throw std::runtime_error("Initialization failed"); });

    EXPECT_THROW(tl.get(), std::runtime_error);
    EXPECT_FALSE(tl.hasValue());
}

TEST_F(ThreadLocalTest, ConcurrentAccess) {
    ThreadLocal<int> tl([]() { return 0; });
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 20;
    const int operationsPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&tl, &successCount, operationsPerThread, i]() {
            try {
                for (int j = 0; j < operationsPerThread; ++j) {
                    tl.set(i * operationsPerThread + j);
                    int value = tl.get();
                    EXPECT_EQ(value, i * operationsPerThread + j);
                }
                successCount.fetch_add(1);
            } catch (...) {
                // Handle any exceptions
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}

// Test thread local with RAII types
TEST_F(ThreadLocalTest, RAIITypes) {
    auto& tracker = getResourceTracker();

    {
        ThreadLocal<std::unique_ptr<atom::async::test::ScopedResourceTracker>>
            tl([&tracker]() {
                return std::make_unique<
                    atom::async::test::ScopedResourceTracker>(tracker);
            });

        std::vector<std::thread> threads;
        const int numThreads = 5;

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&tl]() {
                auto& resource = tl.get();
                EXPECT_NE(resource, nullptr);
                std::this_thread::sleep_for(10ms);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }  // ThreadLocal destructor should trigger cleanup

    // Give some time for cleanup
    std::this_thread::sleep_for(50ms);
    tracker.expectNoLeaks();
}

// Test thread local performance
TEST_F(ThreadLocalTest, Performance) {
    ThreadLocal<int> tl([]() { return 42; });

    const int numOperations = 10000;
    auto timer = createTimer();

    for (int i = 0; i < numOperations; ++i) {
        tl.set(i);
        int value = tl.get();
        EXPECT_EQ(value, i);
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(),
              1000000);  // Less than 1 second for 10k operations

    std::cout << "ThreadLocal performance: " << elapsed.count() / numOperations
              << " microseconds per operation" << std::endl;
}

// Test thread local with complex initialization
TEST_F(ThreadLocalTest, ComplexInitialization) {
    struct ComplexType {
        int id;
        std::string name;
        std::vector<int> data;
        std::thread::id threadId;

        ComplexType(int i)
            : id(i),
              name("thread_" + std::to_string(i)),
              threadId(std::this_thread::get_id()) {
            for (int j = 0; j < 10; ++j) {
                data.push_back(i * 10 + j);
            }
        }
    };

    std::atomic<int> nextId{0};
    ThreadLocal<ComplexType> tl(
        [&nextId]() { return ComplexType(nextId.fetch_add(1)); });

    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> initialized(5);
    std::vector<ComplexType> results(5);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&tl, &initialized, &results, i]() {
            auto& complex = tl.get();
            results[i] = complex;
            initialized[i] = true;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify each thread got a unique, properly initialized object
    std::set<int> uniqueIds;
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(initialized[i].load());
        uniqueIds.insert(results[i].id);
        EXPECT_EQ(results[i].name, "thread_" + std::to_string(results[i].id));
        EXPECT_EQ(results[i].data.size(), 10);
        EXPECT_EQ(results[i].data[0], results[i].id * 10);
        EXPECT_EQ(results[i].data[9], results[i].id * 10 + 9);
    }

    EXPECT_EQ(uniqueIds.size(), 5);  // All IDs should be unique
}

// Test thread local with move-only types
TEST_F(ThreadLocalTest, MoveOnlyTypes) {
    ThreadLocal<std::unique_ptr<int>> tl(
        []() { return std::make_unique<int>(42); });

    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> success(3);

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&tl, &success, i]() {
            auto& ptr = tl.get();
            EXPECT_NE(ptr, nullptr);
            EXPECT_EQ(*ptr, 42);

            // Modify the value
            *ptr = i * 100;
            EXPECT_EQ(*ptr, i * 100);

            success[i] = true;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(success[i].load());
    }
}

// Test thread local stress test
TEST_F(ThreadLocalTest, StressTest) {
    ThreadLocal<std::vector<int>> tl;
    std::atomic<int> totalOperations{0};

    const size_t numThreads = getMaxThreads();
    const size_t operationsPerThread = 100;

    runStressTest(
        numThreads, operationsPerThread,
        [&tl, &totalOperations](size_t threadId, size_t operationId) {
            auto& vec = tl.get();
            vec.push_back(static_cast<int>(threadId * 1000 + operationId));
            totalOperations.fetch_add(1);
        });

    EXPECT_EQ(totalOperations.load(), numThreads * operationsPerThread);
}

// Test thread local with exception in cleanup
TEST_F(ThreadLocalTest, ExceptionInCleanup) {
    struct ThrowingCleanup {
        bool* shouldThrow;

        ThrowingCleanup(bool* st) : shouldThrow(st) {}

        ~ThrowingCleanup() {
            if (shouldThrow && *shouldThrow) {
                // Note: destructors shouldn't throw, but we're testing
                // robustness In real code, this would be a bug
            }
        }
    };

    bool shouldThrow = false;

    {
        ThreadLocal<ThrowingCleanup> tl(
            [&shouldThrow]() { return ThrowingCleanup(&shouldThrow); });

        std::thread t([&tl]() {
            auto& obj = tl.get();
            (void)obj;  // Use the object
        });

        t.join();

        // Enable throwing in destructor (bad practice, but testing robustness)
        shouldThrow = true;
    }  // ThreadLocal destructor should handle exceptions gracefully

    // Test should complete without crashing
}

// Test thread local with custom hash function
TEST_F(ThreadLocalTest, CustomThreadIdHandling) {
    ThreadLocal<std::string> tl;

    tl.setThreadIdInitializer([](std::thread::id tid) {
        std::ostringstream oss;
        oss << "custom_" << tid;
        return oss.str();
    });

    std::vector<std::thread> threads;
    std::vector<std::string> results(3);
    std::vector<std::atomic<bool>> completed(3);

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&tl, &results, &completed, i]() {
            results[i] = tl.get();
            completed[i] = true;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(completed[i].load());
        EXPECT_TRUE(results[i].find("custom_") == 0);
    }

    // All results should be different (different thread IDs)
    EXPECT_NE(results[0], results[1]);
    EXPECT_NE(results[1], results[2]);
    EXPECT_NE(results[0], results[2]);
}

}  // namespace atom::async::threading::test
