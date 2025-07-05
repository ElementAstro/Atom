#include "atom/meta/overload.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>

// Example free functions with the same name but different parameters
void print(int value) {
    std::cout << "Free function print(int): " << value << std::endl;
}

void print(double value) {
    std::cout << "Free function print(double): " << value << std::endl;
}

void print(const std::string& value) noexcept {
    std::cout << "Free function print(string) noexcept: " << value << std::endl;
}

// Class with various overloaded methods
class Calculator {
public:
    // Regular member functions
    int add(int a, int b) {
        std::cout << "Regular add(int, int)" << std::endl;
        return a + b;
    }

    double add(double a, double b) {
        std::cout << "Regular add(double, double)" << std::endl;
        return a + b;
    }

    // Const member functions
    int multiply(int a, int b) const {
        std::cout << "Const multiply(int, int)" << std::endl;
        return a * b;
    }

    double multiply(double a, double b) const {
        std::cout << "Const multiply(double, double)" << std::endl;
        return a * b;
    }

    // Volatile member functions
    int subtract(int a, int b) volatile {
        std::cout << "Volatile subtract(int, int)" << std::endl;
        return a - b;
    }

    // Const volatile member functions
    int divide(int a, int b) const volatile {
        std::cout << "Const volatile divide(int, int)" << std::endl;
        return a / b;
    }

    // Noexcept member functions
    int mod(int a, int b) noexcept {
        std::cout << "Noexcept mod(int, int)" << std::endl;
        return a % b;
    }

    // Const noexcept member functions
    double power(double base, int exponent) const noexcept {
        std::cout << "Const noexcept power(double, int)" << std::endl;
        double result = 1.0;
        for (int i = 0; i < exponent; ++i) {
            result *= base;
        }
        return result;
    }

    // Volatile noexcept member functions
    int negate(int value) volatile noexcept {
        std::cout << "Volatile noexcept negate(int)" << std::endl;
        return -value;
    }

    // Const volatile noexcept member functions
    int abs(int value) const volatile noexcept {
        std::cout << "Const volatile noexcept abs(int)" << std::endl;
        return value < 0 ? -value : value;
    }
};

// Template function to demonstrate function pointer type
template <typename FuncPtr>
void showFunctionType(const std::string& name, FuncPtr) {
    std::cout << name << " is"
              << (std::is_nothrow_invocable_v<FuncPtr> ? " noexcept"
                                                       : " not noexcept")
              << std::endl;
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Atom Meta Overload Library Usage Examples\n";
    std::cout << "=============================================\n\n";

    Calculator calc;
    const Calculator constCalc;
    volatile Calculator volatileCalc;
    const volatile Calculator constVolatileCalc;

    // 1. Selecting specific free function overloads
    std::cout << "1. FREE FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    // Using function pointers with manually specified types
    void (*printInt)(int) = &print;
    void (*printDouble)(double) = &print;
    void (*printString)(const std::string&) noexcept = &print;

    printInt(42);
    printDouble(3.14159);
    printString("Hello world");

    // Using overload_cast to select specific free function overloads
    auto printIntFunc = atom::meta::overload_cast<int>(&print);
    auto printDoubleFunc = atom::meta::overload_cast<double>(&print);
    auto printStringFunc =
        atom::meta::overload_cast<const std::string&>(&print);

    printIntFunc(100);
    printDoubleFunc(2.71828);
    printStringFunc("Using overload_cast");

    std::cout << std::endl;

    // 2. Regular member function overloads
    std::cout << "2. REGULAR MEMBER FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    // Using overload_cast to select specific member function overloads
    auto addInt = atom::meta::overload_cast<int, int>(&Calculator::add);
    auto addDouble =
        atom::meta::overload_cast<double, double>(&Calculator::add);

    std::cout << "Result: " << (calc.*addInt)(5, 7) << std::endl;
    std::cout << "Result: " << (calc.*addDouble)(3.5, 2.5) << std::endl;

    std::cout << std::endl;

    // 3. Const member function overloads
    std::cout << "3. CONST MEMBER FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    auto multiplyInt =
        atom::meta::overload_cast<int, int>(&Calculator::multiply);
    auto multiplyDouble =
        atom::meta::overload_cast<double, double>(&Calculator::multiply);

    std::cout << "Result: " << (constCalc.*multiplyInt)(6, 7) << std::endl;
    std::cout << "Result: " << (constCalc.*multiplyDouble)(3.5, 2.0)
              << std::endl;

    std::cout << std::endl;

    // 4. Volatile member function overloads
    std::cout << "4. VOLATILE MEMBER FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    auto subtractInt =
        atom::meta::overload_cast<int, int>(&Calculator::subtract);

    std::cout << "Result: " << (volatileCalc.*subtractInt)(10, 3) << std::endl;

    std::cout << std::endl;

    // 5. Const volatile member function overloads
    std::cout << "5. CONST VOLATILE MEMBER FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    auto divideInt = atom::meta::overload_cast<int, int>(&Calculator::divide);

    std::cout << "Result: " << (constVolatileCalc.*divideInt)(20, 4)
              << std::endl;

    std::cout << std::endl;

    // 6. Noexcept member function overloads
    std::cout << "6. NOEXCEPT MEMBER FUNCTION OVERLOADS\n";
    std::cout << "-------------------------------------------\n";

    auto modInt = atom::meta::overload_cast<int, int>(&Calculator::mod);
    auto powerFunc = atom::meta::overload_cast<double, int>(&Calculator::power);
    auto negateFunc = atom::meta::overload_cast<int>(&Calculator::negate);
    auto absFunc = atom::meta::overload_cast<int>(&Calculator::abs);

    std::cout << "Result: " << (calc.*modInt)(17, 5) << std::endl;
    std::cout << "Result: " << (constCalc.*powerFunc)(2.0, 8) << std::endl;
    std::cout << "Result: " << (volatileCalc.*negateFunc)(42) << std::endl;
    std::cout << "Result: " << (constVolatileCalc.*absFunc)(-15) << std::endl;

    std::cout << std::endl;

    // 7. Verifying noexcept status
    std::cout << "7. VERIFYING NOEXCEPT STATUS\n";
    std::cout << "-------------------------------------------\n";

    showFunctionType("print(int)", printInt);
    showFunctionType("print(string)", printString);
    showFunctionType("Calculator::mod", modInt);
    showFunctionType("Calculator::add", addInt);

    std::cout << std::endl;

    // 8. Using std::function with overload_cast
    std::cout << "8. USING STD::FUNCTION WITH OVERLOAD_CAST\n";
    std::cout << "-------------------------------------------\n";

    std::function<int(Calculator&, int, int)> addFunc = std::bind(
        atom::meta::overload_cast<int, int>(&Calculator::add),
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    std::function<int(Calculator&, int, int)> modFunc = std::bind(
        atom::meta::overload_cast<int, int>(&Calculator::mod),
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    std::cout << "std::function result: " << addFunc(calc, 11, 22) << std::endl;
    std::cout << "std::function result: " << modFunc(calc, 27, 5) << std::endl;

    std::cout << std::endl;

    // 9. Using decayCopy utility
    std::cout << "9. USING DECAY_COPY UTILITY\n";
    std::cout << "-------------------------------------------\n";

    const std::string constString = "This is a const string";
    auto decayedString = atom::meta::decayCopy(constString);

    std::cout
        << "Original type is const: "
        << std::is_const_v<
               std::remove_reference_t<decltype(constString)>> << std::endl;
    std::cout
        << "Decayed type is const: "
        << std::is_const_v<
               std::remove_reference_t<decltype(decayedString)>> << std::endl;

    // Check if the value is preserved
    std::cout << "Original string: " << constString << std::endl;
    std::cout << "Decayed string: " << decayedString << std::endl;

    // Demonstrate decayCopy with temporary values
    auto decayedTemp = atom::meta::decayCopy(std::string("Temporary string"));
    std::cout << "Decayed temporary: " << decayedTemp << std::endl;

    // Demonstrate decayCopy with integers
    const int constInt = 42;
    auto decayedInt = atom::meta::decayCopy(constInt);
    std::cout << "Decayed int: " << decayedInt << std::endl;

    std::cout << std::endl;

    // 10. Practical use case: storing function pointers in a map
    std::cout << "10. PRACTICAL USE CASE: FUNCTION MAP\n";
    std::cout << "-------------------------------------------\n";

    // Create a map of operation name to function pointer
    std::map<std::string, int (Calculator::*)(int, int)> operations;
    operations["add"] = atom::meta::overload_cast<int, int>(&Calculator::add);
    operations["mod"] = atom::meta::overload_cast<int, int>(&Calculator::mod);

    std::cout << "Function map results:\n";
    for (const auto& [name, op] : operations) {
        std::cout << "  " << name << "(5, 3) = " << (calc.*op)(5, 3)
                  << std::endl;
    }

    return 0;
}
