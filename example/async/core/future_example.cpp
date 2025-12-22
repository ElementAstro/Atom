/**
 * @file future.cpp
 * @brief Comprehensive demonstration of atom::async::EnhancedFuture
 * functionality
 *
 * @details This example demonstrates:
 * - Basic EnhancedFuture creation and result retrieval
 * - Future chaining with then() operations
 * - Callback registration and execution
 * - Timeout handling and cancellation
 * - Error handling and exception propagation
 * - Coroutine integration patterns
 * - Parallel processing with multiple futures
 * - Platform-specific optimizations
 * - Advanced future composition patterns
 *
 * @level Intermediate to Advanced
 * @prerequisites Basic understanding of futures/promises, C++20 concepts
 * @related_examples promise.cpp, async_worker_basic.cpp, parallel.cpp
 *
 * @note Demonstrates comprehensive EnhancedFuture capabilities
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/future.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// ============================================================================
// UTILITY FUNCTIONS AND HELPERS
// ============================================================================

// Print mutex for thread-safe outputstd::mutex print_mutex;

// Thread-safe print function with timestamptemplate <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    (std::cout << ... << args) << std::endl;
}

// Enhanced section separator with better formattingvoid printSeparator(const
// std::string& title) {
std::lock_guard<std::mutex> lock(print_mutex);
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Helper function to get thread ID as stringstd::string get_thread_id() {
std::stringstream ss;
ss << std::this_thread::get_id();
return ss.str();
}

// Performance timer for measuring operationsclass PerformanceTimer {
public:
void start(const std::string& operation) {
    current_operation_ = operation;
    start_time_ = std::chrono::high_resolution_clock::now();
    print_safe("⏱️  Starting: ", operation);
}

void stop() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                        end_time - start_time_)
                        .count();

    print_safe("⏱️  Completed: ", current_operation_, " in ", duration, " μs");
}

private:
std::string current_operation_;
std::chrono::high_resolution_clock::time_point start_time_;
}
;

// ============================================================================
// SECTION 1: BASIC ENHANCEDFUTURE USAGE
// ============================================================================
/**
 * @section basic_usage Basic EnhancedFuture Usage
 *
 * This section demonstrates fundamental EnhancedFuture operations including:
 * - Creating futures from functions
 * - Waiting for results and retrieving values
 * - Basic future chaining with then()
 * - Callback registration and execution
 * - Thread-safe result handling
 *
 * Key concepts:
 * - makeEnhancedFuture(): Creates futures from callable objects
 * - wait(): Blocks until future completes and returns result
 * - then(): Chains operations on future results
 * - onComplete(): Registers callbacks for completion
 *
 * @see promise.cpp for promise-based future creation
 */
void basicUsageExamples() {
    printSeparator("SECTION 1: Basic EnhancedFuture Usage Examples");

    // Example 1.1: Basic future creation and result retrieval
    print_safe("Example 1.1: Basic future creation and result retrieval");
    {
        PerformanceTimer timer;
        timer.start("Basic future");

        auto future = makeEnhancedFuture([]() {
            print_safe("🔧 Future task executing in thread: ", get_thread_id());
            std::this_thread::sleep_for(100ms);
            return 42;
        });

        print_safe("🏠 Main thread waiting for result...");
        int result = future.wait();
        print_safe("🏠 Future result: ", result);

        timer.stop();
    }

    // Example 1.2: Future chaining with then() method
    print_safe("\nExample 1.2: Future chaining with then() method");
    {
        PerformanceTimer timer;
        timer.start("Future chaining");

        auto chainedFuture =
            makeEnhancedFuture([]() {
                print_safe("🔧 Initial task executing...");
                std::this_thread::sleep_for(100ms);
                return 10;
            })
                .then([](int value) {
                    print_safe("🔧 First then() - doubling value: ", value);
                    return value * 2;
                })
                .then([](int value) {
                    print_safe("🔧 Second then() - converting to string: ",
                               value);
                    return "Result: " + std::to_string(value);
                });

        std::string chainedResult = chainedFuture.wait();
        print_safe("🏠 Chained result: ", chainedResult);

        timer.stop();
    }

    // Example 1.3: Callback registration with onComplete
    print_safe("\nExample 1.3: Callback registration with onComplete");
    {
        PerformanceTimer timer;
        timer.start("Future callbacks");

        auto futureWithCallback = makeEnhancedFuture([]() {
            print_safe("🔧 Task with callback executing...");
            std::this_thread::sleep_for(150ms);
            return 100;
        });

        // Register multiple callbacks
        futureWithCallback.onComplete([](int value) {
            print_safe("🔔 Callback 1 received value: ", value);
        });

        futureWithCallback.onComplete([](int value) {
            print_safe("🔔 Callback 2 processing value: ", value * value);
        });

        // Wait for the future and callbacks to complete
        int result = futureWithCallback.wait();
        print_safe("🏠 Main thread result: ", result);

        timer.stop();
    }
}

// ============================================================================
// SECTION 2: TIMEOUT AND CANCELLATION
// ============================================================================
/**
 * @section timeout_cancellation Timeout and Cancellation
 *
 * This section demonstrates EnhancedFuture timeout and cancellation features:
 * - Setting timeouts with waitFor()
 * - Handling timeout results and optional values
 * - Manual future cancellation
 * - Checking future status and cancellation state
 * - Exception handling for cancelled futures
 *
 * Key concepts:
 * - waitFor(): Wait with timeout, returns optional result
 * - cancel(): Manually cancel a running future
 * - isCancelled(): Check if future was cancelled
 * - isDone(): Check if future has completed
 *
 * @see async_worker_basic.cpp for worker-based cancellation
 */
void timeoutAndCancellationExamples() {
    printSeparator("SECTION 2: Timeout and Cancellation Examples");

    // Example 2.1: Timeout handling with waitFor
    print_safe("Example 2.1: Timeout handling with waitFor");
    {
        PerformanceTimer timer;
        timer.start("Timeout handling");

        auto slowFuture = makeEnhancedFuture([]() {
            print_safe("🔧 Slow task starting (will take 2 seconds)...");
            std::this_thread::sleep_for(2s);
            return 99;
        });

        print_safe("🏠 Waiting with 1 second timeout...");
        auto result = slowFuture.waitFor(1000ms);
        print_safe("🔍 Timeout result exists: ",
                   result.has_value() ? "yes" : "no");
        print_safe("🔍 Future is cancelled: ",
                   slowFuture.isCancelled() ? "yes" : "no");

        if (result.has_value()) {
            print_safe("✅ Result: ", result.value());
        } else {
            print_safe("⏰ Task timed out as expected");
        }

        timer.stop();
    }

    // Example 2.2: Manual cancellation
    print_safe("\nExample 2.2: Manual future cancellation");
    {
        PerformanceTimer timer;
        timer.start("Manual cancellation");

        auto cancellableFuture = makeEnhancedFuture([]() {
            print_safe("🔧 Long task starting (5 seconds)...");
            std::this_thread::sleep_for(5s);
            return 77;
        });

        print_safe("🔍 Future current status: ",
                   cancellableFuture.isDone() ? "completed" : "not completed");

        print_safe("🚫 Cancelling future...");
        cancellableFuture.cancel();

        print_safe("🔍 Future is cancelled: ",
                   cancellableFuture.isCancelled() ? "yes" : "no");

        // Try waiting on a cancelled future
        try {
            auto result = cancellableFuture.wait();
            print_safe("❌ Unexpected result: ", result);
        } catch (const atom::error::RuntimeError& e) {
            print_safe("✅ Expected cancellation exception: ", e.what());
        }

        timer.stop();
    }
}

// ============================================================================
// SECTION 3: ERROR HANDLING AND EXCEPTION PROPAGATION
// ============================================================================
/**
 * @section error_handling Error Handling and Exception Propagation
 *
 * This section demonstrates EnhancedFuture error handling capabilities:
 * - Exception propagation from async tasks
 * - Graceful error handling with try-catch
 * - Error recovery strategies
 * - Exception chaining through then() operations
 * - Robust error handling patterns
 *
 * Key concepts:
 * - Exception propagation: Exceptions thrown in futures are propagated to
 * wait()
 * - Error recovery: Using catch blocks for graceful error handling
 * - Exception safety: Ensuring proper cleanup on exceptions
 * - Error chaining: How exceptions propagate through chained operations
 *
 * @see promise.cpp for promise-based error handling
 */
void errorHandlingExamples() {
    printSeparator("SECTION 3: Error Handling and Exception Propagation");

    // Example 3.1: Basic exception handling
    print_safe("Example 3.1: Basic exception handling");
    {
        PerformanceTimer timer;
        timer.start("Exception handling");

        auto failingFuture = makeEnhancedFuture([]() -> int {
            print_safe("🔧 Task about to throw exception...");
            throw std::runtime_error("Deliberately thrown error for testing");
            return 0;  // Won't reach here
        });

        try {
            int result = failingFuture.wait();
            print_safe("❌ Unexpected success: ", result);
        } catch (const std::exception& e) {
            print_safe("✅ Expected exception caught: ", e.what());
        }

        timer.stop();
    }

    // Example 3.2: Exception handling in chained operations
    print_safe("\nExample 3.2: Exception handling in chained operations");
    {
        PerformanceTimer timer;
        timer.start("Chained exception handling");

        auto chainedFailure =
            makeEnhancedFuture([]() {
                print_safe("🔧 First task succeeding...");
                return 10;
            })
                .then([](int value) -> int {
                    print_safe("🔧 Second task about to fail with value: ",
                               value);
                    throw std::runtime_error("Error in chained operation");
                    return value * 2;  // Won't reach here
                })
                .then([](int value) -> std::string {
                    print_safe(
                        "🔧 Third task won't execute due to previous error");
                    return "Won't reach here";
                });

        try {
            std::string result = chainedFailure.wait();
            print_safe("❌ Unexpected success: ", result);
        } catch (const std::exception& e) {
            print_safe("✅ Exception propagated through chain: ", e.what());
        }

        timer.stop();
    }
    // Example 3.3: Error recovery with catching method
    print_safe("\nExample 3.3: Error recovery with catching method");
    {
        PerformanceTimer timer;
        timer.start("Error recovery");

        auto handledFuture =
            makeEnhancedFuture([]() -> int {
                print_safe(
                    "🔧 Task throwing error for recovery demonstration...");
                throw std::runtime_error("Error to be recovered");
                return 0;
            }).catching([](std::exception_ptr eptr) {
                try {
                    if (eptr) {
                        std::rethrow_exception(eptr);
                    }
                    return -1;  // Default case
                } catch (const std::runtime_error& e) {
                    print_safe("🔄 Handling exception in catching: ", e.what());
                    return -999;  // Error recovery value
                }
            });

        int result = handledFuture.wait();
        print_safe("🎯 Processed result after error recovery: ", result);

        timer.stop();
    }
}

// 4. Coroutine support examplesEnhancedFuture<int> coroutineFunctionExample() {
std::cout << "Starting coroutine..." << std::endl;

// Simulate some async work
auto future1 = makeEnhancedFuture([]() {
    std::this_thread::sleep_for(300ms);
    return 10;
});

// Wait for first future
int result1 = co_await future1;
std::cout << "In coroutine: got first result " << result1 << std::endl;

// Simulate more async work
auto future2 = makeEnhancedFuture([result1]() {
    std::this_thread::sleep_for(200ms);
    return result1 * 5;
});

// Wait for second future
int result2 = co_await future2;
std::cout << "In coroutine: got second result " << result2 << std::endl;

co_return result1 + result2;
}

void coroutineExamples() {
    printSeparator("Coroutine Support Examples");

    auto coroutineResult = coroutineFunctionExample();
    std::cout << "Waiting for coroutine to complete..." << std::endl;
    int finalResult = coroutineResult.wait();
    std::cout << "Coroutine final result: " << finalResult << std::endl;
}

// 5. Parallel processing examplesvoid parallelProcessingExamples() {
printSeparator("Parallel Processing Examples");

// 5.1 Using parallelProcess
std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

std::cout << "Processing vector in parallel..." << std::endl;
auto futures = parallelProcess(
    numbers,
    [](int num) {
        std::this_thread::sleep_for(100ms);  // Simulate work
        return num * num;
    },
    3);  // Each task processes 3 items

std::cout << "Number of tasks in processing: " << futures.size() << std::endl;

// Collect results (flatten chunks)
std::vector<int> results;
for (auto& future : futures) {
    auto chunk = future.wait();
    results.insert(results.end(), chunk.begin(), chunk.end());
}

std::cout << "Results: ";
for (size_t i = 0; i < results.size(); ++i) {
    std::cout << results[i];
    if (i < results.size() - 1)
        std::cout << ", ";
}
std::cout << std::endl;

// 5.2 Using whenAll
std::cout << "\nUsing whenAll to wait for multiple futures..." << std::endl;

std::vector<EnhancedFuture<int>> multipleFutures;
for (int i = 1; i <= 5; ++i) {
    multipleFutures.push_back(makeEnhancedFuture([i]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(i * 100));
        return i * 10;
    }));
}

try {
    auto combinedFuture =
        whenAll(multipleFutures.begin(), multipleFutures.end());
    auto allResults = combinedFuture.get();

    std::cout << "whenAll results: ";
    for (size_t i = 0; i < allResults.size(); ++i) {
        std::cout << allResults[i];
        if (i < allResults.size() - 1)
            std::cout << ", ";
    }
    std::cout << std::endl;
} catch (const std::exception& e) {
    std::cout << "whenAll error: " << e.what() << std::endl;
}
}

// 6. Edge cases and special valuesvoid edgeCasesExamples() {
printSeparator("Edge Cases and Special Values");

// 6.1 Handling empty values
std::cout << "Handling potentially empty values..." << std::endl;
auto optionalFuture = makeEnhancedFuture([]() -> std::optional<int> {
    if (rand() % 2 == 0) {
        return 42;
    } else {
        return std::nullopt;
    }
});

auto optionalResult = optionalFuture.wait();
if (optionalResult.has_value()) {
    std::cout << "Result exists: " << optionalResult.value() << std::endl;
} else {
    std::cout << "Result is empty" << std::endl;
}

// 6.2 Zero retry count
std::cout << "\nUsing zero retry count..." << std::endl;
auto zeroRetryFuture = makeEnhancedFuture([]() { return 5; })
                           .retry(
                               [](int value) {
                                   std::cout << "This should not be called"
                                             << std::endl;
                                   return value * 2;
                               },
                               0);

try {
    int result = zeroRetryFuture.wait();
    std::cout << "Zero retry result: " << result << std::endl;
} catch (const std::exception& e) {
    std::cout << "Zero retry exception: " << e.what() << std::endl;
}

// 6.3 void return type
std::cout << "\nHandling void return type..." << std::endl;
auto voidFuture = makeEnhancedFuture([]() {
    std::cout << "Executing void function" << std::endl;
    // No return value
});

voidFuture.wait();
std::cout << "Void future completed" << std::endl;

// Chaining a void future
auto chainedVoidFuture = voidFuture.then([]() {
    std::cout << "Void future's chained call executed" << std::endl;
    return 100;
});

int chainedVoidResult = chainedVoidFuture.wait();
std::cout << "Chained call result: " << chainedVoidResult << std::endl;
}

// 7. Platform-specific optimization examplesvoid platformOptimizationExamples()
// {
printSeparator("Platform-Specific Optimization Examples");

// Using platform-optimized future
std::cout << "Using platform-optimized Future..." << std::endl;
auto optimizedFuture = makeOptimizedFuture([]() {
    std::this_thread::sleep_for(300ms);
    return std::string("Result from optimized thread pool");
});

std::string optimizedResult = optimizedFuture.wait();
std::cout << "Optimized Future result: " << optimizedResult << std::endl;

// Comparing with regular futures
auto start = std::chrono::high_resolution_clock::now();

const int taskCount = 100;
std::vector<EnhancedFuture<int>> optimizedFutures;

for (int i = 0; i < taskCount; i++) {
    optimizedFutures.push_back(makeOptimizedFuture([i]() {
        std::this_thread::sleep_for(1ms);
        return i;
    }));
}

// Wait for all futures to complete
for (auto& future : optimizedFutures) {
    future.wait();
}

auto end = std::chrono::high_resolution_clock::now();
auto duration =
    std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Time taken to execute " << taskCount
          << " optimized tasks: " << duration.count() << "ms" << std::endl;
}

int main() {
    std::cout << "EnhancedFuture Usage Examples\n" << std::endl;

    try {
        // Run all examples
        basicUsageExamples();
        timeoutAndCancellationExamples();
        errorHandlingExamples();
        coroutineExamples();
        parallelProcessingExamples();
        edgeCasesExamples();
        platformOptimizationExamples();

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
