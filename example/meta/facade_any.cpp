/**
 * @file facade_any.cpp
 * @brief Comprehensive example demonstrating the EnhancedBoxedValue facade
 * system
 *
 * This example shows how to:
 * - Use EnhancedBoxedValue for advanced type erasure
 * - Work with facade-based any types
 * - Demonstrate enhanced capabilities over regular BoxedValue
 * - Show serialization, cloning, and comparison features
 * - Use callable and printable capabilities
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <any>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Atom Meta facade headers
#include "atom/meta/any.hpp"
#include "atom/meta/facade_any.hpp"

using namespace atom::meta;

// Example classes for demonstration
class Person {
public:
    Person(const std::string& name, int age) : name_(name), age_(age) {}

    std::string getName() const { return name_; }
    int getAge() const { return age_; }

    std::string toString() const {
        return "Person{name: " + name_ + ", age: " + std::to_string(age_) + "}";
    }

    bool operator==(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }

    void print(std::ostream& os) const { os << toString(); }

private:
    std::string name_;
    int age_;
};

class Calculator {
public:
    Calculator(int initial = 0) : value_(initial) {}

    int operator()() const { return value_; }
    int operator()(int add) const { return value_ + add; }

    std::string toString() const {
        return "Calculator{value: " + std::to_string(value_) + "}";
    }

    void print(std::ostream& os) const { os << toString(); }

    Calculator clone() const { return Calculator(value_); }

private:
    int value_;
};

/**
 * @brief Demonstrates basic EnhancedBoxedValue usage
 */
void basicEnhancedBoxedValueExample() {
    std::cout << "\n=== Basic EnhancedBoxedValue Example ===\n";

    try {
        // Create enhanced boxed values
        EnhancedBoxedValue person_value(Person("Alice", 30));
        EnhancedBoxedValue calc_value(Calculator(42));
        EnhancedBoxedValue int_value(123);
        EnhancedBoxedValue string_value(std::string("Hello World"));

        std::cout << "Created enhanced boxed values\n";

        // Test string conversion capability
        std::cout << "String representations:\n";
        std::cout << "  Person: " << person_value.toString() << "\n";
        std::cout << "  Calculator: " << calc_value.toString() << "\n";
        std::cout << "  Integer: " << int_value.toString() << "\n";
        std::cout << "  String: " << string_value.toString() << "\n";

        // Test printable capability
        std::cout << "Printable capability:\n";
        std::cout << "  Person: ";
        person_value.print(std::cout);
        std::cout << "\n";

        std::cout << "  Calculator: ";
        calc_value.print(std::cout);
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic enhanced boxed value example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates comparison and equality features
 */
void comparisonExample() {
    std::cout << "\n=== Comparison Example ===\n";

    try {
        // Create enhanced boxed values for comparison
        EnhancedBoxedValue person1(Person("Alice", 30));
        EnhancedBoxedValue person2(Person("Alice", 30));
        EnhancedBoxedValue person3(Person("Bob", 25));

        std::cout << "Created enhanced boxed values for comparison\n";

        // Test equality
        std::cout << "Equality tests:\n";
        std::cout << "  person1 == person2: " << person1.equals(person2)
                  << "\n";
        std::cout << "  person1 == person3: " << person1.equals(person3)
                  << "\n";

        // Test with different types
        EnhancedBoxedValue int1(42);
        EnhancedBoxedValue int2(42);
        EnhancedBoxedValue int3(24);

        std::cout << "  int1 == int2: " << int1.equals(int2) << "\n";
        std::cout << "  int1 == int3: " << int1.equals(int3) << "\n";
        std::cout << "  person1 == int1: " << person1.equals(int1) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in comparison example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates cloning capabilities
 */
void cloningExample() {
    std::cout << "\n=== Cloning Example ===\n";

    try {
        // Create enhanced boxed values
        EnhancedBoxedValue calc_value(Calculator(100));
        EnhancedBoxedValue person_value(Person("Charlie", 35));

        std::cout << "Original values:\n";
        std::cout << "  Calculator: " << calc_value.toString() << "\n";
        std::cout << "  Person: " << person_value.toString() << "\n";

        // Test cloning
        auto calc_clone = calc_value.clone();
        auto person_clone = person_value.clone();

        std::cout << "Cloned values:\n";
        if (calc_clone) {
            std::cout << "  Calculator clone: Available\n";
        } else {
            std::cout << "  Calculator clone: Not available\n";
        }

        if (person_clone) {
            std::cout << "  Person clone: Available\n";
        } else {
            std::cout << "  Person clone: Not available\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in cloning example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates callable capabilities
 */
void callableExample() {
    std::cout << "\n=== Callable Example ===\n";

    try {
        // Create callable enhanced boxed values
        EnhancedBoxedValue calc_value(Calculator(50));

        std::cout << "Created callable enhanced boxed value\n";

        // Test calling without arguments
        std::cout << "Calling calculator without arguments:\n";
        auto result1 = calc_value.call({});
        if (result1.has_value()) {
            try {
                int value = std::any_cast<int>(result1);
                std::cout << "  Result: " << value << "\n";
            } catch (const std::bad_any_cast&) {
                std::cout << "  Result: [type conversion failed]\n";
            }
        } else {
            std::cout << "  No result returned\n";
        }

        // Test calling with arguments
        std::cout << "Calling calculator with argument 25:\n";
        auto result2 = calc_value.call({std::any(25)});
        if (result2.has_value()) {
            try {
                int value = std::any_cast<int>(result2);
                std::cout << "  Result: " << value << "\n";
            } catch (const std::bad_any_cast&) {
                std::cout << "  Result: [type conversion failed]\n";
            }
        } else {
            std::cout << "  No result returned\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in callable example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates JSON conversion capabilities
 */
void serializationExample() {
    std::cout << "\n=== JSON Conversion Example ===\n";

    try {
        // Create enhanced boxed values
        EnhancedBoxedValue person_value(Person("David", 40));
        EnhancedBoxedValue int_value(999);
        EnhancedBoxedValue string_value(std::string("Test String"));

        std::cout << "Created enhanced boxed values for JSON conversion\n";

        // Test JSON conversion
        std::cout << "JSON conversion results:\n";
        std::cout << "  Person: " << person_value.toJson() << "\n";
        std::cout << "  Integer: " << int_value.toJson() << "\n";
        std::cout << "  String: " << string_value.toJson() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in JSON conversion example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates comparison with regular BoxedValue
 */
void comparisonWithBoxedValueExample() {
    std::cout << "\n=== Comparison with BoxedValue Example ===\n";

    try {
        // Create regular BoxedValue
        BoxedValue regular_value(Person("Eve", 28));

        // Create EnhancedBoxedValue
        EnhancedBoxedValue enhanced_value(Person("Eve", 28));

        std::cout << "Created both regular and enhanced boxed values\n";

        // Compare capabilities
        std::cout << "Capability comparison:\n";

        // Regular BoxedValue capabilities
        std::cout << "  Regular BoxedValue:\n";
        std::cout << "    Debug string: " << regular_value.debugString()
                  << "\n";
        std::cout << "    Type info: " << regular_value.getTypeInfo().name()
                  << "\n";

        // Enhanced BoxedValue capabilities
        std::cout << "  Enhanced BoxedValue:\n";
        std::cout << "    String representation: " << enhanced_value.toString()
                  << "\n";
        std::cout << "    JSON conversion: " << enhanced_value.toJson() << "\n";
        std::cout << "    Has proxy: " << enhanced_value.hasProxy() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in comparison with BoxedValue example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating all EnhancedBoxedValue capabilities
 */
int main() {
    std::cout << "================================================\n";
    std::cout << "  Atom Meta EnhancedBoxedValue Examples\n";
    std::cout << "================================================\n";

    try {
        basicEnhancedBoxedValueExample();
        comparisonExample();
        cloningExample();
        callableExample();
        serializationExample();
        comparisonWithBoxedValueExample();

        std::cout << "\n=== All EnhancedBoxedValue Examples Completed "
                     "Successfully ===\n";
        std::cout << "The EnhancedBoxedValue provides:\n";
        std::cout << "  ✓ Advanced type erasure with facade pattern\n";
        std::cout << "  ✓ Enhanced string conversion capabilities\n";
        std::cout << "  ✓ Object comparison and equality testing\n";
        std::cout << "  ✓ Cloning support for copyable types\n";
        std::cout << "  ✓ Callable interface for function objects\n";
        std::cout << "  ✓ Serialization and JSON conversion\n";
        std::cout << "  ✓ Printable interface for output streams\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
