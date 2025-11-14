/**
 * @file lodash_example.cpp
 * @brief Comprehensive demonstration of atom::async::Debounce and Throttle
 * functionality
 *
 * @details This example demonstrates:
 * - Debounce mechanism for delaying function execution
 * - Throttle mechanism for rate-limiting function calls
 * - Leading and trailing edge execution
 * - Maximum wait time configuration
 * - Practical use cases (search input, window resize, API calls)
 *
 * @level Intermediate
 * @prerequisites Basic understanding of async programming, timing mechanisms
 * @related_examples timer_example.cpp, limiter_example.cpp
 *
 * @note Debounce and Throttle are utility components for controlling function
 * execution timing
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/lodash.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace atom::async;
using namespace std::chrono_literals;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// ============================================================================
// DEBOUNCE EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic debounce functionality
 *
 * Debounce delays function execution until after a specified time has elapsed
 * since the last call. Useful for search inputs, form validation, etc.
 */
void basicDebounceExample() {
    printSeparator("Basic Debounce Example");

    int callCount = 0;
    auto searchFunction = [&callCount](const std::string& query) {
        callCount++;
        std::cout << "Search executed for: '" << query << "' (call #"
                  << callCount << ")" << std::endl;
    };

    // Create debounce with 500ms delay
    Debounce<decltype(searchFunction)> debouncedSearch(searchFunction, 500ms);

    std::cout << "Simulating rapid search input..." << std::endl;
    debouncedSearch("a");
    std::this_thread::sleep_for(100ms);
    debouncedSearch("ap");
    std::this_thread::sleep_for(100ms);
    debouncedSearch("app");
    std::this_thread::sleep_for(100ms);
    debouncedSearch("appl");
    std::this_thread::sleep_for(100ms);
    debouncedSearch("apple");

    std::cout << "Waiting for debounce to trigger..." << std::endl;
    std::this_thread::sleep_for(600ms);

    std::cout << "Total search executions: " << callCount << " (should be 1)"
              << std::endl;
}

/**
 * @brief Demonstrates debounce with leading edge execution
 *
 * Leading edge means the function executes immediately on first call,
 * then subsequent calls are debounced.
 */
void leadingDebounceExample() {
    printSeparator("Leading Edge Debounce Example");

    int callCount = 0;
    auto buttonClick = [&callCount]() {
        callCount++;
        std::cout << "Button clicked! (execution #" << callCount << ")"
                  << std::endl;
    };

    // Create debounce with leading edge enabled
    Debounce<decltype(buttonClick)> debouncedClick(buttonClick, 300ms, true);

    std::cout << "Simulating rapid button clicks..." << std::endl;
    debouncedClick();  // Executes immediately
    std::this_thread::sleep_for(50ms);
    debouncedClick();  // Debounced
    std::this_thread::sleep_for(50ms);
    debouncedClick();  // Debounced
    std::this_thread::sleep_for(50ms);
    debouncedClick();  // Debounced

    std::cout << "Waiting for debounce period..." << std::endl;
    std::this_thread::sleep_for(400ms);

    std::cout << "Total executions: " << callCount
              << " (should be 1 from leading edge)" << std::endl;
}

/**
 * @brief Demonstrates debounce with maximum wait time
 *
 * Maximum wait ensures the function executes at least once within the specified
 * time, even if calls keep coming in.
 */
void maxWaitDebounceExample() {
    printSeparator("Max Wait Debounce Example");

    int callCount = 0;
    auto saveData = [&callCount]() {
        callCount++;
        std::cout << "Data saved! (save #" << callCount << ")" << std::endl;
    };

    // Create debounce with 200ms delay and 1000ms max wait
    Debounce<decltype(saveData)> debouncedSave(saveData, 200ms, false, 1000ms);

    std::cout << "Simulating continuous data changes..." << std::endl;
    for (int i = 0; i < 15; ++i) {
        debouncedSave();
        std::cout << "Data changed (iteration " << (i + 1) << ")" << std::endl;
        std::this_thread::sleep_for(100ms);  // Changes every 100ms
    }

    std::cout << "Waiting for final save..." << std::endl;
    std::this_thread::sleep_for(300ms);

    std::cout << "Total saves: " << callCount
              << " (should be at least 1 due to max wait)" << std::endl;
}

// ============================================================================
// THROTTLE EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic throttle functionality
 *
 * Throttle ensures a function is called at most once per specified time period.
 * Useful for scroll handlers, resize handlers, API rate limiting, etc.
 */
void basicThrottleExample() {
    printSeparator("Basic Throttle Example");

    int callCount = 0;
    auto scrollHandler = [&callCount]() {
        callCount++;
        std::cout << "Scroll position updated (update #" << callCount << ")"
                  << std::endl;
    };

    // Create throttle with 300ms interval
    Throttle<decltype(scrollHandler)> throttledScroll(scrollHandler, 300ms);

    std::cout << "Simulating rapid scroll events..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        throttledScroll();
        std::cout << "Scroll event " << (i + 1) << std::endl;
        std::this_thread::sleep_for(50ms);  // Events every 50ms
    }

    std::cout << "Waiting for final execution..." << std::endl;
    std::this_thread::sleep_for(400ms);

    std::cout << "Total scroll updates: " << callCount << " (should be ~2-3)"
              << std::endl;
}

/**
 * @brief Demonstrates throttle with leading edge execution
 */
void leadingThrottleExample() {
    printSeparator("Leading Edge Throttle Example");

    int callCount = 0;
    auto apiCall = [&callCount]() {
        callCount++;
        std::cout << "API request sent (request #" << callCount << ")"
                  << std::endl;
    };

    // Create throttle with leading edge enabled
    Throttle<decltype(apiCall)> throttledApi(apiCall, 500ms, true);

    std::cout << "Simulating rapid API requests..." << std::endl;
    throttledApi();  // Executes immediately
    std::this_thread::sleep_for(100ms);
    throttledApi();  // Throttled
    std::this_thread::sleep_for(100ms);
    throttledApi();                      // Throttled
    std::this_thread::sleep_for(400ms);  // Wait for throttle period
    throttledApi();                      // Executes (new period)

    std::cout << "Waiting for completion..." << std::endl;
    std::this_thread::sleep_for(200ms);

    std::cout << "Total API calls: " << callCount << std::endl;
}

/**
 * @brief Demonstrates practical use case: window resize handler
 */
void practicalThrottleExample() {
    printSeparator("Practical Throttle Example - Window Resize");

    int resizeCount = 0;
    auto resizeHandler = [&resizeCount](int width, int height) {
        resizeCount++;
        std::cout << "Window resized to " << width << "x" << height
                  << " (handler call #" << resizeCount << ")" << std::endl;
    };

    // Throttle resize events to once per 250ms
    Throttle<decltype(resizeHandler)> throttledResize(resizeHandler, 250ms);

    std::cout << "Simulating window resize events..." << std::endl;
    std::vector<std::pair<int, int>> sizes = {
        {800, 600},  {850, 650},  {900, 700}, {950, 750},
        {1000, 800}, {1050, 850}, {1100, 900}};

    for (const auto& [width, height] : sizes) {
        throttledResize(width, height);
        std::this_thread::sleep_for(80ms);
    }

    std::cout << "Waiting for final resize..." << std::endl;
    std::this_thread::sleep_for(300ms);

    std::cout << "Total resize handler calls: " << resizeCount << std::endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "=================================================="
              << std::endl;
    std::cout << "  Atom Async Lodash (Debounce/Throttle) Examples"
              << std::endl;
    std::cout << "=================================================="
              << std::endl;

    try {
        // Debounce examples
        basicDebounceExample();
        leadingDebounceExample();
        maxWaitDebounceExample();

        // Throttle examples
        basicThrottleExample();
        leadingThrottleExample();
        practicalThrottleExample();

        std::cout << "\n=================================================="
                  << std::endl;
        std::cout << "  All examples completed successfully!" << std::endl;
        std::cout << "=================================================="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
