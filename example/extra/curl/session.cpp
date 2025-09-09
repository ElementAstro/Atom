/*
 * session.cpp - CURL Session Example (Minimal Stub Implementation)
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <chrono>

using namespace std::chrono_literals;

// Minimal stub implementations since atom-extra-curl has API compatibility issues

namespace atom::extra::curl {

// Stub Response class
class Response {
public:
    int status_code = 200;
    std::string body = "Response body (stub)";
    std::unordered_map<std::string, std::string> headers;
    
    Response() {
        headers["Content-Type"] = "application/json";
        headers["Server"] = "Stub-Server/1.0";
    }
    
    std::string header(const std::string& name) const {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : "";
    }
    
    std::string text() const {
        return body;
    }
    
    std::string url() const {
        return "https://httpbin.org/get (stub)";
    }
};

// Stub Session class
class Session {
public:
    Session() {
        std::cout << "CURL Session created (stub implementation)" << std::endl;
    }
    
    Response get(const std::string& url) {
        std::cout << "GET request (stub): " << url << std::endl;
        Response response;
        response.body = "GET response from " + url + " (stub)";
        return response;
    }
    
    Response post(const std::string& url, const std::string& body, const std::string& content_type = "") {
        std::cout << "POST request (stub): " << url << std::endl;
        std::cout << "  Body: " << body.substr(0, 50) << "..." << std::endl;
        Response response;
        response.body = "POST response from " + url + " (stub)";
        return response;
    }
    
    Response put(const std::string& url, const std::string& body) {
        std::cout << "PUT request (stub): " << url << std::endl;
        Response response;
        response.body = "PUT response from " + url + " (stub)";
        return response;
    }
    
    Response delete_(const std::string& url) {
        std::cout << "DELETE request (stub): " << url << std::endl;
        Response response;
        response.body = "DELETE response from " + url + " (stub)";
        return response;
    }
    
    void set_header(const std::string& name, const std::string& value) {
        std::cout << "Setting header (stub): " << name << " = " << value << std::endl;
        headers_[name] = value;
    }
    
    void set_timeout(std::chrono::seconds timeout) {
        std::cout << "Setting timeout (stub): " << timeout.count() << " seconds" << std::endl;
        timeout_ = timeout;
    }
    
    void set_auth(const std::string& username, const std::string& password) {
        std::cout << "Setting auth (stub): " << username << " / " << password << std::endl;
        auth_username_ = username;
        auth_password_ = password;
    }
    
    void set_verify_ssl(bool verify) {
        std::cout << "Setting SSL verification (stub): " << (verify ? "true" : "false") << std::endl;
        verify_ssl_ = verify;
    }
    
    void set_follow_redirects(bool follow) {
        std::cout << "Setting follow redirects (stub): " << (follow ? "true" : "false") << std::endl;
        follow_redirects_ = follow;
    }

private:
    std::unordered_map<std::string, std::string> headers_;
    std::chrono::seconds timeout_{30};
    std::string auth_username_;
    std::string auth_password_;
    bool verify_ssl_ = true;
    bool follow_redirects_ = false;
};

} // namespace atom::extra::curl

using namespace atom::extra::curl;

int main() {
    std::cout << "=== CURL Session Example (Stub Implementation) ===" << std::endl;
    std::cout << "Note: This is a stub implementation due to API compatibility issues." << std::endl;

    try {
        // 1. Basic GET request
        std::cout << "\n1. Basic GET Request:" << std::endl;
        {
            Session session;
            auto response = session.get("https://httpbin.org/get");
            
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Content-Type: " << response.header("Content-Type") << std::endl;
            std::cout << "Response: " << response.text().substr(0, 200) << "..." << std::endl;
        }

        // 2. POST request with form data
        std::cout << "\n2. POST Request with Form Data:" << std::endl;
        {
            Session session;
            std::string json_data = R"({"name": "John", "age": 30, "city": "New York"})";
            auto response = session.post("https://httpbin.org/post", json_data);
            
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Response: " << response.text().substr(0, 200) << "..." << std::endl;
        }

        // 3. Custom headers
        std::cout << "\n3. Custom Headers:" << std::endl;
        {
            Session session;
            session.set_header("User-Agent", "Atom-CURL-Session/1.0");
            session.set_header("X-Custom-Header", "CustomValue");
            session.set_header("Accept", "application/json");
            
            auto response = session.get("https://httpbin.org/headers");
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Response: " << response.text().substr(0, 400) << "..." << std::endl;
        }

        // 4. Timeout handling
        std::cout << "\n4. Timeout Handling:" << std::endl;
        {
            Session session;
            session.set_timeout(5s);  // 5 second timeout
            
            auto response = session.get("https://httpbin.org/delay/2");
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Timeout test completed (stub)" << std::endl;
        }

        // 5. PUT request
        std::cout << "\n5. PUT Request:" << std::endl;
        {
            Session session;
            session.set_header("Content-Type", "application/json");
            std::string json_data = R"({"updated": true, "timestamp": 1234567890})";
            auto response = session.put("https://httpbin.org/put", json_data);
            
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Response: " << response.text().substr(0, 300) << "..." << std::endl;
        }

        // 6. DELETE request
        std::cout << "\n6. DELETE Request:" << std::endl;
        {
            Session session;
            auto response = session.delete_("https://httpbin.org/delete");
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "DELETE request completed (stub)" << std::endl;
        }

        // 7. Basic authentication
        std::cout << "\n7. Basic Authentication:" << std::endl;
        {
            Session session;
            session.set_auth("testuser", "testpass");
            
            auto response = session.get("https://httpbin.org/basic-auth/testuser/testpass");
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "Authentication test completed (stub)" << std::endl;
        }

        // 8. SSL verification
        std::cout << "\n8. SSL Verification:" << std::endl;
        {
            Session session;
            session.set_verify_ssl(false);
            
            auto response = session.get("https://httpbin.org/get");
            std::cout << "Status: " << response.status_code << std::endl;
            std::cout << "SSL verification disabled (stub)" << std::endl;
        }

        // 9. Follow redirects
        std::cout << "\n9. Follow Redirects:" << std::endl;
        {
            Session session;
            session.set_follow_redirects(true);
            
            auto response = session.get("https://httpbin.org/redirect/3");
            std::cout << "Final URL after redirects: " << response.url() << std::endl;
            std::cout << "Status: " << response.status_code << std::endl;
        }

        std::cout << "\n=== CURL Session Example Complete (Stub Implementation) ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in CURL session examples: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
