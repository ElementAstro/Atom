/**
 * Comprehensive examples for atom::meta::conversion utilities
 *
 * This file demonstrates all type conversion functionality:
 * 1. Basic types conversion (primitive types)
 * 2. Class hierarchy conversion (polymorphic classes)
 * 3. Smart pointer conversion
 * 4. Container conversions (vector, map, set, list, deque)
 * 5. Custom type conversions
 * 6. Complex nested conversions
 * 7. Error handling and validation
 */

#include "atom/meta/conversion.hpp"
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::meta;

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n==========================================================="
                 "======="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout
        << "=================================================================="
        << std::endl;
}

// Helper function to print subsection headers
void printSubHeader(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper to log conversion results
template <typename From, typename To>
void logConversion(const std::string& name, bool success) {
    std::cout << std::setw(40) << std::left << name << ": "
              << (success ? "Success" : "Failed") << std::endl;
}

//=============================================================================
// 1. Basic class hierarchy for polymorphic conversions
//=============================================================================

// Base class
class Shape {
public:
    virtual ~Shape() = default;
    virtual std::string type() const { return "Shape"; }
    virtual double area() const { return 0.0; }
    virtual void describe() const {
        std::cout << "Shape: type=" << type() << ", area=" << area()
                  << std::endl;
    }
};

// Derived class: Circle
class Circle : public Shape {
private:
    double radius_;

public:
    explicit Circle(double radius) : radius_(radius) {}

    std::string type() const override { return "Circle"; }

    double area() const override { return 3.14159 * radius_ * radius_; }

    double getRadius() const { return radius_; }

    void describe() const override {
        std::cout << "Circle: radius=" << radius_ << ", area=" << area()
                  << std::endl;
    }
};

// Derived class: Rectangle
class Rectangle : public Shape {
private:
    double width_;
    double height_;

public:
    Rectangle(double width, double height) : width_(width), height_(height) {}

    std::string type() const override { return "Rectangle"; }

    double area() const override { return width_ * height_; }

    double getWidth() const { return width_; }
    double getHeight() const { return height_; }

    void describe() const override {
        std::cout << "Rectangle: width=" << width_ << ", height=" << height_
                  << ", area=" << area() << std::endl;
    }
};

// Derived class: Square (inherits from Rectangle)
class Square : public Rectangle {
public:
    explicit Square(double side) : Rectangle(side, side) {}

    std::string type() const override { return "Square"; }

    double getSide() const { return getWidth(); }

    void describe() const override {
        std::cout << "Square: side=" << getSide() << ", area=" << area()
                  << std::endl;
    }
};

//=============================================================================
// 2. Custom types for conversion demonstration
//=============================================================================

// Timestamp class
class Timestamp {
private:
    uint64_t milliseconds_;

public:
    explicit Timestamp(uint64_t ms) : milliseconds_(ms) {}

    uint64_t getMilliseconds() const { return milliseconds_; }

    std::string toString() const {
        std::ostringstream oss;
        oss << milliseconds_ << "ms";
        return oss.str();
    }
};

// DateTime class (for conversion with Timestamp)
class DateTime {
private:
    int year_, month_, day_;
    int hour_, minute_, second_, millisecond_;

public:
    DateTime(int year, int month, int day, int hour, int minute, int second,
             int ms)
        : year_(year),
          month_(month),
          day_(day),
          hour_(hour),
          minute_(minute),
          second_(second),
          millisecond_(ms) {}

    // Default to current time
    DateTime() {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto localTime = *std::localtime(&timeT);

        year_ = localTime.tm_year + 1900;
        month_ = localTime.tm_mon + 1;
        day_ = localTime.tm_mday;
        hour_ = localTime.tm_hour;
        minute_ = localTime.tm_min;
        second_ = localTime.tm_sec;

        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
        millisecond_ = nowMs % 1000;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << std::setfill('0') << year_ << "-" << std::setw(2) << month_
            << "-" << std::setw(2) << day_ << " " << std::setw(2) << hour_
            << ":" << std::setw(2) << minute_ << ":" << std::setw(2) << second_
            << "." << std::setw(3) << millisecond_;
        return oss.str();
    }

    uint64_t toMilliseconds() const {
        std::tm timeInfo = {};
        timeInfo.tm_year = year_ - 1900;
        timeInfo.tm_mon = month_ - 1;
        timeInfo.tm_mday = day_;
        timeInfo.tm_hour = hour_;
        timeInfo.tm_min = minute_;
        timeInfo.tm_sec = second_;

        auto timeT = std::mktime(&timeInfo);
        uint64_t ms = static_cast<uint64_t>(timeT) * 1000 + millisecond_;
        return ms;
    }
};

// Currency representation
class Money {
private:
    double amount_;
    std::string currency_;

public:
    Money(double amount, const std::string& currency)
        : amount_(amount), currency_(currency) {}

    double getAmount() const { return amount_; }
    std::string getCurrency() const { return currency_; }

    std::string toString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << amount_ << " "
            << currency_;
        return oss.str();
    }
};

// Simple string representation
class FormattedString {
private:
    std::string value_;

public:
    explicit FormattedString(const std::string& value) : value_(value) {}
    std::string getValue() const { return value_; }

    std::string toString() const { return "\"" + value_ + "\""; }
};

//=============================================================================
// 3. Custom conversion classes
//=============================================================================

// Conversion between Timestamp and DateTime
class TimestampToDateTimeConversion : public TypeConversionBase {
public:
    TimestampToDateTimeConversion()
        : TypeConversionBase(userType<DateTime>(), userType<Timestamp>()) {}

    std::any convert(const std::any& from) const override {
        try {
            const auto& timestamp = std::any_cast<const Timestamp&>(from);
            uint64_t ms = timestamp.getMilliseconds();

            auto timeT = ms / 1000;
            auto msRemaining = ms % 1000;

            // 修复: 正确处理时间转换
            std::time_t timeValue = static_cast<std::time_t>(timeT);
            auto localTime = *std::localtime(&timeValue);

            return std::any(
                DateTime(localTime.tm_year + 1900, localTime.tm_mon + 1,
                         localTime.tm_mday, localTime.tm_hour, localTime.tm_min,
                         localTime.tm_sec, static_cast<int>(msRemaining)));
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert Timestamp to DateTime");
        }
        // 添加返回值以防止编译器警告
        return std::any();
    }

    std::any convertDown(const std::any& to) const override {
        try {
            const auto& dateTime = std::any_cast<const DateTime&>(to);
            uint64_t ms = dateTime.toMilliseconds();
            return std::any(Timestamp(ms));
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert DateTime to Timestamp");
        }
        // 添加返回值以防止编译器警告
        return std::any();
    }
};

// Conversion between Money and FormattedString
class MoneyToFormattedStringConversion : public TypeConversionBase {
public:
    MoneyToFormattedStringConversion()
        : TypeConversionBase(userType<FormattedString>(), userType<Money>()) {}

    std::any convert(const std::any& from) const override {
        try {
            const auto& money = std::any_cast<const Money&>(from);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << money.getAmount()
                << " " << money.getCurrency();
            return std::any(FormattedString(oss.str()));
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR(
                "Failed to convert Money to FormattedString");
        }
        return std::any();
    }

    std::any convertDown(const std::any& to) const override {
        try {
            const auto& str = std::any_cast<const FormattedString&>(to);
            std::string valueStr = str.getValue();

            size_t spacePos = valueStr.find_last_of(' ');
            if (spacePos != std::string::npos) {
                double amount = std::stod(valueStr.substr(0, spacePos));
                std::string currency = valueStr.substr(spacePos + 1);
                return std::any(Money(amount, currency));
            }
            THROW_CONVERSION_ERROR("Invalid money format");
        } catch (const std::bad_any_cast& e) {
            THROW_CONVERSION_ERROR(
                "Failed to convert FormattedString to Money");
        } catch (const std::exception& e) {
            THROW_CONVERSION_ERROR(std::string("Invalid format: ") + e.what());
        }
        return std::any();
    }
};

//=============================================================================
// Main function with comprehensive examples
//=============================================================================

int main() {
    std::cout << "========================================================="
              << std::endl;
    std::cout << "   Comprehensive Type Conversion Examples                "
              << std::endl;
    std::cout << "========================================================="
              << std::endl;

    // Create conversion registry
    auto converter = TypeConversions::createShared();

    //=========================================================================
    // PART 1: Basic Polymorphic Class Conversions
    //=========================================================================
    printHeader("1. Basic Polymorphic Class Conversions");

    // Register class hierarchy
    converter->addBaseClass<Shape, Circle>();
    converter->addBaseClass<Shape, Rectangle>();
    converter->addBaseClass<Rectangle, Square>();

    // Create test objects
    auto circle = std::make_shared<Circle>(5.0);
    auto rectangle = std::make_shared<Rectangle>(4.0, 6.0);
    auto square = std::make_shared<Square>(3.0);

    printSubHeader("1.1 Raw Pointer Conversions");

    // Convert raw pointers
    try {
        Circle* circlePtr = new Circle(2.5);

        // 修复: convert -> convertTo
        std::any upcastPtr = converter->convertTo<Shape*>(std::any(circlePtr));
        Shape* shapePtr = std::any_cast<Shape*>(upcastPtr);
        std::cout << "Raw pointer conversion: " << circlePtr->type() << " -> "
                  << shapePtr->type() << std::endl;
        shapePtr->describe();

        // Clean up
        delete circlePtr;
    } catch (const BadConversionException& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    printSubHeader("1.2 Shared Pointer Conversions");

    // Upcast shared_ptr<Circle> to shared_ptr<Shape>
    try {
        // 修复: convert -> convertTo
        std::any circleAsShape =
            converter->convertTo<std::shared_ptr<Shape>>(std::any(circle));
        auto shapePtr = std::any_cast<std::shared_ptr<Shape>>(circleAsShape);

        std::cout << "Circle converted to Shape:" << std::endl;
        shapePtr->describe();

        // Check if connection is maintained
        std::cout << "Original circle use count: " << circle.use_count()
                  << std::endl;
        std::cout << "Converted shape use count: " << shapePtr.use_count()
                  << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    // Convert Square to Shape (multi-level inheritance)
    try {
        // 修复: convert -> convertTo
        std::any squareAsShape =
            converter->convertTo<std::shared_ptr<Shape>>(std::any(square));
        auto shapeFromSquare =
            std::any_cast<std::shared_ptr<Shape>>(squareAsShape);

        std::cout << "Square converted to Shape:" << std::endl;
        shapeFromSquare->describe();
    } catch (const BadConversionException& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    // Convert Square to Rectangle
    try {
        // 修复: convert -> convertTo
        std::any squareAsRect =
            converter->convertTo<std::shared_ptr<Rectangle>>(std::any(square));
        auto rectFromSquare =
            std::any_cast<std::shared_ptr<Rectangle>>(squareAsRect);

        std::cout << "Square converted to Rectangle:" << std::endl;
        rectFromSquare->describe();
    } catch (const BadConversionException& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    //=========================================================================
    // PART 2: Container Conversions
    //=========================================================================
    printHeader("2. Container Conversions");

    printSubHeader("2.1 Vector Conversions");

    // Register vector conversion
    converter->addVectorConversion<Circle, Shape>();
    converter->addVectorConversion<Rectangle, Shape>();
    converter->addVectorConversion<Square, Rectangle>();

    // Create test vector
    std::vector<std::shared_ptr<Circle>> circles = {
        std::make_shared<Circle>(1.0), std::make_shared<Circle>(2.0),
        std::make_shared<Circle>(3.0)};

    std::vector<std::shared_ptr<Rectangle>> rectangles = {
        std::make_shared<Rectangle>(1.0, 2.0),
        std::make_shared<Rectangle>(3.0, 4.0)};

    std::vector<std::shared_ptr<Square>> squares = {
        std::make_shared<Square>(2.0), std::make_shared<Square>(4.0)};

    // Convert vector of Circles to vector of Shapes
    try {
        // 修复: convert -> convertTo
        std::any circlesAsShapes =
            converter->convertTo<std::vector<std::shared_ptr<Shape>>>(
                std::any(circles));
        auto shapesVec =
            std::any_cast<std::vector<std::shared_ptr<Shape>>>(circlesAsShapes);

        std::cout << "Converted " << circles.size() << " circles to "
                  << shapesVec.size() << " shapes:" << std::endl;

        for (const auto& shape : shapesVec) {
            shape->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Vector conversion failed: " << e.what() << std::endl;
    }

    // Convert vector of Squares to vector of Rectangles
    try {
        // 修复: convert -> convertTo
        std::any squaresAsRects =
            converter->convertTo<std::vector<std::shared_ptr<Rectangle>>>(
                std::any(squares));
        auto rectsVec = std::any_cast<std::vector<std::shared_ptr<Rectangle>>>(
            squaresAsRects);

        std::cout << "Converted " << squares.size() << " squares to "
                  << rectsVec.size() << " rectangles:" << std::endl;

        for (const auto& rect : rectsVec) {
            rect->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Vector conversion failed: " << e.what() << std::endl;
    }

    printSubHeader("2.2 Map Conversions");

    // Register map conversion
    converter->addMapConversion<std::map, int, std::shared_ptr<Circle>, int,
                                std::shared_ptr<Shape>>();

    converter->addMapConversion<std::map, std::string, std::shared_ptr<Square>,
                                std::string, std::shared_ptr<Rectangle>>();

    // Create test maps
    std::map<int, std::shared_ptr<Circle>> circleMap = {
        {1, std::make_shared<Circle>(1.5)},
        {2, std::make_shared<Circle>(2.5)},
        {3, std::make_shared<Circle>(3.5)}};

    std::map<std::string, std::shared_ptr<Square>> squareMap = {
        {"small", std::make_shared<Square>(2.0)},
        {"medium", std::make_shared<Square>(5.0)},
        {"large", std::make_shared<Square>(10.0)}};

    // Convert map of Circles to map of Shapes
    try {
        // 修复: convert -> convertTo
        std::any circleMapAsShapeMap =
            converter->convertTo<std::map<int, std::shared_ptr<Shape>>>(
                std::any(circleMap));
        auto shapeMap = std::any_cast<std::map<int, std::shared_ptr<Shape>>>(
            circleMapAsShapeMap);

        std::cout << "Converted map with " << circleMap.size()
                  << " circles to map with " << shapeMap.size()
                  << " shapes:" << std::endl;

        for (const auto& [key, shape] : shapeMap) {
            std::cout << "Key " << key << ": ";
            shape->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Map conversion failed: " << e.what() << std::endl;
    }

    // Convert map of Squares to map of Rectangles
    try {
        // 修复: convert -> convertTo
        std::any squareMapAsRectMap =
            converter
                ->convertTo<std::map<std::string, std::shared_ptr<Rectangle>>>(
                    std::any(squareMap));
        auto rectMap =
            std::any_cast<std::map<std::string, std::shared_ptr<Rectangle>>>(
                squareMapAsRectMap);

        std::cout << "Converted map with " << squareMap.size()
                  << " squares to map with " << rectMap.size()
                  << " rectangles:" << std::endl;

        for (const auto& [key, rect] : rectMap) {
            std::cout << "Key '" << key << "': ";
            rect->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Map conversion failed: " << e.what() << std::endl;
    }

    printSubHeader("2.3 Set Conversions");

    // Register set conversion
    converter->addSetConversion<std::set, Circle, Shape>();
    converter->addSetConversion<std::set, Square, Rectangle>();

    // Create test sets
    std::set<std::shared_ptr<Circle>> circleSet;
    circleSet.insert(std::make_shared<Circle>(2.0));
    circleSet.insert(std::make_shared<Circle>(3.0));

    std::set<std::shared_ptr<Square>> squareSet;
    squareSet.insert(std::make_shared<Square>(1.0));
    squareSet.insert(std::make_shared<Square>(2.0));

    // Convert set of Circles to set of Shapes
    try {
        // 修复: convert -> convertTo
        std::any circleSetAsShapeSet =
            converter->convertTo<std::set<std::shared_ptr<Shape>>>(
                std::any(circleSet));
        auto shapeSet = std::any_cast<std::set<std::shared_ptr<Shape>>>(
            circleSetAsShapeSet);

        std::cout << "Converted set with " << circleSet.size()
                  << " circles to set with " << shapeSet.size()
                  << " shapes:" << std::endl;

        for (const auto& shape : shapeSet) {
            shape->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Set conversion failed: " << e.what() << std::endl;
    }

    printSubHeader("2.4 Sequence Conversions (List, Deque)");

    // Register sequence conversions
    converter->addSequenceConversion<std::list, Circle, Shape>();
    converter->addSequenceConversion<std::deque, Rectangle, Shape>();

    // Create test sequences
    std::list<std::shared_ptr<Circle>> circleList = {
        std::make_shared<Circle>(4.0), std::make_shared<Circle>(5.0)};

    std::deque<std::shared_ptr<Rectangle>> rectangleDeque = {
        std::make_shared<Rectangle>(2.0, 3.0),
        std::make_shared<Rectangle>(4.0, 5.0)};

    // Convert list of Circles to list of Shapes
    try {
        // 修复: convert -> convertTo
        std::any circleListAsShapeList =
            converter->convertTo<std::list<std::shared_ptr<Shape>>>(
                std::any(circleList));
        auto shapeList = std::any_cast<std::list<std::shared_ptr<Shape>>>(
            circleListAsShapeList);

        std::cout << "Converted list with " << circleList.size()
                  << " circles to list with " << shapeList.size()
                  << " shapes:" << std::endl;

        for (const auto& shape : shapeList) {
            shape->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "List conversion failed: " << e.what() << std::endl;
    }

    // Convert deque of Rectangles to deque of Shapes
    try {
        // 修复: convert -> convertTo
        std::any rectDequeAsShapeDeque =
            converter->convertTo<std::deque<std::shared_ptr<Shape>>>(
                std::any(rectangleDeque));
        auto shapeDeque = std::any_cast<std::deque<std::shared_ptr<Shape>>>(
            rectDequeAsShapeDeque);

        std::cout << "Converted deque with " << rectangleDeque.size()
                  << " rectangles to deque with " << shapeDeque.size()
                  << " shapes:" << std::endl;

        for (const auto& shape : shapeDeque) {
            shape->describe();
        }
    } catch (const BadConversionException& e) {
        std::cout << "Deque conversion failed: " << e.what() << std::endl;
    }

    //=========================================================================
    // PART 3: Custom Type Conversions
    //=========================================================================
    printHeader("3. Custom Type Conversions");

    // Register custom conversions
    converter->addConversion(std::make_shared<TimestampToDateTimeConversion>());
    converter->addConversion(
        std::make_shared<MoneyToFormattedStringConversion>());

    printSubHeader("3.1 Timestamp <-> DateTime Conversion");

    // Current timestamp
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch())
                     .count();

    Timestamp timestamp(nowMs);
    std::cout << "Original timestamp: " << timestamp.toString() << std::endl;

    // Convert Timestamp to DateTime
    try {
        // 修复: convert -> convertTo
        std::any timestampAsDateTime =
            converter->convertTo<DateTime>(std::any(timestamp));
        auto dateTime = std::any_cast<DateTime>(timestampAsDateTime);

        std::cout << "Converted to DateTime: " << dateTime.toString()
                  << std::endl;

        // Convert back to Timestamp
        // 修复: convert -> convertTo
        std::any dateTimeAsTimestamp =
            converter->convertTo<Timestamp>(std::any(dateTime));
        auto convertedBack = std::any_cast<Timestamp>(dateTimeAsTimestamp);

        std::cout << "Converted back to Timestamp: " << convertedBack.toString()
                  << std::endl;

        // Check consistency
        std::cout << "Time difference: "
                  << std::abs(
                         static_cast<int64_t>(convertedBack.getMilliseconds() -
                                              timestamp.getMilliseconds()))
                  << "ms" << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Timestamp conversion failed: " << e.what() << std::endl;
    }

    printSubHeader("3.2 Money <-> FormattedString Conversion");

    Money money(129.99, "USD");
    std::cout << "Original Money: " << money.toString() << std::endl;

    // Convert Money to FormattedString
    try {
        // 修复: convert -> convertTo
        std::any moneyAsString =
            converter->convertTo<FormattedString>(std::any(money));
        auto formattedStr = std::any_cast<FormattedString>(moneyAsString);

        std::cout << "Converted to FormattedString: " << formattedStr.toString()
                  << std::endl;

        // Convert back to Money
        // 修复: convert -> convertTo
        std::any stringAsMoney =
            converter->convertTo<Money>(std::any(formattedStr));
        auto convertedBack = std::any_cast<Money>(stringAsMoney);

        std::cout << "Converted back to Money: " << convertedBack.toString()
                  << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Money conversion failed: " << e.what() << std::endl;
    }

    //=========================================================================
    // PART 4: Conversion Validation and Introspection
    //=========================================================================
    printHeader("4. Conversion Validation and Introspection");

    // Check possible conversions
    auto checkConversion = [&converter](const TypeInfo& from,
                                        const TypeInfo& to) {
        bool canConvert = converter->canConvert(from, to);
        std::cout << "Can convert " << from.name() << " to " << to.name()
                  << ": " << (canConvert ? "Yes" : "No") << std::endl;
    };

    checkConversion(userType<Circle>(), userType<Shape>());
    checkConversion(userType<Shape>(), userType<Circle>());
    checkConversion(userType<Square>(), userType<Rectangle>());
    checkConversion(userType<Rectangle>(), userType<Square>());
    checkConversion(userType<Timestamp>(), userType<DateTime>());
    checkConversion(userType<Money>(), userType<FormattedString>());
    checkConversion(userType<int>(), userType<double>());  // Not registered

    //=========================================================================
    // PART 5: Error Handling
    //=========================================================================
    printHeader("5. Error Handling");

    printSubHeader("5.1 Invalid Conversion Attempts");

    // Try to convert Shape to Circle (invalid downcasting)
    try {
        auto genericShape = std::make_shared<Shape>();
        // 修复: convert -> convertTo
        std::any shapeAsCircle = converter->convertTo<std::shared_ptr<Circle>>(
            std::any(genericShape));
        std::cout << "This should not happen!" << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    // Try to convert between unregistered types
    try {
        int intValue = 42;
        // 修复: convert -> convertTo
        std::any intAsDouble = converter->convertTo<double>(std::any(intValue));
        std::cout << "This should not happen!" << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    printSubHeader("5.2 Null Pointer Handling");

    // Try to convert null pointer
    try {
        std::shared_ptr<Circle> nullCircle;
        // 修复: convert -> convertTo
        std::any nullCircleAsShape =
            converter->convertTo<std::shared_ptr<Shape>>(std::any(nullCircle));
        auto convertedNullShape =
            std::any_cast<std::shared_ptr<Shape>>(nullCircleAsShape);

        std::cout << "Null pointer conversion succeeded" << std::endl;
        std::cout << "Is converted pointer null? "
                  << (convertedNullShape == nullptr ? "Yes" : "No")
                  << std::endl;
    } catch (const BadConversionException& e) {
        std::cout << "Null pointer conversion failed: " << e.what()
                  << std::endl;
    }

    printSubHeader("5.3 Invalid Custom Conversion");

    // Try to pass invalid format to custom converter
    try {
        FormattedString invalidFormat("Not a money format");
        // 修复: convert -> convertTo
        std::any invalidAsMonty =
            converter->convertTo<Money>(std::any(invalidFormat));
    } catch (const BadConversionException& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    //=========================================================================
    // PART 6: Complex Nested Conversions
    //=========================================================================
    printHeader("6. Complex Nested Conversions");

    // Map of vectors
    std::map<int, std::vector<std::shared_ptr<Circle>>> nestedContainer;
    nestedContainer[1] = std::vector<std::shared_ptr<Circle>>{
        std::make_shared<Circle>(1.1), std::make_shared<Circle>(1.2)};
    nestedContainer[2] =
        std::vector<std::shared_ptr<Circle>>{std::make_shared<Circle>(2.1)};

    std::cout << "Complex nested containers are supported through successive "
                 "conversions."
              << std::endl;
    std::cout << "You would need to convert each level separately:"
              << std::endl;

    // First, convert each vector in the map
    std::map<int, std::any> intermediateMap;
    for (const auto& [key, circleVector] : nestedContainer) {
        try {
            std::any converted =
                converter->convertTo<std::vector<std::shared_ptr<Shape>>>(
                    std::any(circleVector));
            intermediateMap[key] = converted;
        } catch (const BadConversionException& e) {
            std::cout << "Conversion failed for key " << key << ": " << e.what()
                      << std::endl;
        }
    }

    return 0;
}