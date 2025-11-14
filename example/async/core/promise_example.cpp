/**
 * @file promise.cpp
 * @brief Comprehensive demonstration of atom::async::Promise functionality
 *
 * @details This example demonstrates:
 * - Basic Promise creation and value setting
 * - Promise callbacks and completion handling
 * - Promise cancellation with stop tokens
 * - Coroutine integration and awaitable patterns
 * - Error handling and exception propagation
 * - Advanced Promise utilities and helper functions
 * - Platform-specific async execution optimizations
 *
 * @level Intermediate to Advanced
 * @prerequisites Basic understanding of futures/promises, C++20 concepts
 * @related_examples future.cpp, async_worker_basic.cpp, coroutine_patterns.cpp
 *
 * @note Requires C++20 for coroutine support
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/promise.hpp"

#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// UTILITY FUNCTIONS AND HELPERS
// ============================================================================

// Print mutex for thread-safe output
std::mutex print_mutex;

// Thread-safe print function with timestamp
template <typename... Args>
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

// Print section divider with enhanced formatting
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Helper function to get thread ID as string
std::string get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// Performance timer for measuring operations
class PerformanceTimer {
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

        print_safe("⏱️  Completed: ", current_operation_, " in ", duration,
                   " μs");
    }

private:
    std::string current_operation_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

// Validation helper
template <typename T>
void validate_result(const T& result, const T& expected,
                     const std::string& test_name) {
    if (result == expected) {
        print_safe("✅ ", test_name, " PASSED");
    } else {
        print_safe("❌ ", test_name, " FAILED: expected ", expected, ", got ",
                   result);
    }
}

// ============================================================================
// SECTION 1: BASIC PROMISE USAGE
// ============================================================================
/**
 * @section basic_usage Basic Promise Usage
 *
 * This section demonstrates fundamental Promise operations including:
 * - Creating promises and getting futures
 * - Setting values and retrieving results
 * - Thread-safe value passing between threads
 * - Basic error handling patterns
 *
 * Key concepts:
 * - Promise<T>: Allows setting a value of type T asynchronously
 * - EnhancedFuture<T>: Provides enhanced future functionality
 * - Thread safety: Promises are thread-safe for value setting
 *
 * @see future.cpp for complementary future operations
 */
void basic_usage_examples() {
    print_section("SECTION 1: Basic Promise Usage Examples");

    // Example 1.1: Basic integer promise
    print_safe("Example 1.1: Basic integer promise");
    {
        PerformanceTimer timer;
        timer.start("Basic integer promise");

        auto promise = std::make_shared<atom::async::Promise<int>>();
        auto future = promise->getEnhancedFuture();

        // Execute in a separate thread
        std::thread worker([promise]() {
            print_safe("🔧 Worker thread [", get_thread_id(),
                       "] processing...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            promise->setValue(42);
            print_safe("🔧 Worker thread set value to 42");
        });

        // Wait for result in main thread
        print_safe("🏠 Main thread [", get_thread_id(),
                   "] waiting for result...");
        int result = future.get();
        print_safe("🏠 Main thread received result: ", result);

        validate_result(result, 42, "Basic integer promise");
        worker.join();

        // Small delay to ensure all Promise operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        timer.stop();
    }

    // Example 1.2: String promise with enhanced error handling
    print_safe("\nExample 1.2: String promise with enhanced error handling");
    {
        PerformanceTimer timer;
        timer.start("String promise");

        atom::async::Promise<std::string> promise;
        auto future = promise.getEnhancedFuture();

        std::thread worker([&promise]() {
            print_safe("🔧 Worker thread processing string task...");
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            promise.setValue("Hello from async world!");
            print_safe("🔧 Worker thread completed string task");
        });

        print_safe("🏠 Main thread waiting for string result...");
        std::string result = future.get();
        print_safe("🏠 String result: '", result, "'");

        validate_result(result, std::string("Hello from async world!"),
                        "String promise");
        worker.join();

        // Small delay to ensure all Promise operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        timer.stop();
    }

    // Example 1.3: Void promise for completion signaling
    print_safe("\nExample 1.3: Void promise for completion signaling");
    {
        PerformanceTimer timer;
        timer.start("Void promise");

        atom::async::Promise<void> promise;
        auto future = promise.getEnhancedFuture();

        std::thread worker([&promise]() {
            print_safe("🔧 Worker thread executing void task...");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            promise.setValue();  // Signal completion
            print_safe("🔧 Worker thread signaled completion");
        });

        print_safe("🏠 Main thread waiting for task completion...");
        future.wait();  // Wait for completion signal
        print_safe("🏠 Task completed successfully");

        worker.join();

        // Small delay to ensure all Promise operations complete
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        timer.stop();
    }

    // Example 1.4: Ready promise using utility functions
    print_safe("\nExample 1.4: Ready promise using utility functions");
    {
        PerformanceTimer timer;
        timer.start("Ready promise");

        // Create a promise that's already resolved
        auto readyPromise = atom::async::makeReadyPromise(100);
        auto readyFuture = readyPromise.getEnhancedFuture();

        print_safe("📦 Ready promise created with value 100");
        int result = readyFuture.get();  // Should return immediately
        print_safe("📦 Ready promise result: ", result);

        validate_result(result, 100, "Ready promise");
        timer.stop();
    }

    // Example 1.5: Promise from function using utility
    print_safe("\nExample 1.5: Promise from function using utility");
    {
        PerformanceTimer timer;
        timer.start("Promise from function");

        // Create promise that executes a function asynchronously
        auto functionPromise =
            atom::async::makePromiseFromFunction([]() -> int {
                print_safe("🔧 Function executing in background thread...");
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return 200;
            });

        auto funcFuture = functionPromise.getEnhancedFuture();
        print_safe("🏠 Waiting for function result...");
        int result = funcFuture.get();
        print_safe("🏠 Function result: ", result);

        validate_result(result, 200, "Promise from function");
        timer.stop();
    }
}

// ============================================================================
// SECTION 2: PROMISE CALLBACKS AND COMPLETION HANDLING
// ============================================================================
/**
 * @section callbacks Promise Callbacks and Completion Handling
 *
 * This section demonstrates advanced Promise callback features:
 * - Setting completion callbacks
 * - Chaining multiple callbacks
 * - Error handling in callbacks
 * - Callback execution order and thread safety
 *
 * Key concepts:
 * - onComplete(): Register callbacks for promise completion
 * - Callback thread safety: Callbacks execute in the completing thread
 * - Exception handling: Callback exceptions don't affect promise state
 *
 * @see async_executor.cpp for callback execution strategies
 */
void callback_examples() {
    print_section("SECTION 2: Promise Callbacks and Completion Handling");

    // Example 2.1: Basic completion callback
    print_safe("Example 2.1: Basic completion callback");
    {
        PerformanceTimer timer;
        timer.start("Basic callback");

        atom::async::Promise<int> promise;
        auto future = promise.getEnhancedFuture();

        // Register completion callback
        promise.onComplete([](int value) {
            print_safe("🔔 Callback received value: ", value);
            print_safe("🔔 Callback executing in thread: ", get_thread_id());
        });

        std::thread worker([&promise]() {
            print_safe("🔧 Worker setting value 42...");
            promise.setValue(42);
        });

        int result = future.get();
        print_safe("🏠 Main thread got result: ", result);

        worker.join();
        timer.stop();
    }
}

// ============================================================================
// SECTION 2: PROMISE CALLBACKS AND COMPLETION HANDLING
// ============================================================================
/**
 * @section callbacks Promise Callbacks and Completion Handling
 *
 * This section demonstrates advanced Promise callback features:
 * - Setting completion callbacks
 * - Chaining multiple callbacks
 * - Error handling in callbacks
 * - Callback execution order and thread safety
 *
 * Key concepts:
 * - onComplete(): Register callbacks for promise completion
 * - Callback thread safety: Callbacks execute in the completing thread
 * - Exception handling: Callback exceptions don't affect promise state
 *
 * @see async_executor.cpp for callback execution strategies
 */
void promise_cancellation_examples() {
    print_section("SECTION 2: Promise Cancellation and Stop Tokens");

    // Example 2.1: Basic promise cancellation
    print_safe("Example 2.1: Basic promise cancellation");
    {
        PerformanceTimer timer;
        timer.start("Promise cancellation");

        atom::async::Promise<int> promise;
        auto future = promise.getEnhancedFuture();

        // Cancel the promise before setting value
        bool cancelled = promise.cancel();
        print_safe("🚫 Promise cancelled: ", cancelled ? "Yes" : "No");

        // Try to set value on cancelled promise (should throw)
        try {
            promise.setValue(42);
            print_safe("❌ ERROR: setValue should have thrown!");
        } catch (const std::exception& e) {
            print_safe("✅ Expected exception: ", e.what());
        }

        timer.stop();
    }

    // Example 2.2: Cancellation with stop tokens
    print_safe("\nExample 2.2: Cancellation with stop tokens");
    {
        PerformanceTimer timer;
        timer.start("Stop token cancellation");

        std::stop_source stopSource;
        atom::async::Promise<int> promise;
        auto future = promise.getEnhancedFuture();

        // Set up cancellation with stop token
        promise.setCancellable(stopSource.get_token());

        std::thread worker([&promise, &stopSource]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            print_safe("🔧 Worker requesting stop...");
            stopSource.request_stop();

            // Try to set value after stop requested
            try {
                promise.setValue(42);
                print_safe("❌ setValue succeeded unexpectedly");
            } catch (const std::exception& e) {
                print_safe("✅ Stop token prevented setValue: ", e.what());
            }
        });

        worker.join();
        timer.stop();
    }
}

// 2. Different parameter combination examples
void parameter_combination_examples() {
    print_section("Different Parameter Combination Examples");

    // Example 1: Promise function with multiple parameters
    print_safe("Example 1: Promise function with multiple parameters");

    auto calcPromise = atom::async::makePromiseFromFunction(
        [](int a, double b, std::string c) -> std::string {
            print_safe("Thread [", get_thread_id(),
                       "] calculating with params: ", a, ", ", b, ", ", c);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Result: " + std::to_string(a) + " + " + std::to_string(b) +
                   " + " + c;
        },
        10, 3.14, "hello");

    auto calcFuture = calcPromise.getEnhancedFuture();
    print_safe("Waiting for multi-param calculation...");
    std::string calcResult = calcFuture.get();
    print_safe("Calculation result: ", calcResult);

    // Example 2: Promise with complex return type
    print_safe("\nExample 2: Promise with complex return type (vector)");

    auto vectorPromise = atom::async::makePromiseFromFunction(
        [](int start, int end, int step) -> std::vector<int> {
            print_safe("Thread [", get_thread_id(), "] generating sequence [",
                       start, ", ", end, ") with step ", step);
            std::vector<int> result;
            for (int i = start; i < end; i += step) {
                result.push_back(i);
            }
            return result;
        },
        0, 20, 2);

    auto vectorFuture = vectorPromise.getEnhancedFuture();
    std::vector<int> sequence = vectorFuture.get();

    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Generated sequence: ";
        for (int val : sequence) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Example 3: Using reference parameters
    print_safe("\nExample 3: Using reference parameters");

    struct ResultAccumulator {
        std::mutex mutex;
        std::vector<int> values;

        void add(int value) {
            std::lock_guard<std::mutex> lock(mutex);
            values.push_back(value);
        }
    };

    ResultAccumulator accumulator;
    atom::async::Promise<void> refPromise;

    // Async task using reference parameters (lifecycle care needed)
    std::thread t3([&refPromise, &accumulator]() {
        print_safe("Thread [", get_thread_id(),
                   "] adding values to accumulator");
        for (int i = 0; i < 5; ++i) {
            accumulator.add(i * 10);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        refPromise.setValue();
    });

    auto refFuture = refPromise.getEnhancedFuture();
    refFuture.wait();  // Wait for task completion

    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Accumulated values: ";
        for (int val : accumulator.values) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    t3.join();

    // Example 4: Combining multiple Promises
    print_safe("\nExample 4: Combining multiple Promises with whenAll");

    std::vector<atom::async::Promise<int>> promises;
    std::vector<atom::async::EnhancedFuture<int>> futures;

    // Create multiple Promises and get their Futures
    for (int i = 0; i < 5; ++i) {
        promises.emplace_back();
        futures.push_back(promises.back().getEnhancedFuture());
    }

    // Get combined result
    auto combinedFuture = atom::async::EnhancedFuture<std::vector<int>>(
        atom::async::whenAll(promises).getFuture());

    // Start multiple threads setting different Promise values
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&promises, i]() {
            // Random delay to simulate different computation times
            std::this_thread::sleep_for(
                std::chrono::milliseconds((5 - i) * 50));
            print_safe("Thread [", get_thread_id(), "] setting value ", i * i);
            promises[i].setValue(i * i);
        });
    }

    print_safe("Waiting for all promises to complete...");
    std::vector<int> allResults = combinedFuture.get();

    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "All results: ";
        for (int val : allResults) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Example 5: Combining void Promises
    print_safe("\nExample 5: Combining void Promises");

    std::vector<atom::async::Promise<void>> voidPromises;
    for (int i = 0; i < 3; ++i) {
        voidPromises.emplace_back();
    }

    auto combinedVoidPromise = atom::async::whenAll(voidPromises);
    auto combinedVoidFuture = combinedVoidPromise.getEnhancedFuture();

    std::vector<std::thread> voidThreads;
    for (int i = 0; i < 3; ++i) {
        voidThreads.emplace_back([&voidPromises, i]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 * (i + 1)));
            print_safe("Thread [", get_thread_id(), "] completing void task ",
                       i);
            voidPromises[i].setValue();
        });
    }

    print_safe("Waiting for all void promises...");
    combinedVoidFuture.wait();
    print_safe("All void promises completed");

    for (auto& t : voidThreads) {
        t.join();
    }
}

// 3. Edge cases and special scenarios examples
void edge_cases_examples() {
    print_section("Edge Cases and Special Situations Examples");

    // Example 1: Cancelling a Promise
    print_safe("Example 1: Cancelling a Promise");

    atom::async::Promise<int> promise1;
    auto future1 = promise1.getEnhancedFuture();

    // Cancel the Promise - Fix: Handle the return value
    bool wasCancelled = promise1.cancel();
    print_safe("Promise was cancelled: ", wasCancelled ? "Yes" : "No");
    print_safe("Promise is in cancelled state: ",
               promise1.isCancelled() ? "Yes" : "No");

    // Try cancelling again (should return false) - Fix: Handle the return value
    bool secondCancel = promise1.cancel();
    print_safe("Second cancellation successful: ", secondCancel ? "Yes" : "No");

    // Try to get value from cancelled Promise
    try {
        print_safe("Attempting to get value from cancelled Promise...");
        int result = future1.get();
        print_safe("Value: ", result);  // Shouldn't reach here
    } catch (const atom::async::PromiseCancelledException& e) {
        print_safe("Correctly caught cancellation exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    // Example 2: Setting value on an already completed Promise
    print_safe("\nExample 2: Setting value on an already completed Promise");

    atom::async::Promise<std::string> promise2;
    auto future2 = promise2.getEnhancedFuture();

    // First set value
    promise2.setValue("First value");
    std::string value = future2.get();
    print_safe("First value retrieved: ", value);

    // Try setting value again
    try {
        print_safe("Attempting to set value again...");
        promise2.setValue("Second value");
        print_safe("Error: Second setValue didn't throw");
    } catch (const atom::async::PromiseCancelledException& e) {
        print_safe("Correctly caught exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    // Example 3: Using a moved Promise
    print_safe("\nExample 3: Using a moved Promise");

    atom::async::Promise<int> originalPromise;

    // Move the Promise
    auto movedPromise = std::move(originalPromise);

    // Try using the moved Promise
    try {
        print_safe("Setting value on moved Promise...");
        movedPromise.setValue(100);

        auto movedFuture = movedPromise.getEnhancedFuture();
        int movedResult = movedFuture.get();
        print_safe("Value from moved Promise: ", movedResult);

        // Now try using the original Promise (potentially undefined behavior)
        print_safe("Attempting to use the original Promise after move...");
        originalPromise.setValue(200);
        print_safe("Error: Using moved-from Promise didn't throw");
    } catch (const std::exception& e) {
        print_safe("Caught exception from moved-from Promise: ", e.what());
    }

    // Example 4: Empty Promises vector
    print_safe("\nExample 4: Empty Promises vector with whenAll");

    std::vector<atom::async::Promise<int>> emptyPromises;
    auto emptyAllPromise = atom::async::whenAll(emptyPromises);
    auto emptyAllFuture = emptyAllPromise.getEnhancedFuture();

    print_safe("Calling get() on whenAll with empty promises vector");
    std::vector<int> emptyResults = emptyAllFuture.get();
    print_safe("Empty results size: ", emptyResults.size());

    // Example 5: Using C++20 stop_token for cancellable operations
    print_safe("\nExample 5: Using stop_token for cancellable operations");

    atom::async::Promise<int> stoppablePromise;

    // Create stop_source and associated stop_token
    std::stop_source stopSource;
    std::stop_token stopToken = stopSource.get_token();

    // Make Promise cancellable
    stoppablePromise.setCancellable(stopToken);
    auto stoppableFuture = stoppablePromise.getEnhancedFuture();

    // Create thread executing long-running task
    std::thread longTask([&stoppablePromise]() {
        print_safe("Thread [", get_thread_id(), "] starting long-running task");

        try {
            // Simulate long-running task
            for (int i = 0; i < 10; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                print_safe("Task progress: ", i * 10, "%");

                // Check if cancelled
                if (stoppablePromise.isCancelled()) {
                    print_safe("Task detected cancellation, exiting early");
                    return;  // Early exit
                }
            }

            print_safe("Task completed successfully");
            stoppablePromise.setValue(999);
        } catch (const std::exception& e) {
            print_safe("Task encountered error: ", e.what());
        }
    });

    // Main thread cancels operation after short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    print_safe("Main thread requesting cancellation...");
    stopSource.request_stop();

    try {
        int result = stoppableFuture.get();
        print_safe("Got result despite cancellation: ", result);
    } catch (const atom::async::PromiseCancelledException& e) {
        print_safe("Promise was cancelled as expected: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    longTask.join();
}

// 4. Error handling examples
void error_handling_examples() {
    print_section("Error Handling Examples");

    // Example 1: Setting exception in a Promise
    print_safe("Example 1: Setting exception in a Promise");

    atom::async::Promise<int> promise1;
    auto future1 = promise1.getEnhancedFuture();

    std::thread t1([&promise1]() {
        try {
            print_safe("Thread [", get_thread_id(),
                       "] executing task that will fail");
            throw std::runtime_error("Intentional failure");
        } catch (const std::exception& e) {
            print_safe("Caught exception in worker thread: ", e.what());
            promise1.setException(std::current_exception());
        }
    });

    try {
        print_safe("Main thread waiting for potentially failing task...");
        int result = future1.get();
        print_safe("Result: ", result);  // Shouldn't reach here
    } catch (const std::runtime_error& e) {
        print_safe("Main thread correctly caught the exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Main thread caught unexpected exception: ", e.what());
    }

    t1.join();

    // Example 2: Exception propagation with makePromiseFromFunction
    print_safe(
        "\nExample 2: Exception propagation with makePromiseFromFunction");

    auto failingPromise =
        atom::async::makePromiseFromFunction([]() -> std::string {
            print_safe("Thread [", get_thread_id(),
                       "] executing function that will throw");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            throw std::invalid_argument("Invalid operation in function");
            return "This will never be returned";
        });

    auto failingFuture = failingPromise.getEnhancedFuture();

    try {
        print_safe("Waiting for failing function result...");
        std::string result = failingFuture.get();
        print_safe("Result: ", result);  // Shouldn't reach here
    } catch (const std::invalid_argument& e) {
        print_safe("Correctly caught specific exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught unexpected exception: ", e.what());
    }

    // Example 3: Setting exception on a cancelled Promise
    print_safe("\nExample 3: Setting exception on a cancelled Promise");

    atom::async::Promise<double> promise3;
    auto future3 = promise3.getEnhancedFuture();

    // First cancel the Promise - Fix: Handle the return value
    [[maybe_unused]] bool cancelled3 = promise3.cancel();

    // Try setting exception
    try {
        print_safe("Setting exception on cancelled Promise...");
        promise3.setException(
            std::make_exception_ptr(std::logic_error("Test exception")));
        print_safe("Error: setException didn't throw on cancelled Promise");
    } catch (const atom::async::PromiseCancelledException& e) {
        print_safe("Correctly caught cancellation exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught unexpected exception: ", e.what());
    }

    // Example 4: Exception handling in whenAll
    print_safe("\nExample 4: Exception handling in whenAll");

    std::vector<atom::async::Promise<int>> promises(3);

    // Start threads, two normal, one throwing exception
    std::thread t4a([&promises]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        promises[0].setValue(10);
    });

    std::thread t4b([&promises]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        try {
            throw std::runtime_error("Error in second promise");
        } catch (...) {
            promises[1].setException(std::current_exception());
        }
    });

    std::thread t4c([&promises]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        promises[2].setValue(30);
    });

    auto allPromise = atom::async::whenAll(promises);
    auto allFuture = allPromise.getEnhancedFuture();

    try {
        print_safe("Waiting for all promises (one will fail)...");
        std::vector<int> results = allFuture.get();
        print_safe("Error: whenAll should have propagated the exception");
    } catch (const std::runtime_error& e) {
        print_safe("Correctly caught exception from whenAll: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught unexpected exception: ", e.what());
    }

    t4a.join();
    t4b.join();
    t4c.join();

    // Example 5: Using null exception pointer
    print_safe("\nExample 5: Using null exception pointer");

    atom::async::Promise<int> promise5;
    auto future5 = promise5.getEnhancedFuture();

    try {
        print_safe("Setting null exception pointer...");
        promise5.setException(nullptr);
    } catch (const std::invalid_argument& e) {
        print_safe("Correctly caught exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    // Try getting value from potentially corrupted promise
    try {
        int result = future5.get();
        print_safe("Unexpectedly got result: ", result);
    } catch (const std::invalid_argument& e) {
        // If a generated invalid_argument exception was set
        print_safe("Got the generated invalid_argument exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception getting result: ", e.what());
    }
}

// 6. Promise coroutine support examples
void coroutine_examples() {
#ifdef __cpp_impl_coroutine
    print_section("Coroutine Support Examples");
    print_safe("C++20 coroutine support is available");

    // C++20 coroutine example
    auto coroutineExample = []()
        -> atom::async::EnhancedFuture<int> {  // Fix: Return EnhancedFuture
                                               // instead of std::future
        atom::async::Promise<int> promise;
        auto future = promise.getEnhancedFuture();

        // Simulate a coroutine doing async work
        std::thread t([promise = std::move(promise)]() mutable {
            print_safe("Async work in coroutine thread");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            promise.setValue(42);
        });
        t.detach();

        // In a real coroutine, we would use co_await here
        // Since a complete coroutine example needs more setup, we're just
        // simulating co_await promise;

        print_safe(
            "Coroutine example: This would use co_await in a real coroutine");
        return future;
    };

    auto result = coroutineExample();
    print_safe("Coroutine result: ", result.get());
#else
    print_section("Coroutine Support Examples");
    print_safe("C++20 coroutine support is not available in this compiler");
#endif
}

// Main function
int main() {
    try {
        std::cout << "====== Promise Usage Examples ======" << std::endl;

        basic_usage_examples();
        promise_cancellation_examples();
        parameter_combination_examples();
        edge_cases_examples();
        error_handling_examples();
        callback_examples();
        coroutine_examples();

        std::cout << "\n====== All Examples Completed ======" << std::endl;

        // Allow any background threads to complete before program exit
        std::cout << "⏳ Waiting for background threads to complete..."
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "✅ Program completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main" << std::endl;
        return 1;
    }

    return 0;
}
