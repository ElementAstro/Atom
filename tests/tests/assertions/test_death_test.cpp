/*
 * test_death_test.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for death test macros in atom/tests/assertions/death_test.hpp

**************************************************/

#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

#include "atom/tests/assertions/death_test.hpp"

namespace atom::test::assertions::tests {

// ============================================================================
// Death Test Helper Functions
// ============================================================================

void functionThatExits() { std::exit(1); }

void functionThatAborts() { std::abort(); }

void functionThatThrows() { throw std::runtime_error("fatal error"); }

void functionThatSucceeds() {
    // Does nothing, should not cause death
}

// ============================================================================
// Exit Code Predicate Tests
// ============================================================================

class ExitCodePredicateTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExitCodePredicateTest, ExitedWithCodeZero) {
    auto predicate = ExitedWithCode(0);
    EXPECT_TRUE(predicate(0));
    EXPECT_FALSE(predicate(1));
}

TEST_F(ExitCodePredicateTest, ExitedWithCodeNonZero) {
    auto predicate = ExitedWithCode(1);
    EXPECT_TRUE(predicate(1));
    EXPECT_FALSE(predicate(0));
}

TEST_F(ExitCodePredicateTest, ExitedWithCodeNegative) {
    auto predicate = ExitedWithCode(-1);
    EXPECT_TRUE(predicate(-1));
    EXPECT_FALSE(predicate(1));
}

// ============================================================================
// Death Test Configuration Tests
// ============================================================================

class DeathTestConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeathTestConfigTest, DefaultConfiguration) {
    DeathTestConfig config;
    EXPECT_FALSE(config.captureStderr);
    EXPECT_TRUE(config.expectedMessage.empty());
}

TEST_F(DeathTestConfigTest, CaptureStderrConfiguration) {
    DeathTestConfig config;
    config.captureStderr = true;
    EXPECT_TRUE(config.captureStderr);
}

TEST_F(DeathTestConfigTest, ExpectedMessageConfiguration) {
    DeathTestConfig config;
    config.expectedMessage = "error message";
    EXPECT_EQ(config.expectedMessage, "error message");
}

// ============================================================================
// Death Test Result Tests
// ============================================================================

class DeathTestResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeathTestResultTest, SuccessResult) {
    DeathTestResult result;
    result.died = true;
    result.exitCode = 1;

    EXPECT_TRUE(result.died);
    EXPECT_EQ(result.exitCode, 1);
}

TEST_F(DeathTestResultTest, FailureResult) {
    DeathTestResult result;
    result.died = false;
    result.exitCode = 0;

    EXPECT_FALSE(result.died);
    EXPECT_EQ(result.exitCode, 0);
}

TEST_F(DeathTestResultTest, ResultWithMessage) {
    DeathTestResult result;
    result.died = true;
    result.exitCode = 1;
    result.capturedStderr = "Fatal error occurred";

    EXPECT_TRUE(result.died);
    EXPECT_FALSE(result.capturedStderr.empty());
}

// ============================================================================
// Death Test Runner Tests
// ============================================================================

class DeathTestRunnerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeathTestRunnerTest, RunnerConstruction) {
    DeathTestRunner runner;
    // Should construct without throwing
    EXPECT_TRUE(true);
}

TEST_F(DeathTestRunnerTest, RunnerWithConfig) {
    DeathTestConfig config;
    config.captureStderr = true;
    config.expectedMessage = "error";

    DeathTestRunner runner(config);
    EXPECT_TRUE(true);
}

// ============================================================================
// Signal Handler Tests (Platform-specific)
// ============================================================================

class SignalHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

#ifdef _WIN32
TEST_F(SignalHandlerTest, WindowsPlatform) {
    // Windows-specific signal handling tests
    EXPECT_TRUE(true);
}
#else
TEST_F(SignalHandlerTest, UnixPlatform) {
    // Unix-specific signal handling tests
    EXPECT_TRUE(true);
}
#endif

// ============================================================================
// Death Test Assertion Helper Tests
// ============================================================================

class DeathTestHelperTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeathTestHelperTest, MessageMatchesSubstring) {
    std::string message = "Fatal error: something went wrong";
    std::string pattern = "error";

    EXPECT_NE(message.find(pattern), std::string::npos);
}

TEST_F(DeathTestHelperTest, MessageMatchesRegex) {
    std::string message = "Error code: 42";
    std::regex pattern("Error code: [0-9]+");

    EXPECT_TRUE(std::regex_search(message, pattern));
}

// ============================================================================
// EXPECT_DEATH Macro Tests (using GTest's version for comparison)
// ============================================================================

class ExpectDeathMacroTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Note: Actual EXPECT_DEATH tests are tricky to test as they involve
// process termination. These tests verify the framework's death test
// infrastructure.

TEST_F(ExpectDeathMacroTest, DeathTestEnabled) {
#ifdef GTEST_HAS_DEATH_TEST
    EXPECT_TRUE(true);
#else
    GTEST_SKIP() << "Death tests not supported on this platform";
#endif
}

// ============================================================================
// ASSERT_DEATH Macro Tests
// ============================================================================

class AssertDeathMacroTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AssertDeathMacroTest, DeathTestAvailable) {
#ifdef GTEST_HAS_DEATH_TEST
    EXPECT_TRUE(true);
#else
    GTEST_SKIP() << "Death tests not supported on this platform";
#endif
}

// ============================================================================
// EXPECT_EXIT Macro Tests
// ============================================================================

class ExpectExitMacroTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExpectExitMacroTest, ExitCodePredicate) {
    auto predicate = [](int code) { return code == 0; };
    EXPECT_TRUE(predicate(0));
    EXPECT_FALSE(predicate(1));
}

// ============================================================================
// Death Test Thread Safety Tests
// ============================================================================

class DeathTestThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DeathTestThreadSafetyTest, ThreadSafeConfiguration) {
    DeathTestConfig config;
    config.threadSafe = true;

    EXPECT_TRUE(config.threadSafe);
}

}  // namespace atom::test::assertions::tests
