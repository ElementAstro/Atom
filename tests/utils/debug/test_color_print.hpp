/*
 * test_color_print.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for color printing utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_COLOR_PRINT_HPP
#define ATOM_UTILS_TEST_COLOR_PRINT_HPP

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <iostream>
#include <thread>
#include <vector>
#include <future>
#include "atom/utils/debug/color_print.hpp"

namespace atom::utils::test {

class ColorPrintTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect cout to capture output for testing
        originalCoutBuffer = std::cout.rdbuf();
        std::cout.rdbuf(capturedOutput.rdbuf());
    }
    
    void TearDown() override {
        // Restore original cout
        std::cout.rdbuf(originalCoutBuffer);
    }
    
    // Helper function to get captured output and clear the buffer
    std::string getCapturedOutput() {
        std::string result = capturedOutput.str();
        capturedOutput.str("");
        capturedOutput.clear();
        return result;
    }
    
    // Helper function to check if string contains ANSI color codes
    bool containsColorCode(const std::string& str, ColorCode color) {
        std::string colorCode = "\033[0;" + std::to_string(static_cast<int>(color)) + "m";
        return str.find(colorCode) != std::string::npos;
    }
    
    // Helper function to check if string contains ANSI style codes
    bool containsStyleCode(const std::string& str, TextStyle style) {
        std::string styleCode = "\033[" + std::to_string(static_cast<int>(style)) + ";";
        return str.find(styleCode) != std::string::npos;
    }
    
    // Helper function to check if string contains reset code
    bool containsResetCode(const std::string& str) {
        return str.find("\033[0m") != std::string::npos;
    }

private:
    std::stringstream capturedOutput;
    std::streambuf* originalCoutBuffer;
};

// Test ColorCode enum values
TEST_F(ColorPrintTest, ColorCodeValues) {
    // Test basic colors
    EXPECT_EQ(static_cast<int>(ColorCode::Black), 30);
    EXPECT_EQ(static_cast<int>(ColorCode::Red), 31);
    EXPECT_EQ(static_cast<int>(ColorCode::Green), 32);
    EXPECT_EQ(static_cast<int>(ColorCode::Yellow), 33);
    EXPECT_EQ(static_cast<int>(ColorCode::Blue), 34);
    EXPECT_EQ(static_cast<int>(ColorCode::Magenta), 35);
    EXPECT_EQ(static_cast<int>(ColorCode::Cyan), 36);
    EXPECT_EQ(static_cast<int>(ColorCode::White), 37);
    
    // Test bright colors
    EXPECT_EQ(static_cast<int>(ColorCode::BrightBlack), 90);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightRed), 91);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightGreen), 92);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightYellow), 93);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightBlue), 94);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightMagenta), 95);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightCyan), 96);
    EXPECT_EQ(static_cast<int>(ColorCode::BrightWhite), 97);
}

// Test TextStyle enum values
TEST_F(ColorPrintTest, TextStyleValues) {
    EXPECT_EQ(static_cast<int>(TextStyle::Normal), 0);
    EXPECT_EQ(static_cast<int>(TextStyle::Bold), 1);
    EXPECT_EQ(static_cast<int>(TextStyle::Dim), 2);
    EXPECT_EQ(static_cast<int>(TextStyle::Italic), 3);
    EXPECT_EQ(static_cast<int>(TextStyle::Underline), 4);
    EXPECT_EQ(static_cast<int>(TextStyle::Blinking), 5);
    EXPECT_EQ(static_cast<int>(TextStyle::Reverse), 7);
    EXPECT_EQ(static_cast<int>(TextStyle::Hidden), 8);
    EXPECT_EQ(static_cast<int>(TextStyle::Strikethrough), 9);
}

// Test basic printColored function
TEST_F(ColorPrintTest, PrintColored) {
    std::string testText = "Hello, World!";
    
    // Test with red color and normal style
    ColorPrinter::printColored(testText, ColorCode::Red);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Red));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(testText) != std::string::npos);
    EXPECT_FALSE(output.find('\n') != std::string::npos); // Should not contain newline
}

// Test printColored with different colors
TEST_F(ColorPrintTest, PrintColoredDifferentColors) {
    std::string testText = "Test";
    
    // Test various colors
    std::vector<ColorCode> colors = {
        ColorCode::Red, ColorCode::Green, ColorCode::Blue, ColorCode::Yellow,
        ColorCode::Cyan, ColorCode::Magenta, ColorCode::White, ColorCode::Black
    };
    
    for (auto color : colors) {
        ColorPrinter::printColored(testText, color);
        std::string output = getCapturedOutput();
        
        EXPECT_TRUE(containsColorCode(output, color));
        EXPECT_TRUE(containsResetCode(output));
        EXPECT_TRUE(output.find(testText) != std::string::npos);
    }
}

// Test printColored with different styles
TEST_F(ColorPrintTest, PrintColoredDifferentStyles) {
    std::string testText = "Styled Text";
    
    // Test various styles
    std::vector<TextStyle> styles = {
        TextStyle::Normal, TextStyle::Bold, TextStyle::Dim, TextStyle::Italic,
        TextStyle::Underline, TextStyle::Blinking, TextStyle::Reverse
    };
    
    for (auto style : styles) {
        ColorPrinter::printColored(testText, ColorCode::Green, style);
        std::string output = getCapturedOutput();
        
        EXPECT_TRUE(containsStyleCode(output, style));
        EXPECT_TRUE(containsColorCode(output, ColorCode::Green));
        EXPECT_TRUE(containsResetCode(output));
        EXPECT_TRUE(output.find(testText) != std::string::npos);
    }
}

// Test printColoredLine function
TEST_F(ColorPrintTest, PrintColoredLine) {
    std::string testText = "Line with newline";
    
    ColorPrinter::printColoredLine(testText, ColorCode::Blue);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Blue));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(testText) != std::string::npos);
    EXPECT_TRUE(output.back() == '\n'); // Should end with newline
}

// Test formatted printColored function
TEST_F(ColorPrintTest, PrintColoredFormatted) {
    ColorPrinter::printColored(ColorCode::Red, TextStyle::Bold, "Number: {}, String: {}", 42, "test");
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Red));
    EXPECT_TRUE(containsStyleCode(output, TextStyle::Bold));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("Number: 42, String: test") != std::string::npos);
}

// Test formatted printColoredLine function
TEST_F(ColorPrintTest, PrintColoredLineFormatted) {
    ColorPrinter::printColoredLine(ColorCode::Green, TextStyle::Normal, "Value: {}", 123);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Green));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("Value: 123") != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test error function
TEST_F(ColorPrintTest, ErrorFunction) {
    std::string errorMsg = "This is an error";
    
    ColorPrinter::error(errorMsg);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Red));
    EXPECT_TRUE(containsStyleCode(output, TextStyle::Bold));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(errorMsg) != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test formatted error function
TEST_F(ColorPrintTest, ErrorFunctionFormatted) {
    ColorPrinter::error("Error code: {}, Message: {}", 404, "Not Found");
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Red));
    EXPECT_TRUE(containsStyleCode(output, TextStyle::Bold));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("Error code: 404, Message: Not Found") != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test warning function
TEST_F(ColorPrintTest, WarningFunction) {
    std::string warningMsg = "This is a warning";
    
    ColorPrinter::warning(warningMsg);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Yellow));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(warningMsg) != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test formatted warning function
TEST_F(ColorPrintTest, WarningFunctionFormatted) {
    ColorPrinter::warning("Warning: {} items remaining", 5);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Yellow));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("Warning: 5 items remaining") != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test success function
TEST_F(ColorPrintTest, SuccessFunction) {
    std::string successMsg = "Operation successful";
    
    ColorPrinter::success(successMsg);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Green));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(successMsg) != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test formatted success function
TEST_F(ColorPrintTest, SuccessFunctionFormatted) {
    ColorPrinter::success("Processed {} files successfully", 10);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Green));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("Processed 10 files successfully") != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test info function
TEST_F(ColorPrintTest, InfoFunction) {
    std::string infoMsg = "Information message";
    
    ColorPrinter::info(infoMsg);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Cyan));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(infoMsg) != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test formatted info function
TEST_F(ColorPrintTest, InfoFunctionFormatted) {
    ColorPrinter::info("System info: {} MB memory", 8192);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Cyan));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find("System info: 8192 MB memory") != std::string::npos);
    EXPECT_TRUE(output.back() == '\n');
}

// Test empty string handling
TEST_F(ColorPrintTest, EmptyStringHandling) {
    ColorPrinter::printColored("", ColorCode::Red);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Red));
    EXPECT_TRUE(containsResetCode(output));
    // Should still contain color codes even with empty text
}

// Test special characters
TEST_F(ColorPrintTest, SpecialCharacters) {
    std::string specialText = "Special: \t\n\r\"'\\";
    
    ColorPrinter::printColored(specialText, ColorCode::Magenta);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Magenta));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(specialText) != std::string::npos);
}

// Test very long strings
TEST_F(ColorPrintTest, LongStrings) {
    std::string longText(10000, 'A');
    
    ColorPrinter::printColored(longText, ColorCode::Blue);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, ColorCode::Blue));
    EXPECT_TRUE(containsResetCode(output));
    EXPECT_TRUE(output.find(longText) != std::string::npos);
}

// Test thread safety
TEST_F(ColorPrintTest, ThreadSafety) {
    const int numThreads = 10;
    const int messagesPerThread = 100;
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [i, messagesPerThread]() {
            for (int j = 0; j < messagesPerThread; ++j) {
                ColorPrinter::info("Thread {} message {}", i, j);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        EXPECT_NO_THROW(future.get());
    }
    
    // If we reach here without crashes, thread safety test passed
    SUCCEED();
}

// Test namespace aliases
TEST_F(ColorPrintTest, NamespaceAliases) {
    // Test that the aliases in atom::test namespace work
    using atom::test::ColorCode;
    using atom::test::TextStyle;
    using atom::test::ColorPrinter;
    
    // These should compile and work the same as the original
    ColorPrinter::printColored("Test", ColorCode::Red, TextStyle::Bold);
    std::string output = getCapturedOutput();
    
    EXPECT_TRUE(containsColorCode(output, atom::utils::ColorCode::Red));
    EXPECT_TRUE(containsStyleCode(output, atom::utils::TextStyle::Bold));
    EXPECT_TRUE(containsResetCode(output));
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_COLOR_PRINT_HPP
