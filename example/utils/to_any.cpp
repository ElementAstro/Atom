/*
 * parser_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils::Parser class
 */

#include <any>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/utils/to_any.hpp"

// Helper function to print any value if possible
void printAny(const std::any& value, const std::string& prefix = "") {
    if (!value.has_value()) {
        std::cout << prefix << "Empty value" << std::endl;
        return;
    }

    std::cout << prefix << "Type: " << value.type().name() << " - Value: ";

    try {
        // Handle common types
        if (value.type() == typeid(int)) {
            std::cout << std::any_cast<int>(value);
        } else if (value.type() == typeid(long)) {
            std::cout << std::any_cast<long>(value);
        } else if (value.type() == typeid(long long)) {
            std::cout << std::any_cast<long long>(value);
        } else if (value.type() == typeid(unsigned int)) {
            std::cout << std::any_cast<unsigned int>(value);
        } else if (value.type() == typeid(float)) {
            std::cout << std::any_cast<float>(value);
        } else if (value.type() == typeid(double)) {
            std::cout << std::any_cast<double>(value);
        } else if (value.type() == typeid(bool)) {
            std::cout << (std::any_cast<bool>(value) ? "true" : "false");
        } else if (value.type() == typeid(char)) {
            std::cout << "'" << std::any_cast<char>(value) << "'";
        } else if (value.type() == typeid(std::string)) {
            std::cout << "\"" << std::any_cast<std::string>(value) << "\"";
        } else if (value.type() ==
                   typeid(std::chrono::system_clock::time_point)) {
            auto timePoint =
                std::any_cast<std::chrono::system_clock::time_point>(value);
            std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
            std::cout << std::put_time(std::localtime(&time),
                                       "%Y-%m-%d %H:%M:%S");
        } else if (value.type() == typeid(std::vector<int>)) {
            const auto& vec = std::any_cast<const std::vector<int>&>(value);
            std::cout << "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << vec[i];
            }
            std::cout << "]";
        } else if (value.type() == typeid(std::set<float>)) {
            const auto& set = std::any_cast<const std::set<float>&>(value);
            std::cout << "{";
            bool first = true;
            for (const auto& item : set) {
                if (!first)
                    std::cout << ", ";
                std::cout << item;
                first = false;
            }
            std::cout << "}";
        } else if (value.type() == typeid(std::map<std::string, int>)) {
            const auto& map =
                std::any_cast<const std::map<std::string, int>&>(value);
            std::cout << "{";
            bool first = true;
            for (const auto& [key, val] : map) {
                if (!first)
                    std::cout << ", ";
                std::cout << "\"" << key << "\": " << val;
                first = false;
            }
            std::cout << "}";
        } else {
            std::cout << "<complex type>";
        }
    } catch (const std::bad_any_cast& e) {
        std::cout << "<bad cast: " << e.what() << ">";
    }

    std::cout << std::endl;
}

// Helper to print optional values
void printOptionalAny(const std::optional<std::any>& optValue,
                      const std::string& prefix = "") {
    if (!optValue.has_value()) {
        std::cout << prefix << "No value (nullopt)" << std::endl;
        return;
    }

    printAny(*optValue, prefix);
}

int main(int argc, char** argv) {
    // Initialize loguru
    loguru::init(argc, argv);
    loguru::add_file("parser_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "=== Parser/To Any Comprehensive Example ===\n\n";

    // Create a parser instance
    atom::utils::Parser parser;

    std::cout << "Example 1: Basic Numeric Parsing\n";
    {
        // Parse integers
        auto intResult = parser.parseLiteral("42");
        std::cout << "Parsing \"42\": ";
        printOptionalAny(intResult);

        // Parse large integers
        auto longResult = parser.parseLiteral("12345678901234");
        std::cout << "Parsing \"12345678901234\": ";
        printOptionalAny(longResult);

        // Parse floating point
        auto floatResult = parser.parseLiteral("3.14159");
        std::cout << "Parsing \"3.14159\": ";
        printOptionalAny(floatResult);

        // Parse scientific notation
        auto sciResult = parser.parseLiteral("6.02e23");
        std::cout << "Parsing \"6.02e23\": ";
        printOptionalAny(sciResult);
    }
    std::cout << std::endl;

    std::cout << "Example 2: Boolean and Character Parsing\n";
    {
        // Parse boolean values
        auto trueResult = parser.parseLiteral("true");
        std::cout << "Parsing \"true\": ";
        printOptionalAny(trueResult);

        auto falseResult = parser.parseLiteral("false");
        std::cout << "Parsing \"false\": ";
        printOptionalAny(falseResult);

        // Parse single character
        auto charResult = parser.parseLiteral("A");
        std::cout << "Parsing \"A\": ";
        printOptionalAny(charResult);
    }
    std::cout << std::endl;

    std::cout << "Example 3: String and Date Parsing\n";
    {
        // Parse string
        auto stringResult = parser.parseLiteral("Hello, world!");
        std::cout << "Parsing \"Hello, world!\": ";
        printOptionalAny(stringResult);

        // Parse date time
        auto dateResult = parser.parseLiteral("2023-10-25 15:30:00");
        std::cout << "Parsing \"2023-10-25 15:30:00\": ";
        printOptionalAny(dateResult);

        // Alternative date format
        auto altDateResult = parser.parseLiteral("2023/10/25 15:30:00");
        std::cout << "Parsing \"2023/10/25 15:30:00\": ";
        printOptionalAny(altDateResult);
    }
    std::cout << std::endl;

    std::cout << "Example 4: Collection Parsing\n";
    {
        // Parse vector of integers
        auto vectorResult = parser.parseLiteral("1,2,3,4,5");
        std::cout << "Parsing \"1,2,3,4,5\": ";
        printOptionalAny(vectorResult);

        // Parse set of floats
        auto setResult =
            parser.parseLiteral("1.1,2.2,3.3,2.2,1.1");  // Note duplicates
        std::cout << "Parsing \"1.1,2.2,3.3,2.2,1.1\": ";
        printOptionalAny(setResult);

        // Parse map (key-value pairs)
        auto mapResult = parser.parseLiteral("name:John,age:30,height:180");
        std::cout << "Parsing \"name:John,age:30,height:180\": ";
        printOptionalAny(mapResult);
    }
    std::cout << std::endl;

    std::cout << "Example 5: Error Handling and Default Values\n";
    {
        // Try to parse invalid input
        auto invalidResult = parser.parseLiteral("@#$%^");
        std::cout << "Parsing \"@#$%^\" (invalid): ";
        printOptionalAny(invalidResult);

        // Parse with default value
        auto withDefaultResult =
            parser.parseLiteralWithDefault("invalid-number", 42);
        std::cout << "Parsing \"invalid-number\" with default 42: ";
        printAny(withDefaultResult);

        // Parse empty input with default
        auto emptyResult =
            parser.parseLiteralWithDefault("", std::string("Default String"));
        std::cout << "Parsing empty string with default: ";
        printAny(emptyResult);
    }
    std::cout << std::endl;

    std::cout << "Example 6: Custom Parsers\n";
    {
        // Register a custom parser for hex values
        parser.registerCustomParser(
            "hex:", [](std::string_view input) -> std::optional<std::any> {
                if (input.substr(0, 4) != "hex:") {
                    return std::nullopt;
                }

                try {
                    std::string hexStr(input.substr(4));
                    size_t pos = 0;
                    int value = std::stoi(hexStr, &pos, 16);
                    if (pos != hexStr.length()) {
                        return std::nullopt;  // Not all characters were parsed
                    }
                    return value;
                } catch (...) {
                    return std::nullopt;
                }
            });

        // Register a custom parser for binary values
        parser.registerCustomParser(
            "bin:", [](std::string_view input) -> std::optional<std::any> {
                if (input.substr(0, 4) != "bin:") {
                    return std::nullopt;
                }

                try {
                    std::string binStr(input.substr(4));
                    size_t pos = 0;
                    int value = std::stoi(binStr, &pos, 2);
                    if (pos != binStr.length()) {
                        return std::nullopt;  // Not all characters were parsed
                    }
                    return value;
                } catch (...) {
                    return std::nullopt;
                }
            });

        // Print registered custom parsers
        std::cout << "Registered custom parsers:" << std::endl;
        parser.printCustomParsers();

        // Use custom parsers
        auto hexResult = parser.parseLiteral("hex:1A");
        std::cout << "Parsing \"hex:1A\": ";
        printOptionalAny(hexResult);

        auto binResult = parser.parseLiteral("bin:1010");
        std::cout << "Parsing \"bin:1010\": ";
        printOptionalAny(binResult);

        // Try another one that won't match custom parsers
        auto otherResult = parser.parseLiteral("oct:777");
        std::cout << "Parsing \"oct:777\" (no custom parser): ";
        printOptionalAny(otherResult);
    }
    std::cout << std::endl;

    std::cout << "Example 7: Batch Conversion\n";
    {
        // Create a vector of strings
        std::vector<std::string_view> inputs = {
            "42",                   // int
            "3.14159",              // double
            "true",                 // bool
            "Hello",                // string
            "2023-11-01 12:00:00",  // datetime
            "1,2,3,4,5",            // vector
            "hex:FF",               // custom parser
            "invalid input"         // will be parsed as string
        };

        std::cout << "Converting batch of inputs:" << std::endl;
        auto results = parser.convertToAnyVector(inputs);

        for (size_t i = 0; i < inputs.size(); ++i) {
            std::cout << "Input \"" << inputs[i] << "\": ";
            printAny(results[i]);
        }
    }
    std::cout << std::endl;

    std::cout << "Example 8: JSON Parsing\n";
    {
        // Create a simple JSON string
        std::string jsonStr = R"({
             "name": "John Doe",
             "age": 30,
             "isStudent": false,
             "grades": [85, 90, 78, 92],
             "address": {
                 "street": "123 Main St",
                 "city": "Anytown",
                 "zipCode": "12345"
             }
         })";

        std::cout << "Parsing JSON:" << std::endl;
        try {
            parser.parseJson(jsonStr);
            std::cout << "JSON parsed successfully! Check log for details."
                      << std::endl;
        } catch (const atom::utils::ParserException& e) {
            std::cerr << "Parser exception: " << e.what() << std::endl;
        }
    }
    std::cout << std::endl;

    std::cout << "Example 9: CSV Parsing\n";
    {
        // Create a CSV string
        std::string csvStr =
            "Name,Age,City,Score\n"
            "John,30,New York,85.5\n"
            "Alice,25,Boston,92.3\n"
            "Bob,35,Chicago,78.9\n"
            "Carol,28,Seattle,88.7\n";

        std::cout << "Parsing CSV:" << std::endl;
        try {
            parser.parseCsv(csvStr);
            std::cout << "CSV parsed successfully! Check log for details."
                      << std::endl;
        } catch (const atom::utils::ParserException& e) {
            std::cerr << "Parser exception: " << e.what() << std::endl;
        }

        // Try with different delimiter
        std::string tsvStr =
            "Name\tAge\tCity\tScore\n"
            "John\t30\tNew York\t85.5\n"
            "Alice\t25\tBoston\t92.3\n";

        std::cout << "Parsing TSV (tab-separated values):" << std::endl;
        try {
            parser.parseCsv(tsvStr, '\t');
            std::cout << "TSV parsed successfully! Check log for details."
                      << std::endl;
        } catch (const atom::utils::ParserException& e) {
            std::cerr << "Parser exception: " << e.what() << std::endl;
        }
    }
    std::cout << std::endl;

    std::cout << "Example 10: Parallel Parsing\n";
    {
        // Create a large vector of strings to parse
        std::vector<std::string> largeInput;
        for (int i = 0; i < 1000; ++i) {
            largeInput.push_back(std::to_string(i));
        }

        std::cout << "Parsing 1000 values in parallel..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        auto results = parser.parseParallel(largeInput);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        std::cout << "Parsed " << results.size() << " values in " << duration
                  << "ms" << std::endl;

        // Show a few results
        std::cout << "First 5 results:" << std::endl;
        for (int i = 0; i < 5 && i < static_cast<int>(results.size()); ++i) {
            std::cout << "  ";
            printAny(results[i]);
        }
    }
    std::cout << std::endl;

    std::cout << "Example 11: Print and Log Parsing\n";
    {
        // Parse a value
        auto result = parser.parseLiteral("42.5");

        // Print the value
        std::cout << "Calling print() on parser (check log output):"
                  << std::endl;
        parser.print(*result);

        // Log the parsing
        std::cout << "Calling logParsing() on parser (check log output):"
                  << std::endl;
        parser.logParsing("42.5", *result);
    }
    std::cout << std::endl;

    std::cout << "Example 12: Error Handling for Edge Cases\n";
    {
        try {
            // Try to parse empty string - should throw
            auto result = parser.parseLiteral("");
            std::cout << "This shouldn't be reached" << std::endl;
        } catch (const atom::utils::ParserException& e) {
            std::cout << "Expected exception for empty input: " << e.what()
                      << std::endl;
        }

        // Parse with multiple threads trying to use the same parser instance
        std::cout
            << "Testing concurrent parsing (first should succeed, others fail):"
            << std::endl;
        auto parseFunc = [&parser](int id) {
            try {
                auto result = parser.parseLiteral("value" + std::to_string(id));
                std::cout << "Thread " << id << " succeeded" << std::endl;
            } catch (const atom::utils::ParserException& e) {
                std::cout << "Thread " << id << " failed: " << e.what()
                          << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(
                50));  // Give time for first thread to finish
        };

        std::thread t1(parseFunc, 1);
        std::thread t2(parseFunc, 2);
        std::thread t3(parseFunc, 3);

        t1.join();
        t2.join();
        t3.join();
    }

    return 0;
}