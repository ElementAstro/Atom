/**
 * @file type_caster.cpp
 * @brief Comprehensive examples of using the TypeCaster library
 * @author Max Qian
 * @date 2024
 */

#include <chrono>
#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "" atom / meta / type_caster.hpp ""

// Example of a custom user-defined type
struct Point {
    double x, y;

    Point(double x_val = 0.0, double y_val = 0.0) : x(x_val), y(y_val) {}

    std::string toString() const {
        return "" Point("" + std::to_string(x) + "",
                        "" + std::to_string(y) + "") "";
    }
};

// Example of a complex user-defined type
struct Rectangle {
    Point topLeft;
    Point bottomRight;

    Rectangle() = default;
    Rectangle(const Point& tl, const Point& br)
        : topLeft(tl), bottomRight(br) {}

    std::string toString() const {
        return "" Rectangle("" + topLeft.toString() + "",
                            "" + bottomRight.toString() + "") "";
    }

    double area() const {
        return (bottomRight.x - topLeft.x) * (bottomRight.y - topLeft.y);
    }
};

// Example of a custom enum type
enum class Color { Red, Green, Blue, Yellow, Black, White };

void printSection(const std::string& title) {
    std::cout << ""\n "" << std::string(60, '=') << std::endl;
    std::cout << ""
                 ""
              << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

int main() {
    std::cout << "" TYPECASTER COMPREHENSIVE EXAMPLES\n "";
    std::cout << ""(Including C++ 23 Enhanced Features)\n "";
    std::cout << std::string(60, '=') << std::endl;

    auto typeCaster = atom::meta::TypeCaster::createShared();

    //--------------------------------------------------------------------------
    printSection("" 1. Basic Type Registration "");
    //--------------------------------------------------------------------------

    auto registeredTypes = typeCaster->getRegisteredTypes();
    std::cout << "" Pre - registered types count : "" << registeredTypes.size()
              << std::endl;

    typeCaster->registerType<Point>("" Point "");
    typeCaster->registerType<Rectangle>("" Rectangle "");
    typeCaster->registerType<std::vector<int>>("" IntVector "");

    std::cout << "" After custom types registration : ""
              << typeCaster->getRegisteredTypes().size()
              << "" types ""
              << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 2. Type Aliases "");
    //--------------------------------------------------------------------------

    typeCaster->registerAlias<Point>("" 2DPoint "");
    typeCaster->registerAlias<Rectangle>("" Rect "");
    typeCaster->registerAlias<std::vector<int>>("" IntArray "");

    std::cout << "" Registered aliases : "" << std::endl;
    std::cout << "" Point->2DPoint "" << std::endl;
    std::cout << "" Rectangle->Rect "" << std::endl;
    std::cout << "" std::vector<int>->IntArray "" << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 3. Basic Type Conversions "");
    //--------------------------------------------------------------------------

    typeCaster->registerConversion<int, double>(
        [](const std::any& value) -> std::any {
            return static_cast<double>(std::any_cast<int>(value));
        });

    typeCaster->registerConversion<double, int>(
        [](const std::any& value) -> std::any {
            return static_cast<int>(std::any_cast<double>(value));
        });

    typeCaster->registerConversion<std::string, int>(
        [](const std::any& value) -> std::any {
            try {
                return std::stoi(std::any_cast<std::string>(value));
            } catch (...) {
                return 0;
            }
        });

    typeCaster->registerConversion<int, std::string>(
        [](const std::any& value) -> std::any {
            return std::to_string(std::any_cast<int>(value));
        });

    int intValue = 42;
    std::any anyInt = intValue;

    std::any convertedDouble = typeCaster->convert<double>(anyInt);
    std::cout << "" int to double : "" << intValue << ""->""
              << std::any_cast<double>(convertedDouble)
              << std::endl;

    std::any convertedString = typeCaster->convert<std::string>(anyInt);
    std::cout << "" int to string : "" << intValue
              << ""-> \""
                       ""
              << std::any_cast<std::string>(convertedString)
              << ""\""
                    ""
              << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 4. NEW : C++ 23 Castable Concepts "");
    //--------------------------------------------------------------------------

    std::cout << "" Castable concept checks : "" << std::endl;
    std::cout << "" Castable<int, double> : ""
              << (atom::meta::Castable<int, double> ? "" true "" : "" false "")
              << std::endl;
    std::cout << "" Castable<double, int> : ""
              << (atom::meta::Castable<double, int> ? "" true "" : "" false "")
              << std::endl;
    std::cout << "" Castable<int, long> : ""
              << (atom::meta::Castable<int, long> ? "" true "" : "" false "")
              << std::endl;
    std::cout << "" Castable<std::string, int> : ""
              << (atom::meta::Castable<std::string, int> ? "" true ""
                                                         : "" false "")
              << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 5. NEW : safeCast Function "");
    //--------------------------------------------------------------------------

    std::cout << "" safeCast examples : "" << std::endl;

    auto safeResult1 = atom::meta::safeCast<double>(42);
    std::cout << "" safeCast<double>(42) : "";
    if (safeResult1.has_value()) {
        std::cout << *safeResult1 << std::endl;
    } else {
        std::cout << "" failed "" << std::endl;
    }

    auto safeResult2 = atom::meta::safeCast<int>(3.14159);
    std::cout << "" safeCast<int>(3.14159) : "";
    if (safeResult2.has_value()) {
        std::cout << *safeResult2 << std::endl;
    } else {
        std::cout << "" failed "" << std::endl;
    }

    auto safeResult3 = atom::meta::safeCast<long>(100);
    std::cout << "" safeCast<long>(100) : "";
    if (safeResult3.has_value()) {
        std::cout << *safeResult3 << std::endl;
    } else {
        std::cout << "" failed "" << std::endl;
    }

    //--------------------------------------------------------------------------
    printSection("" 6. NEW : castOrDefault Function "");
    //--------------------------------------------------------------------------

    std::cout << "" castOrDefault examples : "" << std::endl;

    int result1 = atom::meta::castOrDefault<int>(3.14, 0);
    std::cout << "" castOrDefault<int>(3.14, 0) : "" << result1 << std::endl;

    double result2 = atom::meta::castOrDefault<double>(42, 0.0);
    std::cout << "" castOrDefault<double>(42, 0.0) : "" << result2 << std::endl;

    long result3 = atom::meta::castOrDefault<long>(100, -1L);
    std::cout << "" castOrDefault<long>(100, -1) : "" << result3 << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 7. NEW : TypeCasterBuilder(Fluent API) "");
    //--------------------------------------------------------------------------

    std::cout << "" Building TypeCaster with fluent API... "" << std::endl;

    auto builtCaster = atom::meta::buildTypeCaster()
                           .registerType<int>("" integer "")
                           .registerType<double>("" real "")
                           .registerType<std::string>("" text "")
                           .build();

    auto builtTypes = builtCaster.getRegisteredTypes();
    std::cout << "" Built caster has "" << builtTypes.size()
              << "" registered types "" << std::endl;
    for (const auto& type : builtTypes) {
        std::cout << "" - "" << type << std::endl;
    }

    //--------------------------------------------------------------------------
    printSection("" 8. NEW : DynamicCaster "");
    //--------------------------------------------------------------------------

    std::cout << "" DynamicCaster examples : "" << std::endl;

    atom::meta::DynamicCaster dynCaster;

    dynCaster.registerCast<int, double>(
        [](const int& i) { return static_cast<double>(i); });

    dynCaster.registerCast<double, std::string>(
        [](const double& d) { return std::to_string(d); });

    std::cout << "" Registered int->double and double->string conversions ""
                                                   << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 9. NEW : Global TypeCaster "");
    //--------------------------------------------------------------------------

    auto& globalCaster = atom::meta::getGlobalTypeCaster();
    auto globalTypes = globalCaster.getRegisteredTypes();
    std::cout << "" Global TypeCaster has "" << globalTypes.size()
              << "" registered types "" << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 10. Custom Type Conversions "");
    //--------------------------------------------------------------------------

    typeCaster->registerConversion<std::string, Point>(
        [](const std::any& value) -> std::any {
            std::string str = std::any_cast<std::string>(value);
            size_t commaPos = str.find(',');
            if (commaPos != std::string::npos) {
                try {
                    double x = std::stod(str.substr(0, commaPos));
                    double y = std::stod(str.substr(commaPos + 1));
                    return Point(x, y);
                } catch (...) {
                    return Point();
                }
            }
            return Point();
        });

    typeCaster->registerConversion<Point, std::string>(
        [](const std::any& value) -> std::any {
            Point p = std::any_cast<Point>(value);
            return p.toString();
        });

    std::string pointStr = "" 10.5, 20.3 "";
    std::any anyPointStr = pointStr;

    std::any convertedPoint = typeCaster->convert<Point>(anyPointStr);
    Point point = std::any_cast<Point>(convertedPoint);
    std::cout << ""string to Point: \"""" << pointStr << ""\"" -> ""
              << point.toString() << std::endl;

    std::any anyPoint = point;
    std::any reconvertedStr = typeCaster->convert<std::string>(anyPoint);
    std::cout << "" Point to string : "" << point.toString()
              << ""-> \""
                       ""
              << std::any_cast<std::string>(reconvertedStr)
              << ""\""
                    ""
              << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 11. Enum Registration and Conversion "");
    //--------------------------------------------------------------------------

    typeCaster->registerEnumValue<Color>("" Color "", "" red "", Color::Red);
    typeCaster->registerEnumValue<Color>("" Color "", "" green "",
                                         Color::Green);
    typeCaster->registerEnumValue<Color>("" Color "", "" blue "", Color::Blue);
    typeCaster->registerEnumValue<Color>("" Color "", "" yellow "",
                                         Color::Yellow);
    typeCaster->registerEnumValue<Color>("" Color "", "" black "",
                                         Color::Black);
    typeCaster->registerEnumValue<Color>("" Color "", "" white "",
                                         Color::White);

    Color color = Color::Blue;
    std::string colorStr = typeCaster->enumToString(color, "" Color "");
    std::cout << ""Enum to string: Color::Blue -> \"""" << colorStr << ""\"""" << std::endl;

    std::string colorName = "" yellow "";
    Color convertedColor =
        typeCaster->stringToEnum<Color>(colorName, "" Color "");
    bool isYellow = (convertedColor == Color::Yellow);
    std::cout << "" String to enum : \"" yellow\""->""
              << (isYellow ? "" Color::Yellow "" : "" Other color "")
              << std::endl;

    try {
        auto invalidColor =
            typeCaster->stringToEnum<Color>("" purple "", "" Color "");
        std::cout << "" Invalid color converted(unexpected !) "" << std::endl;
        (void)invalidColor;
    } catch (const std::exception& e) {
        std::cout << ""Exception for invalid enum (expected): "" << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    printSection("" 12. Type Groups "");
    //--------------------------------------------------------------------------

    typeCaster->registerTypeGroup(
        "" NumericTypes "",
        {"" int "", "" double "", "" float "", "" size_t "", "" long ""});
    typeCaster->registerTypeGroup(
        "" GeometryTypes "",
        {"" Point "", "" 2DPoint "", "" Rectangle "", "" Rect ""});

    std::cout << "" Registered type groups : "" << std::endl;
    std::cout << "" - NumericTypes : int, double, float, size_t,
        long "" << std::endl;
    std::cout << "" - GeometryTypes : Point, 2DPoint, Rectangle,
        Rect "" << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 13. Conversion Path Detection "");
    //--------------------------------------------------------------------------

    bool hasIntToDouble =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<int>(),
                                  atom::meta::TypeInfo::create<double>());

    bool hasStringToPoint =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<std::string>(),
                                  atom::meta::TypeInfo::create<Point>());

    bool hasRectToInt =
        typeCaster->hasConversion(atom::meta::TypeInfo::create<Rectangle>(),
                                  atom::meta::TypeInfo::create<int>());

    std::cout << "" Conversion path detection : "" << std::endl;
    std::cout << "" int->double : "" << (hasIntToDouble ? "" Yes "" : "" No "")
              << std::endl;
    std::cout << "" string->Point : ""
              << (hasStringToPoint ? "" Yes "" : "" No "") << std::endl;
    std::cout << "" Rectangle->int : "" << (hasRectToInt ? "" Yes "" : "" No "")
              << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 14. Error Handling "");
    //--------------------------------------------------------------------------

    try {
        std::map<std::string, int> testMap = {{"" key1 "", 1}};
        std::any anyMap = testMap;
        std::any invalidConversion =
            typeCaster->convert<std::vector<double>>(anyMap);
        std::cout << "" Invalid conversion succeeded(unexpected !) ""
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "" Exception caught(expected)
            : "" << e.what() << std::endl;
    }

    try {
        typeCaster->registerConversion<int, int>(
            [](const std::any& value) -> std::any { return value; });
        std::cout << "" Same - type conversion registered(unexpected !) ""
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "" Exception caught(expected)
            : "" << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    printSection("" 15. Temporal Type Conversions "");
    //--------------------------------------------------------------------------

    typeCaster->registerType<std::chrono::seconds>("" seconds "");
    typeCaster->registerType<std::chrono::milliseconds>("" milliseconds "");
    typeCaster->registerType<std::chrono::minutes>("" minutes "");

    typeCaster
        ->registerConversion<std::chrono::seconds, std::chrono::milliseconds>(
            [](const std::any& value) -> std::any {
                auto sec = std::any_cast<std::chrono::seconds>(value);
                return std::chrono::milliseconds(sec);
            });

    typeCaster->registerConversion<std::chrono::minutes, std::chrono::seconds>(
        [](const std::any& value) -> std::any {
            auto min = std::any_cast<std::chrono::minutes>(value);
            return std::chrono::seconds(min);
        });

    std::chrono::minutes testMin(2);
    std::any anyMinutes = testMin;

    std::any convertedSec =
        typeCaster->convert<std::chrono::seconds>(anyMinutes);
    auto seconds = std::any_cast<std::chrono::seconds>(convertedSec);

    std::any convertedMs =
        typeCaster->convert<std::chrono::milliseconds>(convertedSec);
    auto milliseconds = std::any_cast<std::chrono::milliseconds>(convertedMs);

    std::cout << "" Time conversions : "" << std::endl;
    std::cout << "" 2 minutes = "" << seconds.count() << "" seconds ""
                                   << std::endl;
    std::cout << ""
                 ""
              << seconds.count() << "" seconds = "" << milliseconds.count()
                                                    << "" ms "" << std::endl;

    //--------------------------------------------------------------------------
    printSection("" 16. STL Container Conversions "");
    //--------------------------------------------------------------------------

    typeCaster->registerConversion<std::vector<int>, std::string>(
        [](const std::any& value) -> std::any {
            auto vec = std::any_cast<std::vector<int>>(value);
            std::string result;
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0)
                    result += "", "";
                result += std::to_string(vec[i]);
            }
            return result;
        });

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
                    result.push_back(
                        std::stoi(str.substr(pos, commaPos - pos)));
                } catch (...) {
                }
                pos = commaPos + 1;
            }
            return result;
        });

    std::vector<int> testVector = {10, 20, 30, 40, 50};
    std::any anyVector = testVector;

    std::any vecToString = typeCaster->convert<std::string>(anyVector);
    std::string vectorStr = std::any_cast<std::string>(vecToString);

    std::any stringToVec = typeCaster->convert<std::vector<int>>(vecToString);
    auto reconvertedVector = std::any_cast<std::vector<int>>(stringToVec);

    std::cout << "" STL container conversions : "" << std::endl;
    std::cout << "" Vector to string : {
        10, 20, 30, 40, 50
    } -> \"""" << vectorStr << ""\"""" << std::endl;
    std::cout << ""  String to vector: \"""" << vectorStr << ""\"" -> {
        "";
        for (size_t i = 0; i < reconvertedVector.size(); ++i) {
            if (i > 0)
                std::cout << "", "";
            std::cout << reconvertedVector[i];
        }
        std::cout << ""
    }
    "" << std::endl;

    std::cout << ""\n "" << std::string(60, '=') << std::endl;
    std::cout << "" All TypeCaster examples completed successfully !""
              << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}
