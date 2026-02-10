/**
 * @file promise_utils_example.cpp
 * @brief Demonstration of promise_utils.hpp utility functions
 *
 * @details This example demonstrates:
 * - makeResolvedPromise and makeRejectedPromise
 * - whenAny for racing promises
 * - delay for timed promises
 * - retry with exponential backoff
 * - withTimeout for promise timeouts
 *
 * @level Intermediate
 * @prerequisites Basic understanding of Promise concepts
 */

#include "atom/async/core/promise_utils.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace atom::async;

// Thread-safe print function
std::mutex print_mutex;

template <typename... Args>
void print(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    (std::cout << ... << args) << std::endl;
}

void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n" << std::endl;
}

// ============================================================================
// Example 1: makeResolvedPromise and makeRejectedPromise
// ============================================================================

void example_resolved_rejected() {
    print_section("Example 1: makeResolvedPromise and makeRejectedPromise");

    // Create a resolved promise with a value
    print("Creating resolved promise with value 42...");
    auto resolvedPromise = makeResolvedPromise(42);
    auto future = resolvedPromise.getFuture();
    print("Value from resolved promise: ", future.get());

    // Create a resolved promise with a string
    print("\nCreating resolved promise with string...");
    auto stringPromise = makeResolvedPromise(std::string("Hello, World!"));
    auto stringFuture = stringPromise.getFuture();
    print("String value: ", stringFuture.get());

    // Create a rejected promise
    print("\nCreating rejected promise...");
    auto rejectedPromise = makeRejectedPromise<int>(
        std::make_exception_ptr(std::runtime_error("Intentional error")));
    auto rejectedFuture = rejectedPromise.getFuture();

    try {
        rejectedFuture.get();
    } catch (const std::runtime_error& e) {
        print("Caught expected exception: ", e.what());
    }

    print("\n✅ makeResolvedPromise/makeRejectedPromise example completed!");
}

// ============================================================================
// Example 2: whenAny - Racing Promises
// ============================================================================

void example_when_any() {
    print_section("Example 2: whenAny - Racing Promises");

    std::vector<Promise<int>> promises;
    for (int i = 0; i < 5; ++i) {
        promises.emplace_back();
    }

    print("Created 5 promises, racing to see which completes first...");

    auto resultPromise = whenAny(promises);  // Returns shared_ptr<Promise<int>>

    // Simulate different completion times
    std::thread t1([&promises]() {
        std::this_thread::sleep_for(100ms);
        print("  Thread 1: Setting promise[0] = 10");
        try {
            promises[0].setValue(10);
        } catch (...) {
            print("  Thread 1: Promise already resolved");
        }
    });

    std::thread t2([&promises]() {
        std::this_thread::sleep_for(50ms);  // This one is fastest
        print("  Thread 2: Setting promise[1] = 20 (fastest!)");
        try {
            promises[1].setValue(20);
        } catch (...) {
            print("  Thread 2: Promise already resolved");
        }
    });

    std::thread t3([&promises]() {
        std::this_thread::sleep_for(150ms);
        print("  Thread 3: Setting promise[2] = 30");
        try {
            promises[2].setValue(30);
        } catch (...) {
            print("  Thread 3: Promise already resolved");
        }
    });

    t1.join();
    t2.join();
    t3.join();

    print("\n✅ whenAny example completed!");
}

// ============================================================================
// Example 3: delay - Timed Promises
// ============================================================================

void example_delay() {
    print_section("Example 3: delay - Timed Promises");

    print("Creating a promise that resolves after 200ms...");
    auto start = std::chrono::steady_clock::now();

    auto delayPromise = delay(200ms);
    auto future = delayPromise.getFuture();

    print("Waiting for delay promise...");
    future.get();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    print("Delay completed after ", elapsed.count(), "ms");
    print("\n✅ delay example completed!");
}

// ============================================================================
// Example 4: retry - Exponential Backoff
// ============================================================================

void example_retry() {
    print_section("Example 4: retry - Exponential Backoff");

    std::atomic<int> attemptCount{0};

    print("Simulating a flaky operation that fails twice then succeeds...");

    auto retryPromise = retry<int>(
        [&attemptCount]() -> int {
            int attempt = ++attemptCount;
            print("  Attempt ", attempt, "...");

            if (attempt < 3) {
                print("  ❌ Attempt ", attempt, " failed!");
                throw std::runtime_error("Simulated failure");
            }

            print("  ✅ Attempt ", attempt, " succeeded!");
            return 42;
        },
        5,    // max retries
        50ms  // initial delay (doubles each retry)
    );

    auto future = retryPromise.getFuture();
    int result = future.get();

    print("\nFinal result: ", result);
    print("Total attempts: ", attemptCount.load());
    print("\n✅ retry example completed!");
}

// ============================================================================
// Example 5: retryVoid - Retry for Void Functions
// ============================================================================

void example_retry_void() {
    print_section("Example 5: retryVoid - Retry for Void Functions");

    std::atomic<int> attemptCount{0};

    print("Simulating a void operation that fails once then succeeds...");

    auto retryPromise = retryVoid(
        [&attemptCount]() {
            int attempt = ++attemptCount;
            print("  Attempt ", attempt, "...");

            if (attempt < 2) {
                print("  ❌ Attempt ", attempt, " failed!");
                throw std::runtime_error("Simulated failure");
            }

            print("  ✅ Attempt ", attempt, " succeeded!");
        },
        3,    // max retries
        30ms  // initial delay
    );

    auto future = retryPromise.getFuture();
    future.get();

    print("\nOperation completed successfully!");
    print("Total attempts: ", attemptCount.load());
    print("\n✅ retryVoid example completed!");
}

// ============================================================================
// Example 6: withTimeout - Promise with Timeout
// ============================================================================

void example_with_timeout() {
    print_section("Example 6: withTimeout - Promise with Timeout");

    // Example 6a: Promise completes before timeout
    {
        print("Test A: Promise completes before timeout");
        Promise<int> promise;
        auto timeoutPromise = withTimeout(promise, 500ms);

        // Complete quickly
        std::thread([&promise]() {
            std::this_thread::sleep_for(100ms);
            print("  Setting value 42...");
            promise.setValue(42);
        }).detach();

        auto future = timeoutPromise->getFuture();
        try {
            int result = future.get();
            print("  ✅ Got result: ", result);
        } catch (const std::exception& e) {
            print("  ❌ Exception: ", e.what());
        }
    }

    // Example 6b: Promise times out
    {
        print("\nTest B: Promise times out");
        Promise<int> promise;
        auto timeoutPromise = withTimeout(promise, 50ms);

        // Don't complete the promise - let it timeout

        auto future = timeoutPromise->getFuture();
        try {
            future.get();
            print("  ❌ Should have thrown!");
        } catch (const std::runtime_error& e) {
            print("  ✅ Caught expected timeout: ", e.what());
        }
    }

    print("\n✅ withTimeout example completed!");
}

// ============================================================================
// Example 7: Combined Usage
// ============================================================================

void example_combined() {
    print_section("Example 7: Combined Usage");

    print("Combining multiple promise utilities...");

    // First, wait a bit
    print("\n1. Waiting 100ms using delay...");
    delay(100ms).getFuture().get();
    print("   Delay completed.");

    // Create a resolved promise
    print("\n2. Creating resolved promise...");
    auto resolved = makeResolvedPromise(100);
    print("   Value: ", resolved.getFuture().get());

    // Retry an operation
    print("\n3. Retrying an operation with backoff...");
    std::atomic<int> counter{0};
    auto retried = retry<std::string>(
        [&counter]() -> std::string {
            if (++counter < 2) {
                throw std::runtime_error("Not yet");
            }
            return "Success!";
        },
        3, 10ms);
    print("   Result: ", retried.getFuture().get());

    print("\n✅ Combined usage example completed!");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "========================================================\n";
    std::cout << "       Promise Utils Example - Atom Async Library       \n";
    std::cout << "========================================================\n";

    try {
        example_resolved_rejected();
        example_when_any();
        example_delay();
        example_retry();
        example_retry_void();
        example_with_timeout();
        example_combined();

        std::cout << "\n";
        std::cout
            << "========================================================\n";
        std::cout
            << "           All examples completed successfully!          \n";
        std::cout
            << "========================================================\n";

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
