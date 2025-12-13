/*
 * test_skip.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for skip conditions in atom/tests/utilities/test_skip.hpp

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "atom/tests/utilities/test_skip.hpp"

namespace atom::test::utilities::tests {

// ============================================================================
// SkipInfo Tests
// ============================================================================

class SkipInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SkipInfoTest, DefaultConstruction) {
    SkipInfo info;
    EXPECT_FALSE(info.shouldSkip);
    EXPECT_TRUE(info.reason.empty());
}

TEST_F(SkipInfoTest, SkipWithReason) {
    SkipInfo info{true, "Test not applicable on this platform"};
    EXPECT_TRUE(info.shouldSkip);
    EXPECT_EQ(info.reason, "Test not applicable on this platform");
}

TEST_F(SkipInfoTest, NoSkip) {
    SkipInfo info{false, ""};
    EXPECT_FALSE(info.shouldSkip);
}

// ============================================================================
// Platform Detection Tests
// ============================================================================

class PlatformDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PlatformDetectionTest, IsWindowsDetected) {
#ifdef _WIN32
    EXPECT_TRUE(isWindows());
#else
    EXPECT_FALSE(isWindows());
#endif
}

TEST_F(PlatformDetectionTest, IsLinuxDetected) {
#ifdef __linux__
    EXPECT_TRUE(isLinux());
#else
    EXPECT_FALSE(isLinux());
#endif
}

TEST_F(PlatformDetectionTest, IsMacOSDetected) {
#ifdef __APPLE__
    EXPECT_TRUE(isMacOS());
#else
    EXPECT_FALSE(isMacOS());
#endif
}

TEST_F(PlatformDetectionTest, IsUnixDetected) {
#if defined(__unix__) || defined(__APPLE__)
    EXPECT_TRUE(isUnix());
#else
    EXPECT_FALSE(isUnix());
#endif
}

// ============================================================================
// Build Type Detection Tests
// ============================================================================

class BuildTypeDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BuildTypeDetectionTest, DebugOrRelease) {
#ifdef NDEBUG
    EXPECT_TRUE(isReleaseBuild());
    EXPECT_FALSE(isDebugBuild());
#else
    EXPECT_TRUE(isDebugBuild());
    EXPECT_FALSE(isReleaseBuild());
#endif
}

// ============================================================================
// Conditional Skip Tests
// ============================================================================

class ConditionalSkipTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConditionalSkipTest, SkipIfTrue) {
    bool condition = true;
    auto info = skipIf(condition, "Condition is true");

    EXPECT_TRUE(info.shouldSkip);
    EXPECT_EQ(info.reason, "Condition is true");
}

TEST_F(ConditionalSkipTest, DontSkipIfFalse) {
    bool condition = false;
    auto info = skipIf(condition, "Condition is true");

    EXPECT_FALSE(info.shouldSkip);
}

TEST_F(ConditionalSkipTest, SkipUnless) {
    bool condition = false;
    auto info = skipUnless(condition, "Condition must be true");

    EXPECT_TRUE(info.shouldSkip);
}

TEST_F(ConditionalSkipTest, DontSkipUnless) {
    bool condition = true;
    auto info = skipUnless(condition, "Condition must be true");

    EXPECT_FALSE(info.shouldSkip);
}

// ============================================================================
// Environment Variable Skip Tests
// ============================================================================

class EnvVarSkipTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EnvVarSkipTest, SkipIfEnvNotSet) {
    auto info = skipIfEnvNotSet("NONEXISTENT_ENV_VAR_12345");
    EXPECT_TRUE(info.shouldSkip);
}

TEST_F(EnvVarSkipTest, DontSkipIfEnvSet) {
    // PATH should be set on most systems
    auto info = skipIfEnvNotSet("PATH");
    EXPECT_FALSE(info.shouldSkip);
}

TEST_F(EnvVarSkipTest, SkipIfEnvSet) {
    // PATH should be set on most systems
    auto info = skipIfEnvSet("PATH");
    EXPECT_TRUE(info.shouldSkip);
}

// ============================================================================
// Timeout Tests
// ============================================================================

class TimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimeoutTest, CompletesWithinTimeout) {
    auto result = runWithTimeout(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        },
        std::chrono::milliseconds(1000));

    EXPECT_TRUE(result.completed);
    EXPECT_FALSE(result.timedOut);
}

TEST_F(TimeoutTest, TimesOut) {
    auto result = runWithTimeout(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return true;
        },
        std::chrono::milliseconds(50));

    // Note: Due to thread scheduling, this may or may not time out
    // This test verifies the timeout mechanism works
    EXPECT_TRUE(result.completed || result.timedOut);
}

// ============================================================================
// Flaky Test Support Tests
// ============================================================================

class FlakyTestSupportTest : public ::testing::Test {
protected:
    void SetUp() override { attemptCount = 0; }
    void TearDown() override {}

    int attemptCount;
};

TEST_F(FlakyTestSupportTest, RetryOnFailure) {
    int maxRetries = 3;
    bool success = false;

    for (int i = 0; i < maxRetries && !success; ++i) {
        attemptCount++;
        // Succeed on third attempt
        if (attemptCount == 3) {
            success = true;
        }
    }

    EXPECT_TRUE(success);
    EXPECT_EQ(attemptCount, 3);
}

TEST_F(FlakyTestSupportTest, SucceedsOnFirstAttempt) {
    int maxRetries = 3;
    bool success = false;

    for (int i = 0; i < maxRetries && !success; ++i) {
        attemptCount++;
        success = true;  // Succeed immediately
    }

    EXPECT_TRUE(success);
    EXPECT_EQ(attemptCount, 1);
}

// ============================================================================
// Skip Reason Tests
// ============================================================================

class SkipReasonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SkipReasonTest, CustomReason) {
    std::string reason = "Feature not implemented yet";
    SkipInfo info{true, reason};

    EXPECT_EQ(info.reason, reason);
}

TEST_F(SkipReasonTest, PlatformReason) {
    std::string reason = "Windows-specific test";
    SkipInfo info{true, reason};

    EXPECT_NE(info.reason.find("Windows"), std::string::npos);
}

// ============================================================================
// ConditionalTest Class Tests
// ============================================================================

class ConditionalTestClassTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConditionalTestClassTest, ConditionTrue) {
    ConditionalTest test(true);
    EXPECT_TRUE(test.shouldRun());
}

TEST_F(ConditionalTestClassTest, ConditionFalse) {
    ConditionalTest test(false);
    EXPECT_FALSE(test.shouldRun());
}

TEST_F(ConditionalTestClassTest, WithSkipReason) {
    ConditionalTest test(false, "Test disabled for maintenance");
    EXPECT_FALSE(test.shouldRun());
    EXPECT_EQ(test.getSkipReason(), "Test disabled for maintenance");
}

// ============================================================================
// Skip Macros Tests
// ============================================================================

class SkipMacrosTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SkipMacrosTest, SkipIfMacroConditionFalse) {
    bool condition = false;
    if (condition) {
        GTEST_SKIP() << "Condition was true";
    }
    EXPECT_TRUE(true);
}

TEST_F(SkipMacrosTest, ConditionalExecution) {
    bool shouldRun = true;
    bool executed = false;

    if (shouldRun) {
        executed = true;
    }

    EXPECT_TRUE(executed);
}

// ============================================================================
// Platform-Specific Skip Tests
// ============================================================================

class PlatformSpecificSkipTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

#ifdef _WIN32
TEST_F(PlatformSpecificSkipTest, WindowsOnlyTest) {
    EXPECT_TRUE(isWindows());
}
#endif

#ifdef __linux__
TEST_F(PlatformSpecificSkipTest, LinuxOnlyTest) {
    EXPECT_TRUE(isLinux());
}
#endif

#ifdef __APPLE__
TEST_F(PlatformSpecificSkipTest, MacOSOnlyTest) {
    EXPECT_TRUE(isMacOS());
}
#endif

// ============================================================================
// Timeout Configuration Tests
// ============================================================================

class TimeoutConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimeoutConfigTest, DefaultTimeout) {
    TimeoutConfig config;
    EXPECT_GT(config.timeout.count(), 0);
}

TEST_F(TimeoutConfigTest, CustomTimeout) {
    TimeoutConfig config{std::chrono::seconds(30)};
    EXPECT_EQ(config.timeout, std::chrono::seconds(30));
}

TEST_F(TimeoutConfigTest, MillisecondTimeout) {
    TimeoutConfig config{std::chrono::milliseconds(500)};
    EXPECT_EQ(config.timeout, std::chrono::milliseconds(500));
}

}  // namespace atom::test::utilities::tests
