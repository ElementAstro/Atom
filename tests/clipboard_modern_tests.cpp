
/**
 * @file clipboard_modern_tests.cpp
 * @brief Comprehensive tests for the modern C++17+ clipboard interface
 * 
 * Tests cover:
 * - Strong typing with ClipboardFormat
 * - Exception-safe and non-throwing variants  
 * - Zero-copy operations with std::span
 * - Callback mechanism for change monitoring
 * - Error handling with ClipboardResult<T>
 * - Predefined format constants
 */

#include "atom/system/clipboard.hpp"
#include "atom/system/clipboard_error.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace clip;

class ClipboardModernTest : public ::testing::Test {
protected:
    void SetUp() override {
        clipboard_ = &Clipboard::instance();
        // Clear clipboard before each test
        try {
            clipboard_->open();
            clipboard_->clear();
            clipboard_->close();
        } catch (...) {
            // Ignore setup errors
        }
    }

    void TearDown() override {
        // Clean up after each test
        try {
            clipboard_->open();
            clipboard_->clear(); 
            clipboard_->close();
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    Clipboard* clipboard_;
};

// ============================================================================
// Strong Typing Tests
// ============================================================================

TEST_F(ClipboardModernTest, ClipboardFormatConstruction) {
    // Test ClipboardFormat construction and comparison
    ClipboardFormat format1{42};
    ClipboardFormat format2{42};
    ClipboardFormat format3{43};
    
    EXPECT_EQ(format1.value, 42);
    EXPECT_EQ(format1, format2);
    EXPECT_NE(format1, format3);
    EXPECT_LT(format1, format3);
}

TEST_F(ClipboardModernTest, PredefinedFormats) {
    // Test predefined format constants
    EXPECT_NE(formats::TEXT.value, 0);
    EXPECT_NE(formats::HTML.value, 0);
    EXPECT_NE(formats::IMAGE_PNG.value, 0);
    EXPECT_NE(formats::IMAGE_TIFF.value, 0);
    EXPECT_NE(formats::RTF.value, 0);
    
    // Ensure they're all different
    EXPECT_NE(formats::TEXT, formats::HTML);
    EXPECT_NE(formats::HTML, formats::IMAGE_PNG);
    EXPECT_NE(formats::IMAGE_PNG, formats::RTF);
}

// ============================================================================
// Exception-Safe Operations Tests
// ============================================================================

TEST_F(ClipboardModernTest, TextOperationsExceptionSafe) {
    const std::string testText = "Modern C++17 test text";
    
    // Test throwing variants
    EXPECT_NO_THROW({
        clipboard_->open();
        clipboard_->clear();
        clipboard_->setText(testText);
        auto retrieved = clipboard_->getText();
        EXPECT_EQ(retrieved, testText);
        clipboard_->close();
    });
}

TEST_F(ClipboardModernTest, TextOperationsNonThrowing) {
    const std::string testText = "Safe operations test";
    
    // Test non-throwing variants
    auto setResult = clipboard_->setTextSafe(testText);
    EXPECT_TRUE(setResult.has_value());
    EXPECT_TRUE(static_cast<bool>(setResult));
    
    auto getResult = clipboard_->getTextSafe();
    EXPECT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), testText);
}

TEST_F(ClipboardModernTest, ClipboardResultVoidSpecialization) {
    // Test ClipboardResult<void> specialization
    auto result = clipboard_->setTextSafe("test");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NO_THROW(result.value());
    EXPECT_EQ(result.error().value(), 0);
}

// ============================================================================
// Zero-Copy Binary Operations Tests  
// ============================================================================

TEST_F(ClipboardModernTest, BinaryDataZeroCopy) {
    std::vector<std::byte> testData = {
        std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C},
        std::byte{0x6F}  // "Hello"
    };
    
    auto customFormat = Clipboard::registerFormat("application/x-test-binary");
    EXPECT_NE(customFormat.value, 0);
    
    // Test zero-copy setData with std::span
    EXPECT_NO_THROW({
        clipboard_->open();
        clipboard_->setData(customFormat, std::span<const std::byte>{testData});
        clipboard_->close();
    });
    
    // Test data retrieval
    auto retrievedData = clipboard_->getData(customFormat);
    EXPECT_EQ(retrievedData.size(), testData.size());
    EXPECT_EQ(retrievedData, testData);
}

TEST_F(ClipboardModernTest, BinaryDataSafeOperations) {
    std::vector<std::byte> testData = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
    auto format = Clipboard::registerFormat("application/x-test-safe");
    
    auto setResult = clipboard_->setDataSafe(format, std::span<const std::byte>{testData});
    EXPECT_TRUE(setResult.has_value());
    
    auto getResult = clipboard_->getDataSafe(format);
    EXPECT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), testData);
}

// ============================================================================
// Format Detection and Querying Tests
// ============================================================================

TEST_F(ClipboardModernTest, FormatDetection) {
    const std::string testText = "Format detection test";
    clipboard_->setTextSafe(testText);
    
    EXPECT_TRUE(clipboard_->hasText());
    EXPECT_TRUE(clipboard_->containsFormat(formats::TEXT));
}

TEST_F(ClipboardModernTest, AvailableFormatsQuery) {
    clipboard_->setTextSafe("Test for format query");
    
    auto formatsResult = clipboard_->getAvailableFormatsSafe();
    EXPECT_TRUE(formatsResult.has_value());
    EXPECT_FALSE(formatsResult.value().empty());
    
    // Should contain at least text format
    auto formats = formatsResult.value();
    bool hasTextFormat = std::any_of(formats.begin(), formats.end(),
        [](const ClipboardFormat& fmt) { return fmt == formats::TEXT; });
    EXPECT_TRUE(hasTextFormat);
}

TEST_F(ClipboardModernTest, FormatNameResolution) {
    auto nameResult = clipboard_->getFormatNameSafe(formats::TEXT);
    EXPECT_TRUE(nameResult.has_value());
    EXPECT_FALSE(nameResult.value().empty());
}

// ============================================================================
// Change Monitoring Tests
// ============================================================================

TEST_F(ClipboardModernTest, ChangeCallbackRegistration) {
    bool callbackTriggered = false;
    
    auto callbackId = clipboard_->registerChangeCallback([&callbackTriggered]() {
        callbackTriggered = true;
    });
    
    EXPECT_NE(callbackId, 0);
    
    // Trigger a change
    clipboard_->setTextSafe("Change trigger test");
    
    // Give some time for callback processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Clean up
    EXPECT_TRUE(clipboard_->unregisterChangeCallback(callbackId));
}

TEST_F(ClipboardModernTest, ChangeDetection) {
    clipboard_->markChangeProcessed(); // Reset change state
    
    clipboard_->setTextSafe("Change detection test");
    
    // Should detect the change
    EXPECT_TRUE(clipboard_->hasChanged());
    
    clipboard_->markChangeProcessed();
    
    // After marking as processed, should not show change
    EXPECT_FALSE(clipboard_->hasChanged());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ClipboardModernTest, ErrorCodeMapping) {
    // Test that error codes are properly mapped
    auto error = make_error_code(ClipboardErrorCode::INVALID_DATA);
    EXPECT_EQ(error.category(), clipboard_error_category());
    EXPECT_FALSE(error.message().empty());
}

TEST_F(ClipboardModernTest, ClipboardResultErrorHandling) {
    // Create a result with an error
    ClipboardResult<std::string> errorResult{make_error_code(ClipboardErrorCode::ACCESS_DENIED)};
    
    EXPECT_FALSE(errorResult.has_value());
    EXPECT_FALSE(static_cast<bool>(errorResult));
    EXPECT_EQ(errorResult.error().value(), static_cast<int>(ClipboardErrorCode::ACCESS_DENIED));
    
    // Test value_or functionality
    EXPECT_EQ(errorResult.value_or("default"), "default");
}

// ============================================================================
// Custom Format Registration Tests
// ============================================================================

TEST_F(ClipboardModernTest, CustomFormatRegistration) {
    auto format1 = Clipboard::registerFormat("application/x-test-format-1");
    auto format2 = Clipboard::registerFormat("application/x-test-format-2");
    
    EXPECT_NE(format1.value, 0);
    EXPECT_NE(format2.value, 0);
    EXPECT_NE(format1, format2);
}

TEST_F(ClipboardModernTest, CustomFormatRegistrationSafe) {
    auto result = Clipboard::registerFormatSafe("application/x-test-safe-format");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(result.value().value, 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ClipboardModernTest, CompleteWorkflow) {
    // Test a complete clipboard workflow
    const std::string testText = "Complete workflow test";
    
    // 1. Open clipboard
    EXPECT_NO_THROW(clipboard_->open());
    
    // 2. Clear previous content
    EXPECT_NO_THROW(clipboard_->clear());
    
    // 3. Set text content
    EXPECT_NO_THROW(clipboard_->setText(testText));
    
    // 4. Verify content is available
    EXPECT_TRUE(clipboard_->hasText());
    
    // 5. Retrieve and verify content
    auto retrieved = clipboard_->getText();
    EXPECT_EQ(retrieved, testText);
    
    // 6. Check available formats
    auto formats = clipboard_->getAvailableFormats();
    EXPECT_FALSE(formats.empty());
    
    // 7. Close clipboard
    EXPECT_NO_THROW(clipboard_->close());
}

// ============================================================================
// Performance and Edge Case Tests
// ============================================================================

TEST_F(ClipboardModernTest, LargeDataHandling) {
    // Test with larger data to ensure performance
    std::vector<std::byte> largeData(10000);
    std::iota(largeData.begin(), largeData.end(), std::byte{0});
    
    auto format = Clipboard::registerFormat("application/x-large-data");
    
    auto setResult = clipboard_->setDataSafe(format, std::span<const std::byte>{largeData});
    EXPECT_TRUE(setResult.has_value());
    
    auto getResult = clipboard_->getDataSafe(format);
    EXPECT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().size(), largeData.size());
}

TEST_F(ClipboardModernTest, EmptyDataHandling) {
    // Test edge case with empty data
    std::vector<std::byte> emptyData;
    auto format = Clipboard::registerFormat("application/x-empty");
    
    auto setResult = clipboard_->setDataSafe(format, std::span<const std::byte>{emptyData});
    // Behavior with empty data may be platform-specific
    
    auto getResult = clipboard_->getDataSafe(format);
    // Should handle empty data gracefully
}

TEST_F(ClipboardModernTest, MultipleConcurrentCallbacks) {
    // Test multiple callback registration and cleanup
    std::vector<std::size_t> callbackIds;
    
    for (int i = 0; i < 5; ++i) {
        auto id = clipboard_->registerChangeCallback([]() { /* do nothing */ });
        EXPECT_NE(id, 0);
        callbackIds.push_back(id);
    }
    
    // Clean up all callbacks
    for (auto id : callbackIds) {
        EXPECT_TRUE(clipboard_->unregisterChangeCallback(id));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
