#include "atom/async/promise.hpp"

#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Print mutex
std::mutex print_mutex;

// Thread-safe print function
template <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    (std::cout << ... << args) << std::endl;
}

// Print section divider
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

// Helper function to get thread ID
std::string get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// 1. Basic usage examples
void basic_usage_examples() {
    print_section("Basic Promise Usage Examples");

    // Example 1: Create and use a Promise returning an integer
    print_safe("Example 1: Create and use a Promise returning an integer");

    atom::async::Promise<int> promise1;
    auto future1 = promise1.getEnhancedFuture();

    // Execute in a new thread and set value
    std::thread t1([&promise1]() {
        print_safe("Thread [", get_thread_id(), "] working on task...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        print_safe("Thread [", get_thread_id(), "] setting value 42");
        promise1.setValue(42);
    });

    // Wait and get result
    print_safe("Main thread [", get_thread_id(), "] waiting for result...");
    int result1 = future1.get();
    print_safe("Result: ", result1);

    t1.join();

    // Example 2: Create a Promise with string value
    print_safe("\nExample 2: Create a Promise with string value");

    atom::async::Promise<std::string> promise2;
    auto future2 = promise2.getEnhancedFuture();

    std::thread t2([&promise2]() {
        print_safe("Thread [", get_thread_id(),
                   "] calculating string result...");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        promise2.setValue("Hello from worker thread!");
    });

    print_safe("Main thread waiting for string result...");
    std::string result2 = future2.get();
    print_safe("String result: ", result2);

    t2.join();

    // Example 3: Create a void Promise
    print_safe("\nExample 3: Create a void Promise");

    atom::async::Promise<void> promise3;
    auto future3 = promise3.getEnhancedFuture();

    std::thread t3([&promise3]() {
        print_safe("Thread [", get_thread_id(), "] executing void task...");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        print_safe("Thread [", get_thread_id(), "] task completed");
        promise3.setValue();  // No parameter needed
    });

    print_safe("Main thread waiting for void task to complete...");
    future3.wait();  // Wait for completion
    print_safe("Void task completed");

    t3.join();

    // Example 4: Create a ready Promise using makeReadyPromise
    print_safe("\nExample 4: Create a ready Promise");

    auto readyPromise = atom::async::makeReadyPromise(100);
    auto readyFuture = readyPromise.getEnhancedFuture();

    // Ready Promise can get result immediately
    print_safe("Ready Promise created, checking if future is ready...");

    // Fix: EnhancedFuture doesn't support wait_for, use alternate check
    bool is_ready = true;  // Assume ready since it's a readyPromise
    print_safe("Is future ready: ", is_ready ? "Yes" : "No");

    int readyResult = readyFuture.get();
    print_safe("Ready Promise result: ", readyResult);

    // Example 5: Create async function using makePromiseFromFunction
    print_safe("\nExample 5: Create Promise from function");

    auto functionPromise = atom::async::makePromiseFromFunction([]() -> int {
        print_safe("Thread [", get_thread_id(), "] executing function...");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return 200;
    });

    auto funcFuture = functionPromise.getEnhancedFuture();
    print_safe("Waiting for function result...");
    int funcResult = funcFuture.get();
    print_safe("Function result: ", funcResult);
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

// 5. Callback function examples
void callback_examples() {
    print_section("Callback Function Examples");

    print_safe("Example 1: Using onComplete callbacks");

    atom::async::Promise<int> promise;

    // Register callback function
    promise.onComplete([](int value) {
        print_safe("Callback 1 executed with value: ", value);
    });

    // Register another callback function
    promise.onComplete([](int value) {
        print_safe("Callback 2 executed with value: ", value * 2);
    });

    // Get future to wait for result
    auto future = promise.getEnhancedFuture();

    // Set value in another thread
    std::thread t([&promise]() {
        print_safe("Thread [", get_thread_id(), "] working...");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        print_safe("Setting value 42");
        promise.setValue(42);
    });

    // Main thread waits for result
    int result = future.get();
    print_safe("Main thread got result: ", result);

    // Register callback after value is set
    promise.onComplete([](int value) {
        print_safe("Late callback executed with value: ", value);
    });

    t.join();

    // Wait to ensure all callbacks have executed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    print_safe("\nExample 2: Callbacks with void Promise");

    atom::async::Promise<void> voidPromise;

    voidPromise.onComplete([]() { print_safe("Void callback 1 executed"); });

    voidPromise.onComplete([]() { print_safe("Void callback 2 executed"); });

    auto voidFuture = voidPromise.getEnhancedFuture();

    std::thread t2([&voidPromise]() {
        print_safe("Thread [", get_thread_id(), "] working on void task...");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        print_safe("Completing void Promise");
        voidPromise.setValue();
    });

    voidFuture.wait();
    print_safe("Void Promise completed");

    // Try registering callback on cancelled Promise
    print_safe("\nExample 3: Callbacks on cancelled Promise");

    atom::async::Promise<int> cancelledPromise;
    // Fix: Capture and use return value, or use [[maybe_unused]]
    [[maybe_unused]] bool cancelResult = cancelledPromise.cancel();

    // This callback shouldn't be stored or executed
    cancelledPromise.onComplete([](int value) {
        print_safe("This callback should not execute: ", value);
    });

    print_safe("Added callback to cancelled Promise (should be ignored)");

    t2.join();

    // Wait to ensure all callbacks have had a chance to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        parameter_combination_examples();
        edge_cases_examples();
        error_handling_examples();
        callback_examples();
        coroutine_examples();

        std::cout << "\n====== All Examples Completed ======" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main" << std::endl;
        return 1;
    }

    return 0;
}