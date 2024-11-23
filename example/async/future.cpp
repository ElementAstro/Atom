#include "future.hpp"

#include <iostream>
#include <thread>
#include <vector>

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

// Example exception handler
void exampleExceptionHandler(const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
}

// Example complete handler
void exampleCompleteHandler() {
    std::cout << "Complete: Task finished" << std::endl;
}

int main() {
    // Create an EnhancedFuture using makeEnhancedFuture
    auto future = makeEnhancedFuture(exampleFunction, 5, 10);

    // Chain another operation using then
    auto chainedFuture = future.then([](int result) {
        std::cout << "Chained result: " << result * 2 << std::endl;
        return result * 2;
    });

    // Set a completion callback
    chainedFuture.onComplete([](int result) {
        std::cout << "Completion callback: " << result << std::endl;
    });

    // Wait for the future with a timeout
    auto result = chainedFuture.waitFor(std::chrono::milliseconds(3000));
    if (result) {
        std::cout << "Result with timeout: " << *result << std::endl;
    } else {
        std::cout << "Timeout occurred" << std::endl;
    }

    // Check if the future is done
    if (chainedFuture.isDone()) {
        std::cout << "Future is done" << std::endl;
    }

/*
    // Retry the operation associated with the future
    auto retryFuture = future.retry(exampleFunction, 3);

    // Wait for the retry future to complete
    try {
        int retryResult = retryFuture.get();
        std::cout << "Retry result: " << retryResult << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Retry exception: " << e.what() << std::endl;
    }
*/
   
    // Create multiple futures
    auto future1 = makeEnhancedFuture(exampleFunction, 1, 2);
    auto future2 = makeEnhancedFuture(exampleFunction, 3, 4);
    auto future3 = makeEnhancedFuture(exampleFunction, 5, 6);

    /*
    // Use whenAll to wait for all futures to complete
    std::vector<std::shared_future<int>> futures = {future1.get(), future2.get(), future3.get()};
    auto allFuture = whenAll(futures.begin(), futures.end());

    // Get the results of all futures
    try {
        auto results = allFuture.get();
        std::cout << "Results of all futures: ";
        for (const auto& res : results) {
            std::cout << res << " ";
        }
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception while waiting for all futures: " << e.what() << std::endl;
    }
    */
    
    // Use whenAll with variadic template
    auto variadicFuture = whenAll(makeEnhancedFuture(exampleFunction, 7, 8),
                                  makeEnhancedFuture(exampleFunction, 9, 10));

    // Get the results of the variadic futures
    try {
        auto variadicResults = variadicFuture.get();
        std::cout << "Results of variadic futures: "
                  << std::get<0>(variadicResults) << " "
                  << std::get<1>(variadicResults) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception while waiting for variadic futures: " << e.what() << std::endl;
    }

    return 0;
}