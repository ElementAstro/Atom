#include "atom/extra/curl/session.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/request.hpp"
#include "atom/extra/curl/response.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== CURL Session Example ===" << std::endl;

        // Create a session
        Session session;

        // 1. Basic GET request
        std::cout << "\n1. Basic GET Request:" << std::endl;
        try {
            auto response = session.get("https://httpbin.org/get");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Content-Type: " << response.header("Content-Type")
                      << std::endl;
            std::cout << "Response body (first 200 chars): "
                      << response.text().substr(0, 200) << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "GET request failed: " << e.what() << std::endl;
        }

        // 2. POST request with data
        std::cout << "\n2. POST Request with Data:" << std::endl;
        try {
            std::map<std::string, std::string> data = {
                {"key1", "value1"},
                {"key2", "value2"},
                {"message", "Hello from CURL Session!"}};
            auto response = session.post("https://httpbin.org/post", data);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response body (first 300 chars): "
                      << response.text().substr(0, 300) << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "POST request failed: " << e.what() << std::endl;
        }

        // 3. Request with custom headers
        std::cout << "\n3. Request with Custom Headers:" << std::endl;
        try {
            session.set_header("User-Agent", "Atom-CURL-Session/1.0");
            session.set_header("X-Custom-Header", "CustomValue");
            session.set_header("Accept", "application/json");

            auto response = session.get("https://httpbin.org/headers");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response body (first 400 chars): "
                      << response.text().substr(0, 400) << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "Headers request failed: " << e.what() << std::endl;
        }

        // 4. Request with timeout
        std::cout << "\n4. Request with Timeout:" << std::endl;
        try {
            session.set_timeout(5s);  // 5 second timeout
            auto response = session.get("https://httpbin.org/delay/2");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Request completed within timeout" << std::endl;
        } catch (const Error& e) {
            std::cerr << "Timeout request failed: " << e.what() << std::endl;
        }

        // 5. PUT request
        std::cout << "\n5. PUT Request:" << std::endl;
        try {
            std::string json_data =
                R"({"name": "John Doe", "age": 30, "city": "New York"})";
            session.set_header("Content-Type", "application/json");
            auto response = session.put("https://httpbin.org/put", json_data);
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response body (first 300 chars): "
                      << response.text().substr(0, 300) << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "PUT request failed: " << e.what() << std::endl;
        }

        // 6. DELETE request
        std::cout << "\n6. DELETE Request:" << std::endl;
        try {
            auto response = session.delete_("https://httpbin.org/delete");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response body (first 200 chars): "
                      << response.text().substr(0, 200) << "..." << std::endl;
        } catch (const Error& e) {
            std::cerr << "DELETE request failed: " << e.what() << std::endl;
        }

        // 7. Request with authentication
        std::cout << "\n7. Request with Basic Authentication:" << std::endl;
        try {
            session.set_auth("testuser", "testpass");
            auto response =
                session.get("https://httpbin.org/basic-auth/testuser/testpass");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Authentication successful!" << std::endl;
        } catch (const Error& e) {
            std::cerr << "Auth request failed: " << e.what() << std::endl;
        }

        // 8. Request with proxy (commented out as it requires a proxy server)
        /*
        std::cout << "\n8. Request with Proxy:" << std::endl;
        try {
            session.set_proxy("http://proxy.example.com:8080");
            auto response = session.get("https://httpbin.org/ip");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Response: " << response.text() << std::endl;
        } catch (const Error& e) {
            std::cerr << "Proxy request failed: " << e.what() << std::endl;
        }
        */

        // 9. Request with SSL verification disabled (for testing only)
        std::cout << "\n9. Request with SSL Verification Disabled:"
                  << std::endl;
        try {
            session.set_verify_ssl(false);
            auto response = session.get("https://httpbin.org/get");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "SSL verification disabled request successful"
                      << std::endl;
        } catch (const Error& e) {
            std::cerr << "SSL disabled request failed: " << e.what()
                      << std::endl;
        }

        // 10. Request with follow redirects
        std::cout << "\n10. Request with Redirect Following:" << std::endl;
        try {
            session.set_follow_redirects(true);
            auto response = session.get("https://httpbin.org/redirect/3");
            std::cout << "Status: " << response.status_code() << std::endl;
            std::cout << "Final URL after redirects: " << response.url()
                      << std::endl;
        } catch (const Error& e) {
            std::cerr << "Redirect request failed: " << e.what() << std::endl;
        }

        std::cout << "\n=== Session Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
