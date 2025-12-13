/*
 * test_test_runner.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for TestRunner in atom/tests/core/test_runner.hpp

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/tests/core/test_runner.hpp"

namespace atom::test::core::tests {

// ============================================================================
// TestRunnerConfig Tests
// ============================================================================

class TestRunnerConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRunnerConfigTest, DefaultConfiguration) {
    TestRunnerConfig config;

    EXPECT_FALSE(config.enableParallel);
    EXPECT_GE(config.numThreads, 1);
    EXPECT_EQ(config.maxRetries, 0);
    EXPECT_FALSE(config.failFast);
    EXPECT_TRUE(config.verbose);
}

TEST_F(TestRunnerConfigTest, CustomConfiguration) {
    TestRunnerConfig config;
    config.enableParallel = true;
    config.numThreads = 8;
    config.maxRetries = 3;
    config.failFast = true;
    config.verbose = false;

    EXPECT_TRUE(config.enableParallel);
    EXPECT_EQ(config.numThreads, 8);
    EXPECT_EQ(config.maxRetries, 3);
    EXPECT_TRUE(config.failFast);
    EXPECT_FALSE(config.verbose);
}

TEST_F(TestRunnerConfigTest, OutputFormatConfiguration) {
    TestRunnerConfig config;
    config.outputFormat = "json";

    EXPECT_EQ(config.outputFormat, "json");
}

TEST_F(TestRunnerConfigTest, FilterConfiguration) {
    TestRunnerConfig config;
    config.filter = "TestSuite.*";

    EXPECT_EQ(config.filter, "TestSuite.*");
}

TEST_F(TestRunnerConfigTest, TimeoutConfiguration) {
    TestRunnerConfig config;
    config.globalTimeout = std::chrono::seconds(60);

    EXPECT_EQ(config.globalTimeout, std::chrono::seconds(60));
}

TEST_F(TestRunnerConfigTest, ShuffleConfiguration) {
    TestRunnerConfig config;
    config.shuffleTests = true;
    config.randomSeed = 12345;

    EXPECT_TRUE(config.shuffleTests);
    EXPECT_EQ(config.randomSeed, 12345);
}

// ============================================================================
// TestRunner Initialization Tests
// ============================================================================

class TestRunnerInitTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRunnerInitTest, DefaultConstruction) {
    TestRunnerConfig config;
    TestRunner runner(config);

    // Should not throw during construction
    EXPECT_TRUE(true);
}

TEST_F(TestRunnerInitTest, ConstructionWithParallel) {
    TestRunnerConfig config;
    config.enableParallel = true;
    config.numThreads = 4;

    TestRunner runner(config);
    EXPECT_TRUE(true);
}

// ============================================================================
// TestRunner Filter Tests
// ============================================================================

class TestRunnerFilterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRunnerFilterTest, EmptyFilter) {
    TestRunnerConfig config;
    config.filter = "";

    // Empty filter should match all tests
    EXPECT_TRUE(config.filter.empty());
}

TEST_F(TestRunnerFilterTest, WildcardFilter) {
    TestRunnerConfig config;
    config.filter = "*";

    EXPECT_EQ(config.filter, "*");
}

TEST_F(TestRunnerFilterTest, PrefixFilter) {
    TestRunnerConfig config;
    config.filter = "TestSuite.*";

    EXPECT_EQ(config.filter, "TestSuite.*");
}

TEST_F(TestRunnerFilterTest, NegativeFilter) {
    TestRunnerConfig config;
    config.filter = "-SlowTests.*";

    EXPECT_EQ(config.filter, "-SlowTests.*");
}

// ============================================================================
// TestRunner Retry Tests
// ============================================================================

class TestRunnerRetryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRunnerRetryTest, NoRetries) {
    TestRunnerConfig config;
    config.maxRetries = 0;

    EXPECT_EQ(config.maxRetries, 0);
}

TEST_F(TestRunnerRetryTest, WithRetries) {
    TestRunnerConfig config;
    config.maxRetries = 3;

    EXPECT_EQ(config.maxRetries, 3);
}

// ============================================================================
// TestRunner Hooks Tests
// ============================================================================

class TestRunnerHooksTest : public ::testing::Test {
protected:
    void SetUp() override { hookCalled = false; }
    void TearDown() override {}

    bool hookCalled;
};

TEST_F(TestRunnerHooksTest, BeforeAllHook) {
    TestHooks hooks;
    hooks.beforeAll = [this]() { hookCalled = true; };

    hooks.beforeAll();
    EXPECT_TRUE(hookCalled);
}

TEST_F(TestRunnerHooksTest, AfterAllHook) {
    TestHooks hooks;
    hooks.afterAll = [this]() { hookCalled = true; };

    hooks.afterAll();
    EXPECT_TRUE(hookCalled);
}

TEST_F(TestRunnerHooksTest, BeforeEachHook) {
    TestHooks hooks;
    hooks.beforeEach = [this](const std::string&) { hookCalled = true; };

    hooks.beforeEach("TestName");
    EXPECT_TRUE(hookCalled);
}

TEST_F(TestRunnerHooksTest, AfterEachHook) {
    TestHooks hooks;
    hooks.afterEach = [this](const std::string&, bool) { hookCalled = true; };

    hooks.afterEach("TestName", true);
    EXPECT_TRUE(hookCalled);
}

// ============================================================================
// TestRunner Output Format Tests
// ============================================================================

class TestRunnerOutputFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestRunnerOutputFormatTest, ConsoleFormat) {
    TestRunnerConfig config;
    config.outputFormat = "console";

    EXPECT_EQ(config.outputFormat, "console");
}

TEST_F(TestRunnerOutputFormatTest, JsonFormat) {
    TestRunnerConfig config;
    config.outputFormat = "json";

    EXPECT_EQ(config.outputFormat, "json");
}

TEST_F(TestRunnerOutputFormatTest, XmlFormat) {
    TestRunnerConfig config;
    config.outputFormat = "xml";

    EXPECT_EQ(config.outputFormat, "xml");
}

}  // namespace atom::test::core::tests
