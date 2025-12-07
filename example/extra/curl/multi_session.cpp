/**
 * @file multi_session.cpp
 * @brief Demonstration of atom::extra::curl::MultiSession functionality
 *
 * @details This example demonstrates:
 * - Creating a multi-session HTTP client
 * - Queuing multiple concurrent requests
 * - Executing requests in parallel
 * - Handling responses from multiple requests
 * - Configuring concurrency limits
 *
 * @level Intermediate
 * @prerequisites Basic understanding of HTTP and async programming
 * @related_examples session.cpp, rest_client.cpp
 *
 * @author Atom Extra Examples
 * @date 2024
 */

#include "atom/extra/curl/multi_session.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::extra::curl;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

// ============================================================================
// MULTI-SESSION EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic multi-session usage
 *
 * Shows how to queue multiple requests and execute them concurrently.
 */
void basicMultiSessionExample() {
    printSeparator("Basic Multi-Session Example");

    MultiSession client;

    std::cout << "Queuing multiple GET requests..." << std::endl;

    // Queue multiple requests
    client.addGet("https://httpbin.org/get?id=1");
    client.addGet("https://httpbin.org/get?id=2");
    client.addGet("https://httpbin.org/get?id=3");

    std::cout << "Pending requests: " << client.pendingCount() << std::endl;

    std::cout << "Executing all requests concurrently..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    auto responses = client.executeAll();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Received " << responses.size() << " responses in "
              << duration.count() << "ms" << std::endl;

    for (size_t i = 0; i < responses.size(); ++i) {
        std::cout << "Response " << (i + 1)
                  << " length: " << responses[i].length() << " bytes"
                  << std::endl;
    }
}

/**
 * @brief Demonstrates POST requests with multi-session
 *
 * Shows how to queue POST requests with different bodies.
 */
void postRequestsExample() {
    printSeparator("POST Requests Example");

    MultiSession client;

    std::cout << "Queuing POST requests..." << std::endl;

    // Queue POST requests with JSON bodies
    client.addPost("https://httpbin.org/post", R"({"name": "Alice", "id": 1})",
                   {{"Content-Type", "application/json"}});
    client.addPost("https://httpbin.org/post", R"({"name": "Bob", "id": 2})",
                   {{"Content-Type", "application/json"}});
    client.addPost("https://httpbin.org/post",
                   R"({"name": "Charlie", "id": 3})",
                   {{"Content-Type", "application/json"}});

    std::cout << "Executing POST requests..." << std::endl;
    auto responses = client.executeAll();

    std::cout << "Received " << responses.size() << " responses" << std::endl;
}

/**
 * @brief Demonstrates configuring concurrency limits
 *
 * Shows how to limit the number of concurrent requests.
 */
void concurrencyLimitExample() {
    printSeparator("Concurrency Limit Example");

    MultiSession client;

    // Set maximum concurrent requests to 2
    client.setMaxConcurrent(2);
    client.setTimeout(5000);  // 5 second timeout

    std::cout << "Queuing 5 requests with max 2 concurrent..." << std::endl;

    for (int i = 1; i <= 5; ++i) {
        client.addGet("https://httpbin.org/delay/1?id=" + std::to_string(i));
    }

    std::cout << "Executing with concurrency limit..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    auto responses = client.executeAll();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Completed " << responses.size() << " requests in "
              << duration.count() << "ms" << std::endl;
    std::cout << "(With unlimited concurrency, this would be ~1 second)"
              << std::endl;
}

/**
 * @brief Demonstrates clearing and reusing a multi-session
 *
 * Shows how to clear pending requests and reuse the client.
 */
void clearAndReuseExample() {
    printSeparator("Clear and Reuse Example");

    MultiSession client;

    // Queue some requests
    client.addGet("https://httpbin.org/get?batch=1");
    client.addGet("https://httpbin.org/get?batch=1");
    std::cout << "Queued batch 1: " << client.pendingCount() << " requests"
              << std::endl;

    // Execute first batch
    auto batch1 = client.executeAll();
    std::cout << "Batch 1 completed: " << batch1.size() << " responses"
              << std::endl;

    // Queue more requests (client is automatically cleared after executeAll)
    client.addGet("https://httpbin.org/get?batch=2");
    client.addGet("https://httpbin.org/get?batch=2");
    client.addGet("https://httpbin.org/get?batch=2");
    std::cout << "Queued batch 2: " << client.pendingCount() << " requests"
              << std::endl;

    // Clear without executing
    client.clear();
    std::cout << "After clear: " << client.pendingCount() << " requests"
              << std::endl;

    // Queue final batch
    client.addGet("https://httpbin.org/get?batch=3");
    auto batch3 = client.executeAll();
    std::cout << "Batch 3 completed: " << batch3.size() << " responses"
              << std::endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "=================================================="
              << std::endl;
    std::cout << "  Atom Extra cURL Multi-Session Examples" << std::endl;
    std::cout << "=================================================="
              << std::endl;

    try {
        basicMultiSessionExample();
        postRequestsExample();
        concurrencyLimitExample();
        clearAndReuseExample();

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
