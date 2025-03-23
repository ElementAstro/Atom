/**
 * Comprehensive examples for atom::meta::field_count utilities
 *
 * This file demonstrates the use of fieldCountOf to detect struct fields:
 * 1. Basic structs with different field counts
 * 2. Handling of nested structs
 * 3. Empty structs
 * 4. Structs with array members
 * 5. Custom type_info specializations
 * 6. Edge cases and limitations
 * 7. Non-aggregate type handling
 * 8. Template structs
 * 9. Inheritance scenarios
 * 10. Performance considerations
 */

#include "atom/meta/field_count.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

using namespace atom::meta;

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n=========================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "=========================================================="
              << std::endl;
}

// Helper function to print field count information
template <typename T>
void printFieldCount(const std::string& typeName) {
    std::cout << std::left << std::setw(40) << typeName
              << " | Fields: " << fieldCountOf<T>() << std::endl;
}

//=========================================================================
// 1. Basic structs with different field counts
//=========================================================================

// Empty struct
struct EmptyStruct {};

// Single field struct
struct SingleField {
    int x;
};

// Two fields struct
struct TwoFields {
    int x;
    double y;
};

// Three fields struct
struct ThreeFields {
    int x;
    double y;
    char z;
};

// Many fields struct
struct ManyFields {
    int a;
    float b;
    double c;
    char d;
    unsigned e;
    long f;
    bool g;
    short h;
};

//=========================================================================
// 2. Nested structs
//=========================================================================

// Struct with nested struct as member
struct NestedStruct {
    int x;
    TwoFields nested;
    double z;
};

// Deeply nested struct
struct DeeplyNested {
    int a;
    NestedStruct b;
    ThreeFields c;
};

//=========================================================================
// 3. Structs with array members
//=========================================================================

// Struct with array
struct WithArray {
    int values[5];
    double factor;
};

// Struct with 2D array
struct With2DArray {
    int matrix[3][3];
    char label;
};

//=========================================================================
// 4. Custom type_info specializations
//=========================================================================

struct CustomType {
    // This has some complex structure that the default algorithm might not
    // handle correctly
    int data[10];
    std::pair<int, int> pair;
    // More complex members...
};

// Custom type_info specialization
template <>
struct atom::meta::type_info<CustomType> {
    static constexpr size_t count = 2;  // Manually specify the field count
};

//=========================================================================
// 5. Template structs
//=========================================================================

// Template struct
template <typename T>
struct Wrapper {
    T value;
    double weight;
};

// Template struct with variadic parameters
template <typename... Args>
struct Pack {
    std::tuple<Args...> data;
};

//=========================================================================
// 6. Inheritance scenarios
//=========================================================================

// Base struct
struct Base {
    int base_field;
};

// Derived struct with inheritance
struct Derived : Base {
    double derived_field;
};

// Multiple inheritance
struct MultipleInheritance : Base, TwoFields {
    char additional_field;
};

//=========================================================================
// 7. Edge cases and special cases
//=========================================================================

// Struct with bitfields
struct Bitfields {
    unsigned int a : 1;
    unsigned int b : 2;
    unsigned int c : 3;
};

// Struct with private/protected fields
class WithAccess {
public:
    WithAccess() : public_field(0), private_field(0) {}

    int public_field;

private:
    int private_field;
};

//=========================================================================
// 8. Non-aggregate types
//=========================================================================

// Class with constructor (non-aggregate)
class NonAggregate {
public:
    NonAggregate(int val) : x(val) {}
    int x;
    double y;
};

// Class with virtual function (non-aggregate)
class WithVirtual {
public:
    int a;
    double b;
    virtual void foo() {}
};

//=========================================================================
// Main program
//=========================================================================
int main() {
    std::cout << "================================================="
              << std::endl;
    std::cout << "   Field Count Detection Utility Examples" << std::endl;
    std::cout << "================================================="
              << std::endl;

    //=========================================================================
    // 1. Basic structs with different field counts
    //=========================================================================
    printHeader("1. Basic Structs with Different Field Counts");

    printFieldCount<EmptyStruct>("EmptyStruct");
    printFieldCount<SingleField>("SingleField");
    printFieldCount<TwoFields>("TwoFields");
    printFieldCount<ThreeFields>("ThreeFields");
    printFieldCount<ManyFields>("ManyFields");

    //=========================================================================
    // 2. Nested structs
    //=========================================================================
    printHeader("2. Nested Structs");

    printFieldCount<NestedStruct>("NestedStruct");
    printFieldCount<DeeplyNested>("DeeplyNested");

    std::cout << "\nNote: Nested structs count as single fields in their "
                 "parent struct"
              << std::endl;

    //=========================================================================
    // 3. Structs with array members
    //=========================================================================
    printHeader("3. Structs with Array Members");

    printFieldCount<WithArray>("WithArray");
    printFieldCount<With2DArray>("With2DArray");

    std::cout
        << "\nNote: Arrays count as single fields regardless of dimensions"
        << std::endl;

    //=========================================================================
    // 4. Custom type_info specializations
    //=========================================================================
    printHeader("4. Custom type_info Specializations");

    printFieldCount<CustomType>("CustomType");

    std::cout << "\nNote: Custom type_info specialization used for CustomType"
              << std::endl;
    std::cout
        << "      This allows manual specification of field count when needed"
        << std::endl;

    //=========================================================================
    // 5. Template structs
    //=========================================================================
    printHeader("5. Template Structs");

    printFieldCount<Wrapper<int>>("Wrapper<int>");
    printFieldCount<Wrapper<std::string>>("Wrapper<std::string>");
    printFieldCount<Pack<int, double>>("Pack<int, double>");

    std::cout << "\nNote: Template instantiations are evaluated independently"
              << std::endl;

    //=========================================================================
    // 6. Inheritance scenarios
    //=========================================================================
    printHeader("6. Inheritance Scenarios");

    printFieldCount<Base>("Base");
    printFieldCount<Derived>("Derived");
    printFieldCount<MultipleInheritance>("MultipleInheritance");

    std::cout << "\nNote: Inheritance affects field count detection:"
              << std::endl;
    std::cout << "      - In single inheritance, a derived class with one "
                 "field appears to have one field"
              << std::endl;
    std::cout << "        (rather than two) because the base part is treated "
                 "as one unit"
              << std::endl;
    std::cout << "      - Multiple inheritance creates more complex scenarios"
              << std::endl;

    //=========================================================================
    // 7. Edge cases and special cases
    //=========================================================================
    printHeader("7. Edge Cases and Special Cases");

    printFieldCount<Bitfields>("Bitfields");
    printFieldCount<WithAccess>("WithAccess (class with access specifiers)");

    std::cout
        << "\nNote: Bitfields and access specifiers present interesting cases"
        << std::endl;
    std::cout
        << "      - Bitfields: Each bitfield is counted as a separate field"
        << std::endl;
    std::cout << "      - Private fields are counted (reflect memory layout, "
                 "not accessibility)"
              << std::endl;

    //=========================================================================
    // 8. Non-aggregate types
    //=========================================================================
    printHeader("8. Non-Aggregate Types");

    printFieldCount<NonAggregate>("NonAggregate");
    printFieldCount<WithVirtual>("WithVirtual");

    std::cout << "\nNote: Non-aggregate types (classes with constructors, "
                 "virtual functions, etc.)"
              << std::endl;
    std::cout << "      always return 0 fields with fieldCountOf<>()"
              << std::endl;

    //=========================================================================
    // 9. Demonstration of compile-time evaluation
    //=========================================================================
    printHeader("9. Compile-Time Evaluation");

    // Demonstrate compile-time evaluation
    constexpr auto count = fieldCountOf<ThreeFields>();
    std::cout << "Compile-time field count of ThreeFields: " << count
              << std::endl;

    // Using field count in array size
    std::array<int, fieldCountOf<TwoFields>()> values = {1, 2};
    std::cout << "Created an array with size based on field count: ";
    for (auto val : values) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "\nNote: fieldCountOf<> is a consteval function that "
                 "evaluates at compile time"
              << std::endl;

    //=========================================================================
    // 10. Limitations and warnings
    //=========================================================================
    printHeader("10. Limitations and Warnings");

    std::cout << "Important limitations of fieldCountOf<>:" << std::endl;
    std::cout << "1. Cannot count fields in non-aggregate types (classes with "
                 "constructors, etc.)"
              << std::endl;
    std::cout << "2. May have issues with reference type members in some "
                 "compiler versions"
              << std::endl;
    std::cout
        << "3. Inherited base class fields might not be counted as expected"
        << std::endl;
    std::cout << "4. For complex cases, consider using a custom type_info "
                 "specialization"
              << std::endl;

    return 0;
}