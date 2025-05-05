#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/future.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

void basicUsageExamples() {
    printSeparator("Basic Usage Examples");

    std::cout << "Creating and waiting for a future..." << std::endl;
    auto future1 = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(500ms);
        return 42;
    });

    std::cout << "Waiting for result..." << std::endl;
    int result = future1.wait();
    std::cout << "Future result: " << result << std::endl;

    // 1.2 Using the then method for chaining
    std::cout << "\nChaining calls with then method..." << std::endl;
    auto chainedFuture =
        makeEnhancedFuture([]() {
            std::this_thread::sleep_for(300ms);
            return 10;
        }).then([](int value) {
              return value * 2;
          }).then([](int value) { return "Result: " + std::to_string(value); });

    std::string chainedResult = chainedFuture.wait();
    std::cout << "Chained call result: " << chainedResult << std::endl;

    // 1.3 Using onComplete callback
    std::cout << "\nUsing onComplete callback..." << std::endl;
    auto futureWithCallback = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(300ms);
        return 100;
    });

    futureWithCallback.onComplete([](int value) {
        std::cout << "Callback received value: " << value << std::endl;
    });

    // Give the callback time to execute
    std::this_thread::sleep_for(500ms);
}

// 2. Timeout and cancellation examples
void timeoutAndCancellationExamples() {
    printSeparator("Timeout and Cancellation Examples");

    // 2.1 Using timeout
    std::cout << "Using waitFor with timeout..." << std::endl;
    auto slowFuture = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(2s);  // Will timeout
        return 99;
    });

    auto result = slowFuture.waitFor(1000ms);
    std::cout << "Timeout result exists: "
              << (result.has_value() ? "yes" : "no") << std::endl;
    std::cout << "Future is cancelled: "
              << (slowFuture.isCancelled() ? "yes" : "no") << std::endl;

    // 2.2 Manual cancellation
    std::cout << "\nManually canceling future..." << std::endl;
    auto cancellableFuture = makeEnhancedFuture([]() {
        std::this_thread::sleep_for(5s);
        return 77;
    });

    std::cout << "Future current status: "
              << (cancellableFuture.isDone() ? "completed" : "not completed")
              << std::endl;
    cancellableFuture.cancel();
    std::cout << "Future is cancelled: "
              << (cancellableFuture.isCancelled() ? "yes" : "no") << std::endl;

    // Try waiting on a cancelled future
    try {
        cancellableFuture.wait();
    } catch (const atom::error::RuntimeError& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
}

// 3. Error handling examples
void errorHandlingExamples() {
    printSeparator("Error Handling Examples");

    // 3.1 Handling future with exception
    std::cout << "Handling future with exception..." << std::endl;
    auto failingFuture = makeEnhancedFuture([]() -> int {
        throw std::runtime_error("Deliberately thrown error");
        return 0;  // Won't reach here
    });

    try {
        failingFuture.wait();
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    // 3.2 Using catching method to handle exceptions
    std::cout << "\nUsing catching method to handle exceptions..." << std::endl;
    auto handledFuture = makeEnhancedFuture([]() -> int {
                             throw std::runtime_error("Another error");
                             return 0;
                         }).catching([](std::exception_ptr eptr) {
        try {
            if (eptr)
                std::rethrow_exception(eptr);
            return -1;
        } catch (const std::runtime_error& e) {
            std::cout << "Handling exception in catching: " << e.what()
                      << std::endl;
            return -999;  // Error value
        }
    });

    std::cout << "Processed result: " << handledFuture.wait() << std::endl;
}

// 4. Coroutine support examples
EnhancedFuture<int> coroutineFunctionExample() {
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

// 5. Parallel processing examples
void parallelProcessingExamples() {
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

    std::cout << "Number of tasks in processing: " << futures.size()
              << std::endl;

    // Collect results
    std::vector<int> results;
    for (auto& future : futures) {
        results.push_back(future.wait());
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
            std::cout << allResults[i].get();
            if (i < allResults.size() - 1)
                std::cout << ", ";
        }
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cout << "whenAll error: " << e.what() << std::endl;
    }
}

// 6. Edge cases and special values
void edgeCasesExamples() {
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

// 7. Platform-specific optimization examples
void platformOptimizationExamples() {
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