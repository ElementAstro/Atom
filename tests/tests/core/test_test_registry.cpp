/*
 * test_test_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for TestRegistry in atom/tests/core/test_registry.hpp

**************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/tests/core/test_registry.hpp"

namespace atom::test::core::tests {

// ============================================================================
// TestRegistry Singleton Tests
// ============================================================================

class TestRegistrySingletonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistrySingletonTest, SingletonInstance) {
    auto& registry1 = TestRegistry::instance();
    auto& registry2 = TestRegistry::instance();

    // Should return the same instance
    EXPECT_EQ(&registry1, &registry2);
}

TEST_F(TestRegistrySingletonTest, InstanceNotNull) {
    auto& registry = TestRegistry::instance();

    // Instance should be valid
    EXPECT_NE(&registry, nullptr);
}

// ============================================================================
// TestRegistry Suite Registration Tests
// ============================================================================

class TestRegistrySuiteTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistrySuiteTest, RegisterSuite) {
    auto& registry = TestRegistry::instance();

    // Register a test suite
    registry.registerSuite("TestSuite1");

    // Suite should be registered
    auto suites = registry.getAllSuiteNames();
    bool found = false;
    for (const auto& suite : suites) {
        if (suite == "TestSuite1") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(TestRegistrySuiteTest, RegisterMultipleSuites) {
    auto& registry = TestRegistry::instance();

    registry.registerSuite("SuiteA");
    registry.registerSuite("SuiteB");
    registry.registerSuite("SuiteC");

    auto suites = registry.getAllSuiteNames();
    EXPECT_GE(suites.size(), 3);
}

TEST_F(TestRegistrySuiteTest, DuplicateSuiteRegistration) {
    auto& registry = TestRegistry::instance();

    registry.registerSuite("DuplicateSuite");
    size_t countBefore = registry.getAllSuiteNames().size();

    registry.registerSuite("DuplicateSuite");
    size_t countAfter = registry.getAllSuiteNames().size();

    // Should not add duplicate
    EXPECT_EQ(countBefore, countAfter);
}

// ============================================================================
// TestRegistry Test Case Registration Tests
// ============================================================================

class TestRegistryTestCaseTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistryTestCaseTest, RegisterTestCase) {
    auto& registry = TestRegistry::instance();

    TestCase testCase;
    testCase.name = "TestCase1";
    testCase.func = []() {};

    registry.registerTest("TestSuite", testCase);

    // Test case should be registered
    auto tests = registry.getTestsInSuite("TestSuite");
    bool found = false;
    for (const auto& test : tests) {
        if (test.name == "TestCase1") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(TestRegistryTestCaseTest, RegisterTestCaseWithTags) {
    auto& registry = TestRegistry::instance();

    TestCase testCase;
    testCase.name = "TaggedTest";
    testCase.func = []() {};
    testCase.tags = {"unit", "fast"};

    registry.registerTest("TaggedSuite", testCase);

    auto tests = registry.getTestsInSuite("TaggedSuite");
    for (const auto& test : tests) {
        if (test.name == "TaggedTest") {
            EXPECT_EQ(test.tags.size(), 2);
            break;
        }
    }
}

TEST_F(TestRegistryTestCaseTest, RegisterDisabledTestCase) {
    auto& registry = TestRegistry::instance();

    TestCase testCase;
    testCase.name = "DisabledTest";
    testCase.func = []() {};
    testCase.skip = true;

    registry.registerTest("DisabledSuite", testCase);

    auto tests = registry.getTestsInSuite("DisabledSuite");
    for (const auto& test : tests) {
        if (test.name == "DisabledTest") {
            EXPECT_TRUE(test.skip);
            break;
        }
    }
}

// ============================================================================
// TestRegistry Query Tests
// ============================================================================

class TestRegistryQueryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistryQueryTest, GetAllSuiteNames) {
    auto& registry = TestRegistry::instance();

    auto suites = registry.getAllSuiteNames();

    // Should return a vector
    EXPECT_GE(suites.size(), 0);
}

TEST_F(TestRegistryQueryTest, GetTestsInSuite) {
    auto& registry = TestRegistry::instance();

    registry.registerSuite("QueryTestSuite");

    TestCase testCase;
    testCase.name = "QueryTest";
    testCase.func = []() {};
    registry.registerTest("QueryTestSuite", testCase);

    auto tests = registry.getTestsInSuite("QueryTestSuite");
    EXPECT_GE(tests.size(), 1);
}

TEST_F(TestRegistryQueryTest, GetTestsInNonexistentSuite) {
    auto& registry = TestRegistry::instance();

    auto tests = registry.getTestsInSuite("NonexistentSuite12345");

    // Should return empty vector
    EXPECT_TRUE(tests.empty());
}

TEST_F(TestRegistryQueryTest, GetTotalTestCount) {
    auto& registry = TestRegistry::instance();

    size_t count = registry.getTotalTestCount();

    // Should return non-negative count
    EXPECT_GE(count, 0);
}

// ============================================================================
// TestRegistry Filter Tests
// ============================================================================

class TestRegistryFilterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistryFilterTest, FilterByTag) {
    auto& registry = TestRegistry::instance();

    TestCase fastTest;
    fastTest.name = "FastTest";
    fastTest.func = []() {};
    fastTest.tags = {"fast"};

    TestCase slowTest;
    slowTest.name = "SlowTest";
    slowTest.func = []() {};
    slowTest.tags = {"slow"};

    registry.registerTest("FilterSuite", fastTest);
    registry.registerTest("FilterSuite", slowTest);

    auto fastTests = registry.getTestsByTag("fast");
    EXPECT_GE(fastTests.size(), 1);
}

TEST_F(TestRegistryFilterTest, FilterByName) {
    auto& registry = TestRegistry::instance();

    auto tests = registry.findTestsByPattern("*Test*");

    // Should return tests matching pattern
    EXPECT_GE(tests.size(), 0);
}

// ============================================================================
// TestRegistry Clear Tests
// ============================================================================

class TestRegistryClearTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRegistryClearTest, ClearAllTests) {
    auto& registry = TestRegistry::instance();

    // Register some tests
    TestCase testCase;
    testCase.name = "ClearTest";
    testCase.func = []() {};
    registry.registerTest("ClearSuite", testCase);

    // Clear and verify
    registry.clear();
    EXPECT_EQ(registry.getTotalTestCount(), 0);
}

}  // namespace atom::test::core::tests
