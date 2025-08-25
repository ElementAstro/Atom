/*
 * type_conversion_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Type Conversion System Example
Demonstrates automatic type conversion, STL containers, custom types,
and comprehensive type conversion features with the component system.

**************************************************/

#include <chrono>
#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/type_conversion.hpp"

using namespace atom::components;

/**
 * @brief Custom data structure for type conversion testing
 */
struct PlayerData {
    int id;
    std::string name;
    double score;
    bool active;

    PlayerData() : id(0), name(""), score(0.0), active(false) {}
    PlayerData(int i, const std::string& n, double s, bool a)
        : id(i), name(n), score(s), active(a) {}

    std::string toString() const {
        return "PlayerData{id=" + std::to_string(id) + ", name='" + name + "'" +
               ", score=" + std::to_string(score) +
               ", active=" + (active ? "true" : "false") + "}";
    }

    bool operator==(const PlayerData& other) const {
        return id == other.id && name == other.name && score == other.score &&
               active == other.active;
    }
};

/**
 * @brief Custom point class for geometric operations
 */
class Point2D {
public:
    Point2D() : x_(0.0), y_(0.0) {}
    Point2D(double x, double y) : x_(x), y_(y) {}

    double getX() const { return x_; }
    double getY() const { return y_; }
    void setX(double x) { x_ = x; }
    void setY(double y) { y_ = y; }

    double distance(const Point2D& other) const {
        double dx = x_ - other.x_;
        double dy = y_ - other.y_;
        return std::sqrt(dx * dx + dy * dy);
    }

    Point2D add(const Point2D& other) const {
        return Point2D(x_ + other.x_, y_ + other.y_);
    }

    std::string toString() const {
        return "Point2D(" + std::to_string(x_) + ", " + std::to_string(y_) +
               ")";
    }

    bool operator==(const Point2D& other) const {
        const double epsilon = 1e-9;
        return std::abs(x_ - other.x_) < epsilon &&
               std::abs(y_ - other.y_) < epsilon;
    }

private:
    double x_, y_;
};

/**
 * @brief Component demonstrating type conversion features
 */
class TypeConversionComponent : public Component {
public:
    explicit TypeConversionComponent(const std::string& name)
        : Component(name) {
        std::cout << "TypeConversionComponent '" << name << "' created"
                  << std::endl;

        setupTypeConverters();
        setupVariables();
        setupCommands();
    }

private:
    void setupTypeConverters() {
        auto& converter = TypeConverter::instance();

        // Register PlayerData conversions
        converter.registerConverter<PlayerData, std::string>(
            [](const PlayerData& data) -> std::string {
                return data.toString();
            });

        converter.registerConverter<std::string, PlayerData>(
            [](const std::string& str) -> PlayerData {
                // Simple parsing (in real implementation, use JSON or proper
                // parser)
                PlayerData data;
                data.name = str;
                data.id = static_cast<int>(str.length());
                data.score = static_cast<double>(str.length()) * 10.0;
                data.active = !str.empty();
                return data;
            });

        // Register Point2D conversions
        converter.registerConverter<Point2D, std::string>(
            [](const Point2D& point) -> std::string {
                return point.toString();
            });

        converter.registerConverter<std::string, Point2D>(
            [](const std::string& str) -> Point2D {
                // Simple parsing: "Point2D(x, y)"
                size_t start = str.find('(');
                size_t comma = str.find(',');
                size_t end = str.find(')');

                if (start != std::string::npos && comma != std::string::npos &&
                    end != std::string::npos) {
                    double x =
                        std::stod(str.substr(start + 1, comma - start - 1));
                    double y =
                        std::stod(str.substr(comma + 1, end - comma - 1));
                    return Point2D(x, y);
                }
                return Point2D();
            });

        // Register vector conversions
        converter.registerConverter<std::vector<int>, std::string>(
            [](const std::vector<int>& vec) -> std::string {
                std::string result = "[";
                for (size_t i = 0; i < vec.size(); ++i) {
                    result += std::to_string(vec[i]);
                    if (i < vec.size() - 1)
                        result += ", ";
                }
                result += "]";
                return result;
            });

        converter.registerConverter<std::string, std::vector<int>>(
            [](const std::string& str) -> std::vector<int> {
                std::vector<int> result;
                // Simple parsing: "[1, 2, 3]"
                size_t start = str.find('[');
                size_t end = str.find(']');

                if (start != std::string::npos && end != std::string::npos) {
                    std::string content =
                        str.substr(start + 1, end - start - 1);
                    std::stringstream ss(content);
                    std::string item;

                    while (std::getline(ss, item, ',')) {
                        // Trim whitespace
                        item.erase(0, item.find_first_not_of(" \t"));
                        item.erase(item.find_last_not_of(" \t") + 1);

                        if (!item.empty()) {
                            result.push_back(std::stoi(item));
                        }
                    }
                }
                return result;
            });

        std::cout << "Type converters registered" << std::endl;
    }

    void setupVariables() {
        // Basic types
        addVariable<int>("integer_value", 42);
        addVariable<double>("double_value", 3.14159);
        addVariable<std::string>("string_value", "Hello World");
        addVariable<bool>("boolean_value", true);

        // STL containers
        addVariable<std::vector<int>>("int_vector", {1, 2, 3, 4, 5});
        addVariable<std::vector<std::string>>("string_vector",
                                              {"apple", "banana", "cherry"});
        addVariable<std::map<std::string, int>>(
            "string_int_map", {{"one", 1}, {"two", 2}, {"three", 3}});
        addVariable<std::set<double>>("double_set", {1.1, 2.2, 3.3, 4.4});

        // Custom types
        PlayerData playerData(123, "TypeConversionHero", 9999.5, true);
        addVariable<PlayerData>("player_data", playerData);

        Point2D point(10.5, 20.3);
        addVariable<Point2D>("point_2d", point);

        // Complex types
        std::complex<double> complexNum(3.0, 4.0);
        addVariable<std::complex<double>>("complex_number", complexNum);

        // Time-related types
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch())
                             .count();
        addVariable<int64_t>("timestamp", timestamp);
    }

    void setupCommands() {
        auto& converter = TypeConverter::instance();

        // Type conversion commands
        def("convertToString",
            [this, &converter](const std::string& varName) -> std::string {
                try {
                    // Get variable and convert to string
                    if (auto intVar = getVariable<int>(varName)) {
                        return converter.convert<int, std::string>(
                            intVar->get());
                    } else if (auto doubleVar = getVariable<double>(varName)) {
                        return converter.convert<double, std::string>(
                            doubleVar->get());
                    } else if (auto boolVar = getVariable<bool>(varName)) {
                        return converter.convert<bool, std::string>(
                            boolVar->get());
                    } else if (auto playerVar =
                                   getVariable<PlayerData>(varName)) {
                        return converter.convert<PlayerData, std::string>(
                            playerVar->get());
                    } else if (auto pointVar = getVariable<Point2D>(varName)) {
                        return converter.convert<Point2D, std::string>(
                            pointVar->get());
                    } else if (auto vecVar =
                                   getVariable<std::vector<int>>(varName)) {
                        return converter.convert<std::vector<int>, std::string>(
                            vecVar->get());
                    } else {
                        return "Variable not found or unsupported type: " +
                               varName;
                    }
                } catch (const std::exception& e) {
                    return "Conversion error: " + std::string(e.what());
                }
            });

        def("convertFromString",
            [this, &converter](const std::string& varName,
                               const std::string& value,
                               const std::string& targetType) -> bool {
                try {
                    if (targetType == "int") {
                        int converted =
                            converter.convert<std::string, int>(value);
                        setValue(varName, converted);
                    } else if (targetType == "double") {
                        double converted =
                            converter.convert<std::string, double>(value);
                        setValue(varName, converted);
                    } else if (targetType == "bool") {
                        bool converted =
                            converter.convert<std::string, bool>(value);
                        setValue(varName, converted);
                    } else if (targetType == "PlayerData") {
                        PlayerData converted =
                            converter.convert<std::string, PlayerData>(value);
                        setValue(varName, converted);
                    } else if (targetType == "Point2D") {
                        Point2D converted =
                            converter.convert<std::string, Point2D>(value);
                        setValue(varName, converted);
                    } else if (targetType == "vector<int>") {
                        std::vector<int> converted =
                            converter.convert<std::string, std::vector<int>>(
                                value);
                        setValue(varName, converted);
                    } else {
                        std::cout << "  [" << getName()
                                  << "] Unsupported target type: " << targetType
                                  << std::endl;
                        return false;
                    }

                    std::cout << "  [" << getName() << "] Converted '" << value
                              << "' to " << targetType << " and stored in "
                              << varName << std::endl;
                    return true;
                } catch (const std::exception& e) {
                    std::cout << "  [" << getName()
                              << "] Conversion error: " << e.what()
                              << std::endl;
                    return false;
                }
            });

        def("testAutomaticConversion", [this, &converter]() {
            std::cout << "  [" << getName()
                      << "] Testing automatic type conversions..." << std::endl;

            // Test numeric conversions
            int intVal = 42;
            double doubleFromInt = converter.convert<int, double>(intVal);
            std::cout << "    int " << intVal << " -> double " << doubleFromInt
                      << std::endl;

            double doubleVal = 3.14159;
            int intFromDouble = converter.convert<double, int>(doubleVal);
            std::cout << "    double " << doubleVal << " -> int "
                      << intFromDouble << std::endl;

            // Test string conversions
            std::string strVal = "123";
            int intFromStr = converter.convert<std::string, int>(strVal);
            std::cout << "    string '" << strVal << "' -> int " << intFromStr
                      << std::endl;

            bool boolVal = true;
            std::string strFromBool =
                converter.convert<bool, std::string>(boolVal);
            std::cout << "    bool " << boolVal << " -> string '" << strFromBool
                      << "'" << std::endl;
        });

        def("testContainerConversions", [this, &converter]() {
            std::cout << "  [" << getName()
                      << "] Testing container conversions..." << std::endl;

            // Test vector conversion
            auto vecVar = getVariable<std::vector<int>>("int_vector");
            if (vecVar) {
                std::string vecStr =
                    converter.convert<std::vector<int>, std::string>(
                        vecVar->get());
                std::cout << "    vector<int> -> string: " << vecStr
                          << std::endl;

                std::vector<int> vecBack =
                    converter.convert<std::string, std::vector<int>>(vecStr);
                std::cout << "    string -> vector<int>: [";
                for (size_t i = 0; i < vecBack.size(); ++i) {
                    std::cout << vecBack[i];
                    if (i < vecBack.size() - 1)
                        std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }
        });

        def("testCustomTypeConversions", [this, &converter]() {
            std::cout << "  [" << getName()
                      << "] Testing custom type conversions..." << std::endl;

            // Test PlayerData conversion
            auto playerVar = getVariable<PlayerData>("player_data");
            if (playerVar) {
                std::string playerStr =
                    converter.convert<PlayerData, std::string>(
                        playerVar->get());
                std::cout << "    PlayerData -> string: " << playerStr
                          << std::endl;

                PlayerData playerBack =
                    converter.convert<std::string, PlayerData>("TestPlayer");
                std::cout << "    string -> PlayerData: "
                          << playerBack.toString() << std::endl;
            }

            // Test Point2D conversion
            auto pointVar = getVariable<Point2D>("point_2d");
            if (pointVar) {
                std::string pointStr =
                    converter.convert<Point2D, std::string>(pointVar->get());
                std::cout << "    Point2D -> string: " << pointStr << std::endl;

                Point2D pointBack =
                    converter.convert<std::string, Point2D>(pointStr);
                std::cout << "    string -> Point2D: " << pointBack.toString()
                          << std::endl;
            }
        });

        def("getConversionStatistics", [&converter]() -> std::string {
            auto stats = converter.getStatistics();
            return "Conversions: " + std::to_string(stats.totalConversions) +
                   ", Successful: " +
                   std::to_string(stats.successfulConversions) +
                   ", Failed: " + std::to_string(stats.failedConversions) +
                   ", Registered: " +
                   std::to_string(stats.registeredConverters);
        });
    }
};

void demonstrateBasicTypeConversion() {
    std::cout << "\n=== Basic Type Conversion Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& converter = TypeConverter::instance();

    std::cout << "\n1. Creating component with type conversion..." << std::endl;
    auto component =
        registry.createComponent<TypeConversionComponent>("TypeConversionDemo");

    std::cout << "\n2. Testing basic type conversions..." << std::endl;

    // Test automatic conversions
    component->executeCommand("testAutomaticConversion", {});

    // Test manual conversions
    std::cout << "\n--- Manual Conversions ---" << std::endl;

    auto intStr =
        component->executeCommand("convertToString", {"integer_value"});
    std::cout << "integer_value as string: " << intStr << std::endl;

    auto doubleStr =
        component->executeCommand("convertToString", {"double_value"});
    std::cout << "double_value as string: " << doubleStr << std::endl;

    auto boolStr =
        component->executeCommand("convertToString", {"boolean_value"});
    std::cout << "boolean_value as string: " << boolStr << std::endl;

    // Test conversion back
    bool success1 = std::stoi(component->executeCommand(
        "convertFromString", {"new_int", "999", "int"}));
    std::cout << "String to int conversion success: "
              << (success1 ? "true" : "false") << std::endl;

    bool success2 = std::stoi(component->executeCommand(
        "convertFromString", {"new_double", "2.71828", "double"}));
    std::cout << "String to double conversion success: "
              << (success2 ? "true" : "false") << std::endl;
}

void demonstrateContainerConversions() {
    std::cout << "\n=== Container Conversions Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("TypeConversionDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n3. Testing container conversions..." << std::endl;

    component->executeCommand("testContainerConversions", {});

    // Test vector conversion
    auto vecStr = component->executeCommand("convertToString", {"int_vector"});
    std::cout << "int_vector as string: " << vecStr << std::endl;

    // Convert string back to vector
    bool vecSuccess = std::stoi(component->executeCommand(
        "convertFromString",
        {"new_vector", "[10, 20, 30, 40]", "vector<int>"}));
    std::cout << "String to vector conversion success: "
              << (vecSuccess ? "true" : "false") << std::endl;
}

void demonstrateCustomTypeConversions() {
    std::cout << "\n=== Custom Type Conversions Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("TypeConversionDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n4. Testing custom type conversions..." << std::endl;

    component->executeCommand("testCustomTypeConversions", {});

    // Test PlayerData conversion
    auto playerStr =
        component->executeCommand("convertToString", {"player_data"});
    std::cout << "player_data as string: " << playerStr << std::endl;

    // Test Point2D conversion
    auto pointStr = component->executeCommand("convertToString", {"point_2d"});
    std::cout << "point_2d as string: " << pointStr << std::endl;

    // Convert custom types from strings
    bool playerSuccess = std::stoi(component->executeCommand(
        "convertFromString", {"new_player", "CustomPlayer", "PlayerData"}));
    std::cout << "String to PlayerData conversion success: "
              << (playerSuccess ? "true" : "false") << std::endl;

    bool pointSuccess = std::stoi(component->executeCommand(
        "convertFromString", {"new_point", "Point2D(5.5, 7.7)", "Point2D"}));
    std::cout << "String to Point2D conversion success: "
              << (pointSuccess ? "true" : "false") << std::endl;
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& converter = TypeConverter::instance();
    auto component = registry.getComponent("TypeConversionDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n5. Testing conversion error handling..." << std::endl;

    // Test invalid string to int conversion
    std::cout << "\n--- Invalid Conversions ---" << std::endl;
    try {
        int result = converter.convert<std::string, int>("not_a_number");
        std::cout << "Unexpected success: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error (string to int): " << e.what()
                  << std::endl;
    }

    // Test unsupported conversion
    try {
        // This should fail if no converter is registered
        auto result = converter.convert<std::complex<double>, std::string>(
            std::complex<double>(1, 2));
        std::cout << "Complex to string: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error (complex to string): " << e.what()
                  << std::endl;
    }

    // Test component conversion errors
    bool invalidSuccess = std::stoi(component->executeCommand(
        "convertFromString", {"invalid", "invalid_value", "int"}));
    std::cout << "Invalid string to int success: "
              << (invalidSuccess ? "true" : "false") << std::endl;

    auto invalidResult =
        component->executeCommand("convertToString", {"nonexistent_variable"});
    std::cout << "Nonexistent variable conversion: " << invalidResult
              << std::endl;
}

void demonstratePerformanceAnalysis() {
    std::cout << "\n=== Performance Analysis Demo ===" << std::endl;

    auto& converter = TypeConverter::instance();

    std::cout << "\n6. Analyzing conversion performance..." << std::endl;

    const int NUM_CONVERSIONS = 10000;

    // Test int to string conversion performance
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CONVERSIONS; ++i) {
        std::string result = converter.convert<int, std::string>(i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Int to string conversions (" << NUM_CONVERSIONS
              << "): " << duration.count() << " μs" << std::endl;
    std::cout << "Average per conversion: "
              << (duration.count() / NUM_CONVERSIONS) << " μs" << std::endl;

    // Test string to int conversion performance
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CONVERSIONS; ++i) {
        int result = converter.convert<std::string, int>(std::to_string(i));
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "String to int conversions (" << NUM_CONVERSIONS
              << "): " << duration.count() << " μs" << std::endl;
    std::cout << "Average per conversion: "
              << (duration.count() / NUM_CONVERSIONS) << " μs" << std::endl;

    // Test custom type conversion performance
    PlayerData testPlayer(1, "TestPlayer", 100.0, true);

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        std::string result =
            converter.convert<PlayerData, std::string>(testPlayer);
        PlayerData back = converter.convert<std::string, PlayerData>(result);
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Custom type round-trip conversions (1000): "
              << duration.count() << " μs" << std::endl;
    std::cout << "Average per round-trip: " << (duration.count() / 1000)
              << " μs" << std::endl;
}

void demonstrateConversionStatistics() {
    std::cout << "\n=== Conversion Statistics Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("TypeConversionDemo");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n7. Type conversion system statistics..." << std::endl;

    auto stats = component->executeCommand("getConversionStatistics", {});
    std::cout << "Statistics: " << stats << std::endl;

    auto& converter = TypeConverter::instance();
    auto detailedStats = converter.getStatistics();

    std::cout << "\nDetailed Statistics:" << std::endl;
    std::cout << "  Total conversions: " << detailedStats.totalConversions
              << std::endl;
    std::cout << "  Successful conversions: "
              << detailedStats.successfulConversions << std::endl;
    std::cout << "  Failed conversions: " << detailedStats.failedConversions
              << std::endl;
    std::cout << "  Registered converters: "
              << detailedStats.registeredConverters << std::endl;
    std::cout << "  Success rate: "
              << (detailedStats.totalConversions > 0
                      ? (100.0 * detailedStats.successfulConversions /
                         detailedStats.totalConversions)
                      : 0.0)
              << "%" << std::endl;

    // List registered converters
    auto converterList = converter.getRegisteredConverters();
    std::cout << "\nRegistered Converters:" << std::endl;
    for (const auto& conv : converterList) {
        std::cout << "  " << conv.first << " -> " << conv.second << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Component Type Conversion Examples ===" << std::endl;

    try {
        demonstrateBasicTypeConversion();
        demonstrateContainerConversions();
        demonstrateCustomTypeConversions();
        demonstrateErrorHandling();
        demonstratePerformanceAnalysis();
        demonstrateConversionStatistics();

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
