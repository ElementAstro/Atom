/*
 * test_error_formatting.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit tests for error formatting system

**************************************************/

#include <gtest/gtest.h>
#include <sstream>

#include "atom/error/context/error_context.hpp"
#include "atom/error/handler/error_reporter.hpp"

namespace atom::error::test {

class ErrorFormattingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test error context
        testContext_ = ErrorContext::create(100, "Test file not found error");
        testContext_->addTag("test");
        testContext_->addTag("file-system");
        testContext_->setCorrelationId("test-correlation-123");
        testContext_->setSystemInfo("test_field", "test_value");
        testContext_->setStackTrace("Stack trace line 1\nStack trace line 2");
    }

    void TearDown() override { ErrorContextManager::getInstance().clear(); }

    std::shared_ptr<ErrorContext> testContext_;
};

// ============================================================================
// Plain Text Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, PlainTextFormatter) {
    PlainTextFormatter formatter;

    std::string formatted = formatter.format(testContext_);

    // Check that all expected elements are present
    EXPECT_TRUE(formatted.find("ERROR REPORT") != std::string::npos);
    EXPECT_TRUE(formatted.find("Error ID:") != std::string::npos);
    EXPECT_TRUE(formatted.find("Error Code: 100") != std::string::npos);
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("Severity: ERROR") != std::string::npos);
    EXPECT_TRUE(formatted.find("Category: IO") != std::string::npos);
    EXPECT_TRUE(formatted.find("Correlation ID: test-correlation-123") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("Tags: test, file-system") != std::string::npos);
    EXPECT_TRUE(formatted.find("Stack trace line 1") != std::string::npos);
}

TEST_F(ErrorFormattingTest, PlainTextFormatterOptions) {
    PlainTextFormatter formatter;

    // Test disabling stack trace
    formatter.setOption("include_stack_trace", "false");
    std::string formatted = formatter.format(testContext_);
    EXPECT_TRUE(formatted.find("Stack trace line 1") == std::string::npos);

    // Test disabling system info
    formatter.setOption("include_system_info", "false");
    formatted = formatter.format(testContext_);
    EXPECT_TRUE(formatted.find("System Information") == std::string::npos);

    // Test option retrieval
    EXPECT_EQ(formatter.getOption("include_stack_trace"), "false");
    EXPECT_EQ(formatter.getOption("include_system_info"), "false");
}

// ============================================================================
// JSON Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, JsonFormatter) {
    JsonFormatter formatter;

    std::string formatted = formatter.format(testContext_);

    // Check JSON structure
    EXPECT_TRUE(formatted.find("\"errorId\":") != std::string::npos);
    EXPECT_TRUE(formatted.find("\"errorCode\": 100") != std::string::npos);
    EXPECT_TRUE(formatted.find("\"message\": \"Test file not found error\"") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("\"severity\": \"ERROR\"") != std::string::npos);
    EXPECT_TRUE(formatted.find("\"category\": \"IO\"") != std::string::npos);
    EXPECT_TRUE(formatted.find("\"correlationId\": \"test-correlation-123\"") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("\"tags\": [\"test\", \"file-system\"]") !=
                std::string::npos);
}

TEST_F(ErrorFormattingTest, JsonFormatterMultiple) {
    JsonFormatter formatter;

    auto context2 = ErrorContext::create(200, "Second error");
    std::vector<std::shared_ptr<ErrorContext>> contexts = {testContext_,
                                                           context2};

    std::string formatted = formatter.formatMultiple(contexts);

    // Should be a JSON array
    EXPECT_TRUE(formatted.front() == '[');
    EXPECT_TRUE(formatted.back() == ']');
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("Second error") != std::string::npos);
}

TEST_F(ErrorFormattingTest, JsonFormatterOptions) {
    JsonFormatter formatter;

    // Test compact formatting
    formatter.setOption("pretty_print", "false");
    std::string formatted = formatter.format(testContext_);

    // Compact JSON should have no extra whitespace
    EXPECT_TRUE(formatted.find("\n") == std::string::npos);

    // Test indent size
    formatter.setOption("pretty_print", "true");
    formatter.setOption("indent_size", "4");
    formatted = formatter.format(testContext_);

    // Should contain 4-space indentation
    EXPECT_TRUE(formatted.find("    \"errorId\":") != std::string::npos);
}

// ============================================================================
// Colored Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, ColoredFormatter) {
    ColoredFormatter formatter;

    std::string formatted = formatter.format(testContext_);

    // Check for ANSI color codes
    EXPECT_TRUE(formatted.find("\033[") !=
                std::string::npos);  // ANSI escape sequence
    EXPECT_TRUE(formatted.find("ERROR REPORT") != std::string::npos);
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
}

TEST_F(ErrorFormattingTest, ColoredFormatterDisabled) {
    ColoredFormatter formatter;
    formatter.setOption("enable_colors", "false");

    std::string formatted = formatter.format(testContext_);

    // Should not contain ANSI color codes
    EXPECT_TRUE(formatted.find("\033[") == std::string::npos);
    EXPECT_TRUE(formatted.find("ERROR REPORT") != std::string::npos);
}

// ============================================================================
// HTML Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, HtmlFormatter) {
    HtmlFormatter formatter;

    std::string formatted = formatter.format(testContext_);

    // Check HTML structure
    EXPECT_TRUE(formatted.find("<style>") != std::string::npos);
    EXPECT_TRUE(formatted.find("<div class=\"error-report") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("Error Report") != std::string::npos);
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("</div>") != std::string::npos);
}

TEST_F(ErrorFormattingTest, HtmlFormatterNoCSS) {
    HtmlFormatter formatter;
    formatter.setOption("include_css", "false");

    std::string formatted = formatter.format(testContext_);

    // Should not contain CSS
    EXPECT_TRUE(formatted.find("<style>") == std::string::npos);
    EXPECT_TRUE(formatted.find("<div class=\"error-report") !=
                std::string::npos);
}

TEST_F(ErrorFormattingTest, HtmlFormatterMultiple) {
    HtmlFormatter formatter;

    auto context2 = ErrorContext::create(200, "Second error");
    std::vector<std::shared_ptr<ErrorContext>> contexts = {testContext_,
                                                           context2};

    std::string formatted = formatter.formatMultiple(contexts);

    // Should contain both errors and only one CSS block
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("Second error") != std::string::npos);
    EXPECT_TRUE(formatted.find("<div class=\"error-reports\">") !=
                std::string::npos);

    // Count CSS blocks (should be only one)
    size_t cssCount = 0;
    size_t pos = 0;
    while ((pos = formatted.find("<style>", pos)) != std::string::npos) {
        cssCount++;
        pos += 7;
    }
    EXPECT_EQ(cssCount, 1);
}

// ============================================================================
// Structured Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, StructuredFormatter) {
    StructuredFormatter formatter;

    std::string formatted = formatter.format(testContext_);

    // Check structured format
    EXPECT_TRUE(formatted.find("severity=ERROR") != std::string::npos);
    EXPECT_TRUE(formatted.find("category=IO") != std::string::npos);
    EXPECT_TRUE(formatted.find("code=100") != std::string::npos);
    EXPECT_TRUE(formatted.find("message=Test file not found error") !=
                std::string::npos);
}

TEST_F(ErrorFormattingTest, StructuredFormatterCustomSeparators) {
    StructuredFormatter formatter;
    formatter.setOption("field_separator", " | ");
    formatter.setOption("key_value_separator", ": ");

    std::string formatted = formatter.format(testContext_);

    // Check custom separators
    EXPECT_TRUE(formatted.find("severity: ERROR") != std::string::npos);
    EXPECT_TRUE(formatted.find(" | ") != std::string::npos);
}

// ============================================================================
// Error Formatter Factory Tests
// ============================================================================

TEST_F(ErrorFormattingTest, ErrorFormatterFactory) {
    // Test creating different formatters
    auto plainFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Plain);
    EXPECT_NE(plainFormatter, nullptr);

    auto jsonFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Json);
    EXPECT_NE(jsonFormatter, nullptr);

    auto coloredFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Colored);
    EXPECT_NE(coloredFormatter, nullptr);

    auto htmlFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Html);
    EXPECT_NE(htmlFormatter, nullptr);

    auto structuredFormatter =
        ErrorFormatterFactory::createFormatter(OutputFormat::Structured);
    EXPECT_NE(structuredFormatter, nullptr);

    // Test that they produce different output
    std::string plainOutput = plainFormatter->format(testContext_);
    std::string jsonOutput = jsonFormatter->format(testContext_);

    EXPECT_NE(plainOutput, jsonOutput);
}

TEST_F(ErrorFormattingTest, CustomFormatter) {
    // Register a custom formatter
    ErrorFormatterFactory::registerFormatter(
        "custom", []() -> std::unique_ptr<ErrorFormatter> {
            return std::make_unique<PlainTextFormatter>();
        });

    auto customFormatter =
        ErrorFormatterFactory::createCustomFormatter("custom");
    EXPECT_NE(customFormatter, nullptr);

    // Test that it works
    std::string formatted = customFormatter->format(testContext_);
    EXPECT_TRUE(formatted.find("ERROR REPORT") != std::string::npos);

    // Test available formatters list
    auto available = ErrorFormatterFactory::getAvailableFormatters();
    EXPECT_TRUE(std::find(available.begin(), available.end(), "custom") !=
                available.end());
}

// ============================================================================
// Template Formatter Tests
// ============================================================================

TEST_F(ErrorFormattingTest, TemplateFormatter) {
    std::string templateStr =
        "Error {error_code}: {message} (Severity: {severity})";
    TemplateFormatter formatter(templateStr);

    std::string formatted = formatter.format(testContext_);

    // Template variables should be replaced
    EXPECT_TRUE(formatted.find("Error 100:") != std::string::npos);
    EXPECT_TRUE(formatted.find("Test file not found error") !=
                std::string::npos);
    EXPECT_TRUE(formatted.find("(Severity: ERROR)") != std::string::npos);
}

// ============================================================================
// Convenience Macros Tests
// ============================================================================

TEST_F(ErrorFormattingTest, ConvenienceMacros) {
    std::string plainFormatted = FORMAT_ERROR_PLAIN(testContext_);
    EXPECT_TRUE(plainFormatted.find("ERROR REPORT") != std::string::npos);

    std::string jsonFormatted = FORMAT_ERROR_JSON(testContext_);
    EXPECT_TRUE(jsonFormatted.find("\"errorCode\": 100") != std::string::npos);

    std::string coloredFormatted = FORMAT_ERROR_COLORED(testContext_);
    EXPECT_TRUE(coloredFormatted.find("ERROR REPORT") != std::string::npos);
}

}  // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
