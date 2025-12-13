/*
 * test_matchers.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for GTest-style matchers in atom/tests/assertions/matchers.hpp

**************************************************/

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "atom/tests/assertions/matchers.hpp"

namespace atom::test::assertions::tests {

// ============================================================================
// Basic Comparison Matchers Tests
// ============================================================================

class ComparisonMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComparisonMatchersTest, EqMatcher) {
    auto matcher = Eq(5);
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(6));
}

TEST_F(ComparisonMatchersTest, NeMatcher) {
    auto matcher = Ne(5);
    EXPECT_TRUE(matcher.matches(6));
    EXPECT_FALSE(matcher.matches(5));
}

TEST_F(ComparisonMatchersTest, LtMatcher) {
    auto matcher = Lt(10);
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(15));
}

TEST_F(ComparisonMatchersTest, LeMatcher) {
    auto matcher = Le(10);
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(15));
}

TEST_F(ComparisonMatchersTest, GtMatcher) {
    auto matcher = Gt(10);
    EXPECT_TRUE(matcher.matches(15));
    EXPECT_FALSE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(5));
}

TEST_F(ComparisonMatchersTest, GeMatcher) {
    auto matcher = Ge(10);
    EXPECT_TRUE(matcher.matches(15));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(5));
}

// ============================================================================
// Floating Point Matchers Tests
// ============================================================================

class FloatMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FloatMatchersTest, FloatEqMatcher) {
    auto matcher = FloatEq(3.14f);
    EXPECT_TRUE(matcher.matches(3.14f));
    EXPECT_FALSE(matcher.matches(3.15f));
}

TEST_F(FloatMatchersTest, DoubleEqMatcher) {
    auto matcher = DoubleEq(3.14159);
    EXPECT_TRUE(matcher.matches(3.14159));
    EXPECT_FALSE(matcher.matches(3.14));
}

TEST_F(FloatMatchersTest, FloatNearMatcher) {
    auto matcher = FloatNear(3.14f, 0.01f);
    EXPECT_TRUE(matcher.matches(3.14f));
    EXPECT_TRUE(matcher.matches(3.145f));
    EXPECT_FALSE(matcher.matches(3.2f));
}

TEST_F(FloatMatchersTest, DoubleNearMatcher) {
    auto matcher = DoubleNear(3.14159, 0.001);
    EXPECT_TRUE(matcher.matches(3.14159));
    EXPECT_TRUE(matcher.matches(3.1416));
    EXPECT_FALSE(matcher.matches(3.15));
}

TEST_F(FloatMatchersTest, NanValueMatcher) {
    auto matcher = IsNan<double>();
    EXPECT_TRUE(matcher.matches(std::nan("")));
    EXPECT_FALSE(matcher.matches(3.14));
}

TEST_F(FloatMatchersTest, InfValueMatcher) {
    auto matcher = IsInf<double>();
    EXPECT_TRUE(matcher.matches(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(matcher.matches(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(matcher.matches(3.14));
}

// ============================================================================
// Pointer Matchers Tests
// ============================================================================

class PointerMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PointerMatchersTest, IsNullMatcher) {
    auto matcher = IsNull<int*>();
    int* nullPtr = nullptr;
    int value = 42;
    int* validPtr = &value;

    EXPECT_TRUE(matcher.matches(nullPtr));
    EXPECT_FALSE(matcher.matches(validPtr));
}

TEST_F(PointerMatchersTest, NotNullMatcher) {
    auto matcher = NotNull<int*>();
    int* nullPtr = nullptr;
    int value = 42;
    int* validPtr = &value;

    EXPECT_FALSE(matcher.matches(nullPtr));
    EXPECT_TRUE(matcher.matches(validPtr));
}

TEST_F(PointerMatchersTest, PointeeMatcher) {
    auto matcher = Pointee(Eq(42));
    int value = 42;
    int* ptr = &value;

    EXPECT_TRUE(matcher.matches(ptr));

    value = 43;
    EXPECT_FALSE(matcher.matches(ptr));
}

// ============================================================================
// String Matchers Tests
// ============================================================================

class StringMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringMatchersTest, HasSubstrMatcher) {
    auto matcher = HasSubstr("world");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("world"));
    EXPECT_FALSE(matcher.matches("hello"));
}

TEST_F(StringMatchersTest, StartsWithMatcher) {
    auto matcher = StartsWith("hello");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("world hello"));
}

TEST_F(StringMatchersTest, EndsWithMatcher) {
    auto matcher = EndsWith("world");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("world"));
    EXPECT_FALSE(matcher.matches("world hello"));
}

TEST_F(StringMatchersTest, StrEqMatcher) {
    auto matcher = StrEq("hello");
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("Hello"));
    EXPECT_FALSE(matcher.matches("hello "));
}

TEST_F(StringMatchersTest, StrNeMatcher) {
    auto matcher = StrNe("hello");
    EXPECT_FALSE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("world"));
}

TEST_F(StringMatchersTest, StrCaseEqMatcher) {
    auto matcher = StrCaseEq("hello");
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("HELLO"));
    EXPECT_TRUE(matcher.matches("HeLLo"));
    EXPECT_FALSE(matcher.matches("world"));
}

TEST_F(StringMatchersTest, MatchesRegexMatcher) {
    auto matcher = MatchesRegex("[a-z]+[0-9]+");
    EXPECT_TRUE(matcher.matches("test123"));
    EXPECT_TRUE(matcher.matches("abc456"));
    EXPECT_FALSE(matcher.matches("123test"));
}

TEST_F(StringMatchersTest, ContainsRegexMatcher) {
    auto matcher = ContainsRegex("[0-9]+");
    EXPECT_TRUE(matcher.matches("test123test"));
    EXPECT_TRUE(matcher.matches("456"));
    EXPECT_FALSE(matcher.matches("nodigits"));
}

// ============================================================================
// Container Matchers Tests
// ============================================================================

class ContainerMatchersTest : public ::testing::Test {
protected:
    void SetUp() override { vec = {1, 2, 3, 4, 5}; }
    void TearDown() override { vec.clear(); }

    std::vector<int> vec;
};

TEST_F(ContainerMatchersTest, IsEmptyMatcher) {
    auto matcher = IsEmpty<std::vector<int>>();
    std::vector<int> emptyVec;

    EXPECT_TRUE(matcher.matches(emptyVec));
    EXPECT_FALSE(matcher.matches(vec));
}

TEST_F(ContainerMatchersTest, SizeIsMatcher) {
    auto matcher = SizeIs(5);
    EXPECT_TRUE(matcher.matches(vec));

    vec.push_back(6);
    EXPECT_FALSE(matcher.matches(vec));
}

TEST_F(ContainerMatchersTest, ContainsMatcher) {
    auto matcher = Contains(3);
    EXPECT_TRUE(matcher.matches(vec));

    auto matcher2 = Contains(10);
    EXPECT_FALSE(matcher2.matches(vec));
}

TEST_F(ContainerMatchersTest, EachMatcher) {
    auto matcher = Each(Gt(0));
    EXPECT_TRUE(matcher.matches(vec));

    auto matcher2 = Each(Lt(3));
    EXPECT_FALSE(matcher2.matches(vec));
}

TEST_F(ContainerMatchersTest, ElementsAreMatcher) {
    auto matcher = ElementsAre(1, 2, 3, 4, 5);
    EXPECT_TRUE(matcher.matches(vec));

    auto matcher2 = ElementsAre(1, 2, 3);
    EXPECT_FALSE(matcher2.matches(vec));
}

TEST_F(ContainerMatchersTest, UnorderedElementsAreMatcher) {
    auto matcher = UnorderedElementsAre(5, 4, 3, 2, 1);
    EXPECT_TRUE(matcher.matches(vec));
}

TEST_F(ContainerMatchersTest, ContainerEqMatcher) {
    std::vector<int> expected = {1, 2, 3, 4, 5};
    auto matcher = ContainerEq(expected);
    EXPECT_TRUE(matcher.matches(vec));

    expected.push_back(6);
    EXPECT_FALSE(matcher.matches(vec));
}

// ============================================================================
// Logical Matchers Tests
// ============================================================================

class LogicalMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LogicalMatchersTest, NotMatcher) {
    auto matcher = Not(Eq(5));
    EXPECT_TRUE(matcher.matches(6));
    EXPECT_FALSE(matcher.matches(5));
}

TEST_F(LogicalMatchersTest, AllOfMatcher) {
    auto matcher = AllOf(Gt(0), Lt(10), Ne(5));
    EXPECT_TRUE(matcher.matches(3));
    EXPECT_FALSE(matcher.matches(5));
    EXPECT_FALSE(matcher.matches(15));
}

TEST_F(LogicalMatchersTest, AnyOfMatcher) {
    auto matcher = AnyOf(Eq(1), Eq(5), Eq(10));
    EXPECT_TRUE(matcher.matches(1));
    EXPECT_TRUE(matcher.matches(5));
    EXPECT_TRUE(matcher.matches(10));
    EXPECT_FALSE(matcher.matches(7));
}

// ============================================================================
// Optional Matchers Tests
// ============================================================================

class OptionalMatchersTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(OptionalMatchersTest, OptionalMatcher) {
    auto matcher = Optional(Eq(42));
    std::optional<int> opt = 42;

    EXPECT_TRUE(matcher.matches(opt));

    opt = 43;
    EXPECT_FALSE(matcher.matches(opt));

    opt = std::nullopt;
    EXPECT_FALSE(matcher.matches(opt));
}

// ============================================================================
// Wildcard Matcher Tests
// ============================================================================

class WildcardMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(WildcardMatcherTest, AnyValueMatcher) {
    auto matcher = _;
    EXPECT_TRUE(matcher.matches(42));
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches(3.14));
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
    auto matcher = Eq(5);
    std::string desc = matcher.describe();
    EXPECT_FALSE(desc.empty());
}

TEST_F(MatcherDescriptionTest, HasSubstrDescription) {
    auto matcher = HasSubstr("test");
    std::string desc = matcher.describe();
    EXPECT_FALSE(desc.empty());
}

TEST_F(MatcherDescriptionTest, AllOfDescription) {
    auto matcher = AllOf(Gt(0), Lt(10));
    std::string desc = matcher.describe();
    EXPECT_FALSE(desc.empty());
}

}  // namespace atom::test::assertions::tests
