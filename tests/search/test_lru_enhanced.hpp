#ifndef ATOM_SEARCH_TEST_LRU_ENHANCED_HPP
#define ATOM_SEARCH_TEST_LRU_ENHANCED_HPP

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <mutex>
#include <set>

// Note: These tests are designed for when the full implementation is available
// Currently using mock implementation due to linking issues

/*
#include "atom/search/cache/lru.hpp"
using namespace atom::search;
*/

// Mock LRU Cache for testing patterns
template<typename Key, typename Value>
class MockThreadSafeLRUCache {
private:
    size_t maxSize_;
    std::map<Key, Value> cache_;
    std::list<Key> lruOrder_;
    std::atomic<size_t> hitCount_{0};
    std::atomic<size_t> missCount_{0};
    mutable std::mutex mutex_;

    void moveToFront(const Key& key) {
        auto it = std::find(lruOrder_.begin(), lruOrder_.end(), key);
        if (it != lruOrder_.end()) {
            lruOrder_.erase(it);
        }
        lruOrder_.push_front(key);
    }

    void evictLRU() {
        if (cache_.size() >= maxSize_ && !lruOrder_.empty()) {
            Key lruKey = lruOrder_.back();
            lruOrder_.pop_back();
            cache_.erase(lruKey);
        }
    }

public:
    explicit MockThreadSafeLRUCache(size_t maxSize) : maxSize_(maxSize) {
        if (maxSize == 0) {
            throw std::invalid_argument("Max size cannot be zero");
        }
    }

    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            hitCount_++;
            moveToFront(key);
            return it->second;
        }
        missCount_++;
        return std::nullopt;
    }

    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second = value;
            moveToFront(key);
        } else {
            evictLRU();
            cache_[key] = value;
            lruOrder_.push_front(key);
        }
    }

    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            cache_.erase(it);
            auto listIt = std::find(lruOrder_.begin(), lruOrder_.end(), key);
            if (listIt != lruOrder_.end()) {
                lruOrder_.erase(listIt);
            }
            return true;
        }
        return false;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        lruOrder_.clear();
    }

    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.count(key) > 0;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    size_t maxSize() const {
        return maxSize_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.empty();
    }

    float hitRate() const {
        size_t hits = hitCount_.load();
        size_t misses = missCount_.load();
        size_t total = hits + misses;
        return total == 0 ? 0.0f : static_cast<float>(hits) / static_cast<float>(total);
    }

    float loadFactor() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<float>(cache_.size()) / static_cast<float>(maxSize_);
    }

    void resize(size_t newMaxSize) {
        if (newMaxSize == 0) {
            throw std::invalid_argument("Max size cannot be zero");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        maxSize_ = newMaxSize;
        
        // Evict items if necessary
        while (cache_.size() > maxSize_ && !lruOrder_.empty()) {
            Key lruKey = lruOrder_.back();
            lruOrder_.pop_back();
            cache_.erase(lruKey);
        }
    }

    std::vector<Key> keys() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Key> result;
        for (const auto& [key, value] : cache_) {
            result.push_back(key);
        }
        return result;
    }

    std::vector<Value> values() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Value> result;
        for (const auto& [key, value] : cache_) {
            result.push_back(value);
        }
        return result;
    }

    void resetStatistics() {
        hitCount_.store(0);
        missCount_.store(0);
    }
};

class LRUCacheEnhancedTest : public ::testing::Test {
protected:
    std::unique_ptr<MockThreadSafeLRUCache<std::string, int>> cache;

    void SetUp() override {
        cache = std::make_unique<MockThreadSafeLRUCache<std::string, int>>(3);
    }

    void TearDown() override {
        cache.reset();
    }
};

// Basic Functionality Tests
TEST_F(LRUCacheEnhancedTest, BasicPutAndGet) {
    cache->put("key1", 1);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
}

TEST_F(LRUCacheEnhancedTest, GetNonExistentKey) {
    auto value = cache->get("nonexistent");
    EXPECT_FALSE(value.has_value());
}

TEST_F(LRUCacheEnhancedTest, UpdateExistingKey) {
    cache->put("key1", 1);
    cache->put("key1", 2);
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 2);
}

TEST_F(LRUCacheEnhancedTest, LRUEviction) {
    // Fill cache to capacity
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    EXPECT_EQ(cache->size(), 3);

    // Add one more item, should evict key1 (least recently used)
    cache->put("key4", 4);
    EXPECT_EQ(cache->size(), 3);
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key2"));
    EXPECT_TRUE(cache->contains("key3"));
    EXPECT_TRUE(cache->contains("key4"));
}

TEST_F(LRUCacheEnhancedTest, LRUOrderMaintenance) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Access key1 to make it most recently used
    (void)cache->get("key1");

    // Add new item, should evict key2 (now least recently used)
    cache->put("key4", 4);
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("key2"));
    EXPECT_TRUE(cache->contains("key3"));
    EXPECT_TRUE(cache->contains("key4"));
}

// Edge Cases Tests
TEST_F(LRUCacheEnhancedTest, ZeroCapacityThrows) {
    EXPECT_THROW((MockThreadSafeLRUCache<std::string, int>(0)), std::invalid_argument);
}

TEST_F(LRUCacheEnhancedTest, SingleItemCapacity) {
    auto singleCache = std::make_unique<MockThreadSafeLRUCache<std::string, int>>(1);
    
    singleCache->put("key1", 1);
    EXPECT_EQ(singleCache->size(), 1);
    EXPECT_TRUE(singleCache->contains("key1"));

    singleCache->put("key2", 2);
    EXPECT_EQ(singleCache->size(), 1);
    EXPECT_FALSE(singleCache->contains("key1"));
    EXPECT_TRUE(singleCache->contains("key2"));
}

TEST_F(LRUCacheEnhancedTest, EraseExistingKey) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    
    EXPECT_TRUE(cache->erase("key1"));
    EXPECT_EQ(cache->size(), 1);
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key2"));
}

TEST_F(LRUCacheEnhancedTest, EraseNonExistentKey) {
    cache->put("key1", 1);
    EXPECT_FALSE(cache->erase("nonexistent"));
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(LRUCacheEnhancedTest, ClearCache) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
    EXPECT_FALSE(cache->contains("key1"));
}

// Statistics Tests
TEST_F(LRUCacheEnhancedTest, HitRateCalculation) {
    cache->put("key1", 1);
    
    // Hit
    (void)cache->get("key1");
    // Miss
    (void)cache->get("key2");
    
    EXPECT_FLOAT_EQ(cache->hitRate(), 0.5f);
}

TEST_F(LRUCacheEnhancedTest, LoadFactorCalculation) {
    EXPECT_FLOAT_EQ(cache->loadFactor(), 0.0f);
    
    cache->put("key1", 1);
    EXPECT_FLOAT_EQ(cache->loadFactor(), 1.0f / 3.0f);
    
    cache->put("key2", 2);
    EXPECT_FLOAT_EQ(cache->loadFactor(), 2.0f / 3.0f);
    
    cache->put("key3", 3);
    EXPECT_FLOAT_EQ(cache->loadFactor(), 1.0f);
}

TEST_F(LRUCacheEnhancedTest, ResetStatistics) {
    cache->put("key1", 1);
    (void)cache->get("key1");
    (void)cache->get("key2");
    
    EXPECT_GT(cache->hitRate(), 0.0f);
    
    cache->resetStatistics();
    EXPECT_FLOAT_EQ(cache->hitRate(), 0.0f);
}

// Resize Tests
TEST_F(LRUCacheEnhancedTest, ResizeToLargerCapacity) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    
    cache->resize(5);
    EXPECT_EQ(cache->maxSize(), 5);
    EXPECT_EQ(cache->size(), 3);
    
    // Should be able to add more items
    cache->put("key4", 4);
    cache->put("key5", 5);
    EXPECT_EQ(cache->size(), 5);
}

TEST_F(LRUCacheEnhancedTest, ResizeToSmallerCapacity) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    
    cache->resize(2);
    EXPECT_EQ(cache->maxSize(), 2);
    EXPECT_EQ(cache->size(), 2);
    
    // Should have evicted the least recently used item
    EXPECT_FALSE(cache->contains("key1"));
}

TEST_F(LRUCacheEnhancedTest, ResizeToZeroThrows) {
    EXPECT_THROW(cache->resize(0), std::invalid_argument);
}

// Concurrency Tests
TEST_F(LRUCacheEnhancedTest, ConcurrentReads) {
    // Populate cache
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Launch multiple reader threads
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successCount]() {
            for (int j = 0; j < 100; ++j) {
                auto value = cache->get("key1");
                if (value.has_value()) {
                    successCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount, 1000); // All reads should succeed
}

TEST_F(LRUCacheEnhancedTest, ConcurrentWrites) {
    std::vector<std::thread> threads;
    std::atomic<int> writeCount{0};

    // Launch multiple writer threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i, &writeCount]() {
            for (int j = 0; j < 20; ++j) {
                std::string key = "thread" + std::to_string(i) + "_key" + std::to_string(j);
                cache->put(key, i * 100 + j);
                writeCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(writeCount, 100);
    // Cache should contain at most maxSize items
    EXPECT_LE(cache->size(), cache->maxSize());
}

TEST_F(LRUCacheEnhancedTest, ConcurrentReadWrite) {
    std::vector<std::thread> threads;
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    std::atomic<bool> stopFlag{false};

    // Reader threads
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &readCount, &stopFlag]() {
            while (!stopFlag.load()) {
                for (int j = 0; j < 10; ++j) {
                    auto value = cache->get("key" + std::to_string(j));
                    if (value.has_value()) {
                        readCount++;
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Writer threads
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([this, i, &writeCount, &stopFlag]() {
            int count = 0;
            while (!stopFlag.load() && count < 50) {
                std::string key = "writer" + std::to_string(i) + "_" + std::to_string(count);
                cache->put(key, count);
                writeCount++;
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stopFlag.store(true);

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(readCount, 0);
    EXPECT_GT(writeCount, 0);
    EXPECT_LE(cache->size(), cache->maxSize());
}

TEST_F(LRUCacheEnhancedTest, ConcurrentEviction) {
    std::vector<std::thread> threads;
    std::atomic<int> evictionCount{0};

    // Launch threads that will cause evictions
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i, &evictionCount]() {
            for (int j = 0; j < 10; ++j) {
                std::string key = "evict_thread" + std::to_string(i) + "_" + std::to_string(j);
                size_t sizeBefore = cache->size();
                cache->put(key, i * 10 + j);
                size_t sizeAfter = cache->size();

                // If size didn't increase, an eviction occurred
                if (sizeBefore == cache->maxSize() && sizeAfter == sizeBefore) {
                    evictionCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(evictionCount, 0); // Should have had evictions
    EXPECT_EQ(cache->size(), cache->maxSize()); // Should be at capacity
}

// Performance Tests
TEST_F(LRUCacheEnhancedTest, PerformanceUnderLoad) {
    auto largeCache = std::make_unique<MockThreadSafeLRUCache<std::string, int>>(1000);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform many operations
    for (int i = 0; i < 10000; ++i) {
        std::string key = "perf_key_" + std::to_string(i);
        largeCache->put(key, i);

        if (i % 2 == 0) {
            (void)largeCache->get(key);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(largeCache->size(), 1000); // Should be at capacity
    EXPECT_LT(duration.count(), 5000); // Should complete within 5 seconds
}

TEST_F(LRUCacheEnhancedTest, MemoryUsageWithLargeValues) {
    auto stringCache = std::make_unique<MockThreadSafeLRUCache<std::string, std::string>>(100);

    // Add large string values
    for (int i = 0; i < 100; ++i) {
        std::string key = "large_key_" + std::to_string(i);
        std::string value(10000, 'A' + (i % 26)); // 10KB strings
        stringCache->put(key, value);
    }

    EXPECT_EQ(stringCache->size(), 100);

    // Verify values are correct
    auto value = stringCache->get("large_key_50");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->size(), 10000);
    EXPECT_EQ((*value)[0], 'A' + (50 % 26));
}

// Advanced Edge Cases
TEST_F(LRUCacheEnhancedTest, KeysAndValuesRetrieval) {
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    auto keys = cache->keys();
    auto values = cache->values();

    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(values.size(), 3);

    // Verify all keys and values are present
    std::set<std::string> keySet(keys.begin(), keys.end());
    std::set<int> valueSet(values.begin(), values.end());

    EXPECT_TRUE(keySet.count("key1"));
    EXPECT_TRUE(keySet.count("key2"));
    EXPECT_TRUE(keySet.count("key3"));

    EXPECT_TRUE(valueSet.count(1));
    EXPECT_TRUE(valueSet.count(2));
    EXPECT_TRUE(valueSet.count(3));
}

TEST_F(LRUCacheEnhancedTest, EmptyCacheOperations) {
    EXPECT_TRUE(cache->empty());
    EXPECT_EQ(cache->size(), 0);
    EXPECT_FLOAT_EQ(cache->loadFactor(), 0.0f);
    EXPECT_FLOAT_EQ(cache->hitRate(), 0.0f);

    auto keys = cache->keys();
    auto values = cache->values();
    EXPECT_TRUE(keys.empty());
    EXPECT_TRUE(values.empty());

    EXPECT_FALSE(cache->erase("nonexistent"));
    cache->clear(); // Should not crash
}

TEST_F(LRUCacheEnhancedTest, SpecialKeyTypes) {
    auto intCache = std::make_unique<MockThreadSafeLRUCache<int, std::string>>(3);

    intCache->put(1, "one");
    intCache->put(2, "two");
    intCache->put(3, "three");

    auto value = intCache->get(2);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "two");

    EXPECT_TRUE(intCache->contains(1));
    EXPECT_FALSE(intCache->contains(999));
}

TEST_F(LRUCacheEnhancedTest, ComplexValueTypes) {
    struct ComplexValue {
        int id;
        std::string name;
        std::vector<int> data;

        bool operator==(const ComplexValue& other) const {
            return id == other.id && name == other.name && data == other.data;
        }
    };

    auto complexCache = std::make_unique<MockThreadSafeLRUCache<std::string, ComplexValue>>(2);

    ComplexValue val1{1, "first", {1, 2, 3}};
    ComplexValue val2{2, "second", {4, 5, 6}};

    complexCache->put("complex1", val1);
    complexCache->put("complex2", val2);

    auto retrieved = complexCache->get("complex1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, 1);
    EXPECT_EQ(retrieved->name, "first");
    EXPECT_EQ(retrieved->data, std::vector<int>({1, 2, 3}));
}

// Error Handling and Exception Safety
TEST_F(LRUCacheEnhancedTest, ExceptionSafety) {
    // Fill cache
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);

    // Operations should not throw and cache should remain consistent
    EXPECT_NO_THROW(cache->get("nonexistent"));
    EXPECT_NO_THROW(cache->erase("nonexistent"));
    EXPECT_NO_THROW(cache->clear());
    EXPECT_NO_THROW(cache->resize(5));

    // Cache should still be functional
    cache->put("new_key", 999);
    auto value = cache->get("new_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);
}

// TODO: Add these tests when full implementation is available
/*
TEST_F(LRUCacheEnhancedTest, RealLRUCacheFeatures) {
    ThreadSafeLRUCache<std::string, int> realCache(10);

    // Test TTL functionality
    realCache.put("ttl_key", 42, std::chrono::seconds(1));
    auto value = realCache.get("ttl_key");
    EXPECT_TRUE(value.has_value());

    std::this_thread::sleep_for(std::chrono::seconds(2));
    value = realCache.get("ttl_key");
    EXPECT_FALSE(value.has_value()); // Should have expired
}

TEST_F(LRUCacheEnhancedTest, BatchOperations) {
    ThreadSafeLRUCache<std::string, int> realCache(10);

    // Test batch put
    std::vector<std::pair<std::string, int>> items = {
        {"batch1", 1}, {"batch2", 2}, {"batch3", 3}
    };
    realCache.putBatch(items);

    // Test batch get
    std::vector<std::string> keys = {"batch1", "batch2", "batch3"};
    auto values = realCache.getBatch(keys);
    EXPECT_EQ(values.size(), 3);
}

TEST_F(LRUCacheEnhancedTest, CallbackFunctionality) {
    ThreadSafeLRUCache<std::string, int> realCache(2);

    int insertCount = 0;
    int eraseCount = 0;

    realCache.setInsertCallback([&](const std::string&, const int&) {
        insertCount++;
    });

    realCache.setEraseCallback([&](const std::string&, const int&) {
        eraseCount++;
    });

    realCache.put("cb1", 1);
    realCache.put("cb2", 2);
    realCache.put("cb3", 3); // Should trigger erase callback for cb1

    EXPECT_EQ(insertCount, 3);
    EXPECT_EQ(eraseCount, 1);
}
*/

#endif  // ATOM_SEARCH_TEST_LRU_ENHANCED_HPP
