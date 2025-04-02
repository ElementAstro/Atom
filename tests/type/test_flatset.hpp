// filepath: /home/max/Atom-1/atom/type/test_flatset.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "flatset.hpp"

using namespace atom::type;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class FlatSetTest : public ::testing::Test {
protected:
    // Common test data
    FlatSet<int> empty_set_;
    FlatSet<int> small_set_;
    FlatSet<std::string> string_set_;
    FlatSet<int, std::greater<int>> reverse_set_;

    // Performance test settings
    const size_t LARGE_SIZE = 100000;
    const int TEST_ITERATIONS = 100;

    void SetUp() override {
        // Initialize sets with test data
        small_set_.insert({5, 3, 1, 4, 2});
        string_set_.insert({"apple", "banana", "cherry", "date"});
        reverse_set_.insert({5, 3, 1, 4, 2});
    }

    // Helper function to measure execution time
    template <typename Func>
    long long measureExecutionTime(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     start)
            .count();
    }

    // Helper function to generate a large set
    FlatSet<int> generateLargeSet(size_t size) {
        std::vector<int> values(size);
        for (size_t i = 0; i < size; ++i) {
            values[i] = static_cast<int>(i);
        }
        // Shuffle to ensure random insertion order
        std::random_shuffle(values.begin(), values.end());
        return FlatSet<int>(values.begin(), values.end());
    }

    // Helper to create a custom comparable type
    struct ComplexType {
        int id;
        std::string name;

        bool operator==(const ComplexType& other) const {
            return id == other.id && name == other.name;
        }
    };

    struct ComplexTypeCompare {
        bool operator()(const ComplexType& a, const ComplexType& b) const {
            return a.id < b.id;
        }
    };
};

// Test constructors
TEST_F(FlatSetTest, DefaultConstructor) {
    FlatSet<int> set;
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST_F(FlatSetTest, CustomComparatorConstructor) {
    FlatSet<int, std::greater<int>> set;
    set.insert({1, 2, 3, 4, 5});

    // With greater as comparator, elements should be in descending order
    std::vector<int> expected = {5, 4, 3, 2, 1};
    EXPECT_TRUE(std::equal(set.begin(), set.end(), expected.begin()));
}

TEST_F(FlatSetTest, RangeConstructor) {
    std::vector<int> values = {5, 4, 3, 2, 1, 3, 4};  // Note duplicates
    FlatSet<int> set(values.begin(), values.end());

    EXPECT_EQ(set.size(), 5);  // Should eliminate duplicates
    EXPECT_TRUE(std::is_sorted(set.begin(), set.end()));
}

TEST_F(FlatSetTest, InitializerListConstructor) {
    FlatSet<int> set = {5, 3, 1, 4, 2, 3, 4};  // Note duplicates

    EXPECT_EQ(set.size(), 5);  // Should eliminate duplicates
    EXPECT_TRUE(std::is_sorted(set.begin(), set.end()));
}

TEST_F(FlatSetTest, CopyConstructor) {
    FlatSet<int> copy_set(small_set_);

    EXPECT_EQ(copy_set.size(), small_set_.size());
    EXPECT_TRUE(
        std::equal(copy_set.begin(), copy_set.end(), small_set_.begin()));
}

TEST_F(FlatSetTest, MoveConstructor) {
    FlatSet<int> original({5, 3, 1, 4, 2});
    size_t original_size = original.size();

    FlatSet<int> moved_set(std::move(original));

    EXPECT_EQ(moved_set.size(), original_size);
    EXPECT_TRUE(
        original.empty());  // Original should be in valid but unspecified state
}

// Test assignment operators
TEST_F(FlatSetTest, CopyAssignmentOperator) {
    FlatSet<int> set;
    set = small_set_;

    EXPECT_EQ(set.size(), small_set_.size());
    EXPECT_TRUE(std::equal(set.begin(), set.end(), small_set_.begin()));
}

TEST_F(FlatSetTest, MoveAssignmentOperator) {
    FlatSet<int> original({5, 3, 1, 4, 2});
    size_t original_size = original.size();

    FlatSet<int> moved_set;
    moved_set = std::move(original);

    EXPECT_EQ(moved_set.size(), original_size);
    EXPECT_TRUE(
        original.empty());  // Original should be in valid but unspecified state
}

// Test iterators
TEST_F(FlatSetTest, Iterators) {
    // Test begin/end
    auto it = small_set_.begin();
    auto end = small_set_.end();

    std::vector<int> values;
    while (it != end) {
        values.push_back(*it);
        ++it;
    }

    EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5));

    // Test const iterators
    const auto& const_set = small_set_;
    auto const_it = const_set.begin();
    auto const_end = const_set.end();

    values.clear();
    while (const_it != const_end) {
        values.push_back(*const_it);
        ++const_it;
    }

    EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5));

    // Test reverse iterators
    auto rit = small_set_.rbegin();
    auto rend = small_set_.rend();

    values.clear();
    while (rit != rend) {
        values.push_back(*rit);
        ++rit;
    }

    EXPECT_THAT(values, ElementsAre(5, 4, 3, 2, 1));
}

// Test capacity methods
TEST_F(FlatSetTest, Empty) {
    EXPECT_TRUE(empty_set_.empty());
    EXPECT_FALSE(small_set_.empty());
}

TEST_F(FlatSetTest, Size) {
    EXPECT_EQ(empty_set_.size(), 0);
    EXPECT_EQ(small_set_.size(), 5);
}

TEST_F(FlatSetTest, MaxSize) { EXPECT_GT(small_set_.max_size(), 0); }

TEST_F(FlatSetTest, Capacity) {
    // Test that capacity is at least as large as size
    EXPECT_GE(small_set_.capacity(), small_set_.size());
}

TEST_F(FlatSetTest, Reserve) {
    FlatSet<int> set;
    set.reserve(100);
    EXPECT_GE(set.capacity(), 100);

    // Test that reserve throws for unreasonable sizes
    EXPECT_THROW(set.reserve(set.max_size() + 1), std::length_error);
}

TEST_F(FlatSetTest, ShrinkToFit) {
    FlatSet<int> set;
    set.reserve(100);
    set.insert({1, 2, 3});

    size_t capacity_before = set.capacity();
    EXPECT_GE(capacity_before, 100);

    set.shrink_to_fit();

    // The capacity should be reduced to close to the size
    EXPECT_LT(set.capacity(), capacity_before);
    EXPECT_GE(set.capacity(), set.size());
}

// Test element access and modification
TEST_F(FlatSetTest, Clear) {
    FlatSet<int> set({1, 2, 3, 4, 5});
    EXPECT_FALSE(set.empty());

    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST_F(FlatSetTest, Insert) {
    FlatSet<int> set;

    // Insert a new element
    auto [it1, inserted1] = set.insert(5);
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(*it1, 5);

    // Try to insert an element that already exists
    auto [it2, inserted2] = set.insert(5);
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(*it2, 5);

    // Insert more elements
    set.insert(3);
    set.insert(1);
    set.insert(4);
    set.insert(2);

    EXPECT_EQ(set.size(), 5);
    EXPECT_THAT(std::vector<int>(set.begin(), set.end()),
                ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(FlatSetTest, InsertRvalue) {
    FlatSet<std::string> set;

    // Insert an rvalue
    auto [it, inserted] = set.insert(std::string("test"));
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, "test");
}

TEST_F(FlatSetTest, InsertHint) {
    FlatSet<int> set({1, 3, 5});

    // Insert with correct hint
    auto it = set.find(3);
    auto result = set.insert(it, 2);
    EXPECT_EQ(*result, 2);

    // Insert with incorrect hint
    result = set.insert(set.begin(), 4);
    EXPECT_EQ(*result, 4);

    // Verify final set
    EXPECT_THAT(std::vector<int>(set.begin(), set.end()),
                ElementsAre(1, 2, 3, 4, 5));

    // Test with invalid hint
    EXPECT_THROW(set.insert(set.end() + 1, 6), std::invalid_argument);
}

TEST_F(FlatSetTest, InsertRange) {
    FlatSet<int> set;
    std::vector<int> values = {5, 3, 1, 4, 2, 3, 4};  // Note duplicates

    set.insert(values.begin(), values.end());

    EXPECT_EQ(set.size(), 5);  // Should eliminate duplicates
    EXPECT_THAT(std::vector<int>(set.begin(), set.end()),
                ElementsAre(1, 2, 3, 4, 5));

    // Test with invalid range
    EXPECT_THROW(set.insert(values.end(), values.begin()),
                 std::invalid_argument);
}

TEST_F(FlatSetTest, InsertInitializerList) {
    FlatSet<int> set;

    set.insert({5, 3, 1, 4, 2, 3, 4});  // Note duplicates

    EXPECT_EQ(set.size(), 5);  // Should eliminate duplicates
    EXPECT_THAT(std::vector<int>(set.begin(), set.end()),
                ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(FlatSetTest, Emplace) {
    FlatSet<std::string> set;

    auto [it1, inserted1] = set.emplace("apple");
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(*it1, "apple");

    auto [it2, inserted2] = set.emplace("apple");  // Duplicate
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(*it2, "apple");

    set.emplace("banana");
    set.emplace("cherry");

    EXPECT_EQ(set.size(), 3);
}

TEST_F(FlatSetTest, EmplaceHint) {
    FlatSet<std::string> set({"apple", "cherry"});

    // Insert with hint
    auto it = set.find("cherry");
    auto result = set.emplace_hint(it, "banana");
    EXPECT_EQ(*result, "banana");

    // Verify order
    EXPECT_THAT(std::vector<std::string>(set.begin(), set.end()),
                ElementsAre("apple", "banana", "cherry"));

    // Test with invalid hint
    EXPECT_THROW(set.emplace_hint(set.end() + 1, "date"),
                 std::invalid_argument);
}

TEST_F(FlatSetTest, EraseIterator) {
    FlatSet<int> set = small_set_;  // Make a copy

    auto it = set.find(3);
    auto next_it = set.erase(it);

    EXPECT_EQ(set.size(), 4);
    EXPECT_FALSE(set.contains(3));
    EXPECT_EQ(*next_it, 4);  // Next element after 3

    // Test invalid iterator
    EXPECT_THROW(set.erase(set.end()), std::invalid_argument);
}

TEST_F(FlatSetTest, EraseRange) {
    FlatSet<int> set = small_set_;  // Make a copy

    auto first = set.find(2);
    auto last = set.find(4);
    ++last;  // Iterator to element past 4

    auto next_it = set.erase(first, last);

    EXPECT_EQ(set.size(), 3);
    EXPECT_FALSE(set.contains(2));
    EXPECT_FALSE(set.contains(3));
    EXPECT_FALSE(set.contains(4));
    EXPECT_EQ(*next_it, 5);  // Next element after erased range

    // Test invalid range
    EXPECT_THROW(set.erase(set.end(), set.begin()), std::invalid_argument);
}

TEST_F(FlatSetTest, EraseValue) {
    FlatSet<int> set = small_set_;  // Make a copy

    size_t erased = set.erase(3);
    EXPECT_EQ(erased, 1);
    EXPECT_EQ(set.size(), 4);
    EXPECT_FALSE(set.contains(3));

    // Try to erase an element that doesn't exist
    erased = set.erase(10);
    EXPECT_EQ(erased, 0);
    EXPECT_EQ(set.size(), 4);
}

TEST_F(FlatSetTest, Swap) {
    FlatSet<int> set1({1, 2, 3});
    FlatSet<int> set2({4, 5, 6});

    set1.swap(set2);

    EXPECT_THAT(std::vector<int>(set1.begin(), set1.end()),
                ElementsAre(4, 5, 6));
    EXPECT_THAT(std::vector<int>(set2.begin(), set2.end()),
                ElementsAre(1, 2, 3));

    // Test non-member swap function
    swap(set1, set2);

    EXPECT_THAT(std::vector<int>(set1.begin(), set1.end()),
                ElementsAre(1, 2, 3));
    EXPECT_THAT(std::vector<int>(set2.begin(), set2.end()),
                ElementsAre(4, 5, 6));
}

// Test lookup operations
TEST_F(FlatSetTest, Count) {
    EXPECT_EQ(small_set_.count(3), 1);
    EXPECT_EQ(small_set_.count(10), 0);
}

TEST_F(FlatSetTest, Find) {
    auto it = small_set_.find(3);
    EXPECT_NE(it, small_set_.end());
    EXPECT_EQ(*it, 3);

    it = small_set_.find(10);
    EXPECT_EQ(it, small_set_.end());

    // Test const version
    const auto& const_set = small_set_;
    auto const_it = const_set.find(3);
    EXPECT_NE(const_it, const_set.end());
    EXPECT_EQ(*const_it, 3);
}

TEST_F(FlatSetTest, Contains) {
    EXPECT_TRUE(small_set_.contains(3));
    EXPECT_FALSE(small_set_.contains(10));
}

TEST_F(FlatSetTest, EqualRange) {
    auto [first, last] = small_set_.equalRange(3);
    EXPECT_NE(first, small_set_.end());
    EXPECT_EQ(*first, 3);
    EXPECT_EQ(std::distance(first, last), 1);

    auto [first_not_found, last_not_found] = small_set_.equalRange(10);
    EXPECT_EQ(first_not_found, last_not_found);

    // Test const version
    const auto& const_set = small_set_;
    auto [const_first, const_last] = const_set.equalRange(3);
    EXPECT_NE(const_first, const_set.end());
    EXPECT_EQ(*const_first, 3);
    EXPECT_EQ(std::distance(const_first, const_last), 1);
}

TEST_F(FlatSetTest, LowerBound) {
    auto it = small_set_.lowerBound(3);
    EXPECT_NE(it, small_set_.end());
    EXPECT_EQ(*it, 3);

    it = small_set_.lowerBound(2.5);  // Between 2 and 3
    EXPECT_NE(it, small_set_.end());
    EXPECT_EQ(*it, 3);

    it = small_set_.lowerBound(10);  // Beyond the last element
    EXPECT_EQ(it, small_set_.end());

    // Test const version
    const auto& const_set = small_set_;
    auto const_it = const_set.lowerBound(3);
    EXPECT_NE(const_it, const_set.end());
    EXPECT_EQ(*const_it, 3);
}

TEST_F(FlatSetTest, UpperBound) {
    auto it = small_set_.upperBound(3);
    EXPECT_NE(it, small_set_.end());
    EXPECT_EQ(*it, 4);

    it = small_set_.upperBound(2.5);  // Between 2 and 3
    EXPECT_NE(it, small_set_.end());
    EXPECT_EQ(*it, 3);

    it = small_set_.upperBound(10);  // Beyond the last element
    EXPECT_EQ(it, small_set_.end());

    // Test const version
    const auto& const_set = small_set_;
    auto const_it = const_set.upperBound(3);
    EXPECT_NE(const_it, const_set.end());
    EXPECT_EQ(*const_it, 4);
}

TEST_F(FlatSetTest, KeyComp) {
    auto comp = small_set_.keyComp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
}

TEST_F(FlatSetTest, ValueComp) {
    auto comp = small_set_.valueComp();
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
}

// Test comparison operators
TEST_F(FlatSetTest, EqualityOperator) {
    FlatSet<int> set1({1, 2, 3, 4, 5});
    FlatSet<int> set2({1, 2, 3, 4, 5});
    FlatSet<int> set3({1, 2, 3, 4, 6});

    EXPECT_EQ(set1, set2);
    EXPECT_NE(set1, set3);
}

TEST_F(FlatSetTest, LessThanOperator) {
    FlatSet<int> set1({1, 2, 3});
    FlatSet<int> set2({1, 2, 4});
    FlatSet<int> set3({1, 2, 3, 4});

    EXPECT_LT(set1, set2);  // Same length, lexicographically less
    EXPECT_LT(set1, set3);  // Shorter
    EXPECT_FALSE(set2 < set1);
    EXPECT_FALSE(set3 < set1);
}

TEST_F(FlatSetTest, LessThanOrEqualOperator) {
    FlatSet<int> set1({1, 2, 3});
    FlatSet<int> set2({1, 2, 3});
    FlatSet<int> set3({1, 2, 4});

    EXPECT_LE(set1, set2);  // Equal
    EXPECT_LE(set1, set3);  // Less than
    EXPECT_FALSE(set3 <= set1);
}

TEST_F(FlatSetTest, GreaterThanOperator) {
    FlatSet<int> set1({1, 2, 4});
    FlatSet<int> set2({1, 2, 3});
    FlatSet<int> set3({1, 2});

    EXPECT_GT(set1, set2);  // Same length, lexicographically greater
    EXPECT_GT(set1, set3);  // Longer
    EXPECT_FALSE(set2 > set1);
    EXPECT_FALSE(set3 > set1);
}

TEST_F(FlatSetTest, GreaterThanOrEqualOperator) {
    FlatSet<int> set1({1, 2, 3});
    FlatSet<int> set2({1, 2, 3});
    FlatSet<int> set3({1, 2});

    EXPECT_GE(set1, set2);  // Equal
    EXPECT_GE(set1, set3);  // Greater than
    EXPECT_FALSE(set3 >= set1);
}

// Test edge cases and exceptions
TEST_F(FlatSetTest, EmptyRangeConstructor) {
    std::vector<int> empty_vec;
    FlatSet<int> set(empty_vec.begin(), empty_vec.end());

    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST_F(FlatSetTest, CustomComparisonType) {
    using MySet = FlatSet<ComplexType, ComplexTypeCompare>;

    ComplexType a{1, "Alice"};
    ComplexType b{2, "Bob"};
    ComplexType c{3, "Charlie"};

    MySet set;
    set.insert(b);
    set.insert(a);
    set.insert(c);

    // Elements should be sorted by ID
    auto it = set.begin();
    EXPECT_EQ(it->id, 1);
    EXPECT_EQ(it->name, "Alice");
    ++it;
    EXPECT_EQ(it->id, 2);
    EXPECT_EQ(it->name, "Bob");
    ++it;
    EXPECT_EQ(it->id, 3);
    EXPECT_EQ(it->name, "Charlie");

    // Test find
    auto found = set.find(ComplexType{2, "Bob"});
    EXPECT_NE(found, set.end());
    EXPECT_EQ(found->id, 2);
    EXPECT_EQ(found->name, "Bob");

    // Test contains
    EXPECT_TRUE(set.contains(ComplexType{2, "Bob"}));
    EXPECT_FALSE(set.contains(ComplexType{4, "Dave"}));
}

TEST_F(FlatSetTest, ViewMethod) {
    auto view = small_set_.view();

    std::vector<int> values;
    for (const auto& value : view) {
        values.push_back(value);
    }

    EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5));
}

// Performance tests
TEST_F(FlatSetTest, InsertPerformance) {
    // Test insertion performance with reserve vs without
    FlatSet<int> set_with_reserve;
    set_with_reserve.reserve(LARGE_SIZE);

    FlatSet<int> set_without_reserve;

    std::vector<int> values(LARGE_SIZE);
    for (size_t i = 0; i < LARGE_SIZE; ++i) {
        values[i] = static_cast<int>(i);
    }

    // Measure insertion time with reserve
    auto time_with_reserve = measureExecutionTime([&]() {
        for (int val : values) {
            set_with_reserve.insert(val);
        }
    });

    // Measure insertion time without reserve
    auto time_without_reserve = measureExecutionTime([&]() {
        for (int val : values) {
            set_without_reserve.insert(val);
        }
    });

    // Not making a strict assertion since performance varies by system,
    // but with reserve should generally be faster
    std::cout << "Insert with reserve: " << time_with_reserve << "µs\n";
    std::cout << "Insert without reserve: " << time_without_reserve << "µs\n";

    // Just verify both sets have the correct size
    EXPECT_EQ(set_with_reserve.size(), LARGE_SIZE);
    EXPECT_EQ(set_without_reserve.size(), LARGE_SIZE);
}

TEST_F(FlatSetTest, LookupPerformance) {
    auto large_set = generateLargeSet(LARGE_SIZE);

    // Measure lookup time for existing elements
    auto lookup_time = measureExecutionTime([&]() {
        for (int i = 0; i < TEST_ITERATIONS; ++i) {
            int value = i * (LARGE_SIZE / TEST_ITERATIONS);
            auto it = large_set.find(value);
            EXPECT_NE(it, large_set.end());
        }
    });

    // Measure lookup time for non-existing elements
    auto missing_lookup_time = measureExecutionTime([&]() {
        for (int i = 0; i < TEST_ITERATIONS; ++i) {
            int value = LARGE_SIZE + i;
            auto it = large_set.find(value);
            EXPECT_EQ(it, large_set.end());
        }
    });

    std::cout << "Lookup existing elements: " << lookup_time << "µs\n";
    std::cout << "Lookup non-existing elements: " << missing_lookup_time
              << "µs\n";
}

TEST_F(FlatSetTest, BulkInsertPerformance) {
    // Create a large vector of random integers
    std::vector<int> values(LARGE_SIZE);
    for (size_t i = 0; i < LARGE_SIZE; ++i) {
        values[i] = static_cast<int>(i);
    }
    std::random_shuffle(values.begin(), values.end());

    // Measure time for individual inserts
    FlatSet<int> individual_set;
    individual_set.reserve(LARGE_SIZE);

    auto individual_time = measureExecutionTime([&]() {
        for (int val : values) {
            individual_set.insert(val);
        }
    });

    // Measure time for bulk insert
    FlatSet<int> bulk_set;

    auto bulk_time = measureExecutionTime(
        [&]() { bulk_set.insert(values.begin(), values.end()); });

    std::cout << "Individual inserts: " << individual_time << "µs\n";
    std::cout << "Bulk insert: " << bulk_time << "µs\n";

    // Bulk insert should be significantly faster
    EXPECT_LT(bulk_time, individual_time);

    // Both sets should have the same final state
    EXPECT_EQ(individual_set.size(), bulk_set.size());
    EXPECT_TRUE(std::equal(individual_set.begin(), individual_set.end(),
                           bulk_set.begin()));
}

TEST_F(FlatSetTest, MultithreadedAccess) {
    // Shared set for testing
    FlatSet<int> shared_set({1, 2, 3, 4, 5});

    // Launch multiple threads to read from the set simultaneously
    std::vector<std::future<bool>> results;
    for (int i = 0; i < 10; ++i) {
        results.push_back(std::async(std::launch::async, [&shared_set, i]() {
            // Sleep a bit to increase chance of concurrent access
            std::this_thread::sleep_for(std::chrono::milliseconds(i));
            return shared_set.contains(i % 5 + 1);
        }));
    }

    // Collect results
    std::vector<bool> found_values;
    for (auto& result : results) {
        found_values.push_back(result.get());
    }

    // All values 1-5 should be found
    EXPECT_EQ(found_values.size(), 10);
    EXPECT_TRUE(std::all_of(found_values.begin(), found_values.end(),
                            [](bool v) { return v; }));
}
