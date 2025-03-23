/**
 * Comprehensive examples for atom::meta::func_traits utilities
 *
 * This file demonstrates all function traits functionality:
 * 1. Free functions
 * 2. Member functions
 * 3. Lambdas and functors
 * 4. std::function
 * 5. Function qualifiers (const, volatile, ref, noexcept)
 * 6. Variadic functions
 * 7. Function pipes
 * 8. Method detection
 * 9. Function type inspection
 * 10. Practical applications
 */

#include "atom/meta/func_traits.hpp"
#include <any>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

using namespace atom::meta;

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n=========================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "=========================================================="
              << std::endl;
}

// Helper template for printing function trait information
template <typename F>
void printTraits(const std::string& funcName) {
    using Traits = FunctionTraits<std::decay_t<F>>;

    std::cout << std::left << std::setw(30) << funcName << " | ";
    std::cout << "Return: "
              << DemangleHelper::demangle(
                     typeid(typename Traits::return_type).name());
    std::cout << " | Args: " << Traits::arity;

    if constexpr (Traits::is_member_function) {
        std::cout << " | Member of: "
                  << DemangleHelper::demangle(
                         typeid(typename Traits::class_type).name());
    }

    if constexpr (Traits::is_const_member_function) {
        std::cout << " | const";
    }

    if constexpr (Traits::is_volatile_member_function) {
        std::cout << " | volatile";
    }

    if constexpr (Traits::is_lvalue_reference_member_function) {
        std::cout << " | &";
    }

    if constexpr (Traits::is_rvalue_reference_member_function) {
        std::cout << " | &&";
    }

    if constexpr (Traits::is_noexcept) {
        std::cout << " | noexcept";
    }

    if constexpr (Traits::is_variadic) {
        std::cout << " | variadic";
    }

    std::cout << std::endl;

    // Print argument types if there are any
    if constexpr (Traits::arity > 0) {
        std::cout << "  Arguments: ";
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((std::cout
              << (I > 0 ? ", " : "")
              << DemangleHelper::demangle(
                     typeid(typename Traits::template argument_t<I>).name())),
             ...);
        }(std::make_index_sequence<Traits::arity>{});
        std::cout << std::endl;
    }

    std::cout << "  Function type: " << Traits::full_name << std::endl;
}

//===========================================================================
// 1. Example free functions with different signatures
//===========================================================================

// Basic free function
int add(int a, int b) { return a + b; }

// Function with multiple arguments
double calculate(int a, double b, float c, const std::string& d) {
    return a + b + c + d.size();
}

// Function with no arguments
void noArgs() { std::cout << "No arguments function" << std::endl; }

// Variadic function
int sum(int first, ...) { return first; }

// Noexcept function
void safeFunction() noexcept {
    // This never throws
}

//===========================================================================
// 2. Example class with member functions
//===========================================================================

class ExampleClass {
public:
    // Regular member function
    int method(int x, double y) { return x + static_cast<int>(y); }

    // Const member function
    double constMethod(float x) const { return x * 2.0; }

    // Volatile member function
    void volatileMethod(const std::string& s) volatile {
        std::cout << "Volatile method: " << s << std::endl;
    }

    // Const volatile member function
    int constVolatileMethod(int x, int y) const volatile { return x + y; }

    // Reference qualified member functions
    void lvalueMethod(double d) & {
        std::cout << "lvalue method: " << d << std::endl;
    }

    void rvalueMethod(double d) && {
        std::cout << "rvalue method: " << d << std::endl;
    }

    // Noexcept member function
    void safeMethod() const noexcept {
        // This never throws
    }

    // Member function with multiple qualifiers
    int complexMethod(int x) const volatile&& noexcept { return x * 2; }

    // Static method
    static std::string staticMethod(int count, char c) {
        return std::string(count, c);
    }
};

//===========================================================================
// 3. Example functors and lambdas
//===========================================================================

// Functor example
struct Multiplier {
    double factor;

    Multiplier(double f) : factor(f) {}

    double operator()(double x) const { return x * factor; }
};

//===========================================================================
// Function pipe example
//===========================================================================

// Simple function to use with the function pipe
int multiply(int a, int b) { return a * b; }

//===========================================================================
// Method detection examples
//===========================================================================

// Classes for method detection
class HasPrintMethod {
public:
    void print(const std::string& message) const {
        std::cout << "Message: " << message << std::endl;
    }

    static void staticPrint(const std::string& message) {
        std::cout << "Static message: " << message << std::endl;
    }
};

class NoPrintMethod {
public:
    void display(const std::string& message) const {
        std::cout << "Display: " << message << std::endl;
    }
};

// Define method detectors
DEFINE_HAS_METHOD(print);
DEFINE_HAS_STATIC_METHOD(staticPrint);
DEFINE_HAS_CONST_METHOD(print);

// 添加函数特化
namespace atom::meta {
// 特化 FunctionTraits 为 noexcept 函数
template <typename R, typename... Args>
struct FunctionTraits<R(Args...) noexcept> : FunctionTraitsBase<R, Args...> {
    static constexpr bool is_noexcept = true;
    static constexpr std::string_view full_name = "R(Args...) noexcept";
};

// 特化 FunctionTraits 为 noexcept 函数指针
template <typename R, typename... Args>
struct FunctionTraits<R (*)(Args...) noexcept>
    : FunctionTraitsBase<R, Args...> {
    static constexpr bool is_noexcept = true;
    static constexpr std::string_view full_name = "R(*)(Args...) noexcept";
};

// 添加变参函数特化
template <typename R, typename Arg>
struct FunctionTraits<R(Arg, ...)> : FunctionTraitsBase<R, Arg> {
    static constexpr bool is_variadic = true;
    static constexpr std::string_view full_name = "R(Arg, ...)";
};

// 添加变参函数指针特化
template <typename R, typename Arg>
struct FunctionTraits<R (*)(Arg, ...)> : FunctionTraitsBase<R, Arg> {
    static constexpr bool is_variadic = true;
    static constexpr std::string_view full_name = "R(*)(Arg, ...)";
};
}  // namespace atom::meta

//===========================================================================
// Main function with examples
//===========================================================================

int main() {
    std::cout << "================================================="
              << std::endl;
    std::cout << "   Function Traits Utility Examples" << std::endl;
    std::cout << "================================================="
              << std::endl;

    //=========================================================================
    // 1. Free Functions
    //=========================================================================
    printHeader("1. Free Functions");

    printTraits<decltype(add)>("add");
    printTraits<decltype(calculate)>("calculate");
    printTraits<decltype(noArgs)>("noArgs");
    printTraits<decltype(sum)>("sum (variadic)");
    printTraits<decltype(safeFunction)>("safeFunction (noexcept)");

    // Using function trait variables
    std::cout << "\nFunction trait variables example:" << std::endl;
    std::cout << "add is_noexcept: " << std::boolalpha
              << is_noexcept_v<decltype(add)> << std::endl;
    std::cout << "safeFunction is_noexcept: "
              << is_noexcept_v<decltype(safeFunction)> << std::endl;
    std::cout << "sum is_variadic: "
              << is_variadic_v<decltype(sum)> << std::endl;

    //=========================================================================
    // 2. Member Functions
    //=========================================================================
    printHeader("2. Member Functions");

    printTraits<decltype(&ExampleClass::method)>("ExampleClass::method");
    printTraits<decltype(&ExampleClass::constMethod)>(
        "ExampleClass::constMethod");
    printTraits<decltype(&ExampleClass::volatileMethod)>(
        "ExampleClass::volatileMethod");
    printTraits<decltype(&ExampleClass::constVolatileMethod)>(
        "ExampleClass::constVolatileMethod");
    printTraits<decltype(&ExampleClass::lvalueMethod)>(
        "ExampleClass::lvalueMethod");
    printTraits<decltype(&ExampleClass::rvalueMethod)>(
        "ExampleClass::rvalueMethod");
    printTraits<decltype(&ExampleClass::safeMethod)>(
        "ExampleClass::safeMethod");
    printTraits<decltype(&ExampleClass::complexMethod)>(
        "ExampleClass::complexMethod");
    printTraits<decltype(&ExampleClass::staticMethod)>(
        "ExampleClass::staticMethod");

    // Checking qualifiers
    std::cout << "\nMember function qualifiers example:" << std::endl;
    std::cout << "constMethod is_const: "
              << is_const_member_function_v<
                     decltype(&ExampleClass::constMethod)> << std::endl;
    std::cout << "volatileMethod is_volatile: "
              << is_volatile_member_function_v<
                     decltype(&ExampleClass::volatileMethod)> << std::endl;
    std::cout << "lvalueMethod is_lvalue_ref: "
              << is_lvalue_reference_member_function_v<
                     decltype(&ExampleClass::lvalueMethod)> << std::endl;
    std::cout << "rvalueMethod is_rvalue_ref: "
              << is_rvalue_reference_member_function_v<
                     decltype(&ExampleClass::rvalueMethod)> << std::endl;

    //=========================================================================
    // 3. Lambdas and Functors
    //=========================================================================
    printHeader("3. Lambdas and Functors");

    // Lambda examples
    auto simpleCapturelessLambda = [](int x, int y) { return x + y; };
    auto capturingLambda = [factor = 2.0](double x) { return x * factor; };
    auto mutableLambda = [count = 0](int increment) mutable {
        count += increment;
        return count;
    };
    auto genericLambda = [](auto x, auto y) { return x + y; };

    printTraits<decltype(simpleCapturelessLambda)>("capturelessLambda");
    printTraits<decltype(capturingLambda)>("capturingLambda");
    printTraits<decltype(mutableLambda)>("mutableLambda");

    // Functor example
    Multiplier doubler(2.0);
    printTraits<decltype(doubler)>("Multiplier functor");

    std::cout << "\nLambda usage example:" << std::endl;
    std::cout << "simpleCapturelessLambda(5, 7): "
              << simpleCapturelessLambda(5, 7) << std::endl;
    std::cout << "capturingLambda(3.5): " << capturingLambda(3.5) << std::endl;
    std::cout << "mutableLambda call 1 (5): " << mutableLambda(5) << std::endl;
    std::cout << "mutableLambda call 2 (3): " << mutableLambda(3) << std::endl;

    //=========================================================================
    // 4. std::function
    //=========================================================================
    printHeader("4. std::function");

    std::function<int(int, int)> funcAdd = add;
    std::function<void()> funcNoArgs = noArgs;
    std::function<double(double)> funcMultiplier = doubler;

    printTraits<decltype(funcAdd)>("std::function<int(int,int)>");
    printTraits<decltype(funcNoArgs)>("std::function<void()>");
    printTraits<decltype(funcMultiplier)>("std::function<double(double)>");

    std::cout << "\nstd::function usage example:" << std::endl;
    std::cout << "funcAdd(10, 20): " << funcAdd(10, 20) << std::endl;
    std::cout << "funcMultiplier(4.2): " << funcMultiplier(4.2) << std::endl;

    //=========================================================================
    // 5. Function Pointers
    //=========================================================================
    printHeader("5. Function Pointers");

    int (*funcPtr1)(int, int) = add;
    void (*funcPtr2)() = noArgs;
    void (*funcPtr3)() noexcept = safeFunction;

    printTraits<decltype(funcPtr1)>("int(*)(int,int)");
    printTraits<decltype(funcPtr2)>("void(*)()");
    printTraits<decltype(funcPtr3)>("void(*)() noexcept");

    std::cout << "\nFunction pointer usage example:" << std::endl;
    std::cout << "funcPtr1(15, 25): " << funcPtr1(15, 25) << std::endl;
    std::cout << "Calling funcPtr2(): ";
    funcPtr2();

    //=========================================================================
    // 6. Function Pipe Example
    //=========================================================================
    printHeader("6. Function Pipe Example");

    // 修复: 明确指定模板参数
    auto multiplyPipe = function_pipe<decltype(multiply)>(multiply);

    // Use the pipe operator to call the function
    int result = 5 | multiplyPipe(10);

    std::cout << "Function pipe example: 5 | multiplyPipe(10) = " << result
              << std::endl;

    // Advanced pipe example with multiple operations
    // Example of direct instantiation
    auto addLambda = [](int a, int b) { return a + b; };
    auto addPipe = atom::meta::function_pipe<int(int, int)>(addLambda);

    auto multiplyLambda = [](int a, int factor) { return a * factor; };
    auto multiplyByTwoPipe =
        atom::meta::function_pipe<int(int, int)>(multiplyLambda);

    int pipeResult = 10 | addPipe(5) | multiplyByTwoPipe(2);
    std::cout << "Chained pipes: 10 | addPipe(5) | multiplyByTwoPipe(2) = "
              << pipeResult << std::endl;

    //=========================================================================
    // 7. Method Detection
    //=========================================================================
    printHeader("7. Method Detection");

    // Check for methods in different classes
    std::cout << "HasPrintMethod has print method: " << std::boolalpha
              << has_print<HasPrintMethod, void, const std::string&>::value
              << std::endl;

    std::cout << "NoPrintMethod has print method: "
              << has_print<NoPrintMethod, void, const std::string&>::value
              << std::endl;

    std::cout
        << "HasPrintMethod has const print method: "
        << has_const_print<HasPrintMethod, void, const std::string&>::value
        << std::endl;

    std::cout << "HasPrintMethod has staticPrint method: "
              << has_static_staticPrint<HasPrintMethod, void,
                                        const std::string&>::value
              << std::endl;

    std::cout << "NoPrintMethod has staticPrint method: "
              << has_static_staticPrint<NoPrintMethod, void,
                                        const std::string&>::value
              << std::endl;

    //=========================================================================
    // 8. Reference Detection in Arguments
    //=========================================================================
    printHeader("8. Reference Detection in Arguments");

    // Functions with reference arguments
    auto refFunc = [](int& x) { x *= 2; };
    auto constRefFunc = [](const std::string& s) { return s.size(); };
    auto noRefFunc = [](int x) { return x * 2; };

    using RefFuncTraits = FunctionTraits<decltype(refFunc)>;
    using ConstRefFuncTraits = FunctionTraits<decltype(constRefFunc)>;
    using NoRefFuncTraits = FunctionTraits<decltype(noRefFunc)>;

    std::cout << "refFunc has reference argument: "
              << tuple_has_reference<typename RefFuncTraits::argument_types>()
              << std::endl;

    std::cout
        << "constRefFunc has reference argument: "
        << tuple_has_reference<typename ConstRefFuncTraits::argument_types>()
        << std::endl;

    std::cout << "noRefFunc has reference argument: "
              << tuple_has_reference<typename NoRefFuncTraits::argument_types>()
              << std::endl;

    //=========================================================================
    // 9. Practical Application - Function Wrapper
    //=========================================================================
    printHeader("9. Practical Application - Function Wrapper");

    // Example of a generic function wrapper that logs function calls
    auto wrapFunction = []<typename Func>(Func&& f,
                                          const std::string& funcName) {
        using Traits = FunctionTraits<std::decay_t<Func>>;

        return [f = std::forward<Func>(f), funcName](auto&&... args) ->
               typename Traits::return_type {
                   std::cout << "Calling function '" << funcName << "' with "
                             << sizeof...(args) << " arguments" << std::endl;

                   if constexpr (std::is_void_v<typename Traits::return_type>) {
                       f(std::forward<decltype(args)>(args)...);
                       std::cout << "Function '" << funcName
                                 << "' returned void" << std::endl;
                   } else {
                       auto result = f(std::forward<decltype(args)>(args)...);
                       std::cout << "Function '" << funcName
                                 << "' returned a value" << std::endl;
                       return result;
                   }
               };
    };

    // Wrap some functions
    auto wrappedAdd = wrapFunction(add, "add");
    auto wrappedNoArgs = wrapFunction(noArgs, "noArgs");

    // Call the wrapped functions
    std::cout << "\nWrapped function calls:" << std::endl;
    int addResult = wrappedAdd(3, 4);
    std::cout << "Result: " << addResult << std::endl;

    wrappedNoArgs();

    //=========================================================================
    // 10. Practical Application - Dynamic Dispatcher
    //=========================================================================
    printHeader("10. Practical Application - Dynamic Dispatcher");

    // Function registry with different signatures
    std::unordered_map<std::string, std::any> functionRegistry;

    // Register some functions with different signatures
    functionRegistry["add"] = std::function<int(int, int)>(add);
    functionRegistry["multiply"] = std::function<int(int, int)>(multiply);
    functionRegistry["print"] =
        std::function<void(const std::string&)>([](const std::string& msg) {
            std::cout << "Message: " << msg << std::endl;
        });

    // Function to execute a registered function with correct signature
    auto executeFunction = [&functionRegistry](const std::string& name,
                                               auto&&... args) {
        // 修复: 使用 std::invoke_result_t 代替 declval+decltype
        using ArgTypes = std::tuple<std::decay_t<decltype(args)>...>;
        using ReturnType = std::invoke_result_t<
            std::function<void(std::decay_t<decltype(args)>...)>,
            std::decay_t<decltype(args)>...>;
        using ExpectedFuncType =
            std::function<ReturnType(std::decay_t<decltype(args)>...)>;

        try {
            if (functionRegistry.find(name) == functionRegistry.end()) {
                std::cout << "Function '" << name << "' not found in registry"
                          << std::endl;
                return;
            }

            // Try to get the function with the expected signature
            auto& funcAny = functionRegistry[name];
            if (const auto* func = std::any_cast<ExpectedFuncType>(&funcAny)) {
                // Execute the function with provided arguments
                std::cout << "Executing function '" << name << "'" << std::endl;
                (*func)(std::forward<decltype(args)>(args)...);
            } else {
                std::cout << "Function '" << name
                          << "' has incompatible signature" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    };

    // Example usage
    std::cout << "\nDynamic dispatcher examples:" << std::endl;
    executeFunction("add", 10, 20);
    executeFunction("multiply", 5, 6);
    executeFunction("print", std::string("Hello, function traits!"));
    executeFunction("nonexistent", 1, 2);
    executeFunction("add", "wrong", "types");  // Intentional type mismatch

    return 0;
}