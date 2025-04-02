// filepath: /home/max/Atom-1/atom/utils/test_linq.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <deque>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "linq.hpp"

using namespace atom::utils;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace atom::utils::tests {

struct Person {
    std::string name;
    int age;

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age;
    }
};

// Define hash function for Person to use in unordered containers
}  // namespace atom::utils::tests

namespace std {
template <>
struct hash<atom::utils::tests::Person> {
    size_t operator()(const atom::utils::tests::Person& p) const {
        return hash<string>()(p.name) ^ hash<int>()(p.age);
    }
};
}  // namespace std

namespace atom::utils::tests {

class LinqTest : public ::testing::Test {
protected:
    void SetUp() override {
        intList = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        stringList = {"apple", "banana", "cherry", "date", "elderberry"};
        emptyList = {};
        duplicatesList = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
        personList = {{"Alice", 25},
                      {"Bob", 30},
                      {"Charlie", 35},
                      {"Dave", 40},
                      {"Eve", 25}};
    }

    std::vector<int> intList;
    std::vector<std::string> stringList;
    std::vector<int> emptyList;
    std::vector<int> duplicatesList;
    std::vector<Person> personList;
};

// Basic enumeration tests
TEST_F(LinqTest, CreateEnumerableFromVector) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.toStdVector();

    EXPECT_EQ(result, intList);
    EXPECT_EQ(result.size(), intList.size());
}

TEST_F(LinqTest, EmptyEnumerable) {
    auto enumerable = Enumerable<int>(emptyList);
    auto result = enumerable.toStdVector();

    EXPECT_TRUE(result.empty());
}

// Where/filter tests
TEST_F(LinqTest, WhereFilter) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable.where([](int i) { return i % 2 == 0; }).toStdVector();

    EXPECT_THAT(result, ElementsAre(2, 4, 6, 8, 10));
}

TEST_F(LinqTest, WhereFilterWithIndex) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable
            .whereI([](int i, size_t idx) { return i % 2 == 0 && idx > 2; })
            .toStdVector();

    EXPECT_THAT(result, ElementsAre(4, 6, 8, 10));
}

TEST_F(LinqTest, WhereFilterWithEmptyResult) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.where([](int i) { return i > 100; }).toStdVector();

    EXPECT_THAT(result, IsEmpty());
}

// Take tests
TEST_F(LinqTest, Take) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.take(3).toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3));
}

TEST_F(LinqTest, TakeMoreThanAvailable) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.take(20).toStdVector();

    EXPECT_EQ(result, intList);
}

TEST_F(LinqTest, TakeZero) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.take(0).toStdVector();

    EXPECT_THAT(result, IsEmpty());
}

TEST_F(LinqTest, TakeWhile) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable.takeWhile([](int i) { return i < 4; }).toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3));
}

TEST_F(LinqTest, TakeWhileWithIndex) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable
            .takeWhileI([](int i, size_t idx) { return i < 5 || idx < 3; })
            .toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3, 4));
}

// Skip tests
TEST_F(LinqTest, Skip) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.skip(3).toStdVector();

    EXPECT_THAT(result, ElementsAre(4, 5, 6, 7, 8, 9, 10));
}

TEST_F(LinqTest, SkipMoreThanAvailable) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.skip(20).toStdVector();

    EXPECT_THAT(result, IsEmpty());
}

TEST_F(LinqTest, SkipZero) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.skip(0).toStdVector();

    EXPECT_EQ(result, intList);
}

TEST_F(LinqTest, SkipWhile) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable.skipWhile([](int i) { return i < 4; }).toStdVector();

    EXPECT_THAT(result, ElementsAre(4, 5, 6, 7, 8, 9, 10));
}

TEST_F(LinqTest, SkipWhileWithIndex) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable
            .skipWhileI([](int i, size_t idx) { return i < 4 && idx < 3; })
            .toStdVector();

    EXPECT_THAT(result, ElementsAre(4, 5, 6, 7, 8, 9, 10));
}

// OrderBy tests
TEST_F(LinqTest, OrderBy) {
    std::vector<int> unordered = {5, 3, 9, 1, 7};
    auto enumerable = Enumerable<int>(unordered);
    auto result = enumerable.orderBy().toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 3, 5, 7, 9));
}

TEST_F(LinqTest, OrderByWithTransformer) {
    auto enumerable = Enumerable<Person>(personList);
    auto result =
        enumerable.orderBy([](const Person& p) { return p.age; }).toStdVector();

    // Expect: Alice(25), Eve(25), Bob(30), Charlie(35), Dave(40)
    EXPECT_EQ(result[0].name, "Alice");
    EXPECT_EQ(result[1].name, "Eve");
    EXPECT_EQ(result[2].name, "Bob");
    EXPECT_EQ(result[3].name, "Charlie");
    EXPECT_EQ(result[4].name, "Dave");
}

TEST_F(LinqTest, OrderByEmpty) {
    auto enumerable = Enumerable<int>(emptyList);
    auto result = enumerable.orderBy().toStdVector();

    EXPECT_THAT(result, IsEmpty());
}

// Distinct tests
TEST_F(LinqTest, Distinct) {
    auto enumerable = Enumerable<int>(duplicatesList);
    auto result = enumerable.distinct().toStdVector();

    // Result should have unique elements in some order
    EXPECT_EQ(result.size(), 4);
    EXPECT_THAT(result, UnorderedElementsAre(1, 2, 3, 4));
}

TEST_F(LinqTest, DistinctWithTransformer) {
    auto enumerable = Enumerable<Person>(personList);
    // Keep only one person per age
    auto result = enumerable.distinct([](const Person& p) { return p.age; })
                      .toStdVector();

    // Should have 4 elements (one age 25 duplicate removed)
    EXPECT_EQ(result.size(), 4);

    // Check that each age appears exactly once
    std::set<int> ages;
    for (const auto& person : result) {
        EXPECT_TRUE(ages.insert(person.age).second);
    }
}

// Append/Prepend/Concat tests
TEST_F(LinqTest, Append) {
    auto enumerable = Enumerable<int>({1, 2, 3});
    auto result = enumerable.append({4, 5}).toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(LinqTest, Prepend) {
    auto enumerable = Enumerable<int>({3, 4, 5});
    auto result = enumerable.prepend({1, 2}).toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(LinqTest, Concat) {
    auto enumerable1 = Enumerable<int>({1, 2, 3});
    auto enumerable2 = Enumerable<int>({4, 5, 6});
    auto result = enumerable1.concat(enumerable2).toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3, 4, 5, 6));
}

// Reverse test
TEST_F(LinqTest, Reverse) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.reverse().toStdVector();

    EXPECT_THAT(result, ElementsAre(10, 9, 8, 7, 6, 5, 4, 3, 2, 1));
}

// Cast test
TEST_F(LinqTest, Cast) {
    std::vector<int> intValues = {1, 2, 3};
    auto enumerable = Enumerable<int>(intValues);
    auto result = enumerable.cast<double>().toStdVector();

    // Check that types have been converted
    EXPECT_TRUE((std::is_same_v<std::vector<double>, decltype(result)>));
    EXPECT_THAT(result, ElementsAre(1.0, 2.0, 3.0));
}

// Select/Transform tests
TEST_F(LinqTest, Select) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable
                      .select<std::string>(
                          [](int i) { return "Item " + std::to_string(i); })
                      .toStdVector();

    EXPECT_THAT(result,
                ElementsAre("Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
                            "Item 6", "Item 7", "Item 8", "Item 9", "Item 10"));
}

TEST_F(LinqTest, SelectWithIndex) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable
                      .selectI<std::string>([](int i, size_t idx) {
                          return "Item " + std::to_string(i) + " at index " +
                                 std::to_string(idx);
                      })
                      .toStdVector();

    EXPECT_EQ(result.size(), intList.size());
    EXPECT_EQ(result[0], "Item 1 at index 0");
    EXPECT_EQ(result[9], "Item 10 at index 9");
}

// GroupBy test
TEST_F(LinqTest, GroupBy) {
    auto enumerable = Enumerable<Person>(personList);
    auto result = enumerable.groupBy<int>([](const Person& p) { return p.age; })
                      .toStdVector();

    // Should have the unique ages: 25, 30, 35, 40
    EXPECT_EQ(result.size(), 4);
    EXPECT_THAT(result, UnorderedElementsAre(25, 30, 35, 40));
}

// SelectMany test
TEST_F(LinqTest, SelectMany) {
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5, 6}};
    auto enumerable = Enumerable<std::vector<int>>(nested);
    auto result =
        enumerable.selectMany<int>([](const std::vector<int>& v) { return v; })
            .toStdVector();

    EXPECT_THAT(result, ElementsAre(1, 2, 3, 4, 5, 6));
}

// Aggregation tests
/*
TODO: Fix these tests
TEST_F(LinqTest, All) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_TRUE(enumerable.all([](int i) { return i > 0; }));
    EXPECT_FALSE(enumerable.all([](int i) { return i > 5; }));
    EXPECT_TRUE(enumerable.all());  // Default predicate returns true
}

TEST_F(LinqTest, Any) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_TRUE(enumerable.any([](int i) { return i > 5; }));
    EXPECT_FALSE(enumerable.any([](int i) { return i > 100; }));
    EXPECT_TRUE(enumerable.any());  // Default predicate returns true

    auto emptyEnumerable = Enumerable<int>(emptyList);
    EXPECT_FALSE(emptyEnumerable.any());
}
*/

TEST_F(LinqTest, Sum) {
    auto enumerable = Enumerable<int>(intList);
    EXPECT_EQ(enumerable.sum(), 55);  // 1+2+3+4+5+6+7+8+9+10 = 55

    // Sum with transformer
    EXPECT_EQ(enumerable.sum<int>([](int i) { return i * 2; }),
              110);  // Sum of doubled values
}

TEST_F(LinqTest, Average) {
    auto enumerable = Enumerable<int>(intList);
    EXPECT_DOUBLE_EQ(enumerable.avg(), 5.5);  // (1+2+3+...+10)/10 = 5.5

    // Average with transformer
    auto doubleAvg = enumerable.avg<double>([](int i) { return i * 2.0; });
    EXPECT_DOUBLE_EQ(doubleAvg, 11.0);  // Average of doubled values
}

TEST_F(LinqTest, Reduce) {
    auto enumerable = Enumerable<std::string>(stringList);
    auto concatenated =
        enumerable.reduce<std::string>("", [](std::string acc, std::string s) {
            return acc.empty() ? s : acc + "," + s;
        });

    EXPECT_EQ(concatenated, "apple,banana,cherry,date,elderberry");
}

TEST_F(LinqTest, MinMax) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_EQ(enumerable.min(), 1);
    EXPECT_EQ(enumerable.max(), 10);

    // Min/Max with transformer
    auto personEnum = Enumerable<Person>(personList);
    auto youngest = personEnum.min([](const Person& p) { return p.age; });
    auto oldest = personEnum.max([](const Person& p) { return p.age; });

    EXPECT_EQ(youngest.age, 25);
    EXPECT_EQ(oldest.age, 40);
}

TEST_F(LinqTest, Count) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_EQ(enumerable.count(), 10);
    EXPECT_EQ(enumerable.count([](int i) { return i % 2 == 0; }),
              5);  // Count of even numbers
}

TEST_F(LinqTest, Contains) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_TRUE(enumerable.contains(5));
    EXPECT_FALSE(enumerable.contains(11));
}

TEST_F(LinqTest, ElementAt) {
    auto enumerable = Enumerable<std::string>(stringList);

    EXPECT_EQ(enumerable.elementAt(0), "apple");
    EXPECT_EQ(enumerable.elementAt(4), "elderberry");

    // Check exception for out of bounds
    EXPECT_THROW(enumerable.elementAt(10), std::out_of_range);
}

// First/Last tests
TEST_F(LinqTest, First) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_EQ(enumerable.first(), 1);
    EXPECT_EQ(enumerable.first([](int i) { return i > 5; }), 6);

    // Default value when not found
    EXPECT_EQ(enumerable.first([](int i) { return i > 100; }), 0);
}

TEST_F(LinqTest, FirstOrDefault) {
    auto enumerable = Enumerable<int>(intList);

    auto first = enumerable.firstOrDefault();
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), 1);

    auto firstGreaterThan5 =
        enumerable.firstOrDefault([](int i) { return i > 5; });
    EXPECT_TRUE(firstGreaterThan5.has_value());
    EXPECT_EQ(firstGreaterThan5.value(), 6);

    auto notFound = enumerable.firstOrDefault([](int i) { return i > 100; });
    EXPECT_FALSE(notFound.has_value());

    auto emptyEnum = Enumerable<int>(emptyList);
    EXPECT_FALSE(emptyEnum.firstOrDefault().has_value());
}

TEST_F(LinqTest, Last) {
    auto enumerable = Enumerable<int>(intList);

    EXPECT_EQ(enumerable.last(), 10);
    EXPECT_EQ(enumerable.last([](int i) { return i < 5; }), 4);

    // Default value when not found
    EXPECT_EQ(enumerable.last([](int i) { return i > 100; }), 0);
}

TEST_F(LinqTest, LastOrDefault) {
    auto enumerable = Enumerable<int>(intList);

    auto last = enumerable.lastOrDefault();
    EXPECT_TRUE(last.has_value());
    EXPECT_EQ(last.value(), 10);

    auto lastLessThan5 = enumerable.lastOrDefault([](int i) { return i < 5; });
    EXPECT_TRUE(lastLessThan5.has_value());
    EXPECT_EQ(lastLessThan5.value(), 4);

    auto notFound = enumerable.lastOrDefault([](int i) { return i > 100; });
    EXPECT_FALSE(notFound.has_value());

    auto emptyEnum = Enumerable<int>(emptyList);
    EXPECT_FALSE(emptyEnum.lastOrDefault().has_value());
}

// Conversion method tests
TEST_F(LinqTest, ToStdSet) {
    auto enumerable = Enumerable<int>(duplicatesList);
    auto result = enumerable.toStdSet();

    EXPECT_EQ(result.size(), 4);  // Only unique elements
    EXPECT_TRUE(result.contains(1));
    EXPECT_TRUE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
    EXPECT_TRUE(result.contains(4));
}

TEST_F(LinqTest, ToStdList) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.toStdList();

    EXPECT_EQ(result.size(), intList.size());
    EXPECT_EQ(*result.begin(), 1);
    EXPECT_EQ(*std::prev(result.end()), 10);
}

TEST_F(LinqTest, ToStdDeque) {
    auto enumerable = Enumerable<int>(intList);
    auto result = enumerable.toStdDeque();

    EXPECT_EQ(result.size(), intList.size());
    EXPECT_EQ(result.front(), 1);
    EXPECT_EQ(result.back(), 10);
}

// Method chaining tests
TEST_F(LinqTest, MethodChaining) {
    auto enumerable = Enumerable<int>(intList);
    auto result =
        enumerable
            .where([](int i) {
                return i % 2 == 0;
            })  // Filter even numbers: 2, 4, 6, 8, 10
            .select<int>(
                [](int i) { return i * i; })  // Square them: 4, 16, 36, 64, 100
            .where([](int i) { return i > 30; })  // Filter > 30: 36, 64, 100
            .toStdVector();

    EXPECT_THAT(result, ElementsAre(36, 64, 100));
}

TEST_F(LinqTest, ComplexChaining) {
    auto enumerable = Enumerable<Person>(personList);
    auto result =
        enumerable
            .where(
                [](const Person& p) { return p.age < 40; })   // Filter out Dave
            .orderBy([](const Person& p) { return p.name; })  // Order by name
            .select<std::string>(
                [](const Person& p) {  // Extract formatted string
                    return p.name + " (" + std::to_string(p.age) + ")";
                })
            .take(3)  // Take first 3
            .toStdVector();

    EXPECT_THAT(result, ElementsAre("Alice (25)", "Bob (30)", "Charlie (35)"));
}

// Edge case tests
TEST_F(LinqTest, EmptyEnumerableOperations) {
    auto enumerable = Enumerable<int>(emptyList);

    // No elements should be processed
    auto whereResult = enumerable.where([](int) { return true; }).toStdVector();
    EXPECT_TRUE(whereResult.empty());

    // Should return empty result
    auto selectResult =
        enumerable.select<std::string>([](int i) { return std::to_string(i); })
            .toStdVector();
    EXPECT_TRUE(selectResult.empty());

    // Min/Max on empty collection should throw
    EXPECT_THROW(enumerable.min(), std::runtime_error);
    EXPECT_THROW(enumerable.max(), std::runtime_error);

    // Average on empty collection should return NaN or throw
    EXPECT_THROW(enumerable.avg(), std::runtime_error);

    // Sum on empty collection should return default
    EXPECT_EQ(enumerable.sum(), 0);
}

/*
TEST_F(LinqTest, NullPredicateHandling) {
    auto enumerable = Enumerable<int>(intList);

    // Default predicates should work
    EXPECT_TRUE(enumerable.all());
    EXPECT_TRUE(enumerable.any());
}
*/

}  // namespace atom::utils::tests