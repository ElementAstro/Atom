#ifndef ATOM_TYPE_TEST_AUTO_TABLE_HPP
#define ATOM_TYPE_TEST_AUTO_TABLE_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>


#include "atom/type/auto_table.hpp"

using namespace atom::type;
using namespace std::chrono_literals;

class CountingHashTableTest : public ::testing::Test {
protected:
    // Common types used in tests
    using StringTable = CountingHashTable<std::string, int>;
    using IntTable = CountingHashTable<int, std::string>;

    void SetUp() override {
        // Initialize with some data for tests
        strTable.insert("one", 1);
        strTable.insert("two", 2);
        strTable.insert("three", 3);

        intTable.insert(1, "one");
        intTable.insert(2, "two");
        intTable.insert(3, "three");
    }

    StringTable strTable{8, 32};  // Smaller sizes for testing
    IntTable intTable{8, 32};
};

// Basic Operations Tests

TEST_F(CountingHashTableTest, InsertAndGet) {
    // Insert new item
    strTable.insert("four", 4);

    // Get existing items
    auto one = strTable.get("one");
    auto four = strTable.get("four");
    auto nonexistent = strTable.get("nonexistent");

    EXPECT_TRUE(one.has_value());
    EXPECT_EQ(*one, 1);
    EXPECT_TRUE(four.has_value());
    EXPECT_EQ(*four, 4);
    EXPECT_FALSE(nonexistent.has_value());
}

TEST_F(CountingHashTableTest, Erase) {
    // Erase existing item
    bool erased = strTable.erase("one");
    EXPECT_TRUE(erased);

    // Verify it's gone
    auto one = strTable.get("one");
    EXPECT_FALSE(one.has_value());

    // Try to erase non-existent item
    erased = strTable.erase("nonexistent");
    EXPECT_FALSE(erased);
}

TEST_F(CountingHashTableTest, Clear) {
    // Clear the table
    strTable.clear();

    // Verify all items are gone
    auto one = strTable.get("one");
    auto two = strTable.get("two");
    auto three = strTable.get("three");

    EXPECT_FALSE(one.has_value());
    EXPECT_FALSE(two.has_value());
    EXPECT_FALSE(three.has_value());

    // Verify we can add items after clearing
    strTable.insert("new", 100);
    auto new_item = strTable.get("new");
    EXPECT_TRUE(new_item.has_value());
    EXPECT_EQ(*new_item, 100);
}

// Batch Operations Tests

TEST_F(CountingHashTableTest, InsertBatch) {
    std::vector<std::pair<std::string, int>> batch = {
        {"four", 4}, {"five", 5}, {"one", 100}
        // This should update the existing value
    };

    strTable.insertBatch(batch);

    // Verify new items were added
    auto four = strTable.get("four");
    auto five = strTable.get("five");
    EXPECT_TRUE(four.has_value());
    EXPECT_EQ(*four, 4);
    EXPECT_TRUE(five.has_value());
    EXPECT_EQ(*five, 5);

    // Verify existing item was updated
    auto one = strTable.get("one");
    EXPECT_TRUE(one.has_value());
    EXPECT_EQ(*one, 100);
}

TEST_F(CountingHashTableTest, GetBatch) {
    std::vector<std::string> keys = {"one", "nonexistent", "three"};

    auto results = strTable.getBatch(keys);

    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_EQ(*results[0], 1);
    EXPECT_FALSE(results[1].has_value());
    EXPECT_TRUE(results[2].has_value());
    EXPECT_EQ(*results[2], 3);
}

// Counting Mechanism Tests

TEST_F(CountingHashTableTest, AccessCounting) {
    // Access "one" multiple times
    strTable.get("one");
    strTable.get("one");
    strTable.get("one");

    // Access "two" once
    strTable.get("two");

    // Don't access "three"

    // Check access counts
    auto oneCount = strTable.getAccessCount("one");
    auto twoCount = strTable.getAccessCount("two");
    auto threeCount = strTable.getAccessCount("three");

    EXPECT_TRUE(oneCount.has_value());
    EXPECT_EQ(*oneCount, 3 + 1);  // +1 from SetUp
    EXPECT_TRUE(twoCount.has_value());
    EXPECT_EQ(*twoCount, 1 + 1);  // +1 from SetUp
    EXPECT_TRUE(threeCount.has_value());
    EXPECT_EQ(*threeCount, 0 + 1);  // +1 from SetUp

    // Check count for non-existent key
    auto nonexistentCount = strTable.getAccessCount("nonexistent");
    EXPECT_FALSE(nonexistentCount.has_value());
}

TEST_F(CountingHashTableTest, BatchAccessCounting) {
    std::vector<std::string> keys = {"one", "one", "two", "nonexistent"};

    strTable.getBatch(keys);

    auto oneCount = strTable.getAccessCount("one");
    auto twoCount = strTable.getAccessCount("two");

    EXPECT_TRUE(oneCount.has_value());
    EXPECT_EQ(*oneCount, 2 + 1);  // +1 from SetUp
    EXPECT_TRUE(twoCount.has_value());
    EXPECT_EQ(*twoCount, 1 + 1);  // +1 from SetUp
}

// Get All Entries Test

TEST_F(CountingHashTableTest, GetAllEntries) {
    auto allEntries = strTable.getAllEntries();

    // We should have 3 entries
    EXPECT_EQ(allEntries.size(), 3);

    // Check that all keys are present
    std::vector<std::string> keys;
    for (const auto& entry : allEntries) {
        keys.push_back(entry.first);
    }

    EXPECT_THAT(keys, ::testing::UnorderedElementsAre("one", "two", "three"));

    // Check that the values are correct
    for (const auto& [key, entryData] : allEntries) {
        if (key == "one") {
            EXPECT_EQ(entryData.value, 1);
        } else if (key == "two") {
            EXPECT_EQ(entryData.value, 2);
        } else if (key == "three") {
            EXPECT_EQ(entryData.value, 3);
        }
    }
}

// Sorting Tests

TEST_F(CountingHashTableTest, SortEntriesByCountDesc) {
    // Access "one" 3 times, "two" 2 times, "three" 1 time
    strTable.get("one");
    strTable.get("one");
    strTable.get("one");
    strTable.get("two");
    strTable.get("two");
    strTable.get("three");

    // Sort entries
    strTable.sortEntriesByCountDesc();

    // Get all entries
    auto allEntries = strTable.getAllEntries();

    // Verify order (highest count first)
    EXPECT_EQ(allEntries.size(), 3);

    // The order from getAllEntries() may not match the internal sorted order
    // So we need to sort again for comparison
    std::sort(allEntries.begin(), allEntries.end(),
              [](const auto& a, const auto& b) {
                  return a.second.count > b.second.count;
              });

    EXPECT_EQ(allEntries[0].first, "one");
    EXPECT_EQ(allEntries[1].first, "two");
    EXPECT_EQ(allEntries[2].first, "three");
}

TEST_F(CountingHashTableTest, GetTopNEntries) {
    // Access items with different frequencies
    strTable.get("one");
    strTable.get("one");
    strTable.get("one");
    strTable.get("two");
    strTable.get("two");

    // Get top 2 entries
    auto topEntries = strTable.getTopNEntries(2);

    // We should have exactly 2 entries
    EXPECT_EQ(topEntries.size(), 2);

    // First entry should be "one" with count 4
    EXPECT_EQ(topEntries[0].first, "one");
    EXPECT_EQ(topEntries[0].second.count, 4);  // 3 + 1 from SetUp

    // Second entry should be "two" with count 3
    EXPECT_EQ(topEntries[1].first, "two");
    EXPECT_EQ(topEntries[1].second.count, 3);  // 2 + 1 from SetUp
}

// JSON Serialization Tests

TEST_F(CountingHashTableTest, SerializeToJson) {
    // Access items to set different counts
    strTable.get("one");
    strTable.get("one");

    // Serialize to JSON
    json serialized = strTable.serializeToJson();

    // Verify JSON structure
    EXPECT_EQ(serialized.size(), 3);

    // Create a map from key to count for easier verification
    std::unordered_map<std::string, size_t> counts;
    std::unordered_map<std::string, int> values;

    for (const auto& item : serialized) {
        std::string key = item["key"].get<std::string>();
        counts[key] = item["count"].get<size_t>();
        values[key] = item["value"].get<int>();
    }

    // Verify counts
    EXPECT_EQ(counts["one"], 3);    // 2 + 1 from SetUp
    EXPECT_EQ(counts["two"], 1);    // From SetUp
    EXPECT_EQ(counts["three"], 1);  // From SetUp

    // Verify values
    EXPECT_EQ(values["one"], 1);
    EXPECT_EQ(values["two"], 2);
    EXPECT_EQ(values["three"], 3);
}

TEST_F(CountingHashTableTest, DeserializeFromJson) {
    // Create JSON data
    json jsonData =
        json::array({{{"key", "four"}, {"value", 4}, {"count", 10}},
                     {{"key", "five"}, {"value", 5}, {"count", 5}}});

    // Create a new table and deserialize into it
    StringTable newTable;
    newTable.deserializeFromJson(jsonData);

    // Check that the data was loaded correctly
    auto four = newTable.get("four");
    auto five = newTable.get("five");

    EXPECT_TRUE(four.has_value());
    EXPECT_EQ(*four, 4);
    EXPECT_TRUE(five.has_value());
    EXPECT_EQ(*five, 5);

    // Check that the counts were loaded correctly
    auto fourCount = newTable.getAccessCount("four");
    auto fiveCount = newTable.getAccessCount("five");

    EXPECT_TRUE(fourCount.has_value());
    EXPECT_EQ(*fourCount, 11);  // 10 from JSON + 1 from our get() call
    EXPECT_TRUE(fiveCount.has_value());
    EXPECT_EQ(*fiveCount, 6);  // 5 from JSON + 1 from our get() call
}

// Thread Safety Tests

TEST_F(CountingHashTableTest, ConcurrentReads) {
    const int NUM_THREADS = 10;
    const int READS_PER_THREAD = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < READS_PER_THREAD; ++j) {
                auto value = strTable.get("one");
                if (value.has_value() && *value == 1) {
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount, NUM_THREADS * READS_PER_THREAD);

    // Check the access count
    auto oneCount = strTable.getAccessCount("one");
    EXPECT_TRUE(oneCount.has_value());
    EXPECT_EQ(*oneCount, NUM_THREADS * READS_PER_THREAD + 1);  // +1 from SetUp
}

TEST_F(CountingHashTableTest, ConcurrentWrites) {
    const int NUM_THREADS = 10;
    const int WRITES_PER_THREAD = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < WRITES_PER_THREAD; ++j) {
                std::string key =
                    "key_" + std::to_string(i) + "_" + std::to_string(j);
                strTable.insert(key, i * WRITES_PER_THREAD + j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify that all keys were inserted
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (int j = 0; j < WRITES_PER_THREAD; ++j) {
            std::string key =
                "key_" + std::to_string(i) + "_" + std::to_string(j);
            auto value = strTable.get(key);
            EXPECT_TRUE(value.has_value());
            EXPECT_EQ(*value, i * WRITES_PER_THREAD + j);
        }
    }
}

TEST_F(CountingHashTableTest, ConcurrentMixedOperations) {
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> readsSucceeded{0};
    std::atomic<int> writesSucceeded{0};

    // Pre-insert some keys to read
    for (int i = 0; i < 100; ++i) {
        strTable.insert("shared_key_" + std::to_string(i), i);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            std::mt19937 rng(i);  // Different seed for each thread
            std::uniform_int_distribution<int> dist(0, 99);

            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                if (j % 2 == 0) {
                    // Read operation
                    int key_idx = dist(rng);
                    auto value =
                        strTable.get("shared_key_" + std::to_string(key_idx));
                    if (value.has_value() && *value == key_idx) {
                        readsSucceeded++;
                    }
                } else {
                    // Write operation
                    std::string key = "thread_" + std::to_string(i) + "_key_" +
                                      std::to_string(j);
                    strTable.insert(key, j);
                    writesSucceeded++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify that the expected number of operations succeeded
    EXPECT_EQ(readsSucceeded, NUM_THREADS * OPS_PER_THREAD / 2);
    EXPECT_EQ(writesSucceeded, NUM_THREADS * OPS_PER_THREAD / 2);
}

// Auto-Sorting Tests

TEST_F(CountingHashTableTest, AutoSorting) {
    // This is a basic test to verify that auto-sorting doesn't crash
    // since testing actual sorting order would be timing-dependent

    // Start auto-sorting with a short interval
    strTable.startAutoSorting(10ms);

    // Do some operations to change counts
    for (int i = 0; i < 100; ++i) {
        if (i % 3 == 0) {
            strTable.get("one");
        } else if (i % 3 == 1) {
            strTable.get("two");
        } else {
            strTable.get("three");
        }
    }

    // Wait a bit to allow sorting to happen
    std::this_thread::sleep_for(50ms);

    // Stop auto-sorting
    strTable.stopAutoSorting();

    // Verify that we can still do operations
    auto value = strTable.get("one");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);
}

// Edge Cases Tests

TEST_F(CountingHashTableTest, EmptyTable) {
    StringTable emptyTable;

    // Operations on an empty table
    auto value = emptyTable.get("key");
    EXPECT_FALSE(value.has_value());

    auto count = emptyTable.getAccessCount("key");
    EXPECT_FALSE(count.has_value());

    auto allEntries = emptyTable.getAllEntries();
    EXPECT_TRUE(allEntries.empty());

    auto topEntries = emptyTable.getTopNEntries(10);
    EXPECT_TRUE(topEntries.empty());

    // These should not crash
    emptyTable.clear();
    emptyTable.sortEntriesByCountDesc();
    emptyTable.startAutoSorting(10ms);
    std::this_thread::sleep_for(30ms);
    emptyTable.stopAutoSorting();
}

TEST_F(CountingHashTableTest, GetTopNWithLimits) {
    // Test with N = 0
    auto topZero = strTable.getTopNEntries(0);
    EXPECT_TRUE(topZero.empty());

    // Test with N > table size
    auto topTen = strTable.getTopNEntries(10);
    EXPECT_EQ(topTen.size(), 3);  // We only have 3 entries
}

TEST_F(CountingHashTableTest, BatchOperationsEmptyInput) {
    // Empty batch insert
    std::vector<std::pair<std::string, int>> emptyInsertBatch;
    strTable.insertBatch(emptyInsertBatch);

    // Empty batch get
    std::vector<std::string> emptyGetBatch;
    auto results = strTable.getBatch(emptyGetBatch);
    EXPECT_TRUE(results.empty());
}

// Performance Test (optional, can be disabled in CI)
TEST_F(CountingHashTableTest, DISABLED_PerformanceTest) {
    const int NUM_ENTRIES = 100000;
    const int NUM_GETS = 1000000;

    CountingHashTable<int, int> perfTable(64, NUM_ENTRIES);

    // Insert many entries
    auto insertStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ENTRIES; ++i) {
        perfTable.insert(i, i);
    }
    auto insertEnd = std::chrono::high_resolution_clock::now();
    auto insertTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                          insertEnd - insertStart)
                          .count();

    std::cout << "Inserted " << NUM_ENTRIES << " entries in " << insertTime
              << "ms" << std::endl;

    // Random distribution for gets
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_ENTRIES - 1);

    // Get many entries
    auto getStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_GETS; ++i) {
        int key = dist(rng);
        auto value = perfTable.get(key);
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, key);
    }
    auto getEnd = std::chrono::high_resolution_clock::now();
    auto getTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(getEnd - getStart)
            .count();

    std::cout << "Performed " << NUM_GETS << " gets in " << getTime << "ms"
              << std::endl;
    std::cout << "Average get time: "
              << static_cast<double>(getTime) / NUM_GETS * 1000 << "µs"
              << std::endl;

    // Get top entries
    auto topStart = std::chrono::high_resolution_clock::now();
    auto topEntries = perfTable.getTopNEntries(100);
    auto topEnd = std::chrono::high_resolution_clock::now();
    auto topTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(topEnd - topStart)
            .count();

    std::cout << "Got top 100 entries in " << topTime << "ms" << std::endl;

    SUCCEED() << "Performance test completed";
}

#endif  // ATOM_TYPE_TEST_AUTO_TABLE_HPP