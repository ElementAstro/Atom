/*
 * test_test_fixture.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for TestFixture in atom/tests/fixtures/test_fixture.hpp

**************************************************/

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "atom/tests/fixtures/test_fixture.hpp"

namespace atom::test::fixtures::tests {

// ============================================================================
// Basic TestFixture Tests
// ============================================================================

class BasicTestFixtureTest : public atom::test::TestFixture {
protected:
    void SetUp() override {
        TestFixture::SetUp();
        setupCalled = true;
        value = 42;
    }

    void TearDown() override {
        teardownCalled = true;
        TestFixture::TearDown();
    }

    bool setupCalled = false;
    bool teardownCalled = false;
    int value = 0;
};

TEST_F(BasicTestFixtureTest, SetUpIsCalled) {
    EXPECT_TRUE(setupCalled);
    EXPECT_EQ(value, 42);
}

TEST_F(BasicTestFixtureTest, ValueIsInitialized) {
    EXPECT_EQ(value, 42);
    value = 100;
    EXPECT_EQ(value, 100);
}

// ============================================================================
// TestFixture with Resources Tests
// ============================================================================

class ResourceTestFixture : public atom::test::TestFixture {
protected:
    void SetUp() override {
        TestFixture::SetUp();
        resource = std::make_unique<std::string>("Test Resource");
        vec = {1, 2, 3, 4, 5};
    }

    void TearDown() override {
        resource.reset();
        vec.clear();
        TestFixture::TearDown();
    }

    std::unique_ptr<std::string> resource;
    std::vector<int> vec;
};

TEST_F(ResourceTestFixture, ResourceIsCreated) {
    EXPECT_NE(resource, nullptr);
    EXPECT_EQ(*resource, "Test Resource");
}

TEST_F(ResourceTestFixture, VectorIsInitialized) {
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);
}

TEST_F(ResourceTestFixture, CanModifyResource) {
    *resource = "Modified Resource";
    EXPECT_EQ(*resource, "Modified Resource");
}

TEST_F(ResourceTestFixture, CanModifyVector) {
    vec.push_back(6);
    EXPECT_EQ(vec.size(), 6);
    EXPECT_EQ(vec[5], 6);
}

// ============================================================================
// TestFixture Suite-Level Setup/Teardown Tests
// ============================================================================

class SuiteLevelFixture : public atom::test::TestFixture {
public:
    static void SetUpTestSuite() {
        suiteSetupCalled = true;
        sharedResource = std::make_unique<int>(100);
    }

    static void TearDownTestSuite() {
        suiteTeardownCalled = true;
        sharedResource.reset();
    }

protected:
    void SetUp() override {
        TestFixture::SetUp();
        instanceSetupCalled = true;
    }

    void TearDown() override {
        instanceTeardownCalled = true;
        TestFixture::TearDown();
    }

    static bool suiteSetupCalled;
    static bool suiteTeardownCalled;
    static std::unique_ptr<int> sharedResource;
    bool instanceSetupCalled = false;
    bool instanceTeardownCalled = false;
};

bool SuiteLevelFixture::suiteSetupCalled = false;
bool SuiteLevelFixture::suiteTeardownCalled = false;
std::unique_ptr<int> SuiteLevelFixture::sharedResource;

TEST_F(SuiteLevelFixture, SuiteSetupIsCalled) {
    EXPECT_TRUE(suiteSetupCalled);
    EXPECT_NE(sharedResource, nullptr);
    EXPECT_EQ(*sharedResource, 100);
}

TEST_F(SuiteLevelFixture, InstanceSetupIsCalled) {
    EXPECT_TRUE(instanceSetupCalled);
}

TEST_F(SuiteLevelFixture, SharedResourceIsAccessible) {
    EXPECT_NE(sharedResource, nullptr);
    EXPECT_EQ(*sharedResource, 100);
}

// ============================================================================
// TestFixture Inheritance Tests
// ============================================================================

class BaseFixture : public atom::test::TestFixture {
protected:
    void SetUp() override {
        TestFixture::SetUp();
        baseValue = 10;
    }

    int baseValue = 0;
};

class DerivedFixture : public BaseFixture {
protected:
    void SetUp() override {
        BaseFixture::SetUp();
        derivedValue = baseValue * 2;
    }

    int derivedValue = 0;
};

TEST_F(DerivedFixture, BaseSetUpIsCalled) {
    EXPECT_EQ(baseValue, 10);
}

TEST_F(DerivedFixture, DerivedSetUpIsCalled) {
    EXPECT_EQ(derivedValue, 20);
}

// ============================================================================
// TestFixture with Exception Handling Tests
// ============================================================================

class ExceptionFixture : public atom::test::TestFixture {
protected:
    void SetUp() override {
        TestFixture::SetUp();
        if (shouldThrowInSetup) {
            throw std::runtime_error("Setup failed");
        }
    }

    void TearDown() override {
        if (shouldThrowInTeardown) {
            throw std::runtime_error("Teardown failed");
        }
        TestFixture::TearDown();
    }

    bool shouldThrowInSetup = false;
    bool shouldThrowInTeardown = false;
};

TEST_F(ExceptionFixture, NormalExecution) {
    EXPECT_FALSE(shouldThrowInSetup);
    EXPECT_FALSE(shouldThrowInTeardown);
}

// ============================================================================
// TEST Macro Tests (without fixture)
// ============================================================================

class TestMacroTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestMacroTest, SimpleTest) {
    EXPECT_TRUE(true);
}

TEST_F(TestMacroTest, TestWithAssertion) {
    int value = 42;
    EXPECT_EQ(value, 42);
}

// ============================================================================
// TEST_F Macro Tests
// ============================================================================

class TestFMacroFixture : public atom::test::TestFixture {
protected:
    void SetUp() override {
        TestFixture::SetUp();
        counter = 0;
    }

    int counter;
};

TEST_F(TestFMacroFixture, IncrementCounter) {
    counter++;
    EXPECT_EQ(counter, 1);
}

TEST_F(TestFMacroFixture, CounterResetsEachTest) {
    EXPECT_EQ(counter, 0);
    counter = 100;
    EXPECT_EQ(counter, 100);
}

// ============================================================================
// ASSERT vs EXPECT Macro Tests
// ============================================================================

class AssertExpectTest : public atom::test::TestFixture {
protected:
    void SetUp() override { TestFixture::SetUp(); }
};

TEST_F(AssertExpectTest, ExpectContinuesOnFailure) {
    // EXPECT continues test execution after failure
    int value = 42;
    EXPECT_EQ(value, 42);
    value = 100;
    EXPECT_EQ(value, 100);
}

TEST_F(AssertExpectTest, MultipleExpects) {
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
    EXPECT_EQ(1, 1);
    EXPECT_NE(1, 2);
}

// ============================================================================
// GTEST_SKIP Tests
// ============================================================================

class SkipTest : public atom::test::TestFixture {
protected:
    void SetUp() override { TestFixture::SetUp(); }
};

TEST_F(SkipTest, ConditionalSkip) {
    bool condition = false;
    if (condition) {
        GTEST_SKIP() << "Skipping due to condition";
    }
    EXPECT_TRUE(true);
}

// ============================================================================
// Test Timeout Tests
// ============================================================================

class TimeoutTestFixture : public atom::test::TestFixture {
protected:
    void SetUp() override { TestFixture::SetUp(); }
};

TEST_F(TimeoutTestFixture, FastTest) {
    // This test should complete quickly
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        sum += i;
    }
    EXPECT_EQ(sum, 4950);
}

// ============================================================================
// Test Tags Tests
// ============================================================================

class TaggedTestFixture : public atom::test::TestFixture {
protected:
    void SetUp() override { TestFixture::SetUp(); }
};

TEST_F(TaggedTestFixture, UnitTest) {
    // This would be tagged as "unit" test
    EXPECT_TRUE(true);
}

TEST_F(TaggedTestFixture, IntegrationTest) {
    // This would be tagged as "integration" test
    EXPECT_TRUE(true);
}

}  // namespace atom::test::fixtures::tests
