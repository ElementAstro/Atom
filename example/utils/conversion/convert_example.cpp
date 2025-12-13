/**
 * @file convert_example.cpp
 * @brief Comprehensive examples for atom::utils conversion utilities
 *
 * This example demonstrates type conversion functions including:
 * - Basic type conversions
 * - String to numeric conversions
 * - Numeric to string conversions
 * - Safe conversion with error handling
 */

#include "atom/utils/conversion/convert.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// ============================================
// 1. Basic Type Conversions
// ============================================
void demonstrateBasicConversions() {
    printSection("1. Basic Type Conversions");

    // Integer conversions
    std::cout << "--- Integer Conversions ---" << std::endl;
    int intVal = 42;
    long longVal = static_cast<long>(intVal);
    short shortVal = static_cast<short>(intVal);

    std::cout << "int: " << intVal << std::endl;
    std::cout << "  -> long: " << longVal << std::endl;
    std::cout << "  -> short: " << shortVal << std::endl;

    // Floating point conversions
    std::cout << "\n--- Floating Point Conversions ---" << std::endl;
    double doubleVal = 3.14159265358979;
    float floatVal = static_cast<float>(doubleVal);

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "double: " << doubleVal << std::endl;
    std::cout << "  -> float: " << floatVal << " (precision loss)" << std::endl;

    // Integer to floating point
    std::cout << "\n--- Integer to Floating Point ---" << std::endl;
    int intNum = 100;
    double asDouble = static_cast<double>(intNum);
    std::cout << "int " << intNum << " -> double " << asDouble << std::endl;
}

// ============================================
// 2. String Conversions
// ============================================
void demonstrateStringConversions() {
    printSection("2. String Conversions");

    // String to integer
    std::cout << "--- String to Integer ---" << std::endl;
    std::vector<std::string> intStrings = {"123", "-456", "0", "999999"};

    for (const auto& str : intStrings) {
        try {
            int val = std::stoi(str);
            std::cout << "\"" << str << "\" -> " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "\"" << str << "\" -> Error: " << e.what() << std::endl;
        }
    }

    // String to double
    std::cout << "\n--- String to Double ---" << std::endl;
    std::vector<std::string> doubleStrings = {"3.14", "-2.718", "0.0", "1e10",
                                               "1.5e-3"};

    std::cout << std::fixed << std::setprecision(6);
    for (const auto& str : doubleStrings) {
        try {
            double val = std::stod(str);
            std::cout << "\"" << str << "\" -> " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "\"" << str << "\" -> Error: " << e.what() << std::endl;
        }
    }

    // Integer to string
    std::cout << "\n--- Integer to String ---" << std::endl;
    std::vector<int> integers = {0, 42, -100, 1000000};

    for (int val : integers) {
        std::string str = std::to_string(val);
        std::cout << val << " -> \"" << str << "\"" << std::endl;
    }

    // Double to string
    std::cout << "\n--- Double to String ---" << std::endl;
    std::vector<double> doubles = {0.0, 3.14159, -2.718, 1e6};

    for (double val : doubles) {
        std::string str = std::to_string(val);
        std::cout << val << " -> \"" << str << "\"" << std::endl;
    }
}

// ============================================
// 3. Hexadecimal Conversions
// ============================================
void demonstrateHexConversions() {
    printSection("3. Hexadecimal Conversions");

    // Integer to hex string
    std::cout << "--- Integer to Hex ---" << std::endl;
    std::vector<int> values = {0, 15, 16, 255, 256, 4096, 65535};

    for (int val : values) {
        std::cout << std::dec << val << " -> 0x" << std::hex << std::uppercase
                  << val << std::endl;
    }

    // Hex string to integer
    std::cout << std::dec;  // Reset to decimal
    std::cout << "\n--- Hex to Integer ---" << std::endl;
    std::vector<std::string> hexStrings = {"0", "F", "10", "FF", "100", "1000",
                                            "FFFF"};

    for (const auto& hexStr : hexStrings) {
        try {
            int val = std::stoi(hexStr, nullptr, 16);
            std::cout << "0x" << hexStr << " -> " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "0x" << hexStr << " -> Error: " << e.what()
                      << std::endl;
        }
    }
}

// ============================================
// 4. Binary Conversions
// ============================================
void demonstrateBinaryConversions() {
    printSection("4. Binary Conversions");

    // Integer to binary string
    std::cout << "--- Integer to Binary ---" << std::endl;
    std::vector<int> values = {0, 1, 2, 5, 10, 15, 255};

    for (int val : values) {
        std::string binary;
        int temp = val;
        if (temp == 0) {
            binary = "0";
        } else {
            while (temp > 0) {
                binary = (temp % 2 == 0 ? "0" : "1") + binary;
                temp /= 2;
            }
        }
        std::cout << val << " -> 0b" << binary << std::endl;
    }

    // Binary string to integer
    std::cout << "\n--- Binary to Integer ---" << std::endl;
    std::vector<std::string> binaryStrings = {"0",    "1",    "10",
                                               "101",  "1010", "1111",
                                               "11111111"};

    for (const auto& binStr : binaryStrings) {
        try {
            int val = std::stoi(binStr, nullptr, 2);
            std::cout << "0b" << binStr << " -> " << val << std::endl;
        } catch (const std::exception& e) {
            std::cout << "0b" << binStr << " -> Error: " << e.what()
                      << std::endl;
        }
    }
}

// ============================================
// 5. Boolean Conversions
// ============================================
void demonstrateBooleanConversions() {
    printSection("5. Boolean Conversions");

    // String to bool
    std::cout << "--- String to Boolean ---" << std::endl;
    std::vector<std::string> boolStrings = {"true", "false", "1", "0", "yes",
                                             "no",   "TRUE",  "FALSE"};

    for (const auto& str : boolStrings) {
        bool val = (str == "true" || str == "TRUE" || str == "1" ||
                    str == "yes" || str == "YES");
        std::cout << "\"" << str << "\" -> " << std::boolalpha << val
                  << std::endl;
    }

    // Bool to string
    std::cout << "\n--- Boolean to String ---" << std::endl;
    std::cout << "true -> \"" << (true ? "true" : "false") << "\"" << std::endl;
    std::cout << "false -> \"" << (false ? "true" : "false") << "\""
              << std::endl;

    // Integer to bool
    std::cout << "\n--- Integer to Boolean ---" << std::endl;
    std::vector<int> intVals = {0, 1, -1, 42, 100};

    for (int val : intVals) {
        bool boolVal = static_cast<bool>(val);
        std::cout << val << " -> " << std::boolalpha << boolVal << std::endl;
    }
}

// ============================================
// 6. Safe Conversions with Error Handling
// ============================================
void demonstrateSafeConversions() {
    printSection("6. Safe Conversions with Error Handling");

    // Safe string to int
    std::cout << "--- Safe String to Int ---" << std::endl;
    std::vector<std::string> testStrings = {"123",  "-456", "abc",
                                             "12.5", "",     "999999999999"};

    for (const auto& str : testStrings) {
        try {
            size_t pos = 0;
            int val = std::stoi(str, &pos);
            if (pos != str.length()) {
                std::cout << "\"" << str << "\" -> " << val
                          << " (partial conversion, stopped at position " << pos
                          << ")" << std::endl;
            } else {
                std::cout << "\"" << str << "\" -> " << val << " (success)"
                          << std::endl;
            }
        } catch (const std::invalid_argument&) {
            std::cout << "\"" << str << "\" -> Invalid argument" << std::endl;
        } catch (const std::out_of_range&) {
            std::cout << "\"" << str << "\" -> Out of range" << std::endl;
        }
    }

    // Safe string to double
    std::cout << "\n--- Safe String to Double ---" << std::endl;
    std::vector<std::string> doubleTestStrings = {"3.14",  "abc",  "1e1000",
                                                   "-inf",  "nan",  "1.5abc"};

    std::cout << std::fixed << std::setprecision(6);
    for (const auto& str : doubleTestStrings) {
        try {
            size_t pos = 0;
            double val = std::stod(str, &pos);
            if (pos != str.length()) {
                std::cout << "\"" << str << "\" -> " << val
                          << " (partial conversion)" << std::endl;
            } else {
                std::cout << "\"" << str << "\" -> " << val << std::endl;
            }
        } catch (const std::invalid_argument&) {
            std::cout << "\"" << str << "\" -> Invalid argument" << std::endl;
        } catch (const std::out_of_range&) {
            std::cout << "\"" << str << "\" -> Out of range" << std::endl;
        }
    }
}

// ============================================
// 7. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("7. Complex Use Cases");

    // Use case 1: Parsing configuration values
    std::cout << "--- Configuration Parsing ---" << std::endl;

    struct ConfigValue {
        std::string key;
        std::string value;
    };

    std::vector<ConfigValue> config = {{"port", "8080"},
                                        {"timeout", "30.5"},
                                        {"debug", "true"},
                                        {"max_connections", "100"}};

    for (const auto& [key, value] : config) {
        std::cout << key << " = " << value;

        // Try to determine type and convert
        if (value == "true" || value == "false") {
            bool boolVal = (value == "true");
            std::cout << " (bool: " << std::boolalpha << boolVal << ")";
        } else if (value.find('.') != std::string::npos) {
            try {
                double doubleVal = std::stod(value);
                std::cout << " (double: " << doubleVal << ")";
            } catch (...) {
                std::cout << " (string)";
            }
        } else {
            try {
                int intVal = std::stoi(value);
                std::cout << " (int: " << intVal << ")";
            } catch (...) {
                std::cout << " (string)";
            }
        }
        std::cout << std::endl;
    }

    // Use case 2: Data format conversion
    std::cout << "\n--- Data Format Conversion ---" << std::endl;
    int value = 255;

    std::cout << "Value: " << value << std::endl;
    std::cout << "  Decimal: " << std::dec << value << std::endl;
    std::cout << "  Hex: 0x" << std::hex << std::uppercase << value << std::endl;
    std::cout << "  Octal: 0" << std::oct << value << std::endl;

    // Binary representation
    std::string binary;
    int temp = value;
    while (temp > 0) {
        binary = (temp % 2 == 0 ? "0" : "1") + binary;
        temp /= 2;
    }
    std::cout << std::dec;  // Reset
    std::cout << "  Binary: 0b" << binary << std::endl;

    // Use case 3: Unit conversion
    std::cout << "\n--- Unit Conversion ---" << std::endl;
    double meters = 1000.0;
    double kilometers = meters / 1000.0;
    double miles = meters / 1609.344;
    double feet = meters * 3.28084;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << meters << " meters =" << std::endl;
    std::cout << "  " << kilometers << " kilometers" << std::endl;
    std::cout << "  " << miles << " miles" << std::endl;
    std::cout << "  " << feet << " feet" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Conversion Utilities Examples" << std::endl;
    std::cout << "  atom::utils::conversion" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicConversions();
        demonstrateStringConversions();
        demonstrateHexConversions();
        demonstrateBinaryConversions();
        demonstrateBooleanConversions();
        demonstrateSafeConversions();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All conversion examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
