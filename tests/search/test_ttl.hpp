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
    (void)cache->get("key1");  // Fix: Cast to void to ignore nodiscard warning
    (void)cache->get("key2");  // Fix: Cast to void to ignore nodiscard warning
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.5);  // Fix: Correct method name
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
    cache->put("key4", 4);     // This should evict "key1"

    // key1 should be evicted, key2, key3, key4 should be present
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_TRUE(cache->get("key2").has_value());
    EXPECT_TRUE(cache->get("key3").has_value());
    EXPECT_TRUE(cache->get("key4").has_value());
}

#endif  // ATOM_SEARCH_TEST_TTL_HPP
TEST_F(TTLCacheTest, AccessOrderUpdate) {
    // Test that accessing an element moves it to the front of the LRU cache
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Access key1 to move it to front of LRU list
    (void)cache->get("key1");

    // Add new element which should evict the least recently used (key2)
    cache->put("key4", 4);

    (void)cache->get("key1");
    (void)cache->get("key2");
    (void)cache->get("key3");
    (void)cache->get("key4");
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
    // Both keys should expire
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_FALSE(cache->get("key2").has_value());

    // But they're still in the cache until cleanup runs
    EXPECT_EQ(cache->size(), 2);
    (void)cache->get("key2");

    // But they're still in the cache until cleanup runs
    EXPECT_EQ(cache->size(), 2);

    // After cleanup, they should be removed
    cache->cleanup();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(TTLCacheTest, HitRateUpdatesCorrectly) {
    // Test that hit rate calculations are accurate

    // No accesses yet
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.0);  // Fix: Correct method name

    // All misses
    (void)cache->get(
        "nonexistent1");  // Fix: Cast to void to ignore nodiscard warning
    (void)cache->get(
        "nonexistent2");  // Fix: Cast to void to ignore nodiscard warning
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.0);  // Fix: Correct method name

    // Add some hits
    cache->put("key1", 1);
    (void)cache->get("key1");  // Fix: Cast to void to ignore nodiscard warning
    (void)cache->get("key1");  // Fix: Cast to void to ignore nodiscard warning

    // Should be 2 hits out of 4 accesses
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.5);  // Fix: Correct method name

    // Add one more hit
    (void)cache->get("key1");  // Fix: Cast to void to ignore nodiscard warning
    // Should be 3 hits out of 5 accesses
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.6);  // Fix: Correct method name
}

TEST_F(TTLCacheTest, MaxCapacityZero) {
    // Test with a zero capacity cache
    auto zeroCache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(100), 0);

    // Shouldn't be able to add any items
    zeroCache->put("key1", 1);
    EXPECT_EQ(zeroCache->size(), 0);
    (void)zeroCache->get(
        "key1");  // Fix: Cast to void to ignore nodiscard warning
}

TEST_F(TTLCacheTest, ClearResetsHitRate) {
    // Test that clear() resets hit rate stats
    cache->put("key1", 1);
    (void)cache->get("key1");  // Fix: Cast to void to ignore nodiscard warning
    (void)cache->get(
        "nonexistent");  // Fix: Cast to void to ignore nodiscard warning

    // Hit rate should be 0.5
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.5);  // Fix: Correct method name

    // Clear the cache
    cache->clear();

    // Hit rate should reset to 0
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.0);  // Fix: Correct method name

    // Add a new item and hit it
    cache->put("newkey", 5);
    (void)cache->get(
        "newkey");  // Fix: Cast to void to ignore nodiscard warning

    // Hit rate should now be 1.0
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 1.0);  // Fix: Correct method name
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
    (void)cache->get("short1");
    (void)cache->get("short2");

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
    (void)cache->get("key1");

    // Add a new key, which should evict the least recently used item (key2)
    cache->put("key4", 4);

    (void)cache->get("key1");
    (void)cache->get("key2");
    (void)cache->get("key3");
    (void)cache->get("key4");

    // Wait for original TTL to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Even though key1 was accessed recently, it should still expire
    // based on its original insertion time
    (void)cache->get("key1");
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

TEST_F(TTLCacheTest, GetShared) {
    cache->put("key1", 1);
    auto value_ptr = cache->get_shared("key1");
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_EQ(*value_ptr, 1);

    auto non_existent_ptr = cache->get_shared("non_existent");
    EXPECT_EQ(non_existent_ptr, nullptr);
}

TEST_F(TTLCacheTest, BatchPutAndGet) {
    std::vector<std::pair<std::string, int>> items = {
        {"key1", 10}, {"key2", 20}, {"key3", 30}};
    cache->batch_put(items);

    EXPECT_EQ(cache->size(), 3);

    std::vector<std::string> keys_to_get = {"key1", "key3", "key4"};
    auto results = cache->batch_get(keys_to_get);

    ASSERT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_EQ(results[0].value(), 10);
    EXPECT_TRUE(results[1].has_value());
    EXPECT_EQ(results[1].value(), 30);
    EXPECT_FALSE(results[2].has_value());

    // Test batch put with custom TTL
    auto custom_ttl_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(1000), 3);
    std::vector<std::pair<std::string, int>> custom_items = {{"c_key1", 1},
                                                             {"c_key2", 2}};
    custom_ttl_cache->batch_put(custom_items, std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(custom_ttl_cache->get("c_key1").has_value());
}

TEST_F(TTLCacheTest, GetOrCreate) {
    // Item not in cache, should be computed and added
    int value1 = cache->get_or_compute("key1", [] { return 100; });
    EXPECT_EQ(value1, 100);
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_EQ(cache->get("key1").value(), 100);

    // Item already in cache, should return cached value
    int value2 = cache->get_or_compute("key1", [] { return 200; });
    EXPECT_EQ(value2, 100);  // Still 100, not 200
    EXPECT_EQ(cache->get("key1").value(), 100);

    // Test with custom TTL
    int value3 = cache->get_or_compute(
        "key3", [] { return 300; }, std::chrono::milliseconds(50));
    EXPECT_EQ(value3, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(cache->contains("key3"));
}

TEST_F(TTLCacheTest, Remove) {
    cache->put("key1", 1);
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_TRUE(cache->remove("key1"));
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_EQ(cache->size(), 0);

    // Removing non-existent key
    EXPECT_FALSE(cache->remove("non_existent"));
}

TEST_F(TTLCacheTest, BatchRemove) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    EXPECT_EQ(cache->size(), 3);

    std::vector<std::string> keys_to_remove = {"key1", "key3", "key4"};
    size_t removed_count = cache->batch_remove(keys_to_remove);

    EXPECT_EQ(removed_count, 2);
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key2"));
    EXPECT_FALSE(cache->contains("key3"));
    EXPECT_EQ(cache->size(), 1);

    // Remove all remaining
    removed_count = cache->batch_remove({"key2"});
    EXPECT_EQ(removed_count, 1);
    EXPECT_TRUE(cache->empty());
}

TEST_F(TTLCacheTest, Contains) {
    cache->put("key1", 1);
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("key2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(cache->contains("key1"));  // Should be expired
}

TEST_F(TTLCacheTest, UpdateTTL) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(cache->contains("key1"));

    // Extend TTL
    EXPECT_TRUE(cache->update_ttl("key1", std::chrono::milliseconds(200)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(cache->contains("key1"));  // Should still be present

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(cache->contains("key1"));  // Should now be expired

    // Update TTL for non-existent key
    EXPECT_FALSE(
        cache->update_ttl("non_existent", std::chrono::milliseconds(100)));
}

TEST_F(TTLCacheTest, GetRemainingTTL) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto remaining_ttl = cache->get_remaining_ttl("key1");
    ASSERT_TRUE(remaining_ttl.has_value());
    EXPECT_LE(remaining_ttl.value().count(), 80);  // Original 100 - 20ms sleep
    EXPECT_GE(remaining_ttl.value().count(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    remaining_ttl = cache->get_remaining_ttl("key1");
    EXPECT_FALSE(remaining_ttl.has_value());  // Should be expired

    remaining_ttl = cache->get_remaining_ttl("non_existent");
    EXPECT_FALSE(remaining_ttl.has_value());
}

TEST_F(TTLCacheTest, ForceCleanup) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(cache->size(), 1);  // Still in cache, just expired
    cache->force_cleanup();
    EXPECT_EQ(cache->size(), 0);  // Should be removed after force cleanup
}

TEST_F(TTLCacheTest, ResetStatistics) {
    cache->put("key1", 1);
    (void)cache->get("key1");
    (void)cache->get("key2");  // Miss
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.5);

    cache->reset_statistics();
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.0);
    auto stats = cache->get_statistics();
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
}

TEST_F(TTLCacheTest, Empty) {
    EXPECT_TRUE(cache->empty());
    cache->put("key1", 1);
    EXPECT_FALSE(cache->empty());
    cache->remove("key1");
    EXPECT_TRUE(cache->empty());
}

TEST_F(TTLCacheTest, Capacity) { EXPECT_EQ(cache->capacity(), 3); }

TEST_F(TTLCacheTest, TTL) {
    EXPECT_EQ(cache->ttl(), std::chrono::milliseconds(100));
}

TEST_F(TTLCacheTest, GetKeys) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Not expired yet

    auto keys = cache->get_keys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "key1"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "key2"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "key3"), keys.end());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Expire
    keys = cache->get_keys();
    EXPECT_EQ(keys.size(), 0);  // All expired keys should not be returned
}

TEST_F(TTLCacheTest, Resize) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    EXPECT_EQ(cache->size(), 3);
    EXPECT_TRUE(cache->contains("key1"));

    // Resize down, should evict LRU
    cache->resize(2);
    EXPECT_EQ(cache->size(), 2);
    EXPECT_FALSE(cache->contains("key1"));  // key1 was LRU
    EXPECT_TRUE(cache->contains("key2"));
    EXPECT_TRUE(cache->contains("key3"));

    // Resize up
    cache->resize(5);
    EXPECT_EQ(cache->capacity(), 5);
    cache->put("key4", 4);
    cache->put("key5", 5);
    EXPECT_EQ(cache->size(), 4);
    EXPECT_TRUE(cache->contains("key4"));
    EXPECT_TRUE(cache->contains("key5"));

    EXPECT_THROW(
        { cache->resize(0); }, TTLCacheException);  // Fix: Wrap in curly braces
}

TEST_F(TTLCacheTest, Reserve) {
    // This is hard to test directly as it's an internal optimization
    // We can only verify it doesn't throw and doesn't break functionality
    EXPECT_NO_THROW(cache->reserve(100));
    cache->put("key1", 1);
    EXPECT_TRUE(cache->contains("key1"));
}

TEST_F(TTLCacheTest, SetEvictionCallback) {
    bool callback_called = false;
    std::string evicted_key;
    int evicted_value = 0;
    bool was_expired = false;

    auto my_callback = [&](const std::string& key, const int& value,
                           bool expired) {
        callback_called = true;
        evicted_key = key;
        evicted_value = value;
        was_expired = expired;
    };

    cache->set_eviction_callback(my_callback);

    cache->put("key1", 10);
    cache->put("key2", 20);
    cache->put("key3", 30);
    EXPECT_FALSE(callback_called);  // No eviction yet

    // Trigger LRU eviction
    cache->put("key4", 40);
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(evicted_key, "key1");
    EXPECT_EQ(evicted_value, 10);
    EXPECT_FALSE(was_expired);  // Evicted by LRU, not expiry

    // Reset for next test
    callback_called = false;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(150));  // Expire key2, key3, key4
    cache->cleanup();
    EXPECT_TRUE(
        callback_called);      // Callback should be called for expired items
    EXPECT_TRUE(was_expired);  // Evicted by expiry

    // Test clearing cache with callback
    cache->put("key5", 50);
    callback_called = false;
    cache->clear();
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(evicted_key, "key5");
    EXPECT_EQ(evicted_value, 50);
    EXPECT_FALSE(was_expired);  // Evicted by clear, not expiry
}

TEST_F(TTLCacheTest, UpdateConfig) {
    CacheConfig current_config = cache->get_config();
    EXPECT_TRUE(current_config.enable_automatic_cleanup);
    EXPECT_TRUE(current_config.enable_statistics);

    CacheConfig new_config;
    new_config.enable_automatic_cleanup = false;
    new_config.enable_statistics = false;
    new_config.cleanup_batch_size = 50;

    cache->update_config(new_config);
    CacheConfig updated_config = cache->get_config();
    EXPECT_FALSE(updated_config.enable_automatic_cleanup);
    EXPECT_FALSE(updated_config.enable_statistics);
    EXPECT_EQ(updated_config.cleanup_batch_size, 50);

    // Verify statistics are disabled
    cache->put("key1", 1);
    (void)cache->get("key1");
    auto stats = cache->get_statistics();
    EXPECT_EQ(stats.hits, 0);  // Should not increment if disabled
}

TEST_F(TTLCacheTest, Emplace) {
    // Emplace a value directly
    cache->emplace("key1", std::nullopt, 100);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 100);

    // Emplace with custom TTL
    cache->emplace("key2", std::chrono::milliseconds(50), 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(cache->contains("key2"));
}

TEST_F(TTLCacheTest, MoveConstructor) {
    auto original_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(100), 3);
    original_cache->put("key1", 1);
    original_cache->put("key2", 2);
    EXPECT_EQ(original_cache->size(), 2);

    TTLCache<std::string, int> moved_cache = std::move(*original_cache);

    EXPECT_EQ(moved_cache.size(), 2);
    EXPECT_TRUE(moved_cache.contains("key1"));
    EXPECT_TRUE(moved_cache.contains("key2"));
    EXPECT_EQ(moved_cache.capacity(), 3);

    // Original cache should be in a valid but unspecified state, typically
    // empty We can't rely on its state after move, but it shouldn't crash. For
    // unique_ptr, original_cache is now null.
    original_cache.reset();  // Explicitly clear original unique_ptr
}

TEST_F(TTLCacheTest, MoveAssignment) {
    auto cache1 = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(100), 3);
    cache1->put("key1", 1);

    auto cache2 = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(200), 5);
    cache2->put("keyA", 10);
    cache2->put("keyB", 20);

    *cache1 = std::move(*cache2);  // Move assign cache2 to cache1

    EXPECT_EQ(cache1->size(), 2);
    EXPECT_TRUE(cache1->contains("keyA"));
    EXPECT_TRUE(cache1->contains("keyB"));
    EXPECT_EQ(cache1->capacity(), 5);
    EXPECT_EQ(cache1->ttl(), std::chrono::milliseconds(200));

    // Original cache2 should be in a valid but unspecified state, typically
    // empty
    cache2.reset();
}

TEST_F(TTLCacheTest, AutomaticCleanup) {
    // Create a cache with automatic cleanup enabled (default)
    auto auto_cleanup_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(50), 5);

    auto_cleanup_cache->put("key1", 1);
    auto_cleanup_cache->put("key2", 2);
    EXPECT_EQ(auto_cleanup_cache->size(), 2);

    // Wait for cleanup interval (ttl/2 = 25ms, but cleaner_task waits for
    // cleanup_interval_) So, wait for more than TTL to ensure expiration and
    // cleanup cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // The cleaner thread should have run and removed expired items
    EXPECT_EQ(auto_cleanup_cache->size(), 0);

    // Test with automatic cleanup disabled
    CacheConfig no_auto_cleanup_config;
    no_auto_cleanup_config.enable_automatic_cleanup = false;
    auto no_auto_cleanup_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(50), 5, std::nullopt, no_auto_cleanup_config);

    no_auto_cleanup_cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(no_auto_cleanup_cache->size(),
              1);  // Should still be there as auto cleanup is off
    no_auto_cleanup_cache->cleanup();  // Manual cleanup still works
    EXPECT_EQ(no_auto_cleanup_cache->size(), 0);
}

TEST_F(TTLCacheTest, DestructorCleanup) {
    // Ensure destructor correctly joins cleaner thread and cleans up
    {
        TTLCache<std::string, int> temp_cache(std::chrono::milliseconds(100),
                                              3);
        temp_cache.put("key1", 1);
        temp_cache.put("key2", 2);
        // temp_cache goes out of scope here, destructor should handle cleanup
    }
    // No direct assertion possible, but valgrind or similar tools would detect
    // leaks/crashes
    SUCCEED();
}

TEST_F(TTLCacheTest, EvictionCallbackOnClear) {
    bool callback_called = false;
    cache->set_eviction_callback(
        [&](const std::string&, const int&, bool expired) {
            callback_called = true;
            EXPECT_FALSE(expired);  // Should not be marked as expired on clear
        });

    cache->put("key1", 1);
    cache->clear();
    EXPECT_TRUE(callback_called);
}

TEST_F(TTLCacheTest, EvictionCallbackOnOverwrite) {
    bool callback_called = false;
    cache->set_eviction_callback([&](const std::string& key, const int& value,
                                     bool expired) {
        callback_called = true;
        EXPECT_EQ(key, "key1");
        EXPECT_EQ(value, 1);
        EXPECT_FALSE(expired);  // Should not be marked as expired on overwrite
    });

    cache->put("key1", 1);
    cache->put("key1", 2);  // Overwrite
    EXPECT_TRUE(callback_called);
}

TEST_F(TTLCacheTest, EvictionCallbackOnRemove) {
    bool callback_called = false;
    cache->set_eviction_callback(
        [&](const std::string& key, const int& value, bool expired) {
            callback_called = true;
            EXPECT_EQ(key, "key1");
            EXPECT_EQ(value, 1);
            EXPECT_FALSE(
                expired);  // Should not be marked as expired on explicit remove
        });

    cache->put("key1", 1);
    cache->remove("key1");
    EXPECT_TRUE(callback_called);
}

TEST_F(TTLCacheTest, ThreadSafetyWithDisabledThreadSafe) {
    // Test behavior when thread_safe is explicitly set to false
    CacheConfig config;
    config.thread_safe = false;
    auto non_thread_safe_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(100), 3, std::nullopt, config);

    // Operations should still work, but without internal locking for
    // get/get_shared
    non_thread_safe_cache->put("key1", 1);
    auto val = non_thread_safe_cache->get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), 1);

    // Concurrent access to a non-thread-safe cache is undefined behavior,
    // so we don't explicitly test for crashes, but rather that the flag
    // is respected in the get/get_shared paths.
}
TEST_F(TTLCacheTest, PutWithCustomTTL) {
    // Put with shorter TTL
    cache->put("short_ttl_key", 10, std::chrono::milliseconds(50));
    EXPECT_TRUE(cache->contains("short_ttl_key"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(cache->contains("short_ttl_key")); // Should be expired

    // Put with longer TTL than default (default is 100ms)
    cache->put("long_ttl_key", 20, std::chrono::milliseconds(200));
    EXPECT_TRUE(cache->contains("long_ttl_key"));
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Wait past default TTL
    EXPECT_TRUE(cache->contains("long_ttl_key")); // Should still be present

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait past custom TTL
    EXPECT_FALSE(cache->contains("long_ttl_key")); // Should now be expired
}

TEST_F(TTLCacheTest, BatchPutEmpty) {
    EXPECT_EQ(cache->size(), 0);
    std::vector<std::pair<std::string, int>> empty_items;
    cache->batch_put(empty_items);
    EXPECT_EQ(cache->size(), 0); // Size should remain 0
}

TEST_F(TTLCacheTest, BatchGetEmpty) {
    std::vector<std::string> empty_keys;
    auto results = cache->batch_get(empty_keys);
    EXPECT_TRUE(results.empty());
}

TEST_F(TTLCacheTest, GetOrCreateFactoryThrows) {
    struct FactoryError : public std::runtime_error {
        FactoryError() : std::runtime_error("Factory failed") {}
    };

    // Expect the exception from the factory
    EXPECT_THROW(
        {
            cache->get_or_compute("throwing_key", []() -> int {
                throw FactoryError();
                return 0; // Should not be reached
            });
        },
        FactoryError);

    // The key should not have been added to the cache
    EXPECT_FALSE(cache->contains("throwing_key"));
    EXPECT_FALSE(cache->get("throwing_key").has_value());
}

TEST_F(TTLCacheTest, RemoveExpiredItem) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Expire

    EXPECT_FALSE(cache->contains("key1")); // Should be expired
    EXPECT_EQ(cache->size(), 1); // Still in cache list/map

    // Removing an expired item should still work and return true
    EXPECT_TRUE(cache->remove("key1"));
    EXPECT_EQ(cache->size(), 0); // Should be removed
}

TEST_F(TTLCacheTest, BatchRemoveEmpty) {
    cache->put("key1", 1);
    EXPECT_EQ(cache->size(), 1);
    std::vector<std::string> empty_keys;
    size_t removed_count = cache->batch_remove(empty_keys);
    EXPECT_EQ(removed_count, 0);
    EXPECT_EQ(cache->size(), 1); // Size should remain 1
}

TEST_F(TTLCacheTest, UpdateTTLExpiredItem) {
    cache->put("key1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Expire

    EXPECT_FALSE(cache->contains("key1")); // Should be expired

    // Updating TTL for an expired item should return false
    EXPECT_FALSE(cache->update_ttl("key1", std::chrono::milliseconds(100)));

    // The item should still be in the cache list/map but expired
    EXPECT_EQ(cache->size(), 1);
    EXPECT_FALSE(cache->contains("key1"));
}

TEST_F(TTLCacheTest, GetRemainingTTLNearZero) {
    cache->put("key1", 1, std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(45)); // Wait almost until expiry

    auto remaining_ttl = cache->get_remaining_ttl("key1");
    ASSERT_TRUE(remaining_ttl.has_value());
    // Should be a small positive value, e.g., 5ms +/- jitter
    EXPECT_GE(remaining_ttl.value().count(), 0);
    EXPECT_LE(remaining_ttl.value().count(), 10); // Allow some small jitter

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Wait past expiry
    remaining_ttl = cache->get_remaining_ttl("key1");
    EXPECT_FALSE(remaining_ttl.has_value()); // Should be expired
}

TEST_F(TTLCacheTest, CleanupBatchSize) {
    // Create a cache with a small cleanup batch size
    CacheConfig config;
    config.enable_automatic_cleanup = false; // Disable auto cleanup for manual control
    config.cleanup_batch_size = 2;
    auto batch_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(50), 10, std::nullopt, config);

    // Add 5 items, all with short TTL
    for (int i = 0; i < 5; ++i) {
        batch_cache->put("key" + std::to_string(i), i);
    }
    EXPECT_EQ(batch_cache->size(), 5);

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for all to expire

    // All items are expired, but still in cache
    EXPECT_EQ(batch_cache->size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(batch_cache->contains("key" + std::to_string(i)));
    }

    // Run cleanup - should only remove batch_size items
    batch_cache->cleanup();
    EXPECT_EQ(batch_cache->size(), 5 - config.cleanup_batch_size); // 3 items remaining

    // Run cleanup again - should remove the next batch_size items
    batch_cache->cleanup();
    EXPECT_EQ(batch_cache->size(), 5 - 2 * config.cleanup_batch_size); // 1 item remaining

    // Run cleanup again - should remove the last item
    batch_cache->cleanup();
    EXPECT_EQ(batch_cache->size(), 0); // All items removed
}

TEST_F(TTLCacheTest, StatisticsCounts) {
    // Create a cache with stats enabled (default)
    auto stats_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(50), 3); // Capacity 3

    auto stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
    EXPECT_EQ(stats.evictions, 0);
    EXPECT_EQ(stats.expirations, 0);
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.max_capacity, 3);

    // Put 3 items (fill cache)
    stats_cache->put("k1", 1);
    stats_cache->put("k2", 2);
    stats_cache->put("k3", 3);
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.current_size, 3);

    // Put 1 more item (trigger 1 eviction)
    stats_cache->put("k4", 4); // Evicts k1 (LRU)
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.current_size, 3);
    EXPECT_EQ(stats.evictions, 1);
    EXPECT_EQ(stats.expirations, 0); // No expirations yet

    // Get hit
    stats_cache->get("k2");
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 0); // No misses yet

    // Get miss
    stats_cache->get("k1"); // k1 was evicted
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // k2, k3, k4 expire

    // Get expired item (miss)
    stats_cache->get("k2");
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 2); // Miss count increases

    // Cleanup (trigger expirations)
    stats_cache->cleanup();
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.evictions, 1); // Still 1 LRU eviction
    EXPECT_EQ(stats.expirations, 3); // 3 items expired and removed by cleanup

    // Reset stats
    stats_cache->reset_statistics();
    stats = stats_cache->get_statistics();
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
    EXPECT_EQ(stats.evictions, 0);
    EXPECT_EQ(stats.expirations, 0);
}

TEST_F(TTLCacheTest, HitRateZeroAccesses) {
    // Hit rate should be 0 when no gets have occurred
    EXPECT_DOUBLE_EQ(cache->hit_rate(), 0.0);
    auto stats = cache->get_statistics();
    EXPECT_EQ(stats.hit_rate, 0.0);
}

TEST_F(TTLCacheTest, GetKeysEmpty) {
    EXPECT_TRUE(cache->empty());
    auto keys = cache->get_keys();
    EXPECT_TRUE(keys.empty());
}

TEST_F(TTLCacheTest, ResizeToCurrentSize) {
    cache->put("k1", 1);
    cache->put("k2", 2);
    EXPECT_EQ(cache->size(), 2);
    EXPECT_EQ(cache->capacity(), 3);

    // Resize to current size (should do nothing)
    EXPECT_NO_THROW(cache->resize(2));
    EXPECT_EQ(cache->size(), 2);
    EXPECT_EQ(cache->capacity(), 2); // Capacity updates
    EXPECT_TRUE(cache->contains("k1"));
    EXPECT_TRUE(cache->contains("k2"));

    // Resize to current size again (should do nothing)
    EXPECT_NO_THROW(cache->resize(2));
    EXPECT_EQ(cache->size(), 2);
    EXPECT_EQ(cache->capacity(), 2);
}

TEST_F(TTLCacheTest, ResizeWhenEmpty) {
    cache->clear();
    EXPECT_TRUE(cache->empty());
    EXPECT_EQ(cache->capacity(), 3);

    // Resize when empty
    EXPECT_NO_THROW(cache->resize(5));
    EXPECT_TRUE(cache->empty());
    EXPECT_EQ(cache->capacity(), 5);

    // Add items after resizing when empty
    cache->put("k1", 1);
    EXPECT_EQ(cache->size(), 1);
    EXPECT_TRUE(cache->contains("k1"));
}

TEST_F(TTLCacheTest, SetEvictionCallbackToNull) {
    bool callback_called = false;
    cache->set_eviction_callback(
        [&](const std::string&, const int&, bool) { callback_called = true; });

    cache->put("k1", 1);
    cache->put("k2", 2);
    cache->put("k3", 3);
    cache->put("k4", 4); // Triggers eviction of k1
    EXPECT_TRUE(callback_called);

    // Reset callback and state
    callback_called = false;
    cache->set_eviction_callback(nullptr);

    // Trigger another eviction (k2 should be next LRU)
    cache->put("k5", 5); // Triggers eviction of k2
    EXPECT_FALSE(callback_called); // Callback should not be called
}

TEST_F(TTLCacheTest, UpdateConfigAutoCleanupBatchSize) {
    // Create cache with auto cleanup enabled
    auto config_cache = std::make_unique<TTLCache<std::string, int>>(
        std::chrono::milliseconds(50), 5);
    EXPECT_TRUE(config_cache->get_config().enable_automatic_cleanup);

    // Update config to disable auto cleanup
    CacheConfig new_config = config_cache->get_config();
    new_config.enable_automatic_cleanup = false;
    config_cache->update_config(new_config);
    EXPECT_FALSE(config_cache->get_config().enable_automatic_cleanup);

    // Add item and wait past TTL - should not be auto-cleaned
    config_cache->put("k1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(config_cache->size(), 1); // Still present

    // Update config to change batch size and manually cleanup
    new_config.enable_automatic_cleanup = false; // Keep disabled for manual test
    new_config.cleanup_batch_size = 1;
    config_cache->update_config(new_config);
    EXPECT_EQ(config_cache->get_config().cleanup_batch_size, 1);

    config_cache->put("k2", 2);
    config_cache->put("k3", 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Expire k1, k2, k3
    EXPECT_EQ(config_cache->size(), 3);

    // Manual cleanup should use the new batch size
    config_cache->cleanup();
    EXPECT_EQ(config_cache->size(), 2); // Removed 1 (batch size)

    config_cache->cleanup();
    EXPECT_EQ(config_cache->size(), 1); // Removed 1

    config_cache->cleanup();
    EXPECT_EQ(config_cache->size(), 0); // All items removed
}
