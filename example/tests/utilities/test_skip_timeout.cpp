/**
 * @file test_skip_timeout.cpp
 * @brief Test skip and timeout functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <chrono>
#include <thread>

using namespace atom::test;

// ============================================================================
// Platform Detection Tests
// ============================================================================

TEST(PlatformTests, DetectPlatform) {
#ifdef _WIN32
    expect_true(platform::isWindows);
    expect_false(platform::isLinux);
    expect_false(platform::isMacOS);
#elif defined(__linux__)
    expect_false(platform::isWindows);
    expect_true(platform::isLinux);
    expect_false(platform::isMacOS);
#elif defined(__APPLE__)
    expect_false(platform::isWindows);
    expect_false(platform::isLinux);
    expect_true(platform::isMacOS);
#endif
}

TEST(PlatformTests, DetectBuildType) {
#ifdef _DEBUG
    expect_true(platform::isDebug);
#else
    expect_true(platform::isRelease);
#endif
}

// ============================================================================
// Skip Condition Tests
// ============================================================================

TEST(SkipTests, SkipIfConditionTrue) {
    bool shouldSkip = false;  // Set to true to test skipping

    SKIP_IF(shouldSkip, "Skipping because condition is true");

    expect_true(true);
}

TEST(SkipTests, SkipUnlessConditionFalse) {
    bool conditionMet = true;  // Set to false to test skipping

    SKIP_UNLESS(conditionMet, "Skipping because condition is not met");

    expect_true(true);
}

// ============================================================================
// Platform-Specific Skip Tests
// ============================================================================

TEST(SkipTests, WindowsOnlyTest) {
#ifndef _WIN32
    SKIP_IF(true, "This test only runs on Windows");
#endif
    expect_true(true);
}

TEST(SkipTests, LinuxOnlyTest) {
#ifndef __linux__
    SKIP_IF(true, "This test only runs on Linux");
#endif
    expect_true(true);
}

TEST(SkipTests, MacOSOnlyTest) {
#ifndef __APPLE__
    SKIP_IF(true, "This test only runs on macOS");
#endif
    expect_true(true);
}

// ============================================================================
// Skip Info Helper Tests
// ============================================================================

TEST(SkipTests, SkipIfHelper) {
    auto skipInfo = skipIf(false, "Should not skip");
    expect_false(skipInfo.shouldSkip);

    auto skipInfo2 = skipIf(true, "Should skip");
    expect_true(skipInfo2.shouldSkip);
    expect_eq(skipInfo2.reason, "Should skip");
}

TEST(SkipTests, SkipUnlessHelper) {
    auto skipInfo = skipUnless(true, "Should not skip");
    expect_false(skipInfo.shouldSkip);

    auto skipInfo2 = skipUnless(false, "Should skip");
    expect_true(skipInfo2.shouldSkip);
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST_TIMEOUT(TimeoutTests, FastTest, 1000) {
    // This test should complete quickly
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
        sum += i;
    }
    expect_eq(sum, 4950);
}

TEST_TIMEOUT(TimeoutTests, MediumTest, 2000) {
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    expect_true(true);
}

// ============================================================================
// Timeout Assertion Tests
// ============================================================================

TEST(TimeoutTests, ExpectCompletesWithin) {
    expect_completes_within(
        []() {
            int x = 0;
            for (int i = 0; i < 1000; ++i) {
                x += i;
            }
            (void)x;
        },
        500);
}

// ============================================================================
// Conditional Test Registration
// ============================================================================

TEST(ConditionalTests, AlwaysRuns) { expect_true(true); }

TEST_CONDITIONAL(ConditionalTests, RunsOnlyIfTrue, false,
                 "Condition is false") {
    // This test will be skipped because condition is false
    expect_true(true);
}

// ============================================================================
// Environment Variable Skip Tests
// ============================================================================

TEST(SkipTests, SkipIfEnvNotSet) {
    // This will skip if ATOM_TEST_VAR is not set
    auto skipInfo = skipIfEnvNotSet("ATOM_TEST_VAR_NONEXISTENT");
    // Just test the helper, don't actually skip
    expect_true(skipInfo.shouldSkip);  // Should be true since var doesn't exist
}

// ============================================================================
// Complex Skip Scenarios
// ============================================================================

class SkipFixture : public TestFixture {
protected:
    void SetUp() override { setupCalled = true; }

    void TearDown() override { teardownCalled = true; }

    bool setupCalled = false;
    bool teardownCalled = false;
};

TEST_F(SkipFixture, NormalFixtureTest) { expect_true(setupCalled); }

// ============================================================================
// Timeout with Fixture
// ============================================================================

class TimedFixture : public TestFixture {
protected:
    void SetUp() override { startTime = std::chrono::steady_clock::now(); }

    void TearDown() override {
        auto endTime = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
    }

    std::chrono::steady_clock::time_point startTime;
    std::chrono::milliseconds duration{0};
};

TEST_F_TIMEOUT(TimedFixture, TimedFixtureTest, 1000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    expect_true(true);
}

// ============================================================================
// RunWithTimeout Helper Tests
// ============================================================================

TEST(TimeoutTests, RunWithTimeoutSuccess) {
    auto [success, message] = runWithTimeout(
        []() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); },
        std::chrono::milliseconds(1000));

    expect_true(success);
    expect_true(message.empty());
}

TEST(TimeoutTests, RunWithTimeoutException) {
    auto [success, message] =
        runWithTimeout([]() { throw std::runtime_error("Test exception"); },
                       std::chrono::milliseconds(1000));

    expect_false(success);
    expect_contains(message, "Test exception");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) { return runAllTests(argc, argv); }
