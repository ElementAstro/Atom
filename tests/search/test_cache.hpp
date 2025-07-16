#ifndef ATOM_SEARCH_TEST_CACHE_HPP
#define ATOM_SEARCH_TEST_CACHE_HPP

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "cache.hpp"

using namespace atom::search;

class ResourceCacheTest : public ::testing::Test {
protected:
    void SetUp() override { cache = std::make_unique<ResourceCache<int>>(5); }

    void TearDown() override { cache.reset(); }

    std::unique_ptr<ResourceCache<int>> cache;
};

TEST_F(ResourceCacheTest, InsertAndGet) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
}

TEST_F(ResourceCacheTest, Contains) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("key2"));
}

TEST_F(ResourceCacheTest, Remove) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->remove("key1");
    EXPECT_FALSE(cache->contains("key1"));
}

TEST_F(ResourceCacheTest, AsyncGet) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    auto future = cache->asyncGet("key1");
    auto value = future.get();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 1);
}

TEST_F(ResourceCacheTest, AsyncInsert) {
    auto future = cache->asyncInsert("key1", 1, std::chrono::seconds(10));
    future.get();
    EXPECT_TRUE(cache->contains("key1"));
}

TEST_F(ResourceCacheTest, Clear) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->clear();
    EXPECT_FALSE(cache->contains("key1"));
}

TEST_F(ResourceCacheTest, Size) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->insert("key2", 2, std::chrono::seconds(10));
    EXPECT_EQ(cache->size(), 2);
}

TEST_F(ResourceCacheTest, Empty) {
    EXPECT_TRUE(cache->empty());
    cache->insert("key1", 1, std::chrono::seconds(10));
    EXPECT_FALSE(cache->empty());
}

TEST_F(ResourceCacheTest, EvictOldest) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->insert("key2", 2, std::chrono::seconds(10));
    cache->insert("key3", 3, std::chrono::seconds(10));
    cache->insert("key4", 4, std::chrono::seconds(10));
    cache->insert("key5", 5, std::chrono::seconds(10));
    cache->insert("key6", 6, std::chrono::seconds(10));
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key6"));
}

TEST_F(ResourceCacheTest, IsExpired) {
    cache->insert("key1", 1, std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(cache->isExpired("key1"));
}

TEST_F(ResourceCacheTest, AsyncLoad) {
    auto future = cache->asyncLoad("key1", []() { return 1; });
    future.get();
    EXPECT_TRUE(cache->contains("key1"));
}

TEST_F(ResourceCacheTest, SetMaxSize) {
    cache->setMaxSize(2);
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->insert("key2", 2, std::chrono::seconds(10));
    cache->insert("key3", 3, std::chrono::seconds(10));
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key3"));
}

TEST_F(ResourceCacheTest, SetExpirationTime) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->setExpirationTime("key1", std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(cache->isExpired("key1"));
}

TEST_F(ResourceCacheTest, InsertBatch) {
    std::vector<std::pair<std::string, int>> items = {{"key1", 1}, {"key2", 2}};
    cache->insertBatch(items, std::chrono::seconds(10));
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key2"));
}

TEST_F(ResourceCacheTest, RemoveBatch) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->insert("key2", 2, std::chrono::seconds(10));
    std::vector<std::string> keys = {"key1", "key2"};
    cache->removeBatch(keys);
    EXPECT_FALSE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("key2"));
}

TEST_F(ResourceCacheTest, GetStatistics) {
    cache->insert("key1", 1, std::chrono::seconds(10));
    cache->get("key1");
    cache->get("key2");
    auto [hits, misses] = cache->getStatistics();
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(misses, 1);
}

TEST_F(ResourceCacheTest, OnInsertCallback) {
    bool callbackCalled = false;
    String insertedKey;
    cache->onInsert([&](const String &key) {
        callbackCalled = true;
        insertedKey = key;
    });

    cache->insert("key_insert", 10, std::chrono::seconds(10));

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(insertedKey, "key_insert");
}

TEST_F(ResourceCacheTest, OnRemoveCallback) {
    bool callbackCalled = false;
    String removedKey;
    cache->onRemove([&](const String &key) {
        callbackCalled = true;
        removedKey = key;
    });

    cache->insert("key_remove", 20, std::chrono::seconds(10));
    cache->remove("key_remove");

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(removedKey, "key_remove");
}

TEST_F(ResourceCacheTest, ReadWriteToFile) {
    // Need a temporary file path
    String filePath = "test_cache_file.txt";

    // Serializer for int
    auto serializer = [](const int &value) {
        return String(std::to_string(value));
    };
    // Deserializer for int
    auto deserializer = [](const String &valueString) {
        return std::stoi(std::string(valueString.c_str()));
    };

    // Insert some data
    cache->insert("file_key1", 100, std::chrono::seconds(10));
    cache->insert("file_key2", 200, std::chrono::seconds(10));

    // Write to file
    cache->writeToFile(filePath, serializer);

    // Clear cache and read from file
    cache->clear();
    EXPECT_TRUE(cache->empty());

    cache->readFromFile(filePath, deserializer);

    // Verify contents
    EXPECT_TRUE(cache->contains("file_key1"));
    EXPECT_TRUE(cache->contains("file_key2"));
    auto value1 = cache->get("file_key1");
    auto value2 = cache->get("file_key2");
    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value1.value(), 100);
    EXPECT_EQ(value2.value(), 200);

    // Clean up the temporary file
    std::remove(filePath.c_str());
}

TEST_F(ResourceCacheTest, ReadWriteToJsonFile) {
    // Need a temporary file path
    String filePath = "test_cache_file.json";

    // Serializer for int to json
    auto toJson = [](const int &value) { return json(value); };
    // Deserializer for json to int
    auto fromJson = [](const json &j) { return j.get<int>(); };

    // Insert some data
    cache->insert("json_key1", 300, std::chrono::seconds(10));
    cache->insert("json_key2", 400, std::chrono::seconds(10));

    // Write to JSON file
    cache->writeToJsonFile(filePath, toJson);

    // Clear cache and read from JSON file
    cache->clear();
    EXPECT_TRUE(cache->empty());

    cache->readFromJsonFile(filePath, fromJson);

    // Verify contents
    EXPECT_TRUE(cache->contains("json_key1"));
    EXPECT_TRUE(cache->contains("json_key2"));
    auto value1 = cache->get("json_key1");
    auto value2 = cache->get("json_key2");
    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value1.value(), 300);
    EXPECT_EQ(value2.value(), 400);

    // Clean up the temporary file
    std::remove(filePath.c_str());
}

TEST_F(ResourceCacheTest, ExpirationAndCleanup) {
    // Insert an item with a short expiration
    cache->insert("expired_key", 500, std::chrono::seconds(1));

    // Wait for longer than the expiration time and cleanup interval
    // The default cleanup interval is 1 second. Wait for 3 seconds to be safe.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Check if the item is expired and removed by the cleanup thread
    EXPECT_FALSE(cache->contains("expired_key"));
    auto value = cache->get("expired_key");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ResourceCacheTest, LRUEvictionOrder) {
    // Set max size to 3 for easier testing
    cache->setMaxSize(3);

    // Insert 3 items
    cache->insert("lru_key1", 1,
                  std::chrono::seconds(100));  // Oldest initially
    cache->insert("lru_key2", 2, std::chrono::seconds(100));
    cache->insert("lru_key3", 3,
                  std::chrono::seconds(100));  // Newest initially

    EXPECT_EQ(cache->size(), 3);
    EXPECT_TRUE(cache->contains("lru_key1"));
    EXPECT_TRUE(cache->contains("lru_key2"));
    EXPECT_TRUE(cache->contains("lru_key3"));

    // Access lru_key1 - this should move it to the front (most recently used)
    cache->get("lru_key1");

    // Insert a new item - this should evict the current oldest (lru_key2)
    cache->insert("lru_key4", 4, std::chrono::seconds(100));

    EXPECT_EQ(cache->size(), 3);
    EXPECT_TRUE(
        cache->contains("lru_key1"));  // Should still be there (recently used)
    EXPECT_FALSE(cache->contains(
        "lru_key2"));  // Should be evicted (oldest after lru_key1 was accessed)
    EXPECT_TRUE(cache->contains("lru_key3"));  // Should still be there
    EXPECT_TRUE(cache->contains("lru_key4"));  // The new item

    // Access lru_key3 - moves it to front
    cache->get("lru_key3");

    // Insert another new item - should evict the current oldest (lru_key1)
    cache->insert("lru_key5", 5, std::chrono::seconds(100));

    EXPECT_EQ(cache->size(), 3);
    EXPECT_FALSE(cache->contains("lru_key1"));  // Should be evicted
    EXPECT_TRUE(
        cache->contains("lru_key3"));  // Should still be there (recently used)
    EXPECT_TRUE(cache->contains("lru_key4"));  // Should still be there
    EXPECT_TRUE(cache->contains("lru_key5"));  // The new item
}

#endif  // ATOM_SEARCH_TEST_CACHE_HPP
