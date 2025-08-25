#include "atom/extra/curl/rest_client.hpp"
#include "atom/extra/curl/error.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== CURL RestClient Example ===" << std::endl;

        // Create a REST client for JSONPlaceholder API
        RestClient client("https://jsonplaceholder.typicode.com");

        // Set default headers for all requests
        client.set_default_header("User-Agent", "Atom-RestClient/1.0");
        client.set_default_header("Accept", "application/json");

        // 1. GET all posts
        std::cout << "\n1. GET All Posts (first 5):" << std::endl;
        try {
            auto response = client.get("/posts");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Content-Type: " << response.header("Content-Type")
                      << std::endl;

            // Show first 500 characters of response
            std::string body = response.text();
            std::cout << "Response (first 500 chars): " << body.substr(0, 500)
                      << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET posts failed: " << e.what() << std::endl;
        }

        // 2. GET specific post
        std::cout << "\n2. GET Specific Post (ID: 1):" << std::endl;
        try {
            auto response = client.get("/posts/1");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET specific post failed: " << e.what() << std::endl;
        }

        // 3. POST new post
        std::cout << "\n3. POST New Post:" << std::endl;
        try {
            std::string json_data = R"({
                "title": "My New Post",
                "body": "This is the content of my new post created via RestClient",
                "userId": 1
            })";

            client.set_default_header("Content-Type", "application/json");
            auto response = client.post("/posts", json_data);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "POST new post failed: " << e.what() << std::endl;
        }

        // 4. PUT update post
        std::cout << "\n4. PUT Update Post (ID: 1):" << std::endl;
        try {
            std::string json_data = R"({
                "id": 1,
                "title": "Updated Post Title",
                "body": "This post has been updated via RestClient PUT request",
                "userId": 1
            })";

            auto response = client.put("/posts/1", json_data);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "PUT update post failed: " << e.what() << std::endl;
        }

        // 5. PATCH partial update
        std::cout << "\n5. PATCH Partial Update (ID: 1):" << std::endl;
        try {
            std::string json_data = R"({
                "title": "Partially Updated Title"
            })";

            auto response = client.patch("/posts/1", json_data);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "PATCH update failed: " << e.what() << std::endl;
        }

        // 6. DELETE post
        std::cout << "\n6. DELETE Post (ID: 1):" << std::endl;
        try {
            auto response = client.delete_("/posts/1");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Delete successful!" << std::endl;
        } catch (const Error& e) {
            std::cerr << "DELETE post failed: " << e.what() << std::endl;
        }

        // 7. GET with query parameters
        std::cout << "\n7. GET with Query Parameters:" << std::endl;
        try {
            std::map<std::string, std::string> params = {{"userId", "1"},
                                                         {"_limit", "3"}};
            auto response = client.get("/posts", params);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET with params failed: " << e.what() << std::endl;
        }

        // 8. Working with different endpoints
        std::cout << "\n8. GET Comments for Post 1:" << std::endl;
        try {
            auto response = client.get("/posts/1/comments");
            std::cout << "Status: " << response.status_code() << std::endl;

            // Show first 300 characters
            std::string body = response.text();
            std::cout << "Comments (first 300 chars): " << body.substr(0, 300)
                      << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET comments failed: " << e.what() << std::endl;
        }

        // 9. GET Users
        std::cout << "\n9. GET All Users:" << std::endl;
        try {
            auto response = client.get("/users");
            std::cout << "Status: " << response.status_code() << std::endl;

            // Show first 400 characters
            std::string body = response.text();
            std::cout << "Users (first 400 chars): " << body.substr(0, 400)
                      << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET users failed: " << e.what() << std::endl;
        }

        // 10. Error handling - non-existent endpoint
        std::cout << "\n10. Error Handling - Non-existent Endpoint:"
                  << std::endl;
        try {
            auto response = client.get("/nonexistent");
            std::cout << "Status: " << response.status_code() << std::endl;
            if (response.status_code() == 404) {
                std::cout << "Correctly received 404 for non-existent endpoint"
                          << std::endl;
            }
        } catch (const Error& e) {
            std::cerr << "Expected error for non-existent endpoint: "
                      << e.what() << std::endl;
        }

        // 11. Cache demonstration
        std::cout << "\n11. Cache Demonstration:" << std::endl;
        try {
            // First request (should hit the server)
            auto start = std::chrono::high_resolution_clock::now();
            auto response1 = client.get("/posts/1");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration1 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "First request - Status: " << response1.status_code()
                      << ", Duration: " << duration1.count() << "ms"
                      << std::endl;

            // Second request (might be cached)
            start = std::chrono::high_resolution_clock::now();
            auto response2 = client.get("/posts/1");
            end = std::chrono::high_resolution_clock::now();
            auto duration2 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "Second request - Status: " << response2.status_code()
                      << ", Duration: " << duration2.count() << "ms"
                      << std::endl;

            if (duration2 < duration1) {
                std::cout << "Second request was faster (possibly cached)"
                          << std::endl;
            }
        } catch (const Error& e) {
            std::cerr << "Cache demo failed: " << e.what() << std::endl;
        }

        // 12. Clear cache
        std::cout << "\n12. Clear Cache:" << std::endl;
        client.clear_cache();
        std::cout << "Cache cleared successfully" << std::endl;

        std::cout << "\n=== RestClient Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
