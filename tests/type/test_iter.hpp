#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <list>
#include <string>
#include <vector>

#include "atom/type/iter.hpp"

class IteratorTest : public ::testing::Test {
protected:
    std::vector<int> intVector{1, 2, 3, 4, 5};
    std::vector<std::string> stringVector{"one", "two", "three", "four",
                                          "five"};
    std::list<int> intList{10, 20, 30, 40, 50};
};

// PointerIterator Tests
TEST_F(IteratorTest, PointerIteratorBasic) {
    auto ptrRange = makePointerRange(intVector.begin(), intVector.end());

    // Test dereferencing the pointer iterator
    EXPECT_EQ(**ptrRange.first, 1);

    // Test incrementing the iterator
    auto it = ptrRange.first;
    ++it;
    EXPECT_EQ(**it, 2);

    // Test post-increment
    auto it2 = it++;
    EXPECT_EQ(**it, 3);
    EXPECT_EQ(**it2, 2);

    // Test equality/inequality
    EXPECT_NE(it, ptrRange.first);
    EXPECT_NE(it, ptrRange.second);
}

TEST_F(IteratorTest, PointerIteratorTraversal) {
    auto ptrRange = makePointerRange(intVector.begin(), intVector.end());

    std::vector<int*> pointers;
    for (auto it = ptrRange.first; it != ptrRange.second; ++it) {
        pointers.push_back(*it);
    }

    ASSERT_EQ(pointers.size(), intVector.size());
    for (size_t i = 0; i < pointers.size(); ++i) {
        EXPECT_EQ(*pointers[i], intVector[i]);
    }
}

TEST_F(IteratorTest, PointerIteratorModification) {
    auto ptrRange = makePointerRange(intVector.begin(), intVector.end());

    // Modify values through the pointers
    for (auto it = ptrRange.first; it != ptrRange.second; ++it) {
        int* ptr = *it;
        *ptr *= 2;
    }

    // Verify the changes in the original container
    EXPECT_EQ(intVector[0], 2);
    EXPECT_EQ(intVector[1], 4);
    EXPECT_EQ(intVector[2], 6);
    EXPECT_EQ(intVector[3], 8);
    EXPECT_EQ(intVector[4], 10);
}

// ProcessContainer Tests
TEST_F(IteratorTest, ProcessContainerBasic) {
    std::vector<int> v{1, 2, 3, 4, 5};

    // Process container should remove elements, keeping only the first and last
    processContainer(v);

    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 5);
}

TEST_F(IteratorTest, ProcessContainerWithStrings) {
    std::vector<std::string> v{"first", "second", "third", "fourth", "fifth"};

    // Process container should remove elements, keeping only the first and last
    processContainer(v);

    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0], "first");
    EXPECT_EQ(v[1], "fifth");
}

TEST_F(IteratorTest, ProcessContainerWithList) {
    std::list<int> l{100, 200, 300, 400, 500};

    // Process container should remove elements, keeping only the first and last
    processContainer(l);

    ASSERT_EQ(l.size(), 2);
    EXPECT_EQ(*l.begin(), 100);
    EXPECT_EQ(*std::next(l.begin()), 500);
}

// EarlyIncIterator Tests
TEST_F(IteratorTest, EarlyIncIteratorBasic) {
    auto it = makeEarlyIncIterator(intVector.begin());
    auto end = makeEarlyIncIterator(intVector.end());

    EXPECT_EQ(*it, 1);

    // Test pre-increment
    ++it;
    EXPECT_EQ(*it, 2);

    // Test post-increment
    auto oldIt = it++;
    EXPECT_EQ(*oldIt, 2);
    EXPECT_EQ(*it, 3);

    // Test equality/inequality
    EXPECT_NE(it, end);

    // Advance to the end
    ++it;
    ++it;
    EXPECT_EQ(*it, 5);
    ++it;
    EXPECT_EQ(it, end);
}

TEST_F(IteratorTest, EarlyIncIteratorTraversal) {
    auto it = makeEarlyIncIterator(intVector.begin());
    auto end = makeEarlyIncIterator(intVector.end());

    std::vector<int> values;
    for (; it != end; ++it) {
        values.push_back(*it);
    }

    ASSERT_EQ(values.size(), intVector.size());
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(values[i], intVector[i]);
    }
}

// TransformIterator Tests
TEST_F(IteratorTest, TransformIteratorIntToDouble) {
    auto square = [](int x) -> double { return x * x; };
    auto it = makeTransformIterator(intVector.begin(), square);
    auto end = makeTransformIterator(intVector.end(), square);

    // Test dereferencing
    EXPECT_DOUBLE_EQ(*it, 1.0);

    // Test increment
    ++it;
    EXPECT_DOUBLE_EQ(*it, 4.0);

    // Test post-increment
    auto oldIt = it++;
    EXPECT_DOUBLE_EQ(*oldIt, 4.0);
    EXPECT_DOUBLE_EQ(*it, 9.0);

    // Test equality/inequality
    EXPECT_NE(it, end);

    // Collect all transformed values
    std::vector<double> results;
    it = makeTransformIterator(intVector.begin(), square);
    for (; it != end; ++it) {
        results.push_back(*it);
    }

    ASSERT_EQ(results.size(), intVector.size());
    EXPECT_DOUBLE_EQ(results[0], 1.0);
    EXPECT_DOUBLE_EQ(results[1], 4.0);
    EXPECT_DOUBLE_EQ(results[2], 9.0);
    EXPECT_DOUBLE_EQ(results[3], 16.0);
    EXPECT_DOUBLE_EQ(results[4], 25.0);
}

TEST_F(IteratorTest, TransformIteratorStringToLength) {
    auto length = [](const std::string& s) -> size_t { return s.length(); };
    auto it = makeTransformIterator(stringVector.begin(), length);
    auto end = makeTransformIterator(stringVector.end(), length);

    // Test first element transformation
    EXPECT_EQ(*it, 3);  // "one" has length 3

    // Collect all string lengths
    std::vector<size_t> lengths;
    for (; it != end; ++it) {
        lengths.push_back(*it);
    }

    ASSERT_EQ(lengths.size(), stringVector.size());
    EXPECT_EQ(lengths[0], 3);  // "one"
    EXPECT_EQ(lengths[1], 3);  // "two"
    EXPECT_EQ(lengths[2], 5);  // "three"
    EXPECT_EQ(lengths[3], 4);  // "four"
    EXPECT_EQ(lengths[4], 4);  // "five"
}

TEST_F(IteratorTest, TransformIteratorComplex) {
    struct Person {
        std::string name;
        int age;
    };

    std::vector<Person> people{{"Alice", 30}, {"Bob", 25}, {"Charlie", 40}};

    // Transform to get just the names
    auto getName = [](const Person& p) -> std::string { return p.name; };
    auto it = makeTransformIterator(people.begin(), getName);
    auto end = makeTransformIterator(people.end(), getName);

    std::vector<std::string> names;
    for (; it != end; ++it) {
        names.push_back(*it);
    }

    ASSERT_EQ(names.size(), people.size());
    EXPECT_EQ(names[0], "Alice");
    EXPECT_EQ(names[1], "Bob");
    EXPECT_EQ(names[2], "Charlie");
}

// FilterIterator Tests
TEST_F(IteratorTest, FilterIteratorBasic) {
    // Filter for even numbers
    auto isEven = [](int x) { return x % 2 == 0; };
    auto it = makeFilterIterator(intVector.begin(), intVector.end(), isEven);
    auto end = makeFilterIterator(intVector.end(), intVector.end(), isEven);

    // Collect filtered values
    std::vector<int> filtered;
    for (; it != end; ++it) {
        filtered.push_back(*it);
    }

    ASSERT_EQ(filtered.size(), 2);
    EXPECT_EQ(filtered[0], 2);
    EXPECT_EQ(filtered[1], 4);
}

TEST_F(IteratorTest, FilterIteratorEmpty) {
    // Filter that matches nothing
    auto isNegative = [](int x) { return x < 0; };
    auto it =
        makeFilterIterator(intVector.begin(), intVector.end(), isNegative);
    auto end = makeFilterIterator(intVector.end(), intVector.end(), isNegative);

    // Should not iterate at all
    int count = 0;
    for (; it != end; ++it) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST_F(IteratorTest, FilterIteratorAll) {
    // Filter that matches everything
    auto isPositive = [](int x) { return x > 0; };
    auto it =
        makeFilterIterator(intVector.begin(), intVector.end(), isPositive);
    auto end = makeFilterIterator(intVector.end(), intVector.end(), isPositive);

    std::vector<int> filtered;
    for (; it != end; ++it) {
        filtered.push_back(*it);
    }

    ASSERT_EQ(filtered.size(), intVector.size());
    for (size_t i = 0; i < filtered.size(); ++i) {
        EXPECT_EQ(filtered[i], intVector[i]);
    }
}

TEST_F(IteratorTest, FilterIteratorStringLength) {
    // Filter strings longer than 3 characters
    auto isLong = [](const std::string& s) { return s.length() > 3; };
    auto it =
        makeFilterIterator(stringVector.begin(), stringVector.end(), isLong);
    auto end =
        makeFilterIterator(stringVector.end(), stringVector.end(), isLong);

    std::vector<std::string> filtered;
    for (; it != end; ++it) {
        filtered.push_back(*it);
    }

    ASSERT_EQ(filtered.size(), 3);
    EXPECT_EQ(filtered[0], "three");
    EXPECT_EQ(filtered[1], "four");
    EXPECT_EQ(filtered[2], "five");
}

// ReverseIterator Tests
TEST_F(IteratorTest, ReverseIteratorBasic) {
    ReverseIterator<std::vector<int>::iterator> rbegin(intVector.end());
    ReverseIterator<std::vector<int>::iterator> rend(intVector.begin());

    // Test dereferencing at start (should be the last element)
    EXPECT_EQ(*rbegin, 5);

    // Test incrementing (which moves backward in the original container)
    ++rbegin;
    EXPECT_EQ(*rbegin, 4);

    // Test post-increment
    auto oldIt = rbegin++;
    EXPECT_EQ(*oldIt, 4);
    EXPECT_EQ(*rbegin, 3);

    // Test equality/inequality
    EXPECT_NE(rbegin, rend);

    // Test traversal
    std::vector<int> reversed;
    for (auto it = rbegin; it != rend; ++it) {
        reversed.push_back(*it);
    }

    ASSERT_EQ(reversed.size(), 3);  // We've already incremented past 5 and 4
    EXPECT_EQ(reversed[0], 3);
    EXPECT_EQ(reversed[1], 2);
    EXPECT_EQ(reversed[2], 1);
}

TEST_F(IteratorTest, ReverseIteratorFullTraversal) {
    ReverseIterator<std::vector<int>::iterator> rbegin(intVector.end());
    ReverseIterator<std::vector<int>::iterator> rend(intVector.begin());

    std::vector<int> reversed;
    for (auto it = rbegin; it != rend; ++it) {
        reversed.push_back(*it);
    }

    ASSERT_EQ(reversed.size(), intVector.size());
    for (size_t i = 0; i < reversed.size(); ++i) {
        EXPECT_EQ(reversed[i], intVector[intVector.size() - 1 - i]);
    }
}

TEST_F(IteratorTest, ReverseIteratorDecrement) {
    ReverseIterator<std::vector<int>::iterator> rbegin(intVector.end());
    ReverseIterator<std::vector<int>::iterator> rend(intVector.begin());

    // Move to the middle
    ++rbegin;
    ++rbegin;  // Now pointing at 3

    // Test decrement (which moves forward in the original container)
    --rbegin;  // Should now point at 4
    EXPECT_EQ(*rbegin, 4);

    // Test post-decrement
    auto oldIt = rbegin--;
    EXPECT_EQ(*oldIt, 4);
    EXPECT_EQ(*rbegin, 5);
}

// ZipIterator Tests
TEST_F(IteratorTest, ZipIteratorBasic) {
    std::vector<int> vec1{1, 2, 3};
    std::vector<std::string> vec2{"one", "two", "three"};

    auto zip_begin = makeZipIterator(vec1.begin(), vec2.begin());
    auto zip_end = makeZipIterator(vec1.end(), vec2.end());

    // Test dereferencing
    auto first = *zip_begin;
    EXPECT_EQ(std::get<0>(first), 1);
    EXPECT_EQ(std::get<1>(first), "one");

    // Test incrementing
    ++zip_begin;
    auto second = *zip_begin;
    EXPECT_EQ(std::get<0>(second), 2);
    EXPECT_EQ(std::get<1>(second), "two");

    // Test post-increment
    auto oldIt = zip_begin++;
    auto old_val = *oldIt;
    auto new_val = *zip_begin;
    EXPECT_EQ(std::get<0>(old_val), 2);
    EXPECT_EQ(std::get<1>(old_val), "two");
    EXPECT_EQ(std::get<0>(new_val), 3);
    EXPECT_EQ(std::get<1>(new_val), "three");

    // Test equality/inequality
    EXPECT_NE(zip_begin, zip_end);
    ++zip_begin;
    EXPECT_EQ(zip_begin, zip_end);
}

TEST_F(IteratorTest, ZipIteratorDifferentLengths) {
    std::vector<int> vec1{1, 2, 3, 4, 5};
    std::vector<std::string> vec2{"one", "two", "three"};

    auto zip_begin = makeZipIterator(vec1.begin(), vec2.begin());
    auto zip_end = makeZipIterator(vec1.end(), vec2.end());

    // Count iterations (should stop at the shorter container's length)
    int count = 0;
    for (auto it = zip_begin; it != zip_end; ++it) {
        ++count;
    }

    // This should be 5, but note this is a bit of a gotcha with ZipIterator -
    // it doesn't stop at the shorter sequence naturally. In real code, we'd
    // want to create an end iterator using the shorter sequence's end.
    EXPECT_EQ(count, 5);
}

TEST_F(IteratorTest, ZipIteratorThreeContainers) {
    std::vector<int> vec1{1, 2, 3};
    std::vector<std::string> vec2{"one", "two", "three"};
    std::vector<double> vec3{1.1, 2.2, 3.3};

    auto zip_begin = makeZipIterator(vec1.begin(), vec2.begin(), vec3.begin());
    auto zip_end = makeZipIterator(vec1.end(), vec2.end(), vec3.end());

    std::vector<std::tuple<int, std::string, double>> result;
    for (auto it = zip_begin; it != zip_end; ++it) {
        result.push_back(*it);
    }

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(std::get<0>(result[0]), 1);
    EXPECT_EQ(std::get<1>(result[0]), "one");
    EXPECT_DOUBLE_EQ(std::get<2>(result[0]), 1.1);

    EXPECT_EQ(std::get<0>(result[1]), 2);
    EXPECT_EQ(std::get<1>(result[1]), "two");
    EXPECT_DOUBLE_EQ(std::get<2>(result[1]), 2.2);

    EXPECT_EQ(std::get<0>(result[2]), 3);
    EXPECT_EQ(std::get<1>(result[2]), "three");
    EXPECT_DOUBLE_EQ(std::get<2>(result[2]), 3.3);
}

// Integration Tests - Combining Multiple Iterator Types
TEST_F(IteratorTest, CombiningIterators) {
    // Filter even numbers, then transform by squaring
    auto isEven = [](int x) { return x % 2 == 0; };
    auto square = [](int x) -> int { return x * x; };

    auto filter_begin =
        makeFilterIterator(intVector.begin(), intVector.end(), isEven);
    auto filter_end =
        makeFilterIterator(intVector.end(), intVector.end(), isEven);

    auto transform_begin = makeTransformIterator(filter_begin, square);
    auto transform_end = makeTransformIterator(filter_end, square);

    std::vector<int> result;
    for (auto it = transform_begin; it != transform_end; ++it) {
        result.push_back(*it);
    }

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 4);   // 2^2
    EXPECT_EQ(result[1], 16);  // 4^2
}

TEST_F(IteratorTest, IteratorChain) {
    // Complex test: filter even numbers, transform by squaring, then point to
    // the results
    std::vector<int> numbers{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto isEven = [](int x) { return x % 2 == 0; };
    auto square = [](int x) -> int { return x * x; };

    // Step 1: Filter even numbers
    auto filter_begin =
        makeFilterIterator(numbers.begin(), numbers.end(), isEven);
    auto filter_end = makeFilterIterator(numbers.end(), numbers.end(), isEven);

    // Step 2: Square the filtered numbers
    auto transform_begin = makeTransformIterator(filter_begin, square);
    auto transform_end = makeTransformIterator(filter_end, square);

    // Step 3: Create a new vector from transformed values
    std::vector<int> transformed;
    for (auto it = transform_begin; it != transform_end; ++it) {
        transformed.push_back(*it);
    }

    // Step 4: Use PointerIterator to get pointers to the results
    auto ptr_range = makePointerRange(transformed.begin(), transformed.end());

    std::vector<int*> pointers;
    for (auto it = ptr_range.first; it != ptr_range.second; ++it) {
        pointers.push_back(*it);
    }

    // Verify results
    ASSERT_EQ(pointers.size(), 5);  // 5 even numbers in 1-10
    EXPECT_EQ(*pointers[0], 4);     // 2^2
    EXPECT_EQ(*pointers[1], 16);    // 4^2
    EXPECT_EQ(*pointers[2], 36);    // 6^2
    EXPECT_EQ(*pointers[3], 64);    // 8^2
    EXPECT_EQ(*pointers[4], 100);   // 10^2

    // Modify through pointers and check
    *pointers[0] = 1000;
    EXPECT_EQ(transformed[0], 1000);
}

// Edge Case Tests
TEST_F(IteratorTest, EmptyContainer) {
    std::vector<int> empty;

    // PointerIterator
    auto ptr_range = makePointerRange(empty.begin(), empty.end());
    EXPECT_EQ(ptr_range.first, ptr_range.second);

    // TransformIterator
    auto square = [](int x) -> int { return x * x; };
    auto transform_begin = makeTransformIterator(empty.begin(), square);
    auto transform_end = makeTransformIterator(empty.end(), square);
    EXPECT_EQ(transform_begin, transform_end);

    // FilterIterator
    auto isEven = [](int x) { return x % 2 == 0; };
    auto filter_begin = makeFilterIterator(empty.begin(), empty.end(), isEven);
    auto filter_end = makeFilterIterator(empty.end(), empty.end(), isEven);
    EXPECT_EQ(filter_begin, filter_end);

    // ZipIterator
    std::vector<std::string> emptyStrings;
    auto zip_begin = makeZipIterator(empty.begin(), emptyStrings.begin());
    auto zip_end = makeZipIterator(empty.end(), emptyStrings.end());
    EXPECT_EQ(zip_begin, zip_end);
}

TEST_F(IteratorTest, SingleElementContainer) {
    std::vector<int> single{42};

    // PointerIterator
    auto ptr_range = makePointerRange(single.begin(), single.end());
    EXPECT_NE(ptr_range.first, ptr_range.second);
    EXPECT_EQ(**ptr_range.first, 42);

    // FilterIterator with match
    auto isEven = [](int x) { return x % 2 == 0; };
    auto filter_begin =
        makeFilterIterator(single.begin(), single.end(), isEven);
    auto filter_end = makeFilterIterator(single.end(), single.end(), isEven);
    EXPECT_NE(filter_begin, filter_end);
    EXPECT_EQ(*filter_begin, 42);

    // FilterIterator with no match
    auto isOdd = [](int x) { return x % 2 != 0; };
    auto filter_begin2 =
        makeFilterIterator(single.begin(), single.end(), isOdd);
    auto filter_end2 = makeFilterIterator(single.end(), single.end(), isOdd);
    EXPECT_EQ(filter_begin2, filter_end2);
}

// Performance Test (optional, might be disabled in CI)
TEST_F(IteratorTest, DISABLED_LargeContainer) {
    constexpr int SIZE = 1000000;
    std::vector<int> large(SIZE);
    for (int i = 0; i < SIZE; ++i) {
        large[i] = i;
    }

    auto isEven = [](int x) { return x % 2 == 0; };
    auto square = [](int x) -> int { return x * x; };

    // Measure time for FilterIterator
    auto start = std::chrono::high_resolution_clock::now();
    auto filter_begin = makeFilterIterator(large.begin(), large.end(), isEven);
    auto filter_end = makeFilterIterator(large.end(), large.end(), isEven);
    int count = 0;
    for (auto it = filter_begin; it != filter_end; ++it) {
        count++;
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "FilterIterator on " << SIZE
              << " elements: " << elapsed.count() << "s, found " << count
              << " elements" << std::endl;

    EXPECT_EQ(count, SIZE / 2);
}
