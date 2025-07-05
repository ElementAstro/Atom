/**
 * @file cstring_example.cpp
 * @brief Comprehensive examples demonstrating compile-time string utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::cstring.hpp:
 * - Basic string manipulation (deduplicate, replace, concatenate)
 * - Case conversion (toLower, toUpper)
 * - String analysis (find, length, equal)
 * - String transformation (trim, substring, reverse)
 * - String parsing (split)
 * - Numeric operations (arrayToInt, absoluteValue, convertBase)
 */

#include "atom/utils/cstring.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

// Helper function to print array content as string
template <size_t N>
void printArray(const std::array<char, N>& arr, const std::string& label) {
    std::cout << std::left << std::setw(30) << label << ": \"";
    for (char c : arr) {
        if (c == '\0')
            break;  // Stop at null terminator
        std::cout << c;
    }
    std::cout << "\"" << std::endl;
}

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==============================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "===============================================" << std::endl;
}

int main() {
    // ===================================================
    // Example 1: Basic String Manipulation
    // ===================================================
    printSection("1. Basic String Manipulation");

    // Deduplicate characters in a string
    std::cout << "Deduplication examples:" << std::endl;
    constexpr auto original1 = "hello world";
    constexpr auto deduped1 =
        atom::utils::deduplicate<sizeof(original1)>(original1);
    printArray(deduped1, "Deduplicated 'hello world'");

    constexpr auto original2 = "programming";
    constexpr auto deduped2 =
        atom::utils::deduplicate<sizeof(original2)>(original2);
    printArray(deduped2, "Deduplicated 'programming'");

    constexpr auto original3 = "aaaaabbbccc";
    constexpr auto deduped3 =
        atom::utils::deduplicate<sizeof(original3)>(original3);
    printArray(deduped3, "Deduplicated 'aaaaabbbccc'");

    // Replace characters in a string
    std::cout << "\nReplacement examples:" << std::endl;
    constexpr auto replaced1 =
        atom::utils::replace<sizeof(original1)>(original1, 'l', 'x');
    printArray(replaced1, "Replace 'l' with 'x'");

    constexpr auto replaced2 =
        atom::utils::replace<12>("path/to/file", '/', '\\');
    printArray(replaced2, "Replace '/' with '\\'");

    constexpr auto replaced3 =
        atom::utils::replace<12>("123-456-789", '-', '_');
    printArray(replaced3, "Replace '-' with '_'");

    // ===================================================
    // Example 2: Case Conversion
    // ===================================================
    printSection("2. Case Conversion");

    constexpr auto mixedCase = "Hello World 123";

    // Convert to lowercase
    constexpr auto lowerCase =
        atom::utils::toLower<sizeof(mixedCase)>(mixedCase);
    printArray(lowerCase, "toLower");

    // Convert to uppercase
    constexpr auto upperCase =
        atom::utils::toUpper<sizeof(mixedCase)>(mixedCase);
    printArray(upperCase, "toUpper");

    // Chained conversions
    constexpr auto lowerMixedCase =
        atom::utils::toLower<sizeof(mixedCase)>(mixedCase);
    constexpr auto chainedCase =
        atom::utils::toUpper<sizeof(lowerMixedCase)>(lowerMixedCase);
    printArray(chainedCase, "toLower then toUpper");

    // Specialized cases
    constexpr auto specialCase = "Text with MIXED case and 123 numbers!";
    constexpr auto specialLower =
        atom::utils::toLower<sizeof(specialCase)>(specialCase);
    printArray(specialLower, "Special case to lower");
    constexpr auto specialUpper =
        atom::utils::toUpper<sizeof(specialCase)>(specialCase);
    printArray(specialUpper, "Special case to upper");

    // ===================================================
    // Example 3: String Concatenation
    // ===================================================
    printSection("3. String Concatenation");

    constexpr auto firstName = "John";
    constexpr auto lastName = "Doe";

    // Basic concatenation
    constexpr auto fullName =
        atom::utils::concat<sizeof(firstName), sizeof(lastName)>(firstName,
                                                                 lastName);
    printArray(fullName, "First + Last");

    // Multiple concatenations
    constexpr auto greeting = "Hello, ";
    constexpr auto greetingWithName =
        atom::utils::concat<sizeof(greeting), sizeof(firstName)>(greeting,
                                                                 firstName);
    printArray(greetingWithName, "Greeting + First name");

    // Concatenate with space
    constexpr auto space = " ";
    constexpr auto firstNameWithSpace =
        atom::utils::concat<sizeof(firstName), sizeof(space)>(firstName, space);
    constexpr auto properFullName =
        atom::utils::concat<sizeof(firstNameWithSpace), sizeof(lastName)>(
            firstNameWithSpace, lastName);
    printArray(properFullName, "First + Space + Last");

    // Concatenate with punctuation
    constexpr auto exclamation = "!";
    constexpr auto nameWithExclamation =
        atom::utils::concat<sizeof(firstName), sizeof(exclamation)>(
            firstName, exclamation);
    constexpr auto excitedGreeting =
        atom::utils::concat<sizeof(greeting), sizeof(nameWithExclamation)>(
            greeting, nameWithExclamation);
    printArray(excitedGreeting, "Greeting + First + !");

    // ===================================================
    // Example 4: String Analysis
    // ===================================================
    printSection("4. String Analysis");

    constexpr auto sampleText = "The quick brown fox jumps over the lazy dog";

    // Find character
    constexpr auto positionOfQ =
        atom::utils::find<sizeof(sampleText)>(sampleText, 'q');
    std::cout << "Position of 'q': " << positionOfQ << std::endl;

    constexpr auto positionOfZ =
        atom::utils::find<sizeof(sampleText)>(sampleText, 'z');
    std::cout << "Position of 'z': " << positionOfZ << std::endl;

    constexpr auto positionOfX =
        atom::utils::find<sizeof(sampleText)>(sampleText, 'x');
    std::cout << "Position of 'x' (not found): " << positionOfX << std::endl;

    // Get string length
    constexpr auto lengthOfSampleText =
        atom::utils::length<sizeof(sampleText)>(sampleText);
    std::cout << "Length of sample text: " << lengthOfSampleText << std::endl;

    constexpr auto lengthOfEmpty = atom::utils::length<1>("");
    std::cout << "Length of empty string: " << lengthOfEmpty << std::endl;

    // Compare strings
    constexpr auto string1 = "Hello";
    constexpr auto string2 = "Hello";
    constexpr auto string3 = "World";

    constexpr auto equalStrings =
        atom::utils::equal<sizeof(string1), sizeof(string2)>(string1, string2);
    std::cout << "Are \"Hello\" and \"Hello\" equal? "
              << (equalStrings ? "Yes" : "No") << std::endl;

    constexpr auto unequalStrings =
        atom::utils::equal<sizeof(string1), sizeof(string3)>(string1, string3);
    std::cout << "Are \"Hello\" and \"World\" equal? "
              << (unequalStrings ? "Yes" : "No") << std::endl;

    constexpr auto caseSensitive = atom::utils::equal<6, 6>("hello", "Hello");
    std::cout << "Are \"hello\" and \"Hello\" equal? "
              << (caseSensitive ? "Yes" : "No") << std::endl;

    // ===================================================
    // Example 5: String Transformation
    // ===================================================
    printSection("5. String Transformation");

    // Trim whitespace
    constexpr auto spacedText = "  Hello, World!  ";
    constexpr auto trimmedText =
        atom::utils::trim<sizeof(spacedText)>(spacedText);
    printArray(trimmedText, "Trimmed text");

    constexpr auto onlySpaces = "     ";
    constexpr auto trimmedSpaces =
        atom::utils::trim<sizeof(onlySpaces)>(onlySpaces);
    printArray(trimmedSpaces, "Trimmed spaces only");

    // Substring extraction
    constexpr auto sampleForSubstring = "Extract a portion of this string";
    constexpr auto extractedSubstring =
        atom::utils::substring<sizeof(sampleForSubstring), 8>(
            sampleForSubstring, 10, 7);
    printArray(extractedSubstring, "Substring (10, 7)");

    constexpr auto beginningSubstring =
        atom::utils::substring<sizeof(sampleForSubstring), 8>(
            sampleForSubstring, 0, 7);
    printArray(beginningSubstring, "Substring (0, 7)");

    constexpr auto outOfBoundsSubstring =
        atom::utils::substring<sizeof(sampleForSubstring), 11>(
            sampleForSubstring, 30, 10);
    printArray(outOfBoundsSubstring, "Out-of-bounds substring");

    // Reverse string
    constexpr auto palindrome = "level";
    constexpr auto reversedPalindrome =
        atom::utils::reverse<sizeof(palindrome)>(palindrome);
    printArray(reversedPalindrome, "Reversed 'level'");

    constexpr auto sentence = "Hello World";
    constexpr auto reversedSentence =
        atom::utils::reverse<sizeof(sentence)>(sentence);
    printArray(reversedSentence, "Reversed 'Hello World'");

    // Double reverse should give the original
    constexpr auto doubleReversed =
        atom::utils::reverse<sizeof(reversedSentence)>(reversedSentence);
    printArray(doubleReversed, "Double reversed");

    // Trim with std::string_view
    std::string_view svWithSpaces = "  Trimming with string_view  ";
    std::string_view svTrimmed = atom::utils::trim(svWithSpaces);
    std::cout << "Trimmed string_view: \"" << svTrimmed << "\"" << std::endl;

    // ===================================================
    // Example 6: String Splitting
    // ===================================================
    printSection("6. String Splitting");

    // Split by comma
    constexpr auto csvLine = "Apple,Banana,Cherry,Date";
    constexpr auto splitByComma =
        atom::utils::split<sizeof(csvLine)>(csvLine, ',');

    std::cout << "Split CSV line:" << std::endl;
    for (const auto& part : splitByComma) {
        if (part.empty())
            break;  // Stop at empty parts (end of data)
        std::cout << "  - \"" << part << "\"" << std::endl;
    }

    // Split by space
    constexpr auto spaceSeparated = "The quick brown fox";
    constexpr auto splitBySpace =
        atom::utils::split<sizeof(spaceSeparated)>(spaceSeparated, ' ');

    std::cout << "\nSplit by space:" << std::endl;
    for (const auto& part : splitBySpace) {
        if (part.empty())
            break;
        std::cout << "  - \"" << part << "\"" << std::endl;
    }

    // Split with empty parts
    constexpr auto withEmptyParts = "first,,third,fourth,";
    constexpr auto splitWithEmpty =
        atom::utils::split<sizeof(withEmptyParts)>(withEmptyParts, ',');

    std::cout << "\nSplit with empty parts:" << std::endl;
    for (size_t i = 0; i < 5; ++i) {
        if (splitWithEmpty[i].empty() && i > 0) {
            std::cout << "  - [empty string]" << std::endl;
        } else if (!splitWithEmpty[i].empty()) {
            std::cout << "  - \"" << splitWithEmpty[i] << "\"" << std::endl;
        }
    }

    // ===================================================
    // Example 7: Numeric Operations
    // ===================================================
    printSection("7. Numeric Operations");

    // Create character arrays from string literals
    constexpr std::array<char, 5> num1 = {'1', '2', '3', '4', '\0'};
    constexpr std::array<char, 6> num2 = {'-', '5', '6', '7', '8', '\0'};
    constexpr std::array<char, 3> hex1 = {'F', 'F', '\0'};
    constexpr std::array<char, 5> bin1 = {'1', '0', '1', '0', '\0'};

    // Convert to integers
    constexpr int int1 = atom::utils::arrayToInt(num1);
    std::cout << "String '1234' to int: " << int1 << std::endl;

    constexpr int int2 = atom::utils::arrayToInt(num2);
    std::cout << "String '-5678' to int: " << int2 << std::endl;

    constexpr int hexInt = atom::utils::arrayToInt(hex1, atom::utils::BASE_16);
    std::cout << "Hex 'FF' to int: " << hexInt << std::endl;

    constexpr int binInt = atom::utils::arrayToInt(bin1, atom::utils::BASE_2);
    std::cout << "Binary '1010' to int: " << binInt << std::endl;

    // Check if negative
    constexpr bool isNeg1 = atom::utils::isNegative(num1);
    std::cout << "Is '1234' negative? " << (isNeg1 ? "Yes" : "No") << std::endl;

    constexpr bool isNeg2 = atom::utils::isNegative(num2);
    std::cout << "Is '-5678' negative? " << (isNeg2 ? "Yes" : "No")
              << std::endl;

    // Get absolute value
    constexpr int abs1 = atom::utils::absoluteValue(num1);
    std::cout << "Absolute value of '1234': " << abs1 << std::endl;

    constexpr int abs2 = atom::utils::absoluteValue(num2);
    std::cout << "Absolute value of '-5678': " << abs2 << std::endl;

    // Convert between bases (non-constexpr)
    auto decToHex = atom::utils::convertBase(num1, atom::utils::BASE_10,
                                             atom::utils::BASE_16);
    std::cout << "Decimal '1234' to hex: " << decToHex << std::endl;

    auto decToBin = atom::utils::convertBase(num1, atom::utils::BASE_10,
                                             atom::utils::BASE_2);
    std::cout << "Decimal '1234' to binary: " << decToBin << std::endl;

    auto hexToDec = atom::utils::convertBase(hex1, atom::utils::BASE_16,
                                             atom::utils::BASE_10);
    std::cout << "Hex 'FF' to decimal: " << hexToDec << std::endl;

    // ===================================================
    // Example 8: Combining Multiple Operations
    // ===================================================
    printSection("8. Combining Multiple Operations");

    // Create a normalized path
    constexpr auto rawPath = "C:/Users\\John/Documents\\Projects";
    constexpr auto normalizedPath =
        atom::utils::replace<sizeof(rawPath)>(rawPath, '\\', '/');
    printArray(normalizedPath, "Normalized path");

    // Process user input (simulated)
    constexpr auto userInput = "   Username123   ";
    constexpr auto trimmedInput =
        atom::utils::trim<sizeof(userInput)>(userInput);
    constexpr auto processedInput =
        atom::utils::toLower<sizeof(trimmedInput)>(trimmedInput);
    printArray(processedInput, "Processed user input");

    // Format a name
    constexpr auto firstNameRaw = "jOHn";
    constexpr auto lastNameRaw = "DOE";

    // First letter uppercase, rest lowercase
    constexpr auto firstNameLower =
        atom::utils::toLower<sizeof(firstNameRaw)>(firstNameRaw);
    constexpr auto firstNameUpper =
        atom::utils::toUpper<sizeof(firstNameRaw)>(firstNameRaw);
    constexpr auto firstLetterUpper =
        atom::utils::substring<sizeof(firstNameUpper), 2>(firstNameUpper, 0, 1);
    constexpr auto restLower =
        atom::utils::substring<sizeof(firstNameLower), sizeof(firstNameLower)>(
            firstNameLower, 1,
            atom::utils::length<sizeof(firstNameRaw)>(firstNameRaw) - 1);
    constexpr auto properFirstName =
        atom::utils::concat<sizeof(firstLetterUpper), sizeof(restLower)>(
            firstLetterUpper, restLower);

    constexpr auto lastNameLower =
        atom::utils::toLower<sizeof(lastNameRaw)>(lastNameRaw);
    constexpr auto lastNameUpper =
        atom::utils::toUpper<sizeof(lastNameRaw)>(lastNameRaw);
    constexpr auto lastFirstLetterUpper =
        atom::utils::substring<sizeof(lastNameUpper), 2>(lastNameUpper, 0, 1);
    constexpr auto lastRestLower =
        atom::utils::substring<sizeof(lastNameLower), sizeof(lastNameLower)>(
            lastNameLower, 1,
            atom::utils::length<sizeof(lastNameRaw)>(lastNameRaw) - 1);
    constexpr auto properLastName =
        atom::utils::concat<sizeof(lastFirstLetterUpper),
                            sizeof(lastRestLower)>(lastFirstLetterUpper,
                                                   lastRestLower);

    constexpr auto nameWithSpace =
        atom::utils::concat<sizeof(properFirstName), 2>(properFirstName, " ");
    constexpr auto formattedName =
        atom::utils::concat<sizeof(nameWithSpace), sizeof(properLastName)>(
            nameWithSpace, properLastName);

    printArray(formattedName, "Formatted name");

    // Parse and process a configuration line
    constexpr auto configLine = "setting=value";
    constexpr auto configParts =
        atom::utils::split<sizeof(configLine)>(configLine, '=');

    std::cout << "\nParsed configuration:" << std::endl;
    if (!configParts[0].empty() && !configParts[1].empty()) {
        std::cout << "  Setting: \"" << configParts[0] << "\"" << std::endl;
        std::cout << "  Value: \"" << configParts[1] << "\"" << std::endl;
    }

    // ===================================================
    // Example 9: Runtime vs. Compile-time
    // ===================================================
    printSection("9. Runtime vs. Compile-time");

    // Compile-time operations
    std::cout << "Compile-time operation results:" << std::endl;

    constexpr auto compiletimeArray =
        std::array<char, 5>{'t', 'e', 's', 't', '\0'};
    constexpr auto constexprResult =
        atom::utils::charArrayToArrayConstexpr(compiletimeArray);

    for (char c : constexprResult) {
        if (c == '\0')
            break;
        std::cout << c;
    }
    std::cout << std::endl;

    // Runtime operations
    std::cout << "\nRuntime operation results:" << std::endl;

    const auto runtimeArray = std::array<char, 5>{'t', 'e', 's', 't', '\0'};
    const auto runtimeResult = atom::utils::charArrayToArray(runtimeArray);

    for (char c : runtimeResult) {
        if (c == '\0')
            break;
        std::cout << c;
    }
    std::cout << std::endl;

    return 0;
}
