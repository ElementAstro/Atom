// filepath: atom/type/test_robin_hood.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <vector>
#include <string>
#include <random>
#include <future>
#include <algorithm>

#include "robin_hood.hpp"

using namespace atom::utils;

class RobinHoodMapTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize with some key-value pairs for common tests
        for (int i = 0; i < 100; ++i) {
            test_keys.push_back(i);
            test_values.push_back("value-" + std::to_string(i));
        }
    }

    // Helper to fill a map with test data
    template <typename MapType>
    void fill_test_map(MapType& map, size_t count = 100) {
        for (size_t i = 0; i < count && i < test_keys.size(); ++i) {
            map.insert(test_keys[i], test_values[i]);
        }
    }

    std::vector<int> test_keys;
    std::vector<std::string> test_values;
};

// Test basic construction and default values
TEST_F(RobinHoodMapTest, Construction) {
    // Default constructor
    unordered_flat_map<int, std::string> map1;
    EXPECT_TRUE(map1.empty());
    EXPECT_EQ(map1.size(), 0);
    
    // Constructor with threading policy
    unordered_flat_map<int, std::string> map2(
        unordered_flat_map<int, std::string>::threading_policy::mutex);
    EXPECT_TRUE(map2.empty());
    
    // Constructor with allocator
    std::allocator<std::pair<const int, std::string>> alloc;
    unordered_flat_map<int, std::string> map3(alloc);
    EXPECT_TRUE(map3.empty());
    
    // Constructor with bucket count and allocator
    unordered_flat_map<int, std::string> map4(16, alloc);
    EXPECT_TRUE(map4.empty());
    EXPECT_EQ(map4.load_factor(), 0.0f);
}

// Test basic capacity and size operations
TEST_F(RobinHoodMapTest, CapacityAndSize) {
    unordered_flat_map<int, std::string> map;
    
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);
    
    // Insert some elements
    map.insert(1, "one");
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map.size(), 1);
    
    map.insert(2, "two");
    EXPECT_EQ(map.size(), 2);
    
    // Clear the map
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);
}

// Test basic insertion and lookup
TEST_F(RobinHoodMapTest, InsertionAndLookup) {
    unordered_flat_map<int, std::string> map;
    
    // Insert and verify
    auto [it1, inserted1] = map.insert(1, "one");
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(it1->first, 1);
    EXPECT_EQ(it1->second, "one");
    
    // Lookup with at()
    EXPECT_EQ(map.at(1), "one");
    
    // Check exception for non-existent key
    EXPECT_THROW(map.at(99), std::out_of_range);
    
    // Insert multiple elements
    map.insert(2, "two");
    map.insert(3, "three");
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map.at(2), "two");
    EXPECT_EQ(map.at(3), "three");
}

// Test iterators
TEST_F(RobinHoodMapTest, Iterators) {
    unordered_flat_map<int, std::string> map;
    fill_test_map(map, 10);
    
    // Count elements using iterators
    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        ++count;
        // Check that keys are in our test set
        EXPECT_TRUE(std::find(test_keys.begin(), test_keys.end(), it->first) != test_keys.end());
    }
    EXPECT_EQ(count, 10);
    
    // Test const iterators
    const auto& const_map = map;
    count = 0;
    for (auto it = const_map.begin(); it != const_map.end(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 10);
    
    // Test cbegin/cend
    count = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        ++count;
    }
    EXPECT_EQ(count, 10);
}

// Test rehashing and load factor
TEST_F(RobinHoodMapTest, RehashingAndLoadFactor) {
    unordered_flat_map<int, std::string> map;
    
    // Default load factor should be 0.9
    EXPECT_FLOAT_EQ(map.max_load_factor(), 0.9f);
    
    // Change load factor
    map.max_load_factor(0.75f);
    EXPECT_FLOAT_EQ(map.max_load_factor(), 0.75f);
    
    // Insert elements until rehashing occurs
    size_t initial_bucket_count = map.bucket_count();
    if (initial_bucket_count > 0) {
        size_t elements_to_add = static_cast<size_t>(initial_bucket_count * map.max_load_factor()) + 1;
        
        for (size_t i = 0; i < elements_to_add; ++i) {
            map.insert(static_cast<int>(i), "value-" + std::to_string(i));
        }
        
        // Verify that rehashing occurred
        EXPECT_GT(map.bucket_count(), initial_bucket_count);
    }
}

// Test with a large number of elements
TEST_F(RobinHoodMapTest, LargeNumberOfElements) {
    unordered_flat_map<int, std::string> map;
    
    // Insert a large number of elements
    const size_t num_elements = 1000;
    for (size_t i = 0; i < num_elements; ++i) {
        map.insert(static_cast<int>(i), "value-" + std::to_string(i));
    }
    
    EXPECT_EQ(map.size(), num_elements);
    
    // Verify all elements can be found
    for (size_t i = 0; i < num_elements; ++i) {
        EXPECT_EQ(map.at(static_cast<int>(i)), "value-" + std::to_string(i));
    }
}

// Test thread safety with reader locks
TEST_F(RobinHoodMapTest, ThreadSafetyWithReaderLocks) {
    unordered_flat_map<int, std::string> map(
        unordered_flat_map<int, std::string>::threading_policy::reader_lock);
    
    // Fill the map with some test data
    fill_test_map(map, 100);
    
    // Create multiple reader threads
    std::vector<std::thread> threads;
    std::vector<bool> results(10, false);
    
    for (size_t i = 0; i < 10; ++i) {
        threads.emplace_back([&map, i, &results]() {
            try {
                // Each thread tries to access some keys
                for (int j = static_cast<int>(i * 10); j < static_cast<int>((i + 1) * 10); ++j) {
                    std::string expected = "value-" + std::to_string(j);
                    std::string actual = map.at(j);
                    if (actual == expected) {
                        results[i] = true;
                    } else {
                        results[i] = false;
                        break;
                    }
                }
            } catch (const std::exception&) {
                results[i] = false;
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all threads succeeded
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Test thread safety with full mutex
TEST_F(RobinHoodMapTest, ThreadSafetyWithMutex) {
    unordered_flat_map<int, std::string> map(
        unordered_flat_map<int, std::string>::threading_policy::mutex);
    
    // Multiple threads insert different elements
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int elements_per_thread = 100;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&map, i, elements_per_thread]() {
            for (int j = 0; j < elements_per_thread; ++j) {
                int key = i * elements_per_thread + j;
                map.insert(key, "thread-" + std::to_string(i) + "-value-" + std::to_string(j));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify size and all elements
    EXPECT_EQ(map.size(), static_cast<size_t>(num_threads * elements_per_thread));
    
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < elements_per_thread; ++j) {
            int key = i * elements_per_thread + j;
            EXPECT_EQ(map.at(key), "thread-" + std::to_string(i) + "-value-" + std::to_string(j));
        }
    }
}

// Test concurrent reads and writes with reader-writer lock
TEST_F(RobinHoodMapTest, ConcurrentReadsAndWrites) {
    unordered_flat_map<int, std::string> map(
        unordered_flat_map<int, std::string>::threading_policy::reader_lock);
    
    // Fill the map with initial data
    for (int i = 0; i < 100; ++i) {
        map.insert(i, "initial-" + std::to_string(i));
    }
    
    // Create reader threads
    std::vector<std::future<bool>> reader_results;
    for (int i = 0; i < 5; ++i) {
        reader_results.push_back(std::async(std::launch::async, [&map]() {
            // Each reader thread reads all elements multiple times
            for (int iter = 0; iter < 100; ++iter) {
                for (int j = 0; j < 100; ++j) {
                    try {
                        std::string value = map.at(j);
                        if (value.find("initial-") == std::string::npos && 
                            value.find("updated-") == std::string::npos) {
                            return false;
                        }
                    } catch (const std::exception&) {
                        // Keys might be in transition, that's okay
                    }
                }
                std::this_thread::yield(); // Give other threads a chance
            }
            return true;
        }));
    }
    
    // Create writer threads
    std::vector<std::future<bool>> writer_results;
    for (int i = 0; i < 3; ++i) {
        writer_results.push_back(std::async(std::launch::async, [&map, i]() {
            // Each writer thread updates a subset of elements
            for (int j = i * 30; j < (i + 1) * 30 && j < 100; ++j) {
                try {
                    map.insert(j, "updated-" + std::to_string(i) + "-" + std::to_string(j));
                } catch (const std::exception&) {
                    return false;
                }
                std::this_thread::yield(); // Give other threads a chance
            }
            return true;
        }));
    }
    
    // Check results from all threads
    for (auto& result : reader_results) {
        EXPECT_TRUE(result.get());
    }
    
    for (auto& result : writer_results) {
        EXPECT_TRUE(result.get());
    }
}

// Test with custom hash and key equality
class CustomHash {
public:
    size_t operator()(const std::string& key) const {
        // Simple hash function for testing
        size_t hash = 0;
        for (char c : key) {
            hash = hash * 31 + c;
        }
        return hash;
    }
};

class CustomKeyEqual {
public:
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        // Case-insensitive comparison
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                          [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    }
};

TEST_F(RobinHoodMapTest, CustomHashAndKeyEqual) {
    unordered_flat_map<std::string, int, CustomHash, CustomKeyEqual> map;
    
    // Insert with lowercase keys
    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);
    
    // Lookup with mixed case should work with our custom comparator
    EXPECT_EQ(map.at("ONE"), 1);
    EXPECT_EQ(map.at("Two"), 2);
    EXPECT_EQ(map.at("tHrEe"), 3);
    
    // Size should still be accurate
    EXPECT_EQ(map.size(), 3);
}

// Test with move-only types
class MoveOnlyValue {
public:
    explicit MoveOnlyValue(int val) : value(val) {}
    
    MoveOnlyValue(const MoveOnlyValue&) = delete;
    MoveOnlyValue& operator=(const MoveOnlyValue&) = delete;
    
    MoveOnlyValue(MoveOnlyValue&& other) noexcept : value(other.value) {
        other.value = -1;
    }
    
    MoveOnlyValue& operator=(MoveOnlyValue&& other) noexcept {
        if (this != &other) {
            value = other.value;
            other.value = -1;
        }
        return *this;
    }
    
    int get_value() const { return value; }
    
private:
    int value;
};

TEST_F(RobinHoodMapTest, MoveOnlyTypes) {
    unordered_flat_map<int, MoveOnlyValue> map;
    
    // Insert with rvalue
    map.insert(1, MoveOnlyValue(100));
    map.insert(2, MoveOnlyValue(200));
    
    // Check values are correctly moved
    EXPECT_EQ(map.at(1).get_value(), 100);
    EXPECT_EQ(map.at(2).get_value(), 200);
}

// Test exception safety
TEST_F(RobinHoodMapTest, ExceptionSafety) {
    unordered_flat_map<int, std::string> map;
    
    // Insert some elements
    fill_test_map(map, 10);
    
    // Test exceptions from at() method
    EXPECT_THROW(map.at(999), std::out_of_range);
    EXPECT_EQ(map.size(), 10); // Size should be unchanged after exception
    
    // The const version
    const auto& const_map = map;
    EXPECT_THROW(const_map.at(999), std::out_of_range);
    EXPECT_EQ(map.size(), 10);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}