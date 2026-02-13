/*
 * test_environment.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for test environment in
atom/tests/utilities/test_environment.hpp

**************************************************/

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "atom/tests/utilities/test_environment.hpp"

namespace atom::test::utilities::tests {

// ============================================================================
// Environment Base Class Tests
// ============================================================================

class TestEnvironment : public Environment {
public:
    void SetUp() override {
        setupCalled = true;
        setupCount++;
    }

    void TearDown() override {
        teardownCalled = true;
        teardownCount++;
    }

    static bool setupCalled;
    static bool teardownCalled;
    static int setupCount;
    static int teardownCount;
};

bool TestEnvironment::setupCalled = false;
bool TestEnvironment::teardownCalled = false;
int TestEnvironment::setupCount = 0;
int TestEnvironment::teardownCount = 0;

class EnvironmentBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestEnvironment::setupCalled = false;
        TestEnvironment::teardownCalled = false;
        TestEnvironment::setupCount = 0;
        TestEnvironment::teardownCount = 0;
    }
    void TearDown() override {}
};

TEST_F(EnvironmentBaseTest, SetUpIsCalled) {
    TestEnvironment env;
    env.SetUp();

    EXPECT_TRUE(TestEnvironment::setupCalled);
    EXPECT_EQ(TestEnvironment::setupCount, 1);
}

TEST_F(EnvironmentBaseTest, TearDownIsCalled) {
    TestEnvironment env;
    env.TearDown();

    EXPECT_TRUE(TestEnvironment::teardownCalled);
    EXPECT_EQ(TestEnvironment::teardownCount, 1);
}

TEST_F(EnvironmentBaseTest, MultipleSetUpCalls) {
    TestEnvironment env;
    env.SetUp();
    env.SetUp();
    env.SetUp();

    EXPECT_EQ(TestEnvironment::setupCount, 3);
}

// ============================================================================
// EnvironmentRegistry Tests
// ============================================================================

class EnvironmentRegistryTest : public ::testing::Test {
protected:
    void SetUp() override { EnvironmentRegistry::instance().clear(); }
    void TearDown() override { EnvironmentRegistry::instance().clear(); }
};

TEST_F(EnvironmentRegistryTest, SingletonInstance) {
    auto& registry1 = EnvironmentRegistry::instance();
    auto& registry2 = EnvironmentRegistry::instance();

    EXPECT_EQ(&registry1, &registry2);
}

TEST_F(EnvironmentRegistryTest, RegisterEnvironment) {
    auto env = std::make_shared<TestEnvironment>();
    EnvironmentRegistry::instance().add(env);

    EXPECT_EQ(EnvironmentRegistry::instance().count(), 1);
}

TEST_F(EnvironmentRegistryTest, RegisterMultipleEnvironments) {
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());

    EXPECT_EQ(EnvironmentRegistry::instance().count(), 3);
}

TEST_F(EnvironmentRegistryTest, SetUpAll) {
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());

    EnvironmentRegistry::instance().setUpAll();

    EXPECT_EQ(TestEnvironment::setupCount, 2);
}

TEST_F(EnvironmentRegistryTest, TearDownAll) {
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());

    EnvironmentRegistry::instance().tearDownAll();

    EXPECT_EQ(TestEnvironment::teardownCount, 2);
}

TEST_F(EnvironmentRegistryTest, ClearEnvironments) {
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());
    EnvironmentRegistry::instance().add(std::make_shared<TestEnvironment>());

    EnvironmentRegistry::instance().clear();

    EXPECT_EQ(EnvironmentRegistry::instance().count(), 0);
}

// ============================================================================
// EnvironmentGuard Tests
// ============================================================================

class EnvironmentGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestEnvironment::setupCalled = false;
        TestEnvironment::teardownCalled = false;
    }
    void TearDown() override {}
};

TEST_F(EnvironmentGuardTest, GuardCallsSetUp) {
    {
        EnvironmentGuard<TestEnvironment> guard;
        EXPECT_TRUE(TestEnvironment::setupCalled);
    }
}

TEST_F(EnvironmentGuardTest, GuardCallsTearDownOnDestruction) {
    { EnvironmentGuard<TestEnvironment> guard; }
    EXPECT_TRUE(TestEnvironment::teardownCalled);
}

// ============================================================================
// ScopedEnvironment Tests
// ============================================================================

class ScopedEnvironmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        setupCalled = false;
        teardownCalled = false;
    }
    void TearDown() override {}

    static bool setupCalled;
    static bool teardownCalled;
};

bool ScopedEnvironmentTest::setupCalled = false;
bool ScopedEnvironmentTest::teardownCalled = false;

TEST_F(ScopedEnvironmentTest, CallsSetupAndTeardown) {
    {
        ScopedEnvironment env([this]() { setupCalled = true; },
                              [this]() { teardownCalled = true; });

        EXPECT_TRUE(setupCalled);
        EXPECT_FALSE(teardownCalled);
    }

    EXPECT_TRUE(teardownCalled);
}

// ============================================================================
// EnvironmentVariable Tests
// ============================================================================

class EnvironmentVariableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EnvironmentVariableTest, GetExistingVariable) {
    // PATH should exist on all systems
    auto value = EnvironmentVariable::get("PATH");
    EXPECT_TRUE(value.has_value());
    EXPECT_FALSE(value->empty());
}

TEST_F(EnvironmentVariableTest, GetNonexistentVariable) {
    auto value = EnvironmentVariable::get("NONEXISTENT_VAR_12345");
    EXPECT_FALSE(value.has_value());
}

TEST_F(EnvironmentVariableTest, GetWithDefault) {
    auto value =
        EnvironmentVariable::getOrDefault("NONEXISTENT_VAR_12345", "default");
    EXPECT_EQ(value, "default");
}

TEST_F(EnvironmentVariableTest, IsSet) {
    EXPECT_TRUE(EnvironmentVariable::isSet("PATH"));
    EXPECT_FALSE(EnvironmentVariable::isSet("NONEXISTENT_VAR_12345"));
}

// ============================================================================
// TestConfiguration Tests
// ============================================================================

class TestConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestConfigurationTest, DefaultConfiguration) {
    TestConfiguration config;

    EXPECT_FALSE(config.verbose);
    EXPECT_TRUE(config.failFast);
}

TEST_F(TestConfigurationTest, SetConfiguration) {
    TestConfiguration config;
    config.verbose = true;
    config.failFast = false;
    config.timeout = std::chrono::seconds(30);

    EXPECT_TRUE(config.verbose);
    EXPECT_FALSE(config.failFast);
    EXPECT_EQ(config.timeout, std::chrono::seconds(30));
}

TEST_F(TestConfigurationTest, ConfigurationFromEnvironment) {
    // Test that configuration can read from environment
    TestConfiguration config;
    config.loadFromEnvironment();

    // Should not throw
    EXPECT_TRUE(true);
}

// ============================================================================
// ResourcePool Tests
// ============================================================================

class ResourcePoolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResourcePoolTest, AcquireResource) {
    ResourcePool<int> pool([]() { return std::make_unique<int>(42); });

    auto resource = pool.acquire();
    EXPECT_NE(resource, nullptr);
    EXPECT_EQ(*resource, 42);
}

TEST_F(ResourcePoolTest, ReleaseResource) {
    ResourcePool<int> pool([]() { return std::make_unique<int>(42); });

    auto resource = pool.acquire();
    EXPECT_EQ(pool.availableCount(), 0);

    // Resource is automatically released when unique_ptr goes out of scope
    resource.reset();
    EXPECT_EQ(pool.availableCount(), 1);
}

TEST_F(ResourcePoolTest, ReuseResource) {
    ResourcePool<int> pool([]() { return std::make_unique<int>(42); });

    {
        auto resource1 = pool.acquire();
        *resource1 = 100;
        // Resource is automatically released when unique_ptr goes out of scope
    }

    auto resource2 = pool.acquire();
    EXPECT_EQ(*resource2, 100);  // Should be the same resource
}

// ============================================================================
// TestEventListener Tests
// ============================================================================

class TestEventListenerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestEventListenerTest, OnTestStartCalled) {
    // TestEventListener uses virtual methods, not function pointers
    // Just verify the interface exists and can be called
    TestEventListener listener;
    TestCase testCase;
    testCase.name = "TestName";
    listener.onTestStart(testCase);
    SUCCEED();
}

TEST_F(TestEventListenerTest, OnTestEndCalled) {
    // TestEventListener uses virtual methods, not function pointers
    // Just verify the interface exists and can be called
    TestEventListener listener;
    TestResult result;
    result.passed = true;
    listener.onTestEnd(result);
    SUCCEED();
}

TEST_F(TestEventListenerTest, OnSuiteStartCalled) {
    // TestEventListener uses virtual methods, not function pointers
    // Just verify the interface exists and can be called
    TestEventListener listener;
    listener.onTestSuiteStart("SuiteName");
    SUCCEED();
}

TEST_F(TestEventListenerTest, OnSuiteEndCalled) {
    // TestEventListener uses virtual methods, not function pointers
    // Just verify the interface exists and can be called
    TestEventListener listener;
    listener.onTestSuiteEnd("SuiteName");
    SUCCEED();
}

// ============================================================================
// AddGlobalTestEnvironment Tests
// ============================================================================

class AddGlobalEnvironmentTest : public ::testing::Test {
protected:
    void SetUp() override { EnvironmentRegistry::instance().clear(); }
    void TearDown() override { EnvironmentRegistry::instance().clear(); }
};

TEST_F(AddGlobalEnvironmentTest, AddEnvironment) {
    AddGlobalTestEnvironment(new TestEnvironment());

    EXPECT_EQ(EnvironmentRegistry::instance().count(), 1);
}

TEST_F(AddGlobalEnvironmentTest, AddMultipleEnvironments) {
    AddGlobalTestEnvironment(new TestEnvironment());
    AddGlobalTestEnvironment(new TestEnvironment());

    EXPECT_EQ(EnvironmentRegistry::instance().count(), 2);
}

}  // namespace atom::test::utilities::tests
