#include "atom/extra/curl/multipart.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/session.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace atom::extra::curl;

// Helper function to create a test file
void create_test_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    } else {
        std::cerr << "Failed to create test file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== CURL Multipart Example ===" << std::endl;

        // Create test files for upload
        create_test_file(
            "test_document.txt",
            "This is a test document for multipart upload.\nIt contains "
            "multiple lines of text.\nLine 3 of the document.");
        create_test_file(
            "test_image.txt",
            "This simulates an image file content.\nBinary data would go here "
            "in a real scenario.\nImage metadata and pixel data...");
        create_test_file("test_config.json", R"({
    "name": "Test Configuration",
    "version": "1.0.0",
    "settings": {
        "debug": true,
        "timeout": 30
    }
})");

        // 1. Basic multipart form data
        std::cout << "\n1. Basic Multipart Form Data:" << std::endl;
        {
            MultipartFormData form;

            // Add text fields
            form.add_field("username", "john_doe");
            form.add_field("email", "john@example.com");
            form.add_field("message", "Hello from multipart form!");
            form.add_field("priority", "high");

            Session session;
            try {
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Content-Type: " << response.header("Content-Type")
                          << std::endl;

                // Show part of the response
                std::string body = response.text();
                std::cout << "Response (first 500 chars): "
                          << body.substr(0, 500) << "..." << std::endl;
            } catch (const Error& e) {
                std::cerr << "Multipart form request failed: " << e.what()
                          << std::endl;
            }
        }

        // 2. Multipart with file upload
        std::cout << "\n2. Multipart with File Upload:" << std::endl;
        {
            MultipartFormData form;

            // Add text fields
            form.add_field("description", "Document upload test");
            form.add_field("category", "documents");

            // Add file
            try {
                form.add_file("document", "test_document.txt", "text/plain");

                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;

                // Show part of the response
                std::string body = response.text();
                std::cout << "Response (first 600 chars): "
                          << body.substr(0, 600) << "..." << std::endl;
            } catch (const Error& e) {
                std::cerr << "File upload request failed: " << e.what()
                          << std::endl;
            }
        }

        // 3. Multiple file uploads
        std::cout << "\n3. Multiple File Uploads:" << std::endl;
        {
            MultipartFormData form;

            // Add metadata
            form.add_field("upload_type", "batch");
            form.add_field("user_id", "12345");
            form.add_field("timestamp", std::to_string(std::time(nullptr)));

            // Add multiple files
            try {
                form.add_file("file1", "test_document.txt", "text/plain");
                form.add_file("file2", "test_image.txt",
                              "text/plain");  // Simulating image
                form.add_file("config", "test_config.json", "application/json");

                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;

                // Show part of the response
                std::string body = response.text();
                std::cout << "Response (first 700 chars): "
                          << body.substr(0, 700) << "..." << std::endl;
            } catch (const Error& e) {
                std::cerr << "Multiple file upload failed: " << e.what()
                          << std::endl;
            }
        }

        // 4. Multipart with custom content types
        std::cout << "\n4. Multipart with Custom Content Types:" << std::endl;
        {
            MultipartFormData form;

            // Add fields with different content types
            form.add_field("text_data", "Plain text content");
            form.add_field("json_data", R"({"key": "value", "number": 42})",
                           "application/json");
            form.add_field("xml_data", "<root><item>XML content</item></root>",
                           "application/xml");

            // Add file with specific content type
            try {
                form.add_file("json_file", "test_config.json",
                              "application/json");

                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;

                // Show part of the response
                std::string body = response.text();
                std::cout << "Response (first 600 chars): "
                          << body.substr(0, 600) << "..." << std::endl;
            } catch (const Error& e) {
                std::cerr << "Custom content type request failed: " << e.what()
                          << std::endl;
            }
        }

        // 5. Large multipart form
        std::cout << "\n5. Large Multipart Form:" << std::endl;
        {
            MultipartFormData form;

            // Add many fields
            for (int i = 0; i < 10; ++i) {
                form.add_field("field_" + std::to_string(i),
                               "Value for field " + std::to_string(i));
            }

            // Add a large text field
            std::string large_text;
            for (int i = 0; i < 100; ++i) {
                large_text += "This is line " + std::to_string(i) +
                              " of a large text field. ";
            }
            form.add_field("large_text", large_text);

            // Add files
            try {
                form.add_file("doc", "test_document.txt", "text/plain");
                form.add_file("config", "test_config.json", "application/json");

                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Large multipart form submitted successfully"
                          << std::endl;
            } catch (const Error& e) {
                std::cerr << "Large multipart form failed: " << e.what()
                          << std::endl;
            }
        }

        // 6. Multipart with binary data
        std::cout << "\n6. Multipart with Binary Data:" << std::endl;
        {
            MultipartFormData form;

            // Create some binary data
            std::vector<uint8_t> binary_data;
            for (int i = 0; i < 256; ++i) {
                binary_data.push_back(static_cast<uint8_t>(i));
            }

            // Add binary field
            form.add_field("description", "Binary data upload test");
            form.add_field("binary_data",
                           std::string(binary_data.begin(), binary_data.end()),
                           "application/octet-stream");

            try {
                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Binary data upload completed" << std::endl;
            } catch (const Error& e) {
                std::cerr << "Binary data upload failed: " << e.what()
                          << std::endl;
            }
        }

        // 7. Multipart with custom headers
        std::cout << "\n7. Multipart with Custom Headers:" << std::endl;
        {
            MultipartFormData form;

            form.add_field("api_key", "secret_key_12345");
            form.add_field("operation", "file_upload");

            try {
                form.add_file("upload", "test_document.txt", "text/plain");

                Session session;
                session.set_header("X-API-Version", "2.0");
                session.set_header("X-Client-ID", "multipart-test-client");

                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;
                std::cout << "Multipart with custom headers completed"
                          << std::endl;
            } catch (const Error& e) {
                std::cerr << "Multipart with headers failed: " << e.what()
                          << std::endl;
            }
        }

        // 8. Error handling - non-existent file
        std::cout << "\n8. Error Handling - Non-existent File:" << std::endl;
        {
            MultipartFormData form;
            form.add_field("test", "error_handling");

            try {
                form.add_file("missing_file", "non_existent_file.txt",
                              "text/plain");

                Session session;
                auto response = session.post("https://httpbin.org/post", form);
                std::cout << "Status: " << response.status_code() << std::endl;
            } catch (const Error& e) {
                std::cout << "Expected error for non-existent file: "
                          << e.what() << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("test_document.txt");
        std::filesystem::remove("test_image.txt");
        std::filesystem::remove("test_config.json");

        std::cout << "\n=== Multipart Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
