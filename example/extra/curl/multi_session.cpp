/**
 * @file multi_session.cpp
 * @brief Demonstration of atom::extra::curl::MultiSession functionality
 *
 * @details This example demonstrates:
 * - Creating a multi-session HTTP client
 * - Queuing multiple concurrent requests
 * - Executing requests in parallel
 * - Handling responses from multiple requests
 *
 * @author Atom Extra Examples
 * @date 2024
 */

#include "atom/extra/curl/multi_session.hpp"
#include "atom/extra/curl/request.hpp"

#include <atomic>
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

    MultiSession session;
    std::atomic<int> responseCount{0};

    std::cout << "Queuing multiple GET requests..." << std::endl;

    // Queue multiple requests using Request builder
    Request req1;
    req1.method(Request::Method::GET).url("https://httpbin.org/get?id=1");
    session.add_request(req1, [&responseCount](Response resp) {
        std::cout << "Response 1 received, status: " << resp.status_code()
                  << std::endl;
        responseCount++;
    });

    Request req2;
    req2.method(Request::Method::GET).url("https://httpbin.org/get?id=2");
    session.add_request(req2, [&responseCount](Response resp) {
        std::cout << "Response 2 received, status: " << resp.status_code()
                  << std::endl;
        responseCount++;
    });

    Request req3;
    req3.method(Request::Method::GET).url("https://httpbin.org/get?id=3");
    session.add_request(req3, [&responseCount](Response resp) {
        std::cout << "Response 3 received, status: " << resp.status_code()
                  << std::endl;
        responseCount++;
    });

    std::cout << "Executing all requests concurrently..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    session.perform();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Received " << responseCount.load() << " responses in "
              << duration.count() << "ms" << std::endl;
}

/**
 * @brief Demonstrates POST requests with multi-session
 *
 * Shows how to queue POST requests with different bodies.
 */
void postRequestsExample() {
    printSeparator("POST Requests Example");

    MultiSession session;
    std::atomic<int> responseCount{0};

    std::cout << "Queuing POST requests..." << std::endl;

    // Queue POST requests with JSON bodies
    Request req1;
    req1.method(Request::Method::POST)
        .url("https://httpbin.org/post")
        .header("Content-Type", "application/json")
        .body(R"({"name": "Alice", "id": 1})");
    session.add_request(req1, [&responseCount](Response resp) {
        std::cout << "POST response 1: " << resp.status_code() << std::endl;
        responseCount++;
    });

    Request req2;
    req2.method(Request::Method::POST)
        .url("https://httpbin.org/post")
        .header("Content-Type", "application/json")
        .body(R"({"name": "Bob", "id": 2})");
    session.add_request(req2, [&responseCount](Response resp) {
        std::cout << "POST response 2: " << resp.status_code() << std::endl;
        responseCount++;
    });

    std::cout << "Executing POST requests..." << std::endl;
    session.perform();

    std::cout << "Received " << responseCount.load() << " responses"
              << std::endl;
}

/**
 * @brief Demonstrates error handling with multi-session
 *
 * Shows how to handle errors in concurrent requests.
 */
void errorHandlingExample() {
    printSeparator("Error Handling Example");

    MultiSession session;

    // Request with error callback
    Request req;
    req.method(Request::Method::GET)
        .url("https://invalid-domain-12345.org/test");
    session.add_request(
        req,
        [](Response resp) {
            std::cout << "Unexpected success: " << resp.status_code()
                      << std::endl;
        },
        [](const Error& err) {
            std::cout << "Expected error occurred: " << err.what() << std::endl;
        });

    std::cout << "Executing request with error handling..." << std::endl;
    session.perform();
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
        errorHandlingExample();

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
