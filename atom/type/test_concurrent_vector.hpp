// filepath: /home/max/Atom-1/atom/type/test_concurrent_vector.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "concurrent_vector.hpp"

using namespace atom::type;
using ::testing::ElementsAre;
using ::testing::Eq;

// Custom class for testing with non-trivial types
class TestObject {
public:
    TestObject() : value_(0) {}
    explicit TestObject(int value) : value_(value) {}

    TestObject(const TestObject& other) : value_(other.value_) {
        copy_count_++;
    }

    TestObject(TestObject&& other) noexcept : value_(other.value_) {
        other.value_ = 0;
        move_count_++;
    }

    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value_ = other.value_;
            copy_count_++;
        }
        return *this;
    }

    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            value_ = other.value_;
            other.value_ = 0;
            move_count_++;
        }
        return *this;
    }

    bool operator==(const TestObject& other) const {
        return value_ == other.value_;
    }

    int getValue() const { return value_; }

    static void resetCounters() {
        copy_count_ = 0;
        move_count_ = 0;
    }

    static int getCopyCount() { return copy_count_; }
    static int getMoveCount() { return move_count_; }

private:
    int value_;
    static int copy_count_;
    static int move_count_;
};

int TestObject::copy_count_ = 0;
int TestObject::move_count_ = 0;

// Fixture for concurrent_vector tests
class ConcurrentVectorTest : public ::testing::Test {
protected:
    void SetUp() override { TestObject::resetCounters(); }

    void TearDown() override {}

    // Helper method to simulate workload in parallel operations
    void simulate_work(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // Helper for testing with intentional exceptions
    class ThrowingObject {
    public:
        explicit ThrowingObject(bool throw_on_copy = false,
                                bool throw_on_move = false)
            : throw_on_copy_(throw_on_copy),
              throw_on_move_(throw_on_move),
              value_(0) {}

        explicit ThrowingObject(int value, bool throw_on_copy = false,
                                bool throw_on_move = false)
            : throw_on_copy_(throw_on_copy),
              throw_on_move_(throw_on_move),
              value_(value) {}

        ThrowingObject(const ThrowingObject& other)
            : throw_on_copy_(other.throw_on_copy_),
              throw_on_move_(other.throw_on_move_),
              value_(other.value_) {
            if (throw_on_copy_) {
                throw std::runtime_error("Copy constructor exception");
            }
        }

        ThrowingObject(ThrowingObject&& other) noexcept(false)
            : throw_on_copy_(other.throw_on_copy_),
              throw_on_move_(other.throw_on_move_),
              value_(other.value_) {
            if (throw_on_move_) {
                throw std::runtime_error("Move constructor exception");
            }
            other.value_ = 0;
        }

        ThrowingObject& operator=(const ThrowingObject& other) {
            if (throw_on_copy_) {
                throw std::runtime_error("Copy assignment exception");
            }
            if (this != &other) {
                throw_on_copy_ = other.throw_on_copy_;
                throw_on_move_ = other.throw_on_move_;
                value_ = other.value_;
            }
            return *this;
        }

        ThrowingObject& operator=(ThrowingObject&& other) noexcept(false) {
            if (throw_on_move_) {
                throw std::runtime_error("Move assignment exception");
            }
            if (this != &other) {
                throw_on_copy_ = other.throw_on_copy_;
                throw_on_move_ = other.throw_on_move_;
                value_ = other.value_;
                other.value_ = 0;
            }
            return *this;
        }

        bool operator==(const ThrowingObject& other) const {
            return value_ == other.value_;
        }

        int getValue() const { return value_; }

    private:
        bool throw_on_copy_;
        bool throw_on_move_;
        int value_;
    };
};

// Basic construction and initial state tests
TEST_F(ConcurrentVectorTest, DefaultConstruction) {
    concurrent_vector<int> vec;
    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), 0);
    EXPECT_TRUE(vec.empty());
}

TEST_F(ConcurrentVectorTest, ConstructionWithCapacity) {
    concurrent_vector<int> vec(100);
    EXPECT_EQ(vec.size(), 0);
    EXPECT_GE(vec.capacity(), 100);
    EXPECT_TRUE(vec.empty());
}

TEST_F(ConcurrentVectorTest, ConstructionWithZeroThreads) {
    EXPECT_THROW(concurrent_vector<int>(0, 0), std::invalid_argument);
}

TEST_F(ConcurrentVectorTest, ConstructionWithCustomThreadCount) {
    concurrent_vector<int> vec(0, 4);
    EXPECT_EQ(vec.thread_count(), 4);
}

// Basic operations tests
TEST_F(ConcurrentVectorTest, PushBack) {
    concurrent_vector<int> vec;

    vec.push_back(1);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec[0], 1);

    vec.push_back(2);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
}

TEST_F(ConcurrentVectorTest, PushBackMove) {
    concurrent_vector<std::string> vec;

    std::string s1 = "Hello";
    vec.push_back(std::move(s1));
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0], "Hello");
    EXPECT_TRUE(s1.empty());  // s1 should be moved from

    std::string s2 = "World";
    vec.push_back(std::move(s2));
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], "Hello");
    EXPECT_EQ(vec[1], "World");
    EXPECT_TRUE(s2.empty());  // s2 should be moved from
}

TEST_F(ConcurrentVectorTest, EmplaceBack) {
    concurrent_vector<TestObject> vec;

    TestObject::resetCounters();
    vec.emplace_back(42);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0].getValue(), 42);
    EXPECT_EQ(TestObject::getCopyCount(), 0);  // Should be constructed in-place
    EXPECT_EQ(TestObject::getMoveCount(), 0);

    vec.emplace_back(43);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].getValue(), 42);
    EXPECT_EQ(vec[1].getValue(), 43);
}

TEST_F(ConcurrentVectorTest, PopBack) {
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    auto val = vec.pop_back();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(*val, 3);

    val = vec.pop_back();
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(*val, 2);

    val = vec.pop_back();
    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(*val, 1);
    EXPECT_TRUE(vec.empty());
}

TEST_F(ConcurrentVectorTest, PopBackEmptyVector) {
    concurrent_vector<int> vec;
    EXPECT_THROW(vec.pop_back(), concurrent_vector_error);
}

// Access methods tests
TEST_F(ConcurrentVectorTest, At) {
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    EXPECT_EQ(vec.at(0), 1);
    EXPECT_EQ(vec.at(1), 2);
    EXPECT_EQ(vec.at(2), 3);

    EXPECT_THROW(vec.at(3), concurrent_vector_error);
    EXPECT_THROW(vec.at(100), concurrent_vector_error);
}

TEST_F(ConcurrentVectorTest, AtConst) {
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    const concurrent_vector<int>& const_vec = vec;
    EXPECT_EQ(const_vec.at(0), 1);
    EXPECT_EQ(const_vec.at(1), 2);
    EXPECT_EQ(const_vec.at(2), 3);

    EXPECT_THROW(const_vec.at(3), concurrent_vector_error);
    EXPECT_THROW(const_vec.at(100), concurrent_vector_error);
}

TEST_F(ConcurrentVectorTest, SubscriptOperator) {
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);

    // Modify using the subscript operator
    vec[1] = 42;
    EXPECT_EQ(vec[1], 42);
}

TEST_F(ConcurrentVectorTest, SubscriptOperatorConst) {
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    const concurrent_vector<int>& const_vec = vec;
    EXPECT_EQ(const_vec[0], 1);
    EXPECT_EQ(const_vec[1], 2);
    EXPECT_EQ(const_vec[2], 3);
}

TEST_F(ConcurrentVectorTest, Front) {
    concurrent_vector<int> vec;

    EXPECT_THROW(vec.front(), concurrent_vector_error);

    vec.push_back(42);
    EXPECT_EQ(vec.front(), 42);

    vec.push_back(43);
    EXPECT_EQ(vec.front(), 42);  // Front should still be 42

    // Modify the front
    vec.front() = 100;
    EXPECT_EQ(vec.front(), 100);
    EXPECT_EQ(vec[0], 100);
}

TEST_F(ConcurrentVectorTest, FrontConst) {
    concurrent_vector<int> vec;
    vec.push_back(42);
    vec.push_back(43);

    const concurrent_vector<int>& const_vec = vec;
    EXPECT_EQ(const_vec.front(), 42);
}

TEST_F(ConcurrentVectorTest, Back) {
    concurrent_vector<int> vec;

    EXPECT_THROW(vec.back(), concurrent_vector_error);

    vec.push_back(42);
    EXPECT_EQ(vec.back(), 42);

    vec.push_back(43);
    EXPECT_EQ(vec.back(), 43);  // Back should now be 43

    // Modify the back
    vec.back() = 100;
    EXPECT_EQ(vec.back(), 100);
    EXPECT_EQ(vec[1], 100);
}

TEST_F(ConcurrentVectorTest, BackConst) {
    concurrent_vector<int> vec;
    vec.push_back(42);
    vec.push_back(43);

    const concurrent_vector<int>& const_vec = vec;
    EXPECT_EQ(const_vec.back(), 43);
}

// Capacity management tests
TEST_F(ConcurrentVectorTest, Reserve) {
    concurrent_vector<int> vec;
    vec.reserve(100);

    EXPECT_EQ(vec.size(), 0);
    EXPECT_GE(vec.capacity(), 100);

    // Add some elements
    for (int i = 0; i < 50; i++) {
        vec.push_back(i);
    }

    // Reserve less than current size - should not shrink
    vec.reserve(10);
    EXPECT_GE(vec.capacity(), 50);

    // Reserve more
    vec.reserve(200);
    EXPECT_GE(vec.capacity(), 200);
}

TEST_F(ConcurrentVectorTest, ShrinkToFit) {
    concurrent_vector<int> vec;
    vec.reserve(100);

    // Add some elements
    for (int i = 0; i < 50; i++) {
        vec.push_back(i);
    }

    EXPECT_GE(vec.capacity(), 100);
    vec.shrink_to_fit();
    EXPECT_GE(vec.capacity(), 50);
}

TEST_F(ConcurrentVectorTest, Clear) {
    concurrent_vector<int> vec;

    for (int i = 0; i < 50; i++) {
        vec.push_back(i);
    }

    EXPECT_EQ(vec.size(), 50);
    vec.clear();
    EXPECT_EQ(vec.size(), 0);
    EXPECT_TRUE(vec.empty());

    // Capacity should remain unchanged after clear
    size_t capacity_before = vec.capacity();
    vec.clear();
    EXPECT_EQ(vec.capacity(), capacity_before);
}

TEST_F(ConcurrentVectorTest, ClearRange) {
    concurrent_vector<int> vec;

    for (int i = 0; i < 10; i++) {
        vec.push_back(i);
    }

    // Clear middle elements (3, 4, 5)
    vec.clear_range(3, 6);
    EXPECT_EQ(vec.size(), 7);
    EXPECT_EQ(vec[0], 0);
    EXPECT_EQ(vec[1], 1);
    EXPECT_EQ(vec[2], 2);
    EXPECT_EQ(vec[3], 6);
    EXPECT_EQ(vec[4], 7);
    EXPECT_EQ(vec[5], 8);
    EXPECT_EQ(vec[6], 9);

    // Clear from beginning
    vec.clear_range(0, 3);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec[0], 6);
    EXPECT_EQ(vec[1], 7);
    EXPECT_EQ(vec[2], 8);
    EXPECT_EQ(vec[3], 9);

    // Clear to end
    vec.clear_range(1, 4);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0], 6);

    // Test invalid ranges
    EXPECT_THROW(vec.clear_range(1, 1),
                 concurrent_vector_error);  // start == end
    EXPECT_THROW(vec.clear_range(2, 1),
                 concurrent_vector_error);  // start > end
    EXPECT_THROW(vec.clear_range(0, 2), concurrent_vector_error);  // end > size
}

// Batch operations tests
TEST_F(ConcurrentVectorTest, BatchInsert) {
    concurrent_vector<int> vec;

    // Create batch
    std::vector<int> batch(100);
    std::iota(batch.begin(), batch.end(), 0);  // Fill with 0-99

    // Insert batch
    vec.batch_insert(batch);
    EXPECT_EQ(vec.size(), 100);

    // Verify contents
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(vec[i], i);
    }

    // Insert another batch
    std::vector<int> batch2(50);
    std::iota(batch2.begin(), batch2.end(), 100);  // Fill with 100-149
    vec.batch_insert(batch2);

    EXPECT_EQ(vec.size(), 150);
    for (int i = 0; i < 150; i++) {
        EXPECT_EQ(vec[i], i);
    }

    // Empty batch should do nothing
    std::vector<int> empty_batch;
    vec.batch_insert(empty_batch);
    EXPECT_EQ(vec.size(), 150);
}

TEST_F(ConcurrentVectorTest, BatchInsertMove) {
    concurrent_vector<std::string> vec;

    // Create batch
    std::vector<std::string> batch;
    for (int i = 0; i < 100; i++) {
        batch.push_back("String " + std::to_string(i));
    }

    // Move insert batch
    vec.batch_insert(std::move(batch));
    EXPECT_EQ(vec.size(), 100);

    // Verify contents
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(vec[i], "String " + std::to_string(i));
    }

    // Original batch should be valid but empty
    EXPECT_TRUE(batch.empty());
}

TEST_F(ConcurrentVectorTest, ParallelBatchInsert) {
    concurrent_vector<int> vec;

    // Create batch
    std::vector<int> batch(1000);
    std::iota(batch.begin(), batch.end(), 0);  // Fill with 0-999

    // Insert batch in parallel
    vec.parallel_batch_insert(batch);
    EXPECT_EQ(vec.size(), 1000);

    // Verify contents
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(vec[i], i);
    }

    // Empty batch should do nothing
    std::vector<int> empty_batch;
    vec.parallel_batch_insert(empty_batch);
    EXPECT_EQ(vec.size(), 1000);
}

// Parallel operation tests
TEST_F(ConcurrentVectorTest, ParallelForEach) {
    concurrent_vector<int> vec;

    // Add elements
    for (int i = 0; i < 100; i++) {
        vec.push_back(i);
    }

    // Use parallel_for_each to double each value
    vec.parallel_for_each([](int& val) { val *= 2; });

    // Verify results
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(vec[i], i * 2);
    }
}

TEST_F(ConcurrentVectorTest, ParallelForEachConst) {
    concurrent_vector<int> vec;

    // Add elements
    for (int i = 0; i < 100; i++) {
        vec.push_back(i);
    }

    // Use parallel_for_each const version to compute sum
    std::atomic<int> sum(0);
    const concurrent_vector<int>& const_vec = vec;

    const_vec.parallel_for_each([&sum](const int& val) { sum += val; });

    // Verify result - sum should be 0 + 1 + 2 + ... + 99
    int expected_sum = (99 * 100) / 2;
    EXPECT_EQ(sum, expected_sum);
}

TEST_F(ConcurrentVectorTest, ParallelFind) {
    concurrent_vector<int> vec;

    // Add elements
    for (int i = 0; i < 1000; i++) {
        vec.push_back(i);
    }

    // Find existing elements
    auto idx50 = vec.parallel_find(50);
    EXPECT_TRUE(idx50.has_value());
    EXPECT_EQ(*idx50, 50);

    auto idx999 = vec.parallel_find(999);
    EXPECT_TRUE(idx999.has_value());
    EXPECT_EQ(*idx999, 999);

    // Try to find non-existent element
    auto idx1000 = vec.parallel_find(1000);
    EXPECT_FALSE(idx1000.has_value());

    // Test with empty vector
    concurrent_vector<int> empty_vec;
    auto empty_result = empty_vec.parallel_find(0);
    EXPECT_FALSE(empty_result.has_value());
}

TEST_F(ConcurrentVectorTest, ParallelTransform) {
    concurrent_vector<std::string> vec;

    // Add elements
    for (int i = 0; i < 100; i++) {
        vec.push_back("item" + std::to_string(i));
    }

    // Transform strings to uppercase
    vec.parallel_transform([](std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    });

    // Verify results
    for (int i = 0; i < 100; i++) {
        std::string expected = "ITEM" + std::to_string(i);
        EXPECT_EQ(vec[i], expected);
    }
}

// Thread safety tests
TEST_F(ConcurrentVectorTest, ConcurrentPushBack) {
    concurrent_vector<int> vec;

    // Spawn multiple threads that all push_back values
    std::vector<std::thread> threads;
    const int NUM_THREADS = 10;
    const int VALUES_PER_THREAD = 100;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.push_back(std::thread([&vec, t, VALUES_PER_THREAD]() {
            for (int i = 0; i < VALUES_PER_THREAD; i++) {
                vec.push_back(t * VALUES_PER_THREAD + i);
            }
        }));
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Check results
    EXPECT_EQ(vec.size(), NUM_THREADS * VALUES_PER_THREAD);

    // Create a set to check for duplicates
    std::set<int> values;
    for (size_t i = 0; i < vec.size(); i++) {
        values.insert(vec[i]);
    }

    // Make sure there are no duplicates
    EXPECT_EQ(values.size(), NUM_THREADS * VALUES_PER_THREAD);
}

TEST_F(ConcurrentVectorTest, ConcurrentReadWrite) {
    concurrent_vector<int> vec;

    // Initialize with some values
    for (int i = 0; i < 100; i++) {
        vec.push_back(i);
    }

    // Spawn reader threads and writer threads simultaneously
    std::vector<std::thread> threads;
    const int NUM_READERS = 5;
    const int NUM_WRITERS = 5;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> total_sum(0);

    // Readers
    for (int t = 0; t < NUM_READERS; t++) {
        threads.push_back(std::thread([&vec, &total_sum, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                int local_sum = 0;
                for (size_t j = 0; j < vec.size() && j < 100; j++) {
                    local_sum += vec[j];
                }
                total_sum += local_sum;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }));
    }

    // Writers
    for (int t = 0; t < NUM_WRITERS; t++) {
        threads.push_back(std::thread([&vec, t, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                vec.push_back(100 + t * OPS_PER_THREAD + i);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }));
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Final size should account for all pushed values
    EXPECT_EQ(vec.size(), 100 + NUM_WRITERS * OPS_PER_THREAD);
}

TEST_F(ConcurrentVectorTest, ConcurrentParallelOperations) {
    concurrent_vector<int> vec;

    // Initialize with some values
    for (int i = 0; i < 1000; i++) {
        vec.push_back(i);
    }

    // Create multiple futures for different operations
    auto future1 = std::async(std::launch::async, [&vec]() {
        vec.parallel_for_each([](int& val) {
            val *= 2;  // Double all values
        });
    });

    auto future2 = std::async(std::launch::async, [&vec]() {
        vec.parallel_find(500);  // Find a value
    });

    auto future3 = std::async(std::launch::async, [&vec]() {
        std::vector<int> batch(500);
        std::iota(batch.begin(), batch.end(), 1000);
        vec.parallel_batch_insert(batch);  // Add more values
    });

    auto future4 = std::async(std::launch::async, [&vec]() {
        vec.parallel_transform([](int& val) {
            val += 1;  // Add 1 to all values
        });
    });

    // Wait for all futures to complete
    future1.get();
    future2.get();
    future3.get();
    future4.get();

    // Size should include the original 1000 items plus 500 more from
    // batch_insert
    EXPECT_EQ(vec.size(), 1500);

    // Check that transform operations were applied
    // Values should have been doubled and incremented
    // But order of operations is non-deterministic, so we can't check exact
    // values
}

// Move semantics tests
TEST_F(ConcurrentVectorTest, MoveConstructor) {
    concurrent_vector<std::unique_ptr<int>> vec1;

    // Add some elements
    for (int i = 0; i < 10; i++) {
        vec1.push_back(std::make_unique<int>(i));
    }

    // Move to a new vector
    concurrent_vector<std::unique_ptr<int>> vec2(std::move(vec1));

    // Check that elements were moved
    EXPECT_EQ(vec2.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_NE(vec2[i], nullptr);
        EXPECT_EQ(*vec2[i], i);
    }
}

TEST_F(ConcurrentVectorTest, MoveAssignment) {
    concurrent_vector<std::unique_ptr<int>> vec1;
    concurrent_vector<std::unique_ptr<int>> vec2;

    // Add some elements to vec1
    for (int i = 0; i < 10; i++) {
        vec1.push_back(std::make_unique<int>(i));
    }

    // Add some elements to vec2
    for (int i = 0; i < 5; i++) {
        vec2.push_back(std::make_unique<int>(100 + i));
    }

    // Move assign vec1 to vec2
    vec2 = std::move(vec1);

    // Check that elements were moved
    EXPECT_EQ(vec2.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_NE(vec2[i], nullptr);
        EXPECT_EQ(*vec2[i], i);
    }
}

// Exception safety tests
TEST_F(ConcurrentVectorTest, ExceptionInPushBack) {
    concurrent_vector<ThrowingObject> vec;

    // Add some normal elements
    vec.push_back(ThrowingObject(1));
    vec.push_back(ThrowingObject(2));

    // Try to add an element that throws on copy
    ThrowingObject throwing(3, true);
    EXPECT_THROW(vec.push_back(throwing), concurrent_vector_error);

    // Vector should still contain the original elements
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].getValue(), 1);
    EXPECT_EQ(vec[1].getValue(), 2);
}

TEST_F(ConcurrentVectorTest, ExceptionInEmplaceBack) {
    concurrent_vector<ThrowingObject> vec;

    // Add some normal elements
    vec.emplace_back(1);
    vec.emplace_back(2);

    // Try to construct an element that throws
    EXPECT_THROW(vec.emplace_back(3, true), concurrent_vector_error);

    // Vector should still contain the original elements
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0].getValue(), 1);
    EXPECT_EQ(vec[1].getValue(), 2);
}
