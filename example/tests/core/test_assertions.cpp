/**
 * @file test_assertions.cpp
 * @brief Test all assertion macros in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

using namespace atom::test;

// ============================================================================
// Basic Equality Assertions
// ============================================================================

TEST(Assertions, ExpectEq) {
    expect_eq(1, 1);
    expect_eq(3.14, 3.14);
    expect_eq(std::string("hello"), std::string("hello"));
}

TEST(Assertions, ExpectNe) {
    expect_ne(1, 2);
    expect_ne(3.14, 2.71);
    expect_ne(std::string("hello"), std::string("world"));
}

TEST(Assertions, ExpectGt) {
    expect_gt(5, 3);
    expect_gt(3.14, 2.71);
}

TEST(Assertions, ExpectLt) {
    expect_lt(3, 5);
    expect_lt(2.71, 3.14);
}

TEST(Assertions, ExpectGe) {
    expect_ge(5, 3);
    expect_ge(5, 5);
}

TEST(Assertions, ExpectLe) {
    expect_le(3, 5);
    expect_le(5, 5);
}

// ============================================================================
// Boolean Assertions
// ============================================================================

TEST(Assertions, ExpectTrue) {
    expect_true(true);
    expect_true(1 == 1);
    expect_true(5 > 3);
}

TEST(Assertions, ExpectFalse) {
    expect_false(false);
    expect_false(1 == 2);
    expect_false(3 > 5);
}

// ============================================================================
// Pointer Assertions
// ============================================================================

TEST(Assertions, ExpectNull) {
    int* nullPtr = nullptr;
    expect_null(nullPtr);
}

TEST(Assertions, ExpectNotNull) {
    int value = 42;
    int* ptr = &value;
    expect_not_null(ptr);
}

// ============================================================================
// Floating Point Assertions
// ============================================================================

TEST(Assertions, ExpectApprox) {
    expect_approx(3.14159, 3.14160, 0.001);
    expect_approx(1.0 / 3.0, 0.333333, 0.0001);
}

TEST(Assertions, ExpectNear) {
    expect_near(3.14, 3.15, 0.02);
    expect_near(100.0, 100.5, 1.0);
}

TEST(Assertions, ExpectInRange) {
    expect_in_range(5, 1, 10);
    expect_in_range(1, 1, 10);
    expect_in_range(10, 1, 10);
}

// ============================================================================
// String Assertions
// ============================================================================

TEST(Assertions, ExpectContains) {
    expect_contains("hello world", "world");
    expect_contains("testing framework", "frame");
}

TEST(Assertions, ExpectStartsWith) {
    expect_starts_with("hello world", "hello");
    expect_starts_with("testing", "test");
}

TEST(Assertions, ExpectEndsWith) {
    expect_ends_with("hello world", "world");
    expect_ends_with("testing", "ing");
}

TEST(Assertions, ExpectMatches) {
    expect_matches("hello123", "[a-z]+[0-9]+");
    expect_matches("test@example.com", ".*@.*\\.com");
}

// ============================================================================
// Container Assertions
// ============================================================================

TEST(Assertions, ExpectEmpty) {
    std::vector<int> emptyVec;
    expect_empty(emptyVec);
    
    std::string emptyStr;
    expect_empty(emptyStr);
}

TEST(Assertions, ExpectNotEmpty) {
    std::vector<int> vec = {1, 2, 3};
    expect_not_empty(vec);
    
    std::string str = "hello";
    expect_not_empty(str);
}

TEST(Assertions, ExpectSize) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    expect_size(vec, 5);
    
    std::string str = "hello";
    expect_size(str, 5);
}

TEST(Assertions, ExpectAllOf) {
    std::vector<int> vec = {2, 4, 6, 8, 10};
    expect_all_of(vec, [](int x) { return x % 2 == 0; });
}

TEST(Assertions, ExpectAnyOf) {
    std::vector<int> vec = {1, 3, 5, 6, 7};
    expect_any_of(vec, [](int x) { return x % 2 == 0; });
}

TEST(Assertions, ExpectNoneOf) {
    std::vector<int> vec = {1, 3, 5, 7, 9};
    expect_none_of(vec, [](int x) { return x % 2 == 0; });
}

TEST(Assertions, ExpectSorted) {
    std::vector<int> sortedVec = {1, 2, 3, 4, 5};
    expect_sorted(sortedVec);
}

TEST(Assertions, ExpectUnique) {
    std::vector<int> uniqueVec = {1, 2, 3, 4, 5};
    expect_unique(uniqueVec);
}

TEST(Assertions, ExpectContainsElement) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    expect_contains_element(vec, 3);
}

// ============================================================================
// Exception Assertions
// ============================================================================

TEST(Assertions, ExpectThrows) {
    expect_throws([]() { throw std::runtime_error("error"); });
}

TEST(Assertions, ExpectThrowsWithMessage) {
    expect_throws_with_message(
        []() { throw std::runtime_error("specific error message"); },
        "specific error");
}

TEST(Assertions, ExpectNoThrow) {
    expect_no_throw([]() {
        int x = 1 + 1;
        (void)x;
    });
}

// ============================================================================
// Set Assertions
// ============================================================================

TEST(Assertions, ExpectSetEq) {
    std::set<int> set1 = {1, 2, 3};
    std::set<int> set2 = {3, 2, 1};
    expect_set_eq(set1, set2);
}

// ============================================================================
// Custom Predicate Assertions
// ============================================================================

TEST(Assertions, ExpectThat) {
    int value = 42;
    expect_that(value, [](int x) { return x > 0 && x < 100; },
                "Value should be between 0 and 100");
}

// ============================================================================
// FAIL and SUCCEED
// ============================================================================

TEST(Assertions, SucceedMacro) {
    SUCCEED();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
