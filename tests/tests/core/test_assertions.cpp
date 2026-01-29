/*
 * test_assertions.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for core assertion macros in atom/tests/core/test.hpp

**************************************************/

#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test::core::tests {

// ============================================================================
// Equality Assertions Tests
// ============================================================================

class EqualityAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EqualityAssertionsTest, ExpectEqIntegers) {
    int a = 5, b = 5;
    // Test that equal integers pass
    EXPECT_NO_THROW({
        if (a != b) {
            throw std::runtime_error("Values not equal");
        }
    });
}

TEST_F(EqualityAssertionsTest, ExpectEqStrings) {
    std::string s1 = "hello";
    std::string s2 = "hello";
    EXPECT_EQ(s1, s2);
}

TEST_F(EqualityAssertionsTest, ExpectNeIntegers) {
    int a = 5, b = 10;
    EXPECT_NE(a, b);
}

TEST_F(EqualityAssertionsTest, ExpectNeStrings) {
    std::string s1 = "hello";
    std::string s2 = "world";
    EXPECT_NE(s1, s2);
}

// ============================================================================
// Comparison Assertions Tests
// ============================================================================

class ComparisonAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComparisonAssertionsTest, ExpectGt) {
    EXPECT_GT(10, 5);
    EXPECT_GT(3.14, 2.71);
}

TEST_F(ComparisonAssertionsTest, ExpectLt) {
    EXPECT_LT(5, 10);
    EXPECT_LT(2.71, 3.14);
}

TEST_F(ComparisonAssertionsTest, ExpectGe) {
    EXPECT_GE(10, 5);
    EXPECT_GE(5, 5);
}

TEST_F(ComparisonAssertionsTest, ExpectLe) {
    EXPECT_LE(5, 10);
    EXPECT_LE(5, 5);
}

// ============================================================================
// Boolean Assertions Tests
// ============================================================================

class BooleanAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BooleanAssertionsTest, ExpectTrue) {
    EXPECT_TRUE(true);
    EXPECT_TRUE(1 == 1);
    EXPECT_TRUE(5 > 3);
}

TEST_F(BooleanAssertionsTest, ExpectFalse) {
    EXPECT_FALSE(false);
    EXPECT_FALSE(1 == 2);
    EXPECT_FALSE(3 > 5);
}

// ============================================================================
// Pointer Assertions Tests
// ============================================================================

class PointerAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PointerAssertionsTest, ExpectNull) {
    int* nullPtr = nullptr;
    EXPECT_EQ(nullPtr, nullptr);
}

TEST_F(PointerAssertionsTest, ExpectNotNull) {
    int value = 42;
    int* ptr = &value;
    EXPECT_NE(ptr, nullptr);
}

// ============================================================================
// Floating Point Assertions Tests
// ============================================================================

class FloatingPointAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FloatingPointAssertionsTest, ExpectNear) {
    EXPECT_NEAR(3.14159, 3.14160, 0.001);
    EXPECT_NEAR(1.0 / 3.0, 0.333333, 0.0001);
}

TEST_F(FloatingPointAssertionsTest, ExpectFloatEq) {
    float a = 1.0f / 3.0f;
    float b = 1.0f / 3.0f;
    EXPECT_FLOAT_EQ(a, b);
}

TEST_F(FloatingPointAssertionsTest, ExpectDoubleEq) {
    double a = 1.0 / 3.0;
    double b = 1.0 / 3.0;
    EXPECT_DOUBLE_EQ(a, b);
}

// ============================================================================
// String Assertions Tests
// ============================================================================

class StringAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringAssertionsTest, StringContains) {
    std::string str = "hello world";
    EXPECT_NE(str.find("world"), std::string::npos);
}

TEST_F(StringAssertionsTest, StringStartsWith) {
    std::string str = "hello world";
    EXPECT_EQ(str.substr(0, 5), "hello");
}

TEST_F(StringAssertionsTest, StringEndsWith) {
    std::string str = "hello world";
    EXPECT_EQ(str.substr(str.length() - 5), "world");
}

// ============================================================================
// Container Assertions Tests
// ============================================================================

class ContainerAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override { vec = {1, 2, 3, 4, 5}; }
    void TearDown() override { vec.clear(); }

    std::vector<int> vec;
};

TEST_F(ContainerAssertionsTest, ExpectEmpty) {
    std::vector<int> emptyVec;
    EXPECT_TRUE(emptyVec.empty());
}

TEST_F(ContainerAssertionsTest, ExpectNotEmpty) { EXPECT_FALSE(vec.empty()); }

TEST_F(ContainerAssertionsTest, ExpectSize) { EXPECT_EQ(vec.size(), 5); }

TEST_F(ContainerAssertionsTest, ExpectContainsElement) {
    auto it = std::find(vec.begin(), vec.end(), 3);
    EXPECT_NE(it, vec.end());
}

TEST_F(ContainerAssertionsTest, ExpectSorted) {
    EXPECT_TRUE(std::is_sorted(vec.begin(), vec.end()));
}

// ============================================================================
// Exception Assertions Tests
// ============================================================================

class ExceptionAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExceptionAssertionsTest, ExpectThrows) {
    EXPECT_THROW(throw std::runtime_error("error"), std::runtime_error);
}

TEST_F(ExceptionAssertionsTest, ExpectThrowsAny) {
    EXPECT_ANY_THROW(throw std::runtime_error("error"));
}

TEST_F(ExceptionAssertionsTest, ExpectNoThrow) {
    EXPECT_NO_THROW({
        int x = 1 + 1;
        (void)x;
    });
}

// ============================================================================
// Predicate Assertions Tests
// ============================================================================

class PredicateAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PredicateAssertionsTest, ExpectAllOf) {
    std::vector<int> evens = {2, 4, 6, 8, 10};
    EXPECT_TRUE(std::all_of(evens.begin(), evens.end(),
                            [](int x) { return x % 2 == 0; }));
}

TEST_F(PredicateAssertionsTest, ExpectAnyOf) {
    std::vector<int> mixed = {1, 2, 3, 4, 5};
    EXPECT_TRUE(std::any_of(mixed.begin(), mixed.end(),
                            [](int x) { return x % 2 == 0; }));
}

TEST_F(PredicateAssertionsTest, ExpectNoneOf) {
    std::vector<int> odds = {1, 3, 5, 7, 9};
    EXPECT_TRUE(std::none_of(odds.begin(), odds.end(),
                             [](int x) { return x % 2 == 0; }));
}

// ============================================================================
// Set Equality Tests
// ============================================================================

class SetAssertionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SetAssertionsTest, ExpectSetEq) {
    std::set<int> set1 = {1, 2, 3};
    std::set<int> set2 = {3, 2, 1};
    EXPECT_EQ(set1, set2);
}

TEST_F(SetAssertionsTest, ExpectSetNe) {
    std::set<int> set1 = {1, 2, 3};
    std::set<int> set2 = {1, 2, 4};
    EXPECT_NE(set1, set2);
}

}  // namespace atom::test::core::tests
