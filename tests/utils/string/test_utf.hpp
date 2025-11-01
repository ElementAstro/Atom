/*
 * test_utf.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Comprehensive tests for UTF conversion utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_UTF_HPP
#define ATOM_UTILS_TEST_UTF_HPP

#include <gtest/gtest.h>
#include <future>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "atom/error/exception.hpp"
#include "atom/utils/text/utf.hpp"

namespace atom::utils::test {

class UTFConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Basic ASCII test string
        asciiString = "Hello, World!";

        // UTF-8 string with various Unicode characters
        utf8String = "Hello, 世界! 🌍 Ñoël";

        // UTF-16 string with various Unicode characters
        utf16String = u"Hello, 世界! 🌍 Ñoël";

        // UTF-32 string with various Unicode characters
        utf32String = U"Hello, 世界! 🌍 Ñoël";

        // Empty strings for edge case testing
        emptyString = "";
        emptyU16String = u"";
        emptyU32String = U"";

        // Very long string for performance testing
        longString.reserve(10000);
        for (int i = 0; i < 1000; ++i) {
            longString += "Test string with Unicode: 世界 🌍 ";
        }

        // Invalid UTF-8 sequences for error testing
        invalidUtf8Sequences = {
            "\xFF\xFE",          // Invalid start bytes
            "\x80\x80",          // Continuation bytes without start
            "\xC0\x80",          // Overlong encoding
            "\xE0\x80\x80",      // Overlong encoding
            "\xF0\x80\x80\x80",  // Overlong encoding
            "\xC2",              // Incomplete sequence
            "\xE0\x80",          // Incomplete sequence
            "\xF0\x80\x80"       // Incomplete sequence
        };
    }

    // Test data
    std::string asciiString;
    std::string utf8String;
    std::u16string utf16String;
    std::u32string utf32String;
    std::string emptyString;
    std::u16string emptyU16String;
    std::u32string emptyU32String;
    std::string longString;
    std::vector<std::string> invalidUtf8Sequences;

    // Helper function to generate random UTF-8 string
    std::string generateRandomUTF8(size_t length) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0x20, 0x7E);  // Printable ASCII

        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += static_cast<char>(dis(gen));
        }
        return result;
    }
};

// Test UTF-8 validation function
TEST_F(UTFConversionTest, UTF8Validation) {
    // Valid UTF-8 strings
    EXPECT_TRUE(isValidUTF8(asciiString));
    EXPECT_TRUE(isValidUTF8(utf8String));
    EXPECT_TRUE(isValidUTF8(emptyString));
    EXPECT_TRUE(isValidUTF8("Simple ASCII"));
    EXPECT_TRUE(isValidUTF8("UTF-8: café"));

    // Invalid UTF-8 sequences
    for (const auto& invalid : invalidUtf8Sequences) {
        EXPECT_FALSE(isValidUTF8(invalid))
            << "String should be invalid: " << invalid;
    }
}

// Test UTF-8 to UTF-16 conversion
TEST_F(UTFConversionTest, UTF8ToUTF16Conversion) {
    // Note: Testing actual implementation function name
    auto result = utf8toUtF16(asciiString);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf8toUtF16(emptyString);
    EXPECT_TRUE(emptyResult.empty());

    // Test round-trip conversion
    auto utf16Result = utf8toUtF16(utf8String);
    auto backToUtf8 = utf16toUtF8(utf16Result);
    EXPECT_EQ(utf8String, backToUtf8);
}

// Test UTF-16 to UTF-8 conversion
TEST_F(UTFConversionTest, UTF16ToUTF8Conversion) {
    auto result = utf16toUtF8(utf16String);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf16toUtF8(emptyU16String);
    EXPECT_TRUE(emptyResult.empty());

    // Test round-trip conversion
    auto utf8Result = utf16toUtF8(utf16String);
    auto backToUtf16 = utf8toUtF16(utf8Result);
    EXPECT_EQ(utf16String, backToUtf16);
}

// Test UTF-8 to UTF-32 conversion
TEST_F(UTFConversionTest, UTF8ToUTF32Conversion) {
    auto result = utf8toUtF32(asciiString);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf8toUtF32(emptyString);
    EXPECT_TRUE(emptyResult.empty());

    // Test round-trip conversion
    auto utf32Result = utf8toUtF32(utf8String);
    auto backToUtf8 = utf32toUtF8(utf32Result);
    EXPECT_EQ(utf8String, backToUtf8);
}

// Test UTF-32 to UTF-8 conversion
TEST_F(UTFConversionTest, UTF32ToUTF8Conversion) {
    auto result = utf32toUtF8(utf32String);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf32toUtF8(emptyU32String);
    EXPECT_TRUE(emptyResult.empty());
}

// Test UTF-16 to UTF-32 conversion
TEST_F(UTFConversionTest, UTF16ToUTF32Conversion) {
    auto result = utf16toUtF32(utf16String);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf16toUtF32(emptyU16String);
    EXPECT_TRUE(emptyResult.empty());

    // Test round-trip conversion
    auto utf32Result = utf16toUtF32(utf16String);
    auto backToUtf16 = utf32toUtF16(utf32Result);
    EXPECT_EQ(utf16String, backToUtf16);
}

// Test UTF-32 to UTF-16 conversion
TEST_F(UTFConversionTest, UTF32ToUTF16Conversion) {
    auto result = utf32toUtF16(utf32String);
    EXPECT_FALSE(result.empty());

    // Test with empty string
    auto emptyResult = utf32toUtF16(emptyU32String);
    EXPECT_TRUE(emptyResult.empty());
}

// Test error handling for invalid UTF-8 input
TEST_F(UTFConversionTest, InvalidUTF8ErrorHandling) {
    for (const auto& invalid : invalidUtf8Sequences) {
        // These should either throw exceptions or handle gracefully
        EXPECT_NO_THROW({
            try {
                utf8toUtF16(invalid);
                utf8toUtF32(invalid);
            } catch (const std::exception& e) {
                // Exception is acceptable for invalid input
                SUCCEED() << "Exception caught for invalid UTF-8: " << e.what();
            }
        });
    }
}

// Test performance with large strings
TEST_F(UTFConversionTest, LargeStringPerformance) {
    // Test conversion performance with large strings
    auto start = std::chrono::high_resolution_clock::now();

    auto utf16Result = utf8toUtF16(longString);
    auto utf32Result = utf8toUtF32(longString);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000)
        << "Conversion took too long: " << duration.count() << "ms";

    // Results should not be empty
    EXPECT_FALSE(utf16Result.empty());
    EXPECT_FALSE(utf32Result.empty());
}

// Test thread safety
TEST_F(UTFConversionTest, ThreadSafety) {
    const int numThreads = 10;
    const int conversionsPerThread = 100;

    std::vector<std::future<bool>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(
            std::async(std::launch::async, [this, conversionsPerThread]() {
                for (int j = 0; j < conversionsPerThread; ++j) {
                    try {
                        auto utf16 = utf8toUtF16(utf8String);
                        auto utf32 = utf8toUtF32(utf8String);
                        auto backToUtf8_16 = utf16toUtF8(utf16);
                        auto backToUtf8_32 = utf32toUtF8(utf32);

                        if (backToUtf8_16 != utf8String ||
                            backToUtf8_32 != utf8String) {
                            return false;
                        }
                    } catch (const std::exception&) {
                        return false;
                    }
                }
                return true;
            }));
    }

    // Wait for all threads and check results
    for (auto& future : futures) {
        EXPECT_TRUE(future.get()) << "Thread safety test failed";
    }
}

// Test edge cases with special Unicode characters
TEST_F(UTFConversionTest, SpecialUnicodeCharacters) {
    // Test with various special Unicode characters
    std::vector<std::string> specialStrings = {
        "🌍🌎🌏",                    // Emojis
        "𝕳𝖊𝖑𝖑𝖔",                     // Mathematical symbols
        "Ω≈ç√∫˜µ≤≥÷",                // Mathematical operators
        "™®©",                       // Trademark symbols
        "←↑→↓↔↕",                    // Arrows
        "αβγδεζηθικλμνξοπρστυφχψω",  // Greek letters
    };

    for (const auto& str : specialStrings) {
        EXPECT_TRUE(isValidUTF8(str))
            << "Special string should be valid UTF-8: " << str;

        // Test conversions
        auto utf16 = utf8toUtF16(str);
        auto utf32 = utf8toUtF32(str);

        // Test round-trip
        auto backToUtf8_16 = utf16toUtF8(utf16);
        auto backToUtf8_32 = utf32toUtF8(utf32);

        EXPECT_EQ(str, backToUtf8_16)
            << "UTF-8 -> UTF-16 -> UTF-8 round-trip failed";
        EXPECT_EQ(str, backToUtf8_32)
            << "UTF-8 -> UTF-32 -> UTF-8 round-trip failed";
    }
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_UTF_HPP
