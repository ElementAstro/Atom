#include "atom/utils/cstring.hpp"

#include <iostream>
#include <array>

using namespace atom::utils;

int main() {
    // Example C-style string
    constexpr char exampleStr[] = "Hello, World!";

    // Deduplicate characters in a C-style string
    constexpr auto deduplicated = deduplicate("aabbcc");
    std::cout << "Deduplicated: " << deduplicated.data() << std::endl;

    // Split a C-style string into substrings based on a delimiter
    constexpr auto splitResult = split("a,b,c,d", ',');
    std::cout << "Split: ";
    for (const auto& part : splitResult) {
        if (!part.empty()) {
            std::cout << part << " ";
        }
    }
    std::cout << std::endl;

    // Replace all occurrences of a character in a C-style string
    constexpr auto replaced = replace("aabbcc", 'b', 'x');
    std::cout << "Replaced: " << replaced.data() << std::endl;

    // Convert all characters in a C-style string to lowercase
    constexpr auto lower = toLower("HELLO");
    std::cout << "Lowercase: " << lower.data() << std::endl;

    // Convert all characters in a C-style string to uppercase
    constexpr auto upper = toUpper("hello");
    std::cout << "Uppercase: " << upper.data() << std::endl;

    // Concatenate two C-style strings
    constexpr auto concatenated = concat("Hello, ", "World!");
    std::cout << "Concatenated: " << concatenated.data() << std::endl;

    // Trim leading and trailing whitespace from a C-style string
    constexpr auto trimmed = trim("  Hello, World!  ");
    std::cout << "Trimmed: " << trimmed.data() << std::endl;

    // Extract a substring from a C-style string
    constexpr auto substringResult = substring("Hello, World!", 7, 5);
    std::cout << "Substring: " << substringResult.data() << std::endl;

    // Compare two C-style strings for equality
    constexpr bool isEqual = equal("Hello", "Hello");
    std::cout << "Equal: " << std::boolalpha << isEqual << std::endl;

    // Find the first occurrence of a character in a C-style string
    constexpr std::size_t foundIndex = find("Hello, World!", 'W');
    std::cout << "Found index: " << foundIndex << std::endl;

    // Return the length of a C-style string
    constexpr std::size_t lengthResult = length("Hello");
    std::cout << "Length: " << lengthResult << std::endl;

    // Reverse the characters in a C-style string
    constexpr auto reversed = reverse("Hello");
    std::cout << "Reversed: " << reversed.data() << std::endl;

    // Trim leading and trailing whitespace from a std::string_view
    constexpr std::string_view trimmedView = trim("  Hello, World!  "sv);
    std::cout << "Trimmed view: " << trimmedView << std::endl;

    // Convert a char array to an array (constexpr version)
    constexpr std::array<char, 6> charArray = {'H', 'e', 'l', 'l', 'o', '\0'};
    constexpr auto arrayConstexpr = charArrayToArrayConstexpr(charArray);
    std::cout << "Array constexpr: " << arrayConstexpr.data() << std::endl;

    // Convert a char array to an array (non-constexpr version)
    auto arrayNonConstexpr = charArrayToArray(charArray);
    std::cout << "Array non-constexpr: " << arrayNonConstexpr.data() << std::endl;

    /*
    TODO: Uncomment the following code to test the remaining functions
    // Check if a char array represents a negative number
    constexpr bool isNegativeResult = isNegative("-123");
    std::cout << "Is negative: " << std::boolalpha << isNegativeResult << std::endl;

    // Convert a char array to an integer
    constexpr int intValue = arrayToInt("123");
    std::cout << "Integer value: " << intValue << std::endl;

    // Get the absolute value of a char array representing a number
    constexpr int absValue = absoluteValue("-123");
    std::cout << "Absolute value: " << absValue << std::endl;

    // Convert a number from one base to another
    std::string baseConverted = convertBase("1010", BASE_2, BASE_10);
    std::cout << "Base converted: " << baseConverted << std::endl;
    */
    

    return 0;
}