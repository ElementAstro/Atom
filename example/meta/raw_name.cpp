#include "atom/meta/raw_name.hpp"
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

// Enum for testing raw_name_of_enum
enum class Color { Red, Green, Blue, Yellow };

// Class for testing raw_name_of
class TestClass {
public:
    void testMethod() {}

    struct NestedStruct {
        int value;
    };

    template <typename T>
    class NestedTemplate {
    public:
        T data;
    };
};

// Function for testing raw_name_of with function types
template <typename Ret, typename... Args>
void printFunctionType() {
    std::cout << "Function return type: " << atom::meta::raw_name_of<Ret>()
              << std::endl;

    if constexpr (sizeof...(Args) > 0) {
        std::cout << "Function arguments: ";
        // Fold expression to print all argument types
        ((std::cout << atom::meta::raw_name_of<Args>() << " "), ...);
        std::cout << std::endl;
    } else {
        std::cout << "Function has no arguments" << std::endl;
    }
}

// Template for testing raw_name_of_template
template <typename T, typename U>
class TemplateTest {
public:
    T first;
    U second;
};

// For C++20 member name test
#ifdef ATOM_CPP_20_SUPPORT
struct Person {
    int age;
    std::string name;
    double height;

    void sayHello() {}
};
#endif

// Helper function to print raw names with labels
template <typename T>
void printRawName(const std::string& description) {
    std::cout << std::setw(40) << std::left << description << ": "
              << atom::meta::raw_name_of<T>() << std::endl;
}

// Helper function to print raw names of enum values
template <auto EnumValue>
void printEnumName(const std::string& description) {
    std::cout << std::setw(40) << std::left << description << ": "
              << atom::meta::raw_name_of_enum<EnumValue>() << std::endl;
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Raw Name Library Usage Examples\n";
    std::cout << "=============================================\n\n";

    // 1. Basic type names
    std::cout << "1. BASIC TYPE NAMES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<int>("Integer");
    printRawName<double>("Double");
    printRawName<float>("Float");
    printRawName<char>("Character");
    printRawName<bool>("Boolean");
    printRawName<void>("Void");

    std::cout << std::endl;

    // 2. Qualified type names
    std::cout << "2. QUALIFIED TYPE NAMES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<const int>("Const integer");
    printRawName<volatile double>("Volatile double");
    printRawName<const volatile char>("Const volatile char");
    printRawName<int&>("Integer reference");
    printRawName<const int&>("Const integer reference");
    printRawName<int&&>("Integer rvalue reference");
    printRawName<int*>("Integer pointer");
    printRawName<const int*>("Pointer to const integer");
    printRawName<int* const>("Const pointer to integer");
    printRawName<const int* const>("Const pointer to const integer");

    std::cout << std::endl;

    // 3. Standard library types
    std::cout << "3. STANDARD LIBRARY TYPES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<std::string>("String");
    printRawName<std::vector<int>>("Vector of integers");
    printRawName<std::map<std::string, int>>("Map of string to integer");
    printRawName<std::pair<int, double>>("Pair of int and double");
    printRawName<std::function<void(int)>>(
        "Function taking int, returning void");

    std::cout << std::endl;

    // 4. Array types
    std::cout << "4. ARRAY TYPES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<int[5]>("Fixed size array of 5 integers");
    printRawName<char[10]>("Fixed size array of 10 characters");
    printRawName<int[3][4]>("2D array of integers");
    printRawName<std::array<int, 5>>("std::array of 5 integers");

    std::cout << std::endl;

    // 5. Enum types
    std::cout << "5. ENUM TYPES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<Color>("Color enum class");

    // Enum values
    printEnumName<Color::Red>("Red");
    printEnumName<Color::Green>("Green");
    printEnumName<Color::Blue>("Blue");
    printEnumName<Color::Yellow>("Yellow");

    std::cout << std::endl;

    // 6. Custom class types
    std::cout << "6. CUSTOM CLASS TYPES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<TestClass>("Test class");
    printRawName<TestClass::NestedStruct>("Nested struct");
    printRawName<TestClass::NestedTemplate<int>>("Nested template with int");
    printRawName<TestClass::NestedTemplate<std::string>>(
        "Nested template with string");

    std::cout << std::endl;

    // 7. Function types
    std::cout << "7. FUNCTION TYPES\n";
    std::cout << "-------------------------------------------\n";

    printRawName<void()>("Function returning void with no args");
    printRawName<int(double, char)>(
        "Function returning int taking double and char");
    printRawName<std::string(int, int, int)>(
        "Function returning string taking three ints");

    // Using the function type printer
    std::cout << "Using function type printer:" << std::endl;
    printFunctionType<void>();
    printFunctionType<int, double>();
    printFunctionType<std::string, int, float, char>();

    std::cout << std::endl;

    // 8. Template type names
    std::cout << "8. TEMPLATE TYPE NAMES\n";
    std::cout << "-------------------------------------------\n";

    using IntStringTemplate = TemplateTest<int, std::string>;
    using DoubleVectorTemplate = TemplateTest<double, std::vector<int>>;

    printRawName<IntStringTemplate>("Template with int and string");
    printRawName<DoubleVectorTemplate>("Template with double and vector<int>");

    // Display full template names if available
    std::cout << "Full template name for IntStringTemplate: "
              << atom::meta::template_traits<IntStringTemplate>::full_name
              << std::endl;
    std::cout << "Full template name for DoubleVectorTemplate: "
              << atom::meta::template_traits<DoubleVectorTemplate>::full_name
              << std::endl;

    std::cout << std::endl;

    // 9. Value-based raw names
    std::cout << "9. VALUE-BASED RAW NAMES\n";
    std::cout << "-------------------------------------------\n";

    constexpr int intValue = 42;
    constexpr double doubleValue = 3.14159;
    constexpr char charValue = 'A';

    std::cout << std::setw(40) << std::left << "Integer value 42" << ": "
              << atom::meta::raw_name_of<intValue>() << std::endl;
    std::cout << std::setw(40) << std::left << "Double value 3.14159" << ": "
              << atom::meta::raw_name_of<doubleValue>() << std::endl;
    std::cout << std::setw(40) << std::left << "Character value 'A'" << ": "
              << atom::meta::raw_name_of<charValue>() << std::endl;

    std::cout << std::endl;

    // 10. Compiler-specific behavior
    std::cout << "10. COMPILER-SPECIFIC BEHAVIOR\n";
    std::cout << "-------------------------------------------\n";

#if __GNUC__ && !__clang__
    std::cout << "Using GCC compiler" << std::endl;
#elif __clang__
    std::cout << "Using Clang compiler" << std::endl;
#elif _MSC_VER
    std::cout << "Using MSVC compiler" << std::endl;
#else
    std::cout << "Using unknown compiler" << std::endl;
#endif

    std::cout << "Examples of how names appear in this compiler:" << std::endl;

    // Show a few examples with their raw compiler output
    std::cout << "int: " << atom::meta::raw_name_of<int>() << std::endl;
    std::cout << "std::string: " << atom::meta::raw_name_of<std::string>()
              << std::endl;
    std::cout << "TestClass::NestedStruct: "
              << atom::meta::raw_name_of<TestClass::NestedStruct>()
              << std::endl;

    std::cout << std::endl;

#ifdef ATOM_CPP_20_SUPPORT
    // 11. C++20 Member Access (if supported)
    std::cout << "11. C++20 MEMBER ACCESS (IF SUPPORTED)\n";
    std::cout << "-------------------------------------------\n";

    // Create wrappers for member access
    constexpr atom::meta::Wrapper age(&Person::age);
    constexpr atom::meta::Wrapper name(&Person::name);
    constexpr atom::meta::Wrapper height(&Person::height);
    constexpr atom::meta::Wrapper sayHello(&Person::sayHello);

    // Print member names
    std::cout << "Person::age: " << atom::meta::raw_name_of_member<age>()
              << std::endl;
    std::cout << "Person::name: " << atom::meta::raw_name_of_member<name>()
              << std::endl;
    std::cout << "Person::height: " << atom::meta::raw_name_of_member<height>()
              << std::endl;
    std::cout << "Person::sayHello: "
              << atom::meta::raw_name_of_member<sayHello>() << std::endl;

    std::cout << std::endl;
#else
    std::cout << "11. C++20 MEMBER ACCESS\n";
    std::cout << "-------------------------------------------\n";
    std::cout
        << "C++20 member access not supported with current compiler settings."
        << std::endl;
    std::cout << std::endl;
#endif

    // 12. Practical Examples
    std::cout << "12. PRACTICAL EXAMPLES\n";
    std::cout << "-------------------------------------------\n";

    // Type-based logging example
    auto logType = [](auto&& value) {
        using ValueType = std::decay_t<decltype(value)>;
        std::cout << "Logging value of type "
                  << atom::meta::raw_name_of<ValueType>() << std::endl;
    };

    logType(42);
    logType(3.14);
    logType(std::string("Hello"));
    logType(std::vector<int>{1, 2, 3});

    // Type checking utility
    auto checkType = [](auto&& value, auto&& expectedValue) {
        using ValueType = std::decay_t<decltype(value)>;
        using ExpectedType = std::decay_t<decltype(expectedValue)>;

        if constexpr (std::is_same_v<ValueType, ExpectedType>) {
            std::cout << "Types match: " << atom::meta::raw_name_of<ValueType>()
                      << std::endl;
            return true;
        } else {
            std::cout << "Type mismatch: Expected "
                      << atom::meta::raw_name_of<ExpectedType>() << " but got "
                      << atom::meta::raw_name_of<ValueType>() << std::endl;
            return false;
        }
    };

    checkType(42, 100);      // Should match (both int)
    checkType(3.14, 2.71f);  // Should not match (double vs float)
    checkType(std::string("test"),
              "test");  // Should not match (std::string vs const char*)

    return 0;
}