/**
 * @file type_caster_example.cpp
 * @brief Comprehensive examples of using the TypeCaster library
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "atom/meta/type_caster.hpp"

// Example of a custom user-defined type
struct Point {
    double x, y;

    Point(double x_val = 0.0, double y_val = 0.0) : x(x_val), y(y_val) {}

    std::string toString() const {
        return "Point(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

// Example of a complex user-defined type that can be converted from other types
struct Rectangle {
    Point topLeft;
    Point bottomRight;

    Rectangle() = default;
    Rectangle(const Point& tl, const Point& br)
        : topLeft(tl), bottomRight(br) {}

    std::string toString() const {
        return "Rectangle(" + topLeft.toString() + ", " +
               bottomRight.toString() + ")";
    }

    double area() const {
        return (bottomRight.x - topLeft.x) * (bottomRight.y - topLeft.y);
    }
};

// Example of a custom enum type
enum class Color { Red, Green, Blue, Yellow, Black, White };

// Simple utility to print section headers
void printSection(const std::string& title) {
    std::cout << "\n============== " << title << " ==============\n";
}

// Helper function to print any value with label
template <typename T>
void printValue(const std::string& label, const T& value) {
    std::cout << label << ": " << value << std::endl;
}

// Helper to print type information
void printTypeInfo(const std::string& label) {
    std::cout << "Type info for " << label << ": " << typeid(label).name()
              << std::endl;
}

int main() {
    std::cout << "TYPECASTER COMPREHENSIVE EXAMPLES\n";
    std::cout << "================================\n";

    // Create the TypeCaster instance
    auto typeCaster = atom::meta::TypeCaster::createShared();

    //--------------------------------------------------------------------------
    // 1. Basic Type Registration and Built-in Types
    //--------------------------------------------------------------------------
    printSection("Basic Type Registration and Built-in Types");

    // Built-in types are already registered
    auto registeredTypes = typeCaster->getRegisteredTypes();
    std::cout << "Pre-registered types:\n";
    for (const auto& type : registeredTypes) {
        std::cout << "  - " << type << std::endl;
    }

    // Register custom types
    typeCaster->registerType<Point>("Point");
    typeCaster->registerType<Rectangle>("Rectangle");
    typeCaster->registerType<std::vector<int>>("IntVector");
    typeCaster->registerType<std::map<std::string, double>>("StringDoubleMap");

    // Check updated registered types
    registeredTypes = typeCaster->getRegisteredTypes();
    std::cout << "\nRegistered types after adding custom types:\n";
    for (const auto& type : registeredTypes) {
        std::cout << "  - " << type << std::endl;
    }

    //--------------------------------------------------------------------------
    // 2. Type Aliases
    //--------------------------------------------------------------------------
    printSection("Type Aliases");

    // Register aliases for easier type referencing
    typeCaster->registerAlias<Point>("2DPoint");
    typeCaster->registerAlias<Rectangle>("Rect");
    typeCaster->registerAlias<std::vector<int>>("IntArray");

    std::cout << "Aliases have been registered for:\n";
    std::cout << "  - Point -> 2DPoint\n";
    std::cout << "  - Rectangle -> Rect\n";
    std::cout << "  - std::vector<int> -> IntArray\n";

    //--------------------------------------------------------------------------
    // 3. Basic Type Conversions
    //--------------------------------------------------------------------------
    printSection("Basic Type Conversions");

    // Register basic conversions

    // int to double conversion
    typeCaster->registerConversion<int, double>(
        [](const std::any& value) -> std::any {
            return static_cast<double>(std::any_cast<int>(value));
        });

    // double to int conversion
    typeCaster->registerConversion<double, int>(
        [](const std::any& value) -> std::any {
            return static_cast<int>(std::any_cast<double>(value));
        });

    // string to int conversion
    typeCaster->registerConversion<std::string, int>(
        [](const std::any& value) -> std::any {
            try {
                return std::stoi(std::any_cast<std::string>(value));
            } catch (const std::exception&) {
                return 0;  // Default value on failure
            }
        });

    // int to string conversion
    typeCaster->registerConversion<int, std::string>(
        [](const std::any& value) -> std::any {
            return std::to_string(std::any_cast<int>(value));
        });

    // Test basic conversions
    int intValue = 42;
    std::any anyInt = intValue;

    // Convert int to double
    std::any convertedDouble = typeCaster->convert<double>(anyInt);
    std::cout << "int to double: " << intValue << " -> "
              << std::any_cast<double>(convertedDouble) << std::endl;

    // Convert int to string
    std::any convertedString = typeCaster->convert<std::string>(anyInt);
    std::cout << "int to string: " << intValue << " -> "
              << std::any_cast<std::string>(convertedString) << std::endl;

    // Convert string to int
    std::string strValue = "123";
    std::any anyString = strValue;
    std::any convertedInt = typeCaster->convert<int>(anyString);
    std::cout << "string to int: \"" << strValue << "\" -> "
              << std::any_cast<int>(convertedInt) << std::endl;

    //--------------------------------------------------------------------------
    // 4. Custom Type Conversions
    //--------------------------------------------------------------------------
    printSection("Custom Type Conversions");

    // Register conversion from string to Point
    typeCaster->registerConversion<std::string, Point>(
        [](const std::any& value) -> std::any {
            std::string str = std::any_cast<std::string>(value);
            // Parse format like "10.5,20.3"
            size_t commaPos = str.find(',');
            if (commaPos != std::string::npos) {
                try {
                    double x = std::stod(str.substr(0, commaPos));
                    double y = std::stod(str.substr(commaPos + 1));
                    return Point(x, y);
                } catch (const std::exception&) {
                    // Return default Point on failure
                    return Point();
                }
            }
            return Point();
        });

    // Register conversion from Point to string
    typeCaster->registerConversion<Point, std::string>(
        [](const std::any& value) -> std::any {
            Point p = std::any_cast<Point>(value);
            return p.toString();
        });

    // Test custom type conversions
    std::string pointStr = "10.5,20.3";
    std::any anyPointStr = pointStr;

    // Convert string to Point
    std::any convertedPoint = typeCaster->convert<Point>(anyPointStr);
    Point point = std::any_cast<Point>(convertedPoint);
    std::cout << "string to Point: \"" << pointStr << "\" -> "
              << point.toString() << std::endl;

    // Convert Point to string
    std::any anyPoint = point;
    std::any reconvertedStr = typeCaster->convert<std::string>(anyPoint);
    std::cout << "Point to string: " << point.toString() << " -> \""
              << std::any_cast<std::string>(reconvertedStr) << "\""
              << std::endl;

    //--------------------------------------------------------------------------
    // 5. Multi-Stage Conversions
    //--------------------------------------------------------------------------
    printSection("Multi-Stage Conversions");

    // Register conversion from two Points to Rectangle
    typeCaster->registerConversion<std::vector<Point>, Rectangle>(
        [](const std::any& value) -> std::any {
            auto points = std::any_cast<std::vector<Point>>(value);
            if (points.size() >= 2) {
                return Rectangle(points[0], points[1]);
            }
            return Rectangle();
        });

    // Register conversion from string to vector of Points
    typeCaster->registerConversion<std::string, std::vector<Point>>(
        [](const std::any& value) -> std::any {
            std::string str = std::any_cast<std::string>(value);
            // Parse format like "(0,0),(100,100)"
            std::vector<Point> points;

            size_t pos = 0;
            while (pos < str.length()) {
                size_t openBracket = str.find('(', pos);
                if (openBracket == std::string::npos)
                    break;

                size_t closeBracket = str.find(')', openBracket);
                if (closeBracket == std::string::npos)
                    break;

                std::string pointStr =
                    str.substr(openBracket + 1, closeBracket - openBracket - 1);
                size_t commaPos = pointStr.find(',');

                if (commaPos != std::string::npos) {
                    try {
                        double x = std::stod(pointStr.substr(0, commaPos));
                        double y = std::stod(pointStr.substr(commaPos + 1));
                        points.emplace_back(x, y);
                    } catch (const std::exception&) {
                        // Skip invalid points
                    }
                }

                pos = closeBracket + 1;
            }

            return points;
        });

    // Multi-stage conversion from string to Rectangle (via vector<Point>)
    std::string rectStr = "(0,0),(100,100)";
    std::any anyRectStr = rectStr;

    // This will use two conversions: string -> vector<Point> -> Rectangle
    std::any convertedRect = typeCaster->convert<Rectangle>(anyRectStr);
    Rectangle rect = std::any_cast<Rectangle>(convertedRect);

    std::cout << "Multi-stage conversion from string to Rectangle:\n";
    std::cout << "  Input: \"" << rectStr << "\"\n";
    std::cout << "  Output: " << rect.toString() << "\n";
    std::cout << "  Rectangle area: " << rect.area() << std::endl;

    //--------------------------------------------------------------------------
    // 6. Enum Registration and Conversion
    //--------------------------------------------------------------------------
    printSection("Enum Registration and Conversion");

    // Register Color enum values
    typeCaster->registerEnumValue<Color>("Color", "red", Color::Red);
    typeCaster->registerEnumValue<Color>("Color", "green", Color::Green);
    typeCaster->registerEnumValue<Color>("Color", "blue", Color::Blue);
    typeCaster->registerEnumValue<Color>("Color", "yellow", Color::Yellow);
    typeCaster->registerEnumValue<Color>("Color", "black", Color::Black);
    typeCaster->registerEnumValue<Color>("Color", "white", Color::White);

    // Convert enum to string
    Color color = Color::Blue;
    std::string colorStr = typeCaster->enumToString(color, "Color");
    std::cout << "Enum to string: Color::Blue -> \"" << colorStr << "\""
              << std::endl;

    // Convert string to enum
    std::string colorName = "yellow";
    Color convertedColor = typeCaster->stringToEnum<Color>(colorName, "Color");
    bool isYellow = (convertedColor == Color::Yellow);
    std::cout << "String to enum: \"yellow\" -> "
              << (isYellow ? "Color::Yellow" : "Other color") << std::endl;

    // Try an invalid conversion (will throw exception, so we catch it)
    // 处理未使用变量的问题
    try {
        auto invalidColor = typeCaster->stringToEnum<Color>("purple", "Color");
        std::cout << "Invalid color converted successfully (unexpected!): "
                  << typeCaster->enumToString(invalidColor, "Color")
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught (expected): " << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 7. Type Groups
    //--------------------------------------------------------------------------
    printSection("Type Groups");

    // Register type groups
    typeCaster->registerTypeGroup(
        "NumericTypes",
        {"int", "double", "float", "size_t", "long", "long long"});

    typeCaster->registerTypeGroup("GeometryTypes",
                                  {"Point", "2DPoint", "Rectangle", "Rect"});

    std::cout << "Registered type groups:\n";
    std::cout
        << "  - NumericTypes: int, double, float, size_t, long, long long\n";
    std::cout << "  - GeometryTypes: Point, 2DPoint, Rectangle, Rect\n";

    //--------------------------------------------------------------------------
    // 8. Conversion Path Detection
    //--------------------------------------------------------------------------
    printSection("Conversion Path Detection");

    // Check if conversions exist
    bool hasIntToDouble =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<int>(),
                                  atom::meta::TypeInfo::create<double>());

    bool hasStringToRect =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<std::string>(),
                                  atom::meta::TypeInfo::create<Rectangle>());

    bool hasStringToPoint =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<std::string>(),
                                  atom::meta::TypeInfo::create<Point>());

    bool hasRectToInt =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<Rectangle>(),
                                  atom::meta::TypeInfo::create<int>());

    std::cout << "Conversion path detection:\n";
    std::cout << "  - int to double: " << (hasIntToDouble ? "Yes" : "No")
              << "\n";
    std::cout << "  - string to Rectangle: " << (hasStringToRect ? "Yes" : "No")
              << "\n";
    std::cout << "  - string to Point: " << (hasStringToPoint ? "Yes" : "No")
              << "\n";
    std::cout << "  - Rectangle to int: " << (hasRectToInt ? "Yes" : "No")
              << std::endl;

    //--------------------------------------------------------------------------
    // 9. Complex Multi-Stage Conversion Example
    //--------------------------------------------------------------------------
    printSection("Complex Multi-Stage Conversion Example");

    // Create a more complex conversion chain

    // Register conversion from Rectangle to double (area calculation)
    typeCaster->registerConversion<Rectangle, double>(
        [](const std::any& value) -> std::any {
            Rectangle rect = std::any_cast<Rectangle>(value);
            return rect.area();
        });

    // Now we can convert from string to Rectangle to double
    // string -> vector<Point> -> Rectangle -> double

    std::string complexInput = "(10,20),(60,80)";
    std::any anyComplexInput = complexInput;

    // Convert through multiple stages to get the area
    std::any finalResult = typeCaster->convert<double>(anyComplexInput);
    double area = std::any_cast<double>(finalResult);

    std::cout << "Complex multi-stage conversion:\n";
    std::cout << "  Input: \"" << complexInput << "\"\n";
    std::cout << "  Conversion stages: string -> vector<Point> -> Rectangle -> "
                 "double\n";
    std::cout << "  Result (area): " << area << std::endl;

    //--------------------------------------------------------------------------
    // 10. Error Handling
    //--------------------------------------------------------------------------
    printSection("Error Handling");

    // Try to convert between types with no valid conversion path
    try {
        std::map<std::string, int> testMap = {{"key1", 1}, {"key2", 2}};
        std::any anyMap = testMap;
        std::any invalidConversion =
            typeCaster->convert<std::vector<double>>(anyMap);
        std::cout << "Invalid conversion succeeded (unexpected!)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught (expected): " << e.what() << std::endl;
    }

    // Try to register a conversion between the same types
    try {
        typeCaster->registerConversion<int, int>(
            [](const std::any& value) -> std::any {
                return std::any_cast<int>(value);
            });
        std::cout << "Invalid conversion registration succeeded (unexpected!)"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught (expected): " << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 11. Advanced: Temporal Type Conversions
    //--------------------------------------------------------------------------
    printSection("Advanced: Temporal Type Conversions");

    // Register time-related type conversions

    // Register types
    typeCaster->registerType<std::chrono::seconds>("seconds");
    typeCaster->registerType<std::chrono::milliseconds>("milliseconds");
    typeCaster->registerType<std::chrono::minutes>("minutes");

    // Register conversions
    typeCaster
        ->registerConversion<std::chrono::seconds, std::chrono::milliseconds>(
            [](const std::any& value) -> std::any {
                auto sec = std::any_cast<std::chrono::seconds>(value);
                return std::chrono::milliseconds(sec);
            });

    typeCaster
        ->registerConversion<std::chrono::milliseconds, std::chrono::seconds>(
            [](const std::any& value) -> std::any {
                auto ms = std::any_cast<std::chrono::milliseconds>(value);
                return std::chrono::duration_cast<std::chrono::seconds>(ms);
            });

    typeCaster->registerConversion<std::chrono::minutes, std::chrono::seconds>(
        [](const std::any& value) -> std::any {
            auto min = std::any_cast<std::chrono::minutes>(value);
            return std::chrono::seconds(min);
        });

    // Test time conversions
    std::chrono::minutes testMin(2);  // 2 minutes
    std::any anyMinutes = testMin;

    // Convert minutes to seconds
    std::any convertedSec =
        typeCaster->convert<std::chrono::seconds>(anyMinutes);
    auto seconds = std::any_cast<std::chrono::seconds>(convertedSec);

    // Convert seconds to milliseconds
    std::any convertedMs =
        typeCaster->convert<std::chrono::milliseconds>(convertedSec);
    auto milliseconds = std::any_cast<std::chrono::milliseconds>(convertedMs);

    std::cout << "Time conversions:\n";
    std::cout << "  2 minutes = " << seconds.count() << " seconds\n";
    std::cout << "  " << seconds.count()
              << " seconds = " << milliseconds.count() << " milliseconds"
              << std::endl;

    //--------------------------------------------------------------------------
    // 12. Advanced: Registration of STL Container Conversions
    //--------------------------------------------------------------------------
    printSection("Advanced: STL Container Conversions");

    // Register conversions between different STL containers

    // vector<int> to string conversion (comma-separated)
    typeCaster->registerConversion<std::vector<int>, std::string>(
        [](const std::any& value) -> std::any {
            auto vec = std::any_cast<std::vector<int>>(value);
            std::string result;
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0)
                    result += ",";
                result += std::to_string(vec[i]);
            }
            return result;
        });

    // string to vector<int> conversion
    typeCaster->registerConversion<std::string, std::vector<int>>(
        [](const std::any& value) -> std::any {
            std::string str = std::any_cast<std::string>(value);
            std::vector<int> result;

            size_t pos = 0;
            while (pos < str.length()) {
                size_t commaPos = str.find(',', pos);
                if (commaPos == std::string::npos)
                    commaPos = str.length();

                try {
                    int val = std::stoi(str.substr(pos, commaPos - pos));
                    result.push_back(val);
                } catch (const std::exception&) {
                    // Skip invalid numbers
                }

                pos = commaPos + 1;
            }

            return result;
        });

    // Test STL container conversions
    std::vector<int> testVector = {10, 20, 30, 40, 50};
    std::any anyVector = testVector;

    // Convert vector to string
    std::any vecToString = typeCaster->convert<std::string>(anyVector);
    std::string vectorStr = std::any_cast<std::string>(vecToString);

    // Convert string back to vector
    std::any stringToVec = typeCaster->convert<std::vector<int>>(vecToString);
    auto reconvertedVector = std::any_cast<std::vector<int>>(stringToVec);

    std::cout << "STL container conversions:\n";
    std::cout << "  Vector to string: {10,20,30,40,50} -> \"" << vectorStr
              << "\"\n";

    std::cout << "  String to vector: \"" << vectorStr << "\" -> {";
    for (size_t i = 0; i < reconvertedVector.size(); ++i) {
        if (i > 0)
            std::cout << ",";
        std::cout << reconvertedVector[i];
    }
    std::cout << "}" << std::endl;

    std::cout << "\nAll TypeCaster examples completed successfully!\n";
    return 0;
}
