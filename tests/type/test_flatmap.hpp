// filepath: /home/max/Atom-1/atom/type/test_flatmap.hpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "flatmap.hpp"

using namespace atom::type;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Helper class for generating test data
class TestDataGenerator {
public:
    // Generate a vector of pairs with sequential keys and random values
    template <typename Key, typename Value>
    static std::vector<std::pair<Key, Value>> generateSequentialData(
        size_t count, Key startKey = 0) {
        std::vector<std::pair<Key, Value>> data;
        data.reserve(count);

        std::mt19937 gen(12345);  // Fixed seed for reproducibility
        std::uniform_int_distribution<int> dist(1, 1000);

        for (size_t i = 0; i < count; ++i) {
            Key key = static_cast<Key>(startKey + i);
            Value value = static_cast<Value>(dist(gen));
            data.emplace_back(key, value);
        }

        return data;
    }

    // Generate a vector of pairs with random keys and values
    template <typename Key, typename Value>
    static std::vector<std::pair<Key, Value>> generateRandomData(
        size_t count, Key minKey = 0, Key maxKey = 1000) {
        std::vector<std::pair<Key, Value>> data;
        data.reserve(count);

        std::mt19937 gen(12345);  // Fixed seed for reproducibility
        std::uniform_int_distribution<int> keyDist(minKey, maxKey);
        std::uniform_int_distribution<int> valDist(1, 1000);

        for (size_t i = 0; i < count; ++i) {
            Key key = static_cast<Key>(keyDist(gen));
            Value value = static_cast<Value>(valDist(gen));
            data.emplace_back(key, value);
        }

        return data;
    }

    // Generate a vector of random strings with specified length
    static std::vector<std::string> generateRandomStrings(size_t count,
                                                          size_t length = 10) {
        std::vector<std::string> result;
        result.reserve(count);

        std::mt19937 gen(12345);  // Fixed seed for reproducibility
        std::uniform_int_distribution<int> dist(97, 122);  // ASCII a-z

        for (size_t i = 0; i < count; ++i) {
            std::string str;
            str.reserve(length);
            for (size_t j = 0; j < length; ++j) {
                str.push_back(static_cast<char>(dist(gen)));
            }
            result.push_back(str);
        }

        return result;
    }
};

// Base test fixture for FlatMap tests
class FlatMapTest : public ::testing::Test {
protected:
    const size_t TEST_SIZE_SMALL = 100;
    const size_t TEST_SIZE_MEDIUM = 1000;
    const size_t TEST_SIZE_LARGE = 10000;

    // Test data
    std::vector<std::pair<int, int>> intPairs;
    std::vector<std::pair<std::string, double>> stringDoublePairs;

    void SetUp() override {
        // Generate test data
        intPairs = TestDataGenerator::generateSequentialData<int, int>(
            TEST_SIZE_MEDIUM);

        auto strings =
            TestDataGenerator::generateRandomStrings(TEST_SIZE_MEDIUM);
        stringDoublePairs.reserve(TEST_SIZE_MEDIUM);
        for (size_t i = 0; i < TEST_SIZE_MEDIUM; ++i) {
            stringDoublePairs.emplace_back(strings[i],
                                           static_cast<double>(i) * 1.1);
        }
    }
};

// Test fixture for QuickFlatMap with integer keys and values
class QuickFlatMapIntTest : public FlatMapTest {
protected:
    // Different instance types to test various configurations
    using StandardMap = QuickFlatMap<int, int>;
    using ThreadSafeMap =
        QuickFlatMap<int, int, std::less<>, ThreadSafetyMode::ReadWrite>;
    using SortedVectorMap =
        QuickFlatMap<int, int, std::less<>, ThreadSafetyMode::None, true>;

    StandardMap standardMap;
    ThreadSafeMap threadSafeMap;
    SortedVectorMap sortedVectorMap;

    void SetUp() override {
        FlatMapTest::SetUp();

        // Initialize maps with some data
        for (size_t i = 0; i < 20 && i < intPairs.size(); ++i) {
            standardMap.insert(intPairs[i]);
            threadSafeMap.insert(intPairs[i]);
            sortedVectorMap.insert(intPairs[i]);
        }
    }
};

// Test fixture for QuickFlatMultiMap with integer keys and values
class QuickFlatMultiMapIntTest : public FlatMapTest {
protected:
    // Different instance types to test
    using StandardMultiMap = QuickFlatMultiMap<int, int>;
    using ThreadSafeMultiMap = QuickFlatMultiMap<int, int, std::equal_to<>,
                                                 ThreadSafetyMode::ReadWrite>;
    using SortedVectorMultiMap =
        QuickFlatMultiMap<int, int, std::equal_to<>, ThreadSafetyMode::None,
                          true>;

    StandardMultiMap standardMultiMap;
    ThreadSafeMultiMap threadSafeMultiMap;
    SortedVectorMultiMap sortedVectorMultiMap;

    void SetUp() override {
        FlatMapTest::SetUp();

        // Initialize maps with some data including duplicates
        for (size_t i = 0; i < 20 && i < intPairs.size(); ++i) {
            const auto& pair = intPairs[i];
            standardMultiMap.insert(pair);
            threadSafeMultiMap.insert(pair);
            sortedVectorMultiMap.insert(pair);

            // Add a duplicate for each third key
            if (pair.first % 3 == 0) {
                standardMultiMap.insert({pair.first, pair.second * 10});
                threadSafeMultiMap.insert({pair.first, pair.second * 10});
                sortedVectorMultiMap.insert({pair.first, pair.second * 10});
            }
        }
    }
};

// Test fixture for QuickFlatMap with string keys
class QuickFlatMapStringTest : public FlatMapTest {
protected:
    using StringMap = QuickFlatMap<std::string, double>;
    using ThreadSafeStringMap = QuickFlatMap<std::string, double, std::less<>,
                                             ThreadSafetyMode::ReadWrite>;

    StringMap stringMap;
    ThreadSafeStringMap threadSafeStringMap;

    void SetUp() override {
        FlatMapTest::SetUp();
        // Initialize maps with some data
        for (size_t i = 0; i < 20 && i < stringDoublePairs.size(); ++i) {
            stringMap.insert(stringDoublePairs[i]);
            threadSafeStringMap.insert(stringDoublePairs[i]);
        }
    }
};

// Basic functionality tests for QuickFlatMap with int keys
TEST_F(QuickFlatMapIntTest, BasicOperations) {
    // Create a map with initial capacity
    StandardMap map(100);

    // Test empty state
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);

    // Test insertion
    auto result1 = map.insert({1, 100});
    EXPECT_TRUE(result1.second);  // New element inserted
    EXPECT_EQ(result1.first->first, 1);
    EXPECT_EQ(result1.first->second, 100);

    // Test size after insertion
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map.size(), 1);

    // Test contains
    EXPECT_TRUE(map.contains(1));
    EXPECT_FALSE(map.contains(2));

    // Test duplicate insertion
    auto result2 = map.insert({1, 200});
    EXPECT_FALSE(result2.second);  // Duplicate not inserted
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map[1], 100);  // Original value preserved

    // Test operator[]
    map[2] = 200;
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map[2], 200);

    // Test at() method
    EXPECT_EQ(map.at(1), 100);
    EXPECT_EQ(map.at(2), 200);
    EXPECT_THROW(map.at(3), exceptions::key_not_found_error);

    // Test insertOrAssign
    auto result3 = map.insertOrAssign(1, 150);  // Update existing
    EXPECT_FALSE(result3.second);               // Not newly inserted
    EXPECT_EQ(map[1], 150);

    auto result4 = map.insertOrAssign(3, 300);  // Insert new
    EXPECT_TRUE(result4.second);                // Newly inserted
    EXPECT_EQ(map[3], 300);

    // Test erase
    EXPECT_TRUE(map.erase(1));
    EXPECT_FALSE(map.contains(1));
    EXPECT_FALSE(map.erase(1));  // Already erased

    // Test clear
    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);
}

// Test iterators and range-based operations
TEST_F(QuickFlatMapIntTest, IteratorsAndRangeOperations) {
    StandardMap map;

    // Insert some test data
    std::vector<std::pair<int, int>> testData = {
        {1, 100}, {2, 200}, {3, 300}, {4, 400}, {5, 500}};

    for (const auto& pair : testData) {
        map.insert(pair);
    }

    // Test iterators
    std::vector<std::pair<int, int>> iteratedData;
    for (auto it = map.begin(); it != map.end(); ++it) {
        iteratedData.push_back(*it);
    }

    // In non-sorted mode, order might not match insertion order
    EXPECT_EQ(iteratedData.size(), testData.size());

    // Test range-based for loop
    std::vector<int> keys;
    std::vector<int> values;

    for (const auto& [key, value] : map) {
        keys.push_back(key);
        values.push_back(value);
    }

    EXPECT_EQ(keys.size(), 5);
    EXPECT_EQ(values.size(), 5);

    // Sort keys for comparison since map doesn't guarantee order
    std::sort(keys.begin(), keys.end());
    EXPECT_THAT(keys, ElementsAre(1, 2, 3, 4, 5));

    // Test assign method
    StandardMap newMap;
    newMap.assign(testData.begin(), testData.end());

    EXPECT_EQ(newMap.size(), 5);
    for (const auto& [key, value] : testData) {
        EXPECT_TRUE(newMap.contains(key));
        EXPECT_EQ(newMap[key], value);
    }
}

// Test the behavior of sorted vector map
TEST_F(QuickFlatMapIntTest, SortedVectorMapBehavior) {
    // Clear and re-populate with ordered data
    sortedVectorMap.clear();

    std::vector<std::pair<int, int>> orderedData = {
        {5, 500}, {3, 300}, {1, 100}, {4, 400}, {2, 200}};

    for (const auto& pair : orderedData) {
        sortedVectorMap.insert(pair);
    }

    // Verify that elements are stored in sorted order
    std::vector<int> keys;
    for (const auto& [key, value] : sortedVectorMap) {
        keys.push_back(key);
    }

    // Keys should be sorted
    EXPECT_THAT(keys, ElementsAre(1, 2, 3, 4, 5));

    // Performance test: find in sorted vs unsorted mode
    // This is more of a functionality test than a true benchmark
    StandardMap unsortedMap;
    SortedVectorMap sortedMap;

    // Load both maps with the same data
    for (int i = 0; i < 1000; ++i) {
        unsortedMap.insert({i, i * 10});
        sortedMap.insert({i, i * 10});
    }

    // Test find performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto it = unsortedMap.find(i);
        EXPECT_NE(it, unsortedMap.end());
    }
    auto unsortedTime = std::chrono::high_resolution_clock::now() - start;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto it = sortedMap.find(i);
        EXPECT_NE(it, sortedMap.end());
    }
    auto sortedTime = std::chrono::high_resolution_clock::now() - start;

    // Just log the times, don't compare directly as it depends on
    // implementation details
    std::cout << "Find in unsorted map took: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     unsortedTime)
                     .count()
              << " microseconds" << std::endl;
    std::cout << "Find in sorted map took: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     sortedTime)
                     .count()
              << " microseconds" << std::endl;
}

// Test thread safety of different thread modes
TEST_F(QuickFlatMapIntTest, ThreadSafety) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPERATIONS_PER_THREAD = 1000;

    // Test with thread-safe map
    {
        ThreadSafeMap safeMap;

        std::vector<std::thread> threads;

        // Launch multiple threads to simultaneously read and write
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&safeMap, t]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    int key = t * OPERATIONS_PER_THREAD + i;
                    safeMap[key] = key * 10;  // Write operation

                    // Read operations
                    safeMap.contains(key);
                    safeMap.try_get(key - 1);

                    if (i % 10 == 0) {
                        safeMap.erase(key - 10);  // Occasional erase
                    }
                }
            });
        }

        // Join all threads
        for (auto& thread : threads) {
            thread.join();
        }

        // Verify size is as expected (inserted - erased)
        size_t expectedSize = NUM_THREADS * OPERATIONS_PER_THREAD -
                              (NUM_THREADS * OPERATIONS_PER_THREAD / 10);
        EXPECT_LE(safeMap.size(), expectedSize);
    }

    // For comparison, test with non-thread-safe map
    // This is only to demonstrate functionality, in real code this would cause
    // race conditions
    if (false) {  // Disable actual execution to avoid crashes
        StandardMap unsafeMap;

        std::vector<std::thread> threads;

        // Launch multiple threads to simultaneously modify map
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&unsafeMap, t]() {
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    int key = t * OPERATIONS_PER_THREAD + i;
                    unsafeMap[key] = key * 10;  // Write operation
                }
            });
        }

        // Join all threads
        for (auto& thread : threads) {
            thread.join();
        }
    }
}

// Test atomic operations with read and write locks
TEST_F(QuickFlatMapIntTest, AtomicOperations) {
    ThreadSafeMap map;

    // Insert test data
    for (int i = 0; i < 100; ++i) {
        map[i] = i * 10;
    }

    // Test with_read_lock
    auto sum = map.with_read_lock([](const auto& container) {
        int total = 0;
        for (const auto& [key, value] : container) {
            total += value;
        }
        return total;
    });

    EXPECT_EQ(sum, 49500);  // Sum of 0*10 + 1*10 + ... + 99*10

    // Test with_write_lock
    map.with_write_lock([](auto& container) {
        // Double every value
        for (auto& pair : container) {
            pair.second *= 2;
        }
    });

    // Verify values were doubled
    EXPECT_EQ(map[50], 1000);  // Was 500, now 1000
}

// Test capacity and boundary conditions
TEST_F(QuickFlatMapIntTest, CapacityAndBoundaries) {
    StandardMap map(10);  // Initial capacity of 10

    // Test initial capacity
    EXPECT_GE(map.capacity(), 10);

    // Fill up to capacity and beyond
    for (int i = 0; i < 20; ++i) {
        map[i] = i;
    }

    // Capacity should have grown
    EXPECT_GE(map.capacity(), 20);

    // Test reserve
    map.reserve(100);
    EXPECT_GE(map.capacity(), 100);

    // Test with very large capacity (but not too large to allocate)
    EXPECT_NO_THROW(map.reserve(1000000));

    // Test with excessively large capacity
    EXPECT_THROW(map.reserve(MAX_CONTAINER_SIZE + 1),
                 exceptions::container_full_error);

    // Test insertion that would exceed limits
    StandardMap hugeMap;
    try {
        // This is just a test of the exception mechanism
        // We don't actually want to allocate this much memory
        hugeMap.reserve(MAX_CONTAINER_SIZE / 2);
        EXPECT_THROW(
            hugeMap.assign(TestDataGenerator::generateSequentialData<int, int>(
                               MAX_CONTAINER_SIZE + 1)
                               .begin(),
                           TestDataGenerator::generateSequentialData<int, int>(
                               MAX_CONTAINER_SIZE + 1)
                               .end()),
            exceptions::container_full_error);
    } catch (const std::bad_alloc&) {
        // If we can't allocate enough memory for the test, that's fine
        // Just continue with the test suite
    }
}

// Custom allocator for testing allocation failures
struct AlwaysFailAllocator : public std::allocator<std::pair<const int, int>> {
    using value_type = std::pair<const int, int>;

    template <typename U>
    struct rebind {
        using other = AlwaysFailAllocator;
    };

    AlwaysFailAllocator() = default;

    template <typename U>
    AlwaysFailAllocator(const AlwaysFailAllocator&) noexcept {}

    [[nodiscard]] std::pair<const int, int>* allocate(std::size_t) {
        throw std::bad_alloc();
    }
};

// Test error handling and exceptions
TEST_F(QuickFlatMapIntTest, ErrorHandling) {
    StandardMap map;

    // Test key not found error
    EXPECT_THROW(map.at(1), exceptions::key_not_found_error);

    // This section would test allocation failures, but since our class doesn't
    // support custom allocators directly, we'll just verify exception types
    EXPECT_THROW((QuickFlatMap<int, int>().reserve(MAX_CONTAINER_SIZE + 1)),
                 exceptions::container_full_error);
}

// Test with string keys and complex values
TEST_F(QuickFlatMapStringTest, StringKeyOperations) {
    // Test basic operations with string keys
    StringMap map;

    // Insert some data
    map["first"] = 1.1;
    map["second"] = 2.2;
    map["third"] = 3.3;

    // Test access
    EXPECT_DOUBLE_EQ(map["first"], 1.1);
    EXPECT_DOUBLE_EQ(map["second"], 2.2);
    EXPECT_DOUBLE_EQ(map["third"], 3.3);

    // Test contains
    EXPECT_TRUE(map.contains("first"));
    EXPECT_FALSE(map.contains("fourth"));

    // Test find
    auto it = map.find("second");
    EXPECT_NE(it, map.end());
    EXPECT_EQ(it->first, "second");
    EXPECT_DOUBLE_EQ(it->second, 2.2);

    // Test try_get
    auto result = map.try_get("third");
    EXPECT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value(), 3.3);

    result = map.try_get("missing");
    EXPECT_FALSE(result.has_value());

    // Test with very long strings
    std::string longKey(1000, 'a');
    map[longKey] = 1000.0;
    EXPECT_DOUBLE_EQ(map[longKey], 1000.0);

    // Test with empty string key
    map[""] = 0.0;
    EXPECT_DOUBLE_EQ(map[""], 0.0);
}

// Performance comparison tests
TEST_F(QuickFlatMapIntTest, PerformanceComparison) {
    // This test compares performance of different map implementations
    // These are approximations and shouldn't be treated as rigorous benchmarks

    const int TEST_SIZE = 10000;

    // Generate random data for insertion
    auto testData =
        TestDataGenerator::generateRandomData<int, int>(TEST_SIZE, 1, 1000000);

    // Test standard map
    auto startStandard = std::chrono::high_resolution_clock::now();
    StandardMap standardMap;
    for (const auto& pair : testData) {
        standardMap.insert(pair);
    }
    auto endStandard = std::chrono::high_resolution_clock::now();

    // Test thread-safe map
    auto startThreadSafe = std::chrono::high_resolution_clock::now();
    ThreadSafeMap threadSafeMap;
    for (const auto& pair : testData) {
        threadSafeMap.insert(pair);
    }
    auto endThreadSafe = std::chrono::high_resolution_clock::now();

    // Test sorted vector map
    auto startSorted = std::chrono::high_resolution_clock::now();
    SortedVectorMap sortedMap;
    for (const auto& pair : testData) {
        sortedMap.insert(pair);
    }
    auto endSorted = std::chrono::high_resolution_clock::now();

    // Compare times (just log, don't assert)
    auto standardTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            endStandard - startStandard)
                            .count();
    auto threadSafeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                              endThreadSafe - startThreadSafe)
                              .count();
    auto sortedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                          endSorted - startSorted)
                          .count();

    std::cout << "Time to insert " << TEST_SIZE << " elements:" << std::endl;
    std::cout << "Standard map: " << standardTime << " ms" << std::endl;
    std::cout << "Thread-safe map: " << threadSafeTime << " ms" << std::endl;
    std::cout << "Sorted vector map: " << sortedTime << " ms" << std::endl;

    // Now test lookup performance
    std::vector<int> lookupKeys;
    lookupKeys.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        lookupKeys.push_back(testData[i].first);
    }

    // Randomize the lookup order
    std::mt19937 gen(42);
    std::shuffle(lookupKeys.begin(), lookupKeys.end(), gen);

    // Test standard map lookup
    startStandard = std::chrono::high_resolution_clock::now();
    for (int key : lookupKeys) {
        auto it = standardMap.find(key);
        EXPECT_NE(it, standardMap.end());
    }
    endStandard = std::chrono::high_resolution_clock::now();

    // Test thread-safe map lookup
    startThreadSafe = std::chrono::high_resolution_clock::now();
    for (int key : lookupKeys) {
        auto it = threadSafeMap.find(key);
        EXPECT_NE(it, threadSafeMap.end());
    }
    endThreadSafe = std::chrono::high_resolution_clock::now();

    // Test sorted vector map lookup
    startSorted = std::chrono::high_resolution_clock::now();
    for (int key : lookupKeys) {
        auto it = sortedMap.find(key);
        EXPECT_NE(it, sortedMap.end());
    }
    endSorted = std::chrono::high_resolution_clock::now();

    // Compare lookup times
    standardTime = std::chrono::duration_cast<std::chrono::microseconds>(
                       endStandard - startStandard)
                       .count();
    threadSafeTime = std::chrono::duration_cast<std::chrono::microseconds>(
                         endThreadSafe - startThreadSafe)
                         .count();
    sortedTime = std::chrono::duration_cast<std::chrono::microseconds>(
                     endSorted - startSorted)
                     .count();

    std::cout << "Time to lookup 1000 elements:" << std::endl;
    std::cout << "Standard map: " << standardTime << " μs" << std::endl;
    std::cout << "Thread-safe map: " << threadSafeTime << " μs" << std::endl;
    std::cout << "Sorted vector map: " << sortedTime << " μs" << std::endl;
}

// Basic tests for QuickFlatMultiMap
TEST_F(QuickFlatMultiMapIntTest, BasicMultimapOperations) {
    // Create a new multimap for testing
    StandardMultiMap map;

    // Test initial state
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);

    // Test single insertion
    auto result = map.insert({1, 100});
    EXPECT_TRUE(result.second);
    EXPECT_EQ(map.size(), 1);

    // Test duplicate key insertion (multimap allows this)
    result = map.insert({1, 200});
    EXPECT_TRUE(result.second);  // Should succeed for multimap
    EXPECT_EQ(map.size(), 2);

    // Test count
    EXPECT_EQ(map.count(1), 2);
    EXPECT_EQ(map.count(2), 0);

    // Test equal range to get all values for a key
    auto [begin, end] = map.equalRange(1);
    std::vector<int> values;
    for (auto it = begin; it != end; ++it) {
        values.push_back(it->second);
    }
    EXPECT_EQ(values.size(), 2);

    // Values might be in any order, so sort before comparing
    std::sort(values.begin(), values.end());
    EXPECT_THAT(values, ElementsAre(100, 200));

    // Test get_all
    auto allValues = map.get_all(1);
    std::sort(allValues.begin(), allValues.end());
    EXPECT_THAT(allValues, ElementsAre(100, 200));

    // Test operator[] (always returns first value for a key)
    EXPECT_EQ(map[1], 100);

    // Test at() (returns first value for a key)
    EXPECT_EQ(map.at(1), 100);
    EXPECT_THROW(map.at(2), exceptions::key_not_found_error);

    // Test erase (removes all instances of a key)
    EXPECT_TRUE(map.erase(1));
    EXPECT_EQ(map.size(), 0);
    EXPECT_FALSE(map.contains(1));
}
