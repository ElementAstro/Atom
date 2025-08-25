#include "atom/extra/curl/cookie.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/session.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace atom::extra::curl;

int main() {
    try {
        std::cout << "=== CURL Cookie Example ===" << std::endl;

        // 1. Basic cookie handling
        std::cout << "\n1. Basic Cookie Handling:" << std::endl;
        {
            Session session;

            // Enable cookie handling
            session.set_cookie_jar("cookies.txt");

            std::cout << "Making request to set cookies..." << std::endl;
            try {
                // This endpoint sets cookies
                auto response = session.get(
                    "https://httpbin.org/cookies/set/session_id/abc123");
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Set-Cookie headers received" << std::endl;

                // Make another request to see if cookies are sent
                std::cout << "\nMaking request to check cookies..."
                          << std::endl;
                auto response2 = session.get("https://httpbin.org/cookies");
                std::cout << "Status: " << response2.status_code() << std::endl;
                std::cout << "Response: " << response2.text() << std::endl;

            } catch (const Error& e) {
                std::cerr << "Cookie handling request failed: " << e.what()
                          << std::endl;
            }
        }

        // 2. Manual cookie setting
        std::cout << "\n2. Manual Cookie Setting:" << std::endl;
        {
            Session session;

            // Set cookies manually
            session.set_cookie("user_id", "12345");
            session.set_cookie("session_token", "abcdef123456");
            session.set_cookie("preferences", "theme=dark;lang=en");

            std::cout << "Set manual cookies, making request..." << std::endl;
            try {
                auto response = session.get("https://httpbin.org/cookies");
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Response: " << response.text() << std::endl;
            } catch (const Error& e) {
                std::cerr << "Manual cookie request failed: " << e.what()
                          << std::endl;
            }
        }

        // 3. Cookie jar persistence
        std::cout << "\n3. Cookie Jar Persistence:" << std::endl;
        {
            // First session - set cookies and save to jar
            {
                Session session1;
                session1.set_cookie_jar("persistent_cookies.txt");

                std::cout << "Session 1: Setting cookies..." << std::endl;
                try {
                    auto response = session1.get(
                        "https://httpbin.org/cookies/set/persistent/true");
                    std::cout << "Status: " << response.status_code()
                              << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Session 1 failed: " << e.what() << std::endl;
                }
            }

            // Second session - load cookies from jar
            {
                Session session2;
                session2.set_cookie_jar("persistent_cookies.txt");

                std::cout << "Session 2: Loading cookies from jar..."
                          << std::endl;
                try {
                    auto response = session2.get("https://httpbin.org/cookies");
                    std::cout << "Status: " << response.status_code()
                              << std::endl;
                    std::cout << "Response: " << response.text() << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Session 2 failed: " << e.what() << std::endl;
                }
            }
        }

        // 4. Cookie with domain and path
        std::cout << "\n4. Cookie with Domain and Path:" << std::endl;
        {
            Session session;

            // Set cookies with specific domain and path
            session.set_cookie("domain_cookie", "value1", "httpbin.org", "/");
            session.set_cookie("path_cookie", "value2", "httpbin.org",
                               "/cookies");

            std::cout << "Testing domain/path specific cookies..." << std::endl;
            try {
                auto response = session.get("https://httpbin.org/cookies");
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Response: " << response.text() << std::endl;
            } catch (const Error& e) {
                std::cerr << "Domain/path cookie request failed: " << e.what()
                          << std::endl;
            }
        }

        // 5. Secure and HttpOnly cookies
        std::cout << "\n5. Secure and HttpOnly Cookies:" << std::endl;
        {
            Session session;

            // Set secure cookies (only sent over HTTPS)
            session.set_cookie("secure_cookie", "secure_value", "", "",
                               true);  // secure
            session.set_cookie("httponly_cookie", "httponly_value", "", "",
                               false, true);  // httponly

            std::cout << "Testing secure/httponly cookies..." << std::endl;
            try {
                auto response = session.get("https://httpbin.org/cookies");
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Response: " << response.text() << std::endl;
            } catch (const Error& e) {
                std::cerr << "Secure/HttpOnly cookie request failed: "
                          << e.what() << std::endl;
            }
        }

        // 6. Cookie expiration
        std::cout << "\n6. Cookie Expiration:" << std::endl;
        {
            Session session;

            // Set cookies with expiration
            auto future_time = std::time(nullptr) + 3600;  // 1 hour from now
            session.set_cookie("temp_cookie", "temp_value", "", "", false,
                               false, future_time);

            // Set expired cookie
            auto past_time = std::time(nullptr) - 3600;  // 1 hour ago
            session.set_cookie("expired_cookie", "expired_value", "", "", false,
                               false, past_time);

            std::cout << "Testing cookie expiration..." << std::endl;
            try {
                auto response = session.get("https://httpbin.org/cookies");
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Response (should not contain expired_cookie): "
                          << response.text() << std::endl;
            } catch (const Error& e) {
                std::cerr << "Cookie expiration request failed: " << e.what()
                          << std::endl;
            }
        }

        // 7. Cookie deletion
        std::cout << "\n7. Cookie Deletion:" << std::endl;
        {
            Session session;

            // Set some cookies
            session.set_cookie("cookie1", "value1");
            session.set_cookie("cookie2", "value2");
            session.set_cookie("cookie3", "value3");

            std::cout << "Set 3 cookies, making first request..." << std::endl;
            try {
                auto response1 = session.get("https://httpbin.org/cookies");
                std::cout << "Response with all cookies: " << response1.text()
                          << std::endl;

                // Delete one cookie
                session.delete_cookie("cookie2");

                std::cout << "\nDeleted cookie2, making second request..."
                          << std::endl;
                auto response2 = session.get("https://httpbin.org/cookies");
                std::cout << "Response after deletion: " << response2.text()
                          << std::endl;

            } catch (const Error& e) {
                std::cerr << "Cookie deletion request failed: " << e.what()
                          << std::endl;
            }
        }

        // 8. Cookie jar file operations
        std::cout << "\n8. Cookie Jar File Operations:" << std::endl;
        {
            const std::string jar_file = "test_cookies.txt";

            // Create a session and set some cookies
            {
                Session session;
                session.set_cookie_jar(jar_file);
                session.set_cookie("file_cookie1", "file_value1");
                session.set_cookie("file_cookie2", "file_value2");

                // Make a request to trigger cookie saving
                try {
                    auto response = session.get("https://httpbin.org/cookies");
                    std::cout << "Cookies saved to file: " << jar_file
                              << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Cookie jar save failed: " << e.what()
                              << std::endl;
                }
            }

            // Check if cookie file exists and show its contents
            if (std::filesystem::exists(jar_file)) {
                std::cout << "Cookie jar file contents:" << std::endl;
                std::ifstream file(jar_file);
                std::string line;
                while (std::getline(file, line)) {
                    std::cout << "  " << line << std::endl;
                }
                file.close();
            }

            // Load cookies from file in a new session
            {
                Session session2;
                session2.set_cookie_jar(jar_file);

                std::cout << "Loading cookies from file in new session..."
                          << std::endl;
                try {
                    auto response = session2.get("https://httpbin.org/cookies");
                    std::cout
                        << "Response with loaded cookies: " << response.text()
                        << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Cookie jar load failed: " << e.what()
                              << std::endl;
                }
            }
        }

        // 9. Cookie handling with redirects
        std::cout << "\n9. Cookie Handling with Redirects:" << std::endl;
        {
            Session session;
            session.set_cookie_jar("redirect_cookies.txt");
            session.set_follow_redirects(true);

            std::cout << "Testing cookie handling through redirects..."
                      << std::endl;
            try {
                // This endpoint sets cookies and redirects
                auto response = session.get(
                    "https://httpbin.org/cookies/set/redirect_test/success");
                std::cout << "Final status: " << response.status_code()
                          << std::endl;
                std::cout << "Final URL: " << response.url() << std::endl;

                // Check if cookies were preserved through redirect
                auto cookie_response =
                    session.get("https://httpbin.org/cookies");
                std::cout << "Cookies after redirect: "
                          << cookie_response.text() << std::endl;

            } catch (const Error& e) {
                std::cerr << "Cookie redirect test failed: " << e.what()
                          << std::endl;
            }
        }

        // Cleanup cookie files
        std::cout << "\nCleaning up cookie files..." << std::endl;
        std::filesystem::remove("cookies.txt");
        std::filesystem::remove("persistent_cookies.txt");
        std::filesystem::remove("test_cookies.txt");
        std::filesystem::remove("redirect_cookies.txt");

        std::cout << "\n=== Cookie Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
