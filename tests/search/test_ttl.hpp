#ifndef ATOM_SEARCH_TEST_TTL_HPP
#define ATOM_SEARCH_TEST_TTL_HPP

#include "atom/search/ttl.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace atom::search;

class TTLCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<TTLCache<std::string, int>>(
            std::chrono::milliseconds(100), 3);
    }

    void TearDown() override { cache.reset(); }

    std::unique_ptr<TTLCache<std::string, int>> cache;
};

TEST_F(TTLCacheTest, PutAndGet) {
    cache->put("key1", 1);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
}

TEST_F(TTLCacheTest, GetNonExistentKey) {
    auto value = cache->get("key1");
    EXPECT_FALSE(value.has_value());
}

TEST_F(TTLCacheTest, PutUpdatesValue) {
    cache->put("key1", 1);
    cache->put("key1", 2);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 2);
}

TEST_F(TTLCacheTest, Expiry) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto value = cache->get("key1");
    EXPECT_FALSE(value.has_value());
}

TEST_F(TTLCacheTest, Cleanup) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    cache->cleanup();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(TTLCacheTest, HitRate) {
    cache->put("key1", 1);
    cache->get("key1");
    cache->get("key2");
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.5);
}

TEST_F(TTLCacheTest, Size) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    EXPECT_EQ(cache->size(), 2);
}

TEST_F(TTLCacheTest, Clear) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(TTLCacheTest, LRU_Eviction) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    cache->put("key4", 4);  // This should evict "key1"
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_TRUE(cache->get("key4").has_value());
}

#endif  // ATOM_SEARCH_TEST_TTL_HPP
TEST_F(TTLCacheTest, AccessOrderUpdate) {
    // Test that accessing an element moves it to the front of the LRU cache
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Access key1 to move it to front of LRU list
    cache->get("key1");

    // Add new element which should evict the least recently used (key2)
    cache->put("key4", 4);

    EXPECT_TRUE(cache->get("key1").has_value());
    EXPECT_FALSE(cache->get("key2").has_value());
    EXPECT_TRUE(cache->get("key3").has_value());
    EXPECT_TRUE(cache->get("key4").has_value());
}

TEST_F(TTLCacheTest, ConsecutiveUpdates) {
    // Test multiple updates to the same key
    cache->put("key1", 1);
    cache->put("key1", 2);
    cache->put("key1", 3);

    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 3);

    // Verify cache size is still 1
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(TTLCacheTest, CleanupAfterExpiry) {
    // Verify cleanup correctly removes expired items
    cache->put("key1", 1);
    cache->put("key2", 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Both keys should expire
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_FALSE(cache->get("key2").has_value());

    // But they're still in the cache until cleanup runs
    EXPECT_EQ(cache->size(), 2);

    // After cleanup, they should be removed
    cache->cleanup();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(TTLCacheTest, HitRateUpdatesCorrectly) {
    // Test that hit rate calculations are accurate

    // No accesses yet
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.0);

    // All misses
    cache->get("nonexistent1");
    cache->get("nonexistent2");
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.0);

    // Add some hits
    cache->put("key1", 1);
    cache->get("key1");
    cache->get("key1");

    // Should be 2 hits out of 4 accesses
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.5);

    // Add one more hit
    cache->get("key1");
    // Should be 3 hits out of 5 accesses
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.6);
}

TEST_F(TTLCacheTest, MaxCapacityZero) {
    // Test with a zero capacity cache
    auto zeroCache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(100), 0);

    // Shouldn't be able to add any items
    zeroCache->put("key1", 1);
    EXPECT_EQ(zeroCache->size(), 0);
    EXPECT_FALSE(zeroCache->get("key1").has_value());
}

TEST_F(TTLCacheTest, ClearResetsHitRate) {
    // Test that clear() resets hit rate stats
    cache->put("key1", 1);
    cache->get("key1");
    cache->get("nonexistent");

    // Hit rate should be 0.5
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.5);

    // Clear the cache
    cache->clear();

    // Hit rate should reset to 0
    EXPECT_DOUBLE_EQ(cache->hitRate(), 0.0);

    // Add a new item and hit it
    cache->put("newkey", 5);
    cache->get("newkey");

    // Hit rate should now be 1.0
    EXPECT_DOUBLE_EQ(cache->hitRate(), 1.0);
}

TEST_F(TTLCacheTest, PartialExpiry) {
    // Test case where some items expire but others don't
    auto longTTLCache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(500), 3);

    // Add one item with longer TTL
    longTTLCache->put("long", 100);

    // Switch to shorter TTL for subsequent items
    cache->put("short1", 1);
    cache->put("short2", 2);

    // Wait for short TTL items to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Short TTL items should have expired
    EXPECT_FALSE(cache->get("short1").has_value());
    EXPECT_FALSE(cache->get("short2").has_value());

    // But long TTL item should still be there
    auto longValue = longTTLCache->get("long");
    ASSERT_TRUE(longValue.has_value());
    EXPECT_EQ(longValue.value(), 100);
}

TEST_F(TTLCacheTest, ConcurrentAccess) {
    constexpr int numThreads = 4;
    constexpr int opsPerThread = 100;

    // Create a larger cache for concurrent testing
    auto concurrentCache = std::make_unique<TTLCache<int, int>>(
        std::chrono::milliseconds(500), 100);

    std::vector<std::thread> threads;
    std::atomic<int> successful_gets{0};

    // Launch threads that perform concurrent puts and gets
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&concurrentCache, i, &successful_gets]() {
            // Each thread works on its own range of keys
            int base = i * opsPerThread;

            // Insert values
            for (int j = 0; j < opsPerThread; j++) {
                concurrentCache->put(base + j, base + j);
            }

            // Read values back
            for (int j = 0; j < opsPerThread; j++) {
                auto value = concurrentCache->get(base + j);
                if (value.has_value() && value.value() == base + j) {
                    successful_gets++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // We should have gotten back a substantial number of values
    // (some might have been evicted or expired during the test)
    EXPECT_GT(successful_gets, numThreads * opsPerThread / 2);

    // Cache should have items, but not necessarily all due to capacity limits
    EXPECT_GT(concurrentCache->size(), 0);
    EXPECT_LE(concurrentCache->size(), 100);  // max_capacity
}

TEST_F(TTLCacheTest, RefreshOnAccess) {
    // Test that accessing an item refreshes its position in the LRU list
    // but does not extend its expiration time

    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Wait a bit but not enough for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Access key1 to refresh its LRU position
    cache->get("key1");

    // Add a new key, which should evict the least recently used item (key2)
    cache->put("key4", 4);

    EXPECT_TRUE(cache->get("key1").has_value());
    EXPECT_FALSE(cache->get("key2").has_value());
    EXPECT_TRUE(cache->get("key3").has_value());
    EXPECT_TRUE(cache->get("key4").has_value());

    // Wait for original TTL to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Even though key1 was accessed recently, it should still expire
    // based on its original insertion time
    EXPECT_FALSE(cache->get("key1").has_value());
}

TEST_F(TTLCacheTest, StressTest) {
    // Create a larger cache for stress testing
    auto stressCache = std::make_unique<TTLCache<int, int>>(
        std::chrono::milliseconds(200), 50);

    // Insert 100 items (exceeding capacity)
    for (int i = 0; i < 100; i++) {
        stressCache->put(i, i * 10);
    }

    // Should have exactly max_capacity items
    EXPECT_EQ(stressCache->size(), 50);

    // The most recently added items should be present
    for (int i = 99; i >= 50; i--) {
        auto value = stressCache->get(i);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i * 10);
    }

    // The oldest items should have been evicted
    for (int i = 0; i < 50; i++) {
        EXPECT_FALSE(stressCache->get(i).has_value());
    }
}
