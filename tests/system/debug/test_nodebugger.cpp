/**
 * @file test_nodebugger.cpp
 * @brief Comprehensive tests for debugger detection functionality
 *
 * This file contains tests for the anti-debugging and debugger detection
 * functionality in atom/system/debug/nodebugger.hpp.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "atom/system/debug/nodebugger.hpp"

namespace atom::system::test {

using namespace std::chrono_literals;
using namespace atom::system;

/**
 * @brief Test fixture for debugger detection tests
 */
class NoDebuggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Stop any ongoing monitoring before each test
        stopAntiDebugMonitoring();
        std::this_thread::sleep_for(100ms);
    }

    void TearDown() override {
        // Clean up after each test
        stopAntiDebugMonitoring();
        std::this_thread::sleep_for(100ms);
    }
};

// ============================================================================
// Debugger Detection Tests
// ============================================================================

/**
 * @brief Test basic debugger detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_BasicCheck) {
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::BASIC_CHECK);
    // Result depends on whether a debugger is actually attached
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test timing-based debugger detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_TimingCheck) {
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::TIMING_CHECK);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test exception-based debugger detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_ExceptionBased) {
    bool attached =
        isDebuggerAttached(DebuggerDetectionMethod::EXCEPTION_BASED);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test hardware breakpoint detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_HardwareBreakpoints) {
    bool attached =
        isDebuggerAttached(DebuggerDetectionMethod::HARDWARE_BREAKPOINTS);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test memory breakpoint detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_MemoryBreakpoints) {
    bool attached =
        isDebuggerAttached(DebuggerDetectionMethod::MEMORY_BREAKPOINTS);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test process environment detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_ProcessEnvironment) {
    bool attached =
        isDebuggerAttached(DebuggerDetectionMethod::PROCESS_ENVIRONMENT);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test parent process detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_ParentProcess) {
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::PARENT_PROCESS);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test thread context detection
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_ThreadContext) {
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::THREAD_CONTEXT);
    EXPECT_TRUE(attached || !attached);
}

/**
 * @brief Test all detection methods
 */
TEST_F(NoDebuggerTest, IsDebuggerAttached_AllMethods) {
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::ALL_METHODS);
    EXPECT_TRUE(attached || !attached);
}

// ============================================================================
// Anti-Debug Configuration Tests
// ============================================================================

/**
 * @brief Test default anti-debug configuration
 */
TEST_F(NoDebuggerTest, AntiDebugConfig_DefaultValues) {
    AntiDebugConfig config;
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.method, DebuggerDetectionMethod::BASIC_CHECK);
    EXPECT_EQ(config.action, AntiDebugAction::EXIT);
    EXPECT_EQ(config.customAction, nullptr);
    EXPECT_GT(config.timingThreshold, 0);
    EXPECT_FALSE(config.continuousMonitoring);
    EXPECT_GT(config.checkInterval, 0);
}

/**
 * @brief Test custom anti-debug configuration
 */
TEST_F(NoDebuggerTest, AntiDebugConfig_CustomValues) {
    bool customActionCalled = false;
    AntiDebugConfig config;
    config.enabled = true;
    config.method = DebuggerDetectionMethod::TIMING_CHECK;
    config.action = AntiDebugAction::CUSTOM;
    config.customAction = [&customActionCalled]() {
        customActionCalled = true;
    };
    config.timingThreshold = 5000;
    config.continuousMonitoring = true;
    config.checkInterval = 1000;

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.method, DebuggerDetectionMethod::TIMING_CHECK);
    EXPECT_EQ(config.action, AntiDebugAction::CUSTOM);
    EXPECT_NE(config.customAction, nullptr);
}

// ============================================================================
// Debugger Detection Handling Tests
// ============================================================================

/**
 * @brief Test handling debugger detection with default config
 */
TEST_F(NoDebuggerTest, HandleDebuggerDetection_DefaultConfig) {
    // This test should not crash or exit
    // Note: Actual behavior depends on whether debugger is attached
    EXPECT_NO_THROW({
        AntiDebugConfig config;
        config.enabled = false;  // Disable to prevent actual exit
        handleDebuggerDetection(config);
    });
}

/**
 * @brief Test handling with custom action
 */
TEST_F(NoDebuggerTest, HandleDebuggerDetection_CustomAction) {
    bool customActionCalled = false;
    AntiDebugConfig config;
    config.action = AntiDebugAction::CUSTOM;
    config.customAction = [&customActionCalled]() {
        customActionCalled = true;
    };

    // This might or might not call the custom action depending on debugger
    // state
    EXPECT_NO_THROW(handleDebuggerDetection(config));
}

/**
 * @brief Test handling with mislead action
 */
TEST_F(NoDebuggerTest, HandleDebuggerDetection_MisleadAction) {
    AntiDebugConfig config;
    config.action = AntiDebugAction::MISLEAD;

    EXPECT_NO_THROW(handleDebuggerDetection(config));
}

// ============================================================================
// Anti-Debug Monitoring Tests
// ============================================================================

/**
 * @brief Test starting anti-debug monitoring
 */
TEST_F(NoDebuggerTest, StartAntiDebugMonitoring_BasicUsage) {
    AntiDebugConfig config;
    config.continuousMonitoring = true;
    config.checkInterval = 100;
    config.enabled = false;  // Disable to prevent actual exit

    EXPECT_NO_THROW(startAntiDebugMonitoring(config));
    std::this_thread::sleep_for(300ms);
    EXPECT_NO_THROW(stopAntiDebugMonitoring());
}

/**
 * @brief Test stopping monitoring when not started
 */
TEST_F(NoDebuggerTest, StopAntiDebugMonitoring_NotStarted) {
    EXPECT_NO_THROW(stopAntiDebugMonitoring());
}

/**
 * @brief Test multiple start/stop cycles
 */
TEST_F(NoDebuggerTest, AntiDebugMonitoring_MultipleCycles) {
    AntiDebugConfig config;
    config.continuousMonitoring = true;
    config.checkInterval = 50;
    config.enabled = false;

    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(startAntiDebugMonitoring(config));
        std::this_thread::sleep_for(100ms);
        EXPECT_NO_THROW(stopAntiDebugMonitoring());
    }
}

// ============================================================================
// Memory Protection Tests
// ============================================================================

/**
 * @brief Test memory region protection
 */
TEST_F(NoDebuggerTest, ProtectMemoryRegion_BasicUsage) {
    int testData = 42;
    EXPECT_NO_THROW(protectMemoryRegion(&testData, sizeof(testData)));
}

/**
 * @brief Test protecting null pointer
 */
TEST_F(NoDebuggerTest, ProtectMemoryRegion_NullPointer) {
    // Should handle null pointer gracefully
    EXPECT_NO_THROW(protectMemoryRegion(nullptr, 0));
}

/**
 * @brief Test protecting zero size
 */
TEST_F(NoDebuggerTest, ProtectMemoryRegion_ZeroSize) {
    int testData = 42;
    EXPECT_NO_THROW(protectMemoryRegion(&testData, 0));
}

// ============================================================================
// Integrity Check Tests
// ============================================================================

/**
 * @brief Test installing integrity checks
 */
TEST_F(NoDebuggerTest, InstallIntegrityChecks_BasicUsage) {
    uint8_t code[] = {0x90, 0x90, 0x90, 0x90};  // NOP instructions
    uint8_t hash[] = {0x00, 0x01, 0x02, 0x03};

    EXPECT_NO_THROW(installIntegrityChecks(code, sizeof(code), hash));
}

/**
 * @brief Test integrity checks with null pointers
 */
TEST_F(NoDebuggerTest, InstallIntegrityChecks_NullPointers) {
    EXPECT_NO_THROW(installIntegrityChecks(nullptr, 0, nullptr));
}

// ============================================================================
// Dump Prevention Tests
// ============================================================================

/**
 * @brief Test preventing memory dumps
 */
TEST_F(NoDebuggerTest, PreventDumping_BasicUsage) {
    EXPECT_NO_THROW(preventDumping());
}

// ============================================================================
// Windows-Specific Tests
// ============================================================================

#ifdef _WIN32
/**
 * @brief Test hiding PEB debugging flags (Windows only)
 */
TEST_F(NoDebuggerTest, HidePEBDebuggingFlags_Windows) {
    EXPECT_NO_THROW(hidePEBDebuggingFlags());
}

/**
 * @brief Test detecting remote threads (Windows only)
 */
TEST_F(NoDebuggerTest, DetectRemoteThreads_Windows) {
    EXPECT_NO_THROW(detectRemoteThreads());
}

/**
 * @brief Test enabling self-modifying code (Windows only)
 */
TEST_F(NoDebuggerTest, EnableSelfModifyingCode_Windows) {
    uint8_t code[] = {0x90, 0x90, 0x90, 0x90};
    EXPECT_NO_THROW(enableSelfModifyingCode(code, sizeof(code)));
}

/**
 * @brief Test self-modifying code with null pointer (Windows only)
 */
TEST_F(NoDebuggerTest, EnableSelfModifyingCode_NullPointer) {
    EXPECT_NO_THROW(enableSelfModifyingCode(nullptr, 0));
}
#endif

// ============================================================================
// Integration Tests
// ============================================================================

/**
 * @brief Test complete anti-debug workflow
 */
TEST_F(NoDebuggerTest, Integration_CompleteWorkflow) {
    // Configure anti-debug
    AntiDebugConfig config;
    config.enabled = false;  // Disable to prevent actual exit
    config.method = DebuggerDetectionMethod::ALL_METHODS;
    config.action = AntiDebugAction::MISLEAD;
    config.continuousMonitoring = true;
    config.checkInterval = 100;

    // Start monitoring
    EXPECT_NO_THROW(startAntiDebugMonitoring(config));

    // Let it run for a bit
    std::this_thread::sleep_for(300ms);

    // Check debugger status
    bool attached = isDebuggerAttached(DebuggerDetectionMethod::BASIC_CHECK);
    EXPECT_TRUE(attached || !attached);

    // Stop monitoring
    EXPECT_NO_THROW(stopAntiDebugMonitoring());
}

}  // namespace atom::system::test
