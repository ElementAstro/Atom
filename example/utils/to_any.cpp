/*
 * parser_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils::Parser class
 */

#include "atom/utils/to_any.hpp"
#include <spdlog/spdlog.h>
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

// Helper function to print any value if possible
void printAny(const std::any& value, const std::string& prefix = "") {
    if (!value.has_value()) {
        spdlog::info("{}Empty value", prefix);
        return;
    }
    std::ostringstream oss;
    oss << prefix << "Type: " << value.type().name() << " - Value: ";
    try {
        // Handle common types
        if (value.type() == typeid(int)) {
            oss << std::any_cast<int>(value);
        } else if (value.type() == typeid(long)) {
            oss << std::any_cast<long>(value);
        } else if (value.type() == typeid(long long)) {
            oss << std::any_cast<long long>(value);
        } else if (value.type() == typeid(unsigned int)) {
            oss << std::any_cast<unsigned int>(value);
        } else if (value.type() == typeid(float)) {
            oss << std::any_cast<float>(value);
        } else if (value.type() == typeid(double)) {
            oss << std::any_cast<double>(value);
        } else if (value.type() == typeid(bool)) {
            oss << (std::any_cast<bool>(value) ? "true" : "false");
        } else if (value.type() == typeid(char)) {
            oss << "'" << std::any_cast<char>(value) << "'";
        } else if (value.type() == typeid(std::string)) {
            oss << "\"" << std::any_cast<std::string>(value) << "\"";
        } else if (value.type() ==
                   typeid(std::chrono::system_clock::time_point)) {
            auto timePoint =
                std::any_cast<std::chrono::system_clock::time_point>(value);
            std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
            oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        } else if (value.type() == typeid(std::vector<int>)) {
            const auto& vec = std::any_cast<const std::vector<int>&>(value);
            oss << "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << vec[i];
            }
            oss << "]";
        } else if (value.type() == typeid(std::set<float>)) {
            const auto& set = std::any_cast<const std::set<float>&>(value);
            oss << "{";
            bool first = true;
            for (const auto& item : set) {
                if (!first)
                    oss << ", ";
                oss << item;
                first = false;
            }
            oss << "}";
        } else if (value.type() == typeid(std::map<std::string, int>)) {
            const auto& map =
                std::any_cast<const std::map<std::string, int>&>(value);
            oss << "{";
            bool first = true;
            for (const auto& [key, val] : map) {
                if (!first)
                    oss << ", ";
                oss << "\"" << key << "\": " << val;
                first = false;
            }
            oss << "}";
        } else {
            oss << "<complex type>";
        }
    } catch (const std::bad_any_cast& e) {
        oss << "<bad cast: " << e.what() << ">";
    }
    spdlog::info("{}", oss.str());
}

// Helper to print optional values
void printOptionalAny(const std::optional<std::any>& optValue,
                      const std::string& prefix = "") {
    if (!optValue.has_value()) {
        spdlog::info("{}No value (nullopt)", prefix);
        return;
    }
    printAny(*optValue, prefix);
}

int main(int argc, char** argv) {
    // Initialize loguru
    loguru::init(argc, argv);
    loguru::add_file("parser_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    spdlog::info("=== Parser/To Any Comprehensive Example ===");

    // Create a parser instance
    atom::utils::Parser parser;

    spdlog::info("Example 1: Basic Numeric Parsing");
    {
        // Parse integers
        auto intResult = parser.parseLiteral("42");
        spdlog::info("Parsing \"42\":");
        printOptionalAny(intResult);

        // Parse large integers
        auto longResult = parser.parseLiteral("12345678901234");
        spdlog::info("Parsing \"12345678901234\":");
        printOptionalAny(longResult);

        // Parse floating point
        auto floatResult = parser.parseLiteral("3.14159");
        spdlog::info("Parsing \"3.14159\":");
        printOptionalAny(floatResult);

        // Parse scientific notation
        auto sciResult = parser.parseLiteral("6.02e23");
        spdlog::info("Parsing \"6.02e23\":");
        printOptionalAny(sciResult);
    }

    spdlog::info("Example 2: Boolean and Character Parsing");
    {
        // Parse boolean values
        auto trueResult = parser.parseLiteral("true");
        spdlog::info("Parsing \"true\":");
        printOptionalAny(trueResult);

        auto falseResult = parser.parseLiteral("false");
        spdlog::info("Parsing \"false\":");
        printOptionalAny(falseResult);

        // Parse single character
        auto charResult = parser.parseLiteral("A");
        spdlog::info("Parsing \"A\":");
        printOptionalAny(charResult);
    }

    spdlog::info("Example 3: String and Date Parsing");
    {
        // Parse string
        auto stringResult = parser.parseLiteral("Hello, world!");
        spdlog::info("Parsing \"Hello, world!\":");
        printOptionalAny(stringResult);

        // Parse date time
        auto dateResult = parser.parseLiteral("2023-10-25 15:30:00");
        spdlog::info("Parsing \"2023-10-25 15:30:00\":");
        printOptionalAny(dateResult);

        // Alternative date format
        auto altDateResult = parser.parseLiteral("2023/10/25 15:30:00");
        spdlog::info("Parsing \"2023/10/25 15:30:00\":");
        printOptionalAny(altDateResult);
    }

    spdlog::info("Example 4: Collection Parsing");
    {
        // Parse vector of integers
        auto vectorResult = parser.parseLiteral("1,2,3,4,5");
        spdlog::info("Parsing \"1,2,3,4,5\":");
        printOptionalAny(vectorResult);

        // Parse set of floats
        auto setResult = parser.parseLiteral("1.1,2.2,3.3,2.2,1.1");
        spdlog::info("Parsing \"1.1,2.2,3.3,2.2,1.1\":");
        printOptionalAny(setResult);

        // Parse map (key-value pairs)
        auto mapResult = parser.parseLiteral("name:John,age:30,height:180");
        spdlog::info("Parsing \"name:John,age:30,height:180\":");
        printOptionalAny(mapResult);
    }

    spdlog::info("Example 5: Error Handling and Default Values");
    {
        // Try to parse invalid input
        auto invalidResult = parser.parseLiteral("@#$%^");
        spdlog::info("Parsing \"@#$%^\" (invalid):");
        printOptionalAny(invalidResult);

        // Parse with default value
        auto withDefaultResult =
            parser.parseLiteralWithDefault("invalid-number", 42);
        spdlog::info("Parsing \"invalid-number\" with default 42:");
        printAny(withDefaultResult);

        // Parse empty input with default
        auto emptyResult =
            parser.parseLiteralWithDefault("", std::string("Default String"));
        spdlog::info("Parsing empty string with default:");
        printAny(emptyResult);
    }

    spdlog::info("Example 6: Custom Parsers");
    {
        // Register a custom parser for hex values
        parser.registerCustomParser(
            "hex:", [](std::string_view input) -> std::optional<std::any> {
                if (input.substr(0, 4) != "hex:")
                    return std::nullopt;
                try {
                    std::string hexStr(input.substr(4));
                    size_t pos = 0;
                    int value = std::stoi(hexStr, &pos, 16);
                    if (pos != hexStr.length())
                        return std::nullopt;
                    return value;
                } catch (...) {
                    return std::nullopt;
                }
            });

        // Register a custom parser for binary values
        parser.registerCustomParser(
            "bin:", [](std::string_view input) -> std::optional<std::any> {
                if (input.substr(0, 4) != "bin:")
                    return std::nullopt;
                try {
                    std::string binStr(input.substr(4));
                    size_t pos = 0;
                    int value = std::stoi(binStr, &pos, 2);
                    if (pos != binStr.length())
                        return std::nullopt;
                    return value;
                } catch (...) {
                    return std::nullopt;
                }
            });

        // Print registered custom parsers
        spdlog::info("Registered custom parsers:");
        parser.printCustomParsers();

        // Use custom parsers
        auto hexResult = parser.parseLiteral("hex:1A");
        spdlog::info("Parsing \"hex:1A\":");
        printOptionalAny(hexResult);

        auto binResult = parser.parseLiteral("bin:1010");
        spdlog::info("Parsing \"bin:1010\":");
        printOptionalAny(binResult);

        // Try another one that won't match custom parsers
        auto otherResult = parser.parseLiteral("oct:777");
        spdlog::info("Parsing \"oct:777\" (no custom parser):");
        printOptionalAny(otherResult);
    }

    spdlog::info("Example 7: Batch Conversion");
    {
        // Create a vector of strings
        std::vector<std::string_view> inputs = {"42",
                                                "3.14159",
                                                "true",
                                                "Hello",
                                                "2023-11-01 12:00:00",
                                                "1,2,3,4,5",
                                                "hex:FF",
                                                "invalid input"};
        spdlog::info("Converting batch of inputs:");
        auto results = parser.convertToAnyVector(inputs);
        for (size_t i = 0; i < inputs.size(); ++i) {
            spdlog::info("Input \"{}\":", inputs[i]);
            printAny(results[i]);
        }
    }

    spdlog::info("Example 8: JSON Parsing");
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
        spdlog::info("Parsing JSON:");
        try {
            parser.parseJson(jsonStr);
            spdlog::info("JSON parsed successfully! Check log for details.");
        } catch (const atom::utils::ParserException& e) {
            spdlog::error("Parser exception: {}", e.what());
        }
    }

    spdlog::info("Example 9: CSV Parsing");
    {
        // Create a CSV string
        std::string csvStr =
            "Name,Age,City,Score\n"
            "John,30,New York,85.5\n"
            "Alice,25,Boston,92.3\n"
            "Bob,35,Chicago,78.9\n"
            "Carol,28,Seattle,88.7\n";
        spdlog::info("Parsing CSV:");
        try {
            parser.parseCsv(csvStr);
            spdlog::info("CSV parsed successfully! Check log for details.");
        } catch (const atom::utils::ParserException& e) {
            spdlog::error("Parser exception: {}", e.what());
        }

        // Try with different delimiter
        std::string tsvStr =
            "Name\tAge\tCity\tScore\n"
            "John\t30\tNew York\t85.5\n"
            "Alice\t25\tBoston\t92.3\n";
        spdlog::info("Parsing TSV (tab-separated values):");
        try {
            parser.parseCsv(tsvStr, '\t');
            spdlog::info("TSV parsed successfully! Check log for details.");
        } catch (const atom::utils::ParserException& e) {
            spdlog::error("Parser exception: {}", e.what());
        }
    }

    spdlog::info("Example 10: Parallel Parsing");
    {
        // Create a large vector of strings to parse
        std::vector<std::string> largeInput;
        for (int i = 0; i < 1000; ++i) {
            largeInput.push_back(std::to_string(i));
        }
        spdlog::info("Parsing 1000 values in parallel...");
        auto start = std::chrono::high_resolution_clock::now();
        auto results = parser.parseParallel(largeInput);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        spdlog::info("Parsed {} values in {}ms", results.size(), duration);
        for (int i = 0; i < 5 && i < static_cast<int>(results.size()); ++i) {
            spdlog::info("Result {}:", i);
            printAny(results[i]);
        }
    }

    spdlog::info("Example 11: Print and Log Parsing");
    {
        // Parse a value
        auto result = parser.parseLiteral("42.5");

        // Print the value
        spdlog::info("Calling print() on parser (check log output):");
        parser.print(*result);

        // Log the parsing
        spdlog::info("Calling logParsing() on parser (check log output):");
        parser.logParsing("42.5", *result);
    }

    spdlog::info("Example 12: Error Handling for Edge Cases");
    {
        try {
            // Try to parse empty string - should throw
            parser.parseLiteral("");
            spdlog::info("This shouldn't be reached");
        } catch (const atom::utils::ParserException& e) {
            spdlog::info("Expected exception for empty input: {}", e.what());
        }
        spdlog::info(
            "Testing concurrent parsing (first should succeed, others fail):");
        auto parseFunc = [&parser](int id) {
            try {
                parser.parseLiteral("value" + std::to_string(id));
                spdlog::info("Thread {} succeeded", id);
            } catch (const atom::utils::ParserException& e) {
                spdlog::info("Thread {} failed: {}", id, e.what());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
