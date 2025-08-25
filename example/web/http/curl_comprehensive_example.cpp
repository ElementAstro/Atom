/*
 * curl_comprehensive_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating advanced features of the Atom
 * CurlWrapper class
 */

#include "atom/log/loguru.hpp"
#include "atom/web/curl.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace atom::web;

// Global variables for async operations
std::atomic<bool> asyncCompleted{false};
std::atomic<size_t> asyncResponseCount{0};

void demonstrateBasicHTTPMethods() {
    std::cout << "\n=== Basic HTTP Methods ===\n";

    try {
        CurlWrapper curl;

        // GET request
        std::cout << "1. GET Request:\n";
        curl.setUrl("https://httpbin.org/get");
        curl.setRequestMethod("GET");
        curl.addHeader("User-Agent", "AtomCurlWrapper/1.0");
        curl.setTimeout(10L);

        std::string getResponse = curl.perform();
        std::cout << "GET Response length: " << getResponse.length()
                  << " characters\n";

        // POST request with JSON data
        std::cout << "\n2. POST Request with JSON:\n";
        curl.setUrl("https://httpbin.org/post");
        curl.setRequestMethod("POST");
        curl.addHeader("Content-Type", "application/json");
        curl.setRequestBody(R"({"name": "test", "value": 123})");

        std::string postResponse = curl.perform();
        std::cout << "POST Response length: " << postResponse.length()
                  << " characters\n";

        // PUT request
        std::cout << "\n3. PUT Request:\n";
        curl.setUrl("https://httpbin.org/put");
        curl.setRequestMethod("PUT");
        curl.setRequestBody("Updated data");

        std::string putResponse = curl.perform();
        std::cout << "PUT Response length: " << putResponse.length()
                  << " characters\n";

        // DELETE request
        std::cout << "\n4. DELETE Request:\n";
        curl.setUrl("https://httpbin.org/delete");
        curl.setRequestMethod("DELETE");
        curl.setRequestBody("");  // Clear previous body

        std::string deleteResponse = curl.perform();
        std::cout << "DELETE Response length: " << deleteResponse.length()
                  << " characters\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic HTTP methods: " << e.what() << "\n";
    }
}

void demonstrateAsynchronousOperations() {
    std::cout << "\n=== Asynchronous Operations ===\n";

    try {
        std::vector<std::unique_ptr<CurlWrapper>> curlInstances;

        // Create multiple CURL instances for async operations
        std::vector<std::string> urls = {"https://httpbin.org/delay/1",
                                         "https://httpbin.org/delay/2",
                                         "https://httpbin.org/delay/1"};

        std::cout << "Starting " << urls.size()
                  << " asynchronous requests...\n";

        for (size_t i = 0; i < urls.size(); ++i) {
            auto curl = std::make_unique<CurlWrapper>();

            curl->setUrl(urls[i]);
            curl->setRequestMethod("GET");
            curl->setTimeout(15L);

            // Set response callback
            curl->onResponse([i](const std::string& response) {
                std::cout << "Async response " << i
                          << " received (length: " << response.length()
                          << ")\n";
                asyncResponseCount++;
            });

            // Set error callback
            curl->onError([i](CURLcode code) {
                std::cout << "Async request " << i
                          << " failed: " << curl_easy_strerror(code) << "\n";
                asyncResponseCount++;
            });

            // Start async operation
            curl->performAsync();
            curlInstances.push_back(std::move(curl));
        }

        // Wait for all async operations to complete
        std::cout << "Waiting for async operations to complete...\n";
        auto startTime = std::chrono::steady_clock::now();

        while (asyncResponseCount < urls.size() &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(20)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for all to complete
        for (auto& curl : curlInstances) {
            curl->waitAll();
        }

        std::cout << "Async operations completed. Responses received: "
                  << asyncResponseCount << "/" << urls.size() << "\n";

        asyncResponseCount = 0;  // Reset for next example

    } catch (const std::exception& e) {
        std::cerr << "Error in async operations: " << e.what() << "\n";
    }
}

void demonstrateFileUpload() {
    std::cout << "\n=== File Upload Example ===\n";

    try {
        // Create a test file to upload
        std::string testFileName = "test_upload.txt";
        std::ofstream testFile(testFileName);
        testFile << "This is a test file for upload demonstration.\n";
        testFile << "It contains multiple lines of text.\n";
        testFile << "Upload timestamp: " << std::time(nullptr) << "\n";
        testFile.close();

        std::cout << "Created test file: " << testFileName << "\n";

        CurlWrapper curl;
        curl.setUrl("https://httpbin.org/post");
        curl.setRequestMethod("POST");
        curl.addHeader("User-Agent", "AtomCurlWrapper-FileUpload/1.0");

        // Set the file for upload
        curl.setUploadFile(testFileName);

        curl.onResponse([](const std::string& response) {
            std::cout << "File upload response received (length: "
                      << response.length() << ")\n";
        });

        curl.onError([](CURLcode code) {
            std::cout << "File upload failed: " << curl_easy_strerror(code)
                      << "\n";
        });

        std::cout << "Uploading file...\n";
        std::string response = curl.perform();

        if (!response.empty()) {
            std::cout << "File upload completed successfully!\n";
        }

        // Cleanup
        std::filesystem::remove(testFileName);
        std::cout << "Test file cleaned up.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in file upload: " << e.what() << "\n";
    }
}

void demonstrateAuthenticationAndHeaders() {
    std::cout << "\n=== Authentication and Custom Headers ===\n";

    try {
        CurlWrapper curl;

        // Basic authentication example
        std::cout << "1. Basic Authentication:\n";
        curl.setUrl("https://httpbin.org/basic-auth/testuser/testpass");
        curl.setRequestMethod("GET");
        curl.setBasicAuth("testuser", "testpass");
        curl.setTimeout(10L);

        std::string authResponse = curl.perform();
        std::cout << "Basic auth response length: " << authResponse.length()
                  << " characters\n";

        // Custom headers example
        std::cout << "\n2. Custom Headers:\n";
        curl.setUrl("https://httpbin.org/headers");
        curl.setRequestMethod("GET");

        // Add multiple custom headers
        curl.addHeader("X-Custom-Header", "CustomValue123");
        curl.addHeader("X-API-Key", "secret-api-key");
        curl.addHeader("X-Request-ID", "req-12345");
        curl.addHeader("Accept", "application/json");
        curl.addHeader("Accept-Language", "en-US,en;q=0.9");

        std::string headersResponse = curl.perform();
        std::cout << "Custom headers response length: "
                  << headersResponse.length() << " characters\n";

        // Bearer token authentication
        std::cout << "\n3. Bearer Token Authentication:\n";
        curl.setUrl("https://httpbin.org/bearer");
        curl.addHeader("Authorization",
                       "Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9");

        std::string bearerResponse = curl.perform();
        std::cout << "Bearer token response length: " << bearerResponse.length()
                  << " characters\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in authentication: " << e.what() << "\n";
    }
}

void demonstrateSSLAndSecurity() {
    std::cout << "\n=== SSL and Security Options ===\n";

    try {
        CurlWrapper curl;

        // HTTPS with SSL verification
        std::cout << "1. HTTPS with SSL verification:\n";
        curl.setUrl("https://httpbin.org/get");
        curl.setRequestMethod("GET");
        curl.setSSLOptions(true, true);  // Verify peer and host
        curl.setTimeout(10L);

        std::string secureResponse = curl.perform();
        std::cout << "Secure HTTPS response length: " << secureResponse.length()
                  << " characters\n";

        // HTTPS without SSL verification (for testing)
        std::cout << "\n2. HTTPS without SSL verification:\n";
        curl.setUrl("https://httpbin.org/get");
        curl.setSSLOptions(false, false);  // Don't verify peer or host

        std::string insecureResponse = curl.perform();
        std::cout << "Insecure HTTPS response length: "
                  << insecureResponse.length() << " characters\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in SSL operations: " << e.what() << "\n";
    }
}

void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Features ===\n";

    try {
        CurlWrapper curl;

        // Redirects and cookies
        std::cout << "1. Following redirects:\n";
        curl.setUrl("https://httpbin.org/redirect/3");  // Will redirect 3 times
        curl.setRequestMethod("GET");
        curl.setFollowLocation(true);
        curl.setTimeout(15L);

        std::string redirectResponse = curl.perform();
        std::cout << "Redirect response length: " << redirectResponse.length()
                  << " characters\n";

        // Speed limiting
        std::cout << "\n2. Download speed limiting:\n";
        curl.setUrl("https://httpbin.org/bytes/10240");  // Download 10KB
        curl.setMaxDownloadSpeed(1024);                  // Limit to 1KB/s

        auto startTime = std::chrono::steady_clock::now();
        std::string speedLimitedResponse = curl.perform();
        auto endTime = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            endTime - startTime);
        std::cout << "Speed limited download took " << duration.count()
                  << " seconds (expected ~10 seconds for 10KB at 1KB/s)\n";

        // Proxy support (commented out as it requires a proxy server)
        /*
        std::cout << "\n3. Proxy support:\n";
        curl.setUrl("https://httpbin.org/get");
        curl.setProxy("http://proxy.example.com:8080");
        curl.setMaxDownloadSpeed(0);  // Remove speed limit

        std::string proxyResponse = curl.perform();
        std::cout << "Proxy response length: " << proxyResponse.length() << "
        characters\n";
        */

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced features: " << e.what() << "\n";
    }
}

void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===\n";

    try {
        CurlWrapper curl;

        // Test with invalid URL
        std::cout << "1. Testing invalid URL:\n";
        curl.setUrl("https://this-domain-does-not-exist-12345.com");
        curl.setRequestMethod("GET");
        curl.setTimeout(5L);

        curl.onError([](CURLcode code) {
            std::cout << "Expected error caught: " << curl_easy_strerror(code)
                      << "\n";
        });

        try {
            std::string response = curl.perform();
            std::cout << "Unexpected success with invalid URL\n";
        } catch (const std::exception& e) {
            std::cout << "Exception caught as expected: " << e.what() << "\n";
        }

        // Test with timeout
        std::cout << "\n2. Testing timeout:\n";
        curl.setUrl("https://httpbin.org/delay/10");  // 10 second delay
        curl.setTimeout(2L);                          // 2 second timeout

        try {
            std::string response = curl.perform();
            std::cout << "Unexpected success with timeout\n";
        } catch (const std::exception& e) {
            std::cout << "Timeout exception caught as expected\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in error handling demo: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("curl_comprehensive_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "     ATOM CURL COMPREHENSIVE DEMO          \n";
    std::cout << "============================================\n";

    try {
        demonstrateBasicHTTPMethods();
        demonstrateAsynchronousOperations();
        demonstrateFileUpload();
        demonstrateAuthenticationAndHeaders();
        demonstrateSSLAndSecurity();
        demonstrateAdvancedFeatures();
        demonstrateErrorHandling();

        std::cout << "\n============================================\n";
        std::cout << "     CURL COMPREHENSIVE DEMO COMPLETED     \n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
