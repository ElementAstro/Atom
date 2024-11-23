#include "atom/type/string.hpp"
#include <iostream>
#include <vector>

int main() {
    // Default constructor
    String str1;
    std::cout << "str1: \"" << str1 << "\"" << std::endl;

    // Constructor from C-style string
    String str2("Hello");
    std::cout << "str2: \"" << str2 << "\"" << std::endl;

    // Constructor from std::string_view
    String str3(std::string_view("World"));
    std::cout << "str3: \"" << str3 << "\"" << std::endl;

    // Constructor from std::string
    String str4(std::string("C++"));
    std::cout << "str4: \"" << str4 << "\"" << std::endl;

    // Copy constructor
    String str5 = str2;
    std::cout << "str5: \"" << str5 << "\"" << std::endl;

    // Move constructor
    String str6 = std::move(str3);
    std::cout << "str6: \"" << str6 << "\"" << std::endl;

    // Copy assignment
    str1 = str4;
    std::cout << "str1 after copy assignment: \"" << str1 << "\"" << std::endl;

    // Move assignment
    str1 = std::move(str5);
    std::cout << "str1 after move assignment: \"" << str1 << "\"" << std::endl;

    // Equality comparison
    bool isEqual = (str2 == str4);
    std::cout << "str2 == str4: " << std::boolalpha << isEqual << std::endl;

    // Three-way comparison
    auto cmp = str2 <=> str4;
    if (cmp < 0) {
        std::cout << "str2 is less than str4" << std::endl;
    } else if (cmp > 0) {
        std::cout << "str2 is greater than str4" << std::endl;
    } else {
        std::cout << "str2 is equal to str4" << std::endl;
    }

    // Concatenation with another String
    str2 += str4;
    std::cout << "str2 after concatenation: \"" << str2 << "\"" << std::endl;

    // Concatenation with C-style string
    str2 += " Programming";
    std::cout << "str2 after concatenation with C-style string: \"" << str2
              << "\"" << std::endl;

    // Concatenation with a single character
    str2 += '!';
    std::cout << "str2 after concatenation with a single character: \"" << str2
              << "\"" << std::endl;

    // Get C-style string
    const char* cStr = str2.cStr();
    std::cout << "C-style string: \"" << cStr << "\"" << std::endl;

    // Get length of the string
    size_t length = str2.length();
    std::cout << "Length of str2: " << length << std::endl;

    // Get substring
    String substr = str2.substr(6, 11);
    std::cout << "Substring of str2: \"" << substr << "\"" << std::endl;

    // Find a substring
    size_t pos = str2.find("Programming");
    std::cout << "Position of 'Programming' in str2: " << pos << std::endl;

    // Replace first occurrence of oldStr with newStr
    bool replaced = str2.replace("Programming", "Coding");
    std::cout << "str2 after replace: \"" << str2
              << "\", replaced: " << std::boolalpha << replaced << std::endl;

    // Replace all occurrences of oldStr with newStr
    size_t replaceCount = str2.replaceAll("o", "0");
    std::cout << "str2 after replaceAll: \"" << str2
              << "\", replaceCount: " << replaceCount << std::endl;

    // Convert string to uppercase
    String upperStr = str2.toUpper();
    std::cout << "Uppercase str2: \"" << upperStr << "\"" << std::endl;

    // Convert string to lowercase
    String lowerStr = str2.toLower();
    std::cout << "Lowercase str2: \"" << lowerStr << "\"" << std::endl;

    // Split the string by a delimiter
    std::vector<String> tokens = str2.split(" ");
    std::cout << "Tokens after split: ";
    for (const auto& token : tokens) {
        std::cout << "\"" << token << "\" ";
    }
    std::cout << std::endl;

    // Join a vector of strings with a separator
    String joinedStr = String::join(tokens, "-");
    std::cout << "Joined string: \"" << joinedStr << "\"" << std::endl;

    // Trim whitespace from both ends
    String str7("   Trim me!   ");
    str7.trim();
    std::cout << "str7 after trim: \"" << str7 << "\"" << std::endl;

    // Left trim
    String str8("   Left trim me!");
    str8.ltrim();
    std::cout << "str8 after ltrim: \"" << str8 << "\"" << std::endl;

    // Right trim
    String str9("Right trim me!   ");
    str9.rtrim();
    std::cout << "str9 after rtrim: \"" << str9 << "\"" << std::endl;

    // Reverse the string
    String reversedStr = str2.reverse();
    std::cout << "Reversed str2: \"" << reversedStr << "\"" << std::endl;

    // Case-insensitive comparison
    bool equalsIgnoreCase = str2.equalsIgnoreCase("c++ c0ding!");
    std::cout << "str2 equalsIgnoreCase 'c++ c0ding!': " << std::boolalpha
              << equalsIgnoreCase << std::endl;

    // Check if string starts with a prefix
    bool startsWith = str2.startsWith("C++");
    std::cout << "str2 starts with 'C++': " << std::boolalpha << startsWith
              << std::endl;

    // Check if string ends with a suffix
    bool endsWith = str2.endsWith("!");
    std::cout << "str2 ends with '!': " << std::boolalpha << endsWith
              << std::endl;

    // Check if the string contains a substring
    bool containsStr = str2.contains("Coding");
    std::cout << "str2 contains 'Coding': " << std::boolalpha << containsStr
              << std::endl;

    // Check if the string contains a specific character
    bool containsChar = str2.contains('C');
    std::cout << "str2 contains 'C': " << std::boolalpha << containsChar
              << std::endl;

    // Replace all occurrences of a character with another character
    size_t replaceCharCount = str2.replace('C', 'K');
    std::cout << "str2 after replace 'C' with 'K': \"" << str2
              << "\", replaceCharCount: " << replaceCharCount << std::endl;

    // Remove a specific character from the string
    size_t removeCharCount = str2.remove('0');
    std::cout << "str2 after remove '0': \"" << str2
              << "\", removeCharCount: " << removeCharCount << std::endl;

    // Check if the string is empty
    bool isEmpty = str1.empty();
    std::cout << "str1 is empty: " << std::boolalpha << isEmpty << std::endl;

    // Format a string
    std::string formattedStr =
        String::format("Hello, {}! The answer is {}.", "World", 42);
    std::cout << "Formatted string: \"" << formattedStr << "\"" << std::endl;

    return 0;
}