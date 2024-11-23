#include "atom/utils/valid_string.hpp"

#include <array>
#include <iostream>

using namespace atom::utils;

int main() {
    // Validate a string with brackets using isValidBracket function
    std::string str1 = "{[()]}";
    ValidationResult result1 = isValidBracket(str1);
    std::cout << "String: " << str1 << std::endl;
    std::cout << "Is valid: " << std::boolalpha << result1.isValid << std::endl;
    if (!result1.isValid) {
        std::cout << "Invalid brackets at positions: ";
        for (const auto& bracket : result1.invalidBrackets) {
            std::cout << bracket.position << " ";
        }
        std::cout << std::endl;
    }

    // Validate a string with brackets using BracketValidator
    constexpr auto result2 = validateBrackets("{[(])}");
    std::cout << "String: {[(])}" << std::endl;
    std::cout << "Is valid: " << std::boolalpha << result2.isValid()
              << std::endl;
    if (!result2.isValid()) {
        std::cout << "Invalid brackets at positions: ";
        for (int i = 0; i < result2.getErrorCount(); ++i) {
            std::cout << result2.getErrorPositions()[i] << " ";
        }
        std::cout << std::endl;
    }

    // Validate a string with mismatched quotes
    constexpr auto result3 = validateBrackets("{['\"']}");
    std::cout << "String: {['\"']}" << std::endl;
    std::cout << "Is valid: " << std::boolalpha << result3.isValid()
              << std::endl;
    if (!result3.isValid()) {
        std::cout << "Invalid brackets at positions: ";
        for (int i = 0; i < result3.getErrorCount(); ++i) {
            std::cout << result3.getErrorPositions()[i] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}