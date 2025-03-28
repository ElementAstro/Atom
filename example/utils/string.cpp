/*
 * string_utils_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils string utilities
 */

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "atom/utils/string.hpp"

// Helper function to print a collection with a header
template <typename Collection>
void printCollection(const Collection& collection, const std::string& header) {
    std::cout << header << ":\n";
    for (const auto& item : collection) {
        std::cout << "  \"" << item << "\"\n";
    }
    std::cout << std::endl;
}

// Helper to print an optional value
template <typename T>
void printOptional(const std::optional<T>& opt, const std::string& label) {
    std::cout << label << ": ";
    if (opt) {
        std::cout << "\"" << *opt << "\"" << std::endl;
    } else {
        std::cout << "No value" << std::endl;
    }
}

int main() {
    std::cout << "=== String Utilities Examples ===\n\n";

    // Example strings for demonstration
    const std::string camelCaseText = "helloWorldExample";
    const std::string snakeCaseText = "hello_world_example";
    const std::string mixedCaseText = "HelloWorld_example123";
    const std::string urlText = "Hello World & Special Chars: !@#$%^";
    const std::string delimiterText = "apple,orange,banana,grape,melon";
    const std::string quotedText = "  This is a quoted text with spaces.  ";

    std::cout << "Example 1: Case Detection and Conversion\n";

    // Check for uppercase characters
    std::cout << "hasUppercase(\"" << camelCaseText << "\"): "
              << (atom::utils::hasUppercase(camelCaseText) ? "true" : "false")
              << std::endl;
    std::cout << "hasUppercase(\"" << snakeCaseText << "\"): "
              << (atom::utils::hasUppercase(snakeCaseText) ? "true" : "false")
              << std::endl;
    std::cout << "hasUppercase(\"" << mixedCaseText << "\"): "
              << (atom::utils::hasUppercase(mixedCaseText) ? "true" : "false")
              << std::endl;

    // Convert to snake_case
    std::cout << "toUnderscore(\"" << camelCaseText
              << "\"): " << atom::utils::toUnderscore(camelCaseText)
              << std::endl;
    std::cout << "toUnderscore(\"" << mixedCaseText
              << "\"): " << atom::utils::toUnderscore(mixedCaseText)
              << std::endl;

    // Convert to camelCase
    std::cout << "toCamelCase(\"" << snakeCaseText
              << "\"): " << atom::utils::toCamelCase(snakeCaseText)
              << std::endl;
    std::cout << std::endl;

    std::cout << "Example 2: URL Encoding and Decoding\n";

    // URL encode
    std::string encodedUrl = atom::utils::urlEncode(urlText);
    std::cout << "Original: \"" << urlText << "\"" << std::endl;
    std::cout << "URL encoded: \"" << encodedUrl << "\"" << std::endl;

    // URL decode
    std::string decodedUrl = atom::utils::urlDecode(encodedUrl);
    std::cout << "URL decoded: \"" << decodedUrl << "\"" << std::endl;
    std::cout << std::endl;

    std::cout << "Example 3: String Prefix and Suffix Checks\n";

    // Test startsWith
    std::cout << "startsWith(\"" << camelCaseText << "\", \"hello\"): "
              << (atom::utils::startsWith(camelCaseText, "hello") ? "true"
                                                                  : "false")
              << std::endl;
    std::cout << "startsWith(\"" << camelCaseText << "\", \"world\"): "
              << (atom::utils::startsWith(camelCaseText, "world") ? "true"
                                                                  : "false")
              << std::endl;

    // Test endsWith
    std::cout << "endsWith(\"" << camelCaseText << "\", \"Example\"): "
              << (atom::utils::endsWith(camelCaseText, "Example") ? "true"
                                                                  : "false")
              << std::endl;
    std::cout << "endsWith(\"" << camelCaseText << "\", \"hello\"): "
              << (atom::utils::endsWith(camelCaseText, "hello") ? "true"
                                                                : "false")
              << std::endl;
    std::cout << std::endl;

    std::cout << "Example 4: String Splitting and Joining\n";

    // Split string
    std::vector<std::string> fruits =
        atom::utils::splitString(delimiterText, ',');
    printCollection(fruits, "splitString(\"" + delimiterText + "\", ',')");

    // Join strings
    std::vector<std::string_view> fruitViews = {"apple", "orange", "banana",
                                                "grape", "melon"};
    std::string joinedFruits = atom::utils::joinStrings(fruitViews, " | ");
    std::cout << "joinStrings([\"apple\", \"orange\", ...], \" | \"): \""
              << joinedFruits << "\"" << std::endl;
    std::cout << std::endl;

    std::cout << "Example 5: String Replacement\n";

    // Replace string
    std::string replaced =
        atom::utils::replaceString(delimiterText, "apple", "pineapple");
    std::cout << "replaceString(\"" << delimiterText
              << "\", \"apple\", \"pineapple\"): \"" << replaced << "\""
              << std::endl;

    // Replace multiple strings
    std::vector<std::pair<std::string_view, std::string_view>> replacements = {
        {"apple", "pineapple"},
        {"orange", "blood orange"},
        {"banana", "plantain"}};
    std::string multiReplaced =
        atom::utils::replaceStrings(delimiterText, replacements);
    std::cout << "replaceStrings with multiple replacements: \""
              << multiReplaced << "\"" << std::endl;

    // Parallel replace for large strings
    std::string largeText = delimiterText;
    for (int i = 0; i < 10; i++)
        largeText += largeText;  // Make it larger
    std::string parallelReplaced =
        atom::utils::parallelReplaceString(largeText, "apple", "pineapple");
    std::cout << "parallelReplaceString (first 50 chars): \""
              << parallelReplaced.substr(0, 50) << "...\"" << std::endl;
    std::cout << std::endl;

    std::cout << "Example 6: String View to String Conversion\n";

    // Convert vector of string_view to vector of string
    std::vector<std::string> stringVector = atom::utils::SVVtoSV(fruitViews);
    printCollection(stringVector, "SVVtoSV([string_view array])");

    // Parallel conversion for large arrays
    std::vector<std::string_view> largeFruitViews(1000, "apple");
    std::vector<std::string> largeStringVector =
        atom::utils::parallelSVVtoSV(largeFruitViews);
    std::cout << "parallelSVVtoSV: Converted " << largeStringVector.size()
              << " elements" << std::endl;
    std::cout << std::endl;

    std::cout << "Example 7: String Explode and Trim\n";

    // Explode
    std::vector<std::string> exploded =
        atom::utils::explode(delimiterText, ',');
    printCollection(exploded, "explode(\"" + delimiterText + "\", ',')");

    // Trim
    std::string trimmed = atom::utils::trim(quotedText);
    std::cout << "trim(\"" << quotedText << "\"): \"" << trimmed << "\""
              << std::endl;

    // Trim with custom symbols
    std::string customTrimmed = atom::utils::trim("###Hello World###", "#");
    std::cout << "trim(\"###Hello World###\", \"#\"): \"" << customTrimmed
              << "\"" << std::endl;
    std::cout << std::endl;

    std::cout << "Example 8: String Tokenization\n";

    // nstrtok
    std::string_view remaining = "apple:orange;banana,grape";
    std::cout
        << "Tokenizing \"apple:orange;banana,grape\" with delimiters \":;,\":"
        << std::endl;

    while (auto token = atom::utils::nstrtok(remaining, ":;,")) {
        std::cout << "  Token: \"" << *token << "\"" << std::endl;
    }
    std::cout << std::endl;

    // splitTokens
    std::string_view moreTokens = "this|is|another|test";
    std::cout
        << "Using splitTokens on \"this|is|another|test\" with delimiter \"|\":"
        << std::endl;

    while (auto token = atom::utils::splitTokens(moreTokens, "|")) {
        std::cout << "  Token: \"" << *token << "\"" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 9: Case Conversion\n";

    // toLower
    std::string lowerCase = atom::utils::toLower(mixedCaseText);
    std::cout << "toLower(\"" << mixedCaseText << "\"): \"" << lowerCase << "\""
              << std::endl;

    // toUpper
    std::string upperCase = atom::utils::toUpper(mixedCaseText);
    std::cout << "toUpper(\"" << mixedCaseText << "\"): \"" << upperCase << "\""
              << std::endl;
    std::cout << std::endl;

    std::cout << "Example 10: String/WString Conversion\n";

    // String to WString
    std::wstring wideString = atom::utils::stringToWString("Hello World");
    std::wcout << "stringToWString(\"Hello World\"): L\"";
    std::wcout << wideString << L"\"" << std::endl;

    // WString to String
    std::string narrowString = atom::utils::wstringToString(L"Wide String");
    std::cout << "wstringToString(L\"Wide String\"): \"" << narrowString << "\""
              << std::endl;
    std::cout << std::endl;

    std::cout << "Example 11: String to Number Conversion\n";

    // stod
    double dValue = atom::utils::stod("123.456");
    std::cout << "stod(\"123.456\"): " << dValue << std::endl;

    // stof
    float fValue = atom::utils::stof("78.9");
    std::cout << "stof(\"78.9\"): " << fValue << std::endl;

    // stoi
    int iValue = atom::utils::stoi("42");
    std::cout << "stoi(\"42\"): " << iValue << std::endl;

    // stoi with different base
    int hexValue = atom::utils::stoi("2A", nullptr, 16);
    std::cout << "stoi(\"2A\", nullptr, 16): " << hexValue << std::endl;

    // stol
    long lValue = atom::utils::stol("-12345");
    std::cout << "stol(\"-12345\"): " << lValue << std::endl;
    std::cout << std::endl;

    std::cout << "Example 12: Modern C++20 Split Implementation\n";

    // Split by char
    std::cout << "split(\"" << delimiterText << "\", ','):" << std::endl;
    for (const auto& part : atom::utils::split(delimiterText, ',')) {
        std::cout << "  \"" << part << "\"" << std::endl;
    }

    // Split by string
    std::string csvWithHeader =
        "Name,Age,Location\nJohn,30,New York\nMary,25,Boston";
    std::cout << "\nsplit(csvWithHeader, \"\\n\"):" << std::endl;
    for (const auto& line : atom::utils::split(csvWithHeader, "\n")) {
        std::cout << "  \"" << line << "\"" << std::endl;
    }

    // Split with trim
    std::string spacedText = " apple , orange , banana , grape ";
    std::cout << "\nsplit with trim=true:" << std::endl;
    for (const auto& part : atom::utils::split(spacedText, ',', true)) {
        std::cout << "  \"" << part << "\"" << std::endl;
    }

    // Split with skipEmpty
    std::string textWithEmpties = "first,,second,,,third";
    std::cout << "\nsplit with skipEmpty=true:" << std::endl;
    for (const auto& part :
         atom::utils::split(textWithEmpties, ',', false, true)) {
        std::cout << "  \"" << part << "\"" << std::endl;
    }

    // Split with predicate
    std::cout << "\nsplit with predicate (isspace):" << std::endl;
    std::string words = "This is a    test with   spaces";
    for (const auto& word : atom::utils::split(
             words, [](char c) { return std::isspace(c); }, false, true)) {
        std::cout << "  \"" << word << "\"" << std::endl;
    }

    // Split and collect
    std::cout << "\nUsing collectVector():" << std::endl;
    auto vec = atom::utils::split(delimiterText, ',').collectVector();
    printCollection(vec, "Vector from split");

    // Split and collect to list
    std::cout << "Using collectList():" << std::endl;
    auto list = atom::utils::split(delimiterText, ',').collectList();
    printCollection(list, "List from split");

    // Split and collect to array
    std::cout << "Using collectArray<5>():" << std::endl;
    auto arr = atom::utils::split(delimiterText, ',').collectArray<5>();
    printCollection(arr, "Array from split");

    return 0;
}