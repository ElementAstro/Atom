// filepath: /home/max/Atom-1/atom/utils/test_valid_string.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "valid_string.hpp"

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "valid_string.hpp"

using namespace atom::utils;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

class ValidStringTest : public ::testing::Test {
protected:
    // Test various string representations
    std::string stdString = "This is a (test) string with [brackets]";
    std::string_view stdStringView =
        "This is a {test} string view with <brackets>";
    const char* cString = "This is a C-style string with (nested [brackets])";

    // Helper method to validate bracket error messages
    void expectErrorMessageForBracket(const std::string& message, char bracket,
                                      int position, bool isOpening) {
        std::string expectedStart;
        if (isOpening) {
            expectedStart = "Error: Opening bracket '" +
                            std::string(1, bracket) + "' at position " +
                            std::to_string(position);
        } else {
            expectedStart = "Error: Closing bracket '" +
                            std::string(1, bracket) + "' at position " +
                            std::to_string(position);
        }
        EXPECT_THAT(message, HasSubstr(expectedStart));
    }
};

// Basic validation tests with various string types
TEST_F(ValidStringTest, BasicValidation) {
    // Test with std::string
    auto result1 = isValidBracket(stdString);
    EXPECT_TRUE(result1.isValid);
    EXPECT_TRUE(result1.invalidBrackets.empty());
    EXPECT_TRUE(result1.errorMessages.empty());

    // Test with std::string_view
    auto result2 = isValidBracket(stdStringView);
    EXPECT_TRUE(result2.isValid);
    EXPECT_TRUE(result2.invalidBrackets.empty());
    EXPECT_TRUE(result2.errorMessages.empty());

    // Test with C-style string
    // auto result3 = isValidBracket(cString);
    // EXPECT_TRUE(result3.isValid);
    // EXPECT_TRUE(result3.invalidBrackets.empty());
    // EXPECT_TRUE(result3.errorMessages.empty());

    // Test with string literal
    auto result4 = isValidBracket("Simple (test) with [brackets]");
    EXPECT_TRUE(result4.isValid);
    EXPECT_TRUE(result4.invalidBrackets.empty());
    EXPECT_TRUE(result4.errorMessages.empty());
}

// Test with empty strings
TEST_F(ValidStringTest, EmptyStrings) {
    // Empty std::string
    auto result1 = isValidBracket(std::string());
    EXPECT_TRUE(result1.isValid);
    EXPECT_TRUE(result1.invalidBrackets.empty());
    EXPECT_TRUE(result1.errorMessages.empty());

    // Empty std::string_view
    auto result2 = isValidBracket(std::string_view());
    EXPECT_TRUE(result2.isValid);
    EXPECT_TRUE(result2.invalidBrackets.empty());
    EXPECT_TRUE(result2.errorMessages.empty());

    // Empty C-style string
    auto result3 = isValidBracket("");
    EXPECT_TRUE(result3.isValid);
    EXPECT_TRUE(result3.invalidBrackets.empty());
    EXPECT_TRUE(result3.errorMessages.empty());
}

// Test for mismatched brackets
TEST_F(ValidStringTest, MismatchedBrackets) {
    // Missing closing bracket
    auto result1 =
        isValidBracket("This has an opening bracket ( but no closing bracket");
    EXPECT_FALSE(result1.isValid);
    ASSERT_EQ(result1.invalidBrackets.size(), 1);
    EXPECT_EQ(result1.invalidBrackets[0].character, '(');
    EXPECT_EQ(result1.invalidBrackets[0].position, 23);
    ASSERT_EQ(result1.errorMessages.size(), 1);
    expectErrorMessageForBracket(result1.errorMessages[0], '(', 23, true);

    // Missing opening bracket
    auto result2 =
        isValidBracket("This has a closing bracket ) but no opening bracket");
    EXPECT_FALSE(result2.isValid);
    ASSERT_EQ(result2.invalidBrackets.size(), 1);
    EXPECT_EQ(result2.invalidBrackets[0].character, ')');
    EXPECT_EQ(result2.invalidBrackets[0].position, 24);
    ASSERT_EQ(result2.errorMessages.size(), 1);
    expectErrorMessageForBracket(result2.errorMessages[0], ')', 24, false);
}

// Test for nested and complex bracket patterns
TEST_F(ValidStringTest, NestedBrackets) {
    // Valid nested brackets
    auto result1 = isValidBracket("Nested brackets: ([{<>}])");
    EXPECT_TRUE(result1.isValid);
    EXPECT_TRUE(result1.invalidBrackets.empty());
    EXPECT_TRUE(result1.errorMessages.empty());

    // Invalid nested brackets with missing inner bracket
    auto result2 = isValidBracket("Nested brackets with error: ([{>}])");
    EXPECT_FALSE(result2.isValid);
    ASSERT_EQ(result2.invalidBrackets.size(), 1);
    EXPECT_EQ(result2.invalidBrackets[0].character, '>');
    ASSERT_EQ(result2.errorMessages.size(), 1);

    // Complex invalid case with multiple errors
    auto result3 = isValidBracket("Multiple errors: ([)] and {>");
    EXPECT_FALSE(result3.isValid);
    ASSERT_EQ(result3.invalidBrackets.size(),
              3);  // ')' mismatch, '[' unclosed, '>' mismatch
    ASSERT_EQ(result3.errorMessages.size(), 3);
}

// Test for quoted sections where brackets should be ignored
TEST_F(ValidStringTest, QuotedSections) {
    // Brackets inside single quotes should be ignored
    auto result1 =
        isValidBracket("This has brackets in quotes: '(not a real bracket)'");
    EXPECT_TRUE(result1.isValid);

    // Brackets inside double quotes should be ignored
    auto result2 = isValidBracket(
        "This has brackets in double quotes: \"[not a real bracket]\"");
    EXPECT_TRUE(result2.isValid);

    // Mixed quotes with valid brackets outside quotes
    auto result3 = isValidBracket(
        "(Valid bracket) with quotes: 'invalid )(' and \"[also ignored]\"");
    EXPECT_TRUE(result3.isValid);

    // Unclosed single quote
    auto result4 = isValidBracket("This has an unclosed quote: '");
    EXPECT_FALSE(result4.isValid);
    EXPECT_THAT(result4.errorMessages,
                Contains(HasSubstr("Single quote is not closed")));

    // Unclosed double quote
    auto result5 = isValidBracket("This has an unclosed double quote: \"");
    EXPECT_FALSE(result5.isValid);
    EXPECT_THAT(result5.errorMessages,
                Contains(HasSubstr("Double quote is not closed")));
}

// Test for escaped quotes
TEST_F(ValidStringTest, EscapedQuotes) {
    // Escaped single quote
    auto result1 = isValidBracket("This has an escaped quote: \\'");
    EXPECT_TRUE(result1.isValid);

    // Escaped double quote
    auto result2 = isValidBracket("This has an escaped double quote: \\\"");
    EXPECT_TRUE(result2.isValid);

    // Complex escaped sequences
    auto result3 =
        isValidBracket("Complex escapes: '\\'' and \"\\\"\" and \\\\");
    EXPECT_TRUE(result3.isValid);

    // Escaped backslashes before quotes
    auto result4 = isValidBracket("Escaped backslashes: \\\\'");
    EXPECT_FALSE(result4.isValid);  // The quote is still unclosed
    EXPECT_THAT(result4.errorMessages,
                Contains(HasSubstr("Single quote is not closed")));
}

// Test for very large strings that should trigger parallel processing
TEST_F(ValidStringTest, LargeStrings) {
    // Create a large valid string
    std::string largeValidString(20000, 'a');  // Base string
    largeValidString += "([{<>}])";            // Valid brackets at the end

    auto result1 = isValidBracket(largeValidString);
    EXPECT_TRUE(result1.isValid);

    // Create a large invalid string
    std::string largeInvalidString(20000, 'b');  // Base string
    largeInvalidString += "([{<>])";  // Missing closing bracket for '{'

    auto result2 = isValidBracket(largeInvalidString);
    EXPECT_FALSE(result2.isValid);
    ASSERT_EQ(result2.invalidBrackets.size(), 1);
    EXPECT_EQ(result2.invalidBrackets[0].character, '{');
    EXPECT_EQ(result2.invalidBrackets[0].position, 20002);
}

// Test for positions reported in error messages
TEST_F(ValidStringTest, ErrorPositions) {
    std::string testStr = "Position test: ) and ( and ] and [";

    auto result = isValidBracket(testStr);
    EXPECT_FALSE(result.isValid);
    ASSERT_EQ(result.invalidBrackets.size(), 4);

    // Check error positions
    EXPECT_EQ(result.invalidBrackets[0].character, ')');
    EXPECT_EQ(result.invalidBrackets[0].position, 15);

    EXPECT_EQ(result.invalidBrackets[1].character, '(');
    EXPECT_EQ(result.invalidBrackets[1].position, 21);

    EXPECT_EQ(result.invalidBrackets[2].character, ']');
    EXPECT_EQ(result.invalidBrackets[2].position, 28);

    EXPECT_EQ(result.invalidBrackets[3].character, '[');
    EXPECT_EQ(result.invalidBrackets[3].position, 34);
}

// Test with the validateBracketsWithExceptions function
TEST_F(ValidStringTest, ValidateWithExceptions) {
    // Valid string should not throw
    EXPECT_NO_THROW(validateBracketsWithExceptions("(This) is [valid]"));

    // Invalid string should throw with the correct message
    try {
        validateBracketsWithExceptions("(This is not valid]");
        FAIL() << "Expected ValidationException";
    } catch (const ValidationException& e) {
        // The exception should contain error information
        auto result = e.getResult();
        EXPECT_FALSE(result.isValid);
        EXPECT_FALSE(result.errorMessages.empty());
    }
}

// Test compile-time validation using validateBrackets
TEST_F(ValidStringTest, CompileTimeValidation) {
    // This should work at compile time
    constexpr auto result1 = validateBrackets("Compile time (valid) test");
    EXPECT_TRUE(result1.isValid());
    EXPECT_EQ(result1.getErrorCount(), 0);

    // This should detect errors at compile time
    constexpr auto result2 = validateBrackets("Compile time (invalid] test");
    EXPECT_FALSE(result2.isValid());
    EXPECT_GT(result2.getErrorCount(), 0);

    // Check error positions
    auto positions = result2.getErrorPositions();
    EXPECT_FALSE(std::span<const int>(positions).empty());
}

// Test the toArray helper function
TEST_F(ValidStringTest, ToArray) {
    auto arr = toArray("Test string");
    EXPECT_EQ(arr.size(), 12);  // 11 chars + null terminator
    EXPECT_EQ(arr[0], 'T');
    EXPECT_EQ(arr[10], 'g');
    EXPECT_EQ(arr[11], '\0');
}

// Test with different bracket types
TEST_F(ValidStringTest, DifferentBracketTypes) {
    // Test parentheses
    auto result1 = isValidBracket("Parentheses test: (text)");
    EXPECT_TRUE(result1.isValid);

    // Test square brackets
    auto result2 = isValidBracket("Square brackets test: [text]");
    EXPECT_TRUE(result2.isValid);

    // Test curly braces
    auto result3 = isValidBracket("Curly braces test: {text}");
    EXPECT_TRUE(result3.isValid);

    // Test angle brackets
    auto result4 = isValidBracket("Angle brackets test: <text>");
    EXPECT_TRUE(result4.isValid);

    // Test mixed brackets
    auto result5 = isValidBracket("Mixed brackets: ([{<text>}])");
    EXPECT_TRUE(result5.isValid);
}

// Test with incorrect bracket pairings
TEST_F(ValidStringTest, IncorrectPairings) {
    auto result1 = isValidBracket("Wrong pairing: (]");
    EXPECT_FALSE(result1.isValid);
    ASSERT_EQ(result1.invalidBrackets.size(), 2);
    EXPECT_EQ(result1.invalidBrackets[0].character, ']');
    EXPECT_EQ(result1.invalidBrackets[1].character, '(');

    auto result2 = isValidBracket("Multiple wrong pairings: ([)]");
    EXPECT_FALSE(result2.isValid);
    ASSERT_EQ(result2.invalidBrackets.size(), 2);
}

// Test the validateString helper function
TEST_F(ValidStringTest, ValidateStringHelper) {
    // With string literal
    auto result1 = validateString("Helper function test (valid)");
    EXPECT_TRUE(result1.isValid);

    // With std::string
    std::string stdStr = "Helper with std::string [valid]";
    auto result2 = validateString(stdStr);
    EXPECT_TRUE(result2.isValid);

    // With invalid string
    auto result3 = validateString("Helper function test (invalid]");
    EXPECT_FALSE(result3.isValid);
}

// Test with multiple errors in complex text
TEST_F(ValidStringTest, ComplexErrorCases) {
    std::string complexStr =
        "Complex (test [with {multiple errors) ] and unclosed brackets";

    auto result = isValidBracket(complexStr);
    EXPECT_FALSE(result.isValid);
    ASSERT_GE(result.invalidBrackets.size(), 3);
    ASSERT_GE(result.errorMessages.size(), 3);

    // Check that error positions are sorted
    int lastPos = -1;
    for (const auto& info : result.invalidBrackets) {
        EXPECT_GT(info.position, lastPos);
        lastPos = info.position;
    }
}

// Performance test for large strings
TEST_F(ValidStringTest, PerformanceTest) {
    // Generate a large string (1MB) with balanced brackets
    const int strSize = 1024 * 1024;
    std::string largeStr;
    largeStr.reserve(strSize);

    for (int i = 0; i < strSize / 8; ++i) {
        largeStr += "a(b)c[d]";  // 8 characters per iteration
    }

    // Measure validation time
    auto start = std::chrono::high_resolution_clock::now();
    auto result = isValidBracket(largeStr);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Check result is valid
    EXPECT_TRUE(result.isValid);

    // Log performance (not an actual test assertion)
    std::cout << "Validated " << strSize << " byte string in " << duration
              << "ms" << std::endl;
}

// Test parallel vs. sequential processing
TEST_F(ValidStringTest, ParallelVsSequential) {
    // Create a string just below the parallel threshold
    std::string smallStr(9999, 'a');
    smallStr += "(balanced)";

    // Create a string just above the parallel threshold
    std::string largeStr(10001, 'a');
    largeStr += "(balanced)";

    // Both should give the same result
    auto resultSmall = isValidBracket(smallStr);
    auto resultLarge = isValidBracket(largeStr);

    EXPECT_TRUE(resultSmall.isValid);
    EXPECT_TRUE(resultLarge.isValid);
}

// Test with malformed input that might cause exceptions
TEST_F(ValidStringTest, ExceptionHandling) {
    // Create a string with null characters
    std::string badStr = "Bad string with \0 null characters";
    badStr.resize(30);  // Ensure it includes the null character

    // This should handle the null character gracefully
    auto result = isValidBracket(badStr);
    EXPECT_TRUE(result.isValid);
}

// Test multiple threads calling isValidBracket simultaneously
TEST_F(ValidStringTest, ThreadSafety) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<bool> results(numThreads);

    // Create different test strings
    std::vector<std::string> testStrings = {
        "Thread (test) 1", "Thread [test] 2",
        "Thread {test} 3", "Thread <test> 4",
        "Thread (test 5",  // Invalid
        "Thread test] 6",  // Invalid
        "Thread test} 7",  // Invalid
        "Thread test> 8",  // Invalid
        "Thread (test) 9", "Thread [test] 10"};

    // Launch threads
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &testStrings, &results]() {
            auto result = isValidBracket(testStrings[i]);
            results[i] = result.isValid;
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // Check results - strings 0-3 and 8-9 should be valid, 4-7 invalid
    EXPECT_TRUE(results[0]);
    EXPECT_TRUE(results[1]);
    EXPECT_TRUE(results[2]);
    EXPECT_TRUE(results[3]);
    EXPECT_FALSE(results[4]);
    EXPECT_FALSE(results[5]);
    EXPECT_FALSE(results[6]);
    EXPECT_FALSE(results[7]);
    EXPECT_TRUE(results[8]);
    EXPECT_TRUE(results[9]);
}

// Test various ValidationResult methods
TEST_F(ValidStringTest, ValidationResultMethods) {
    ValidationResult result;

    // Initially valid
    EXPECT_TRUE(result.isValid);

    // Add an error with BracketInfo
    BracketInfo info{'(', 10};
    result.addError(info, "Test error with bracket");

    EXPECT_FALSE(result.isValid);
    ASSERT_EQ(result.invalidBrackets.size(), 1);
    EXPECT_EQ(result.invalidBrackets[0].character, '(');
    EXPECT_EQ(result.invalidBrackets[0].position, 10);
    ASSERT_EQ(result.errorMessages.size(), 1);
    EXPECT_EQ(result.errorMessages[0], "Test error with bracket");

    // Add another error with message only
    result.addError("Test error without bracket");

    EXPECT_FALSE(result.isValid);
    ASSERT_EQ(result.invalidBrackets.size(), 1);  // Still only one bracket info
    ASSERT_EQ(result.errorMessages.size(), 2);
    EXPECT_EQ(result.errorMessages[1], "Test error without bracket");
}

// Test BracketInfo equality operator
TEST_F(ValidStringTest, BracketInfoEquality) {
    BracketInfo info1{'(', 10};
    BracketInfo info2{'(', 10};
    BracketInfo info3{')', 10};
    BracketInfo info4{'(', 20};

    EXPECT_EQ(info1, info2);
    EXPECT_NE(info1, info3);
    EXPECT_NE(info1, info4);
}

// Test ValidationException class
TEST_F(ValidStringTest, ValidationException) {
    // Create with message
    ValidationException ex1("Test exception message");
    EXPECT_STREQ(ex1.what(), "Test exception message");
    EXPECT_FALSE(ex1.getResult().isValid);
    EXPECT_THAT(ex1.getResult().errorMessages,
                ElementsAre("Test exception message"));

    // Create with ValidationResult
    ValidationResult result;
    result.addError("Test result message");

    ValidationException ex2(result);
    EXPECT_STREQ(ex2.what(), "Test result message");
    EXPECT_FALSE(ex2.getResult().isValid);
    EXPECT_THAT(ex2.getResult().errorMessages,
                ElementsAre("Test result message"));
}

// Test bracket validation with programming language syntax
TEST_F(ValidStringTest, ProgrammingSyntax) {
    // C++ like syntax
    std::string cppCode = R"(
        int main() {
            if (x > 0) {
                cout << "Positive" << endl;
            } else {
                cout << "Non-positive" << endl;
            }
            return 0;
        }
    )";

    auto result1 = isValidBracket(cppCode);
    EXPECT_TRUE(result1.isValid);

    // Python like syntax with mismatched brackets
    std::string pythonCode = R"(
        def main():
            if x > 0:
                print("Positive")
                data = {"key": [1, 2, 3}
            else:
                print("Non-positive")
            return 0
    )";

    auto result2 = isValidBracket(pythonCode);
    EXPECT_FALSE(result2.isValid);

    // SQL like syntax
    std::string sqlCode = R"(
        SELECT * FROM users 
        WHERE (age > 18) AND (
            status = 'active' OR 
            (registration_date > '2023-01-01')
        )
    )";

    auto result3 = isValidBracket(sqlCode);
    EXPECT_TRUE(result3.isValid);
}

// Test handling of HTML/XML-like syntax
TEST_F(ValidStringTest, HtmlSyntax) {
    std::string html = R"(
        <html>
            <head>
                <title>Test Page</title>
            </head>
            <body>
                <div>
                    <p>Hello, <strong>world</strong>!</p>
                    <img src="image.jpg" />
                </div>
            </body>
        </html>
    )";

    // HTML angle brackets should be detected correctly
    auto result = isValidBracket(html);
    EXPECT_TRUE(result.isValid);

    // Malformed HTML
    std::string badHtml = R"(
        <html>
            <div>
                <p>Unclosed paragraph tag
            </div>
        </html>
    )";

    auto result2 = isValidBracket(badHtml);
    EXPECT_FALSE(result2.isValid);
}

// Test with really long text
TEST_F(ValidStringTest, VeryLongText) {
    // Create an artificial example with many nested brackets
    std::string openBrackets;
    std::string closeBrackets;

    const int nestingLevel = 1000;

    for (int i = 0; i < nestingLevel; ++i) {
        if (i % 4 == 0)
            openBrackets += '(';
        else if (i % 4 == 1)
            openBrackets += '[';
        else if (i % 4 == 2)
            openBrackets += '{';
        else
            openBrackets += '<';
    }

    for (int i = nestingLevel - 1; i >= 0; --i) {
        if (i % 4 == 0)
            closeBrackets += ')';
        else if (i % 4 == 1)
            closeBrackets += ']';
        else if (i % 4 == 2)
            closeBrackets += '}';
        else
            closeBrackets += '>';
    }

    std::string deeplyNestedText = openBrackets + "text" + closeBrackets;

    auto result = isValidBracket(deeplyNestedText);
    EXPECT_TRUE(result.isValid);

    // Now introduce a single error
    deeplyNestedText = openBrackets + "text" + closeBrackets.substr(1);
    deeplyNestedText += ')';  // Wrong closing bracket at the end

    auto result2 = isValidBracket(deeplyNestedText);
    EXPECT_FALSE(result2.isValid);
}

// Test with all four bracket types used incorrectly
TEST_F(ValidStringTest, AllBracketTypesError) {
    std::string badStr = ")(][}{><";

    auto result = isValidBracket(badStr);
    EXPECT_FALSE(result.isValid);
    ASSERT_EQ(result.invalidBrackets.size(), 8);
    ASSERT_EQ(result.errorMessages.size(), 8);

    // Check each bracket was detected
    EXPECT_EQ(result.invalidBrackets[0].character, ')');
    EXPECT_EQ(result.invalidBrackets[1].character, '(');
    EXPECT_EQ(result.invalidBrackets[2].character, ']');
    EXPECT_EQ(result.invalidBrackets[3].character, '[');
    EXPECT_EQ(result.invalidBrackets[4].character, '}');
    EXPECT_EQ(result.invalidBrackets[5].character, '{');
    EXPECT_EQ(result.invalidBrackets[6].character, '>');
    EXPECT_EQ(result.invalidBrackets[7].character, '<');
}

// Test bracket validator with constexpr
TEST_F(ValidStringTest, ConstexprBracketValidator) {
    constexpr const char validStr[] = "(valid)";
    constexpr const char invalidStr[] = "(invalid]";

    constexpr auto validResult = BracketValidator<sizeof(validStr)>::validate(
        std::span<const char, sizeof(validStr)>{validStr, sizeof(validStr)});

    constexpr auto invalidResult =
        BracketValidator<sizeof(invalidStr)>::validate(
            std::span<const char, sizeof(invalidStr)>{invalidStr,
                                                      sizeof(invalidStr)});

    EXPECT_TRUE(validResult.isValid());
    EXPECT_FALSE(invalidResult.isValid());

    constexpr auto errorCount = invalidResult.getErrorCount();
    EXPECT_EQ(errorCount, 2);
}
