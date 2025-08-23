/**
 * @file comprehensive_meta_example.cpp
 * @brief Comprehensive example demonstrating the Atom Meta module's metaprogramming capabilities
 *
 * This example shows how to:
 * - Use type traits and template metaprogramming
 * - Work with reflection and introspection
 * - Handle function traits and signatures
 * - Use type conversion and casting utilities
 * - Demonstrate advanced metaprogramming patterns
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <type_traits>

// Atom Meta module headers
#include "atom/meta/type_info.hpp"
#include "atom/meta/func_traits.hpp"
#include "atom/meta/any.hpp"

using namespace atom::meta;

// Example classes for demonstration
class Person {
public:
    Person(const std::string& name, int age) : name_(name), age_(age) {}

    std::string getName() const { return name_; }
    int getAge() const { return age_; }
    void setAge(int age) { age_ = age; }

    void introduce() const {
        std::cout << "Hello, I'm " << name_ << " and I'm " << age_ << " years old.\n";
    }

private:
    std::string name_;
    int age_;
};

class Employee : public Person {
public:
    Employee(const std::string& name, int age, const std::string& department)
        : Person(name, age), department_(department) {}

    std::string getDepartment() const { return department_; }

private:
    std::string department_;
};

// Example functions for function traits demonstration
int add(int a, int b) { return a + b; }
double multiply(double x, double y) { return x * y; }
std::string concatenate(const std::string& a, const std::string& b) { return a + b; }

// Helper template function for SFINAE demonstration
template<typename T>
constexpr bool canProcess() {
    if constexpr (std::is_arithmetic_v<T>) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Demonstrates type information and introspection
 */
void typeInfoExample() {
    std::cout << "\n=== Type Information Example ===\n";

    try {
        // Basic type information using TypeInfo
        std::cout << "Type information for built-in types:\n";
        auto intInfo = TypeInfo::fromType<int>();
        auto doubleInfo = TypeInfo::fromType<double>();
        auto stringInfo = TypeInfo::fromType<std::string>();
        auto vectorInfo = TypeInfo::fromType<std::vector<int>>();

        std::cout << "int: " << intInfo.name() << " (bare: " << intInfo.bareName() << ")\n";
        std::cout << "double: " << doubleInfo.name() << " (bare: " << doubleInfo.bareName() << ")\n";
        std::cout << "std::string: " << stringInfo.name() << " (bare: " << stringInfo.bareName() << ")\n";
        std::cout << "std::vector<int>: " << vectorInfo.name() << " (bare: " << vectorInfo.bareName() << ")\n";

        // Custom class type information
        std::cout << "\nType information for custom classes:\n";
        auto personInfo = TypeInfo::fromType<Person>();
        auto employeeInfo = TypeInfo::fromType<Employee>();

        std::cout << "Person: " << personInfo.name() << " (bare: " << personInfo.bareName() << ")\n";
        std::cout << "Employee: " << employeeInfo.name() << " (bare: " << employeeInfo.bareName() << ")\n";

        // Type properties using TypeInfo
        std::cout << "\nType properties:\n";
        std::cout << "int is arithmetic: " << intInfo.isArithmetic() << "\n";
        std::cout << "std::string is arithmetic: " << stringInfo.isArithmetic() << "\n";
        std::cout << "Person is class: " << personInfo.isClass() << "\n";
        std::cout << "int is const: " << intInfo.isConst() << "\n";
        std::cout << "int is pointer: " << intInfo.isPointer() << "\n";
        std::cout << "int is reference: " << intInfo.isReference() << "\n";

        // Type relationships using standard library
        std::cout << "\nType relationships (using std::type_traits):\n";
        std::cout << "Is Employee derived from Person? "
                  << std::is_base_of_v<Person, Employee> << "\n";
        std::cout << "Is Person same as Employee? "
                  << std::is_same_v<Person, Employee> << "\n";

        // JSON representation
        std::cout << "\nJSON representation of int type:\n";
        std::cout << intInfo.toJson() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in type info example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates function traits and signature analysis
 */
void functionTraitsExample() {
    std::cout << "\n=== Function Traits Example ===\n";

    try {
        // Function signature analysis
        std::cout << "Function signature analysis:\n";

        // Analyze free functions
        using AddTraits = FunctionTraits<decltype(add)>;
        std::cout << "add function:\n";
        auto returnTypeInfo = TypeInfo::fromType<AddTraits::return_type>();
        std::cout << "  Return type: " << returnTypeInfo.name() << "\n";
        std::cout << "  Argument count: " << AddTraits::arity << "\n";

        auto arg0Info = TypeInfo::fromType<AddTraits::argument_t<0>>();
        auto arg1Info = TypeInfo::fromType<AddTraits::argument_t<1>>();
        std::cout << "  First argument type: " << arg0Info.name() << "\n";
        std::cout << "  Second argument type: " << arg1Info.name() << "\n";

        using MultiplyTraits = FunctionTraits<decltype(multiply)>;
        std::cout << "\nmultiply function:\n";
        auto multiplyReturnInfo = TypeInfo::fromType<MultiplyTraits::return_type>();
        std::cout << "  Return type: " << multiplyReturnInfo.name() << "\n";
        std::cout << "  Argument count: " << MultiplyTraits::arity << "\n";

        // Analyze lambda functions
        auto lambda = [](int x, double y) -> std::string {
            return std::to_string(x) + " + " + std::to_string(y);
        };

        using LambdaTraits = FunctionTraits<decltype(lambda)>;
        std::cout << "\nlambda function:\n";
        auto lambdaReturnInfo = TypeInfo::fromType<LambdaTraits::return_type>();
        std::cout << "  Return type: " << lambdaReturnInfo.name() << "\n";
        std::cout << "  Argument count: " << LambdaTraits::arity << "\n";

        // Analyze member functions
        using MemberFuncTraits = FunctionTraits<decltype(&Person::getName)>;
        std::cout << "\nPerson::getName member function:\n";
        auto memberReturnInfo = TypeInfo::fromType<MemberFuncTraits::return_type>();
        std::cout << "  Return type: " << memberReturnInfo.name() << "\n";
        std::cout << "  Argument count: " << MemberFuncTraits::arity << "\n";
        std::cout << "  Is member function: " << MemberFuncTraits::is_member_function << "\n";

        // Check function properties
        std::cout << "\nFunction properties:\n";
        std::cout << "add is noexcept: " << AddTraits::is_noexcept << "\n";
        std::cout << "Member function is const: " << MemberFuncTraits::is_const_member_function << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in function traits example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates BoxedValue (Any) usage
 */
void boxedValueExample() {
    std::cout << "\n=== BoxedValue (Any) Example ===\n";

    try {
        // Create BoxedValue instances
        std::cout << "Creating BoxedValue instances:\n";

        BoxedValue intBox(42);
        BoxedValue doubleBox(3.14);
        BoxedValue stringBox(std::string("Hello, World!"));
        BoxedValue personBox(Person("Alice", 30));

        std::cout << "int BoxedValue: " << intBox.getTypeInfo().name() << "\n";
        std::cout << "double BoxedValue: " << doubleBox.getTypeInfo().name() << "\n";
        std::cout << "string BoxedValue: " << stringBox.getTypeInfo().name() << "\n";
        std::cout << "Person BoxedValue: " << personBox.getTypeInfo().name() << "\n";

        // Type checking
        std::cout << "\nType checking:\n";
        std::cout << "intBox can cast to int: " << intBox.canCast<int>() << "\n";
        std::cout << "stringBox can cast to string: " << stringBox.canCast<std::string>() << "\n";
        std::cout << "intBox can cast to string: " << intBox.canCast<std::string>() << "\n";

        // Value extraction using tryCast
        std::cout << "\nValue extraction using tryCast:\n";
        if (auto intValue = intBox.tryCast<int>()) {
            std::cout << "Extracted int value: " << *intValue << "\n";
        } else {
            std::cout << "Failed to extract int value\n";
        }

        if (auto stringValue = stringBox.tryCast<std::string>()) {
            std::cout << "Extracted string value: " << *stringValue << "\n";
        } else {
            std::cout << "Failed to extract string value\n";
        }

        if (auto personValue = personBox.tryCast<Person>()) {
            std::cout << "Extracted person name: " << personValue->getName() << "\n";
        } else {
            std::cout << "Failed to extract person value\n";
        }

        // Type safety - this should fail
        std::cout << "\nTesting type safety (should fail):\n";
        if (auto wrongType = intBox.tryCast<std::string>()) {
            std::cout << "This shouldn't print: " << *wrongType << "\n";
        } else {
            std::cout << "Expected: Failed to cast int to string\n";
        }

        // Debug string representation
        std::cout << "\nDebug representations:\n";
        std::cout << "intBox: " << intBox.debugString() << "\n";
        std::cout << "doubleBox: " << doubleBox.debugString() << "\n";
        std::cout << "stringBox: " << stringBox.debugString() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in BoxedValue example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates template metaprogramming with standard library
 */
void templateMetaprogrammingExample() {
    std::cout << "\n=== Template Metaprogramming Example ===\n";

    try {
        // Type traits from standard library
        std::cout << "Standard library type traits:\n";

        std::cout << "std::is_integral<int>: " << std::is_integral_v<int> << "\n";
        std::cout << "std::is_floating_point<double>: " << std::is_floating_point_v<double> << "\n";
        std::cout << "std::is_class<Person>: " << std::is_class_v<Person> << "\n";
        std::cout << "std::is_polymorphic<Person>: " << std::is_polymorphic_v<Person> << "\n";

        // Type relationships
        std::cout << "\nType relationships:\n";
        std::cout << "std::is_base_of<Person, Employee>: " << std::is_base_of_v<Person, Employee> << "\n";
        std::cout << "std::is_same<int, int>: " << std::is_same_v<int, int> << "\n";
        std::cout << "std::is_same<int, double>: " << std::is_same_v<int, double> << "\n";

        // Type modifications
        std::cout << "\nType modifications:\n";
        using IntPtr = std::add_pointer_t<int>;
        using IntRef = std::add_lvalue_reference_t<int>;
        using ConstInt = std::add_const_t<int>;

        auto intPtrInfo = TypeInfo::fromType<IntPtr>();
        auto intRefInfo = TypeInfo::fromType<IntRef>();
        auto constIntInfo = TypeInfo::fromType<ConstInt>();

        std::cout << "int* type: " << intPtrInfo.name() << "\n";
        std::cout << "int& type: " << intRefInfo.name() << "\n";
        std::cout << "const int type: " << constIntInfo.name() << "\n";

        // Conditional types
        std::cout << "\nConditional types:\n";
        using ConditionalType1 = std::conditional_t<true, int, double>;
        using ConditionalType2 = std::conditional_t<false, int, double>;

        auto cond1Info = TypeInfo::fromType<ConditionalType1>();
        auto cond2Info = TypeInfo::fromType<ConditionalType2>();

        std::cout << "std::conditional_t<true, int, double>: " << cond1Info.name() << "\n";
        std::cout << "std::conditional_t<false, int, double>: " << cond2Info.name() << "\n";

        // SFINAE example with enable_if
        std::cout << "\nSFINAE demonstration:\n";
        std::cout << "Can process int: " << canProcess<int>() << "\n";
        std::cout << "Can process double: " << canProcess<double>() << "\n";
        std::cout << "Can process std::string: " << canProcess<std::string>() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in template metaprogramming example: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all meta capabilities
 */
int main() {
    std::cout << "=== Atom Meta Module Comprehensive Example ===\n";
    std::cout << "Demonstrating metaprogramming and reflection capabilities...\n";

    try {
        // Run all examples
        typeInfoExample();
        functionTraitsExample();
        boxedValueExample();
        templateMetaprogrammingExample();

        std::cout << "\n=== All Examples Completed Successfully ===\n";
        std::cout << "The meta module provides:\n";
        std::cout << "  ✓ Type information and introspection\n";
        std::cout << "  ✓ Function traits and signature analysis\n";
        std::cout << "  ✓ Safe type conversion utilities\n";
        std::cout << "  ✓ Advanced template metaprogramming\n";
        std::cout << "  ✓ Compile-time computations\n";
        std::cout << "  ✓ Type list operations\n";
        std::cout << "  ✓ SFINAE and template specialization\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
