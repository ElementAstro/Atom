#include "atom/meta/proxy.hpp"
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Simple free functions for demonstration
int add(int a, int b) { return a + b; }

double multiply(double a, double b) { return a * b; }

std::string concatenate(const std::string& a, const std::string& b) {
    return a + b;
}

void printMessage(const std::string& message) {
    std::cout << "Message: " << message << std::endl;
}

int incrementAndReturn(int& value) {
    value++;
    return value;
}

// Noexcept function example
bool isPositive(int value) noexcept { return value > 0; }

// Function that might throw
double divide(double a, double b) {
    if (b == 0.0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}

// A class with member functions
class Calculator {
public:
    Calculator() : result_(0) {}

    int add(int a, int b) {
        result_ = a + b;
        return result_;
    }

    double multiply(double a, double b) const { return a * b; }

    int getResult() const { return result_; }

    void reset() { result_ = 0; }

    // Noexcept member function
    bool hasResult() const noexcept { return result_ != 0; }

private:
    int result_;
};

// A long-running function for async examples
int slowCalculation(int a, int b) {
    std::cout << "Starting slow calculation..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Finished slow calculation" << std::endl;
    return a * b;
}

// Function that transforms the result of another function
std::string formatResult(int value) {
    return "Result: " + std::to_string(value);
}

// Helper function to print JSON with indentation
void printJson(const nlohmann::json& j) {
    std::cout << std::setw(4) << j << std::endl;
}

// Helper function for error handling
template <typename Func>
void tryOperation(const std::string& description, Func operation) {
    std::cout << "Attempting: " << description << std::endl;
    try {
        operation();
        std::cout << "  Success!" << std::endl;
    } catch (const atom::meta::ProxyTypeError& e) {
        std::cout << "  ProxyTypeError: " << e.what() << std::endl;
    } catch (const atom::meta::ProxyArgumentError& e) {
        std::cout << "  ProxyArgumentError: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Proxy Function Library Usage Examples\n";
    std::cout << "=============================================\n\n";

    // 1. Basic function proxying
    std::cout << "1. BASIC FUNCTION PROXYING\n";
    std::cout << "-------------------------------------------\n";

    // Create a proxy for the add function
    auto addProxy = atom::meta::makeProxy(add);
    addProxy.setName("add_function");
    addProxy.setParameterName(0, "a");
    addProxy.setParameterName(1, "b");

    // Call the function using std::any arguments
    std::vector<std::any> addArgs = {std::make_any<int>(5),
                                     std::make_any<int>(7)};
    auto addResult = addProxy(addArgs);
    std::cout << "5 + 7 = " << std::any_cast<int>(addResult) << std::endl;

    // Using FunctionParams for argument passing
    atom::meta::FunctionParams addParams;
    addParams.emplace_back("a", 10);
    addParams.emplace_back("b", 20);
    auto addParamsResult = addProxy(addParams);
    std::cout << "10 + 20 = " << std::any_cast<int>(addParamsResult)
              << std::endl;

    // Get and display function info
    /*
    auto addInfo = addProxy.getFunctionInfo();
    std::cout << "Function name: " << addInfo.getName() << std::endl;
    std::cout << "Return type: " << addInfo.getReturnType() << std::endl;
    std::cout << "Parameter names: ";
    for (const auto& name : addInfo.getParameterNames()) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // Serialize function info to JSON
    nlohmann::json addInfoJson = addInfo.toJson();
    std::cout << "Function info as JSON:" << std::endl;
    printJson(addInfoJson);

    std::cout << std::endl;
    */

    // 2. String function proxying
    std::cout << "2. STRING FUNCTION PROXYING\n";
    std::cout << "-------------------------------------------\n";

    auto concatProxy = atom::meta::makeProxy(concatenate);
    concatProxy.setName("concatenate");

    std::vector<std::any> concatArgs = {std::make_any<std::string>("Hello, "),
                                        std::make_any<std::string>("World!")};

    auto concatResult = concatProxy(concatArgs);
    std::cout << "Concatenate result: "
              << std::any_cast<std::string>(concatResult) << std::endl;

    // Using different types that can be converted
    atom::meta::FunctionParams concatParams;
    concatParams.emplace_back("first", "C++");
    concatParams.emplace_back("second", std::string_view(" is awesome!"));

    auto concatParamsResult = concatProxy(concatParams);
    std::cout << "Concatenate with params: "
              << std::any_cast<std::string>(concatParamsResult) << std::endl;

    std::cout << std::endl;

    // 3. Void function proxying
    std::cout << "3. VOID FUNCTION PROXYING\n";
    std::cout << "-------------------------------------------\n";

    auto printProxy = atom::meta::makeProxy(printMessage);
    printProxy.setName("print_message");

    std::vector<std::any> printArgs = {
        std::make_any<std::string>("Hello from proxy!")};
    auto printResult = printProxy(printArgs);

    // Check that void functions return empty std::any
    std::cout << "Void function returned: "
              << (printResult.has_value() ? "a value" : "empty any")
              << std::endl;

    std::cout << std::endl;

    // 4. Reference parameter handling
    std::cout << "4. REFERENCE PARAMETER HANDLING\n";
    std::cout << "-------------------------------------------\n";

    auto incrementProxy = atom::meta::makeProxy(incrementAndReturn);
    incrementProxy.setName("increment_and_return");

    int value = 41;
    std::vector<std::any> incrementArgs = {std::make_any<int&>(value)};

    auto incrementResult = incrementProxy(incrementArgs);
    std::cout << "Increment result: " << std::any_cast<int>(incrementResult)
              << std::endl;
    std::cout << "Original value after increment: " << value << std::endl;

    std::cout << std::endl;

    // 5. Member function proxying
    std::cout << "5. MEMBER FUNCTION PROXYING\n";
    std::cout << "-------------------------------------------\n";

    Calculator calc;
    auto calcAddProxy = atom::meta::makeProxy(&Calculator::add);
    calcAddProxy.setName("calculator_add");

    // The first argument must be the object instance
    std::vector<std::any> calcAddArgs = {std::make_any<Calculator&>(calc),
                                         std::make_any<int>(15),
                                         std::make_any<int>(27)};

    auto calcAddResult = calcAddProxy(calcAddArgs);
    std::cout << "Calculator add result: " << std::any_cast<int>(calcAddResult)
              << std::endl;
    std::cout << "Calculator internal result: " << calc.getResult()
              << std::endl;

    // Const member function example
    auto calcMultiplyProxy = atom::meta::makeProxy(&Calculator::multiply);
    calcMultiplyProxy.setName("calculator_multiply");

    std::vector<std::any> calcMultiplyArgs = {
        std::make_any<const Calculator&>(calc), std::make_any<double>(2.5),
        std::make_any<double>(4.0)};

    auto calcMultiplyResult = calcMultiplyProxy(calcMultiplyArgs);
    std::cout << "Calculator multiply result: "
              << std::any_cast<double>(calcMultiplyResult) << std::endl;

    // Using reference_wrapper for better control
    auto calcResetProxy = atom::meta::makeProxy(&Calculator::reset);
    calcResetProxy.setName("calculator_reset");

    std::vector<std::any> calcResetArgs = {
        std::make_any<std::reference_wrapper<Calculator>>(std::ref(calc))};

    calcResetProxy(calcResetArgs);
    std::cout << "Calculator result after reset: " << calc.getResult()
              << std::endl;

    std::cout << std::endl;

    // 6. Async function proxying
    std::cout << "6. ASYNC FUNCTION PROXYING\n";
    std::cout << "-------------------------------------------\n";

    auto slowCalcProxy = atom::meta::makeAsyncProxy(slowCalculation);
    slowCalcProxy.setName("slow_calculation");

    std::vector<std::any> slowCalcArgs = {std::make_any<int>(6),
                                          std::make_any<int>(7)};

    std::cout << "Calling slow calculation asynchronously..." << std::endl;
    auto slowCalcFuture = slowCalcProxy(slowCalcArgs);

    std::cout << "Future received, doing other work while waiting..."
              << std::endl;
    // Simulate doing other work
    for (int i = 0; i < 5; i++) {
        std::cout << "  Working..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Get the result when ready
    auto asyncResult = slowCalcFuture.get();
    std::cout << "Async calculation result: " << std::any_cast<int>(asyncResult)
              << std::endl;

    // Async with FunctionParams
    atom::meta::FunctionParams slowParams;
    slowParams.emplace_back("a", 8);
    slowParams.emplace_back("b", 9);

    std::cout << "Calling slow calculation with params asynchronously..."
              << std::endl;
    auto slowParamsFuture = slowCalcProxy(slowParams);

    std::cout << "Future received, getting result immediately (will block)..."
              << std::endl;
    auto asyncParamsResult = slowParamsFuture.get();
    std::cout << "Async params calculation result: "
              << std::any_cast<int>(asyncParamsResult) << std::endl;

    std::cout << std::endl;

    // 7. Function composition
    std::cout << "7. FUNCTION COMPOSITION\n";
    std::cout << "-------------------------------------------\n";

    // Compose two functions: first add, then format the result
    auto addFormat = atom::meta::composeProxy(add, formatResult);

    std::vector<std::any> composeArgs = {std::make_any<int>(25),
                                         std::make_any<int>(17)};

    auto composeResult = addFormat(composeArgs);
    std::cout << "Composed function result: "
              << std::any_cast<std::string>(composeResult) << std::endl;

    // Get composed function info
    auto composeInfo = addFormat.getFunctionInfo();
    std::cout << "Composed function name: " << composeInfo.getName()
              << std::endl;
    std::cout << "Composed function return type: "
              << composeInfo.getReturnType() << std::endl;

    std::cout << std::endl;

    // 8. Error handling
    std::cout << "8. ERROR HANDLING\n";
    std::cout << "-------------------------------------------\n";

    // Type error example
    tryOperation("Call add with string arguments", []() {
        auto errorAddProxy = atom::meta::makeProxy(add);
        std::vector<std::any> badArgs = {std::make_any<std::string>("not"),
                                         std::make_any<std::string>("numbers")};
        errorAddProxy(badArgs);
    });

    // Argument count error
    tryOperation("Call add with wrong number of arguments", []() {
        auto errorAddProxy = atom::meta::makeProxy(add);
        std::vector<std::any> tooFewArgs = {std::make_any<int>(5)};
        errorAddProxy(tooFewArgs);
    });

    // Function that throws
    tryOperation("Call divide by zero", []() {
        auto divideProxy = atom::meta::makeProxy(divide);
        std::vector<std::any> divideByZeroArgs = {std::make_any<double>(10.0),
                                                  std::make_any<double>(0.0)};
        divideProxy(divideByZeroArgs);
    });

    // Successful division
    tryOperation("Call divide with valid arguments", []() {
        auto divideProxy = atom::meta::makeProxy(divide);
        std::vector<std::any> validDivideArgs = {std::make_any<double>(10.0),
                                                 std::make_any<double>(2.0)};
        auto result = divideProxy(validDivideArgs);
        std::cout << "  Division result: " << std::any_cast<double>(result)
                  << std::endl;
    });

    std::cout << std::endl;

    // 9. Type conversion
    std::cout << "9. TYPE CONVERSION\n";
    std::cout << "-------------------------------------------\n";

    auto multiplyProxy = atom::meta::makeProxy(multiply);

    // Int can be converted to double
    std::vector<std::any> mixedArgs = {
        std::make_any<int>(3),  // Will be converted to double
        std::make_any<double>(4.5)};

    auto mixedResult = multiplyProxy(mixedArgs);
    std::cout << "Multiply with type conversion: "
              << std::any_cast<double>(mixedResult) << std::endl;

    // String conversion
    auto concatProxyAgain = atom::meta::makeProxy(concatenate);

    std::vector<std::any> mixedStrings = {
        std::make_any<const char*>(
            "Hello "),  // Will be converted to std::string
        std::make_any<std::string>("world!")};

    auto stringResult = concatProxyAgain(mixedStrings);
    std::cout << "Concatenate with string conversion: "
              << std::any_cast<std::string>(stringResult) << std::endl;

    std::cout << std::endl;

    // 12. Complex example: Calculator API
    std::cout << "12. COMPLEX EXAMPLE: CALCULATOR API\n";
    std::cout << "-------------------------------------------\n";

    // Create a calculator instance
    Calculator apiCalc;

    // Create proxies for all calculator methods
    auto calcApiAdd = atom::meta::makeProxy(&Calculator::add);
    calcApiAdd.setName("calculator.add");

    auto calcApiMultiply = atom::meta::makeProxy(&Calculator::multiply);
    calcApiMultiply.setName("calculator.multiply");

    auto calcApiGetResult = atom::meta::makeProxy(&Calculator::getResult);
    calcApiGetResult.setName("calculator.getResult");

    auto calcApiReset = atom::meta::makeProxy(&Calculator::reset);
    calcApiReset.setName("calculator.reset");

    // Store all function proxies in a map (could be used for dynamic dispatch)
    std::unordered_map<
        std::string, std::function<std::any(const atom::meta::FunctionParams&)>>
        calculatorApi;

    // Setup the API with proxies
    calculatorApi["add"] =
        [&apiCalc, &calcApiAdd](const atom::meta::FunctionParams& params) {
            atom::meta::FunctionParams fullParams;
            fullParams.emplace_back(
                "instance", std::reference_wrapper<Calculator>(apiCalc));

            // Copy parameters from the input
            for (const auto& param : params) {
                fullParams.push_back(param);
            }

            return calcApiAdd(fullParams);
        };

    calculatorApi["multiply"] =
        [&apiCalc, &calcApiMultiply](const atom::meta::FunctionParams& params) {
            atom::meta::FunctionParams fullParams;
            fullParams.emplace_back(
                "instance", std::reference_wrapper<Calculator>(apiCalc));

            for (const auto& param : params) {
                fullParams.push_back(param);
            }

            return calcApiMultiply(fullParams);
        };

    calculatorApi["getResult"] =
        [&apiCalc,
         &calcApiGetResult](const atom::meta::FunctionParams& /*params*/) {
            atom::meta::FunctionParams fullParams;
            fullParams.emplace_back(
                "instance", std::reference_wrapper<Calculator>(apiCalc));
            return calcApiGetResult(fullParams);
        };

    calculatorApi["reset"] = [&apiCalc, &calcApiReset](
                                 const atom::meta::FunctionParams& /*params*/) {
        atom::meta::FunctionParams fullParams;
        fullParams.emplace_back("instance",
                                std::reference_wrapper<Calculator>(apiCalc));
        return calcApiReset(fullParams);
    };

    // Use the API
    std::cout << "Using calculator API:" << std::endl;

    // Call add
    atom::meta::FunctionParams addAPIParams;
    addAPIParams.emplace_back("a", 123);
    addAPIParams.emplace_back("b", 456);
    calculatorApi["add"](addAPIParams);

    // Get result
    atom::meta::FunctionParams emptyParams;
    auto apiResult = calculatorApi["getResult"](emptyParams);
    std::cout << "  Result after add: " << std::any_cast<int>(apiResult)
              << std::endl;

    // Call multiply
    atom::meta::FunctionParams multiplyAPIParams;
    multiplyAPIParams.emplace_back("a", 2.5);
    multiplyAPIParams.emplace_back("b", 3.0);
    auto multiplyAPIResult = calculatorApi["multiply"](multiplyAPIParams);
    std::cout << "  Multiply result: "
              << std::any_cast<double>(multiplyAPIResult) << std::endl;

    // Call reset
    calculatorApi["reset"](emptyParams);
    auto resetResult = calculatorApi["getResult"](emptyParams);
    std::cout << "  Result after reset: " << std::any_cast<int>(resetResult)
              << std::endl;

    return 0;
}
