/**
 * @file anyutils_example.cpp
 * @brief Comprehensive examples for atom::utils anyutils utilities
 *
 * This example demonstrates std::any utility functions including:
 * - Container to string conversion
 * - Map to string conversion
 * - JSON/XML/YAML/TOML serialization
 * - Caching mechanisms
 * - Pretty printing options
 */

#include "atom/utils/core/anyutils.hpp"

#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// ============================================
// 1. Container to String Conversion
// ============================================
void demonstrateContainerToString() {
    printSection("1. Container to String Conversion");

    // Vector of integers
    std::cout << "--- Vector<int> ---" << std::endl;
    std::vector<int> intVec = {1, 2, 3, 4, 5};
    std::cout << "Vector: " << toString(intVec, false) << std::endl;
    std::cout << "Pretty: " << toString(intVec, true) << std::endl;

    // Vector of strings
    std::cout << "\n--- Vector<string> ---" << std::endl;
    std::vector<std::string> strVec = {"apple", "banana", "cherry"};
    std::cout << "Vector: " << toString(strVec, false) << std::endl;
    std::cout << "Pretty: " << toString(strVec, true) << std::endl;

    // Vector of doubles
    std::cout << "\n--- Vector<double> ---" << std::endl;
    std::vector<double> doubleVec = {1.1, 2.2, 3.3, 4.4};
    std::cout << "Vector: " << toString(doubleVec, false) << std::endl;

    // Empty vector
    std::cout << "\n--- Empty Vector ---" << std::endl;
    std::vector<int> emptyVec;
    std::cout << "Empty: " << toString(emptyVec, false) << std::endl;

    // Nested vectors
    std::cout << "\n--- Nested Vector ---" << std::endl;
    std::vector<std::vector<int>> nestedVec = {{1, 2}, {3, 4, 5}, {6}};
    std::cout << "Nested: " << toString(nestedVec, false) << std::endl;
    std::cout << "Pretty: " << toString(nestedVec, true) << std::endl;

    // Set
    std::cout << "\n--- Set<int> ---" << std::endl;
    std::set<int> intSet = {5, 2, 8, 1, 9};
    std::cout << "Set: " << toString(intSet, false) << std::endl;
}

// ============================================
// 2. Map to String Conversion
// ============================================
void demonstrateMapToString() {
    printSection("2. Map to String Conversion");

    // HashMap of string to int
    std::cout << "--- HashMap<string, int> ---" << std::endl;
    HashMap<std::string, int> strIntMap;
    strIntMap["one"] = 1;
    strIntMap["two"] = 2;
    strIntMap["three"] = 3;
    std::cout << "Map: " << toString(strIntMap, false) << std::endl;
    std::cout << "Pretty: " << toString(strIntMap, true) << std::endl;

    // HashMap of int to string
    std::cout << "\n--- HashMap<int, string> ---" << std::endl;
    HashMap<int, std::string> intStrMap;
    intStrMap[1] = "one";
    intStrMap[2] = "two";
    intStrMap[3] = "three";
    std::cout << "Map: " << toString(intStrMap, false) << std::endl;

    // Empty map
    std::cout << "\n--- Empty HashMap ---" << std::endl;
    HashMap<std::string, int> emptyMap;
    std::cout << "Empty: " << toString(emptyMap, false) << std::endl;

    // Nested map
    std::cout << "\n--- HashMap with Vector values ---" << std::endl;
    HashMap<std::string, std::vector<int>> mapWithVec;
    mapWithVec["evens"] = {2, 4, 6, 8};
    mapWithVec["odds"] = {1, 3, 5, 7};
    std::cout << "Map: " << toString(mapWithVec, false) << std::endl;
    std::cout << "Pretty: " << toString(mapWithVec, true) << std::endl;
}

// ============================================
// 3. JSON Serialization
// ============================================
void demonstrateJsonSerialization() {
    printSection("3. JSON Serialization");

    // Simple values
    std::cout << "--- Simple Values to JSON ---" << std::endl;
    std::cout << "Integer 42: " << toJson(42) << std::endl;
    std::cout << "Double 3.14: " << toJson(3.14) << std::endl;
    std::cout << "Bool true: " << toJson(true) << std::endl;
    std::cout << "String \"hello\": " << toJson(std::string("hello"))
              << std::endl;

    // Vector to JSON
    std::cout << "\n--- Vector to JSON ---" << std::endl;
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::cout << "Vector: " << toJson(numbers) << std::endl;

    std::vector<std::string> fruits = {"apple", "banana", "cherry"};
    std::cout << "Strings: " << toJson(fruits) << std::endl;

    // Map to JSON
    std::cout << "\n--- Map to JSON ---" << std::endl;
    HashMap<std::string, int> scores;
    scores["Alice"] = 95;
    scores["Bob"] = 87;
    scores["Charlie"] = 92;
    std::cout << "Scores: " << toJson(scores) << std::endl;
}

// ============================================
// 4. XML Serialization
// ============================================
void demonstrateXmlSerialization() {
    printSection("4. XML Serialization");

    // Simple values
    std::cout << "--- Simple Values to XML ---" << std::endl;
    std::cout << "Integer: " << toXml(42, "value") << std::endl;
    std::cout << "Double: " << toXml(3.14, "pi") << std::endl;
    std::cout << "String: " << toXml(std::string("hello"), "greeting")
              << std::endl;
    std::cout << "Bool: " << toXml(true, "enabled") << std::endl;

    // Vector to XML
    std::cout << "\n--- Vector to XML ---" << std::endl;
    std::vector<int> numbers = {1, 2, 3};
    std::cout << "Numbers: " << toXml(numbers, "numbers") << std::endl;

    std::vector<std::string> items = {"item1", "item2", "item3"};
    std::cout << "Items: " << toXml(items, "items") << std::endl;
}

// ============================================
// 5. YAML Serialization
// ============================================
void demonstrateYamlSerialization() {
    printSection("5. YAML Serialization");

    // Simple values
    std::cout << "--- Simple Values to YAML ---" << std::endl;
    std::cout << toYaml(42, "count") << std::endl;
    std::cout << toYaml(3.14159, "pi") << std::endl;
    std::cout << toYaml(std::string("Hello World"), "message") << std::endl;
    std::cout << toYaml(true, "enabled") << std::endl;

    // Vector to YAML
    std::cout << "\n--- Vector to YAML ---" << std::endl;
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::cout << toYaml(numbers, "numbers") << std::endl;

    std::vector<std::string> colors = {"red", "green", "blue"};
    std::cout << toYaml(colors, "colors") << std::endl;
}

// ============================================
// 6. TOML Serialization
// ============================================
void demonstrateTomlSerialization() {
    printSection("6. TOML Serialization");

    // Simple values
    std::cout << "--- Simple Values to TOML ---" << std::endl;
    std::cout << toToml(42, "port") << std::endl;
    std::cout << toToml(3.14, "version") << std::endl;
    std::cout << toToml(std::string("MyApp"), "name") << std::endl;
    std::cout << toToml(true, "debug") << std::endl;

    // Vector to TOML
    std::cout << "\n--- Vector to TOML ---" << std::endl;
    std::vector<int> ports = {8080, 8081, 8082};
    std::cout << toToml(ports, "ports") << std::endl;

    std::vector<std::string> hosts = {"localhost", "127.0.0.1", "0.0.0.0"};
    std::cout << toToml(hosts, "hosts") << std::endl;
}

// ============================================
// 7. Pretty Print Options
// ============================================
void demonstratePrettyPrint() {
    printSection("7. Pretty Print Options");

    // Large vector
    std::cout << "--- Large Vector ---" << std::endl;
    std::vector<int> largeVec;
    for (int i = 1; i <= 20; ++i) {
        largeVec.push_back(i);
    }

    std::cout << "Compact:" << std::endl;
    std::cout << toString(largeVec, false) << std::endl;

    std::cout << "\nPretty:" << std::endl;
    std::cout << toString(largeVec, true) << std::endl;

    // Complex nested structure
    std::cout << "\n--- Complex Nested Structure ---" << std::endl;
    HashMap<std::string, std::vector<std::string>> categories;
    categories["fruits"] = {"apple", "banana", "cherry"};
    categories["vegetables"] = {"carrot", "broccoli", "spinach"};
    categories["grains"] = {"rice", "wheat", "oats"};

    std::cout << "Compact:" << std::endl;
    std::cout << toString(categories, false) << std::endl;

    std::cout << "\nPretty:" << std::endl;
    std::cout << toString(categories, true) << std::endl;
}

// ============================================
// 8. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("8. Complex Use Cases");

    // Use case 1: Configuration serialization
    std::cout << "--- Configuration Serialization ---" << std::endl;

    HashMap<std::string, std::string> config;
    config["server.host"] = "localhost";
    config["server.port"] = "8080";
    config["database.url"] = "mysql://localhost:3306";
    config["logging.level"] = "debug";

    std::cout << "As string: " << toString(config, false) << std::endl;
    std::cout << "\nAs JSON: " << toJson(config) << std::endl;

    // Use case 2: Data export
    std::cout << "\n--- Data Export ---" << std::endl;

    struct Record {
        std::string name;
        int value;
    };

    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::vector<int> values = {100, 200, 300};

    std::cout << "Names: " << toString(names, false) << std::endl;
    std::cout << "Values: " << toString(values, false) << std::endl;

    // Use case 3: Debug output
    std::cout << "\n--- Debug Output ---" << std::endl;

    std::vector<std::vector<int>> matrix = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    std::cout << "Matrix (compact): " << toString(matrix, false) << std::endl;
    std::cout << "Matrix (pretty):" << std::endl;
    std::cout << toString(matrix, true) << std::endl;

    // Use case 4: API response formatting
    std::cout << "\n--- API Response Formatting ---" << std::endl;

    HashMap<std::string, int> apiResponse;
    apiResponse["status"] = 200;
    apiResponse["count"] = 42;
    apiResponse["page"] = 1;
    apiResponse["total_pages"] = 5;

    std::cout << "JSON Response: " << toJson(apiResponse) << std::endl;
    std::cout << "XML Response: " << toXml(apiResponse, "response")
              << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  AnyUtils Examples" << std::endl;
    std::cout << "  atom::utils::anyutils" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateContainerToString();
        demonstrateMapToString();
        demonstrateJsonSerialization();
        demonstrateXmlSerialization();
        demonstrateYamlSerialization();
        demonstrateTomlSerialization();
        demonstratePrettyPrint();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All AnyUtils examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
