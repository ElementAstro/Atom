/*
 * test_convert.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for Windows-specific conversion utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_CONVERT_HPP
#define ATOM_UTILS_TEST_CONVERT_HPP

#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include "atom/utils/conversion/convert.hpp"
#include "atom/error/exception.hpp"

namespace atom::utils::test {

class ConvertTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test strings with various character sets
        asciiString = "Hello, World!";
        utf8String = "Hello, 世界! Café";
        emptyString = "";

        // Wide string equivalents
        asciiWString = L"Hello, World!";
        utf8WString = L"Hello, 世界! Café";
        emptyWString = L"";

        // Long string for performance testing
        longString.reserve(10000);
        for (int i = 0; i < 1000; ++i) {
            longString += "Test string ";
        }

        // Special characters that might cause issues
        specialChars = "Special: \t\n\r\"'\\";
        specialWChars = L"Special: \t\n\r\"'\\";
    }

    void TearDown() override {
        // Clean up any allocated memory if needed
    }

    // Helper function to compare LPWSTR with std::wstring
    bool compareLPWSTRWithWString(LPWSTR lpwstr, const std::wstring& wstr) {
        if (!lpwstr && wstr.empty()) return true;
        if (!lpwstr || wstr.empty()) return false;
        return std::wstring(lpwstr) == wstr;
    }

    // Helper function to compare LPSTR with std::string
    bool compareLPSTRWithString(LPSTR lpstr, const std::string& str) {
        if (!lpstr && str.empty()) return true;
        if (!lpstr || str.empty()) return false;
        return std::string(lpstr) == str;
    }

    // Test data
    std::string asciiString;
    std::string utf8String;
    std::string emptyString;
    std::string longString;
    std::string specialChars;

    std::wstring asciiWString;
    std::wstring utf8WString;
    std::wstring emptyWString;
    std::wstring specialWChars;
};

// Test CharToLPWSTR function
TEST_F(ConvertTest, CharToLPWSTR) {
    // Test with ASCII string
    LPWSTR result = CharToLPWSTR(asciiString);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(result, asciiWString));

    // Test with UTF-8 string
    LPWSTR utf8Result = CharToLPWSTR(utf8String);
    ASSERT_NE(utf8Result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(utf8Result, utf8WString));

    // Test with empty string
    LPWSTR emptyResult = CharToLPWSTR(emptyString);
    ASSERT_NE(emptyResult, nullptr);
    EXPECT_EQ(std::wstring(emptyResult), emptyWString);

    // Test with special characters
    LPWSTR specialResult = CharToLPWSTR(specialChars);
    ASSERT_NE(specialResult, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(specialResult, specialWChars));
}

// Test WCharArrayToString function
TEST_F(ConvertTest, WCharArrayToString) {
    // Test with ASCII wide string
    std::string result = WCharArrayToString(asciiWString.c_str());
    EXPECT_EQ(result, asciiString);

    // Test with UTF-8 wide string
    std::string utf8Result = WCharArrayToString(utf8WString.c_str());
    EXPECT_EQ(utf8Result, utf8String);

    // Test with empty wide string
    std::string emptyResult = WCharArrayToString(emptyWString.c_str());
    EXPECT_EQ(emptyResult, emptyString);

    // Test with null pointer
    EXPECT_THROW(WCharArrayToString(nullptr), std::exception);
}

// Test StringToLPSTR function
TEST_F(ConvertTest, StringToLPSTR) {
    // Test with ASCII string
    LPSTR result = StringToLPSTR(asciiString);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(compareLPSTRWithString(result, asciiString));

    // Test with empty string
    LPSTR emptyResult = StringToLPSTR(emptyString);
    ASSERT_NE(emptyResult, nullptr);
    EXPECT_EQ(std::string(emptyResult), emptyString);

    // Test with special characters
    LPSTR specialResult = StringToLPSTR(specialChars);
    ASSERT_NE(specialResult, nullptr);
    EXPECT_TRUE(compareLPSTRWithString(specialResult, specialChars));
}

// Test WStringToLPSTR function
TEST_F(ConvertTest, WStringToLPSTR) {
    // Test with ASCII wide string
    LPSTR result = WStringToLPSTR(asciiWString);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(compareLPSTRWithString(result, asciiString));

    // Test with empty wide string
    LPSTR emptyResult = WStringToLPSTR(emptyWString);
    ASSERT_NE(emptyResult, nullptr);
    EXPECT_EQ(std::string(emptyResult), emptyString);
}

// Test StringToLPWSTR function
TEST_F(ConvertTest, StringToLPWSTR) {
    // Test with ASCII string
    LPWSTR result = StringToLPWSTR(asciiString);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(result, asciiWString));

    // Test with UTF-8 string
    LPWSTR utf8Result = StringToLPWSTR(utf8String);
    ASSERT_NE(utf8Result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(utf8Result, utf8WString));

    // Test with empty string
    LPWSTR emptyResult = StringToLPWSTR(emptyString);
    ASSERT_NE(emptyResult, nullptr);
    EXPECT_EQ(std::wstring(emptyResult), emptyWString);
}

// Test LPWSTRToString function
TEST_F(ConvertTest, LPWSTRToString) {
    // Create LPWSTR from wide string
    LPWSTR lpwstr = StringToLPWSTR(asciiString);
    ASSERT_NE(lpwstr, nullptr);

    // Convert back to string
    std::string result = LPWSTRToString(lpwstr);
    EXPECT_EQ(result, asciiString);

    // Test with null pointer
    EXPECT_THROW(LPWSTRToString(nullptr), std::exception);
}

// Test LPCWSTRToString function
TEST_F(ConvertTest, LPCWSTRToString) {
    // Test with const wide string
    LPCWSTR lpcwstr = asciiWString.c_str();
    std::string result = LPCWSTRToString(lpcwstr);
    EXPECT_EQ(result, asciiString);

    // Test with empty const wide string
    LPCWSTR emptyLpcwstr = emptyWString.c_str();
    std::string emptyResult = LPCWSTRToString(emptyLpcwstr);
    EXPECT_EQ(emptyResult, emptyString);

    // Test with null pointer
    EXPECT_THROW(LPCWSTRToString(nullptr), std::exception);
}

// Test WStringToLPWSTR function
TEST_F(ConvertTest, WStringToLPWSTR) {
    // Test with ASCII wide string
    LPWSTR result = WStringToLPWSTR(asciiWString);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(result, asciiWString));

    // Test with UTF-8 wide string
    LPWSTR utf8Result = WStringToLPWSTR(utf8WString);
    ASSERT_NE(utf8Result, nullptr);
    EXPECT_TRUE(compareLPWSTRWithWString(utf8Result, utf8WString));

    // Test with empty wide string
    LPWSTR emptyResult = WStringToLPWSTR(emptyWString);
    ASSERT_NE(emptyResult, nullptr);
    EXPECT_EQ(std::wstring(emptyResult), emptyWString);
}

// Test LPWSTRToWString function
TEST_F(ConvertTest, LPWSTRToWString) {
    // Create LPWSTR from wide string
    LPWSTR lpwstr = WStringToLPWSTR(asciiWString);
    ASSERT_NE(lpwstr, nullptr);

    // Convert back to wide string
    std::wstring result = LPWSTRToWString(lpwstr);
    EXPECT_EQ(result, asciiWString);

    // Test with null pointer
    EXPECT_THROW(LPWSTRToWString(nullptr), std::exception);
}

// Test LPCWSTRToWString function
TEST_F(ConvertTest, LPCWSTRToWString) {
    // Test with const wide string
    LPCWSTR lpcwstr = asciiWString.c_str();
    std::wstring result = LPCWSTRToWString(lpcwstr);
    EXPECT_EQ(result, asciiWString);

    // Test with empty const wide string
    LPCWSTR emptyLpcwstr = emptyWString.c_str();
    std::wstring emptyResult = LPCWSTRToWString(emptyLpcwstr);
    EXPECT_EQ(emptyResult, emptyWString);

    // Test with null pointer
    EXPECT_THROW(LPCWSTRToWString(nullptr), std::exception);
}

// Test round-trip conversions
TEST_F(ConvertTest, RoundTripConversions) {
    // String -> LPWSTR -> String
    LPWSTR lpwstr = StringToLPWSTR(asciiString);
    ASSERT_NE(lpwstr, nullptr);
    std::string backToString = LPWSTRToString(lpwstr);
    EXPECT_EQ(backToString, asciiString);

    // WString -> LPWSTR -> WString
    LPWSTR lpwstrFromWString = WStringToLPWSTR(asciiWString);
    ASSERT_NE(lpwstrFromWString, nullptr);
    std::wstring backToWString = LPWSTRToWString(lpwstrFromWString);
    EXPECT_EQ(backToWString, asciiWString);
}

// Test performance with large strings
TEST_F(ConvertTest, LargeStringPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Convert large string multiple times
    for (int i = 0; i < 100; ++i) {
        LPWSTR result = StringToLPWSTR(longString);
        ASSERT_NE(result, nullptr);
        std::string backToString = LPWSTRToString(result);
        EXPECT_EQ(backToString.length(), longString.length());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000) << "Large string conversion took too long: " << duration.count() << "ms";
}

// Test error handling
TEST_F(ConvertTest, ErrorHandling) {
    // Test with very large strings that might cause allocation failures
    std::string veryLargeString(SIZE_MAX / 4, 'A'); // This might fail

    // The function should either succeed or throw an exception, not crash
    EXPECT_NO_THROW({
        try {
            LPWSTR result = StringToLPWSTR(veryLargeString);
            if (result != nullptr) {
                // If it succeeded, verify it's correct
                EXPECT_EQ(std::wstring(result).length(), veryLargeString.length());
            }
        } catch (const std::exception& e) {
            // Exception is acceptable for very large strings
            SUCCEED() << "Exception caught for very large string: " << e.what();
        }
    });
}

}  // namespace atom::utils::test

#else
// Non-Windows platforms - create dummy tests
namespace atom::utils::test {

class ConvertTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ConvertTest, NonWindowsPlatform) {
    // This test just ensures the test file compiles on non-Windows platforms
    SUCCEED() << "Convert utilities are Windows-specific, skipping tests on this platform";
}

}  // namespace atom::utils::test

#endif // _WIN32

#endif  // ATOM_UTILS_TEST_CONVERT_HPP
