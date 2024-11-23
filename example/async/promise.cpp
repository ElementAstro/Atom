#include "atom/async/promise.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be executed asynchronously
int exampleFunction(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return a + b;
}

// Example callback function
void exampleCallback(int result) {
    std::cout << "Callback: Result is " << result << std::endl;
}

// Example void function to be executed asynchronously
void exampleVoidFunction() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Void function executed" << std::endl;
}

// Example void callback function
void exampleVoidCallback() {
    std::cout << "Void callback executed" << std::endl;
}

int main() {
    // Create an EnhancedPromise for a function with a result
    EnhancedPromise<int> promise;

    // Get the EnhancedFuture associated with the promise
    auto future = promise.getEnhancedFuture();

    // Add a callback to be called upon promise completion
    promise.onComplete(exampleCallback);

    // Execute the function asynchronously and set the promise value
    std::thread taskThread([&promise]() {
        try {
            int result = exampleFunction(5, 10);
            promise.setValue(result);
        } catch (...) {
            promise.setException(std::current_exception());
        }
    });

    // Wait for the future to complete and get the result
    int result = future.get();
    std::cout << "Promise result: " << result << std::endl;

    // Check if the promise is cancelled
    if (promise.isCancelled()) {
        std::cout << "Promise was cancelled" << std::endl;
    }

    // Join the task thread
    taskThread.join();

    // Create an EnhancedPromise for a void function
    EnhancedPromise<void> voidPromise;

    // Get the EnhancedFuture associated with the void promise
    auto voidFuture = voidPromise.getEnhancedFuture();

    // Add a callback to be called upon void promise completion
    voidPromise.onComplete(exampleVoidCallback);

    // Execute the void function asynchronously and set the promise value
    std::thread voidTaskThread([&voidPromise]() {
        try {
            exampleVoidFunction();
            voidPromise.setValue();
        } catch (...) {
            voidPromise.setException(std::current_exception());
        }
    });

    // Wait for the void future to complete
    voidFuture.get();

    // Check if the void promise is cancelled
    if (voidPromise.isCancelled()) {
        std::cout << "Void promise was cancelled" << std::endl;
    }

    // Join the void task thread
    voidTaskThread.join();

    return 0;
}