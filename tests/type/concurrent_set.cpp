#include "atom/type/concurrent_set.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace atom::type;

class ConcurrentSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a set with 4 threads and cache size of 5
        set = std::make_unique<concurrent_set<int>>(4, 5);
    }

    void TearDown() override {
        set.reset();
        // Clean up any test files
        std::remove("test_data.bin");
    }

    std::unique_ptr<concurrent_set<int>> set;
};

// Basic Operations Tests
TEST_F(ConcurrentSetTest, BasicInsertAndFind) {
    set->insert(1);
    auto result = set->find(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(*result);

    result = set->find(2);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConcurrentSetTest, EraseOperation) {
    set->insert(1);
    set->erase(1);
    auto result = set->find(1);
    EXPECT_FALSE(result.has_value());
}

// Concurrent Operations Tests
TEST_F(ConcurrentSetTest, ConcurrentInserts) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([this, i]() { set->insert(i); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(set->size(), 100);
}

TEST_F(ConcurrentSetTest, AsyncOperations) {
    std::promise<bool> findPromise;
    auto findFuture = findPromise.get_future();

    set->async_insert(42);
    set->async_find(42, [&findPromise](std::optional<bool> result) {
        findPromise.set_value(result.has_value() && *result);
    });

    EXPECT_TRUE(findFuture.get());
}

// Batch Operations Tests
TEST_F(ConcurrentSetTest, BatchInsertAndErase) {
    std::vector<int> values = {1, 2, 3, 4, 5};
    set->batch_insert(values);
    EXPECT_EQ(set->size(), 5);

    set->batch_erase({1, 3, 5});
    EXPECT_EQ(set->size(), 2);
}

// LRU Cache Tests
TEST_F(ConcurrentSetTest, CacheHitRate) {
    for (int i = 0; i < 10; ++i) {
        set->insert(i);
    }

    // Access some elements repeatedly
    for (int i = 0; i < 5; ++i) {
        set->find(i);
    }

    double hitRate = set->get_cache_hit_rate();
    EXPECT_GT(hitRate, 0.0);
}

// File I/O Tests
TEST_F(ConcurrentSetTest, SaveAndLoadFile) {
    std::vector<int> values = {1, 2, 3, 4, 5};
    set->batch_insert(values);

    EXPECT_TRUE(set->save_to_file("test_data.bin"));

    auto new_set = std::make_unique<concurrent_set<int>>(4, 5);
    EXPECT_TRUE(new_set->load_from_file("test_data.bin"));
    EXPECT_EQ(new_set->size(), values.size());
}

// Thread Pool Tests
TEST_F(ConcurrentSetTest, ThreadPoolAdjustment) {
    set->adjust_thread_pool_size(8);
    // Verify the set still works after adjustment
    set->insert(1);
    auto result = set->find(1);
    EXPECT_TRUE(result.has_value());
}

// Transaction Tests
TEST_F(ConcurrentSetTest, TransactionSupport) {
    std::vector<std::function<void()>> operations = {
        [this]() { set->insert(1); }, [this]() { set->insert(2); },
        [this]() { set->erase(1); }};

    EXPECT_TRUE(set->transaction(operations));
    EXPECT_EQ(set->size(), 1);
    auto result = set->find(2);
    EXPECT_TRUE(result.has_value());
}

// Conditional Query Tests
TEST_F(ConcurrentSetTest, ConditionalFind) {
    for (int i = 0; i < 10; ++i) {
        set->insert(i);
    }

    auto evenNumbers =
        set->conditional_find([](const int& key) { return key % 2 == 0; });

    EXPECT_EQ(evenNumbers.size(), 5);
}

// Error Callback Tests
TEST_F(ConcurrentSetTest, ErrorCallback) {
    bool errorCalled = false;
    set->set_error_callback(
        [&errorCalled]([[maybe_unused]] const std::string& error) {
            errorCalled = true;
        });

    // Trigger an error condition (e.g., saving to invalid path)
    set->save_to_file("/invalid/path/file.bin");
    EXPECT_TRUE(errorCalled);
}

// Performance Tests
TEST_F(ConcurrentSetTest, PerformanceUnderLoad) {
    const int NUM_OPERATIONS = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = i * NUM_OPERATIONS; j < (i + 1) * NUM_OPERATIONS;
                 ++j) {
                set->insert(j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify performance is within acceptable range (adjust threshold as
    // needed)
    EXPECT_LT(duration.count(), 5000);  // Should complete within 5 seconds
    EXPECT_EQ(set->size(), NUM_OPERATIONS * 4);
}