#ifndef ATOM_SEARCH_TEST_LRU_HPP
#define ATOM_SEARCH_TEST_LRU_HPP

#include "atom/search/lru.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace atom::search;

class ThreadSafeLRUCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<ThreadSafeLRUCache<std::string, int>>(3);
    }

    void TearDown() override { cache.reset(); }

    std::unique_ptr<ThreadSafeLRUCache<std::string, int>> cache;
};

TEST_F(ThreadSafeLRUCacheTest, PutAndGet) {
    cache->put("key1", 1);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
}

TEST_F(ThreadSafeLRUCacheTest, GetNonExistentKey) {
    auto value = cache->get("key1");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ThreadSafeLRUCacheTest, PutUpdatesValue) {
    cache->put("key1", 1);
    cache->put("key1", 2);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 2);
}

TEST_F(ThreadSafeLRUCacheTest, Erase) {
    cache->put("key1", 1);
    cache->erase("key1");
    auto value = cache->get("key1");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ThreadSafeLRUCacheTest, Clear) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(ThreadSafeLRUCacheTest, Keys) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    auto keys = cache->keys();
    EXPECT_EQ(keys.size(), 2);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "key1"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "key2"), keys.end());
}

TEST_F(ThreadSafeLRUCacheTest, PopLru) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    auto lru = cache->popLru();
    ASSERT_TRUE(lru.has_value());
    EXPECT_EQ(lru->first, "key1");
    EXPECT_EQ(lru->second, 1);
}

TEST_F(ThreadSafeLRUCacheTest, Resize) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    cache->resize(2);
    EXPECT_EQ(cache->size(), 2);
    EXPECT_FALSE(cache->get("key1").has_value());
}

TEST_F(ThreadSafeLRUCacheTest, LoadFactor) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    EXPECT_FLOAT_EQ(cache->loadFactor(), 2.0 / 3.0);
}

TEST_F(ThreadSafeLRUCacheTest, HitRate) {
    cache->put("key1", 1);
    cache->get("key1");
    cache->get("key2");
    EXPECT_FLOAT_EQ(cache->hitRate(), 0.5);
}

TEST_F(ThreadSafeLRUCacheTest, SaveToFile) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->saveToFile("test_cache.dat");

    auto newCache = std::make_unique<ThreadSafeLRUCache<std::string, int>>(3);
    newCache->loadFromFile("test_cache.dat");
    EXPECT_EQ(newCache->size(), 2);
    EXPECT_EQ(newCache->get("key1").value(), 1);
    EXPECT_EQ(newCache->get("key2").value(), 2);
}

TEST_F(ThreadSafeLRUCacheTest, LoadFromFile) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->saveToFile("test_cache.dat");

    auto newCache = std::make_unique<ThreadSafeLRUCache<std::string, int>>(3);
    newCache->loadFromFile("test_cache.dat");
    EXPECT_EQ(newCache->size(), 2);
    EXPECT_EQ(newCache->get("key1").value(), 1);
    EXPECT_EQ(newCache->get("key2").value(), 2);
}

TEST_F(ThreadSafeLRUCacheTest, Expiry) {
    cache->put("key1", 1, std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_FALSE(cache->get("key1").has_value());
}

TEST_F(ThreadSafeLRUCacheTest, InsertCallback) {
    bool callbackCalled = false;
    cache->setInsertCallback([&callbackCalled](const std::string&, const int&) {
        callbackCalled = true;
    });
    cache->put("key1", 1);
    EXPECT_TRUE(callbackCalled);
}

TEST_F(ThreadSafeLRUCacheTest, EraseCallback) {
    bool callbackCalled = false;
    cache->setEraseCallback(
        [&callbackCalled](const std::string&) { callbackCalled = true; });
    cache->put("key1", 1);
    cache->erase("key1");
    EXPECT_TRUE(callbackCalled);
}

TEST_F(ThreadSafeLRUCacheTest, ClearCallback) {
    bool callbackCalled = false;
    cache->setClearCallback([&callbackCalled]() { callbackCalled = true; });
    cache->put("key1", 1);
    cache->clear();
    EXPECT_TRUE(callbackCalled);
}

#endif  // ATOM_SEARCH_TEST_LRU_HPP
TEST_F(ThreadSafeLRUCacheTest, GetSharedPointer) {
    cache->put("key1", 1);
    auto valuePtr = cache->getShared("key1");
    ASSERT_TRUE(valuePtr != nullptr);
    EXPECT_EQ(*valuePtr, 1);

    // Non-existent key should return nullptr
    auto nullPtr = cache->getShared("nonexistent");
    EXPECT_EQ(nullPtr, nullptr);
}

TEST_F(ThreadSafeLRUCacheTest, BatchOperations) {
    // Test batch put and get
    std::vector<ThreadSafeLRUCache<std::string, int>::KeyValuePair> items = {
        {"key1", 1}, {"key2", 2}, {"key3", 3}};

    cache->putBatch(items);

    std::vector<std::string> keys = {"key1", "key3", "nonexistent"};
    auto results = cache->getBatch(keys);

    ASSERT_EQ(results.size(), 3);
    ASSERT_NE(results[0], nullptr);
    EXPECT_EQ(*results[0], 1);
    ASSERT_NE(results[1], nullptr);
    EXPECT_EQ(*results[1], 3);
    EXPECT_EQ(results[2], nullptr);
}

TEST_F(ThreadSafeLRUCacheTest, PruneExpired) {
    // Add items with short TTL
    cache->put("key1", 1, std::chrono::seconds(1));
    cache->put("key2", 2);  // No TTL

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Should prune one item
    size_t prunedCount = cache->pruneExpired();
    EXPECT_EQ(prunedCount, 1);

    // key1 should be gone, key2 should remain
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_TRUE(cache->get("key2").has_value());
}

TEST_F(ThreadSafeLRUCacheTest, Prefetch) {
    // Test prefetch functionality
    std::vector<std::string> keysToPrefetch = {"key1", "key2", "key3"};

    int loaderCallCount = 0;
    auto loader = [&loaderCallCount](const std::string& key) -> int {
        loaderCallCount++;
        return key == "key1"   ? 100
               : key == "key2" ? 200
               : key == "key3" ? 300
                               : 0;
    };

    size_t prefetchedCount = cache->prefetch(keysToPrefetch, loader);

    EXPECT_EQ(prefetchedCount, 3);
    EXPECT_EQ(loaderCallCount, 3);

    // Verify the items were added
    EXPECT_EQ(cache->get("key1").value_or(-1), 100);
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
    EXPECT_EQ(cache->get("key3").value_or(-1), 300);

    // Second prefetch should not call the loader for existing keys
    loaderCallCount = 0;
    prefetchedCount = cache->prefetch(keysToPrefetch, loader);
    EXPECT_EQ(prefetchedCount, 0);
    EXPECT_EQ(loaderCallCount, 0);
}

TEST_F(ThreadSafeLRUCacheTest, GetStatistics) {
    cache->put("key1", 1);
    cache->get("key1");         // Hit
    cache->get("nonexistent");  // Miss

    auto stats = cache->getStatistics();

    EXPECT_EQ(stats.hitCount, 1);
    EXPECT_EQ(stats.missCount, 1);
    EXPECT_FLOAT_EQ(stats.hitRate, 0.5f);
    EXPECT_EQ(stats.size, 1);
    EXPECT_EQ(stats.maxSize, 3);
    EXPECT_FLOAT_EQ(stats.loadFactor, 1.0f / 3.0f);
}

TEST_F(ThreadSafeLRUCacheTest, TimeToLiveExpiration) {
    // Add item with short TTL
    cache->put("key1", 1,
               std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::milliseconds(50)));

    // Should be available immediately
    EXPECT_TRUE(cache->get("key1").has_value());

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should be gone now
    EXPECT_FALSE(cache->get("key1").has_value());

    // Check that contains also respects expiry
    EXPECT_FALSE(cache->contains("key1"));
}

TEST_F(ThreadSafeLRUCacheTest, ResizeWithValidation) {
    // Test that resize validates input
    EXPECT_THROW(cache->resize(0), std::invalid_argument);

    // Add three items
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Resize to smaller capacity
    cache->resize(1);

    // Only one item should remain (the most recently used)
    EXPECT_EQ(cache->size(), 1);
    EXPECT_TRUE(cache->get("key3").has_value());
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_FALSE(cache->get("key2").has_value());
}

TEST_F(ThreadSafeLRUCacheTest, EmptyOperations) {
    // Test operations on empty cache
    EXPECT_EQ(cache->size(), 0);
    EXPECT_FALSE(cache->contains("any"));
    EXPECT_FALSE(cache->get("any").has_value());
    EXPECT_EQ(cache->getShared("any"), nullptr);

    auto lru = cache->popLru();
    EXPECT_FALSE(lru.has_value());

    std::vector<std::string> emptyVec;
    auto batchResults = cache->getBatch(emptyVec);
    EXPECT_TRUE(batchResults.empty());

    // Pruning empty cache should return 0
    EXPECT_EQ(cache->pruneExpired(), 0);
}

TEST_F(ThreadSafeLRUCacheTest, ConcurrentAccess) {
    constexpr int numThreads = 4;
    constexpr int numOperations = 1000;

    std::atomic<int> successCount(0);
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i, &successCount]() {
            for (int j = 0; j < numOperations; j++) {
                try {
                    std::string key =
                        "key" + std::to_string(i) + "_" + std::to_string(j);
                    cache->put(key, j);
                    auto val = cache->get(key);
                    if (val.has_value() && val.value() == j) {
                        successCount++;
                    }
                } catch (...) {
                    // Ignore exceptions in this test
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // We should have had a significant number of successful operations
    // Note: Not all operations will succeed due to cache size limits
    EXPECT_GT(successCount, numThreads * numOperations / 4);
}

TEST_F(ThreadSafeLRUCacheTest, EdgeCases) {
    // Test with exact capacity
    auto smallCache = std::make_unique<ThreadSafeLRUCache<int, int>>(1);

    smallCache->put(1, 100);
    EXPECT_TRUE(smallCache->contains(1));

    // This should evict key 1
    smallCache->put(2, 200);
    EXPECT_FALSE(smallCache->contains(1));
    EXPECT_TRUE(smallCache->contains(2));

    // Test with negative TTL (should fail gracefully)
    try {
        cache->put("negative", 42, std::chrono::seconds(-10));
        auto val = cache->get("negative");
        // Implementation-dependent whether this succeeds, but shouldn't crash
    } catch (const std::exception&) {
        // Exception is acceptable
    }
}

TEST_F(ThreadSafeLRUCacheTest, CallbackChain) {
    std::string insertRecord, eraseRecord;
    int clearCount = 0;

    cache->setInsertCallback(
        [&insertRecord](const std::string& key, const int& val) {
            insertRecord += key + ":" + std::to_string(val) + ";";
        });

    cache->setEraseCallback(
        [&eraseRecord](const std::string& key) { eraseRecord += key + ";"; });

    cache->setClearCallback([&clearCount]() { clearCount++; });

    // Test callbacks with normal operations
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    EXPECT_EQ(insertRecord, "key1:1;key2:2;key3:3;");

    // This should trigger erase for key1 (LRU item)
    cache->put("key4", 4);
    EXPECT_EQ(eraseRecord, "key1;");

    // Manual erase
    cache->erase("key2");
    EXPECT_EQ(eraseRecord, "key1;key2;");

    // Clear should trigger the clear callback
    cache->clear();
    EXPECT_EQ(clearCount, 1);
}

TEST_F(ThreadSafeLRUCacheTest, FileOperations) {
    const std::string testFile = "test_lru_cache_file_ops.dat";

    // Test saving empty cache
    cache->saveToFile(testFile);

    // Test loading from empty file
    auto newCache = std::make_unique<ThreadSafeLRUCache<std::string, int>>(3);
    newCache->loadFromFile(testFile);
    EXPECT_EQ(newCache->size(), 0);

    // Add data and test save/load with content
    cache->put("key1", 101);
    cache->put("key2", 102);
    cache->saveToFile(testFile);

    newCache->loadFromFile(testFile);
    EXPECT_EQ(newCache->size(), 2);
    EXPECT_EQ(newCache->get("key1").value_or(-1), 101);

    // Test loading with different max size
    auto smallerCache =
        std::make_unique<ThreadSafeLRUCache<std::string, int>>(1);
    smallerCache->loadFromFile(testFile);
    EXPECT_EQ(smallerCache->size(), 1);  // Should only load up to capacity

    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(ThreadSafeLRUCacheTest, ExceptionSafety) {
    // Test invalid constructor
    EXPECT_THROW((ThreadSafeLRUCache<std::string, int>(0)),
                 std::invalid_argument);

    // Test file operations with invalid paths
    EXPECT_THROW(
        cache->saveToFile("/invalid/path/that/should/not/exist/file.dat"),
        LRUCacheIOException);

    EXPECT_THROW(
        cache->loadFromFile("/invalid/path/that/should/not/exist/file.dat"),
        LRUCacheIOException);
}

// Custom class with serialization/deserialization for testing complex types
class TestSerializable {
public:
    int id;
    std::string name;

    TestSerializable() : id(0), name("") {}
    TestSerializable(int i, const std::string& n) : id(i), name(n) {}

    bool operator==(const TestSerializable& other) const {
        return id == other.id && name == other.name;
    }
};

TEST_F(ThreadSafeLRUCacheTest, ComplexValueType) {
    // Test with a more complex value type
    auto complexCache =
        std::make_unique<ThreadSafeLRUCache<int, TestSerializable>>(3);

    complexCache->put(1, TestSerializable(101, "Item 1"));
    complexCache->put(2, TestSerializable(102, "Item 2"));

    auto value = complexCache->get(1);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->id, 101);
    EXPECT_EQ(value->name, "Item 1");

    // Test LRU eviction with complex type
    complexCache->put(3, TestSerializable(103, "Item 3"));
    complexCache->put(4, TestSerializable(104, "Item 4"));

    EXPECT_FALSE(complexCache->get(1).has_value());  // Should be evicted
    EXPECT_TRUE(complexCache->get(2).has_value());
    EXPECT_TRUE(complexCache->get(3).has_value());
    EXPECT_TRUE(complexCache->get(4).has_value());
}

TEST_F(ThreadSafeLRUCacheTest, AccessOrder) {
    // Test that access order is correctly maintained
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Access key1 to move it to front
    cache->get("key1");

    // Add a new key to evict LRU item (should be key2)
    cache->put("key4", 4);

    EXPECT_TRUE(cache->get("key1").has_value());   // Was accessed recently
    EXPECT_FALSE(cache->get("key2").has_value());  // Should be evicted
    EXPECT_TRUE(cache->get("key3").has_value());
    EXPECT_TRUE(cache->get("key4").has_value());
}
