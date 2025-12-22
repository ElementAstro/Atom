/**
 * @file to_any_example.cpp
 * @brief Comprehensive examples for atom::utils Parser class
 *
 * This example demonstrates the Parser class functionality including:
 * - Literal parsing (integers, floats, booleans, strings)
 * - JSON parsing
 * - CSV parsing
 * - Custom parser registration
 * - Parallel parsing
 * - Error handling
 */

#include "atom/utils/conversion/to_any.hpp"

#include <any>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// Helper to print std::any valuevoid printAnyValue(const std::any& value) {
if (!value.has_value()) {
    std::cout << "(empty)";
    return;
}

try {
    if (value.type() == typeid(int)) {
        std::cout << "int: " << std::any_cast<int>(value);
    } else if (value.type() == typeid(long)) {
        std::cout << "long: " << std::any_cast<long>(value);
    } else if (value.type() == typeid(long long)) {
        std::cout << "long long: " << std::any_cast<long long>(value);
    } else if (value.type() == typeid(double)) {
        std::cout << "double: " << std::any_cast<double>(value);
    } else if (value.type() == typeid(float)) {
        std::cout << "float: " << std::any_cast<float>(value);
    } else if (value.type() == typeid(bool)) {
        std::cout << "bool: " << std::boolalpha << std::any_cast<bool>(value);
    } else if (value.type() == typeid(std::string)) {
        std::cout << "string: \"" << std::any_cast<std::string>(value) << "\"";
    } else if (value.type() == typeid(const char*)) {
        std::cout << "const char*: \"" << std::any_cast<const char*>(value)
                  << "\"";
    } else {
        std::cout << "unknown type: " << value.type().name();
    }
} catch (const std::bad_any_cast& e) {
    std::cout << "cast error: " << e.what();
}
}

// ============================================
// 1. Basic Literal Parsing
// ============================================
void demonstrateLiteralParsing() {
    printSection("1. Basic Literal Parsing");

    Parser parser;

    // Integer literals
    std::cout << "--- Integer Literals ---" << std::endl;
    std::vector<std::string> intLiterals = {"0",    "42",     "-100", "1000000",
                                            "0x1F", "0b1010", "0777"};

    for (const auto& literal : intLiterals) {
        auto result = parser.parseLiteral(literal);
        std::cout << "\"" << literal << "\" -> ";
        if (result) {
            printAnyValue(*result);
        } else {
            std::cout << "(parse failed)";
        }
        std::cout << std::endl;
    }

    // Floating point literals
    std::cout << "\n--- Floating Point Literals ---" << std::endl;
    std::vector<std::string> floatLiterals = {"3.14",   "-2.718", "1e10",
                                              "1.5e-3", "0.0",    ".5"};

    for (const auto& literal : floatLiterals) {
        auto result = parser.parseLiteral(literal);
        std::cout << "\"" << literal << "\" -> ";
        if (result) {
            printAnyValue(*result);
        } else {
            std::cout << "(parse failed)";
        }
        std::cout << std::endl;
    }

    // Boolean literals
    std::cout << "\n--- Boolean Literals ---" << std::endl;
    std::vector<std::string> boolLiterals = {"true",  "false", "True",
                                             "False", "TRUE",  "FALSE"};

    for (const auto& literal : boolLiterals) {
        auto result = parser.parseLiteral(literal);
        std::cout << "\"" << literal << "\" -> ";
        if (result) {
            printAnyValue(*result);
        } else {
            std::cout << "(parse failed)";
        }
        std::cout << std::endl;
    }

    // String literals
    std::cout << "\n--- String Literals ---" << std::endl;
    std::vector<std::string> stringLiterals = {
        "\"hello\"", "'world'", "\"with spaces\"", "\"escaped\\\"quote\""};

    for (const auto& literal : stringLiterals) {
        auto result = parser.parseLiteral(literal);
        std::cout << literal << " -> ";
        if (result) {
            printAnyValue(*result);
        } else {
            std::cout << "(parse failed)";
        }
        std::cout << std::endl;
    }
}

// ============================================
// 2. Parse with Default Value
// ============================================
void demonstrateParseWithDefault() {
    printSection("2. Parse with Default Value");

    Parser parser;

    // Valid inputs
    std::cout << "--- Valid Inputs ---" << std::endl;
    auto result1 = parser.parseLiteralWithDefault("42", std::any(0));
    std::cout << "\"42\" with default 0 -> ";
    printAnyValue(result1);
    std::cout << std::endl;

    auto result2 = parser.parseLiteralWithDefault("3.14", std::any(0.0));
    std::cout << "\"3.14\" with default 0.0 -> ";
    printAnyValue(result2);
    std::cout << std::endl;

    // Invalid inputs - should return default
    std::cout << "\n--- Invalid Inputs (returns default) ---" << std::endl;
    auto result3 =
        parser.parseLiteralWithDefault("not_a_number", std::any(999));
    std::cout << "\"not_a_number\" with default 999 -> ";
    printAnyValue(result3);
    std::cout << std::endl;

    auto result4 = parser.parseLiteralWithDefault("", std::any(-1));
    std::cout << "\"\" (empty) with default -1 -> ";
    printAnyValue(result4);
    std::cout << std::endl;
}

// ============================================
// 3. JSON Parsing
// ============================================
void demonstrateJsonParsing() {
    printSection("3. JSON Parsing");

    Parser parser;

    // Simple JSON objects
    std::cout << "--- Simple JSON Objects ---" << std::endl;

    std::string json1 = R"({"name": "John", "age": 30})";
    std::cout << "Parsing: " << json1 << std::endl;
    try {
        parser.parseJson(json1);
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }

    std::string json2 = R"({"items": [1, 2, 3], "active": true})";
    std::cout << "\nParsing: " << json2 << std::endl;
    try {
        parser.parseJson(json2);
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }

    // Nested JSON
    std::cout << "\n--- Nested JSON ---" << std::endl;
    std::string json3 = R"({
        "user": {
            "name": "Alice",
            "address": {
                "city": "New York",
                "zip": "10001"
            }
        },
        "scores": [95, 87, 92]
    })";
    std::cout << "Parsing nested JSON..." << std::endl;
    try {
        parser.parseJson(json3);
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }

    // Invalid JSON
    std::cout << "\n--- Invalid JSON ---" << std::endl;
    std::string invalidJson = R"({"name": "John", age: 30})";  // Missing quotes
    std::cout << "Parsing: " << invalidJson << std::endl;
    try {
        parser.parseJson(invalidJson);
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error (expected): " << e.what() << std::endl;
    }
}

// ============================================
// 4. CSV Parsing
// ============================================
void demonstrateCsvParsing() {
    printSection("4. CSV Parsing");

    Parser parser;

    // Simple CSV
    std::cout << "--- Simple CSV (comma delimiter) ---" << std::endl;
    std::string csv1 = "name,age,city\nJohn,30,New York\nJane,25,Los Angeles";
    std::cout << "CSV data:" << std::endl;
    std::cout << csv1 << std::endl;
    std::cout << "\nParsing..." << std::endl;
    try {
        parser.parseCsv(csv1, ',');
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }

    // Tab-separated values
    std::cout << "\n--- Tab-Separated Values ---" << std::endl;
    std::string tsv = "id\tproduct\tprice\n1\tApple\t1.50\n2\tBanana\t0.75";
    std::cout << "TSV data:" << std::endl;
    std::cout << tsv << std::endl;
    std::cout << "\nParsing with tab delimiter..." << std::endl;
    try {
        parser.parseCsv(tsv, '\t');
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }

    // Semicolon-separated (European format)
    std::cout << "\n--- Semicolon-Separated (European format) ---" << std::endl;
    std::string ssv =
        "name;value;unit\ntemperature;25,5;celsius\npressure;1013,25;hPa";
    std::cout << "SSV data:" << std::endl;
    std::cout << ssv << std::endl;
    std::cout << "\nParsing with semicolon delimiter..." << std::endl;
    try {
        parser.parseCsv(ssv, ';');
        std::cout << "  -> Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  -> Error: " << e.what() << std::endl;
    }
}

// ============================================
// 5. Custom Parser Registration
// ============================================
void demonstrateCustomParsers() {
    printSection("5. Custom Parser Registration");

    Parser parser;

    // Register a custom parser for dates
    std::cout << "--- Registering Custom Date Parser ---" << std::endl;
    parser.registerCustomParser(
        "date", [](std::string_view input) -> std::optional<std::any> {
            // Simple date parser (YYYY-MM-DD format)
            if (input.length() == 10 && input[4] == '-' && input[7] == '-') {
                return std::any(std::string(input));
            }
            return std::nullopt;
        });
    std::cout << "Registered 'date' parser for YYYY-MM-DD format" << std::endl;

    // Register a custom parser for colors
    std::cout << "\n--- Registering Custom Color Parser ---" << std::endl;
    parser.registerCustomParser(
        "color", [](std::string_view input) -> std::optional<std::any> {
            // Parse hex color (#RRGGBB)
            if (input.length() == 7 && input[0] == '#') {
                return std::any(std::string(input));
            }
            return std::nullopt;
        });
    std::cout << "Registered 'color' parser for #RRGGBB format" << std::endl;

    // Register a custom parser for IP addresses
    std::cout << "\n--- Registering Custom IP Parser ---" << std::endl;
    parser.registerCustomParser(
        "ip", [](std::string_view input) -> std::optional<std::any> {
            // Simple IP validation (just check for dots)
            int dotCount = 0;
            for (char c : input) {
                if (c == '.')
                    dotCount++;
            }
            if (dotCount == 3) {
                return std::any(std::string(input));
            }
            return std::nullopt;
        });
    std::cout << "Registered 'ip' parser for IPv4 addresses" << std::endl;

    // Print registered parsers
    std::cout << "\n--- Registered Custom Parsers ---" << std::endl;
    parser.printCustomParsers();
}

// ============================================
// 6. Parallel Parsing
// ============================================
void demonstrateParallelParsing() {
    printSection("6. Parallel Parsing");

    Parser parser;

    // Create a batch of inputs to parse
    std::vector<std::string> inputs = {
        "42",  "3.14", "true", "100",   "-50",  "2.718", "false", "999",
        "1.5", "0",    "1000", "0.001", "true", "-100",  "42.5"};

    std::cout << "Inputs to parse:" << std::endl;
    for (size_t i = 0; i < inputs.size(); ++i) {
        std::cout << "  [" << i << "] \"" << inputs[i] << "\"" << std::endl;
    }

    std::cout << "\n--- Parallel Parsing Results ---" << std::endl;
    try {
        auto results = parser.parseParallel(inputs);

        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "  [" << i << "] \"" << inputs[i] << "\" -> ";
            printAnyValue(results[i]);
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error during parallel parsing: " << e.what() << std::endl;
    }
}

// ============================================
// 7. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("7. Complex Use Cases");

    Parser parser;

    // Use case 1: Configuration file parsing
    std::cout << "--- Configuration File Parsing ---" << std::endl;
    std::vector<std::pair<std::string, std::string>> configLines = {
        {"server.port", "8080"},
        {"server.timeout", "30.5"},
        {"debug.enabled", "true"},
        {"app.name", "\"MyApplication\""},
        {"max.connections", "100"}};

    std::cout << "Configuration:" << std::endl;
    for (const auto& [key, value] : configLines) {
        auto parsed = parser.parseLiteral(value);
        std::cout << "  " << key << " = " << value << " -> ";
        if (parsed) {
            printAnyValue(*parsed);
        } else {
            std::cout << "(raw string)";
        }
        std::cout << std::endl;
    }

    // Use case 2: Command-line argument parsing
    std::cout << "\n--- Command-Line Argument Parsing ---" << std::endl;
    std::vector<std::string> args = {"--verbose", "true",    "--count",
                                     "10",        "--ratio", "0.75",
                                     "--name",    "\"test\""};

    std::cout << "Arguments:" << std::endl;
    for (size_t i = 0; i < args.size(); i += 2) {
        if (i + 1 < args.size()) {
            auto parsed = parser.parseLiteral(args[i + 1]);
            std::cout << "  " << args[i] << " = ";
            if (parsed) {
                printAnyValue(*parsed);
            } else {
                std::cout << args[i + 1];
            }
            std::cout << std::endl;
        }
    }

    // Use case 3: Data validation
    std::cout << "\n--- Data Validation ---" << std::endl;
    std::vector<std::string> userInputs = {"42",   "hello",     "3.14",
                                           "true", "not_valid", "100"};

    std::cout << "Validating user inputs:" << std::endl;
    for (const auto& input : userInputs) {
        auto parsed = parser.parseLiteral(input);
        std::cout << "  \"" << input << "\": ";
        if (parsed) {
            std::cout << "Valid (";
            printAnyValue(*parsed);
            std::cout << ")" << std::endl;
        } else {
            std::cout << "Invalid or plain string" << std::endl;
        }
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Parser (to_any) Examples" << std::endl;
    std::cout << "  atom::utils::Parser" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateLiteralParsing();
        demonstrateParseWithDefault();
        demonstrateJsonParsing();
        demonstrateCsvParsing();
        demonstrateCustomParsers();
        demonstrateParallelParsing();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All Parser examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
