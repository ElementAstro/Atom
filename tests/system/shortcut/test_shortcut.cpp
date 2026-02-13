/**
 * @file test_shortcut.cpp
 * @brief Comprehensive tests for keyboard shortcut detection
 *
 * This file contains tests for the shortcut detection functionality in
 * atom/system/shortcut/detector.hpp including shortcut capture detection,
 * keyboard hook monitoring, and platform-specific features.
 *
 * Note: Many tests are Windows-specific as the shortcut detector is primarily
 * designed for Windows platforms.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

// Only compile shortcut tests on Windows as the implementation is
// Windows-specific
#ifdef _WIN32

#include "atom/system/shortcut/detector.hpp"
#include "atom/system/shortcut/shortcut.h"
#include "atom/system/shortcut/status.h"

namespace shortcut_detector::test {

// ============================================================================
// Shortcut Struct Tests
// ============================================================================

/**
 * @brief Test fixture for Shortcut struct tests
 */
class ShortcutStructTest : public ::testing::Test {
protected:
    // Common virtual key codes for testing
    static constexpr uint32_t VK_A = 0x41;
    static constexpr uint32_t VK_F1 = 0x70;
    static constexpr uint32_t VK_ESCAPE = 0x1B;
};

/**
 * @brief Test Shortcut construction with no modifiers
 */
TEST_F(ShortcutStructTest, Constructor_NoModifiers_Success) {
    Shortcut shortcut(VK_A);

    EXPECT_EQ(shortcut.vkCode, VK_A);
    EXPECT_FALSE(shortcut.ctrl);
    EXPECT_FALSE(shortcut.alt);
    EXPECT_FALSE(shortcut.shift);
    EXPECT_FALSE(shortcut.win);
}

/**
 * @brief Test Shortcut construction with Ctrl modifier
 */
TEST_F(ShortcutStructTest, Constructor_WithCtrl_Success) {
    Shortcut shortcut(VK_A, true, false, false, false);

    EXPECT_EQ(shortcut.vkCode, VK_A);
    EXPECT_TRUE(shortcut.ctrl);
    EXPECT_FALSE(shortcut.alt);
    EXPECT_FALSE(shortcut.shift);
    EXPECT_FALSE(shortcut.win);
}

/**
 * @brief Test Shortcut construction with multiple modifiers
 */
TEST_F(ShortcutStructTest, Constructor_MultipleModifiers_Success) {
    Shortcut shortcut(VK_F1, true, true, false, false);

    EXPECT_EQ(shortcut.vkCode, VK_F1);
    EXPECT_TRUE(shortcut.ctrl);
    EXPECT_TRUE(shortcut.alt);
    EXPECT_FALSE(shortcut.shift);
    EXPECT_FALSE(shortcut.win);
}

/**
 * @brief Test Shortcut construction with all modifiers
 */
TEST_F(ShortcutStructTest, Constructor_AllModifiers_Success) {
    Shortcut shortcut(VK_A, true, true, true, true);

    EXPECT_TRUE(shortcut.ctrl);
    EXPECT_TRUE(shortcut.alt);
    EXPECT_TRUE(shortcut.shift);
    EXPECT_TRUE(shortcut.win);
}

/**
 * @brief Test Shortcut toString method
 */
TEST_F(ShortcutStructTest, ToString_NoModifiers_ReturnsKeyName) {
    Shortcut shortcut(VK_A);

    std::string str = shortcut.toString();
    EXPECT_FALSE(str.empty());
}

/**
 * @brief Test Shortcut toString with Ctrl
 */
TEST_F(ShortcutStructTest, ToString_WithCtrl_ContainsCtrl) {
    Shortcut shortcut(VK_A, true);

    std::string str = shortcut.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Ctrl"));
}

/**
 * @brief Test Shortcut toString with multiple modifiers
 */
TEST_F(ShortcutStructTest, ToString_MultipleModifiers_ContainsAll) {
    Shortcut shortcut(VK_F1, true, true, true, false);

    std::string str = shortcut.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Ctrl"));
    EXPECT_THAT(str, ::testing::HasSubstr("Alt"));
    EXPECT_THAT(str, ::testing::HasSubstr("Shift"));
}

/**
 * @brief Test Shortcut equality operator
 */
TEST_F(ShortcutStructTest, Equality_SameShortcuts_ReturnsTrue) {
    Shortcut shortcut1(VK_A, true, false, false, false);
    Shortcut shortcut2(VK_A, true, false, false, false);

    EXPECT_EQ(shortcut1, shortcut2);
}

/**
 * @brief Test Shortcut inequality
 */
TEST_F(ShortcutStructTest, Equality_DifferentKeys_ReturnsFalse) {
    Shortcut shortcut1(VK_A, true);
    Shortcut shortcut2(VK_F1, true);

    EXPECT_NE(shortcut1, shortcut2);
}

/**
 * @brief Test Shortcut inequality with different modifiers
 */
TEST_F(ShortcutStructTest, Equality_DifferentModifiers_ReturnsFalse) {
    Shortcut shortcut1(VK_A, true, false);
    Shortcut shortcut2(VK_A, false, true);

    EXPECT_NE(shortcut1, shortcut2);
}

/**
 * @brief Test Shortcut hash function
 */
TEST_F(ShortcutStructTest, Hash_SameShortcuts_SameHash) {
    Shortcut shortcut1(VK_A, true);
    Shortcut shortcut2(VK_A, true);

    EXPECT_EQ(shortcut1.hash(), shortcut2.hash());
}

/**
 * @brief Test Shortcut hash function with different shortcuts
 */
TEST_F(ShortcutStructTest, Hash_DifferentShortcuts_DifferentHash) {
    Shortcut shortcut1(VK_A, true);
    Shortcut shortcut2(VK_F1, true);

    // Hashes should be different (though collision is theoretically possible)
    EXPECT_NE(shortcut1.hash(), shortcut2.hash());
}

/**
 * @brief Test std::hash specialization
 */
TEST_F(ShortcutStructTest, StdHash_Works) {
    Shortcut shortcut(VK_A, true);

    std::hash<Shortcut> hasher;
    size_t hashValue = hasher(shortcut);

    EXPECT_EQ(hashValue, shortcut.hash());
}

// ============================================================================
// ShortcutDetector Construction Tests
// ============================================================================

/**
 * @brief Test fixture for ShortcutDetector tests
 */
class ShortcutDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create detector instance
        detector = std::make_unique<ShortcutDetector>();
    }

    void TearDown() override {
        // Clean up detector
        detector.reset();
    }

    std::unique_ptr<ShortcutDetector> detector;

    static constexpr uint32_t VK_A = 0x41;
    static constexpr uint32_t VK_F1 = 0x70;
    static constexpr uint32_t VK_F12 = 0x7B;
};

/**
 * @brief Test ShortcutDetector construction
 */
TEST_F(ShortcutDetectorTest, Constructor_Success) {
    EXPECT_NE(detector, nullptr);
}

/**
 * @brief Test ShortcutDetector is non-copyable
 */
TEST_F(ShortcutDetectorTest, NonCopyable_Verified) {
    // This test verifies at compile time that copy operations are deleted
    EXPECT_TRUE((std::is_copy_constructible_v<ShortcutDetector> == false));
    EXPECT_TRUE((std::is_copy_assignable_v<ShortcutDetector> == false));
}

/**
 * @brief Test ShortcutDetector is non-movable
 */
TEST_F(ShortcutDetectorTest, NonMovable_Verified) {
    // This test verifies at compile time that move operations are deleted
    EXPECT_TRUE((std::is_move_constructible_v<ShortcutDetector> == false));
    EXPECT_TRUE((std::is_move_assignable_v<ShortcutDetector> == false));
}

// ============================================================================
// Shortcut Capture Detection Tests
// ============================================================================

/**
 * @brief Test checking if a simple shortcut is captured
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_SimpleKey_ReturnsResult) {
    Shortcut shortcut(VK_A);

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));

    // Result should have a valid status
    // We can't predict the exact status as it depends on system state
}

/**
 * @brief Test checking Ctrl+A shortcut
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_CtrlA_ReturnsResult) {
    Shortcut shortcut(VK_A, true);  // Ctrl+A

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));
}

/**
 * @brief Test checking Ctrl+Alt+Del (system reserved)
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_CtrlAltDel_ReservedBySystem) {
    Shortcut shortcut(0x2E, true, true);  // Ctrl+Alt+Del (VK_DELETE = 0x2E)

    ShortcutCheckResult result = detector->isShortcutCaptured(shortcut);

    // Ctrl+Alt+Del should be captured by system or reserved
    EXPECT_TRUE(result.status == ShortcutStatus::CapturedBySystem ||
                result.status == ShortcutStatus::Reserved);
}

/**
 * @brief Test checking Win+L (system shortcut)
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_WinL_CapturedBySystem) {
    Shortcut shortcut(0x4C, false, false, false, true);  // Win+L (VK_L = 0x4C)

    ShortcutCheckResult result = detector->isShortcutCaptured(shortcut);

    // Win+L should be captured by system
    EXPECT_TRUE(result.status == ShortcutStatus::CapturedBySystem ||
                result.status == ShortcutStatus::Reserved);
}

/**
 * @brief Test checking F1 key
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_F1_ReturnsResult) {
    Shortcut shortcut(VK_F1);

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));
}

/**
 * @brief Test checking Ctrl+Alt+F12
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_CtrlAltF12_ReturnsResult) {
    Shortcut shortcut(VK_F12, true, true);

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));
}

/**
 * @brief Test that result contains details
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_ResultContainsDetails) {
    Shortcut shortcut(VK_A, true);

    ShortcutCheckResult result = detector->isShortcutCaptured(shortcut);

    // Details string should not be null (may be empty)
    EXPECT_NO_THROW(std::string details = result.details);
}

/**
 * @brief Test checking multiple shortcuts in sequence
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_MultipleChecks_Success) {
    Shortcut shortcut1(VK_A, true);
    Shortcut shortcut2(VK_F1);
    Shortcut shortcut3(VK_F12, true, true);

    EXPECT_NO_THROW(detector->isShortcutCaptured(shortcut1));
    EXPECT_NO_THROW(detector->isShortcutCaptured(shortcut2));
    EXPECT_NO_THROW(detector->isShortcutCaptured(shortcut3));
}

// ============================================================================
// Keyboard Hook Detection Tests
// ============================================================================

/**
 * @brief Test checking if keyboard hooks are installed
 */
TEST_F(ShortcutDetectorTest, HasKeyboardHookInstalled_ReturnsBoolean) {
    bool hasHooks = false;
    EXPECT_NO_THROW(hasHooks = detector->hasKeyboardHookInstalled());

    // Result should be either true or false (no exception)
    // Use the variable to avoid unused warning
    EXPECT_TRUE(hasHooks == true || hasHooks == false);
}

/**
 * @brief Test getting processes with keyboard hooks
 */
TEST_F(ShortcutDetectorTest, GetProcessesWithKeyboardHooks_ReturnsVector) {
    std::vector<std::string> processes;
    EXPECT_NO_THROW(processes = detector->getProcessesWithKeyboardHooks());

    // Should return a vector (may be empty if no hooks detected)
}

/**
 * @brief Test that hook detection is consistent
 */
TEST_F(ShortcutDetectorTest, HasKeyboardHookInstalled_ConsistentResults) {
    bool result1 = detector->hasKeyboardHookInstalled();
    bool result2 = detector->hasKeyboardHookInstalled();

    // Results should be consistent when called immediately after each other
    EXPECT_EQ(result1, result2);
}

/**
 * @brief Test that process list matches hook detection
 */
TEST_F(ShortcutDetectorTest, GetProcessesWithKeyboardHooks_MatchesHasHooks) {
    bool hasHooks = detector->hasKeyboardHookInstalled();
    auto processes = detector->getProcessesWithKeyboardHooks();

    if (hasHooks) {
        // If hooks are detected, process list should not be empty
        EXPECT_FALSE(processes.empty());
    }
    // Note: If hasHooks is false, processes may still contain entries
    // as the detection methods may differ
}

// ============================================================================
// Edge Case Tests
// ============================================================================

/**
 * @brief Test with invalid virtual key code
 */
TEST_F(ShortcutDetectorTest,
       IsShortcutCaptured_InvalidVkCode_HandlesGracefully) {
    Shortcut shortcut(0xFFFF);  // Invalid VK code

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));
}

/**
 * @brief Test with zero virtual key code
 */
TEST_F(ShortcutDetectorTest, IsShortcutCaptured_ZeroVkCode_HandlesGracefully) {
    Shortcut shortcut(0);

    ShortcutCheckResult result;
    EXPECT_NO_THROW(result = detector->isShortcutCaptured(shortcut));
}

/**
 * @brief Test ShortcutStatus enum values
 */
TEST_F(ShortcutDetectorTest, ShortcutStatus_AllValuesValid) {
    // Verify all enum values are distinct
    EXPECT_NE(ShortcutStatus::Available, ShortcutStatus::CapturedByApp);
    EXPECT_NE(ShortcutStatus::Available, ShortcutStatus::CapturedBySystem);
    EXPECT_NE(ShortcutStatus::Available, ShortcutStatus::Reserved);
    EXPECT_NE(ShortcutStatus::CapturedByApp, ShortcutStatus::CapturedBySystem);
    EXPECT_NE(ShortcutStatus::CapturedByApp, ShortcutStatus::Reserved);
    EXPECT_NE(ShortcutStatus::CapturedBySystem, ShortcutStatus::Reserved);
}

/**
 * @brief Test ShortcutCheckResult structure
 */
TEST_F(ShortcutDetectorTest, ShortcutCheckResult_CanBeConstructed) {
    ShortcutCheckResult result;
    result.status = ShortcutStatus::Available;
    result.capturingApplication = "TestApp";
    result.details = "Test details";

    EXPECT_EQ(result.status, ShortcutStatus::Available);
    EXPECT_EQ(result.capturingApplication, "TestApp");
    EXPECT_EQ(result.details, "Test details");
}

/**
 * @brief Test rapid successive calls
 */
TEST_F(ShortcutDetectorTest, RapidSuccessiveCalls_NoErrors) {
    Shortcut shortcut(VK_A, true);

    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(detector->isShortcutCaptured(shortcut));
    }
}

/**
 * @brief Test with all modifier combinations
 */
TEST_F(ShortcutDetectorTest,
       IsShortcutCaptured_AllModifierCombinations_Success) {
    // Test all 16 combinations of modifiers (2^4)
    for (int ctrl = 0; ctrl <= 1; ++ctrl) {
        for (int alt = 0; alt <= 1; ++alt) {
            for (int shift = 0; shift <= 1; ++shift) {
                for (int win = 0; win <= 1; ++win) {
                    Shortcut shortcut(VK_A, ctrl, alt, shift, win);
                    EXPECT_NO_THROW(detector->isShortcutCaptured(shortcut));
                }
            }
        }
    }
}

}  // namespace shortcut_detector::test

#else  // !_WIN32

// Placeholder test for non-Windows platforms
TEST(ShortcutDetectorTest, NotAvailableOnThisPlatform) {
    GTEST_SKIP() << "Shortcut detector is only available on Windows platforms";
}

#endif  // _WIN32
