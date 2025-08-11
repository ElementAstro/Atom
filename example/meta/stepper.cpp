/**
 * @file function_sequence_example.cpp
 * @brief Comprehensive examples for using the FunctionSequence class
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/meta/stepper.hpp"

// Helper function to print results
template <typename T>
void printResult(const atom::meta::Result<T>& result) {
    if (result.isSuccess()) {
        try {
            const auto& value = result.value();
            std::cout << "  Success: " << std::any_cast<std::string>(value)
                      << std::endl;
        } catch (const std::bad_any_cast&) {
            std::cout << "  Success: <value of non-string type>" << std::endl;
        }
    } else {
        std::cout << "  Error: " << result.error() << std::endl;
    }
}

// Helper function to print execution stats
void printStats(const atom::meta::FunctionSequence::ExecutionStats& stats) {
    std::cout << "Execution Statistics:" << std::endl;
    std::cout << "  Total execution time: "
              << stats.totalExecutionTime.count() / 1000000.0 << " ms"
              << std::endl;
    std::cout << "  Invocation count: " << stats.invocationCount << std::endl;
    std::cout << "  Cache hits: " << stats.cacheHits << std::endl;
    std::cout << "  Cache misses: " << stats.cacheMisses << std::endl;
    std::cout << "  Error count: " << stats.errorCount << std::endl;
}

int main() {
    std::cout << "=== FunctionSequence Comprehensive Examples ===" << std::endl
              << std::endl;

    // Create a new FunctionSequence instance
    atom::meta::FunctionSequence sequence;

    // Example 1: Basic function registration and execution
    std::cout << "Example 1: Basic Function Registration and Execution"
              << std::endl;
    {
        // Register a simple string transformation function
        auto id = sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }
                std::string input = std::any_cast<std::string>(args[0]);
                return std::string("Processed: ") + input;
            });

        std::cout << "Registered function with ID: " << id << std::endl;

        // Create argument batches for execution
        std::vector<std::vector<std::any>> argsBatch = {{std::string("hello")},
                                                        {std::string("world")}};

        // Execute the function
        auto results = sequence.run(argsBatch);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results) {
            printResult(result);
        }

        std::cout << std::endl;
    }

    // Example 2: Multiple function registration
    std::cout << "Example 2: Multiple Function Registration" << std::endl;
    {
        sequence.clearFunctions();

        // Create multiple functions
        std::vector<atom::meta::FunctionSequence::FunctionType> functions = {
            // Function 1: Uppercase converter
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }
                std::string input = std::any_cast<std::string>(args[0]);
                std::string result;
                for (char c : input) {
                    result += std::toupper(c);
                }
                return result;
            },

            // Function 2: Add exclamation marks
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }
                std::string input = std::any_cast<std::string>(args[0]);
                return input + "!!!";
            },

            // Function 3: Add prefix
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }
                std::string input = std::any_cast<std::string>(args[0]);
                return std::string("PREFIX_") + input;
            }};

        // Register the functions
        auto ids = sequence.registerFunctions(functions);

        std::cout << "Registered " << ids.size() << " functions with IDs:";
        for (auto id : ids) {
            std::cout << " " << id;
        }
        std::cout << std::endl;

        // Create argument batches
        std::vector<std::vector<std::any>> argsBatch = {
            {std::string("test")}, {std::string("example")}};

        // Execute all functions for each argument
        auto resultsBatch = sequence.runAll(argsBatch);

        std::cout << "Results:" << std::endl;
        for (size_t i = 0; i < resultsBatch.size(); ++i) {
            std::cout << "Argument set " << i << ":" << std::endl;
            for (size_t j = 0; j < resultsBatch[i].size(); ++j) {
                std::cout << "  Function " << j << ": ";
                printResult(resultsBatch[i][j]);
            }
        }

        std::cout << std::endl;
    }

    // Example 3: Execution with timeout
    std::cout << "Example 3: Execution with Timeout" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function that sleeps for a while
        sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<int>(&args[0])) {
                    return std::string("No valid input");
                }

                int sleepTime = std::any_cast<int>(args[0]);
                std::cout << "  Function running, sleeping for " << sleepTime
                          << " ms..." << std::endl;
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(sleepTime));
                return std::string("Finished after ") +
                       std::to_string(sleepTime) + " ms";
            });

        // Create argument batches with different sleep times
        std::vector<std::vector<std::any>> argsBatch = {
            {50},  // 50ms - should succeed with 500ms timeout
            {600}  // 600ms - should fail with 500ms timeout
        };

        // Execute with timeout
        std::chrono::milliseconds timeout(500);
        std::cout << "Executing with " << timeout.count() << "ms timeout..."
                  << std::endl;
        auto results = sequence.executeWithTimeout(argsBatch, timeout);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results) {
            printResult(result);
        }

        printStats(sequence.getStats());
        sequence.resetStats();

        std::cout << std::endl;
    }

    // Example 4: Execution with retries
    std::cout << "Example 4: Execution with Retries" << std::endl;
    {
        sequence.clearFunctions();

        // Keep track of attempts for each input
        std::unordered_map<int, int> attemptCounter;
        std::mutex counterMutex;

        // Register a function that fails a few times before succeeding
        sequence.registerFunction(
            [&](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<int>(&args[0])) {
                    return std::string("No valid input");
                }

                int input = std::any_cast<int>(args[0]);
                int requiredAttempts = input;

                // Update attempt counter
                int currentAttempt;
                {
                    std::lock_guard<std::mutex> lock(counterMutex);
                    currentAttempt = ++attemptCounter[input];
                    std::cout << "  Function called with input " << input
                              << ", attempt " << currentAttempt << " of "
                              << requiredAttempts << " required" << std::endl;
                }

                // Fail if we haven't reached the required number of attempts
                if (currentAttempt < requiredAttempts) {
                    throw std::runtime_error(
                        "Simulated failure, need more attempts");
                }

                return std::string("Success after ") +
                       std::to_string(currentAttempt) + " attempts";
            });

        // Create argument batches
        std::vector<std::vector<std::any>> argsBatch = {
            {2},  // Requires 2 attempts
            {3}   // Requires 3 attempts
        };

        // Execute with retries
        size_t maxRetries = 3;
        std::cout << "Executing with " << maxRetries << " retries..."
                  << std::endl;
        auto results = sequence.executeWithRetries(argsBatch, maxRetries);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results) {
            printResult(result);
        }

        printStats(sequence.getStats());
        sequence.resetStats();

        std::cout << std::endl;
    }

    // Example 5: Execution with caching
    std::cout << "Example 5: Execution with Caching" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function that returns a timestamp (to show caching effect)
        sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }

                std::string input = std::any_cast<std::string>(args[0]);
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                std::string timestamp = std::ctime(&time);

                return std::string("Result for '") + input + "' at " +
                       timestamp;
            });

        // Create argument batches with some repeated values
        std::vector<std::vector<std::any>> argsBatch1 = {
            {std::string("A")}, {std::string("B")}, {std::string("C")}};

        std::vector<std::vector<std::any>> argsBatch2 = {
            {std::string("A")},  // This should be served from cache
            {std::string("B")},  // This should be served from cache
            {std::string("D")}   // This is new and should be computed
        };

        // First execution - all should be computed
        std::cout << "First execution (no cache):" << std::endl;
        auto results1 = sequence.executeWithCaching(argsBatch1);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results1) {
            printResult(result);
        }

        // Second execution - some should be from cache
        std::cout << "\nSecond execution (with cache):" << std::endl;
        auto results2 = sequence.executeWithCaching(argsBatch2);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results2) {
            printResult(result);
        }

        std::cout << "Cache size: " << sequence.cacheSize() << std::endl;
        printStats(sequence.getStats());
        std::cout << "Cache hit ratio: " << std::fixed << std::setprecision(2)
                  << sequence.getCacheHitRatio() * 100.0 << "%" << std::endl;

        // Clear the cache
        sequence.clearCache();
        std::cout << "Cache cleared. New size: " << sequence.cacheSize()
                  << std::endl;
        sequence.resetStats();

        std::cout << std::endl;
    }

    // Example 6: Asynchronous execution
    std::cout << "Example 6: Asynchronous Execution" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function that takes some time
        sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<int>(&args[0])) {
                    return std::string("No valid input");
                }

                int sleepTime = std::any_cast<int>(args[0]);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(sleepTime));
                return std::string("Processed after ") +
                       std::to_string(sleepTime) + " ms";
            });

        // Create argument batch
        std::vector<std::vector<std::any>> argsBatch = {{100}, {200}, {300}};

        // Execute asynchronously
        std::cout << "Starting async execution..." << std::endl;
        auto future = sequence.runAsync(std::move(argsBatch));

        std::cout << "Doing other work while waiting..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::cout << "  Other work: " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Get the results
        std::cout << "Getting results..." << std::endl;
        auto results = future.get();

        std::cout << "Results:" << std::endl;
        for (const auto& result : results) {
            printResult(result);
        }

        std::cout << std::endl;
    }

    // Example 7: Parallel execution
    std::cout << "Example 7: Parallel Execution" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function that takes some time and reports its thread
        sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<int>(&args[0])) {
                    return std::string("No valid input");
                }

                int sleepTime = std::any_cast<int>(args[0]);
                auto threadId = std::this_thread::get_id();
                std::stringstream ss;
                ss << threadId;

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(sleepTime));

                return std::string("Processed in thread ") + ss.str() +
                       " after " + std::to_string(sleepTime) + " ms";
            });

        // Create argument batch with more items than typical cores
        std::vector<std::vector<std::any>> argsBatch;
        for (int i = 0; i < 12; ++i) {
            argsBatch.push_back({100});  // All take 100ms
        }

        // Configuration for parallel execution
        atom::meta::FunctionSequence::ExecutionOptions options;
        options.policy =
            atom::meta::FunctionSequence::ExecutionPolicy::Parallel;

        // Execute in parallel
        auto startTime = std::chrono::high_resolution_clock::now();
        std::cout << "Starting parallel execution with " << argsBatch.size()
                  << " items..." << std::endl;
        auto results = sequence.execute(argsBatch, options);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << "Parallel execution completed in " << duration.count()
                  << "ms" << std::endl;
        std::cout << "Results (showing first few):" << std::endl;
        for (size_t i = 0; i < std::min(results.size(), size_t(5)); ++i) {
            std::cout << "  Item " << i << ": ";
            printResult(results[i]);
        }

        // For comparison, run sequentially
        startTime = std::chrono::high_resolution_clock::now();
        std::cout << "\nStarting sequential execution with " << argsBatch.size()
                  << " items..." << std::endl;
        results = sequence.run(argsBatch);
        endTime = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << "Sequential execution completed in " << duration.count()
                  << "ms" << std::endl;

        std::cout << std::endl;
    }

    // Example 8: Combining multiple features
    std::cout << "Example 8: Combining Multiple Features" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function that processes data with potential for failure
        sequence.registerFunction(
            [](std::span<const std::any> args) -> std::any {
                if (args.empty() || !std::any_cast<std::string>(&args[0])) {
                    return std::string("No valid input");
                }

                std::string input = std::any_cast<std::string>(args[0]);

                // Simulate occasional failures
                if (input.length() % 3 == 0) {
                    throw std::runtime_error("Simulated random failure");
                }

                // Simulate processing time proportional to input length
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(input.length() * 10));

                return std::string("Processed '") + input + "' successfully";
            });

        // Create argument batch with various inputs
        std::vector<std::vector<std::any>> argsBatch = {
            {std::string("short")},
            {std::string("medium length")},
            {std::string("this is a longer input string")},
            {std::string("abc")}  // This will fail (length % 3 == 0)
        };

        // Set up combined execution options
        atom::meta::FunctionSequence::ExecutionOptions options;
        options.timeout =
            std::chrono::milliseconds(500);  // Timeout after 500ms
        options.retryCount = 2;              // Retry up to 2 times
        options.enableCaching = true;        // Use caching
        options.policy = atom::meta::FunctionSequence::ExecutionPolicy::
            Parallel;  // Run in parallel
        options.notificationCallback = [](const std::any& result) {
            try {
                std::cout << "  Notification: "
                          << std::any_cast<std::string>(result) << std::endl;
            } catch (const std::bad_any_cast&) {
                std::cout << "  Notification: <non-string result>" << std::endl;
            }
        };

        // Execute with combined options
        std::cout << "Executing with combined options (timeout, retries, "
                     "caching, parallel, notifications)..."
                  << std::endl;
        auto results = sequence.execute(argsBatch, options);

        std::cout << "Results:" << std::endl;
        for (const auto& result : results) {
            printResult(result);
        }

        // Show statistics
        printStats(sequence.getStats());
        std::cout << "Average execution time: " << std::fixed
                  << std::setprecision(2) << sequence.getAverageExecutionTime()
                  << " ms" << std::endl;
        std::cout << "Cache hit ratio: " << std::fixed << std::setprecision(2)
                  << sequence.getCacheHitRatio() * 100.0 << "%" << std::endl;

        sequence.resetStats();
        sequence.clearCache();

        std::cout << std::endl;
    }

    // Example 9: Advanced error handling
    std::cout << "Example 9: Advanced Error Handling" << std::endl;
    {
        sequence.clearFunctions();

        // Register a function with comprehensive error handling
        sequence.registerFunction([](std::span<const std::any> args)
                                      -> std::any {
            try {
                if (args.empty()) {
                    throw std::invalid_argument("No arguments provided");
                }

                // Try to process different types with type checking
                if (auto strPtr = std::any_cast<std::string>(&args[0])) {
                    if (strPtr->empty()) {
                        throw std::invalid_argument("Empty string provided");
                    }
                    return std::string("String processed: ") + *strPtr;
                } else if (auto intPtr = std::any_cast<int>(&args[0])) {
                    if (*intPtr < 0) {
                        throw std::domain_error("Negative integer not allowed");
                    }
                    return std::string("Integer processed: ") +
                           std::to_string(*intPtr);
                } else if (auto doublePtr = std::any_cast<double>(&args[0])) {
                    if (std::isnan(*doublePtr) || std::isinf(*doublePtr)) {
                        throw std::domain_error("NaN or infinity not allowed");
                    }
                    return std::string("Double processed: ") +
                           std::to_string(*doublePtr);
                } else {
                    throw std::bad_any_cast();
                }
            } catch (const std::invalid_argument& e) {
                throw std::runtime_error(std::string("Invalid input: ") +
                                         e.what());
            } catch (const std::domain_error& e) {
                throw std::runtime_error(std::string("Domain error: ") +
                                         e.what());
            } catch (const std::bad_any_cast&) {
                throw std::runtime_error(
                    "Type mismatch: Unsupported argument type");
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Processing error: ") +
                                         e.what());
            } catch (...) {
                throw std::runtime_error("Unknown error occurred");
            }
        });

        // Test cases with different types and error conditions
        std::vector<std::vector<std::any>> argsBatch = {
            {std::string("valid string")},  // Should succeed
            {std::string("")},              // Should fail: empty string
            {42},                           // Should succeed
            {-10},                          // Should fail: negative integer
            {3.14159},                      // Should succeed
            {std::vector<int>{1, 2, 3}}     // Should fail: unsupported type
        };

        // Execute and examine results
        std::cout << "Executing with various inputs to test error handling..."
                  << std::endl;
        auto results = sequence.run(argsBatch);

        std::cout << "Results:" << std::endl;
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "  Input " << i << ": ";
            printResult(results[i]);
        }

        std::cout << std::endl;
    }

    return 0;
}
