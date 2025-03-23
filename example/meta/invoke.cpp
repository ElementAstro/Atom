/**
 * Comprehensive examples for atom::meta invoke utility functions
 *
 * This file demonstrates all utility functions provided in invoke.hpp:
 * 1. Basic invocation utilities (delayInvoke, compose, etc.)
 * 2. Error handling and safety mechanisms (safeCall, retryCall, etc.)
 * 3. Memoization and caching (memoize, cacheCall)
 * 4. Parallel and asynchronous execution (parallelBatchCall, asyncCall)
 * 5. Performance instrumentation
 *
 * @author Example Author
 * @date 2025-03-21
 */

#include "atom/meta/invoke.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace atom::meta;

// Simple class for member function examples
class Calculator {
public:
    Calculator(int base = 0) : base_value(base) {}

    int add(int a, int b) const { return a + b + base_value; }

    int subtract(int a, int b) {
        call_count++;
        return a - b - base_value;
    }

    static int multiply(int a, int b) { return a * b; }

    int get_call_count() const { return call_count; }

    int base_value{0};

private:
    int call_count{0};
};

// Function with custom validation
bool is_valid_input(int a, int b) { return a >= 0 && b >= 0; }

// Helper to print a section divider
void print_section(const std::string& title) {
    std::cout << "\n==================================================\n";
    std::cout << "  " << title;
    std::cout << "\n==================================================\n";
}

// Forward declarations for organization
void demo_basic_invocation();
void demo_error_handling();
void demo_memoization_caching();
void demo_parallel_async();
void demo_transformation_composition();
void demo_timeout_retry();
void demo_instrumentation();

// Main function to run all examples
int main() {
    std::cout << "=== atom::meta::invoke Utility Functions Examples ===\n";

    try {
        demo_basic_invocation();
        demo_error_handling();
        demo_memoization_caching();
        demo_parallel_async();
        demo_transformation_composition();
        demo_timeout_retry();
        demo_instrumentation();

        std::cout << "\nAll examples completed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "\nException caught in main: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

//==============================================================================
// 1. Basic Invocation Utilities
//==============================================================================
void demo_basic_invocation() {
    print_section("1. Basic Invocation Utilities");

    // Example 1: delayInvoke with regular function
    std::cout << "1.1 delayInvoke with regular function\n";
    {
        auto add = [](int a, int b) -> int { return a + b; };

        // Create a delayed invocation
        auto delayed_add_5_10 = delayInvoke(add, 5, 10);

        // Call the delayed function
        int result = delayed_add_5_10();

        std::cout << "  Delayed add(5, 10) = " << result << "\n";
    }

    // Example 2: delayInvoke with std::function
    std::cout << "\n1.2 delayInvoke with std::function\n";
    {
        std::function<std::string(std::string, int)> repeat =
            [](std::string s, int times) -> std::string {
            std::string result;
            for (int i = 0; i < times; ++i) {
                result += s;
            }
            return result;
        };

        auto delayed_repeat = delayInvoke(repeat, "Hello ", 3);
        std::cout << "  Delayed repeat(\"Hello \", 3) = " << delayed_repeat()
                  << "\n";
    }

    // Example 3: delayMemInvoke with member function
    std::cout << "\n1.3 delayMemInvoke with member function\n";
    {
        Calculator calc(5);  // base value of 5

        // Create a delayed member function invocation
        auto delayed_add = delayMemInvoke(&Calculator::add, &calc);

        // Call the delayed function
        int result = delayed_add(10, 20);

        std::cout << "  Delayed calc.add(10, 20) = " << result
                  << " (includes base value 5)\n";
    }

    // Example 4: delayMemInvoke with const member function
    std::cout << "\n1.4 delayMemInvoke with const member function\n";
    {
        const Calculator calc(3);  // const object

        // Create a delayed const member function invocation
        auto delayed_add = delayMemInvoke(&Calculator::add, &calc);

        // Call the delayed function
        int result = delayed_add(7, 8);

        std::cout << "  Delayed const calc.add(7, 8) = " << result
                  << " (includes base value 3)\n";
    }

    // Example 5: delayStaticMemInvoke with static member function
    std::cout << "\n1.5 delayStaticMemInvoke with static member function\n";
    {
        // Create a delayed static member function invocation
        auto delayed_multiply =
            delayStaticMemInvoke<int, int, int>(&Calculator::multiply);

        // Call the delayed function
        int result = delayed_multiply(6, 7);

        std::cout << "  Delayed Calculator::multiply(6, 7) = " << result
                  << "\n";
    }

    // Example 6: delayMemberVarInvoke with member variable
    std::cout << "\n1.6 delayMemberVarInvoke with member variable\n";
    {
        Calculator calc;
        calc.base_value = 42;

        // Create a delayed member variable access
        auto delayed_base_access =
            delayMemberVarInvoke(&Calculator::base_value, &calc);

        // Access the member variable
        int& base_ref = delayed_base_access();
        std::cout << "  Original base_value = " << base_ref << "\n";

        // Modify through the reference
        base_ref = 100;
        std::cout << "  Modified base_value = " << calc.base_value << "\n";
    }

    // Example 7: makeDeferred for type erasure
    std::cout << "\n1.7 makeDeferred for type-erased callable\n";
    {
        auto lambda = [](const std::string& prefix, int x) -> std::string {
            return prefix + std::to_string(x);
        };

        // Create a type-erased function object
        std::function<std::string()> deferred =
            makeDeferred<std::string>(lambda, "Number: ", 42);

        // Call the deferred function
        std::cout << "  Deferred result: " << deferred() << "\n";
    }

    // Example 8: validate_then_invoke
    std::cout << "\n1.8 validate_then_invoke\n";
    {
        auto divide = [](int a, int b) -> double {
            return static_cast<double>(a) / b;
        };

        // Validate that the divisor is not zero
        auto validator = [](int, int b) { return b != 0; };

        // Create a validated function
        auto safe_divide = validate_then_invoke(validator, divide);

        try {
            std::cout << "  safe_divide(10, 2) = " << safe_divide(10, 2)
                      << "\n";

            std::cout << "  Attempting safe_divide(10, 0)... ";
            safe_divide(10, 0);  // This should throw an exception
        } catch (const std::invalid_argument& e) {
            std::cout << "Caught exception: " << e.what() << "\n";
        }
    }
}

//==============================================================================
// 2. Error Handling and Safety Mechanisms
//==============================================================================
void demo_error_handling() {
    print_section("2. Error Handling and Safety Mechanisms");

    // Example 1: safeCall - basic usage
    std::cout << "2.1 safeCall - basic usage\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        double result1 = safeCall(divide, 10, 2);
        std::cout << "  safeCall(divide, 10, 2) = " << result1 << "\n";

        double result2 = safeCall(
            divide, 10, 0);  // This would throw, but safeCall returns 0.0
        std::cout << "  safeCall(divide, 10, 0) = " << result2
                  << " (default constructed value)\n";
    }

    // Example 2: safeCallResult - returns Result<T>
    std::cout << "\n2.2 safeCallResult - returns Result<T>\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        auto result1 = safeCallResult(divide, 10, 2);
        std::cout << "  safeCallResult(divide, 10, 2) has value: "
                  << result1.has_value() << "\n";
        if (result1.has_value()) {
            std::cout << "  Value: " << result1.value() << "\n";
        }

        auto result2 = safeCallResult(divide, 10, 0);
        std::cout << "  safeCallResult(divide, 10, 0) has value: "
                  << result2.has_value() << "\n";
        if (!result2.has_value()) {
            std::cout << "  Error occurred\n";
        }
    }

    // Example 3: safeTryCatch - returns variant with result or exception_ptr
    std::cout << "\n2.3 safeTryCatch - returns variant with result or "
                 "exception_ptr\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        auto result1 = safeTryCatch(divide, 10, 2);
        if (std::holds_alternative<double>(result1)) {
            std::cout << "  safeTryCatch(divide, 10, 2) = "
                      << std::get<double>(result1) << "\n";
        }

        auto result2 = safeTryCatch(divide, 10, 0);
        if (std::holds_alternative<std::exception_ptr>(result2)) {
            try {
                std::rethrow_exception(std::get<std::exception_ptr>(result2));
            } catch (const std::exception& e) {
                std::cout << "  safeTryCatch(divide, 10, 0) caught: "
                          << e.what() << "\n";
            }
        }
    }

    // Example 4: safeTryWithDiagnostics - includes function call info
    std::cout << "\n2.4 safeTryWithDiagnostics - includes function call info\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        auto result1 = safeTryWithDiagnostics(divide, "divide", 10, 2);
        if (std::holds_alternative<double>(result1)) {
            std::cout << "  safeTryWithDiagnostics(divide, 10, 2) = "
                      << std::get<double>(result1) << "\n";
        }

        auto result2 = safeTryWithDiagnostics(divide, "divide", 10, 0);
        if (std::holds_alternative<
                std::pair<std::exception_ptr, FunctionCallInfo>>(result2)) {
            const auto& [ex_ptr, info] = std::get<1>(result2);
            try {
                std::rethrow_exception(ex_ptr);
            } catch (const std::exception& e) {
                std::cout << "  Exception: " << e.what() << "\n";
                std::cout << "  Function info: " << info.to_string() << "\n";
            }
        }
    }

    // Example 5: safeTryCatchOrDefault - provides default value
    std::cout << "\n2.5 safeTryCatchOrDefault - provides default value\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        double result1 = safeTryCatchOrDefault(divide, -1.0, 10, 2);
        std::cout << "  safeTryCatchOrDefault(divide, -1.0, 10, 2) = "
                  << result1 << "\n";

        double result2 = safeTryCatchOrDefault(divide, -1.0, 10, 0);
        std::cout << "  safeTryCatchOrDefault(divide, -1.0, 10, 0) = "
                  << result2 << " (default value)\n";
    }

    // Example 6: safeTryCatchWithCustomHandler - custom exception handling
    std::cout
        << "\n2.6 safeTryCatchWithCustomHandler - custom exception handling\n";
    {
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        std::function<void(std::exception_ptr)> handler =
            [](std::exception_ptr eptr) {
                try {
                    if (eptr) {
                        std::rethrow_exception(eptr);
                    }
                } catch (const std::exception& e) {
                    std::cout << "  Custom handler caught: " << e.what()
                              << "\n";
                }
            };

        double result1 = safeTryCatchWithCustomHandler(divide, handler, 10, 2);
        std::cout
            << "  safeTryCatchWithCustomHandler(divide, handler, 10, 2) = "
            << result1 << "\n";

        double result2 = safeTryCatchWithCustomHandler(divide, handler, 10, 0);
        std::cout << "  Result after handler: " << result2 << "\n";
    }
}

//==============================================================================
// 4. Parallel and Asynchronous Execution
//==============================================================================
void demo_parallel_async() {
    print_section("4. Parallel and Asynchronous Execution");

    // Example 1: asyncCall - asynchronous execution
    std::cout << "4.1 asyncCall - asynchronous execution\n";
    {
        // Define a function that takes some time
        auto process_data = [](const std::string& data) -> std::string {
            std::cout << "  Processing data: " << data << "...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return "Processed: " + data;
        };

        std::cout << "  Starting asynchronous call...\n";
        auto future = asyncCall(process_data, "sample data");

        std::cout << "  Main thread continues executing while async work "
                     "happens...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "  Still doing other work...\n";

        // Wait for the result
        std::string result = future.get();
        std::cout << "  Async result: " << result << "\n";
    }

    // Example 2: batchCall - sequential batch processing
    std::cout << "\n4.2 batchCall - sequential batch processing\n";
    {
        // Define a function that processes a pair of values
        auto process_pair = [](int a, int b) -> std::string {
            std::cout << "  Processing pair (" << a << ", " << b << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return "Result: " + std::to_string(a + b);
        };

        // Create a batch of argument tuples
        std::vector<std::tuple<int, int>> args = {
            {1, 2}, {3, 4}, {5, 6}, {7, 8}};

        std::cout << "  Starting batch processing...\n";
        auto start = std::chrono::high_resolution_clock::now();

        auto results = batchCall(process_pair, args);

        auto duration = std::chrono::high_resolution_clock::now() - start;
        std::cout << "  Batch processing completed in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         duration)
                         .count()
                  << "ms\n";

        // Display results
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "  Batch result " << i << ": " << results[i] << "\n";
        }
    }

    // Example 3: parallelBatchCall - parallel batch processing
    std::cout << "\n4.3 parallelBatchCall - parallel batch processing\n";
    {
        // Define a function that processes a pair of values
        auto process_pair = [](int a, int b) -> std::string {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Result: " + std::to_string(a + b) + " (thread: " +
                   std::to_string(std::hash<std::thread::id>{}(
                                      std::this_thread::get_id()) %
                                  1000) +
                   ")";
        };

        // Create a batch of argument tuples
        std::vector<std::tuple<int, int>> args = {{1, 2},   {3, 4},  {5, 6},
                                                  {7, 8},   {9, 10}, {11, 12},
                                                  {13, 14}, {15, 16}};

        std::cout << "  Starting parallel batch processing with 4 threads...\n";
        auto start = std::chrono::high_resolution_clock::now();

        auto results = parallelBatchCall(process_pair, args, 4);

        auto duration = std::chrono::high_resolution_clock::now() - start;
        std::cout << "  Parallel batch processing completed in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         duration)
                         .count()
                  << "ms\n";

        // Display results
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "  Batch result " << i << ": " << results[i] << "\n";
        }
    }

    // Example 4: parallelBatchCall with custom thread pool size
    std::cout << "\n4.4 parallelBatchCall with exception handling\n";
    {
        // Define a function that might throw
        auto process_value = [](int value) -> double {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (value == 0) {
                throw std::runtime_error("Cannot process zero");
            }
            return 100.0 / value;
        };

        // Create a batch of argument tuples
        std::vector<std::tuple<int>> args = {
            {10},
            {5},
            {2},
            {0},
            {1}  // Includes a value that will cause exception
        };

        std::cout << "  Starting parallel batch with potential exception...\n";

        try {
            auto results = parallelBatchCall(process_value, args, 2);

            // This won't execute due to exception
            for (size_t i = 0; i < results.size(); ++i) {
                std::cout << "  Result " << i << ": " << results[i] << "\n";
            }
        } catch (const std::exception& e) {
            std::cout << "  Caught exception from parallel batch: " << e.what()
                      << "\n";
        }
    }
}

//==============================================================================
// 5. Transformation and Composition
//==============================================================================
void demo_transformation_composition() {
    print_section("5. Transformation and Composition");

    // Example 1: compose - function composition
    std::cout << "5.1 compose - function composition\n";
    {
        // Define some simple functions
        auto add_one = [](int x) -> int { return x + 1; };
        auto multiply_by_two = [](int x) -> int { return x * 2; };
        auto square = [](int x) -> int { return x * x; };

        // Compose functions: square(multiply_by_two(add_one(x)))
        auto composed = compose(add_one, multiply_by_two, square);

        // Test the composed function
        int result = composed(3);
        std::cout << "  compose(add_one, multiply_by_two, square)(3) = "
                  << result << "\n";
        std::cout
            << "  This is equivalent to square(multiply_by_two(add_one(3)))\n";
        std::cout << "  = square(multiply_by_two(4))\n";
        std::cout << "  = square(8)\n";
        std::cout << "  = 64\n";
    }

    // Example 2: compose with different types
    std::cout << "\n5.2 compose with different types\n";
    {
        // Define functions with different type signatures
        auto to_string = [](int x) -> std::string { return std::to_string(x); };
        auto add_prefix = [](const std::string& s) -> std::string {
            return "Number: " + s;
        };
        auto count_chars = [](const std::string& s) -> size_t {
            return s.length();
        };

        // Compose functions: count_chars(add_prefix(to_string(x)))
        auto composed = compose(to_string, add_prefix, count_chars);

        // Test the composed function
        size_t result = composed(42);
        std::cout << "  compose(to_string, add_prefix, count_chars)(42) = "
                  << result << "\n";
        std::cout << "  This counts the length of \"Number: 42\" which is "
                  << result << " characters\n";
    }

    // Example 3: transform_args - transform function arguments
    std::cout << "\n5.3 transform_args - transform function arguments\n";
    {
        // Define a function that works with transformed arguments
        auto add = [](int a, int b) -> int { return a + b; };

        // Define a transformation that doubles each argument
        auto double_arg = [](int x) -> int { return x * 2; };

        // Create a function that doubles its arguments before adding
        auto add_doubled = transform_args(double_arg, add);

        // Test the transformed function
        int result = add_doubled(3, 4);
        std::cout << "  add_doubled(3, 4) = " << result << "\n";
        std::cout
            << "  This is equivalent to add(double_arg(3), double_arg(4))\n";
        std::cout << "  = add(6, 8)\n";
        std::cout << "  = 14\n";
    }

    // Example 4: transform_args with complex transformation
    std::cout << "\n5.4 transform_args with complex transformation\n";
    {
        // Define a string concatenation function
        auto concat = [](const std::string& a,
                         const std::string& b) -> std::string { return a + b; };

        // Define a transformation that uppercases each string
        auto to_uppercase = [](const std::string& s) -> std::string {
            std::string result = s;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::toupper(c); });
            return result;
        };

        // Create a function that uppercases its arguments before concatenation
        auto concat_uppercase = transform_args(to_uppercase, concat);

        // Test the transformed function
        std::string result = concat_uppercase("hello", "world");
        std::cout << "  concat_uppercase(\"hello\", \"world\") = \"" << result
                  << "\"\n";
    }
}

//==============================================================================
// 6. Timeout and Retry Mechanisms
//==============================================================================
void demo_timeout_retry() {
    print_section("6. Timeout and Retry Mechanisms");

    // Example 1: timeoutCall - function with timeout
    std::cout << "6.1 timeoutCall - function with timeout\n";
    {
        // Define a function that may take a long time
        auto long_task = [](int duration_ms) -> std::string {
            std::cout << "  Starting long task (" << duration_ms << "ms)...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
            std::cout << "  Long task completed\n";
            return "Task result after " + std::to_string(duration_ms) + "ms";
        };

        // Call with sufficient timeout
        try {
            std::cout << "  Calling with 500ms timeout for a 200ms task...\n";
            auto result = timeoutCall(long_task, 500ms, 200);
            std::cout << "  Result: " << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Exception: " << e.what() << "\n";
        }

        // Call with insufficient timeout
        try {
            std::cout << "\n  Calling with 100ms timeout for a 500ms task...\n";
            auto result = timeoutCall(long_task, 100ms, 500);
            std::cout << "  Result: " << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Caught timeout exception: " << e.what() << "\n";
        }
    }

    // Example 2: retryCall - function with retry mechanism
    std::cout << "\n6.2 retryCall - function with retry mechanism\n";
    {
        // Create a function that fails a certain number of times
        std::atomic<int> call_count{0};
        auto unreliable_function =
            [&call_count](int fail_until) -> std::string {
            int current_call = ++call_count;
            std::cout << "  Attempt #" << current_call << "...\n";

            if (current_call < fail_until) {
                std::cout << "  Failed!\n";
                throw std::runtime_error(
                    "Simulated failure in unreliable_function");
            }

            std::cout << "  Succeeded!\n";
            return "Success on attempt #" + std::to_string(current_call);
        };

        // Call with retries - will succeed on the 3rd attempt
        try {
            std::cout
                << "  Calling with 5 retries, will succeed on attempt #3...\n";
            auto result = retryCall(unreliable_function, 5, 100ms, 3);
            std::cout << "  Final result: " << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Failed after all retries: " << e.what() << "\n";
        }

        // Reset counter for next example
        call_count = 0;

        // Call with insufficient retries - will fail
        try {
            std::cout << "\n  Calling with only 2 retries, needs 4 attempts to "
                         "succeed...\n";
            auto result = retryCall(unreliable_function, 2, 50ms, 4);
            std::cout << "  Final result: " << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Failed after all retries: " << e.what() << "\n";
        }
    }

    // Example 3: retryCall with exponential backoff
    std::cout << "\n6.3 retryCall with exponential backoff\n";
    {
        // Create a function that logs time between attempts
        std::atomic<int> call_count{0};
        std::chrono::time_point<std::chrono::high_resolution_clock>
            last_call_time;

        auto backoff_test = [&]() -> int {
            auto now = std::chrono::high_resolution_clock::now();
            int current_call = ++call_count;

            if (current_call > 1) {
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_call_time)
                        .count();
                std::cout << "  Time since last attempt: " << elapsed << "ms\n";
            }

            last_call_time = now;

            if (current_call <= 3) {
                std::cout << "  Attempt #" << current_call << " failing...\n";
                throw std::runtime_error("Simulated failure");
            }

            std::cout << "  Attempt #" << current_call << " succeeding\n";
            return current_call;
        };

        // Initialize the time of the "previous" call
        last_call_time = std::chrono::high_resolution_clock::now();

        // Call with exponential backoff starting at 50ms
        try {
            std::cout
                << "  Calling with exponential backoff (starting at 50ms)...\n";
            auto result = retryCall(backoff_test, 5, 50ms);
            std::cout << "  Final result: " << result << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Failed after all retries: " << e.what() << "\n";
        }
    }
}

//==============================================================================
// 7. Performance Instrumentation
//==============================================================================
void demo_instrumentation() {
    print_section("7. Performance Instrumentation");

    // Example 1: instrument - basic function instrumentation
    std::cout << "7.1 instrument - basic function instrumentation\n";
    {
        // Define a function to instrument
        auto fibonacci = [](int n) -> int {
            if (n <= 1)
                return n;

            int a = 0, b = 1;
            for (int i = 2; i <= n; ++i) {
                int temp = a + b;
                a = b;
                b = temp;
            }
            return b;
        };

        // Create instrumented version
        auto instrumented_fib = instrument(fibonacci, "fibonacci");

        // Call the instrumented function multiple times
        std::cout << "  fibonacci(10) = " << instrumented_fib(10) << "\n";
        std::cout << "  fibonacci(20) = " << instrumented_fib(20) << "\n";
        std::cout << "  fibonacci(30) = " << instrumented_fib(30) << "\n";

        // Call with larger value to see performance difference
        std::cout << "  fibonacci(40) = " << instrumented_fib(40) << "\n";

        // Check instrumentation metrics
        // The metrics are captured by the instrumented_fib lambda
        std::cout << "  Instrumentation report for fibonacci:\n";
        // Note: In a real application, you'd add a method to access the report
        std::cout << "  - This would show call count, average/min/max times\n";
    }

    // Example 2: instrument with exception tracking
    std::cout << "\n7.2 instrument with exception tracking\n";
    {
        // Define a function that sometimes throws
        auto divide = [](int a, int b) -> double {
            if (b == 0) {
                throw std::runtime_error("Division by zero");
            }
            return static_cast<double>(a) / b;
        };

        // Create instrumented version
        auto instrumented_divide = instrument(divide, "divide_function");

        // Make some successful calls
        std::cout << "  divide(10, 2) = " << instrumented_divide(10, 2) << "\n";
        std::cout << "  divide(20, 4) = " << instrumented_divide(20, 4) << "\n";

        // Make some calls that throw exceptions
        try {
            std::cout << "  Attempting divide(5, 0)... ";
            instrumented_divide(5, 0);
        } catch (const std::exception& e) {
            std::cout << "caught exception: " << e.what() << "\n";
        }

        try {
            std::cout << "  Attempting divide(7, 0)... ";
            instrumented_divide(7, 0);
        } catch (const std::exception& e) {
            std::cout << "caught exception: " << e.what() << "\n";
        }

        // Check instrumentation metrics
        std::cout << "  Instrumentation report for divide_function:\n";
        std::cout << "  - Would show 4 calls, 2 exceptions\n";
    }
}