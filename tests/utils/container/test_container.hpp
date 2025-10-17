// filepath: /home/max/Atom-1/atom/utils/test_container.hpp
/*
 * test_container.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for container utility functions

**************************************************/

#ifndef ATOM_UTILS_TEST_CONTAINER_HPP
#define ATOM_UTILS_TEST_CONTAINER_HPP

#include <gtest/gtest.h>
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include "atom/utils/container.hpp"

namespace atom::utils::test {

class ContainerTest : public ::testing::Test {
protected:
    std::vector<int> vec1{1, 2, 3, 4, 5};
    std::vector<int> vec2{3, 4, 5, 6, 7};
    std::vector<int> empty{};
    std::vector<int> subset{3, 4};
    std::vector<int> duplicate{1, 2, 2, 3, 3, 3};

    std::list<int> list1{1, 2, 3, 4, 5};

    std::vector<std::vector<int>> nested{{1, 2}, {3, 4}, {5}};

    // For map testing
    std::map<int, std::string> map1{{1, "one"}, {2, "two"}, {3, "three"}};
    std::unordered_map<int, std::string> umap1{
        {1, "one"}, {2, "two"}, {3, "three"}};
};

// Test subset checking functions
TEST_F(ContainerTest, IsSubset) {
    // Basic subset checks
    EXPECT_TRUE(isSubset(subset, vec1));
    EXPECT_FALSE(isSubset(vec1, subset));
    EXPECT_TRUE(isSubset(empty, vec1));  // Empty set is always a subset
    EXPECT_FALSE(isSubset(vec1, vec2));

    // Test with different container types
    EXPECT_TRUE(isSubset(subset, list1));
}

// Test contains function
TEST_F(ContainerTest, Contains) {
    EXPECT_TRUE(contains(vec1, 3));
    EXPECT_FALSE(contains(vec1, 8));
    EXPECT_FALSE(contains(empty, 1));

    // Test with different container types
    EXPECT_TRUE(contains(list1, 3));

    // Test with different value types
    std::vector<std::string> strVec{"apple", "banana", "cherry"};
    EXPECT_TRUE(contains(strVec, std::string("banana")));
    EXPECT_FALSE(contains(strVec, std::string("grape")));
}

// Test toUnorderedSet function
TEST_F(ContainerTest, ToUnorderedSet) {
    auto set1 = toUnorderedSet(vec1);
    EXPECT_EQ(set1.size(), 5);
    EXPECT_TRUE(set1.contains(1));
    EXPECT_TRUE(set1.contains(5));
    EXPECT_FALSE(set1.contains(8));

    // Test with duplicates
    auto setDuplicates = toUnorderedSet(duplicate);
    EXPECT_EQ(setDuplicates.size(), 3);  // Should only have 3 unique elements

    // Test with empty container
    auto emptySet = toUnorderedSet(empty);
    EXPECT_TRUE(emptySet.empty());
}

// Test isSubsetLinearSearch function
TEST_F(ContainerTest, IsSubsetLinearSearch) {
    EXPECT_TRUE(isSubsetLinearSearch(subset, vec1));
    EXPECT_FALSE(isSubsetLinearSearch(vec1, subset));
    EXPECT_TRUE(
        isSubsetLinearSearch(empty, vec1));  // Empty set is always a subset
    EXPECT_FALSE(isSubsetLinearSearch(vec1, vec2));
}

// Test isSubsetWithHashSet function
TEST_F(ContainerTest, IsSubsetWithHashSet) {
    EXPECT_TRUE(isSubsetWithHashSet(subset, vec1));
    EXPECT_FALSE(isSubsetWithHashSet(vec1, subset));
    EXPECT_TRUE(
        isSubsetWithHashSet(empty, vec1));  // Empty set is always a subset
    EXPECT_FALSE(isSubsetWithHashSet(vec1, vec2));
}

// Test set operation functions
TEST_F(ContainerTest, SetOperations) {
    // Test intersection
    auto inter = intersection(vec1, vec2);
    EXPECT_EQ(inter.size(), 3);
    EXPECT_TRUE(contains(inter, 3));
    EXPECT_TRUE(contains(inter, 4));
    EXPECT_TRUE(contains(inter, 5));
    EXPECT_FALSE(contains(inter, 1));

    // Test union
    auto uni = unionSet(vec1, vec2);
    EXPECT_EQ(uni.size(), 7);
    for (int i = 1; i <= 7; ++i) {
        EXPECT_TRUE(contains(uni, i));
    }

    // Test difference
    auto diff1 = difference(vec1, vec2);
    EXPECT_EQ(diff1.size(), 2);
    EXPECT_TRUE(contains(diff1, 1));
    EXPECT_TRUE(contains(diff1, 2));

    auto diff2 = difference(vec2, vec1);
    EXPECT_EQ(diff2.size(), 2);
    EXPECT_TRUE(contains(diff2, 6));
    EXPECT_TRUE(contains(diff2, 7));

    // Test symmetric difference
    auto symDiff = symmetricDifference(vec1, vec2);
    EXPECT_EQ(symDiff.size(), 4);
    EXPECT_TRUE(contains(symDiff, 1));
    EXPECT_TRUE(contains(symDiff, 2));
    EXPECT_TRUE(contains(symDiff, 6));
    EXPECT_TRUE(contains(symDiff, 7));
}

// Test isEqual function
TEST_F(ContainerTest, IsEqual) {
    EXPECT_TRUE(isEqual(vec1, vec1));
    EXPECT_FALSE(isEqual(vec1, vec2));
    EXPECT_TRUE(isEqual(empty, empty));

    std::vector<int> vec1Copy = vec1;
    EXPECT_TRUE(isEqual(vec1, vec1Copy));

    // Test with different container types
    EXPECT_TRUE(isEqual(vec1, list1));

    // Test with reordered elements
    std::vector<int> vec1Shuffled = vec1;
    std::reverse(vec1Shuffled.begin(), vec1Shuffled.end());
    EXPECT_TRUE(isEqual(vec1, vec1Shuffled));
}

// Class for testing member function operations
class TestClass {
public:
    explicit TestClass(int val) : value(val) {}

    int getValue() const { return value; }
    std::string toString() const { return std::to_string(value); }
    bool isEven() const { return value % 2 == 0; }

    int doubleValue() const { return value * 2; }

private:
    int value;
};

// Test applyAndStore function
TEST_F(ContainerTest, ApplyAndStore) {
    std::vector<TestClass> objects{TestClass(1), TestClass(2), TestClass(3)};

    // Test with member function returning int
    auto values = applyAndStore(objects, &TestClass::getValue);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);

    // Test with member function returning string
    auto strings = applyAndStore(objects, &TestClass::toString);
    ASSERT_EQ(strings.size(), 3);
    EXPECT_EQ(strings[0], "1");
    EXPECT_EQ(strings[1], "2");
    EXPECT_EQ(strings[2], "3");
}

// Test transformToVector function
TEST_F(ContainerTest, TransformToVector) {
    std::vector<TestClass> objects{TestClass(1), TestClass(2), TestClass(3)};

    // Test with member function
    auto values = transformToVector(objects, &TestClass::getValue);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);

    // Test with lambda function
    auto doubled = transformToVector(
        objects, [](const TestClass& obj) { return obj.doubleValue(); });
    ASSERT_EQ(doubled.size(), 3);
    EXPECT_EQ(doubled[0], 2);
    EXPECT_EQ(doubled[1], 4);
    EXPECT_EQ(doubled[2], 6);
}

// Test unique function for regular containers
TEST_F(ContainerTest, UniqueForContainers) {
    auto unique1 = unique(duplicate);
    EXPECT_EQ(unique1.size(), 3);  // Should remove duplicates
    EXPECT_TRUE(contains(unique1, 1));
    EXPECT_TRUE(contains(unique1, 2));
    EXPECT_TRUE(contains(unique1, 3));

    // Test with already unique container
    auto unique2 = unique(vec1);
    EXPECT_EQ(unique2.size(), 5);

    // Test with empty container
    auto unique3 = unique(empty);
    EXPECT_TRUE(unique3.empty());
}

/*
TODO: Fix this test
// Test unique function for map containers
TEST_F(ContainerTest, UniqueForMaps) {
    // Create a map with duplicate keys (which will be overwritten in the map)
    std::vector<std::pair<int, std::string>> entries{
        {1, "one"}, {2, "two"}, {1, "ONE"}, {3, "three"}};

    auto uniqueMap = unique(entries);
    EXPECT_EQ(uniqueMap.size(), 3);  // Should have 3 unique keys
    EXPECT_EQ(uniqueMap[1], "ONE");  // Later value should overwrite earlier one
    EXPECT_EQ(uniqueMap[2], "two");
    EXPECT_EQ(uniqueMap[3], "three");
}
*/

// Test flatten function
TEST_F(ContainerTest, Flatten) {
    auto flattened = flatten(nested);
    EXPECT_EQ(flattened.size(), 5);
    EXPECT_EQ(flattened[0], 1);
    EXPECT_EQ(flattened[1], 2);
    EXPECT_EQ(flattened[2], 3);
    EXPECT_EQ(flattened[3], 4);
    EXPECT_EQ(flattened[4], 5);

    // Test with empty outer container
    std::vector<std::vector<int>> emptyOuter;
    auto flattened2 = flatten(emptyOuter);
    EXPECT_TRUE(flattened2.empty());

    // Test with empty inner containers
    std::vector<std::vector<int>> emptyInner{{}, {}, {}};
    auto flattened3 = flatten(emptyInner);
    EXPECT_TRUE(flattened3.empty());
}

// Test zip function
TEST_F(ContainerTest, Zip) {
    std::vector<std::string> words{"apple", "banana", "cherry", "date"};

    auto zipped = zip(vec1, words);
    ASSERT_EQ(zipped.size(), 4);  // Should be min(vec1.size(), words.size())
    EXPECT_EQ(zipped[0].first, 1);
    EXPECT_EQ(zipped[0].second, "apple");
    EXPECT_EQ(zipped[3].first, 4);
    EXPECT_EQ(zipped[3].second, "date");

    // Test with empty container
    auto zippedEmpty = zip(empty, words);
    EXPECT_TRUE(zippedEmpty.empty());

    // Test with different container types
    auto zippedMixed = zip(list1, words);
    ASSERT_EQ(zippedMixed.size(), 4);
    EXPECT_EQ(zippedMixed[0].first, 1);
    EXPECT_EQ(zippedMixed[0].second, "apple");
}

// Test cartesianProduct function
TEST_F(ContainerTest, CartesianProduct) {
    std::vector<char> chars{'A', 'B'};

    auto product = cartesianProduct(subset, chars);
    ASSERT_EQ(product.size(), 4);  // 2 elements * 2 characters

    // Check all combinations are present
    std::vector<std::pair<int, char>> expected{
        {3, 'A'}, {3, 'B'}, {4, 'A'}, {4, 'B'}};

    for (const auto& pair : expected) {
        bool found = false;
        for (const auto& p : product) {
            if (p.first == pair.first && p.second == pair.second) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Pair (" << pair.first << ", " << pair.second
                           << ") not found";
    }

    // Test with empty container
    auto productEmpty = cartesianProduct(empty, chars);
    EXPECT_TRUE(productEmpty.empty());
}

// Test filter function
TEST_F(ContainerTest, Filter) {
    // Filter even numbers
    auto evens = filter(vec1, [](int x) { return x % 2 == 0; });
    ASSERT_EQ(evens.size(), 2);
    EXPECT_EQ(evens[0], 2);
    EXPECT_EQ(evens[1], 4);

    // Test with empty container
    auto filtered = filter(empty, [](int x) { return x > 0; });
    EXPECT_TRUE(filtered.empty());

    // Test with object member function
    std::vector<TestClass> objects{TestClass(1), TestClass(2), TestClass(3),
                                   TestClass(4)};
    auto evenObjects =
        filter(objects, [](const TestClass& obj) { return obj.isEven(); });
    ASSERT_EQ(evenObjects.size(), 2);
    EXPECT_EQ(evenObjects[0].getValue(), 2);
    EXPECT_EQ(evenObjects[1].getValue(), 4);
}

// Test partition function
TEST_F(ContainerTest, Partition) {
    // Partition into even and odd numbers
    auto [even, odd] = partition(vec1, [](int x) { return x % 2 == 0; });

    ASSERT_EQ(even.size(), 2);
    EXPECT_EQ(even[0], 2);
    EXPECT_EQ(even[1], 4);

    ASSERT_EQ(odd.size(), 3);
    EXPECT_EQ(odd[0], 1);
    EXPECT_EQ(odd[1], 3);
    EXPECT_EQ(odd[2], 5);

    // Test with empty container
    auto [emptyEven, emptyOdd] =
        partition(empty, [](int x) { return x % 2 == 0; });
    EXPECT_TRUE(emptyEven.empty());
    EXPECT_TRUE(emptyOdd.empty());
}

// Test findIf function
TEST_F(ContainerTest, FindIf) {
    // Find first even number
    auto firstEven = findIf(vec1, [](int x) { return x % 2 == 0; });
    ASSERT_TRUE(firstEven.has_value());
    EXPECT_EQ(*firstEven, 2);

    // Test with no matching element
    auto noMatch = findIf(vec1, [](int x) { return x > 10; });
    EXPECT_FALSE(noMatch.has_value());

    // Test with empty container
    auto emptyResult = findIf(empty, [](int x) { return x % 2 == 0; });
    EXPECT_FALSE(emptyResult.has_value());
}

// Test string literal operator for creating vectors
TEST_F(ContainerTest, StringLiteralOperator) {
    auto vec = "apple, banana, cherry, date"_vec;
    ASSERT_EQ(vec.size(), 4);
    EXPECT_EQ(vec[0], "apple");
    EXPECT_EQ(vec[1], "banana");
    EXPECT_EQ(vec[2], "cherry");
    EXPECT_EQ(vec[3], "date");

    // Test with extra spaces
    auto vecWithSpaces = "  apple,   banana  ,cherry  ,  date  "_vec;
    ASSERT_EQ(vecWithSpaces.size(), 4);
    EXPECT_EQ(vecWithSpaces[0], "apple");
    EXPECT_EQ(vecWithSpaces[1], "banana");
    EXPECT_EQ(vecWithSpaces[2], "cherry");
    EXPECT_EQ(vecWithSpaces[3], "date");

    // Test with empty inputs
    auto emptyVec = ""_vec;
    EXPECT_TRUE(emptyVec.empty());

    auto emptyElementsVec = ",,"_vec;
    EXPECT_TRUE(emptyElementsVec.empty());

    // Test with single element
    auto singleElementVec = "apple"_vec;
    ASSERT_EQ(singleElementVec.size(), 1);
    EXPECT_EQ(singleElementVec[0], "apple");
}

// Testing with large containers for performance verification
TEST_F(ContainerTest, LargeContainerOperations) {
    // Create larger vectors for operations that might have performance
    // implications
    std::vector<int> large1(1000);
    std::vector<int> large2(1000);
    std::vector<int> largeSubset(100);

    // Fill with ascending values
    std::ranges::iota(large1.begin(), large1.end(), 0);              // 0-999
    std::ranges::iota(large2.begin(), large2.end(), 500);            // 500-1499
    std::ranges::iota(largeSubset.begin(), largeSubset.end(), 500);  // 500-599

    // IsSubset operation with large containers
    EXPECT_TRUE(isSubset(largeSubset, large2));  // Should be a subset
    EXPECT_FALSE(isSubset(large1, large2));      // Should not be a subset

    // Set operations with large containers
    auto largeIntersection = intersection(large1, large2);
    EXPECT_EQ(largeIntersection.size(), 500);  // 500-999 overlap

    auto largeUnion = unionSet(large1, large2);
    EXPECT_EQ(largeUnion.size(), 1500);  // 0-1499 unique values

    // Filter large container
    auto filteredLarge = filter(large1, [](int x) { return x % 7 == 0; });
    EXPECT_EQ(filteredLarge.size(), 143);  // 0, 7, 14, ..., 994 (143 numbers)
}

// Test combinations of operations
TEST_F(ContainerTest, CombinedOperations) {
    // Combine multiple operations to test integration

    // Find unique elements in the symmetric difference
    auto symDiff = symmetricDifference(vec1, vec2);
    auto uniqueSymDiff = unique(symDiff);
    EXPECT_EQ(uniqueSymDiff.size(), 4);  // Should be the same size as symDiff

    // Filter and then find the first element
    auto evenNumbers = filter(vec1, [](int x) { return x % 2 == 0; });
    auto firstEven = findIf(evenNumbers, [](int x) { return x > 3; });
    ASSERT_TRUE(firstEven.has_value());
    EXPECT_EQ(*firstEven, 4);

    // Combine flatten and unique
    std::vector<std::vector<int>> duplicateNested{{1, 2}, {2, 3}, {3, 4}};
    auto flattenedDuplicates = flatten(duplicateNested);
    EXPECT_EQ(flattenedDuplicates.size(),
              6);  // Total elements across nested vectors

    auto uniqueFlattened = unique(flattenedDuplicates);
    EXPECT_EQ(uniqueFlattened.size(), 4);  // Unique elements: 1, 2, 3, 4
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_CONTAINER_HPP
