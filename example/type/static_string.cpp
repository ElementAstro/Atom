#include "atom/type/static_string.hpp"

#include <iostream>

int main() {
    // Create an empty StaticString
    StaticString<10> str1;
    std::cout << "str1 size: " << str1.size() << ", content: \"" << str1.c_str()
              << "\"" << std::endl;

    // Create a StaticString with a C-style string literal
    StaticString<10> str2("Hello");
    std::cout << "str2 size: " << str2.size() << ", content: \"" << str2.c_str()
              << "\"" << std::endl;

    // Create a StaticString with a std::string_view
    StaticString<10> str3(std::string_view("World"));
    std::cout << "str3 size: " << str3.size() << ", content: \"" << str3.c_str()
              << "\"" << std::endl;

    // Append a character
    str2.push_back('!');
    std::cout << "str2 after push_back: \"" << str2.c_str() << "\""
              << std::endl;

    // Append a string
    str2.append(" C++");
    std::cout << "str2 after append: \"" << str2.c_str() << "\"" << std::endl;

    // Get a substring
    auto substr = str2.substr(0, 5);
    std::cout << "Substring of str2: \"" << substr.c_str() << "\"" << std::endl;

    // Find a character
    auto pos = str2.find('C');
    std::cout << "Position of 'C' in str2: " << pos << std::endl;

    // Replace a portion of the string
    str2.replace(6, 3, "Programming");
    std::cout << "str2 after replace: \"" << str2.c_str() << "\"" << std::endl;

    // Concatenate two StaticStrings
    auto str4 = str2 + str3;
    std::cout << "Concatenated string: \"" << str4.c_str() << "\"" << std::endl;

    // Check if two StaticStrings are equal
    bool isEqual = (str2 == str3);
    std::cout << "str2 and str3 are equal: " << std::boolalpha << isEqual
              << std::endl;

    // Check if two StaticStrings are not equal
    bool isNotEqual = (str2 != str3);
    std::cout << "str2 and str3 are not equal: " << std::boolalpha << isNotEqual
              << std::endl;

    return 0;
}