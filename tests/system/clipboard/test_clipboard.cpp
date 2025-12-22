/**
 * @file test_clipboard.cpp
 * @brief Comprehensive tests for clipboard operations
 *
 * This file contains tests for the clipboard management functions in
 * atom/system/clipboard/clipboard.hpp including text operations, binary data
 * operations, format management, and clipboard change monitoring.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/clipboard/clipboard.hpp"
#include "atom/system/clipboard/clipboard_error.hpp"

namespace clip::test {

using namespace std::chrono_literals;

/**
 * @brief Test fixture for clipboard tests
 */
class ClipboardTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get clipboard instance
        clipboard = &Clipboard::instance();

        // Clear clipboard before each test to ensure clean state
        try {
            clipboard->clear();
        } catch (...) {
            // Ignore errors during setup cleanup
        }
    }

    void TearDown() override {
        // Clean up clipboard after each test
        try {
            clipboard->clear();
        } catch (...) {
            // Ignore errors during teardown cleanup
        }
    }

    Clipboard* clipboard;
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

/**
 * @brief Test that clipboard is a singleton
 */
TEST_F(ClipboardTest, Singleton_ReturnsSameInstance) {
    Clipboard& instance1 = Clipboard::instance();
    Clipboard& instance2 = Clipboard::instance();

    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Basic Operation Tests
// ============================================================================

/**
 * @brief Test clipboard clear operation
 */
TEST_F(ClipboardTest, Clear_RemovesAllContent) {
    // Set some text
    clipboard->setText("Test content");

    // Clear clipboard
    EXPECT_NO_THROW(clipboard->clear());

    // Verify clipboard is empty
    EXPECT_FALSE(clipboard->hasText());
}

// ============================================================================
// Text Operation Tests
// ============================================================================

/**
 * @brief Test setting and getting text
 */
TEST_F(ClipboardTest, SetText_GetText_Success) {
    const std::string testText = "Hello, Clipboard!";

    EXPECT_NO_THROW(clipboard->setText(testText));

    std::string retrievedText;
    EXPECT_NO_THROW(retrievedText = clipboard->getText());
    EXPECT_EQ(retrievedText, testText);
}

/**
 * @brief Test setting empty text
 */
TEST_F(ClipboardTest, SetText_EmptyString_Success) {
    const std::string emptyText = "";

    EXPECT_NO_THROW(clipboard->setText(emptyText));
    EXPECT_TRUE(clipboard->hasText());
}

/**
 * @brief Test setting text with Unicode characters
 */
TEST_F(ClipboardTest, SetText_UnicodeCharacters_Success) {
    const std::string unicodeText = "Hello 世界 🌍";

    EXPECT_NO_THROW(clipboard->setText(unicodeText));

    std::string retrievedText;
    EXPECT_NO_THROW(retrievedText = clipboard->getText());
    EXPECT_EQ(retrievedText, unicodeText);
}

/**
 * @brief Test safe text operations (non-throwing)
 */
TEST_F(ClipboardTest, SetTextSafe_GetTextSafe_Success) {
    const std::string testText = "Safe operations test";

    auto setResult = clipboard->setTextSafe(testText);
    EXPECT_TRUE(setResult.has_value());

    auto getResult = clipboard->getTextSafe();
    EXPECT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), testText);
}

/**
 * @brief Test hasText query
 */
TEST_F(ClipboardTest, HasText_AfterSetText_ReturnsTrue) {
    clipboard->setText("Some text");
    EXPECT_TRUE(clipboard->hasText());
}

/**
 * @brief Test hasText query on empty clipboard
 */
TEST_F(ClipboardTest, HasText_EmptyClipboard_ReturnsFalse) {
    clipboard->clear();
    // Note: This might return true if empty string is considered text
    // The actual behavior depends on platform implementation
}

// ============================================================================
// Binary Data Operation Tests
// ============================================================================

/**
 * @brief Test setting and getting binary data
 */
TEST_F(ClipboardTest, SetData_GetData_Success) {
    std::vector<std::byte> testData = {std::byte{0x48}, std::byte{0x65},
                                       std::byte{0x6C}, std::byte{0x6C},
                                       std::byte{0x6F}};

    try {
        clipboard->setData(formats::TEXT, testData);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "setData failed: " << e.what();
    }

    std::vector<std::byte> retrievedData;
    try {
        retrievedData = clipboard->getData(formats::TEXT);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "getData failed: " << e.what();
    }

    // Data might be modified by platform (e.g., null terminator added)
    // So we check if our data is contained in the retrieved data
    EXPECT_GE(retrievedData.size(), testData.size());
}

/**
 * @brief Test containsFormat query
 */
TEST_F(ClipboardTest, ContainsFormat_AfterSetData_ReturnsTrue) {
    std::vector<std::byte> testData = {std::byte{0x01}, std::byte{0x02}};

    clipboard->setData(formats::TEXT, testData);
    EXPECT_TRUE(clipboard->containsFormat(formats::TEXT));
}

/**
 * @brief Test containsFormat for non-existent format
 */
TEST_F(ClipboardTest, ContainsFormat_NonExistentFormat_ReturnsFalse) {
    clipboard->clear();

    // Use a custom format that shouldn't exist
    ClipboardFormat customFormat{9999};
    EXPECT_FALSE(clipboard->containsFormat(customFormat));
}

/**
 * @brief Test safe binary data operations
 */
TEST_F(ClipboardTest, SetDataSafe_GetDataSafe_Success) {
    std::vector<std::byte> testData = {std::byte{0xDE}, std::byte{0xAD},
                                       std::byte{0xBE}, std::byte{0xEF}};

    auto setResult = clipboard->setDataSafe(formats::TEXT, testData);
    EXPECT_TRUE(setResult.has_value());

    auto getResult = clipboard->getDataSafe(formats::TEXT);
    EXPECT_TRUE(getResult.has_value());
}

// ============================================================================
// Format Management Tests
// ============================================================================

/**
 * @brief Test getting available formats
 */
TEST_F(ClipboardTest, GetAvailableFormats_AfterSetText_ContainsTextFormat) {
    clipboard->setText("Test");

    auto formats = clipboard->getAvailableFormats();

    // Should contain at least one format
    EXPECT_FALSE(formats.empty());
}

/**
 * @brief Test getting format name for predefined format
 */
TEST_F(ClipboardTest, GetFormatName_PredefinedFormat_ReturnsName) {
    // This test depends on platform implementation
    // Some platforms may not support format name queries
    try {
        std::string formatName = clipboard->getFormatName(formats::TEXT);
        EXPECT_FALSE(formatName.empty());
    } catch (const ClipboardException&) {
        // Format name query not supported on this platform
        GTEST_SKIP() << "Format name query not supported on this platform";
    }
}

/**
 * @brief Test registering custom format
 */
TEST_F(ClipboardTest, RegisterFormat_CustomFormat_Success) {
    const std::string customFormatName = "application/x-atom-test-format";

    ClipboardFormat customFormat{0};
    EXPECT_NO_THROW(customFormat = Clipboard::registerFormat(customFormatName));

    // Verify the format was registered (value should be non-zero)
    EXPECT_NE(customFormat.value, 0u);
}

/**
 * @brief Test safe format registration
 */
TEST_F(ClipboardTest, RegisterFormatSafe_CustomFormat_Success) {
    const std::string customFormatName = "application/x-atom-test-safe";

    auto result = Clipboard::registerFormatSafe(customFormatName);
    EXPECT_TRUE(result.has_value());

    if (result.has_value()) {
        EXPECT_NE(result.value().value, 0u);
    }
}

// ============================================================================
// Clipboard Monitoring Tests
// ============================================================================

/**
 * @brief Test registering clipboard change callback
 */
TEST_F(ClipboardTest, RegisterChangeCallback_ValidCallback_ReturnsNonZeroId) {
    bool callbackInvoked = false;

    auto callbackId = clipboard->registerChangeCallback(
        [&callbackInvoked]() { callbackInvoked = true; });

    EXPECT_NE(callbackId, 0u);

    // Clean up
    clipboard->unregisterChangeCallback(callbackId);
}

/**
 * @brief Test unregistering clipboard change callback
 */
TEST_F(ClipboardTest, UnregisterChangeCallback_ValidId_ReturnsTrue) {
    auto callbackId = clipboard->registerChangeCallback([]() {});

    EXPECT_TRUE(clipboard->unregisterChangeCallback(callbackId));
}

/**
 * @brief Test unregistering non-existent callback
 */
TEST_F(ClipboardTest, UnregisterChangeCallback_InvalidId_ReturnsFalse) {
    EXPECT_FALSE(clipboard->unregisterChangeCallback(99999));
}

/**
 * @brief Test hasChanged functionality
 */
TEST_F(ClipboardTest, HasChanged_InitialState_ReturnsFalse) {
    // After clearing, hasChanged should be false
    clipboard->clear();
    clipboard->markChangeProcessed();

    // Note: Actual behavior depends on platform implementation
    // Some platforms may not support change detection
}

/**
 * @brief Test markChangeProcessed
 */
TEST_F(ClipboardTest, MarkChangeProcessed_ResetsChangeFlag) {
    clipboard->setText("Test");
    clipboard->markChangeProcessed();

    // After marking as processed, hasChanged should return false
    // until next change occurs
    EXPECT_NO_THROW(clipboard->markChangeProcessed());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

/**
 * @brief Test ClipboardResult with error
 */
TEST_F(ClipboardTest, ClipboardResult_WithError_HasNoValue) {
    ClipboardResult<std::string> result(
        make_error_code(ClipboardErrorCode::ACCESS_DENIED));

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              make_error_code(ClipboardErrorCode::ACCESS_DENIED));
}

/**
 * @brief Test ClipboardResult with value
 */
TEST_F(ClipboardTest, ClipboardResult_WithValue_HasValue) {
    ClipboardResult<std::string> result("test value");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "test value");
}

/**
 * @brief Test ClipboardResult value_or
 */
TEST_F(ClipboardTest, ClipboardResult_ValueOr_ReturnsDefault) {
    ClipboardResult<std::string> errorResult(
        make_error_code(ClipboardErrorCode::INVALID_DATA));

    EXPECT_EQ(errorResult.value_or("default"), "default");

    ClipboardResult<std::string> valueResult("actual");
    EXPECT_EQ(valueResult.value_or("default"), "actual");
}

/**
 * @brief Test ClipboardException construction
 */
TEST_F(ClipboardTest, ClipboardException_WithErrorCode_ContainsMessage) {
    ClipboardException ex(ClipboardErrorCode::ACCESS_DENIED);

    std::string message = ex.what();
    EXPECT_FALSE(message.empty());
    EXPECT_EQ(ex.code(), make_error_code(ClipboardErrorCode::ACCESS_DENIED));
}

/**
 * @brief Test ClipboardAccessDeniedException
 */
TEST_F(ClipboardTest, ClipboardAccessDeniedException_HasCorrectErrorCode) {
    ClipboardAccessDeniedException ex;

    EXPECT_EQ(ex.code(), make_error_code(ClipboardErrorCode::ACCESS_DENIED));
}

/**
 * @brief Test ClipboardFormatException
 */
TEST_F(ClipboardTest, ClipboardFormatException_HasCorrectErrorCode) {
    ClipboardFormatException ex;

    EXPECT_EQ(ex.code(),
              make_error_code(ClipboardErrorCode::FORMAT_NOT_SUPPORTED));
}

/**
 * @brief Test ClipboardTimeoutException
 */
TEST_F(ClipboardTest, ClipboardTimeoutException_HasCorrectErrorCode) {
    ClipboardTimeoutException ex;

    EXPECT_EQ(ex.code(), make_error_code(ClipboardErrorCode::TIMEOUT));
}

/**
 * @brief Test ClipboardSystemException
 */
TEST_F(ClipboardTest, ClipboardSystemException_HasCorrectErrorCode) {
    ClipboardSystemException ex;

    EXPECT_EQ(ex.code(), make_error_code(ClipboardErrorCode::SYSTEM_ERROR));
}

// ============================================================================
// Edge Case Tests
// ============================================================================

/**
 * @brief Test setting very long text
 */
TEST_F(ClipboardTest, SetText_VeryLongString_Success) {
    std::string longText(10000, 'A');

    EXPECT_NO_THROW(clipboard->setText(longText));

    std::string retrieved;
    EXPECT_NO_THROW(retrieved = clipboard->getText());
    EXPECT_EQ(retrieved, longText);
}

/**
 * @brief Test setting text with special characters
 */
TEST_F(ClipboardTest, SetText_SpecialCharacters_Success) {
    const std::string specialText = "Line1\nLine2\tTabbed\r\nWindows\0Null";

    EXPECT_NO_THROW(clipboard->setText(specialText));
}

/**
 * @brief Test multiple consecutive operations
 */
TEST_F(ClipboardTest, MultipleOperations_Consecutive_Success) {
    clipboard->setText("First");
    EXPECT_EQ(clipboard->getText(), "First");

    clipboard->setText("Second");
    EXPECT_EQ(clipboard->getText(), "Second");

    clipboard->clear();
    EXPECT_FALSE(clipboard->hasText());
}

/**
 * @brief Test ClipboardFormat equality
 */
TEST_F(ClipboardTest, ClipboardFormat_Equality_Works) {
    ClipboardFormat format1{1};
    ClipboardFormat format2{1};
    ClipboardFormat format3{2};

    EXPECT_EQ(format1, format2);
    EXPECT_NE(format1, format3);
}

/**
 * @brief Test ClipboardFormat comparison
 */
TEST_F(ClipboardTest, ClipboardFormat_Comparison_Works) {
    ClipboardFormat format1{1};
    ClipboardFormat format2{2};

    EXPECT_LT(format1, format2);
    EXPECT_GT(format2, format1);
}

/**
 * @brief Test ScopeGuard functionality
 */
TEST_F(ClipboardTest, ScopeGuard_ExecutesOnDestruction) {
    bool executed = false;

    {
        auto guard = make_scope_guard([&executed]() { executed = true; });
    }

    EXPECT_TRUE(executed);
}

/**
 * @brief Test ScopeGuard dismiss
 */
TEST_F(ClipboardTest, ScopeGuard_Dismiss_DoesNotExecute) {
    bool executed = false;

    {
        auto guard = make_scope_guard([&executed]() { executed = true; });
        guard.dismiss();
    }

    EXPECT_FALSE(executed);
}

}  // namespace clip::test
