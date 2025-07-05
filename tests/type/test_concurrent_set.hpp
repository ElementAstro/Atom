// filepath: /home/max/Atom-1/atom/type/test_concurrent_set.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <future>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "concurrent_set.hpp"

using namespace atom::type;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Not;

// Helper class for testing with complex types
class TestObject {
public:
    explicit TestObject(int id = 0) : id_(id) {}

    int getId() const { return id_; }

    bool operator==(const TestObject& other) const { return id_ == other.id_; }

    bool operator<(const TestObject& other) const { return id_ < other.id_; }

private:
    int id_;
};

// Hash function for TestObject
namespace std {
template <>
struct hash<TestObject> {
    size_t operator()(const TestObject& obj) const {
        return hash<int>()(obj.getId());
    }
};
}  // namespace std

// Serialization support for TestObject
inline std::vector<char> serialize(const TestObject& obj) {
    std::vector<char> result(sizeof(int));
    int id = obj.getId();
    std::memcpy(result.data(), &id, sizeof(int));
    return result;
}

// Declare the generic deserialize template function
template <typename T>
inline T deserialize(const std::vector<char>& data);

// Deserialization support for TestObject
template <>
inline TestObject deserialize<TestObject>(const std::vector<char>& data) {
    if (data.size() < sizeof(int)) {
        throw std::runtime_error(
            "Invalid data size for TestObject deserialization");
    }

    int id;
    std::memcpy(&id, data.data(), sizeof(int));
    return TestObject(id);
}

// Test fixture for LRUCache
class LRUCacheTest : public ::testing::Test {
protected:
    const size_t DEFAULT_CACHE_SIZE = 10;
};

// Test fixture for concurrent_set
class ConcurrentSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary file for testing file operations
        temp_filename_ =
            "test_concurrent_set_temp_" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count()) +
            ".bin";
    }

    void TearDown() override {
        // Clean up temporary file
        std::remove(temp_filename_.c_str());
    }

    std::string temp_filename_;
};

// Basic LRUCache tests
TEST_F(LRUCacheTest, ConstructorAndBasicOperations) {
    LRUCache<int> cache(DEFAULT_CACHE_SIZE);

    // Test exists on empty cache
    EXPECT_FALSE(cache.exists(42));

    // Test put and exists
    cache.put(42);
    EXPECT_TRUE(cache.exists(42));

    // Test get
    auto result = cache.get(42);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);

    // Test get for non-existent value
    auto missing = cache.get(99);
    EXPECT_FALSE(missing.has_value());
}

TEST_F(LRUCacheTest, CacheEviction) {
    LRUCache<int> cache(5);

    // Fill cache to capacity
    for (int i = 0; i < 5; i++) {
        cache.put(i);
        EXPECT_TRUE(cache.exists(i));
    }

    // Add one more, should evict oldest (0)
    cache.put(5);
    EXPECT_FALSE(cache.exists(0));
    EXPECT_TRUE(cache.exists(5));

    // Access key 1, then add another key - should evict 2, not 1
    cache.get(1);
    cache.put(6);
    EXPECT_TRUE(cache.exists(1));
    EXPECT_FALSE(cache.exists(2));
    EXPECT_TRUE(cache.exists(6));
}

TEST_F(LRUCacheTest, Clear) {
    LRUCache<int> cache(DEFAULT_CACHE_SIZE);

    for (int i = 0; i < 5; i++) {
        cache.put(i);
    }

    cache.clear();

    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(cache.exists(i));
    }
}

TEST_F(LRUCacheTest, Resize) {
    LRUCache<int> cache(5);

    // Fill cache
    for (int i = 0; i < 5; i++) {
        cache.put(i);
    }

    // Resize to smaller
    cache.resize(3);
    EXPECT_EQ(cache.get_max_size(), 3);
    EXPECT_FALSE(cache.exists(0));
    EXPECT_FALSE(cache.exists(1));
    EXPECT_TRUE(cache.exists(2));
    EXPECT_TRUE(cache.exists(3));
    EXPECT_TRUE(cache.exists(4));

    // Resize to larger
    cache.resize(10);
    EXPECT_EQ(cache.get_max_size(), 10);
}

TEST_F(LRUCacheTest, Stats) {
    LRUCache<int> cache(DEFAULT_CACHE_SIZE);

    // Initial stats should be zeros
    auto [hits, misses] = cache.get_stats();
    EXPECT_EQ(hits, 0);
    EXPECT_EQ(misses, 0);
    EXPECT_EQ(cache.get_hit_rate(), 0.0);

    // Add an item and check it - should be a miss then a hit
    EXPECT_FALSE(cache.exists(42));
    cache.put(42);
    EXPECT_TRUE(cache.exists(42));

    // Check stats
    std::tie(hits, misses) = cache.get_stats();
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(misses, 1);
    EXPECT_DOUBLE_EQ(cache.get_hit_rate(), 50.0);
}

TEST_F(LRUCacheTest, CacheSize) {
    LRUCache<int> cache(DEFAULT_CACHE_SIZE);

    EXPECT_EQ(cache.size(), 0);

    for (int i = 0; i < 5; i++) {
        cache.put(i);
    }

    EXPECT_EQ(cache.size(), 5);
    EXPECT_EQ(cache.get_max_size(), DEFAULT_CACHE_SIZE);
}

// Constructors and basic operations
TEST_F(ConcurrentSetTest, Constructor) {
    // Default constructor
    concurrent_set<int> set1;
    EXPECT_EQ(set1.size(), 0);

    // Constructor with thread count
    concurrent_set<int> set2(4);
    EXPECT_EQ(set2.size(), 0);
    EXPECT_EQ(set2.get_thread_count(), 4);

    // Constructor with thread count and cache size
    concurrent_set<int> set3(4, 500);
    EXPECT_EQ(set3.size(), 0);

    // Constructor with zero threads should throw
    EXPECT_THROW(concurrent_set<int>(0), std::invalid_argument);
}

TEST_F(ConcurrentSetTest, InsertAndFind) {
    concurrent_set<int> set;

    // Insert a value
    set.insert(42);
    EXPECT_EQ(set.size(), 1);
    EXPECT_EQ(set.get_insertion_count(), 1);

    // Find the value
    auto result = set.find(42);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
    EXPECT_EQ(set.get_find_count(), 1);

    // Find a non-existent value
    auto missing = set.find(99);
    EXPECT_FALSE(missing.has_value());
    EXPECT_EQ(set.get_find_count(), 2);
}

TEST_F(ConcurrentSetTest, InsertMoveSemantics) {
    concurrent_set<std::string> set;

    std::string value = "test_string";
    set.insert(std::move(value));

    EXPECT_EQ(set.size(), 1);
    auto result = set.find("test_string");
    EXPECT_TRUE(result.has_value());
}

TEST_F(ConcurrentSetTest, DuplicateInsert) {
    concurrent_set<int> set;

    set.insert(42);
    set.insert(42);  // Duplicate should be ignored

    EXPECT_EQ(set.size(), 1);
    // Insertion count should still be 1 since the second insert was a duplicate
    EXPECT_EQ(set.get_insertion_count(), 1);
}

TEST_F(ConcurrentSetTest, Erase) {
    concurrent_set<int> set;

    // Insert and then erase
    set.insert(42);
    bool erased = set.erase(42);

    EXPECT_TRUE(erased);
    EXPECT_EQ(set.size(), 0);
    EXPECT_EQ(set.get_deletion_count(), 1);

    // Erase non-existent value
    erased = set.erase(99);
    EXPECT_FALSE(erased);
    EXPECT_EQ(set.get_deletion_count(), 1);  // No change
}

TEST_F(ConcurrentSetTest, BatchInsert) {
    concurrent_set<int> set;

    std::vector<int> values = {1, 2, 3, 4, 5};
    set.batch_insert(values);

    EXPECT_EQ(set.size(), 5);
    EXPECT_EQ(set.get_insertion_count(), 5);

    for (int value : values) {
        auto result = set.find(value);
        EXPECT_TRUE(result.has_value());
    }
}

TEST_F(ConcurrentSetTest, BatchErase) {
    concurrent_set<int> set;

    // Insert some values
    std::vector<int> values = {1, 2, 3, 4, 5};
    set.batch_insert(values);

    // Erase a subset
    std::vector<int> to_erase = {2, 3, 7};  // 7 doesn't exist
    size_t erased = set.batch_erase(to_erase);

    EXPECT_EQ(erased, 2);  // Only 2 values should be erased
    EXPECT_EQ(set.size(), 3);
    EXPECT_EQ(set.get_deletion_count(), 2);

    // Verify what's left
    EXPECT_TRUE(set.find(1).has_value());
    EXPECT_FALSE(set.find(2).has_value());
    EXPECT_FALSE(set.find(3).has_value());
    EXPECT_TRUE(set.find(4).has_value());
    EXPECT_TRUE(set.find(5).has_value());
}

TEST_F(ConcurrentSetTest, Clear) {
    concurrent_set<int> set;

    // Insert some values
    std::vector<int> values = {1, 2, 3, 4, 5};
    set.batch_insert(values);

    // Clear the set
    set.clear();

    EXPECT_EQ(set.size(), 0);
    for (int value : values) {
        EXPECT_FALSE(set.find(value).has_value());
    }

    // Insertion count should remain unchanged (it's a historical counter)
    EXPECT_EQ(set.get_insertion_count(), 5);
}

// Async operations
TEST_F(ConcurrentSetTest, AsyncInsert) {
    concurrent_set<int> set;

    set.async_insert(42);

    // Wait for the async operation to complete
    EXPECT_TRUE(set.wait_for_tasks(1000));

    // Verify the insertion
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.find(42).has_value());
}

TEST_F(ConcurrentSetTest, AsyncInsertMove) {
    concurrent_set<std::string> set;

    std::string value = "test_string";
    set.async_insert(std::move(value));

    // Wait for the async operation to complete
    EXPECT_TRUE(set.wait_for_tasks(1000));

    // Verify the insertion
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.find("test_string").has_value());
}

TEST_F(ConcurrentSetTest, AsyncFind) {
    concurrent_set<int> set;
    set.insert(42);

    std::promise<std::optional<bool>> promise;
    auto future = promise.get_future();

    set.async_find(42, [&promise](std::optional<bool> result) {
        promise.set_value(result);
    });

    // Wait for the result
    EXPECT_TRUE(future.valid());
    auto result = future.get();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(ConcurrentSetTest, AsyncErase) {
    concurrent_set<int> set;
    set.insert(42);

    std::promise<bool> promise;
    auto future = promise.get_future();

    set.async_erase(42, [&promise](bool result) { promise.set_value(result); });

    // Wait for the result
    EXPECT_TRUE(future.valid());
    bool erased = future.get();
    EXPECT_TRUE(erased);

    // Verify the element is gone
    EXPECT_FALSE(set.find(42).has_value());
}

TEST_F(ConcurrentSetTest, AsyncBatchInsert) {
    concurrent_set<int> set;

    std::vector<int> values(1000);
    for (size_t i = 0; i < values.size(); i++) {
        values[i] = static_cast<int>(i);
    }

    std::promise<bool> promise;
    auto future = promise.get_future();

    set.async_batch_insert(
        values, [&promise](bool success) { promise.set_value(success); });

    // Wait for the result
    EXPECT_TRUE(future.valid());
    bool success = future.get();
    EXPECT_TRUE(success);

    // Wait for all internal tasks to complete
    EXPECT_TRUE(set.wait_for_tasks(1000));

    // Verify all values were inserted
    EXPECT_EQ(set.size(), values.size());
    for (int value : values) {
        EXPECT_TRUE(set.find(value).has_value());
    }
}

// Complex operations
TEST_F(ConcurrentSetTest, ParallelForEach) {
    concurrent_set<int> set;

    // Insert values
    std::vector<int> values(100);
    for (size_t i = 0; i < values.size(); i++) {
        values[i] = static_cast<int>(i);
    }
    set.batch_insert(values);

    // Use parallel_for_each to compute sum
    std::atomic<int> sum = 0;
    set.parallel_for_each([&sum](int value) { sum += value; });

    // Calculate expected sum
    int expected_sum = 0;
    for (int value : values) {
        expected_sum += value;
    }

    EXPECT_EQ(sum.load(), expected_sum);
}

TEST_F(ConcurrentSetTest, ConditionalFind) {
    concurrent_set<int> set;

    // Insert values
    for (int i = 0; i < 100; i++) {
        set.insert(i);
    }

    // Find even numbers
    auto even_numbers =
        set.conditional_find([](int value) { return value % 2 == 0; });

    // Verify results
    EXPECT_EQ(even_numbers.size(), 50);
    for (int value : even_numbers) {
        EXPECT_EQ(value % 2, 0);
    }
}

TEST_F(ConcurrentSetTest, AsyncConditionalFind) {
    concurrent_set<int> set;

    // Insert values
    for (int i = 0; i < 100; i++) {
        set.insert(i);
    }

    std::promise<std::vector<int>> promise;
    auto future = promise.get_future();

    // Find even numbers asynchronously
    set.async_conditional_find(
        [](int value) { return value % 2 == 0; },
        [&promise](std::vector<int> results) { promise.set_value(results); });

    // Wait for the result
    EXPECT_TRUE(future.valid());
    auto even_numbers = future.get();

    // Verify results
    EXPECT_EQ(even_numbers.size(), 50);
    for (int value : even_numbers) {
        EXPECT_EQ(value % 2, 0);
    }
}

TEST_F(ConcurrentSetTest, Transaction) {
    concurrent_set<int> set;

    // Create a transaction that inserts some values
    std::vector<std::function<void()>> operations = {[&]() { set.insert(1); },
                                                     [&]() { set.insert(2); },
                                                     [&]() { set.insert(3); }};

    bool success = set.transaction(operations);
    EXPECT_TRUE(success);
    EXPECT_EQ(set.size(), 3);

    // Create a transaction with a failing operation
    std::vector<std::function<void()>> fail_operations = {
        [&]() { set.insert(4); },
        [&]() { throw std::runtime_error("Test error"); },
        [&]() { set.insert(6); }};

    success = set.transaction(fail_operations);
    EXPECT_FALSE(success);

    // Check that the set was rolled back - only the original values should
    // exist
    EXPECT_EQ(set.size(), 3);
    EXPECT_TRUE(set.find(1).has_value());
    EXPECT_TRUE(set.find(2).has_value());
    EXPECT_TRUE(set.find(3).has_value());
    EXPECT_FALSE(set.find(4).has_value());
    EXPECT_FALSE(set.find(6).has_value());
}

// Thread pool adjustments
TEST_F(ConcurrentSetTest, AdjustThreadPoolSize) {
    concurrent_set<int> set(4);

    EXPECT_EQ(set.get_thread_count(), 4);

    // Increase thread count
    set.adjust_thread_pool_size(8);
    EXPECT_EQ(set.get_thread_count(), 8);

    // Decrease thread count
    set.adjust_thread_pool_size(2);
    EXPECT_EQ(set.get_thread_count(), 2);

    // Zero threads should throw
    EXPECT_THROW(set.adjust_thread_pool_size(0), std::invalid_argument);
}

// Cache operations
TEST_F(ConcurrentSetTest, CacheOperations) {
    concurrent_set<int> set(4, 10);

    // Insert some values to populate cache
    for (int i = 0; i < 20; i++) {
        set.insert(i);
    }

    // Check cache hit by finding a value multiple times
    for (int i = 0; i < 5; i++) {
        set.find(5);
    }

    // Get cache stats
    auto [cache_size, hits, misses, hit_rate] = set.get_cache_stats();

    EXPECT_GT(hits, 0);
    EXPECT_GT(hit_rate, 0.0);

    // Resize cache
    set.resize_cache(20);
    auto [new_size, new_hits, new_misses, new_hit_rate] = set.get_cache_stats();
    EXPECT_EQ(new_size, 20);
}

// File operations
TEST_F(ConcurrentSetTest, SaveAndLoadFile) {
    concurrent_set<int> set;

    // Insert some values
    for (int i = 0; i < 100; i++) {
        set.insert(i);
    }

    // Save to file
    bool saved = set.save_to_file(temp_filename_);
    EXPECT_TRUE(saved);

    // Create a new set and load from file
    concurrent_set<int> loaded_set;
    bool loaded = loaded_set.load_from_file(temp_filename_);
    EXPECT_TRUE(loaded);

    // Verify loaded data
    EXPECT_EQ(loaded_set.size(), 100);
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(loaded_set.find(i).has_value());
    }

    // Check that metrics were also restored
    EXPECT_EQ(loaded_set.get_insertion_count(), 100);
}

TEST_F(ConcurrentSetTest, AsyncSaveToFile) {
    concurrent_set<int> set;

    // Insert some values
    for (int i = 0; i < 100; i++) {
        set.insert(i);
    }

    std::promise<bool> promise;
    auto future = promise.get_future();

    // Async save
    set.async_save_to_file(temp_filename_, [&promise](bool success) {
        promise.set_value(success);
    });

    // Wait for the result
    EXPECT_TRUE(future.valid());
    bool success = future.get();
    EXPECT_TRUE(success);

    // Verify by loading the file
    concurrent_set<int> loaded_set;
    bool loaded = loaded_set.load_from_file(temp_filename_);
    EXPECT_TRUE(loaded);
    EXPECT_EQ(loaded_set.size(), 100);
}

// Testing with complex types
TEST_F(ConcurrentSetTest, ComplexTypes) {
    concurrent_set<TestObject> set;

    // Insert objects
    for (int i = 0; i < 10; i++) {
        set.insert(TestObject(i));
    }

    EXPECT_EQ(set.size(), 10);

    // Find objects
    for (int i = 0; i < 10; i++) {
        auto result = set.find(TestObject(i));
        EXPECT_TRUE(result.has_value());
    }

    // Erase an object
    bool erased = set.erase(TestObject(5));
    EXPECT_TRUE(erased);
    EXPECT_EQ(set.size(), 9);

    // Save and load
    bool saved = set.save_to_file(temp_filename_);
    EXPECT_TRUE(saved);

    concurrent_set<TestObject> loaded_set;
    bool loaded = loaded_set.load_from_file(temp_filename_);
    EXPECT_TRUE(loaded);
    EXPECT_EQ(loaded_set.size(), 9);
}

// Error handling
TEST_F(ConcurrentSetTest, ErrorCallback) {
    concurrent_set<int> set;

    std::atomic<bool> callback_called = false;
    std::string error_message;
    std::mutex error_message_mutex;

    set.set_error_callback([&](std::string_view msg, std::exception_ptr) {
        callback_called = true;
        {
            std::lock_guard<std::mutex> lock(error_message_mutex);
            error_message = msg;
        }
    });

    // Trigger an error by loading from a non-existent file
    try {
        set.load_from_file("nonexistent_file.bin");
    } catch (...) {
        // Exception is still thrown after callback
    }

    EXPECT_TRUE(callback_called);
    EXPECT_FALSE(error_message.empty());
}

// Thread safety stress tests
TEST_F(ConcurrentSetTest, ThreadSafetyStressTest) {
    concurrent_set<int> set(8, 100);  // 8 threads, 100 cache size
    std::atomic<int> success_count = 0;
    std::atomic<int> error_count = 0;

    const int num_threads = 10;
    const int operations_per_thread = 1000;
    const int value_range = 100;

    std::vector<std::thread> threads;

    // Create worker threads
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);  // Different seed for each thread
            std::uniform_int_distribution<int> dist(0, value_range - 1);
            std::uniform_int_distribution<int> op_dist(0, 3);  // 4 operations

            for (int i = 0; i < operations_per_thread; i++) {
                int value = dist(rng);

                try {
                    switch (op_dist(rng)) {
                        case 0:  // insert
                            set.insert(value);
                            success_count++;
                            break;
                        case 1:  // find
                            set.find(value);
                            success_count++;
                            break;
                        case 2:  // erase
                            set.erase(value);
                            success_count++;
                            break;
                        case 3:  // async insert
                            set.async_insert(value);
                            success_count++;
                            break;
                    }
                } catch (const std::exception&) {
                    error_count++;
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for any outstanding async tasks
    set.wait_for_tasks(5000);

    // Verify that operations were performed
    EXPECT_EQ(success_count, num_threads * operations_per_thread);
    EXPECT_EQ(error_count, 0);

    // The set should contain some values
    EXPECT_GT(set.size(), 0);

    // Check stats
    EXPECT_GT(set.get_insertion_count(), 0);
    EXPECT_GT(set.get_find_count(), 0);
}

// Move semantics tests
TEST_F(ConcurrentSetTest, MoveConstructor) {
    concurrent_set<int> set1;

    // Insert some values
    for (int i = 0; i < 10; i++) {
        set1.insert(i);
    }

    // Move construct
    concurrent_set<int> set2(std::move(set1));

    // Check that data was moved
    EXPECT_EQ(set2.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(set2.find(i).has_value());
    }
}

TEST_F(ConcurrentSetTest, MoveAssignment) {
    concurrent_set<int> set1;
    concurrent_set<int> set2;

    // Insert values into set1
    for (int i = 0; i < 10; i++) {
        set1.insert(i);
    }

    // Move assign
    set2 = std::move(set1);

    // Check that data was moved
    EXPECT_EQ(set2.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(set2.find(i).has_value());
    }
}

// Edge cases
TEST_F(ConcurrentSetTest, EmptySetOperations) {
    concurrent_set<int> set;

    // Operations on empty set
    EXPECT_EQ(set.size(), 0);
    EXPECT_FALSE(set.find(42).has_value());
    EXPECT_FALSE(set.erase(42));

    // Empty batch operations
    std::vector<int> empty_batch;
    EXPECT_NO_THROW(set.batch_insert(empty_batch));
    EXPECT_EQ(set.batch_erase(empty_batch), 0);

    // Empty transaction
    std::vector<std::function<void()>> empty_ops;
    EXPECT_TRUE(set.transaction(empty_ops));
}

TEST_F(ConcurrentSetTest, EdgeCasePendingTaskCount) {
    concurrent_set<int> set;

    // Initially no pending tasks
    EXPECT_EQ(set.get_pending_task_count(), 0);

    // Add some async tasks
    for (int i = 0; i < 10; i++) {
        set.async_insert(i);
    }

    // Should have pending tasks now
    EXPECT_GT(set.get_pending_task_count(), 0);

    // Wait for tasks to complete
    set.wait_for_tasks();

    // Now should have no pending tasks again
    EXPECT_EQ(set.get_pending_task_count(), 0);
}

TEST_F(ConcurrentSetTest, FileOperationEdgeCases) {
    concurrent_set<int> set;

    // Empty filename
    EXPECT_THROW(set.save_to_file(""), std::invalid_argument);
    EXPECT_THROW(set.load_from_file(""), std::invalid_argument);
    EXPECT_THROW(set.async_save_to_file(""), std::invalid_argument);

    // Non-existent file
    EXPECT_THROW(set.load_from_file("nonexistent_file.bin"), io_exception);

    // Save empty set
    EXPECT_TRUE(set.save_to_file(temp_filename_));

    // Load from empty set file
    concurrent_set<int> loaded_set;
    EXPECT_TRUE(loaded_set.load_from_file(temp_filename_));
    EXPECT_EQ(loaded_set.size(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
