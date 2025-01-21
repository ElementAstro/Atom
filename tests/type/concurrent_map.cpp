#include "atom/type/concurrent_map.hpp"
#include <gtest/gtest.h>
#include <string>
#include <thread>

using namespace atom::type;

class ConcurrentMapTest : public ::testing::Test {
protected:
    void SetUp() override {
        map = std::make_unique<concurrent_map<std::string, int>>(4, 10);
    }

    void TearDown() override { map.reset(); }

    std::unique_ptr<concurrent_map<std::string, int>> map;
};

// Basic Operations Tests
TEST_F(ConcurrentMapTest, InsertAndFind) {
    map->insert("key1", 100);
    auto value = map->find("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 100);
}

TEST_F(ConcurrentMapTest, FindNonExistent) {
    auto value = map->find("nonexistent");
    ASSERT_FALSE(value.has_value());
}

TEST_F(ConcurrentMapTest, FindOrInsert) {
    map->find_or_insert("key1", 100);
    auto value = map->find("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 100);
}

// Batch Operations Tests
TEST_F(ConcurrentMapTest, BatchFind) {
    map->insert("key1", 100);
    map->insert("key2", 200);

    std::vector<std::string> keys = {"key1", "key2", "key3"};
    auto results = map->batch_find(keys);

    ASSERT_EQ(results.size(), 3);
    ASSERT_TRUE(results[0].has_value());
    ASSERT_TRUE(results[1].has_value());
    ASSERT_FALSE(results[2].has_value());
    EXPECT_EQ(results[0].value(), 100);
    EXPECT_EQ(results[1].value(), 200);
}

TEST_F(ConcurrentMapTest, BatchUpdate) {
    std::vector<std::pair<std::string, int>> updates = {{"key1", 100},
                                                        {"key2", 200}};
    map->batch_update(updates);

    auto value1 = map->find("key1");
    auto value2 = map->find("key2");

    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value1.value(), 100);
    EXPECT_EQ(value2.value(), 200);
}

TEST_F(ConcurrentMapTest, BatchErase) {
    map->insert("key1", 100);
    map->insert("key2", 200);

    std::vector<std::string> keys_to_erase = {"key1", "key2"};
    map->batch_erase(keys_to_erase);

    EXPECT_FALSE(map->find("key1").has_value());
    EXPECT_FALSE(map->find("key2").has_value());
}

// Range Query Tests
TEST_F(ConcurrentMapTest, RangeQuery) {
    map->insert("a", 1);
    map->insert("b", 2);
    map->insert("c", 3);

    auto results = map->range_query("a", "b");
    ASSERT_EQ(results.size(), 2);
}

// Thread Pool Tests
TEST_F(ConcurrentMapTest, AdjustThreadPoolSize) {
    map->adjust_thread_pool_size(8);
    map->insert("key1", 100);
    auto value = map->find("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 100);
}

// Concurrency Tests
TEST_F(ConcurrentMapTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key =
                    "key" + std::to_string(i) + "_" + std::to_string(j);
                map->insert(key, j);
                auto value = map->find(key);
                EXPECT_TRUE(value.has_value());
                EXPECT_EQ(value.value(), j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(ConcurrentMapTest, MergeTest) {
    concurrent_map<std::string, int> other_map(4, 10);
    other_map.insert("key1", 100);
    other_map.insert("key2", 200);

    map->merge(other_map);

    auto value1 = map->find("key1");
    auto value2 = map->find("key2");

    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value1.value(), 100);
    EXPECT_EQ(value2.value(), 200);
}

TEST_F(ConcurrentMapTest, ClearTest) {
    map->insert("key1", 100);
    map->insert("key2", 200);

    map->clear();

    EXPECT_FALSE(map->find("key1").has_value());
    EXPECT_FALSE(map->find("key2").has_value());
}

TEST_F(ConcurrentMapTest, StressTest) {
    const int num_operations = 10000;

    std::thread writer([&]() {
        for (int i = 0; i < num_operations; ++i) {
            map->insert("key" + std::to_string(i), i);
        }
    });

    std::thread reader([&]() {
        for (int i = 0; i < num_operations; ++i) {
            map->find("key" + std::to_string(i));
        }
    });

    writer.join();
    reader.join();
}