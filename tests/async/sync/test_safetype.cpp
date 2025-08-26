/*
 * test_safetype.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Safe Type Wrappers
Tests thread-safe type wrappers, concurrent operations, and synchronization guarantees.

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

#include "atom/async/sync/safetype.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

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
        threads.emplace_back([&safeInt, &ready, &readCount, operationsPerThread]() {
            while (!ready.load()) {
                std::this_thread::yield();
            }
            
            for (int j = 0; j < operationsPerThread; ++j) {
                int value = safeInt.get();
                EXPECT_GE(value, 0); // Should always be non-negative
                readCount.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }
    
    // Writer threads
    for (int i = 0; i < numWriters; ++i) {
        threads.emplace_back([&safeInt, &ready, &writeCount, operationsPerThread, i]() {
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
    
    safeInt.modify([](int& value) {
        value *= 2;
    });
    
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
        threads.emplace_back([&safeInt, &completedOperations, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                safeInt.modify([](int& value) {
                    value++;
                });
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
    EXPECT_EQ(safeInt.get(), 20); // Should remain unchanged
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
    EXPECT_EQ(safeType.get().value, 10); // Should remain unchanged
    
    // Test exception in assignment
    EXPECT_THROW(safeType.set(ThrowingType(888)), std::runtime_error);
    EXPECT_EQ(safeType.get().value, 10); // Should remain unchanged
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
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable (this is just a sanity check)
    EXPECT_LT(duration.count(), 100000); // Less than 100ms for 10k operations
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
    EXPECT_EQ(vec.size(), 3 + numThreads * 10); // Original 3 + added elements
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
        SafeType<std::unique_ptr<atom::async::test::ScopedResourceTracker>> safePtr;

        safePtr.set(std::make_unique<atom::async::test::ScopedResourceTracker>(tracker));

        // Use the resource
        safePtr.read([](const std::unique_ptr<atom::async::test::ScopedResourceTracker>& ptr) {
            EXPECT_NE(ptr, nullptr);
        });

        // Modify the resource
        safePtr.modify([](std::unique_ptr<atom::async::test::ScopedResourceTracker>& ptr) {
            // Resource is being used
            EXPECT_NE(ptr, nullptr);
        });
    } // SafeType destructor should clean up properly

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
    EXPECT_LT(elapsed.count(), 1000000); // Less than 1 second for 10k operations

    std::cout << "SafeType performance: "
              << elapsed.count() / numOperations << " microseconds per operation" << std::endl;
}

// Test SafeType with move semantics
TEST_F(SafeTypeTest, MoveSemantics) {
    SafeType<std::unique_ptr<int>> safePtr;

    auto ptr = std::make_unique<int>(42);
    safePtr.set(std::move(ptr));

    EXPECT_EQ(ptr, nullptr); // Should be moved

    auto retrieved = safePtr.get();
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
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

        void use() const {
            accessCount.fetch_add(1);
        }
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
                safeMap.read([&readOperations](const std::map<int, std::string>& map) {
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
                safeMap.modify([&writeOperations, i, j](std::map<int, std::string>& map) {
                    map[1000 + i * 100 + j] = "writer_" + std::to_string(i) + "_" + std::to_string(j);
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
    EXPECT_GE(finalMap.size(), 10); // At least initial data
}

// Test SafeType with timeout operations (if supported)
TEST_F(SafeTypeTest, TimeoutOperations) {
    SafeType<int> safeInt(42);

    // Test that operations complete quickly under normal conditions
    auto timer = createTimer();

    for (int i = 0; i < 100; ++i) {
        safeInt.modify([i](int& value) {
            value = i;
        });
    }

    auto elapsed = timer.elapsed();

    // Should complete quickly
    EXPECT_LT(elapsed.count(), 100000); // Less than 100ms
}

// Test SafeType with exception handling in callbacks
TEST_F(SafeTypeTest, ExceptionHandlingInCallbacks) {
    SafeType<int> safeInt(42);

    // Exception in read callback
    EXPECT_THROW(
        safeInt.read([](const int& /*value*/) {
            throw std::runtime_error("Read exception");
        }),
        std::runtime_error
    );

    // Value should remain unchanged after exception
    EXPECT_EQ(safeInt.get(), 42);

    // Exception in modify callback
    EXPECT_THROW(
        safeInt.modify([](int& /*value*/) {
            throw std::runtime_error("Modify exception");
        }),
        std::runtime_error
    );

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

    const size_t numThreads = getMaxThreads() * 2; // High contention
    const size_t operationsPerThread = 100;

    runStressTest(numThreads, operationsPerThread,
                  [&safeMap, &totalOperations](size_t threadId, size_t operationId) {
        if (operationId % 3 == 0) {
            // Read operation
            safeMap.read([&totalOperations](const std::unordered_map<int, int>& map) {
                volatile size_t size = map.size(); // Prevent optimization
                (void)size;
                totalOperations.fetch_add(1);
            });
        } else {
            // Write operation
            safeMap.modify([&totalOperations, threadId, operationId](std::unordered_map<int, int>& map) {
                map[static_cast<int>(threadId * 1000 + operationId)] = static_cast<int>(operationId);
                totalOperations.fetch_add(1);
            });
        }
    });

    EXPECT_EQ(totalOperations.load(), numThreads * operationsPerThread);

    // Verify final state
    auto finalMap = safeMap.get();
    EXPECT_GT(finalMap.size(), 0);
}

}  // namespace atom::async::sync::test
