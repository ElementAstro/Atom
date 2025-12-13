/**
 * @file string_example.cpp
 * @brief Examples for atom::utils string utilities
 */

#include "atom/utils/text/string.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateCaseConversion() {
    printSection("1. Case Conversion");

    std::vector<std::string> testStrings = {
        "HelloWorld", "myVariableName", "XMLParser", "getHTTPResponse"
    };

    std::cout << "--- toUnderscore (snake_case) ---" << std::endl;
    for (const auto& str : testStrings) {
        std::cout << "  " << str << " -> " << toUnderscore(str) << std::endl;
    }

    std::cout << "\n--- toCamelCase ---" << std::endl;
    std::vector<std::string> snakeStrings = {
        "hello_world", "my_variable_name", "xml_parser", "get_http_response"
    };
    for (const auto& str : snakeStrings) {
        std::cout << "  " << str << " -> " << toCamelCase(str) << std::endl;
    }

    std::cout << "\n--- hasUppercase ---" << std::endl;
    std::vector<std::string> caseTests = {"hello", "Hello", "HELLO", "heLLo"};
    for (const auto& str : caseTests) {
        std::cout << "  \"" << str << "\": " << (hasUppercase(str) ? "Yes" : "No") << std::endl;
    }
}

void demonstrateURLEncoding() {
    printSection("2. URL Encoding/Decoding");

    std::vector<std::string> testStrings = {
        "Hello World",
        "name=John&age=30",
        "path/to/file.txt",
        "special chars: @#$%",
        "unicode: 你好"
    };

    std::cout << "--- urlEncode ---" << std::endl;
    for (const auto& str : testStrings) {
        std::string encoded = urlEncode(str);
        std::cout << "  \"" << str << "\"" << std::endl;
        std::cout << "    -> \"" << encoded << "\"" << std::endl;
    }

    std::cout << "\n--- urlDecode ---" << std::endl;
    std::vector<std::string> encodedStrings = {
        "Hello%20World",
        "name%3DJohn%26age%3D30",
        "path%2Fto%2Ffile.txt"
    };
    for (const auto& str : encodedStrings) {
        std::string decoded = urlDecode(str);
        std::cout << "  \"" << str << "\"" << std::endl;
        std::cout << "    -> \"" << decoded << "\"" << std::endl;
    }
}

void demonstrateStringChecks() {
    printSection("3. String Checks");

    std::string testStr = "Hello, World!";
    std::cout << "Test string: \"" << testStr << "\"" << std::endl;

    std::cout << "\n--- startsWith ---" << std::endl;
    std::vector<std::string> prefixes = {"Hello", "World", "He", "hello"};
    for (const auto& prefix : prefixes) {
        std::cout << "  startsWith(\"" << prefix << "\"): "
                  << (startsWith(testStr, prefix) ? "Yes" : "No") << std::endl;
    }

    std::cout << "\n--- endsWith ---" << std::endl;
    std::vector<std::string> suffixes = {"World!", "!", "world!", "Hello"};
    for (const auto& suffix : suffixes) {
        std::cout << "  endsWith(\"" << suffix << "\"): "
                  << (endsWith(testStr, suffix) ? "Yes" : "No") << std::endl;
    }
}

void demonstrateSplitJoin() {
    printSection("4. Split and Join");

    std::cout << "--- splitString ---" << std::endl;
    std::string csv = "apple,banana,cherry,date";
    std::cout << "  Input: \"" << csv << "\"" << std::endl;
    auto parts = splitString(csv, ',');
    std::cout << "  Split by ',': [";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << parts[i] << "\"";
    }
    std::cout << "]" << std::endl;

    std::string path = "/usr/local/bin/app";
    std::cout << "\n  Input: \"" << path << "\"" << std::endl;
    auto pathParts = splitString(path, '/');
    std::cout << "  Split by '/': [";
    for (size_t i = 0; i < pathParts.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << pathParts[i] << "\"";
    }
    std::cout << "]" << std::endl;

    std::cout << "\n--- joinStrings ---" << std::endl;
    std::vector<std::string_view> words = {"Hello", "World", "!"};
    std::string joined = joinStrings(words, " ");
    std::cout << "  Join with ' ': \"" << joined << "\"" << std::endl;

    joined = joinStrings(words, "-");
    std::cout << "  Join with '-': \"" << joined << "\"" << std::endl;

    joined = joinStrings(words, "");
    std::cout << "  Join with '': \"" << joined << "\"" << std::endl;
}

void demonstrateReplace() {
    printSection("5. String Replacement");

    std::cout << "--- replaceString ---" << std::endl;
    std::string text = "Hello World, Hello Universe";
    std::cout << "  Original: \"" << text << "\"" << std::endl;

    std::string replaced = replaceString(text, "Hello", "Hi");
    std::cout << "  Replace 'Hello' with 'Hi': \"" << replaced << "\"" << std::endl;

    replaced = replaceString(text, "World", "Earth");
    std::cout << "  Replace 'World' with 'Earth': \"" << replaced << "\"" << std::endl;

    std::cout << "\n--- replaceStrings (multiple) ---" << std::endl;
    std::vector<std::pair<std::string_view, std::string_view>> replacements = {
        {"Hello", "Hi"},
        {"World", "Earth"},
        {"Universe", "Galaxy"}
    };
    std::string multiReplaced = replaceStrings(text, replacements);
    std::cout << "  Multiple replacements: \"" << multiReplaced << "\"" << std::endl;
}

void demonstrateTrim() {
    printSection("6. Trimming");

    std::vector<std::string> testStrings = {
        "  hello  ",
        "\t\ttabbed\t\t",
        "\n\nnewlines\n\n",
        "  mixed \t\n ",
        "no_whitespace"
    };

    std::cout << "--- trim ---" << std::endl;
    for (const auto& str : testStrings) {
        std::string trimmed = trim(str);
        std::cout << "  \"" << str << "\" -> \"" << trimmed << "\"" << std::endl;
    }

    std::cout << "\n--- Custom trim symbols ---" << std::endl;
    std::string custom = "###hello###";
    std::string trimmedCustom = trim(custom, "#");
    std::cout << "  \"" << custom << "\" (trim '#') -> \"" << trimmedCustom << "\"" << std::endl;
}

void demonstrateExplode() {
    printSection("7. Explode");

    std::cout << "--- explode ---" << std::endl;
    std::string data = "one:two:three:four";
    std::cout << "  Input: \"" << data << "\"" << std::endl;

    auto parts = explode(data, ':');
    std::cout << "  Explode by ':': [";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << parts[i] << "\"";
    }
    std::cout << "]" << std::endl;
}

void demonstrateTokenization() {
    printSection("8. Tokenization");

    std::cout << "--- nstrtok ---" << std::endl;
    std::string_view text = "hello world foo bar";
    std::cout << "  Input: \"" << text << "\"" << std::endl;
    std::cout << "  Tokens: ";

    std::string_view remaining = text;
    while (auto token = nstrtok(remaining, " ")) {
        std::cout << "\"" << *token << "\" ";
    }
    std::cout << std::endl;
}

void demonstrateRealWorldExamples() {
    printSection("9. Real-World Examples");

    std::cout << "--- Parsing CSV line ---" << std::endl;
    std::string csvLine = "John,Doe,30,New York";
    auto fields = splitString(csvLine, ',');
    std::cout << "  CSV: " << csvLine << std::endl;
    std::cout << "  First name: " << fields[0] << std::endl;
    std::cout << "  Last name: " << fields[1] << std::endl;
    std::cout << "  Age: " << fields[2] << std::endl;
    std::cout << "  City: " << fields[3] << std::endl;

    std::cout << "\n--- Building URL query string ---" << std::endl;
    std::string name = "John Doe";
    std::string city = "New York";
    std::string query = "name=" + urlEncode(name) + "&city=" + urlEncode(city);
    std::cout << "  Query: " << query << std::endl;

    std::cout << "\n--- Converting API naming conventions ---" << std::endl;
    std::string jsonKey = "user_first_name";
    std::string cppVar = toCamelCase(jsonKey);
    std::cout << "  JSON key: " << jsonKey << std::endl;
    std::cout << "  C++ variable: " << cppVar << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  String Utilities Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateCaseConversion();
        demonstrateURLEncoding();
        demonstrateStringChecks();
        demonstrateSplitJoin();
        demonstrateReplace();
        demonstrateTrim();
        demonstrateExplode();
        demonstrateTokenization();
        demonstrateRealWorldExamples();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All string examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
