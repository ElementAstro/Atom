/*
 * test_mock_matchers.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for mock argument matchers in
atom/tests/mocking/mock_matchers.hpp

**************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/tests/mocking/mock_matchers.hpp"

namespace atom::test::mocking::tests {

// ============================================================================
// Wildcard Matcher Tests
// ============================================================================

class WildcardMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(WildcardMatcherTest, MatchesAnyInteger) {
    auto matcher = Any<int>();
    EXPECT_TRUE(matcher.matches(0));
    EXPECT_TRUE(matcher.matches(42));
    EXPECT_TRUE(matcher.matches(-100));
}

TEST_F(WildcardMatcherTest, MatchesAnyString) {
    auto matcher = Any<std::string>();
    EXPECT_TRUE(matcher.matches(""));
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("world"));
}

TEST_F(WildcardMatcherTest, UnderscoreShorthand) {
    // The _ constant can be implicitly converted to MatcherWrapper<T>
    MatcherWrapper<int> matcher = _;
    EXPECT_TRUE(matcher.matches(123));
}

// ============================================================================
// Equality Matcher Tests
// ============================================================================

class EqualityMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EqualityMatcherTest, EqMatcherInteger) {
    auto matcher = Eq(42);
    EXPECT_TRUE(matcher.matches(42));
    EXPECT_FALSE(matcher.matches(43));
}

TEST_F(EqualityMatcherTest, EqMatcherString) {
    auto matcher = Eq(std::string("hello"));
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("world"));
}

TEST_F(EqualityMatcherTest, NeMatcherInteger) {
    auto matcher = Ne(42);
    EXPECT_FALSE(matcher.matches(42));
    EXPECT_TRUE(matcher.matches(43));
}

// ============================================================================
// Comparison Matcher Tests
// ============================================================================

class ComparisonMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComparisonMatcherTest, LtMatcher) {
    auto matcher = Lt(10);
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(15));
}

TEST_F(ComparisonMatcherTest, LeMatcher) {
    auto matcher = Le(10);
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(15));
}

TEST_F(ComparisonMatcherTest, GtMatcher) {
    auto matcher = Gt(10);
    EXPECT_FALSE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(10));
    EXPECT_TRUE(matcher.matches(15));
}

TEST_F(ComparisonMatcherTest, GeMatcher) {
    auto matcher = Ge(10);
    EXPECT_FALSE(matcher.matches(5));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_TRUE(matcher.matches(15));
}

// ============================================================================
// String Matcher Tests
// ============================================================================

class StringMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringMatcherTest, StrEqMatcher) {
    auto matcher = StrEq("hello");
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("Hello"));
    EXPECT_FALSE(matcher.matches("world"));
}

TEST_F(StringMatcherTest, StrNeMatcher) {
    auto matcher = StrNe("hello");
    EXPECT_FALSE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("world"));
}

TEST_F(StringMatcherTest, StrContainsMatcher) {
    auto matcher = HasSubstr("world");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("world"));
    EXPECT_FALSE(matcher.matches("hello"));
}

TEST_F(StringMatcherTest, StrStartsWithMatcher) {
    auto matcher = StartsWith("hello");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("world hello"));
}

TEST_F(StringMatcherTest, StrEndsWithMatcher) {
    auto matcher = EndsWith("world");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("world"));
    EXPECT_FALSE(matcher.matches("world hello"));
}

TEST_F(StringMatcherTest, StrCaseEqMatcher) {
    auto matcher = StrCaseEq("hello");
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("HELLO"));
    EXPECT_TRUE(matcher.matches("HeLLo"));
    EXPECT_FALSE(matcher.matches("world"));
}

// ============================================================================
// Pointer Matcher Tests
// ============================================================================

class PointerMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PointerMatcherTest, IsNullMatcher) {
    auto matcher = IsNull<int*>();
    int* nullPtr = nullptr;
    int value = 42;
    int* validPtr = &value;

    EXPECT_TRUE(matcher.matches(nullPtr));
    EXPECT_FALSE(matcher.matches(validPtr));
}

TEST_F(PointerMatcherTest, NotNullMatcher) {
    auto matcher = NotNull<int*>();
    int* nullPtr = nullptr;
    int value = 42;
    int* validPtr = &value;

    EXPECT_FALSE(matcher.matches(nullPtr));
    EXPECT_TRUE(matcher.matches(validPtr));
}

TEST_F(PointerMatcherTest, PointeeMatcher) {
    auto matcher = Pointee(Eq(42));
    int value = 42;
    int* ptr = &value;

    EXPECT_TRUE(matcher.matches(ptr));

    value = 43;
    EXPECT_FALSE(matcher.matches(ptr));
}

// ============================================================================
// Container Matcher Tests
// ============================================================================

class ContainerMatcherTest : public ::testing::Test {
protected:
    void SetUp() override { vec = {1, 2, 3, 4, 5}; }
    void TearDown() override { vec.clear(); }

    std::vector<int> vec;
};

TEST_F(ContainerMatcherTest, IsEmptyMatcher) {
    auto matcher = IsEmpty<std::vector<int>>();
    std::vector<int> emptyVec;

    EXPECT_TRUE(matcher.matches(emptyVec));
    EXPECT_FALSE(matcher.matches(vec));
}

TEST_F(ContainerMatcherTest, SizeIsMatcher) {
    auto matcher = SizeIs<std::vector<int>>(5);
    EXPECT_TRUE(matcher.matches(vec));

    vec.push_back(6);
    EXPECT_FALSE(matcher.matches(vec));
}

TEST_F(ContainerMatcherTest, ContainsElementMatcher) {
    // Contains returns a lambda, test it directly
    auto containsThree = Contains(3);
    EXPECT_TRUE(containsThree(vec));

    auto containsTen = Contains(10);
    EXPECT_FALSE(containsTen(vec));
}

// ============================================================================
// Predicate Matcher Tests
// ============================================================================

class PredicateMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PredicateMatcherTest, CustomPredicate) {
    auto matcher = Truly([](int x) { return x % 2 == 0; });
    EXPECT_TRUE(matcher.matches(2));
    EXPECT_TRUE(matcher.matches(4));
    EXPECT_FALSE(matcher.matches(3));
}

TEST_F(PredicateMatcherTest, RangePredicate) {
    auto matcher = Truly([](int x) { return x >= 0 && x <= 100; });
    EXPECT_TRUE(matcher.matches(50));
    EXPECT_FALSE(matcher.matches(-1));
    EXPECT_FALSE(matcher.matches(101));
}

// ============================================================================
// Logical Matcher Tests
// ============================================================================

class LogicalMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LogicalMatcherTest, NotMatcher) {
    auto matcher = Not(Eq(5));
    EXPECT_TRUE(matcher.matches(6));
    EXPECT_FALSE(matcher.matches(5));
}

TEST_F(LogicalMatcherTest, AllOfMatcher) {
    auto matcher = AllOf(Gt(0), Lt(10));
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(0));
    EXPECT_FALSE(matcher.matches(10));
}

TEST_F(LogicalMatcherTest, AnyOfMatcher) {
    auto matcher = AnyOf(Eq(1), Eq(5), Eq(10));
    EXPECT_TRUE(matcher.matches(1));
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(7));
}

// ============================================================================
// Field Matcher Tests
// ============================================================================

struct TestStruct {
    int id;
    std::string name;
};

class FieldMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FieldMatcherTest, FieldMatcher) {
    auto matcher = Field(&TestStruct::id, Eq(42));
    TestStruct obj{42, "test"};

    EXPECT_TRUE(matcher.matches(obj));

    obj.id = 43;
    EXPECT_FALSE(matcher.matches(obj));
}

TEST_F(FieldMatcherTest, MultipleFieldMatchers) {
    auto idMatcher = Field(&TestStruct::id, Eq(42));
    auto nameMatcher = Field(&TestStruct::name, StrEq("test"));

    TestStruct obj{42, "test"};

    EXPECT_TRUE(idMatcher.matches(obj));
    EXPECT_TRUE(nameMatcher.matches(obj));
}

// ============================================================================
// Property Matcher Tests
// ============================================================================

class TestClass {
public:
    int getValue() const { return value_; }
    void setValue(int v) { value_ = v; }

private:
    int value_ = 0;
};

class PropertyMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PropertyMatcherTest, PropertyMatcher) {
    auto matcher = Property(&TestClass::getValue, Eq(42));
    TestClass obj;
    obj.setValue(42);

    EXPECT_TRUE(matcher.matches(obj));

    obj.setValue(43);
    EXPECT_FALSE(matcher.matches(obj));
}

// ============================================================================
// Truly (Predicate) Matcher Tests - Additional
// ============================================================================

class TrulyMatcherAdditionalTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TrulyMatcherAdditionalTest, TrulyWithTransformation) {
    // Use Truly to test a transformation result
    auto matcher = Truly([](int x) { return x * 2 == 10; });
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(6));
}

TEST_F(TrulyMatcherAdditionalTest, TrulyWithStringLength) {
    auto matcher = Truly([](const std::string& s) { return s.length() == 5; });
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("hi"));
}

// ============================================================================
// Matcher Description Tests
// ============================================================================

class MatcherDescriptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MatcherDescriptionTest, EqDescription) {
    auto matcher = Eq(42);
    std::string desc = matcher.describe();
    EXPECT_FALSE(desc.empty());
}

TEST_F(MatcherDescriptionTest, AllOfDescription) {
    auto matcher = AllOf(Gt(0), Lt(10));
    std::string desc = matcher.describe();
    EXPECT_FALSE(desc.empty());
}

}  // namespace atom::test::mocking::tests
