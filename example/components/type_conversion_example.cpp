/*
 * type_conversion_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Type Conversion System Example
Demonstrates type conversion capabilities using the component system's
built-in type handling through std::any and component variables.
Shows conversion between different types, validation, and error handling.

**************************************************/

#include <any>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"

// Note: Registry and Component are in the global namespace

/**
 * @brief Component demonstrating type conversion capabilities
 */
class TypeConversionComponent : public Component {
public:
    explicit TypeConversionComponent(const std::string& name)
        : Component(name) {
        std::cout << "TypeConversionComponent '" << name << "' created"
                  << std::endl;
        setupVariables();
        setupCommands();
    }

private:
    void setupVariables() {
        // Add variables of different types
        addVariable<int>("int_value", 42);
        addVariable<double>("double_value", 3.14159);
        addVariable<std::string>("string_value", "Hello");
        addVariable<bool>("bool_value", true);

        // Add vector variables
        addVariable<std::vector<int>>("int_vector",
                                      std::vector<int>{1, 2, 3, 4, 5});
        addVariable<std::vector<double>>("double_vector",
                                         std::vector<double>{1.1, 2.2, 3.3});
    }

    void setupCommands() {
        // String to numeric conversions
        def("stringToInt", [](const std::string& str) -> int {
            try {
                return std::stoi(str);
            } catch (const std::exception& e) {
                std::cerr << "Error converting string to int: " << e.what()
                          << std::endl;
                return 0;
            }
        });

        def("stringToDouble", [](const std::string& str) -> double {
            try {
                return std::stod(str);
            } catch (const std::exception& e) {
                std::cerr << "Error converting string to double: " << e.what()
                          << std::endl;
                return 0.0;
            }
        });

        def("stringToBool", [](const std::string& str) -> bool {
            std::string lower = str;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           ::tolower);
            return lower == "true" || lower == "1" || lower == "yes";
        });

        // Numeric to string conversions
        def("intToString",
            [](int value) -> std::string { return std::to_string(value); });

        def("doubleToString", [](double value) -> std::string {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << value;
            return oss.str();
        });

        def("boolToString",
            [](bool value) -> std::string { return value ? "true" : "false"; });

        // Type casting conversions
        def("intToDouble",
            [](int value) -> double { return static_cast<double>(value); });

        def("doubleToInt",
            [](double value) -> int { return static_cast<int>(value); });

        // Vector conversions
        def("intVectorToDoubleVector", [this]() -> std::vector<double> {
            auto intVec = getVariable<std::vector<int>>("int_vector");
            if (intVec) {
                const auto& vec = intVec->get();
                std::vector<double> result;
                result.reserve(vec.size());
                for (int val : vec) {
                    result.push_back(static_cast<double>(val));
                }
                return result;
            }
            return {};
        });

        def("doubleVectorToIntVector", [this]() -> std::vector<int> {
            auto doubleVec = getVariable<std::vector<double>>("double_vector");
            if (doubleVec) {
                const auto& vec = doubleVec->get();
                std::vector<int> result;
                result.reserve(vec.size());
                for (double val : vec) {
                    result.push_back(static_cast<int>(val));
                }
                return result;
            }
            return {};
        });

        // JSON-like string conversion
        def("vectorToString", [this]() -> std::string {
            auto intVec = getVariable<std::vector<int>>("int_vector");
            if (intVec) {
                const auto& vec = intVec->get();
                std::ostringstream oss;
                oss << "[";
                for (size_t i = 0; i < vec.size(); ++i) {
                    oss << vec[i];
                    if (i < vec.size() - 1)
                        oss << ", ";
                }
                oss << "]";
                return oss.str();
            }
            return "[]";
        });

        // Safe conversion with validation
        def("safeStringToInt",
            [](const std::string& str) -> std::pair<bool, int> {
                try {
                    size_t pos;
                    int value = std::stoi(str, &pos);
                    // Check if entire string was converted
                    bool success = (pos == str.length());
                    return {success, value};
                } catch (const std::exception&) {
                    return {false, 0};
                }
            });
    }
};

int main() {
    std::cout << "=== Atom Component Type Conversion Examples ===" << std::endl;

    try {
        auto& registry = Registry::instance();

        // Create type conversion component
        auto component = registry.createComponent<TypeConversionComponent>(
            "TypeConversionDemo");

        std::cout << "\n1. String to Numeric Conversions" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Test string to int
        std::vector<std::any> stringToIntArgs = {std::string("42")};
        auto intResult = std::any_cast<int>(
            component->runCommand("stringToInt", stringToIntArgs));
        std::cout << "String '42' to int: " << intResult << std::endl;

        // Test string to double
        std::vector<std::any> stringToDoubleArgs = {std::string("3.14159")};
        auto doubleResult = std::any_cast<double>(
            component->runCommand("stringToDouble", stringToDoubleArgs));
        std::cout << "String '3.14159' to double: " << doubleResult
                  << std::endl;

        // Test string to bool
        std::vector<std::any> stringToBoolArgs = {std::string("true")};
        auto boolResult = std::any_cast<bool>(
            component->runCommand("stringToBool", stringToBoolArgs));
        std::cout << "String 'true' to bool: "
                  << (boolResult ? "true" : "false") << std::endl;

        std::cout << "\n2. Numeric to String Conversions" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Test int to string
        std::vector<std::any> intToStringArgs = {100};
        auto intStr = std::any_cast<std::string>(
            component->runCommand("intToString", intToStringArgs));
        std::cout << "Int 100 to string: '" << intStr << "'" << std::endl;

        // Test double to string
        std::vector<std::any> doubleToStringArgs = {2.71828};
        auto doubleStr = std::any_cast<std::string>(
            component->runCommand("doubleToString", doubleToStringArgs));
        std::cout << "Double 2.71828 to string: '" << doubleStr << "'"
                  << std::endl;

        // Test bool to string
        std::vector<std::any> boolToStringArgs = {false};
        auto boolStr = std::any_cast<std::string>(
            component->runCommand("boolToString", boolToStringArgs));
        std::cout << "Bool false to string: '" << boolStr << "'" << std::endl;

        std::cout << "\n3. Type Casting Conversions" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Test int to double
        std::vector<std::any> intToDoubleArgs = {42};
        auto intToDouble = std::any_cast<double>(
            component->runCommand("intToDouble", intToDoubleArgs));
        std::cout << "Int 42 to double: " << intToDouble << std::endl;

        // Test double to int
        std::vector<std::any> doubleToIntArgs = {3.14159};
        auto doubleToInt = std::any_cast<int>(
            component->runCommand("doubleToInt", doubleToIntArgs));
        std::cout << "Double 3.14159 to int: " << doubleToInt << std::endl;

        std::cout << "\n4. Vector Conversions" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Test int vector to double vector
        auto doubleVec = std::any_cast<std::vector<double>>(
            component->runCommand("intVectorToDoubleVector", {}));
        std::cout << "Int vector to double vector: [";
        for (size_t i = 0; i < doubleVec.size(); ++i) {
            std::cout << doubleVec[i];
            if (i < doubleVec.size() - 1)
                std::cout << ", ";
        }
        std::cout << "]" << std::endl;

        // Test double vector to int vector
        auto intVec = std::any_cast<std::vector<int>>(
            component->runCommand("doubleVectorToIntVector", {}));
        std::cout << "Double vector to int vector: [";
        for (size_t i = 0; i < intVec.size(); ++i) {
            std::cout << intVec[i];
            if (i < intVec.size() - 1)
                std::cout << ", ";
        }
        std::cout << "]" << std::endl;

        // Test vector to string
        auto vecStr = std::any_cast<std::string>(
            component->runCommand("vectorToString", {}));
        std::cout << "Vector to string: " << vecStr << std::endl;

        std::cout << "\n5. Safe Conversion with Validation" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Test safe string to int with valid input
        std::vector<std::any> safeArgs1 = {std::string("123")};
        auto safeResult1 = std::any_cast<std::pair<bool, int>>(
            component->runCommand("safeStringToInt", safeArgs1));
        std::cout << "Safe convert '123': success="
                  << (safeResult1.first ? "true" : "false")
                  << ", value=" << safeResult1.second << std::endl;

        // Test safe string to int with invalid input
        std::vector<std::any> safeArgs2 = {std::string("abc")};
        auto safeResult2 = std::any_cast<std::pair<bool, int>>(
            component->runCommand("safeStringToInt", safeArgs2));
        std::cout << "Safe convert 'abc': success="
                  << (safeResult2.first ? "true" : "false")
                  << ", value=" << safeResult2.second << std::endl;

        // Test safe string to int with partial number
        std::vector<std::any> safeArgs3 = {std::string("123abc")};
        auto safeResult3 = std::any_cast<std::pair<bool, int>>(
            component->runCommand("safeStringToInt", safeArgs3));
        std::cout << "Safe convert '123abc': success="
                  << (safeResult3.first ? "true" : "false")
                  << ", value=" << safeResult3.second << std::endl;

        std::cout << "\n6. Component Variable Access" << std::endl;
        std::cout << "-----------------------------------" << std::endl;

        // Access and display component variables
        auto intVar = component->getVariable<int>("int_value");
        auto doubleVar = component->getVariable<double>("double_value");
        auto stringVar = component->getVariable<std::string>("string_value");
        auto boolVar = component->getVariable<bool>("bool_value");

        if (intVar)
            std::cout << "int_value: " << intVar->get() << std::endl;
        if (doubleVar)
            std::cout << "double_value: " << doubleVar->get() << std::endl;
        if (stringVar)
            std::cout << "string_value: " << stringVar->get() << std::endl;
        if (boolVar)
            std::cout << "bool_value: " << (boolVar->get() ? "true" : "false")
                      << std::endl;

        std::cout
            << "\n=== All Type Conversion Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in type conversion examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
