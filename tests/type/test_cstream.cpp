#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "atom/type/cstream.hpp"

using namespace atom::type;

class CStreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test vectors
        vec = {1, 2, 3, 4, 5};
        str_vec = {"hello", "world", "test", "cpp", "stream"};

        // Initialize test lists
        lst = {10, 20, 30, 40, 50};

        // Initialize test maps
        pairs = {{1, "one"}, {2, "two"}, {3, "three"}};
    }

    std::vector<int> vec;
    std::vector<std::string> str_vec;
    std::list<int> lst;
    std::vector<std::pair<int, std::string>> pairs;
};

// Test constructors and basic accessors
TEST_F(CStreamTest, ConstructorsAndAccessors) {
    // Test lvalue reference constructor
    cstream<std::vector<int>> stream1(vec);
    EXPECT_EQ(stream1.size(), 5);

    // Test rvalue constructor
    std::vector<int> temp_vec = {6, 7, 8};
    cstream<std::vector<int>> stream2(std::move(temp_vec));
    EXPECT_EQ(stream2.size(), 3);

    // Test getRef
    auto& ref = stream1.getRef();
    EXPECT_EQ(&ref, &vec);

    // Test get (copy)
    auto copy = stream1.get();
    EXPECT_EQ(copy, vec);
    EXPECT_NE(&copy, &vec);

    // Test getMove
    std::vector<int> move_vec = {9, 10, 11};
    cstream<std::vector<int>> stream3(move_vec);
    auto moved = stream3.getMove();
    EXPECT_TRUE(stream3.get().empty());  // Container should be moved out
    EXPECT_EQ(moved, std::vector<int>({9, 10, 11}));

    // Test conversion operator
    std::vector<int> another_vec = {12, 13, 14};
    cstream<std::vector<int>> stream4(another_vec);
    std::vector<int> explicit_move = static_cast<std::vector<int>&&>(stream4);
    EXPECT_TRUE(stream4.get().empty());  // Container should be moved out
    EXPECT_EQ(explicit_move, std::vector<int>({12, 13, 14}));
}

// Test sorting operations
TEST_F(CStreamTest, Sorting) {
    // Test default sort
    std::vector<int> unsorted = {5, 3, 1, 4, 2};
    cstream<std::vector<int>> stream(unsorted);
    stream.sorted();

    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(stream.get(), expected);

    // Test custom sort
    std::vector<int> custom_unsorted = {1, 2, 3, 4, 5};
    cstream<std::vector<int>> custom_stream(custom_unsorted);
    custom_stream.sorted(std::greater<int>());

    std::vector<int> custom_expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(custom_stream.get(), custom_expected);

    // Test string sort
    std::vector<std::string> str_unsorted = {"banana", "apple", "cherry"};
    cstream<std::vector<std::string>> str_stream(str_unsorted);
    str_stream.sorted();

    std::vector<std::string> str_expected = {"apple", "banana", "cherry"};
    EXPECT_EQ(str_stream.get(), str_expected);
}

// Test transformation operations
TEST_F(CStreamTest, Transform) {
    // Transform integers to strings
    cstream<std::vector<int>> stream(vec);
    auto transformed = stream.transform<std::vector<std::string>>(
        [](int i) { return "num" + std::to_string(i); });

    std::vector<std::string> expected = {"num1", "num2", "num3", "num4",
                                         "num5"};
    EXPECT_EQ(transformed.get(), expected);

    // Transform with mathematical operation
    auto doubled =
        stream.transform<std::vector<int>>([](int i) { return i * 2; });

    std::vector<int> doubled_expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(doubled.get(), doubled_expected);

    // Transform to different container type
    auto to_list =
        stream.transform<std::list<int>>([](int i) { return i + 100; });

    std::list<int> list_expected = {101, 102, 103, 104, 105};
    EXPECT_EQ(to_list.get(), list_expected);
}

// Test remove and erase operations
TEST_F(CStreamTest, RemoveAndErase) {
    // Test remove with predicate
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    cstream<std::vector<int>> stream(nums);
    stream.remove([](int i) { return i % 2 == 0; });  // Remove even numbers

    std::vector<int> expected = {1, 3, 5, 7, 9};
    EXPECT_EQ(stream.get(), expected);

    // Test erase with value - this requires a container with erase method for
    // values
    std::vector<int> values = {1, 2, 3, 4, 5};
    std::map<int, std::string> map_data = {
        {1, "one"}, {2, "two"}, {3, "three"}};
    cstream<std::map<int, std::string>> map_stream(map_data);
    map_stream.erase(2);  // Remove entry with key 2

    std::map<int, std::string> map_expected = {{1, "one"}, {3, "three"}};
    EXPECT_EQ(map_stream.get(), map_expected);
}

// Test filter operations
TEST_F(CStreamTest, Filter) {
    // Test filter (modifies the stream)
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    cstream<std::vector<int>> stream(nums);
    stream.filter([](int i) { return i % 2 == 0; });  // Keep even numbers

    std::vector<int> expected = {2, 4, 6, 8, 10};
    EXPECT_EQ(stream.get(), expected);

    // Test cpFilter (creates a copy)
    std::vector<int> more_nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    cstream<std::vector<int>> cp_stream(more_nums);
    auto filtered =
        cp_stream.cpFilter([](int i) { return i > 5; });  // Keep numbers > 5

    std::vector<int> filtered_expected = {6, 7, 8, 9, 10};
    EXPECT_EQ(filtered.get(), filtered_expected);
    EXPECT_EQ(cp_stream.get(), more_nums);  // Original should be unchanged
}

// Test accumulation operations
TEST_F(CStreamTest, Accumulate) {
    // Test default accumulate (sum)
    cstream<std::vector<int>> stream(vec);
    int sum = stream.accumulate();
    EXPECT_EQ(sum, 15);  // 1+2+3+4+5 = 15

    // Test accumulate with custom function and initial value
    int product = stream.accumulate(1, std::multiplies<int>());
    EXPECT_EQ(product, 120);  // 1*1*2*3*4*5 = 120

    // Test accumulate with lambda
    int sum_squared =
        stream.accumulate(0, [](int acc, int val) { return acc + val * val; });
    EXPECT_EQ(sum_squared, 55);  // 1²+2²+3²+4²+5² = 55

    // Test accumulate with strings
    cstream<std::vector<std::string>> str_stream(str_vec);
    std::string concat = str_stream.accumulate(
        std::string(), [](const std::string& acc, const std::string& val) {
            return acc.empty() ? val : acc + "," + val;
        });
    EXPECT_EQ(concat, "hello,world,test,cpp,stream");
}

// Test forEach, all, any, none operations
TEST_F(CStreamTest, IterationAndPredicates) {
    // Test forEach
    std::vector<int> data = {1, 2, 3, 4, 5};
    cstream<std::vector<int>> stream(data);
    int sum = 0;
    stream.forEach([&sum](int val) { sum += val; });
    EXPECT_EQ(sum, 15);

    // Test all
    bool all_positive = stream.all([](int val) { return val > 0; });
    bool all_even = stream.all([](int val) { return val % 2 == 0; });
    EXPECT_TRUE(all_positive);
    EXPECT_FALSE(all_even);

    // Test any
    bool any_even = stream.any([](int val) { return val % 2 == 0; });
    bool any_negative = stream.any([](int val) { return val < 0; });
    EXPECT_TRUE(any_even);
    EXPECT_FALSE(any_negative);

    // Test none
    bool none_zero = stream.none([](int val) { return val == 0; });
    bool none_odd = stream.none([](int val) { return val % 2 != 0; });
    EXPECT_TRUE(none_zero);
    EXPECT_FALSE(none_odd);
}

// Test copy, size, count operations
TEST_F(CStreamTest, CopyAndCount) {
    // Test copy
    cstream<std::vector<int>> stream(vec);
    auto copied = stream.copy();

    // Modify original to prove copy is separate
    stream.getRef().push_back(6);

    EXPECT_EQ(copied.size(), 5);
    EXPECT_EQ(stream.size(), 6);

    // Test count with predicate
    int even_count = stream.count([](int val) { return val % 2 == 0; });
    EXPECT_EQ(even_count, 3);  // 2, 4, 6

    // Test count with value
    std::vector<int> with_dupes = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
    cstream<std::vector<int>> dupe_stream(with_dupes);
    int count_3 = dupe_stream.count(3);
    EXPECT_EQ(count_3, 3);
}

// Test contains, min, max, mean operations
TEST_F(CStreamTest, AggregationOperations) {
    // Test contains
    cstream<std::vector<int>> stream(vec);
    EXPECT_TRUE(stream.contains(3));
    EXPECT_FALSE(stream.contains(10));

    // Test min
    int min_val = stream.min();
    EXPECT_EQ(min_val, 1);

    // Test max
    int max_val = stream.max();
    EXPECT_EQ(max_val, 5);

    // Test mean
    double mean_val = stream.mean();
    EXPECT_DOUBLE_EQ(mean_val, 3.0);  // (1+2+3+4+5)/5 = 3.0

    // Edge case - single element
    std::vector<int> single = {42};
    cstream<std::vector<int>> single_stream(single);
    EXPECT_EQ(single_stream.min(), 42);
    EXPECT_EQ(single_stream.max(), 42);
    EXPECT_DOUBLE_EQ(single_stream.mean(), 42.0);
}

// Test first operations
TEST_F(CStreamTest, FirstOperations) {
    // Test first
    cstream<std::vector<int>> stream(vec);
    auto first_val = stream.first();
    EXPECT_TRUE(first_val.has_value());
    EXPECT_EQ(*first_val, 1);

    // Test first with predicate
    auto first_even = stream.first([](int val) { return val % 2 == 0; });
    EXPECT_TRUE(first_even.has_value());
    EXPECT_EQ(*first_even, 2);

    // Test first with predicate that matches nothing
    auto first_negative = stream.first([](int val) { return val < 0; });
    EXPECT_FALSE(first_negative.has_value());

    // Test first on empty container
    std::vector<int> empty;
    cstream<std::vector<int>> empty_stream(empty);
    EXPECT_FALSE(empty_stream.first().has_value());
}

// Test map, flatMap operations
TEST_F(CStreamTest, MapOperations) {
    // Test map
    cstream<std::vector<int>> stream(vec);
    auto mapped = stream.map([](int val) { return val * val; });

    std::vector<int> expected = {1, 4, 9, 16, 25};
    EXPECT_EQ(mapped.get(), expected);

    // Test flatMap
    std::vector<int> data = {1, 2, 3};
    cstream<std::vector<int>> flat_stream(data);
    auto flat_mapped = flat_stream.flatMap([](int val) {
        return std::vector<int>(
            val, val);  // Create a vector with 'val' copies of 'val'
    });

    std::vector<int> flat_expected = {1, 2, 2, 3, 3, 3};
    EXPECT_EQ(flat_mapped.get(), flat_expected);
}

// Test distinct and reverse operations
TEST_F(CStreamTest, DistinctAndReverse) {
    // Test distinct
    std::vector<int> with_dupes = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5};
    cstream<std::vector<int>> dupe_stream(with_dupes);
    dupe_stream.distinct();

    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(dupe_stream.get(), expected);

    // Test reverse
    cstream<std::vector<int>> rev_stream(vec);
    rev_stream.reverse();

    std::vector<int> rev_expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(rev_stream.get(), rev_expected);
}

// Test ContainerAccumulate
TEST_F(CStreamTest, ContainerAccumulate) {
    std::vector<int> vec1 = {1, 2, 3};
    std::vector<int> vec2 = {4, 5, 6};

    ContainerAccumulate<std::vector<int>> accumulator;
    accumulator(vec1, vec2);

    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(vec1, expected);
}

// Test JoinAccumulate
TEST_F(CStreamTest, JoinAccumulate) {
    std::string str1 = "Hello";
    std::string str2 = "World";

    JoinAccumulate<std::string> joiner{", "};
    auto result = joiner(str1, str2);

    EXPECT_EQ(result, "Hello, World");

    // Test with empty destination
    std::string empty;
    auto result2 = joiner(empty, str2);
    EXPECT_EQ(result2, "World");  // No separator when first string is empty
}

// Test Pair utility
TEST_F(CStreamTest, PairUtility) {
    std::pair<int, std::string> p{42, "answer"};

    EXPECT_EQ((Pair<int, std::string>::first(p)), 42);
    EXPECT_EQ((Pair<int, std::string>::second(p)), "answer");
}

// Test identity functor
TEST_F(CStreamTest, IdentityFunctor) {
    identity<int> id_int;
    EXPECT_EQ(id_int(42), 42);

    identity<std::string> id_str;
    EXPECT_EQ(id_str("test"), "test");
}

// Test makeStream functions
TEST_F(CStreamTest, MakeStreamFunctions) {
    // Test makeStream with lvalue
    std::vector<int> lvalue = {1, 2, 3};
    auto stream1 = makeStream(lvalue);
    EXPECT_EQ(stream1.get(), lvalue);

    // Test makeStream with rvalue
    auto stream2 = makeStream(std::vector<int>{4, 5, 6});
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(stream2.get(), expected);

    // Test makeStreamCopy
    std::vector<int> original = {7, 8, 9};
    auto stream3 = makeStreamCopy(original);

    // Modify original to prove copy is separate
    original.push_back(10);

    std::vector<int> copy_expected = {7, 8, 9};
    EXPECT_EQ(stream3.get(), copy_expected);
}

// Test cpstream function
TEST_F(CStreamTest, CpStreamFunction) {
    int arr[] = {1, 2, 3, 4, 5};
    auto stream = cpstream<int>(arr, 5);

    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(stream.get(), expected);

    // Test with custom type conversion
    double darr[] = {1.1, 2.2, 3.3};
    auto int_stream = cpstream<int, double>(darr, 3);

    std::vector<int> int_expected = {1, 2, 3};  // Truncated to int
    EXPECT_EQ(int_stream.get(), int_expected);
}

// Test chained operations
TEST_F(CStreamTest, ChainedOperations) {
    std::vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};

    // Chain multiple operations
    auto result =
        makeStream(data)
            .filter([](int val) { return val % 2 == 1; })  // Keep odd numbers
            .copy()                                        // Make a copy
            .distinct()                                    // Remove duplicates
            .sorted()                                      // Sort ascending
            .get();  // Get the final container

    std::vector<int> expected = {1, 3, 5, 9};
    EXPECT_EQ(result, expected);

    // Another chain with transformation
    auto transformed =
        makeStream(data)
            .map([](int val) { return val * 2; })     // Double each value
            .filter([](int val) { return val > 5; })  // Keep values > 5
            .sorted(std::greater<int>())              // Sort descending
            .get();

    std::vector<int> transform_expected = {18, 12, 10, 10, 8, 6};
    EXPECT_EQ(transformed, transform_expected);
}

// Test edge cases
TEST_F(CStreamTest, EdgeCases) {
    // Empty container
    std::vector<int> empty;
    cstream<std::vector<int>> empty_stream(empty);

    EXPECT_EQ(empty_stream.size(), 0);
    EXPECT_FALSE(empty_stream.first().has_value());
    EXPECT_FALSE(empty_stream.any([](int) { return true; }));
    EXPECT_TRUE(empty_stream.all([](int) { return false; }));  // Vacuously true
    EXPECT_TRUE(empty_stream.none([](int) { return true; }));

    // Single element container
    std::vector<int> single = {42};
    cstream<std::vector<int>> single_stream(single);

    EXPECT_EQ(single_stream.size(), 1);
    EXPECT_EQ(*single_stream.first(), 42);
    EXPECT_EQ(single_stream.min(), 42);
    EXPECT_EQ(single_stream.max(), 42);
    EXPECT_DOUBLE_EQ(single_stream.mean(), 42.0);

    // Exception cases
    EXPECT_THROW(
        {
            // Trying to get min/max of empty container
            empty_stream.min();
        },
        std::runtime_error);

    EXPECT_THROW({ empty_stream.max(); }, std::runtime_error);

    EXPECT_THROW(
        {
            empty_stream.mean();  // Division by zero
        },
        std::runtime_error);
}
