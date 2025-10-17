/**
 * @file text_to_string_example.cpp
 * @brief Comprehensive examples demonstrating text to string conversion
 * utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::text/to_string.hpp:
 * - Basic type to string conversions
 * - Container to string conversions
 * - Custom type to string conversions
 * - Error handling and edge cases
 * - Performance considerations
 * - Formatting options and customization
 */

#include "atom/utils/text/to_string.hpp"

#include <chrono>
#include <complex>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to demonstrate conversion
template <typename T>
void demonstrateConversion(const T& value, const std::string& description) {
    std::cout << description << ": ";
    try {
        std::string result = toString(value);
        std::cout << "\"" << result << "\" (Success)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

// Custom class for demonstration
class Point {
public:
    double x, y;

    Point(double x = 0, double y = 0) : x(x), y(y) {}

    // Custom toString method that will be picked up by the conversion system
    std::string toString() const {
        return "Point(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

// Custom class without toString method
class Rectangle {
public:
    double width, height;

    Rectangle(double w = 0, double h = 0) : width(w), height(h) {}
};

// Specialization for Rectangle
namespace atom::utils {
template <>
auto toString(const Rectangle& rect) -> std::string {
    return "Rectangle(" + std::to_string(rect.width) + "x" +
           std::to_string(rect.height) + ")";
}
}  // namespace atom::utils

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Text ToString Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Basic Type Conversions
    // ============================
    printSection("1. Basic Type Conversions");

    printSubsection("Fundamental Types");

    demonstrateConversion(42, "int to string");
    demonstrateConversion(3.14159, "double to string");
    demonstrateConversion(true, "bool to string");
    demonstrateConversion('A', "char to string");
    demonstrateConversion(42L, "long to string");
    demonstrateConversion(42UL, "unsigned long to string");
    demonstrateConversion(3.14f, "float to string");

    printSubsection("String Types");

    std::string stdString = "Hello, World!";
    std::string_view stringView = "String View";
    const char* cString = "C String";

    demonstrateConversion(stdString, "std::string to string");
    demonstrateConversion(stringView, "std::string_view to string");
    demonstrateConversion(cString, "const char* to string");

    printSubsection("Special Values");

    demonstrateConversion(std::numeric_limits<double>::infinity(),
                          "infinity to string");
    demonstrateConversion(std::numeric_limits<double>::quiet_NaN(),
                          "NaN to string");
    demonstrateConversion(std::numeric_limits<int>::max(), "int max to string");
    demonstrateConversion(std::numeric_limits<int>::min(), "int min to string");

    // ============================
    // Example 2: Container Conversions
    // ============================
    printSection("2. Container Conversions");

    printSubsection("Vector Conversions");

    std::vector<int> intVector = {1, 2, 3, 4, 5};
    std::vector<std::string> stringVector = {"apple", "banana", "cherry"};
    std::vector<double> doubleVector = {1.1, 2.2, 3.3};

    demonstrateConversion(intVector, "vector<int> to string");
    demonstrateConversion(stringVector, "vector<string> to string");
    demonstrateConversion(doubleVector, "vector<double> to string");

    printSubsection("Map Conversions");

    std::map<std::string, int> stringIntMap = {
        {"one", 1}, {"two", 2}, {"three", 3}};
    std::map<int, std::string> intStringMap = {
        {1, "first"}, {2, "second"}, {3, "third"}};

    demonstrateConversion(stringIntMap, "map<string, int> to string");
    demonstrateConversion(intStringMap, "map<int, string> to string");

    printSubsection("Set Conversions");

    std::set<int> intSet = {5, 3, 8, 1, 9};
    std::set<std::string> stringSet = {"zebra", "apple", "banana"};

    demonstrateConversion(intSet, "set<int> to string");
    demonstrateConversion(stringSet, "set<string> to string");

    // ============================
    // Example 3: Optional and Variant
    // ============================
    printSection("3. Optional and Variant Conversions");

    printSubsection("Optional Types");

    std::optional<int> hasValue = 42;
    std::optional<int> noValue;
    std::optional<std::string> optionalString = "Hello";

    demonstrateConversion(hasValue, "optional<int> with value");
    demonstrateConversion(noValue, "optional<int> without value");
    demonstrateConversion(optionalString, "optional<string> with value");

    printSubsection("Variant Types");

    std::variant<int, std::string, double> variantInt = 42;
    std::variant<int, std::string, double> variantString = std::string("Hello");
    std::variant<int, std::string, double> variantDouble = 3.14;

    demonstrateConversion(variantInt, "variant holding int");
    demonstrateConversion(variantString, "variant holding string");
    demonstrateConversion(variantDouble, "variant holding double");

    // ============================
    // Example 4: Custom Types
    // ============================
    printSection("4. Custom Type Conversions");

    printSubsection("Class with toString Method");

    Point p1(3.5, 4.2);
    Point p2(-1.0, 2.5);

    demonstrateConversion(p1, "Point with toString method");
    demonstrateConversion(p2, "Another Point");

    printSubsection("Class with Specialized Template");

    Rectangle rect1(10.5, 7.3);
    Rectangle rect2(5.0, 5.0);

    demonstrateConversion(rect1, "Rectangle with specialized template");
    demonstrateConversion(rect2, "Square Rectangle");

    printSubsection("Container of Custom Types");

    std::vector<Point> points = {Point(1, 2), Point(3, 4), Point(5, 6)};
    std::vector<Rectangle> rectangles = {Rectangle(2, 3), Rectangle(4, 5)};

    demonstrateConversion(points, "vector<Point>");
    demonstrateConversion(rectangles, "vector<Rectangle>");

    // ============================
    // Example 5: Complex Types
    // ============================
    printSection("5. Complex and Advanced Types");

    printSubsection("Complex Numbers");

    std::complex<double> complexNum(3.0, 4.0);
    std::complex<float> complexFloat(1.5f, -2.5f);

    demonstrateConversion(complexNum, "complex<double>");
    demonstrateConversion(complexFloat, "complex<float>");

    printSubsection("Nested Containers");

    std::vector<std::vector<int>> nestedVector = {{1, 2}, {3, 4, 5}, {6}};
    std::map<std::string, std::vector<int>> mapOfVectors = {
        {"first", {1, 2, 3}}, {"second", {4, 5, 6}}};

    demonstrateConversion(nestedVector, "vector<vector<int>>");
    demonstrateConversion(mapOfVectors, "map<string, vector<int>>");

    // ============================
    // Example 6: Error Handling
    // ============================
    printSection("6. Error Handling");

    printSubsection("Edge Cases");

    std::vector<int> emptyVector;
    std::map<std::string, int> emptyMap;
    std::string emptyString = "";

    demonstrateConversion(emptyVector, "empty vector");
    demonstrateConversion(emptyMap, "empty map");
    demonstrateConversion(emptyString, "empty string");

    printSubsection("Large Data");

    std::vector<int> largeVector(1000);
    std::iota(largeVector.begin(), largeVector.end(), 1);

    std::cout << "Large vector (1000 elements): ";
    try {
        std::string result = toString(largeVector);
        std::cout << "Length: " << result.length() << " characters (Success)"
                  << std::endl;
        std::cout << "Preview: " << result.substr(0, 50) << "..." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    std::cout << "\nAll toString examples completed successfully!" << std::endl;

    return 0;
}
