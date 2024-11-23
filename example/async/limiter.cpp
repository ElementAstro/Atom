#include "atom/async/limiter.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example function to be rate-limited, debounced, or throttled
void exampleFunction() {
    std::cout << "Function called at "
              << std::chrono::steady_clock::now().time_since_epoch().count()
              << std::endl;
}

int main() {
    // RateLimiter example
    RateLimiter rateLimiter;
    rateLimiter.setFunctionLimit("exampleFunction", 3, std::chrono::seconds(5));

    for (int i = 0; i < 5; ++i) {
        auto awaiter = rateLimiter.acquire("exampleFunction");
        if (!awaiter.await_ready()) {
            std::cout << "Rate limit exceeded, waiting..." << std::endl;
            awaiter.await_suspend(std::noop_coroutine());
        }
        exampleFunction();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Print the log of requests
    rateLimiter.printLog();

    // Get the number of rejected requests
    size_t rejectedRequests =
        rateLimiter.getRejectedRequests("exampleFunction");
    std::cout << "Number of rejected requests: " << rejectedRequests
              << std::endl;

    // Debounce example
    Debounce debounce(exampleFunction, std::chrono::milliseconds(500), true);

    for (int i = 0; i < 5; ++i) {
        debounce();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Flush the debounced function
    debounce.flush();

    // Get the call count
    size_t debounceCallCount = debounce.callCount();
    std::cout << "Debounce call count: " << debounceCallCount << std::endl;

    // Throttle example
    Throttle throttle(exampleFunction, std::chrono::milliseconds(500), true);

    for (int i = 0; i < 5; ++i) {
        throttle();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Get the call count
    size_t throttleCallCount = throttle.callCount();
    std::cout << "Throttle call count: " << throttleCallCount << std::endl;

    return 0;
}