/**
 * @file type_convert_example.cpp
 * @brief Comprehensive examples demonstrating general type conversion utilities
 *
 * This example demonstrates type conversion patterns and utilities:
 * - Basic type conversions between fundamental types
 * - String to numeric conversions with error handling
 * - Container type conversions
 * - Custom type conversion patterns
 * - Safe conversion with bounds checking
 * - Conversion error handling and recovery
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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

// Safe conversion template with bounds checking
template <typename To, typename From>
std::optional<To> safeConvert(const From& value) {
    if constexpr (std::is_same_v<From, To>) {
        return value;
    } else if constexpr (std::is_arithmetic_v<From> &&
                         std::is_arithmetic_v<To>) {
        // Check for overflow/underflow
        if constexpr (std::is_integral_v<From> && std::is_integral_v<To>) {
            if (value > std::numeric_limits<To>::max() ||
                value < std::numeric_limits<To>::min()) {
                return std::nullopt;
            }
        }
        return static_cast<To>(value);
    } else {
        return std::nullopt;
    }
}

// String to numeric conversion with error handling
template <typename T>
std::optional<T> stringToNumeric(const std::string& str) {
    try {
        if constexpr (std::is_same_v<T, int>) {
            size_t pos;
            int result = std::stoi(str, &pos);
            return (pos == str.length()) ? std::optional<T>(result)
                                         : std::nullopt;
        } else if constexpr (std::is_same_v<T, long>) {
            size_t pos;
            long result = std::stol(str, &pos);
            return (pos == str.length()) ? std::optional<T>(result)
                                         : std::nullopt;
        } else if constexpr (std::is_same_v<T, float>) {
            size_t pos;
            float result = std::stof(str, &pos);
            return (pos == str.length()) ? std::optional<T>(result)
                                         : std::nullopt;
        } else if constexpr (std::is_same_v<T, double>) {
            size_t pos;
            double result = std::stod(str, &pos);
            return (pos == str.length()) ? std::optional<T>(result)
                                         : std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

// Custom Point class for conversion demonstration
class Point {
public:
    double x, y;

    Point(double x = 0, double y = 0) : x(x), y(y) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "Point(" << std::fixed << std::setprecision(2) << x << ", " << y
            << ")";
        return oss.str();
    }

    static std::optional<Point> fromString(const std::string& str) {
        // Parse "Point(x, y)" format
        if (str.substr(0, 6) != "Point(")
            return std::nullopt;

        size_t start = 6;
        size_t comma = str.find(',', start);
        size_t end = str.find(')', comma);

        if (comma == std::string::npos || end == std::string::npos) {
            return std::nullopt;
        }

        try {
            double x = std::stod(str.substr(start, comma - start));
            double y = std::stod(str.substr(comma + 1, end - comma - 1));
            return Point(x, y);
        } catch (...) {
            return std::nullopt;
        }
    }

    bool operator==(const Point& other) const {
        return std::abs(x - other.x) < 1e-6 && std::abs(y - other.y) < 1e-6;
    }
};

// Container conversion utilities
template <typename ToContainer, typename FromContainer>
ToContainer convertContainer(const FromContainer& from) {
    ToContainer result;
    if constexpr (requires { result.reserve(from.size()); }) {
        result.reserve(from.size());
    }

    for (const auto& item : from) {
        if constexpr (requires { result.push_back(item); }) {
            result.push_back(
                static_cast<typename ToContainer::value_type>(item));
        } else if constexpr (requires { result.insert(item); }) {
            result.insert(static_cast<typename ToContainer::value_type>(item));
        }
    }
    return result;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Type Conversion Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Safe Numeric Conversions
    // ============================
    printSection("1. Safe Numeric Conversions");

    printSubsection("Integer Conversions with Bounds Checking");

    std::vector<long> testValues = {42L, 1000000L, -50L,
                                    std::numeric_limits<long>::max(),
                                    std::numeric_limits<long>::min()};

    for (long val : testValues) {
        auto result = safeConvert<int>(val);
        std::cout << "long(" << val << ") to int: ";
        if (result) {
            std::cout << *result << " (Success)" << std::endl;
        } else {
            std::cout << "Out of range" << std::endl;
        }
    }

    printSubsection("Floating Point Conversions");

    std::vector<double> floatValues = {3.14159, 1e10, -2.5, 0.0,
                                       std::numeric_limits<double>::infinity()};

    for (double val : floatValues) {
        auto result = safeConvert<float>(val);
        std::cout << "double(" << std::fixed << std::setprecision(5) << val
                  << ") to float: ";
        if (result) {
            std::cout << *result << " (Success)" << std::endl;
        } else {
            std::cout << "Conversion failed" << std::endl;
        }
    }

    // ============================
    // Example 2: String to Numeric Conversions
    // ============================
    printSection("2. String to Numeric Conversions");

    printSubsection("Integer Parsing");

    std::vector<std::string> intStrings = {
        "42", "-123", "0", "999999999999", "42abc", "", "not_a_number"};

    for (const auto& str : intStrings) {
        auto result = stringToNumeric<int>(str);
        std::cout << "\"" << str << "\" to int: ";
        if (result) {
            std::cout << *result << " (Success)" << std::endl;
        } else {
            std::cout << "Failed" << std::endl;
        }
    }

    printSubsection("Floating Point Parsing");

    std::vector<std::string> floatStrings = {
        "3.14159", "-2.5", "1e-6", "1.23e10", "inf", "nan", "3.14abc"};

    for (const auto& str : floatStrings) {
        auto result = stringToNumeric<double>(str);
        std::cout << "\"" << str << "\" to double: ";
        if (result) {
            std::cout << std::fixed << std::setprecision(6) << *result
                      << " (Success)" << std::endl;
        } else {
            std::cout << "Failed" << std::endl;
        }
    }

    // ============================
    // Example 3: Container Conversions
    // ============================
    printSection("3. Container Conversions");

    printSubsection("Vector Type Conversions");

    std::vector<int> intVector = {1, 2, 3, 4, 5};
    auto doubleVector = convertContainer<std::vector<double>>(intVector);

    std::cout << "int vector: [";
    for (size_t i = 0; i < intVector.size(); ++i) {
        std::cout << intVector[i];
        if (i < intVector.size() - 1)
            std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    std::cout << "double vector: [";
    for (size_t i = 0; i < doubleVector.size(); ++i) {
        std::cout << std::fixed << std::setprecision(1) << doubleVector[i];
        if (i < doubleVector.size() - 1)
            std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    printSubsection("String Vector to Numeric Vector");

    std::vector<std::string> stringNumbers = {"10", "20", "30", "invalid",
                                              "40"};
    std::vector<int> validNumbers;
    std::vector<std::string> invalidStrings;

    for (const auto& str : stringNumbers) {
        auto result = stringToNumeric<int>(str);
        if (result) {
            validNumbers.push_back(*result);
            std::cout << "\"" << str << "\" -> " << *result << " (Valid)"
                      << std::endl;
        } else {
            invalidStrings.push_back(str);
            std::cout << "\"" << str << "\" -> Failed (Invalid)" << std::endl;
        }
    }

    std::cout << "Valid numbers: " << validNumbers.size() << std::endl;
    std::cout << "Invalid strings: " << invalidStrings.size() << std::endl;

    // ============================
    // Example 4: Custom Type Conversions
    // ============================
    printSection("4. Custom Type Conversions");

    printSubsection("Point Serialization/Deserialization");

    std::vector<Point> points = {Point(1.5, 2.3), Point(-3.7, 4.8),
                                 Point(0.0, 0.0)};

    std::vector<std::string> serializedPoints;

    // Serialize points to strings
    std::cout << "Serialization:" << std::endl;
    for (const auto& point : points) {
        std::string serialized = point.toString();
        serializedPoints.push_back(serialized);
        std::cout << "Point(" << point.x << ", " << point.y << ") -> \""
                  << serialized << "\"" << std::endl;
    }

    // Deserialize strings back to points
    std::cout << "\nDeserialization:" << std::endl;
    std::vector<Point> deserializedPoints;
    for (const auto& str : serializedPoints) {
        auto result = Point::fromString(str);
        if (result) {
            deserializedPoints.push_back(*result);
            std::cout << "\"" << str << "\" -> Point(" << result->x << ", "
                      << result->y << ") (Success)" << std::endl;
        } else {
            std::cout << "\"" << str << "\" -> Failed" << std::endl;
        }
    }

    // Verify round-trip conversion
    std::cout << "\nRound-trip verification:" << std::endl;
    bool allMatch = true;
    for (size_t i = 0; i < points.size() && i < deserializedPoints.size();
         ++i) {
        bool matches = points[i] == deserializedPoints[i];
        allMatch &= matches;
        std::cout << "Point " << i << ": " << (matches ? "MATCH" : "MISMATCH")
                  << std::endl;
    }
    std::cout << "Overall result: " << (allMatch ? "SUCCESS" : "FAILED")
              << std::endl;

    // ============================
    // Example 5: Variant and Optional Handling
    // ============================
    printSection("5. Variant and Optional Handling");

    printSubsection("Variant Type Conversions");

    using NumberVariant = std::variant<int, double, std::string>;

    std::vector<NumberVariant> variants = {42, 3.14159, std::string("Hello"),
                                           -100, 2.71828};

    for (const auto& var : variants) {
        std::cout << "Variant contains: ";
        std::visit(
            [](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, int>) {
                    std::cout << "int(" << value << ")";
                } else if constexpr (std::is_same_v<T, double>) {
                    std::cout << "double(" << std::fixed << std::setprecision(5)
                              << value << ")";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << "string(\"" << value << "\")";
                }
            },
            var);
        std::cout << std::endl;
    }

    printSubsection("Optional Chain Conversions");

    auto processString = [](const std::string& input) -> std::optional<double> {
        // Chain: string -> int -> double
        auto intResult = stringToNumeric<int>(input);
        if (!intResult)
            return std::nullopt;

        auto doubleResult = safeConvert<double>(*intResult);
        if (!doubleResult)
            return std::nullopt;

        return *doubleResult * 2.0;  // Some processing
    };

    std::vector<std::string> chainTests = {"42", "invalid", "100",
                                           "999999999999"};
    for (const auto& test : chainTests) {
        auto result = processString(test);
        std::cout << "Process \"" << test << "\": ";
        if (result) {
            std::cout << *result << " (Success)";
        } else {
            std::cout << "Failed";
        }
        std::cout << std::endl;
    }

    std::cout << "\nAll type conversion examples completed successfully!"
              << std::endl;

    return 0;
}
