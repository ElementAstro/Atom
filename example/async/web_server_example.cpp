/**
 * @file web_server_example.cpp
 * @brief Real-world async web server implementation using atom::async
 *
 * @details This example demonstrates:
 * - Async HTTP request handling with connection pooling
 * - Request routing and middleware patterns
 * - Database connection management with async operations
 * - Session management and authentication
 * - Rate limiting and request throttling
 * - Error handling and logging in production systems
 * - Performance monitoring and metrics collection
 * - Graceful shutdown and resource cleanup
 *
 * @level Expert
 * @prerequisites Understanding of HTTP, web servers, database operations
 * @related_examples component_integration.cpp, message_bus.cpp, pool.cpp
 *
 * @note Demonstrates production-ready async web server architecture
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/async.hpp"
#include "atom/async/async_executor.hpp"
#include "atom/async/future.hpp"
#include "atom/async/limiter.hpp"
#include "atom/async/message_bus.hpp"
#include "atom/async/message_queue.hpp"
#include "atom/async/pool.hpp"
#include "atom/async/promise.hpp"
#include "atom/async/safetype.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace atom::async;

// ============================================================================
// UTILITY FUNCTIONS AND HELPERS
// ============================================================================

// Print mutex for thread-safe output
std::mutex print_mutex;

// Thread-safe print function with timestamp
template <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    (std::cout << ... << args) << std::endl;
}

// Enhanced section separator
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Performance timer
class PerformanceTimer {
public:
    void start(const std::string& operation) {
        current_operation_ = operation;
        start_time_ = std::chrono::high_resolution_clock::now();
        print_safe("⏱️  Starting: ", operation);
    }

    void stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            end_time - start_time_)
                            .count();

        print_safe("⏱️  Completed: ", current_operation_, " in ", duration,
                   " μs");
    }

private:
    std::string current_operation_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

// ============================================================================
// WEB SERVER DATA STRUCTURES
// ============================================================================

// HTTP Request representation
struct HttpRequest {
    enum class Method { GET, POST, PUT, DELETE_METHOD, PATCH };

    Method method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::string body;
    std::string client_ip;
    std::chrono::system_clock::time_point timestamp;

    HttpRequest(Method method, std::string path, std::string client_ip)
        : method(method),
          path(std::move(path)),
          client_ip(std::move(client_ip)),
          timestamp(std::chrono::system_clock::now()) {}
};

// HTTP Response representation
struct HttpResponse {
    int status_code;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::chrono::system_clock::time_point timestamp;

    HttpResponse(int status_code, std::string body)
        : status_code(status_code),
          body(std::move(body)),
          timestamp(std::chrono::system_clock::now()) {}
};

// Database Query representation
struct DatabaseQuery {
    enum class Type { SELECT, INSERT, UPDATE, DELETE };

    Type type;
    std::string table;
    std::string query;
    std::unordered_map<std::string, std::string> parameters;
    int connection_id;

    DatabaseQuery(Type type, std::string table, std::string query,
                  int connection_id)
        : type(type),
          table(std::move(table)),
          query(std::move(query)),
          connection_id(connection_id) {}
};

// Database Result representation
struct DatabaseResult {
    bool success;
    std::vector<std::unordered_map<std::string, std::string>> rows;
    std::string error_message;
    int affected_rows;

    DatabaseResult(bool success, int affected_rows = 0)
        : success(success), affected_rows(affected_rows) {}
};

// Session data
struct Session {
    std::string session_id;
    std::string user_id;
    std::unordered_map<std::string, std::string> data;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;

    Session(std::string session_id, std::string user_id)
        : session_id(std::move(session_id)),
          user_id(std::move(user_id)),
          created_at(std::chrono::system_clock::now()),
          last_accessed(std::chrono::system_clock::now()) {}
};

// ============================================================================
// ASYNC WEB SERVER COMPONENTS
// ============================================================================

/**
 * @brief Async Database Connection Pool
 *
 * Manages database connections with async operations
 */
class AsyncDatabasePool {
public:
    AsyncDatabasePool(size_t pool_size)
        : pool_size_(pool_size), next_connection_id_(1) {
        // Initialize connection pool
        for (size_t i = 0; i < pool_size_; ++i) {
            available_connections_.push_back(next_connection_id_++);
        }
        print_safe("🗄️  Database pool initialized with ", pool_size_,
                   " connections");
    }

    EnhancedFuture<DatabaseResult> executeQuery(const DatabaseQuery& query) {
        return makeEnhancedFuture([this, query]() -> DatabaseResult {
            // Simulate getting connection from pool
            int connection_id = getConnection();

            print_safe("🗄️  Executing ", getQueryTypeString(query.type),
                       " query on table '", query.table,
                       "' (conn: ", connection_id, ")");

            // Simulate database operation
            std::this_thread::sleep_for(
                std::chrono::milliseconds(50 + (rand() % 100)));

            DatabaseResult result(true);

            if (query.type == DatabaseQuery::Type::SELECT) {
                // Simulate returning some data
                result.rows.push_back({{"id", "1"},
                                       {"name", "John"},
                                       {"email", "john@example.com"}});
                result.rows.push_back({{"id", "2"},
                                       {"name", "Jane"},
                                       {"email", "jane@example.com"}});
            } else {
                result.affected_rows = 1;
            }

            // Return connection to pool
            returnConnection(connection_id);

            print_safe("✅ Database query completed (conn: ", connection_id,
                       ")");
            return result;
        });
    }

private:
    size_t pool_size_;
    std::atomic<int> next_connection_id_;
    std::vector<int> available_connections_;
    std::mutex pool_mutex_;

    int getConnection() {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (available_connections_.empty()) {
            // In real implementation, would wait or create new connection
            return next_connection_id_++;
        }
        int conn_id = available_connections_.back();
        available_connections_.pop_back();
        return conn_id;
    }

    void returnConnection(int connection_id) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        available_connections_.push_back(connection_id);
    }

    std::string getQueryTypeString(DatabaseQuery::Type type) {
        switch (type) {
            case DatabaseQuery::Type::SELECT:
                return "SELECT";
            case DatabaseQuery::Type::INSERT:
                return "INSERT";
            case DatabaseQuery::Type::UPDATE:
                return "UPDATE";
            case DatabaseQuery::Type::DELETE:
                return "DELETE";
            default:
                return "UNKNOWN";
        }
    }
};

/**
 * @brief Async Session Manager
 *
 * Manages user sessions with thread-safe operations
 */
class AsyncSessionManager {
public:
    AsyncSessionManager() = default;

    EnhancedFuture<std::string> createSession(const std::string& user_id) {
        return makeEnhancedFuture([this, user_id]() -> std::string {
            std::string session_id = generateSessionId();

            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_[session_id] =
                    std::make_shared<Session>(session_id, user_id);
            }

            print_safe("🔐 Created session ", session_id, " for user ",
                       user_id);
            return session_id;
        });
    }

    EnhancedFuture<std::shared_ptr<Session>> getSession(
        const std::string& session_id) {
        return makeEnhancedFuture([this,
                                   session_id]() -> std::shared_ptr<Session> {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(session_id);
            if (it != sessions_.end()) {
                it->second->last_accessed = std::chrono::system_clock::now();
                print_safe("🔐 Retrieved session ", session_id);
                return it->second;
            }
            print_safe("❌ Session not found: ", session_id);
            return nullptr;
        });
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;

    std::string generateSessionId() {
        static std::atomic<int> counter{1};
        return "session_" + std::to_string(counter++);
    }
};

/**
 * @brief Async Web Server
 *
 * Main web server class that handles HTTP requests asynchronously
 */
class AsyncWebServer {
public:
    AsyncWebServer() : db_pool_(4), request_count_(0), response_count_(0) {
        // Configure thread pool for 8 threads
        ThreadPool::Options options;
        options.initialThreadCount = 8;
        thread_pool_ = std::make_unique<ThreadPool>(options);

        // Set up rate limiting for API endpoints
        rate_limiter_.setFunctionLimit("api_request", 100,
                                       std::chrono::seconds(60));

        // Set up request processing
        setupRequestHandling();
        print_safe("🌐 Async Web Server initialized");
    }

    void start() {
        print_safe("🚀 Starting web server...");
        is_running_ = true;

        // Start request simulation
        simulateIncomingRequests();

        print_safe("✅ Web server started successfully");
    }

    void stop() {
        print_safe("🛑 Stopping web server...");
        is_running_ = false;

        // Wait for pending requests
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        print_safe("📊 Final stats - Requests: ", request_count_.load(),
                   ", Responses: ", response_count_.load());
        print_safe("✅ Web server stopped");
    }

private:
    std::unique_ptr<ThreadPool> thread_pool_;
    AsyncDatabasePool db_pool_;
    AsyncSessionManager session_manager_;
    RateLimiter rate_limiter_;
    std::atomic<bool> is_running_{false};
    std::atomic<int> request_count_;
    std::atomic<int> response_count_;

    void setupRequestHandling() {
        print_safe("⚙️  Setting up request handling pipeline");
    }

    void simulateIncomingRequests() {
        // Simulate various HTTP requests
        std::vector<std::thread> request_threads;

        // Simulate user registration
        request_threads.emplace_back([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            HttpRequest request(HttpRequest::Method::POST, "/api/register",
                                "192.168.1.100");
            request.body =
                R"({"username": "john_doe", "email": "john@example.com", "password": "secret123"})";
            handleRequest(request);
        });

        // Simulate user login
        request_threads.emplace_back([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            HttpRequest request(HttpRequest::Method::POST, "/api/login",
                                "192.168.1.100");
            request.body =
                R"({"username": "john_doe", "password": "secret123"})";
            handleRequest(request);
        });

        // Simulate data retrieval
        request_threads.emplace_back([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            HttpRequest request(HttpRequest::Method::GET, "/api/users",
                                "192.168.1.101");
            request.headers["Authorization"] = "Bearer session_1";
            handleRequest(request);
        });

        // Simulate profile update
        request_threads.emplace_back([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            HttpRequest request(HttpRequest::Method::PUT, "/api/profile",
                                "192.168.1.100");
            request.headers["Authorization"] = "Bearer session_1";
            request.body =
                R"({"name": "John Doe Updated", "bio": "Software Developer"})";
            handleRequest(request);
        });

        // Simulate static file request
        request_threads.emplace_back([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            HttpRequest request(HttpRequest::Method::GET, "/static/app.js",
                                "192.168.1.102");
            handleRequest(request);
        });

        // Wait for all request threads to start
        for (auto& thread : request_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void handleRequest(const HttpRequest& request) {
        request_count_++;

        print_safe("📥 Incoming request: ", getMethodString(request.method),
                   " ", request.path, " from ", request.client_ip);

        // Submit request to thread pool for processing
        auto future = thread_pool_->submit(
            [this, request]() { return processRequest(request); });

        // Handle response asynchronously
        std::thread([this, future = std::move(future)]() mutable {
            try {
                HttpResponse response = future.get();
                sendResponse(response);
            } catch (const std::exception& e) {
                print_safe("❌ Error processing request: ", e.what());
                HttpResponse errorResponse(500, "Internal Server Error");
                sendResponse(errorResponse);
            }
        }).detach();
    }

    HttpResponse processRequest(const HttpRequest& request) {
        // Rate limiting check (simplified for demo)
        try {
            auto awaiter = rate_limiter_.acquire("api_request");
            // In a real implementation, this would be used in a coroutine
            // context For this demo, we'll just proceed
        } catch (const std::exception& e) {
            print_safe("🚫 Rate limit exceeded for ", request.client_ip, ": ",
                       e.what());
            return HttpResponse(429, "Too Many Requests");
        }

        // Route the request
        if (request.path.starts_with("/api/")) {
            return handleApiRequest(request);
        } else if (request.path.starts_with("/static/")) {
            return handleStaticRequest(request);
        } else {
            return HttpResponse(404, "Not Found");
        }
    }

    HttpResponse handleApiRequest(const HttpRequest& request) {
        print_safe("🔧 Processing API request: ", request.path);

        if (request.path == "/api/register" &&
            request.method == HttpRequest::Method::POST) {
            return handleUserRegistration(request);
        } else if (request.path == "/api/login" &&
                   request.method == HttpRequest::Method::POST) {
            return handleUserLogin(request);
        } else if (request.path == "/api/users" &&
                   request.method == HttpRequest::Method::GET) {
            return handleGetUsers(request);
        } else if (request.path == "/api/profile" &&
                   request.method == HttpRequest::Method::PUT) {
            return handleUpdateProfile(request);
        } else {
            return HttpResponse(404, "API endpoint not found");
        }
    }

    HttpResponse handleStaticRequest(const HttpRequest& request) {
        print_safe("📁 Serving static file: ", request.path);

        // Simulate file serving
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (request.path.ends_with(".js")) {
            HttpResponse response(200, "console.log('Hello from app.js');");
            response.headers["Content-Type"] = "application/javascript";
            return response;
        } else if (request.path.ends_with(".css")) {
            HttpResponse response(200,
                                  "body { font-family: Arial, sans-serif; }");
            response.headers["Content-Type"] = "text/css";
            return response;
        } else {
            return HttpResponse(404, "File not found");
        }
    }

    HttpResponse handleUserRegistration(const HttpRequest& request) {
        print_safe("👤 Processing user registration");

        // Simulate user validation and database insertion
        DatabaseQuery query(DatabaseQuery::Type::INSERT, "users",
                            "INSERT INTO users (username, email, "
                            "password_hash) VALUES (?, ?, ?)",
                            1);

        auto dbFuture = db_pool_.executeQuery(query);
        DatabaseResult result = dbFuture.get();

        if (result.success) {
            return HttpResponse(
                201,
                R"({"status": "success", "message": "User registered successfully"})");
        } else {
            return HttpResponse(
                400,
                R"({"status": "error", "message": "Registration failed"})");
        }
    }

    HttpResponse handleUserLogin(const HttpRequest& request) {
        print_safe("🔐 Processing user login");

        // Simulate user authentication
        DatabaseQuery query(DatabaseQuery::Type::SELECT, "users",
                            "SELECT id, username FROM users WHERE username = ? "
                            "AND password_hash = ?",
                            1);

        auto dbFuture = db_pool_.executeQuery(query);
        DatabaseResult result = dbFuture.get();

        if (result.success && !result.rows.empty()) {
            // Create session
            auto sessionFuture = session_manager_.createSession("user_123");
            std::string session_id = sessionFuture.get();

            return HttpResponse(
                200, R"({"status": "success", "session_id": ")" + session_id +
                         R"("})");
        } else {
            return HttpResponse(
                401,
                R"({"status": "error", "message": "Invalid credentials"})");
        }
    }

    HttpResponse handleGetUsers(const HttpRequest& request) {
        print_safe("📋 Processing get users request");

        // Check authentication
        if (!isAuthenticated(request)) {
            return HttpResponse(
                401, R"({"status": "error", "message": "Unauthorized"})");
        }

        // Query users from database
        DatabaseQuery query(DatabaseQuery::Type::SELECT, "users",
                            "SELECT id, username, email FROM users", 1);

        auto dbFuture = db_pool_.executeQuery(query);
        DatabaseResult result = dbFuture.get();

        if (result.success) {
            return HttpResponse(
                200,
                R"({"status": "success", "users": [{"id": 1, "username": "john_doe", "email": "john@example.com"}]})");
        } else {
            return HttpResponse(
                500, R"({"status": "error", "message": "Database error"})");
        }
    }

    HttpResponse handleUpdateProfile(const HttpRequest& request) {
        print_safe("✏️  Processing profile update");

        // Check authentication
        if (!isAuthenticated(request)) {
            return HttpResponse(
                401, R"({"status": "error", "message": "Unauthorized"})");
        }

        // Update user profile
        DatabaseQuery query(DatabaseQuery::Type::UPDATE, "users",
                            "UPDATE users SET name = ?, bio = ? WHERE id = ?",
                            1);

        auto dbFuture = db_pool_.executeQuery(query);
        DatabaseResult result = dbFuture.get();

        if (result.success) {
            return HttpResponse(
                200, R"({"status": "success", "message": "Profile updated"})");
        } else {
            return HttpResponse(
                500, R"({"status": "error", "message": "Update failed"})");
        }
    }

    bool isAuthenticated(const HttpRequest& request) {
        auto auth_header = request.headers.find("Authorization");
        if (auth_header != request.headers.end()) {
            std::string token = auth_header->second;
            if (token.starts_with("Bearer ")) {
                std::string session_id = token.substr(7);
                auto sessionFuture = session_manager_.getSession(session_id);
                auto session = sessionFuture.get();
                return session != nullptr;
            }
        }
        return false;
    }

    void sendResponse(const HttpResponse& response) {
        response_count_++;
        print_safe("📤 Sending response: ", response.status_code, " (",
                   response.body.length(), " bytes)");
    }

    std::string getMethodString(HttpRequest::Method method) {
        switch (method) {
            case HttpRequest::Method::GET:
                return "GET";
            case HttpRequest::Method::POST:
                return "POST";
            case HttpRequest::Method::PUT:
                return "PUT";
            case HttpRequest::Method::DELETE_METHOD:
                return "DELETE";
            case HttpRequest::Method::PATCH:
                return "PATCH";
            default:
                return "UNKNOWN";
        }
    }
};

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

void web_server_demonstration() {
    print_section("Real-World Async Web Server Demonstration");

    PerformanceTimer timer;
    timer.start("Web server simulation");

    // Create and start the web server
    AsyncWebServer server;
    server.start();

    // Let the server run and process requests
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Stop the server
    server.stop();

    timer.stop();
}

// Main function
int main() {
    try {
        std::cout << "====== Real-World Async Web Server Example ======"
                  << std::endl;

        web_server_demonstration();

        std::cout << "\n====== Web Server Example Completed ======"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main" << std::endl;
        return 1;
    }

    return 0;
}
