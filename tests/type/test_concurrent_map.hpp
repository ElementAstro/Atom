// filepath: /home/max/Atom-1/atom/type/test_concurrent_map.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "concurrent_map.hpp"

using namespace atom::type;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class ConcurrentMapTest : public ::testing::Test {
protected:
    using IntMap = concurrent_map<int, std::string>;
    using StringMap = concurrent_map<std::string, int>;

    // Helper method to wait for all threads to finish their work
    void wait_for_threads(concurrent_map<int, std::string>& map,
                          int timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();

        // Attempt to submit a dummy task and wait for it to complete
        while (true) {
            try {
                auto future = map.submit([]() { return true; });
                if (future.wait_for(std::chrono::milliseconds(50)) ==
                    std::future_status::ready) {
                    break;
                }
            } catch (...) {
                // Ignore exceptions
            }

            // Check for timeout
            auto current = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(current -
                                                                      start)
                    .count();
            if (elapsed > timeout_ms) {
                break;
            }

            // Sleep before trying again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

// Test construction with various parameters
TEST_F(ConcurrentMapTest, Construction) {
    // Default construction
    IntMap map1;
    EXPECT_EQ(map1.size(), 0);
    EXPECT_TRUE(map1.empty());
    EXPECT_GT(map1.get_thread_count(), 0);
    EXPECT_FALSE(map1.has_cache());

    // Construction with thread count
    IntMap map2(4);
    EXPECT_EQ(map2.get_thread_count(), 4);
    EXPECT_FALSE(map2.has_cache());

    // Construction with thread count and cache size
    IntMap map3(4, 100);
    EXPECT_EQ(map3.get_thread_count(), 4);
    EXPECT_TRUE(map3.has_cache());

    // Invalid construction (0 threads)
    EXPECT_THROW(IntMap(0), std::invalid_argument);
}

// Test basic insert and find operations
TEST_F(ConcurrentMapTest, InsertAndFind) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    // Find existing values
    auto result1 = map.find(1);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "one");

    auto result2 = map.find(2);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "two");

    auto result3 = map.find(3);
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(*result3, "three");

    // Find non-existent value
    auto result4 = map.find(4);
    EXPECT_FALSE(result4.has_value());

    // Check map size
    EXPECT_EQ(map.size(), 3);
    EXPECT_FALSE(map.empty());
}

// Test insert with move semantics
TEST_F(ConcurrentMapTest, InsertWithMove) {
    IntMap map;

    // Insert a value with move semantics
    std::string value = "movable";
    map.insert(1, std::move(value));

    // Check that value was moved
    EXPECT_TRUE(
        value.empty());  // This is not guaranteed by the standard but common

    // Find the inserted value
    auto result = map.find(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "movable");
}

// Test find_or_insert operation
TEST_F(ConcurrentMapTest, FindOrInsert) {
    IntMap map;

    // Insert a new value
    bool inserted1 = map.find_or_insert(1, "one");
    EXPECT_TRUE(inserted1);

    // Try to insert the same key again
    bool inserted2 = map.find_or_insert(1, "another one");
    EXPECT_FALSE(inserted2);

    // Check that the value wasn't changed
    auto result = map.find(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "one");

    // Insert another new value
    bool inserted3 = map.find_or_insert(2, "two");
    EXPECT_TRUE(inserted3);

    // Check map size
    EXPECT_EQ(map.size(), 2);
}

// Test merge operation
TEST_F(ConcurrentMapTest, Merge) {
    IntMap map1;
    IntMap map2;

    // Insert values into map1
    map1.insert(1, "one");
    map1.insert(2, "two");

    // Insert values into map2
    map2.insert(2, "TWO");  // Duplicate key with different value
    map2.insert(3, "three");

    // Merge map2 into map1
    map1.merge(map2);

    // Check map1 after merge
    EXPECT_EQ(map1.size(), 3);

    auto result1 = map1.find(1);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "one");

    auto result2 = map1.find(2);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "TWO");  // Value from map2 overwrites map1

    auto result3 = map1.find(3);
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(*result3, "three");

    // Check that map2 remains unchanged
    EXPECT_EQ(map2.size(), 2);
}

// Test batch_find operation
TEST_F(ConcurrentMapTest, BatchFind) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(3, "three");
    map.insert(5, "five");

    // Perform batch find
    std::vector<int> keys = {1, 2, 3, 4, 5};
    auto results = map.batch_find(keys);

    // Check results
    EXPECT_EQ(results.size(), 5);

    EXPECT_TRUE(results[0].has_value());
    EXPECT_EQ(*results[0], "one");

    EXPECT_FALSE(results[1].has_value());  // Key 2 doesn't exist

    EXPECT_TRUE(results[2].has_value());
    EXPECT_EQ(*results[2], "three");

    EXPECT_FALSE(results[3].has_value());  // Key 4 doesn't exist

    EXPECT_TRUE(results[4].has_value());
    EXPECT_EQ(*results[4], "five");

    // Test batch_find with empty vector
    auto empty_results = map.batch_find({});
    EXPECT_TRUE(empty_results.empty());
}

// Test batch_update operation
TEST_F(ConcurrentMapTest, BatchUpdate) {
    IntMap map;

    // Create batch update data
    std::vector<std::pair<int, std::string>> updates = {
        {1, "one"}, {2, "two"}, {3, "three"}};

    // Perform batch update
    map.batch_update(updates);

    // Check map after update
    EXPECT_EQ(map.size(), 3);

    auto result1 = map.find(1);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "one");

    auto result2 = map.find(2);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "two");

    auto result3 = map.find(3);
    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(*result3, "three");

    // Test batch_update with empty vector
    map.batch_update({});
    EXPECT_EQ(map.size(), 3);  // Size should remain unchanged

    // Test batch_update with duplicate keys (last value wins)
    std::vector<std::pair<int, std::string>> updates_with_duplicates = {
        {1, "ONE"}, {1, "UPDATED_ONE"}};

    map.batch_update(updates_with_duplicates);

    auto updated_result = map.find(1);
    EXPECT_TRUE(updated_result.has_value());
    EXPECT_EQ(*updated_result, "UPDATED_ONE");
}

// Test batch_erase operation
TEST_F(ConcurrentMapTest, BatchErase) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    map.insert(4, "four");

    // Perform batch erase
    std::vector<int> keys_to_erase = {1, 3, 5};  // 5 doesn't exist
    size_t erased_count = map.batch_erase(keys_to_erase);

    // Check result
    EXPECT_EQ(erased_count, 2);  // Only 2 keys were actually erased
    EXPECT_EQ(map.size(), 2);

    // Check remaining elements
    EXPECT_FALSE(map.find(1).has_value());
    EXPECT_TRUE(map.find(2).has_value());
    EXPECT_FALSE(map.find(3).has_value());
    EXPECT_TRUE(map.find(4).has_value());

    // Test batch_erase with empty vector
    erased_count = map.batch_erase({});
    EXPECT_EQ(erased_count, 0);
    EXPECT_EQ(map.size(), 2);  // Size should remain unchanged
}

// Test range_query operation
TEST_F(ConcurrentMapTest, RangeQuery) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    map.insert(4, "four");
    map.insert(5, "five");

    // Perform range query
    auto results = map.range_query(2, 4);

    // Since we're using an unordered_map, the results can be in any order
    // So we sort them for consistent testing
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Check results
    EXPECT_EQ(results.size(), 3);

    EXPECT_EQ(results[0].first, 2);
    EXPECT_EQ(results[0].second, "two");

    EXPECT_EQ(results[1].first, 3);
    EXPECT_EQ(results[1].second, "three");

    EXPECT_EQ(results[2].first, 4);
    EXPECT_EQ(results[2].second, "four");

    // Test empty range
    auto empty_results = map.range_query(6, 8);
    EXPECT_TRUE(empty_results.empty());

    // Test invalid range (end < start)
    EXPECT_THROW(map.range_query(4, 2), std::invalid_argument);
}

// Test get_data operation
TEST_F(ConcurrentMapTest, GetData) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(2, "two");

    // Get all data
    auto data = map.get_data();

    // Check data
    EXPECT_EQ(data.size(), 2);
    EXPECT_EQ(data[1], "one");
    EXPECT_EQ(data[2], "two");

    // Modify the returned data and check that it doesn't affect the original
    // map
    data[3] = "three";

    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(map.size(), 2);
    EXPECT_FALSE(map.find(3).has_value());
}

// Test clear operation
TEST_F(ConcurrentMapTest, Clear) {
    IntMap map;

    // Insert some values
    map.insert(1, "one");
    map.insert(2, "two");

    // Clear the map
    map.clear();

    // Check map state
    EXPECT_EQ(map.size(), 0);
    EXPECT_TRUE(map.empty());
    EXPECT_FALSE(map.find(1).has_value());
    EXPECT_FALSE(map.find(2).has_value());
}

// Test submit and thread pool functionality
TEST_F(ConcurrentMapTest, SubmitTask) {
    IntMap map(4);  // 4 threads

    // Submit a task that returns a value
    auto future1 = map.submit([]() { return 42; });

    // Wait for and check the result
    EXPECT_EQ(future1.get(), 42);

    // Submit a task with arguments
    auto future2 = map.submit([](int x, int y) { return x + y; }, 10, 20);

    // Wait for and check the result
    EXPECT_EQ(future2.get(), 30);

    // Submit a void task
    auto future3 = map.submit([]() { /* do nothing */ });

    // Wait for completion
    future3.wait();
    EXPECT_TRUE(future3.valid());
}

// Test thread pool adjustment
TEST_F(ConcurrentMapTest, AdjustThreadPoolSize) {
    IntMap map(2);  // Start with 2 threads

    EXPECT_EQ(map.get_thread_count(), 2);

    // Increase thread count
    map.adjust_thread_pool_size(4);
    EXPECT_EQ(map.get_thread_count(), 4);

    // Decrease thread count
    map.adjust_thread_pool_size(1);
    EXPECT_EQ(map.get_thread_count(), 1);

    // Test invalid adjustment (0 threads)
    EXPECT_THROW(map.adjust_thread_pool_size(0), std::invalid_argument);
}

// Test cache functionality
TEST_F(ConcurrentMapTest, CacheFunctionality) {
    // Create map with cache enabled
    IntMap map(4, 100);

    EXPECT_TRUE(map.has_cache());

    // Insert a value
    map.insert(1, "one");

    // Find it multiple times (should be cached)
    map.find(1);
    map.find(1);
    map.find(1);

    // Disable cache
    map.set_cache_size(0);
    EXPECT_FALSE(map.has_cache());

    // Re-enable cache with a different size
    map.set_cache_size(50);
    EXPECT_TRUE(map.has_cache());
}

// Test concurrent insert operations
TEST_F(ConcurrentMapTest, ConcurrentInsert) {
    IntMap map(4);  // 4 threads

    constexpr int num_threads = 10;
    constexpr int ops_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&map, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = t * ops_per_thread + i;
                map.insert(key, std::to_string(key));
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    wait_for_threads(map);

    // Check that all insertions were successful
    EXPECT_EQ(map.size(), num_threads * ops_per_thread);

    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < ops_per_thread; ++i) {
            int key = t * ops_per_thread + i;
            auto result = map.find(key);
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ(*result, std::to_string(key));
        }
    }
}

// Test concurrent find operations
TEST_F(ConcurrentMapTest, ConcurrentFind) {
    IntMap map(4);  // 4 threads

    // Insert some values
    for (int i = 0; i < 1000; ++i) {
        map.insert(i, std::to_string(i));
    }

    constexpr int num_threads = 10;
    constexpr int ops_per_thread = 1000;

    std::atomic<int> success_count(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&map, &success_count]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = i % 1000;  // Will hit existing keys
                auto result = map.find(key);
                if (result.has_value() && *result == std::to_string(key)) {
                    success_count++;
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Check that all finds were successful
    EXPECT_EQ(success_count.load(), num_threads * ops_per_thread);
}

// Test concurrent batch operations
TEST_F(ConcurrentMapTest, ConcurrentBatchOperations) {
    IntMap map(4);  // 4 threads

    // Insert some values
    for (int i = 0; i < 1000; ++i) {
        map.insert(i, std::to_string(i));
    }

    constexpr int num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&map, t, &success_count]() {
            // Each thread does a different operation
            switch (t % 5) {
                case 0: {  // batch_find
                    std::vector<int> keys;
                    for (int i = 0; i < 200; ++i) {
                        keys.push_back(i);
                    }
                    auto results = map.batch_find(keys);
                    for (const auto& result : results) {
                        if (result.has_value())
                            success_count++;
                    }
                    break;
                }
                case 1: {  // batch_update
                    std::vector<std::pair<int, std::string>> updates;
                    for (int i = 0; i < 200; ++i) {
                        updates.emplace_back(i, "updated_" + std::to_string(i));
                    }
                    map.batch_update(updates);
                    success_count += 200;
                    break;
                }
                case 2: {  // batch_erase
                    std::vector<int> keys;
                    for (int i = 0; i < 200; ++i) {
                        keys.push_back(i);
                    }
                    size_t erased = map.batch_erase(keys);
                    success_count += erased;
                    break;
                }
                case 3: {  // range_query
                    auto results = map.range_query(300, 500);
                    success_count += results.size();
                    break;
                }
                case 4: {  // get_data
                    auto data = map.get_data();
                    success_count += data.size();
                    break;
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    wait_for_threads(map);

    // We don't check specific success_count as it depends on the thread
    // interleaving
    EXPECT_GT(success_count.load(), 0);
}

// Test with different map types
TEST_F(ConcurrentMapTest, DifferentMapTypes) {
    // Use a std::map instead of the default std::unordered_map
    concurrent_map<int, std::string, std::map<int, std::string>> ordered_map(2);

    // Basic operations should work the same
    ordered_map.insert(3, "three");
    ordered_map.insert(1, "one");
    ordered_map.insert(2, "two");

    // Check size
    EXPECT_EQ(ordered_map.size(), 3);

    // Check values
    EXPECT_EQ(*ordered_map.find(1), "one");
    EXPECT_EQ(*ordered_map.find(2), "two");
    EXPECT_EQ(*ordered_map.find(3), "three");

    // Range query should return ordered results
    auto range = ordered_map.range_query(1, 3);

    EXPECT_EQ(range.size(), 3);
    EXPECT_EQ(range[0].first, 1);
    EXPECT_EQ(range[1].first, 2);
    EXPECT_EQ(range[2].first, 3);
}

// Test with complex key types
TEST_F(ConcurrentMapTest, ComplexKeyTypes) {
    // Use strings as keys
    StringMap string_map;

    // Insert some values
    string_map.insert("apple", 1);
    string_map.insert("banana", 2);
    string_map.insert("cherry", 3);

    // Check size
    EXPECT_EQ(string_map.size(), 3);

    // Check values
    EXPECT_EQ(*string_map.find("apple"), 1);
    EXPECT_EQ(*string_map.find("banana"), 2);
    EXPECT_EQ(*string_map.find("cherry"), 3);

    // Range query
    auto range = string_map.range_query("apple", "cherry");

    EXPECT_EQ(range.size(), 3);
}

// Test with complex value types
TEST_F(ConcurrentMapTest, ComplexValueTypes) {
    // Use a vector as value type
    concurrent_map<int, std::vector<int>> vector_map;

    // Insert some values
    vector_map.insert(1, std::vector<int>{1, 2, 3});
    vector_map.insert(2, std::vector<int>{4, 5, 6});

    // Check values
    auto result1 = vector_map.find(1);
    EXPECT_TRUE(result1.has_value());
    EXPECT_THAT(*result1, ElementsAre(1, 2, 3));

    auto result2 = vector_map.find(2);
    EXPECT_TRUE(result2.has_value());
    EXPECT_THAT(*result2, ElementsAre(4, 5, 6));

    // Update with move semantics
    std::vector<int> new_value{7, 8, 9};
    vector_map.insert(1, std::move(new_value));

    auto updated_result = vector_map.find(1);
    EXPECT_TRUE(updated_result.has_value());
    EXPECT_THAT(*updated_result, ElementsAre(7, 8, 9));
}

// Test error handling
/*
TODO: Implement error handling tests
TEST_F(ConcurrentMapTest, ErrorHandling) {
    IntMap map;

    // Map operations that should throw exceptions

    // Stop the thread pool (using a hack since there's no direct way)
    *(const_cast<std::atomic<bool>*>(&map.stop_pool)) = true;

    // Submit should now throw
    EXPECT_THROW(map.submit([]() { return 42; }), concurrent_map_error);

    // Reset the stop flag
    *(const_cast<std::atomic<bool>*>(&map.stop_pool)) = false;
}

*/

// Test custom exception class
TEST_F(ConcurrentMapTest, CustomException) {
    concurrent_map_error error("Test error message");

    EXPECT_STREQ(error.what(), "Test error message");
}

// Test extreme cases
TEST_F(ConcurrentMapTest, ExtremeCases) {
    IntMap map;

    // Large batch operations
    std::vector<int> large_batch_keys(10000);
    std::iota(large_batch_keys.begin(), large_batch_keys.end(), 0);

    // This should not crash
    auto results = map.batch_find(large_batch_keys);
    EXPECT_EQ(results.size(), 10000);

    // Large number of updates
    std::vector<std::pair<int, std::string>> large_batch_updates;
    large_batch_updates.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        large_batch_updates.emplace_back(i, std::to_string(i));
    }

    // This should not crash
    map.batch_update(large_batch_updates);
    EXPECT_EQ(map.size(), 10000);

    // Very large thread pool
    EXPECT_NO_THROW(map.adjust_thread_pool_size(32));

    // This should eventually get reduced to a reasonable number
    EXPECT_EQ(map.get_thread_count(), 32);
}

// Performance test
TEST_F(ConcurrentMapTest, DISABLED_PerformanceTest) {
    // This test is marked as DISABLED to avoid running during normal tests
    // Run it manually when needed for performance evaluation

    const int num_operations = 1000000;

    // Sequential insert test
    {
        IntMap map(1);  // Single thread

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_operations; ++i) {
            map.insert(i, std::to_string(i));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Sequential insert: " << duration.count() << " ms"
                  << std::endl;
    }

    // Parallel insert test
    {
        IntMap map(8);  // 8 threads

        auto start = std::chrono::high_resolution_clock::now();

        // Break into chunks
        const int chunk_size = 10000;
        std::vector<std::future<void>> futures;

        for (int chunk_start = 0; chunk_start < num_operations;
             chunk_start += chunk_size) {
            int chunk_end = std::min(chunk_start + chunk_size, num_operations);

            futures.push_back(map.submit([&map, chunk_start, chunk_end]() {
                for (int i = chunk_start; i < chunk_end; ++i) {
                    map.insert(i, std::to_string(i));
                }
            }));
        }

        // Wait for all tasks
        for (auto& f : futures) {
            f.get();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Parallel insert: " << duration.count() << " ms"
                  << std::endl;
    }
}
