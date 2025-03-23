/**
 * Comprehensive examples for atom::meta::decorate utilities
 *
 * This file demonstrates the use of all decorator functionalities:
 * 1. Basic decorator patterns
 * 2. Switchable functions
 * 3. Loop decorators
 * 4. Retry decorators with backoff
 * 5. Condition check decorators
 * 6. Cache decorators
 * 7. Timing decorators
 * 8. Throttling decorators
 * 9. Parameter validation
 * 10. Expected-based error handling
 * 11. Decorator composition with DecorateStepper
 */

#include "atom/meta/decorate.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "atom/macro.hpp"

using namespace atom::meta;
using namespace std::chrono_literals;

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n==========================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================================="
              << std::endl;
}

// Helper function to print subsection headers
void printSubHeader(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

//=============================================================================
// Example functions to decorate
//=============================================================================

// Function that might fail randomly
int unstableFunction(int value) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(0, 10);

    if (dist(gen) < 3) {
        throw std::runtime_error("Random failure in unstable function");
    }

    return value * 2;
}

// Function with expensive computation
double expensiveCalculation(double x, double y) {
    // Simulate an expensive calculation
    std::this_thread::sleep_for(100ms);
    return std::pow(x, 2) + std::pow(y, 2);
}

// Simple string processing function
std::string processText(const std::string& text, bool uppercase) {
    std::string result = text;
    if (uppercase) {
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    }
    return result;
}

// Function that divides two numbers
double safeDivide(double a, double b) {
    if (b == 0.0) {
        throw std::invalid_argument("Division by zero");
    }
    return a / b;
}

// Database query simulation
std::vector<std::string> queryDatabase(const std::string& query, int limit) {
    // Simulate database query delay
    std::this_thread::sleep_for(200ms);

    // Generate sample results
    std::vector<std::string> results;
    for (int i = 0; i < limit; ++i) {
        results.push_back("Result " + std::to_string(i) + " for: " + query);
    }

    return results;
}

// User authentication function
bool authenticateUser(const std::string& username,
                      const std::string& password) {
    // Simple authentication for demonstration
    return username == "admin" && password == "password123";
}

// Function that creates records
void createRecord(const std::string& name, int age, const std::string& email) {
    std::cout << "Creating record: name=" << name << ", age=" << age
              << ", email=" << email << std::endl;

    // Simulate record creation
    std::this_thread::sleep_for(50ms);
}

// 修复1: 为void返回类型特化LoopDecorator

// 修复2: 改进RetryDecorator实现

//=============================================================================
// Main function with examples
//=============================================================================

int main() {
    std::cout << "========================================================="
              << std::endl;
    std::cout << "   Comprehensive Decorator Pattern Examples               "
              << std::endl;
    std::cout << "========================================================="
              << std::endl;

    //=========================================================================
    // 1. Basic Decorators
    //=========================================================================
    printHeader("1. Basic Decorators");

    printSubHeader("1.1 Basic Function Decorator");

    // Create decorator using std::function
    auto decoratedProcessText =
        decorator<std::function<std::string(const std::string&, bool)>>(
            processText);

    // Use the decorated function
    std::string basicResult = decoratedProcessText("Hello, World!", true);
    std::cout << "Basic decorated result: " << basicResult << std::endl;

    printSubHeader("1.2 Switchable Function");

    // Create a switchable function
    auto switchableCalc =
        Switchable<double(double, double)>(expensiveCalculation);

    // Test the original function
    std::cout << "Original calculation: " << switchableCalc(3.0, 4.0)
              << std::endl;

    // Switch to a new implementation
    switchableCalc.switchTo([](double x, double y) {
        std::cout << "Using alternative calculation" << std::endl;
        return x * y;  // Different implementation (multiplication)
    });

    // Test the new implementation
    std::cout << "Alternative calculation: " << switchableCalc(3.0, 4.0)
              << std::endl;

    //=========================================================================
    // 2. Loop Decorators
    //=========================================================================
    printHeader("2. Loop Decorators");

    // Create a function that will be repeated
    auto counterFunc = [count = 0](int /* unused */) mutable {
        return ++count;
    };

    // Create a loop decorator
    auto loopedCounter = makeLoopDecorator(counterFunc);

    // Run the function 5 times with a progress callback
    auto progressCallback = [](int current, int total) {
        std::cout << "Progress: " << current + 1 << "/" << total << std::endl;
    };

    int finalCount = loopedCounter(5, progressCallback, 10);
    std::cout << "Final count after 5 loops: " << finalCount << std::endl;

    // Example with void return type
    auto printMessage = []() {
        std::cout << "Executing loop iteration" << std::endl;
    };

    // 修复: 为void返回类型的函数特化处理
    auto loopedPrinter = makeLoopDecorator(printMessage);
    loopedPrinter(3);  // Will print the message 3 times

    //=========================================================================
    // 3. Retry Decorators
    //=========================================================================
    printHeader("3. Retry Decorators");

    // Create retry decorator for the unstable function
    auto retryUnstable =
        std::make_shared<RetryDecorator<int, int>>(unstableFunction, 5);

    // Stepper to combine the retry logic with the unstable function
    auto retryStep = makeDecorateStepper(unstableFunction);
    // 修复: 使用正确的addDecorator方法
    retryStep.addDecorator(retryUnstable);

    // Try to call the unstable function with retries
    try {
        int retryResult = retryStep(42);
        std::cout << "Retry succeeded, result: " << retryResult << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Retry failed after 5 attempts: " << e.what() << std::endl;
    }

    // Custom retry with exponential backoff
    auto customRetry = RetryDecorator<int, int>(
        unstableFunction,               // function to retry
        10,                             // max retries
        std::chrono::milliseconds(50),  // initial backoff
        2.0,                            // backoff multiplier
        true                            // use exponential backoff
    );

    auto customRetryStep = makeDecorateStepper(unstableFunction);
    // 修复: 正确的参数传递给addDecorator
    customRetryStep.addDecorator(std::make_shared<RetryDecorator<int, int>>(
        unstableFunction, 10, std::chrono::milliseconds(50), 2.0, true));

    try {
        int result = customRetryStep(21);
        std::cout << "Custom retry succeeded, result: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Custom retry failed: " << e.what() << std::endl;
    }

    //=========================================================================
    // 4. Condition Check Decorators
    //=========================================================================
    printHeader("4. Condition Check Decorators");

    // Create a condition check decorator for processText
    auto conditionalText = makeConditionCheckDecorator(processText);

    // Define conditions
    bool shouldProcess = true;
    auto checkCondition = [&shouldProcess]() { return shouldProcess; };

    // With condition true
    std::string conditionalResult =
        conditionalText(checkCondition,
                        "Default text",  // Fallback value if condition is false
                        "Hello from conditional decorator", true);
    std::cout << "Conditional result (true): " << conditionalResult
              << std::endl;

    // Change condition to false
    shouldProcess = false;
    conditionalResult =
        conditionalText(checkCondition,
                        "Default text",  // Now this fallback will be used
                        "This text won't be processed", true);
    std::cout << "Conditional result (false): " << conditionalResult
              << std::endl;

    // Using a fallback function instead of a value
    // 修复: 增加unused参数标记
    conditionalResult = conditionalText(
        checkCondition,
        [](const std::string& s, bool /* unused */) {
            return "Fallback: " + s;
        },
        "This text will use fallback function", false);
    std::cout << "Conditional result with fallback function: "
              << conditionalResult << std::endl;

    //=========================================================================
    // 5. Cache Decorators
    //=========================================================================
    printHeader("5. Cache Decorators");

    // Create a cache decorator for the expensive calculation
    auto cacheStep = makeDecorateStepper(expensiveCalculation);
    // 修复: 正确使用addDecorator方法
    auto cacheDecorator =
        std::make_shared<CacheDecorator<double, double, double>>(
            expensiveCalculation, std::chrono::seconds(60), 100);
    cacheStep.addDecorator(cacheDecorator);

    // First call (will compute)
    auto start = std::chrono::high_resolution_clock::now();
    double cacheResult1 = cacheStep(3.0, 4.0);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "First call result: " << cacheResult1 << " (took "
              << duration1.count() << "ms)" << std::endl;

    // Second call with same parameters (should use cache)
    start = std::chrono::high_resolution_clock::now();
    double cacheResult2 = cacheStep(3.0, 4.0);
    end = std::chrono::high_resolution_clock::now();
    auto duration2 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Second call result: " << cacheResult2 << " (took "
              << duration2.count() << "ms)" << std::endl;

    // Call with different parameters (will compute)
    start = std::chrono::high_resolution_clock::now();
    double cacheResult3 = cacheStep(5.0, 6.0);
    end = std::chrono::high_resolution_clock::now();
    auto duration3 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Different parameters result: " << cacheResult3 << " (took "
              << duration3.count() << "ms)" << std::endl;

    //=========================================================================
    // 6. Timing Decorators
    //=========================================================================
    printHeader("6. Timing Decorators");

    // Create a timing callback
    auto timingCallback = [](std::string_view functionName,
                             std::chrono::microseconds duration) {
        std::cout << "Function '" << functionName << "' executed in "
                  << duration.count() / 1000.0 << "ms" << std::endl;
    };

    // Create a timing decorator for the database query function
    auto timingDecorator =
        makeTimingDecorator(queryDatabase, "Database Query", timingCallback);

    // Create a decorator stepper for the database query
    auto timingStep = makeDecorateStepper(queryDatabase);
    // 修复: 正确传递装饰器
    auto timingDec = std::make_shared<
        TimingDecorator<std::vector<std::string>, const std::string&, int>>(
        "Database Query", timingCallback);

    timingStep.addDecorator(timingDec);

    // Execute the timed database query
    std::vector<std::string> queryResults =
        timingStep("SELECT * FROM users", 3);

    std::cout << "Query returned " << queryResults.size()
              << " results:" << std::endl;
    for (const auto& result : queryResults) {
        std::cout << "  - " << result << std::endl;
    }

    //=========================================================================
    // 7. Throttling Decorators
    //=========================================================================
    printHeader("7. Throttling Decorators");

    // Create a throttling decorator for a rapid function
    auto rapidFunction = [](int id) {
        std::cout << "Processing request " << id << std::endl;
        return id * 10;
    };

    auto throttleStep = makeDecorateStepper(rapidFunction);
    // 修复: 正确使用addDecorator
    throttleStep.addDecorator(std::make_shared<ThrottlingDecorator<int, int>>(
        std::chrono::milliseconds(500)  // Minimum 500ms between calls
        ));

    // Execute several calls in rapid succession
    std::cout
        << "Starting throttled calls (should be spaced out by at least 500ms):"
        << std::endl;

    auto throttleStart = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= 5; ++i) {
        int result = throttleStep(i);
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - throttleStart);
        std::cout << "Call " << i << " at " << elapsed.count()
                  << "ms, result: " << result << std::endl;
    }

    //=========================================================================
    // 8. Parameter Validation Decorators
    //=========================================================================
    printHeader("8. Parameter Validation Decorators");

    // Create a validator for the safeDivide function
    auto divideValidator = [](double a, double b) {
        return b != 0.0;  // Check if divisor is not zero
    };

    auto divideErrorMsg = [](double a, double b) {
        return std::string("Cannot divide ") + std::to_string(a) + " by " +
               std::to_string(b) + " (division by zero)";
    };

    // Create a validation decorator
    auto validatedDivide = ValidationDecorator<double, double, double>(
        divideValidator, divideErrorMsg);

    // Create a stepper for the divide function with validation
    auto validateStep = makeDecorateStepper(safeDivide);
    validateStep.addDecorator(
        std::make_shared<ValidationDecorator<double, double, double>>(
            divideValidator, divideErrorMsg));

    // Test with valid parameters
    try {
        double validResult = validateStep(10.0, 2.0);
        std::cout << "Valid division result: " << validResult << std::endl;
    } catch (const DecoratorError& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }

    // Test with invalid parameters
    try {
        // 修复: 声明但未使用变量
        validateStep(10.0, 0.0);
        std::cout << "This should not be reached." << std::endl;
    } catch (const DecoratorError& e) {
        std::cout << "Expected validation error: " << e.what() << std::endl;
    }

    // Create validators for the createRecord function
    auto recordValidator = [](const std::string& name, int age,
                              const std::string& email) {
        return !name.empty() && age > 0 && age < 150 &&
               email.find('@') != std::string::npos;
    };

    auto recordErrorMsg = [](const std::string& name, int age,
                             const std::string& email) {
        std::string msg = "Invalid record data:";
        if (name.empty())
            msg += " name cannot be empty;";
        if (age <= 0 || age >= 150)
            msg += " age must be between 1 and 149;";
        if (email.find('@') == std::string::npos)
            msg += " email must contain @;";
        return msg;
    };

    // Create a validation stepper for createRecord
    auto validateRecordStep = makeDecorateStepper(createRecord);
    // 修复: 正确传递参数给addDecorator
    validateRecordStep.addDecorator(
        std::make_shared<ValidationDecorator<void, const std::string&, int,
                                             const std::string&>>(
            recordValidator, recordErrorMsg));

    // Test with valid record
    try {
        validateRecordStep("John Doe", 35, "john.doe@example.com");
        std::cout << "Record created successfully" << std::endl;
    } catch (const DecoratorError& e) {
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }

    // Test with invalid record
    try {
        validateRecordStep("", -5, "invalid-email");
        std::cout << "This should not be reached." << std::endl;
    } catch (const DecoratorError& e) {
        std::cout << "Expected validation error: " << e.what() << std::endl;
    }

    //=========================================================================
    // 10. Complex Decorator Composition
    //=========================================================================
    printHeader("10. Complex Decorator Composition");

    // Create a complex decorating chain for queryDatabase:
    // 1. Add validation
    // 2. Add caching
    // 3. Add timing
    // 4. Add retries
    // 5. Add throttling

    auto queryValidator = [](const std::string& query, int limit) {
        return !query.empty() && limit > 0 && limit <= 100;
    };

    auto queryErrorMsg = [](const std::string& query, int limit) {
        std::string msg = "Invalid query parameters:";
        if (query.empty())
            msg += " query cannot be empty;";
        if (limit <= 0 || limit > 100)
            msg += " limit must be between 1 and 100;";
        return msg;
    };

    // Create the complex decorator chain
    auto complexQueryStep = makeDecorateStepper(queryDatabase);

    // Add validation (first to run)
    complexQueryStep.addDecorator(
        std::make_shared<ValidationDecorator<std::vector<std::string>,
                                             const std::string&, int>>(
            queryValidator, queryErrorMsg));

    // Add caching (second to run)
    complexQueryStep.addDecorator(
        std::make_shared<
            CacheDecorator<std::vector<std::string>, const std::string&, int>>(
            queryDatabase,
            std::chrono::seconds(30),  // 30s TTL
            50                         // Max 50 cached results
            ));

    // Add timing (third to run)
    complexQueryStep.addDecorator(
        std::make_shared<
            TimingDecorator<std::vector<std::string>, const std::string&, int>>(
            "Database Query", timingCallback));

    complexQueryStep.addDecorator(
        std::make_shared<
            RetryDecorator<std::vector<std::string>, const std::string&, int>>(
            queryDatabase, 3, std::chrono::milliseconds(100), 2.0, true));

    complexQueryStep.addDecorator(
        std::make_shared<ThrottlingDecorator<std::vector<std::string>,
                                             const std::string&, int>>(
            std::chrono::milliseconds(300)  // Minimum 300ms between calls
            ));

    // Add timing (sixth to run)
    // Execute the complex decorated function
    std::cout << "First complex query execution:" << std::endl;
    auto complexResult1 = complexQueryStep("SELECT * FROM products", 5);
    std::cout << "Returned " << complexResult1.size() << " results"
              << std::endl;

    std::cout << "\nSecond complex query execution (should use cache):"
              << std::endl;
    auto complexResult2 = complexQueryStep("SELECT * FROM products", 5);
    std::cout << "Returned " << complexResult2.size() << " results"
              << std::endl;

    std::cout << "\nThird complex query execution (different parameters):"
              << std::endl;
    auto complexResult3 = complexQueryStep("SELECT * FROM users", 3);
    std::cout << "Returned " << complexResult3.size() << " results"
              << std::endl;

    // Try invalid parameters to trigger validation
    try {
        std::cout << "\nAttempting invalid query parameters:" << std::endl;
        // 修复: 去除未使用变量
        ATOM_UNUSED_RESULT(complexQueryStep("", -5));
        std::cout << "This should not be reached." << std::endl;
    } catch (const DecoratorError& e) {
        std::cout << "Expected validation error: " << e.what() << std::endl;
    }

    return 0;
}