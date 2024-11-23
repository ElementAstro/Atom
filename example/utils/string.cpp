#include "atom/utils/string.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Check if the string contains any uppercase characters
    std::string str = "Hello World";
    bool hasUpper = hasUppercase(str);
    std::cout << "Has uppercase: " << std::boolalpha << hasUpper << std::endl;

    // Convert the string to snake_case format
    std::string snakeCase = toUnderscore(str);
    std::cout << "Snake case: " << snakeCase << std::endl;

    // Convert the string to camelCase format
    std::string camelCase = toCamelCase(str);
    std::cout << "Camel case: " << camelCase << std::endl;

    // URL encode the string
    std::string urlEncoded = urlEncode(str);
    std::cout << "URL encoded: " << urlEncoded << std::endl;

    // URL decode the string
    std::string urlDecoded = urlDecode(urlEncoded);
    std::cout << "URL decoded: " << urlDecoded << std::endl;

    // Check if the string starts with a prefix
    bool starts = startsWith(str, "Hello");
    std::cout << "Starts with 'Hello': " << std::boolalpha << starts << std::endl;

    // Check if the string ends with a suffix
    bool ends = endsWith(str, "World");
    std::cout << "Ends with 'World': " << std::boolalpha << ends << std::endl;

    // Split the string into multiple strings
    std::vector<std::string> split = splitString(str, ' ');
    std::cout << "Split string: ";
    for (const auto& s : split) {
        std::cout << s << " ";
    }
    std::cout << std::endl;

    // Concatenate an array of strings into a single string with a delimiter
    std::vector<std::string_view> strings = {"Hello", "World"};
    std::string joined = joinStrings(strings, ", ");
    std::cout << "Joined string: " << joined << std::endl;

    // Replace all occurrences of a substring with another substring
    std::string replaced = replaceString(str, "World", "Universe");
    std::cout << "Replaced string: " << replaced << std::endl;

    // Replace multiple substrings with their corresponding replacements
    std::vector<std::pair<std::string_view, std::string_view>> replacements = {{"Hello", "Hi"}, {"World", "Universe"}};
    std::string multiReplaced = replaceStrings(str, replacements);
    std::cout << "Multi-replaced string: " << multiReplaced << std::endl;

    // Convert a vector of string_view to a vector of string
    std::vector<std::string> converted = SVVtoSV(strings);
    std::cout << "Converted vector: ";
    for (const auto& s : converted) {
        std::cout << s << " ";
    }
    std::cout << std::endl;

    // Explode a string_view into a vector of string_view
    std::vector<std::string> exploded = explode(str, ' ');
    std::cout << "Exploded string: ";
    for (const auto& s : exploded) {
        std::cout << s << " ";
    }
    std::cout << std::endl;

    // Trim a string_view
    std::string trimmed = trim("  Hello World  ");
    std::cout << "Trimmed string: '" << trimmed << "'" << std::endl;

    // Convert a u8string to a wstring
    std::wstring wstr = stringToWString("Hello World");
    std::wcout << L"Converted to wstring: " << wstr << std::endl;

    // Convert a wstring to a u8string
    std::string u8str = wstringToString(wstr);
    std::cout << "Converted to u8string: " << u8str << std::endl;

    // Convert a string to a double
    double d = stod("123.456");
    std::cout << "String to double: " << d << std::endl;

    // Convert a string to a float
    float f = stof("123.456");
    std::cout << "String to float: " << f << std::endl;

    // Convert a string to an integer
    int i = stoi("123");
    std::cout << "String to int: " << i << std::endl;

    // Convert a string to a long integer
    long l = stol("1234567890");
    std::cout << "String to long: " << l << std::endl;

    // Split a string into multiple strings using nstrtok
    std::string_view strView = "Hello,World,Example";
    std::optional<std::string_view> token;
    while ((token = nstrtok(strView, ","))) {
        std::cout << "Token: " << *token << std::endl;
    }

    return 0;
}