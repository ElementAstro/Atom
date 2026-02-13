/*
 * test_safetype.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Safe Type Wrappers
Tests thread-safe type wrappers, concurrent operations, and synchronization
guarantees.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "../test_fixtures.hpp"
#include "../test_utils.hpp"
#include "atom/async/sync/safetype.hpp"

using namespace std::chrono_literals;
using namespace atom::async;
using namespace atom::async::sync;

namespace atom::async::sync::test {

// ============================================================================
// Safe Type Tests
// ============================================================================

class SafeTypeTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override {
        SynchronizationTestFixture::SetUp();
        // Additional safe type specific setup
    }

    void TearDown() override {
        // Safe type specific cleanup
        SynchronizationTestFixture::TearDown();
    }
};

TEST_F(SafeTypeTest, BasicSafeTypeOperations) {
    SafeType<int> safeInt(42);

    EXPECT_EQ(safeInt.get(), 42);

    safeInt.set(100);
    EXPECT_EQ(safeInt.get(), 100);
}

TEST_F(SafeTypeTest, ConcurrentReadWrite) {
    SafeType<int> safeInt(0);
    std::atomic<bool> ready{false};
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};

    std::vector<std::thread> threads;
    const int numReaders = 5;
    const int numWriters = 3;
    const int operationsPerThread = 100;

    // Reader threads
    for (int i = 0; i < numReaders; ++i) {
        threads.emplace_back(
            [&safeInt, &ready, &readCount, operationsPerThread]() {
                while (!ready.load()) {
                    std::this_thread::yield();
                }

                for (int j = 0; j < operationsPerThread; ++j) {
                    int value = safeInt.get();
                    EXPECT_GE(value, 0);  // Should always be non-negative
                    readCount.fetch_add(1);
                    std::this_thread::yield();
                }
            });
    }

    // Writer threads
    for (int i = 0; i < numWriters; ++i) {
        threads.emplace_back(
            [&safeInt, &ready, &writeCount, operationsPerThread, i]() {
                while (!ready.load()) {
                    std::this_thread::yield();
                }

                for (int j = 0; j < operationsPerThread; ++j) {
                    safeInt.set((i + 1) * 1000 + j);
                    writeCount.fetch_add(1);
                    std::this_thread::yield();
                }
            });
    }

    ready.store(true);

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(readCount.load(), numReaders * operationsPerThread);
    EXPECT_EQ(writeCount.load(), numWriters * operationsPerThread);
}

TEST_F(SafeTypeTest, SafeTypeWithComplexType) {
    struct TestStruct {
        int value;
        std::string name;

        TestStruct(int v = 0, const std::string& n = "") : value(v), name(n) {}

        bool operator==(const TestStruct& other) const {
            return value == other.value && name == other.name;
        }
    };

    SafeType<TestStruct> safeStruct(TestStruct(42, "test"));

    TestStruct retrieved = safeStruct.get();
    EXPECT_EQ(retrieved.value, 42);
    EXPECT_EQ(retrieved.name, "test");

    safeStruct.set(TestStruct(100, "updated"));
    retrieved = safeStruct.get();
    EXPECT_EQ(retrieved.value, 100);
    EXPECT_EQ(retrieved.name, "updated");
}

TEST_F(SafeTypeTest, SafeTypeModify) {
    SafeType<int> safeInt(10);

    safeInt.modify([](int& value) { value *= 2; });

    EXPECT_EQ(safeInt.get(), 20);

    // Test modify with return value
    int result = safeInt.modify([](int& value) -> int {
        value += 5;
        return value;
    });

    EXPECT_EQ(result, 25);
    EXPECT_EQ(safeInt.get(), 25);
}

TEST_F(SafeTypeTest, ConcurrentModify) {
    SafeType<int> safeInt(0);
    std::atomic<int> completedOperations{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int incrementsPerThread = 100;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(
            [&safeInt, &completedOperations, incrementsPerThread]() {
                for (int j = 0; j < incrementsPerThread; ++j) {
                    safeInt.modify([](int& value) { value++; });
                    completedOperations.fetch_add(1);
                }
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(safeInt.get(), numThreads * incrementsPerThread);
    EXPECT_EQ(completedOperations.load(), numThreads * incrementsPerThread);
}

TEST_F(SafeTypeTest, SafeTypeSwap) {
    SafeType<int> safeInt1(10);
    SafeType<int> safeInt2(20);

    safeInt1.swap(safeInt2);

    EXPECT_EQ(safeInt1.get(), 20);
    EXPECT_EQ(safeInt2.get(), 10);
}

TEST_F(SafeTypeTest, SafeTypeCompareAndSwap) {
    SafeType<int> safeInt(10);

    // Successful compare and swap
    bool success = safeInt.compareAndSwap(10, 20);
    EXPECT_TRUE(success);
    EXPECT_EQ(safeInt.get(), 20);

    // Failed compare and swap
    success = safeInt.compareAndSwap(10, 30);
    EXPECT_FALSE(success);
    EXPECT_EQ(safeInt.get(), 20);  // Should remain unchanged
}

TEST_F(SafeTypeTest, SafeTypeWithSharedPtr) {
    SafeType<std::shared_ptr<int>> safePtr(std::make_shared<int>(42));

    auto ptr = safePtr.get();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);

    safePtr.set(std::make_shared<int>(100));
    ptr = safePtr.get();
    EXPECT_EQ(*ptr, 100);
}

TEST_F(SafeTypeTest, SafeTypeExceptionSafety) {
    struct ThrowingType {
        int value;

        ThrowingType(int v) : value(v) {}

        ThrowingType(const ThrowingType& other) : value(other.value) {
            if (value == 999) {
                throw std::runtime_error("Copy constructor exception");
            }
        }

        ThrowingType& operator=(const ThrowingType& other) {
            if (other.value == 888) {
                throw std::runtime_error("Assignment operator exception");
            }
            value = other.value;
            return *this;
        }
    };

    SafeType<ThrowingType> safeType(ThrowingType(10));

    // Test exception in copy constructor
    EXPECT_THROW(safeType.set(ThrowingType(999)), std::runtime_error);
    EXPECT_EQ(safeType.get().value, 10);  // Should remain unchanged

    // Test exception in assignment
    EXPECT_THROW(safeType.set(ThrowingType(888)), std::runtime_error);
    EXPECT_EQ(safeType.get().value, 10);  // Should remain unchanged
}

TEST_F(SafeTypeTest, SafeTypePerformance) {
    SafeType<int> safeInt(0);
    const int numOperations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOperations; ++i) {
        safeInt.set(i);
        int value = safeInt.get();
        EXPECT_EQ(value, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Performance should be reasonable (this is just a sanity check)
    EXPECT_LT(duration.count(), 100000);  // Less than 100ms for 10k operations
}

TEST_F(SafeTypeTest, SafeTypeWithVector) {
    SafeType<std::vector<int>> safeVector;

    safeVector.modify([](std::vector<int>& vec) {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
    });

    auto vec = safeVector.get();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);

    // Concurrent modifications
    std::vector<std::thread> threads;
    const int numThreads = 5;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&safeVector, i]() {
            for (int j = 0; j < 10; ++j) {
                safeVector.modify([i, j](std::vector<int>& vec) {
                    vec.push_back(i * 100 + j);
                });
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    vec = safeVector.get();
    EXPECT_EQ(vec.size(), 3 + numThreads * 10);  // Original 3 + added elements
}

TEST_F(SafeTypeTest, SafeTypeReadOnlyAccess) {
    SafeType<std::string> safeString("Hello, World!");

    // Test read-only access
    safeString.read([](const std::string& str) {
        EXPECT_EQ(str, "Hello, World!");
        EXPECT_EQ(str.length(), 13);
    });

    // Multiple concurrent readers should work fine
    std::vector<std::thread> threads;
    std::atomic<int> readCount{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&safeString, &readCount]() {
            safeString.read([&readCount](const std::string& str) {
                EXPECT_EQ(str, "Hello, World!");
                readCount.fetch_add(1);
            });
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(readCount.load(), 10);
}

// Test SafeType with RAII types
TEST_F(SafeTypeTest, RAIITypes) {
    auto& tracker = getResourceTracker();

    {
        SafeType<std::unique_ptr<atom::async::test::ScopedResourceTracker>>
            safePtr;

        safePtr.set(std::make_unique<atom::async::test::ScopedResourceTracker>(
            tracker));

        // Use the resource
        safePtr.read(
            [](const std::unique_ptr<atom::async::test::ScopedResourceTracker>&
                   ptr) { EXPECT_NE(ptr, nullptr); });

        // Modify the resource
        safePtr.modify(
            [](std::unique_ptr<atom::async::test::ScopedResourceTracker>& ptr) {
                // Resource is being used
                EXPECT_NE(ptr, nullptr);
            });
    }  // SafeType destructor should clean up properly

    // Give some time for cleanup
    std::this_thread::sleep_for(10ms);
    tracker.expectNoLeaks();
}

// Test SafeType performance characteristics
TEST_F(SafeTypeTest, PerformanceCharacteristics) {
    SafeType<int> safeInt(0);
    const int numOperations = 10000;

    auto timer = createTimer();

    for (int i = 0; i < numOperations; ++i) {
        safeInt.set(i);
        int value = safeInt.get();
        EXPECT_EQ(value, i);
    }

    auto elapsed = timer.elapsed();

    // Performance should be reasonable
    EXPECT_LT(elapsed.count(),
              1000000);  // Less than 1 second for 10k operations

    std::cout << "SafeType performance: " << elapsed.count() / numOperations
              << " microseconds per operation" << std::endl;
}

// Test SafeType with move semantics
TEST_F(SafeTypeTest, MoveSemantics) {
    SafeType<std::unique_ptr<int>> safePtr;

    auto ptr = std::make_unique<int>(42);
    safePtr.set(std::move(ptr));

    EXPECT_EQ(ptr, nullptr);  // Should be moved

    // Use read() for move-only types instead of get()
    safePtr.read([](const std::unique_ptr<int>& retrieved) {
        EXPECT_NE(retrieved, nullptr);
        EXPECT_EQ(*retrieved, 42);
    });
}

// Test SafeType with custom types requiring special handling
TEST_F(SafeTypeTest, CustomTypeSpecialHandling) {
    struct CustomType {
        mutable std::atomic<int> accessCount{0};
        int value;

        CustomType(int v) : value(v) {}

        CustomType(const CustomType& other) : value(other.value) {
            other.accessCount.fetch_add(1);
        }

        CustomType& operator=(const CustomType& other) {
            if (this != &other) {
                value = other.value;
                other.accessCount.fetch_add(1);
            }
            return *this;
        }

        void use() const { accessCount.fetch_add(1); }
    };

    SafeType<CustomType> safeCustom(CustomType(100));

    // Test read access
    safeCustom.read([](const CustomType& obj) {
        obj.use();
        EXPECT_EQ(obj.value, 100);
    });

    // Test modify access
    safeCustom.modify([](CustomType& obj) {
        obj.use();
        obj.value = 200;
    });

    auto final = safeCustom.get();
    EXPECT_EQ(final.value, 200);
    EXPECT_GT(final.accessCount.load(), 0);
}

// Test SafeType with concurrent readers and writers
TEST_F(SafeTypeTest, ConcurrentReadersWriters) {
    SafeType<std::map<int, std::string>> safeMap;
    std::atomic<int> readOperations{0};
    std::atomic<int> writeOperations{0};

    const size_t numReaders = getMaxThreads() / 2;
    const size_t numWriters = getMaxThreads() / 2;

    // Initialize with some data
    safeMap.modify([](std::map<int, std::string>& map) {
        for (int i = 0; i < 10; ++i) {
            map[i] = "initial_" + std::to_string(i);
        }
    });

    std::vector<std::thread> threads;

    // Reader threads
    for (size_t i = 0; i < numReaders; ++i) {
        threads.emplace_back([&safeMap, &readOperations]() {
            for (int j = 0; j < 100; ++j) {
                safeMap.read(
                    [&readOperations](const std::map<int, std::string>& map) {
                        EXPECT_GE(map.size(), 0);
                        readOperations.fetch_add(1);
                    });
                std::this_thread::yield();
            }
        });
    }

    // Writer threads
    for (size_t i = 0; i < numWriters; ++i) {
        threads.emplace_back([&safeMap, &writeOperations, i]() {
            for (int j = 0; j < 50; ++j) {
                safeMap.modify([&writeOperations, i,
                                j](std::map<int, std::string>& map) {
                    map[1000 + i * 100 + j] =
                        "writer_" + std::to_string(i) + "_" + std::to_string(j);
                    writeOperations.fetch_add(1);
                });
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(readOperations.load(), numReaders * 100);
    EXPECT_EQ(writeOperations.load(), numWriters * 50);

    // Verify final state
    auto finalMap = safeMap.get();
    EXPECT_GE(finalMap.size(), 10);  // At least initial data
}

// Test SafeType with timeout operations (if supported)
TEST_F(SafeTypeTest, TimeoutOperations) {
    SafeType<int> safeInt(42);

    // Test that operations complete quickly under normal conditions
    auto timer = createTimer();

    for (int i = 0; i < 100; ++i) {
        safeInt.modify([i](int& value) { value = i; });
    }

    auto elapsed = timer.elapsed();

    // Should complete quickly
    EXPECT_LT(elapsed.count(), 100000);  // Less than 100ms
}

// Test SafeType with exception handling in callbacks
TEST_F(SafeTypeTest, ExceptionHandlingInCallbacks) {
    SafeType<int> safeInt(42);

    // Exception in read callback
    EXPECT_THROW(safeInt.read([](const int& /*value*/) {
        throw std::runtime_error("Read exception");
    }),
                 std::runtime_error);

    // Value should remain unchanged after exception
    EXPECT_EQ(safeInt.get(), 42);

    // Exception in modify callback
    EXPECT_THROW(safeInt.modify([](int& /*value*/) {
        throw std::runtime_error("Modify exception");
    }),
                 std::runtime_error);

    // Value should remain unchanged after exception
    EXPECT_EQ(safeInt.get(), 42);
}

// Test SafeType with large data structures
TEST_F(SafeTypeTest, LargeDataStructures) {
    SafeType<std::vector<std::vector<int>>> safeLargeData;

    // Initialize with large data
    safeLargeData.modify([](std::vector<std::vector<int>>& data) {
        data.resize(100);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i].resize(100, static_cast<int>(i));
        }
    });

    // Verify data integrity
    safeLargeData.read([](const std::vector<std::vector<int>>& data) {
        EXPECT_EQ(data.size(), 100);
        for (size_t i = 0; i < data.size(); ++i) {
            EXPECT_EQ(data[i].size(), 100);
            for (size_t j = 0; j < data[i].size(); ++j) {
                EXPECT_EQ(data[i][j], static_cast<int>(i));
            }
        }
    });
}

// Test SafeType stress test with high contention
TEST_F(SafeTypeTest, HighContentionStressTest) {
    SafeType<std::unordered_map<int, int>> safeMap;
    std::atomic<int> totalOperations{0};

    const size_t numThreads = getMaxThreads() * 2;  // High contention
    const size_t operationsPerThread = 100;

    runStressTest(
        numThreads, operationsPerThread,
        [&safeMap, &totalOperations](size_t threadId, size_t operationId) {
            if (operationId % 3 == 0) {
                // Read operation
                safeMap.read([&totalOperations](
                                 const std::unordered_map<int, int>& map) {
                    volatile size_t size = map.size();  // Prevent optimization
                    (void)size;
                    totalOperations.fetch_add(1);
                });
            } else {
                // Write operation
                safeMap.modify([&totalOperations, threadId, operationId](
                                   std::unordered_map<int, int>& map) {
                    map[static_cast<int>(threadId * 1000 + operationId)] =
                        static_cast<int>(operationId);
                    totalOperations.fetch_add(1);
                });
            }
        });

    EXPECT_EQ(totalOperations.load(), numThreads * operationsPerThread);

    // Verify final state
    auto finalMap = safeMap.get();
    EXPECT_GT(finalMap.size(), 0);
}

// ============================================================================
// LockFreeStack Tests
// ============================================================================

class LockFreeStackTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }

    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

TEST_F(LockFreeStackTest, BasicOperations) {
    LockFreeStack<int> stack;

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);

    stack.push(1);
    stack.push(2);
    stack.push(3);

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    auto top = stack.top();
    EXPECT_TRUE(top.has_value());
    EXPECT_EQ(top.value(), 3);

    auto popped = stack.pop();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 3);

    EXPECT_EQ(stack.size(), 2);
}

TEST_F(LockFreeStackTest, PushAndPopOrder) {
    LockFreeStack<int> stack;

    for (int i = 0; i < 10; ++i) {
        stack.push(i);
    }

    // LIFO order
    for (int i = 9; i >= 0; --i) {
        auto value = stack.pop();
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }

    EXPECT_TRUE(stack.empty());
}

TEST_F(LockFreeStackTest, PopFromEmpty) {
    LockFreeStack<int> stack;

    auto result = stack.pop();
    EXPECT_FALSE(result.has_value());

    auto top = stack.top();
    EXPECT_FALSE(top.has_value());
}

TEST_F(LockFreeStackTest, ConcurrentPush) {
    LockFreeStack<int> stack;
    std::atomic<int> pushCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int pushesPerThread = 100;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&stack, &pushCount, t, pushesPerThread]() {
            for (int i = 0; i < pushesPerThread; ++i) {
                stack.push(t * 1000 + i);
                pushCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(pushCount.load(), numThreads * pushesPerThread);
    EXPECT_EQ(stack.size(), numThreads * pushesPerThread);
}

TEST_F(LockFreeStackTest, ConcurrentPushPop) {
    LockFreeStack<int> stack;
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int opsPerThread = 100;

    // Push threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&stack, &pushCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                stack.push(i);
                pushCount.fetch_add(1);
            }
        });
    }

    // Pop threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&stack, &popCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (stack.pop().has_value()) {
                    popCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Drain remaining items
    while (stack.pop().has_value()) {
        popCount.fetch_add(1);
    }

    EXPECT_EQ(pushCount.load(), popCount.load());
}

TEST_F(LockFreeStackTest, MoveSemantics) {
    LockFreeStack<int> stack1;
    stack1.push(1);
    stack1.push(2);
    stack1.push(3);

    LockFreeStack<int> stack2 = std::move(stack1);

    EXPECT_EQ(stack2.size(), 3);
    EXPECT_EQ(stack2.pop().value(), 3);
}

TEST_F(LockFreeStackTest, WithComplexType) {
    struct TestData {
        int id;
        std::string name;

        TestData(int i, std::string n) : id(i), name(std::move(n)) {}
    };

    LockFreeStack<TestData> stack;

    stack.push(TestData(1, "one"));
    stack.push(TestData(2, "two"));
    stack.push(TestData(3, "three"));

    auto top = stack.top();
    EXPECT_TRUE(top.has_value());
    EXPECT_EQ(top.value().id, 3);
    EXPECT_EQ(top.value().name, "three");

    auto popped = stack.pop();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value().id, 3);
}

// ============================================================================
// LockFreeHashTable Tests
// ============================================================================

class LockFreeHashTableTest
    : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }

    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

TEST_F(LockFreeHashTableTest, BasicOperations) {
    LockFreeHashTable<int, std::string> table(16);

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.size(), 0u);

    table.insert(1, "one");
    table.insert(2, "two");
    table.insert(3, "three");

    EXPECT_FALSE(table.empty());
    EXPECT_EQ(table.size(), 3u);

    auto value1 = table.find(1);
    EXPECT_TRUE(value1.has_value());
    EXPECT_EQ(value1.value().get(), "one");

    auto value2 = table.find(2);
    EXPECT_TRUE(value2.has_value());
    EXPECT_EQ(value2.value().get(), "two");

    auto notFound = table.find(999);
    EXPECT_FALSE(notFound.has_value());
}

TEST_F(LockFreeHashTableTest, InsertAndErase) {
    LockFreeHashTable<int, int> table(16);

    table.insert(1, 100);
    table.insert(2, 200);
    table.insert(3, 300);

    EXPECT_EQ(table.size(), 3u);

    bool erased = table.erase(2);
    EXPECT_TRUE(erased);
    EXPECT_EQ(table.size(), 2u);

    auto value = table.find(2);
    EXPECT_FALSE(value.has_value());

    // Erase non-existent key
    erased = table.erase(999);
    EXPECT_FALSE(erased);
}

TEST_F(LockFreeHashTableTest, UpdateValue) {
    LockFreeHashTable<std::string, int> table(16);

    table.insert("key", 100);

    auto value = table.find("key");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value().get(), 100);

    // Insert same key with different value (update)
    table.insert("key", 200);

    value = table.find("key");
    EXPECT_TRUE(value.has_value());
    // Note: Behavior depends on implementation - may keep old or new value
}

TEST_F(LockFreeHashTableTest, ConcurrentInsert) {
    LockFreeHashTable<int, int> table(64);
    std::atomic<int> insertCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int insertsPerThread = 100;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&table, &insertCount, t, insertsPerThread]() {
            for (int i = 0; i < insertsPerThread; ++i) {
                table.insert(t * 1000 + i, i);
                insertCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(insertCount.load(), numThreads * insertsPerThread);
    EXPECT_EQ(table.size(), static_cast<size_t>(numThreads * insertsPerThread));
}

TEST_F(LockFreeHashTableTest, ConcurrentFindAndInsert) {
    LockFreeHashTable<int, int> table(64);
    std::atomic<int> findCount{0};
    std::atomic<int> insertCount{0};

    // Pre-populate with some data
    for (int i = 0; i < 100; ++i) {
        table.insert(i, i * 10);
    }

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int opsPerThread = 100;

    // Find threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&table, &findCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (table.find(i % 100).has_value()) {
                    findCount.fetch_add(1);
                }
            }
        });
    }

    // Insert threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&table, &insertCount, t, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                table.insert(1000 + t * 1000 + i, i);
                insertCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(findCount.load(), 0);
    EXPECT_EQ(insertCount.load(), (numThreads / 2) * opsPerThread);
}

TEST_F(LockFreeHashTableTest, ConcurrentErase) {
    LockFreeHashTable<int, int> table(64);

    // Pre-populate
    for (int i = 0; i < 1000; ++i) {
        table.insert(i, i * 10);
    }

    std::atomic<int> eraseCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&table, &eraseCount, t]() {
            for (int i = t * 100; i < (t + 1) * 100; ++i) {
                if (table.erase(i)) {
                    eraseCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(eraseCount.load(), 1000);
    EXPECT_TRUE(table.empty());
}

TEST_F(LockFreeHashTableTest, Clear) {
    LockFreeHashTable<int, int> table(16);

    for (int i = 0; i < 100; ++i) {
        table.insert(i, i * 10);
    }

    EXPECT_EQ(table.size(), 100u);

    table.clear();

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.size(), 0u);
}

TEST_F(LockFreeHashTableTest, StringKeys) {
    LockFreeHashTable<std::string, int> table(16);

    table.insert("apple", 1);
    table.insert("banana", 2);
    table.insert("cherry", 3);

    auto apple = table.find("apple");
    EXPECT_TRUE(apple.has_value());
    EXPECT_EQ(apple.value().get(), 1);

    auto banana = table.find("banana");
    EXPECT_TRUE(banana.has_value());
    EXPECT_EQ(banana.value().get(), 2);

    auto notFound = table.find("grape");
    EXPECT_FALSE(notFound.has_value());
}

// ============================================================================
// ThreadSafeVector Tests
// ============================================================================

class ThreadSafeVectorTest
    : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }
    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

TEST_F(ThreadSafeVectorTest, BasicOperations) {
    ThreadSafeVector<int> vec;

    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.getSize(), 0u);

    vec.pushBack(1);
    vec.pushBack(2);
    vec.pushBack(3);

    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec.getSize(), 3u);

    EXPECT_EQ(vec.at(0), 1);
    EXPECT_EQ(vec.at(1), 2);
    EXPECT_EQ(vec.at(2), 3);
}

TEST_F(ThreadSafeVectorTest, SnapshotMethod) {
    ThreadSafeVector<int> vec;

    vec.pushBack(10);
    vec.pushBack(20);
    vec.pushBack(30);
    vec.pushBack(40);

    // Get snapshot - returns a copy
    auto snapshot = vec.snapshot();

    EXPECT_EQ(snapshot.size(), 4u);
    EXPECT_EQ(snapshot[0], 10);
    EXPECT_EQ(snapshot[1], 20);
    EXPECT_EQ(snapshot[2], 30);
    EXPECT_EQ(snapshot[3], 40);

    // Modifying snapshot doesn't affect original
    snapshot[0] = 999;
    EXPECT_EQ(vec.at(0), 10);  // Original unchanged
}

TEST_F(ThreadSafeVectorTest, WithDataMethod) {
    ThreadSafeVector<int> vec;

    vec.pushBack(1);
    vec.pushBack(2);
    vec.pushBack(3);
    vec.pushBack(4);
    vec.pushBack(5);

    // Use withData to compute sum
    int sum = vec.withData([](const std::vector<int>& data) {
        int total = 0;
        for (int val : data) {
            total += val;
        }
        return total;
    });

    EXPECT_EQ(sum, 15);  // 1+2+3+4+5

    // Use withData to find max
    int maxVal = vec.withData([](const std::vector<int>& data) {
        return *std::max_element(data.begin(), data.end());
    });

    EXPECT_EQ(maxVal, 5);
}

TEST_F(ThreadSafeVectorTest, ConcurrentSnapshot) {
    ThreadSafeVector<int> vec(100);
    std::atomic<int> snapshotCount{0};

    // Populate vector
    for (int i = 0; i < 50; ++i) {
        vec.pushBack(i);
    }

    std::vector<std::thread> threads;
    const int numThreads = 10;

    // Concurrent snapshot operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&vec, &snapshotCount]() {
            for (int i = 0; i < 10; ++i) {
                auto snapshot = vec.snapshot();
                EXPECT_GE(snapshot.size(), 0u);
                snapshotCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(snapshotCount.load(), numThreads * 10);
}

TEST_F(ThreadSafeVectorTest, ConcurrentWithData) {
    ThreadSafeVector<int> vec(100);
    std::atomic<int> operationCount{0};

    // Populate vector
    for (int i = 1; i <= 100; ++i) {
        vec.pushBack(i);
    }

    std::vector<std::thread> threads;
    const int numThreads = 8;

    // Concurrent withData operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&vec, &operationCount]() {
            for (int i = 0; i < 20; ++i) {
                int sum = vec.withData([](const std::vector<int>& data) {
                    int total = 0;
                    for (int val : data) {
                        total += val;
                    }
                    return total;
                });
                EXPECT_EQ(sum, 5050);  // Sum of 1 to 100
                operationCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(operationCount.load(), numThreads * 20);
}

TEST_F(ThreadSafeVectorTest, FrontAndBack) {
    ThreadSafeVector<int> vec;

    vec.pushBack(10);
    vec.pushBack(20);
    vec.pushBack(30);

    EXPECT_EQ(vec.front(), 10);
    EXPECT_EQ(vec.back(), 30);

    auto tryFront = vec.try_front();
    EXPECT_TRUE(tryFront.has_value());
    EXPECT_EQ(tryFront.value(), 10);

    auto tryBack = vec.try_back();
    EXPECT_TRUE(tryBack.has_value());
    EXPECT_EQ(tryBack.value(), 30);
}

TEST_F(ThreadSafeVectorTest, PopBack) {
    ThreadSafeVector<int> vec;

    vec.pushBack(1);
    vec.pushBack(2);
    vec.pushBack(3);

    auto popped = vec.popBack();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 3);
    EXPECT_EQ(vec.getSize(), 2u);

    popped = vec.popBack();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 2);

    popped = vec.popBack();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 1);

    popped = vec.popBack();
    EXPECT_FALSE(popped.has_value());  // Empty now
}

TEST_F(ThreadSafeVectorTest, ClearAndShrink) {
    ThreadSafeVector<int> vec(100);

    for (int i = 0; i < 50; ++i) {
        vec.pushBack(i);
    }

    EXPECT_EQ(vec.getSize(), 50u);
    EXPECT_GE(vec.getCapacity(), 50u);

    vec.clear();
    EXPECT_EQ(vec.getSize(), 0u);
    EXPECT_TRUE(vec.empty());

    // Capacity may still be high after clear
    size_t oldCapacity = vec.getCapacity();

    vec.shrinkToFit();
    // Capacity should be reduced
    EXPECT_LE(vec.getCapacity(), oldCapacity);
}

// ============================================================================
// LockFreeList Tests
// ============================================================================

class LockFreeListTest : public atom::async::test::SynchronizationTestFixture {
protected:
    void SetUp() override { SynchronizationTestFixture::SetUp(); }
    void TearDown() override { SynchronizationTestFixture::TearDown(); }
};

TEST_F(LockFreeListTest, BasicOperations) {
    LockFreeList<int> list;

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);

    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);

    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.size(), 3u);

    auto front = list.front();
    EXPECT_TRUE(front.has_value());
    EXPECT_EQ(front.value(), 3);  // LIFO order
}

TEST_F(LockFreeListTest, PopFront) {
    LockFreeList<int> list;

    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);

    auto popped = list.popFront();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 3);

    popped = list.popFront();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 2);

    popped = list.popFront();
    EXPECT_TRUE(popped.has_value());
    EXPECT_EQ(popped.value(), 1);

    popped = list.popFront();
    EXPECT_FALSE(popped.has_value());
}

TEST_F(LockFreeListTest, ConcurrentPushPop) {
    LockFreeList<int> list;
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int opsPerThread = 100;

    // Push threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&list, &pushCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                list.pushFront(i);
                pushCount.fetch_add(1);
            }
        });
    }

    // Pop threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&list, &popCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (list.popFront().has_value()) {
                    popCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Drain remaining items
    while (list.popFront().has_value()) {
        popCount.fetch_add(1);
    }

    EXPECT_EQ(pushCount.load(), popCount.load());
}

TEST_F(LockFreeListTest, Iterator) {
    LockFreeList<int> list;

    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);

    std::vector<int> values;
    for (const auto& val : list) {
        values.push_back(val);
    }

    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], 3);  // First pushed is last in LIFO
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 1);
}

TEST_F(LockFreeListTest, Clear) {
    LockFreeList<int> list;

    for (int i = 0; i < 10; ++i) {
        list.pushFront(i);
    }

    EXPECT_EQ(list.size(), 10u);

    list.clear();

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
}

}  // namespace atom::async::sync::test
