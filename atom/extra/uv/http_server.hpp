/**
 * @file http_server.hpp
 * @brief High-performance HTTP server built on libuv with coroutine support
 * @version 1.0
 */

#ifndef ATOM_EXTRA_UV_HTTP_SERVER_HPP
#define ATOM_EXTRA_UV_HTTP_SERVER_HPP

#include <uv.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <regex>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <coroutine>
#include <optional>

namespace uv_http {

/**
 * @struct HttpRequest
 * @brief HTTP request representation
 */
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;
    std::string body;
    std::string remote_addr;
    uint16_t remote_port;
    std::chrono::steady_clock::time_point start_time;
    
    // Helper methods
    std::optional<std::string> get_header(const std::string& name) const;
    std::optional<std::string> get_query_param(const std::string& name) const;
    std::optional<std::string> get_path_param(const std::string& name) const;
    bool has_header(const std::string& name) const;
    std::string get_content_type() const;
    size_t get_content_length() const;
};

/**
 * @struct HttpResponse
 * @brief HTTP response representation
 */
struct HttpResponse {
    int status_code = 200;
    std::string status_message = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool sent = false;
    
    // Helper methods
    void set_header(const std::string& name, const std::string& value);
    void set_content_type(const std::string& content_type);
    void set_json_content();
    void set_html_content();
    void set_text_content();
    void set_status(int code, const std::string& message = "");
    void redirect(const std::string& location, int code = 302);
    void send_json(const std::string& json);
    void send_file(const std::string& file_path);
    void send_error(int code, const std::string& message = "");
};

/**
 * @class HttpContext
 * @brief HTTP request/response context
 */
class HttpContext {
public:
    HttpRequest request;
    HttpResponse response;
    std::unordered_map<std::string, std::any> data; // Context data storage
    
    // Convenience methods
    void json(const std::string& json_data, int status = 200);
    void text(const std::string& text_data, int status = 200);
    void html(const std::string& html_data, int status = 200);
    void file(const std::string& file_path);
    void error(int status, const std::string& message = "");
    void redirect(const std::string& location, int status = 302);
    
    // Data access
    template<typename T>
    void set(const std::string& key, T&& value) {
        data[key] = std::forward<T>(value);
    }
    
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};

// Handler types
using HttpHandler = std::function<void(HttpContext&)>;
using AsyncHttpHandler = std::function<std::coroutine_handle<>(HttpContext&)>;
using MiddlewareHandler = std::function<bool(HttpContext&)>; // Return false to stop chain

/**
 * @struct Route
 * @brief HTTP route definition
 */
struct Route {
    std::string method;
    std::string pattern;
    std::regex regex_pattern;
    std::vector<std::string> param_names;
    HttpHandler handler;
    std::vector<MiddlewareHandler> middleware;
    
    Route(const std::string& m, const std::string& p, HttpHandler h);
    bool matches(const std::string& method, const std::string& path) const;
    void extract_params(const std::string& path, HttpRequest& request) const;
};

/**
 * @struct ServerConfig
 * @brief HTTP server configuration
 */
struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    size_t max_connections = 1000;
    size_t max_request_size = 1024 * 1024; // 1MB
    std::chrono::seconds keep_alive_timeout{60};
    std::chrono::seconds request_timeout{30};
    size_t thread_pool_size = std::thread::hardware_concurrency();
    bool enable_compression = true;
    bool enable_keep_alive = true;
    bool enable_cors = false;
    std::string cors_origin = "*";
    std::string static_file_root;
    bool enable_static_files = false;
    bool enable_directory_listing = false;
    std::string index_file = "index.html";
    
    // SSL/TLS configuration
    bool enable_ssl = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    
    // Logging configuration
    bool enable_access_log = true;
    std::string access_log_format = "%h %l %u %t \"%r\" %>s %b";
    
    // Performance tuning
    size_t tcp_backlog = 128;
    bool tcp_nodelay = true;
    bool tcp_keepalive = true;
    std::chrono::seconds tcp_keepalive_delay{60};
};

/**
 * @struct ServerStats
 * @brief HTTP server statistics
 */
struct ServerStats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<uint64_t> total_connections{0};
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    
    // Performance metrics
    std::atomic<uint64_t> avg_response_time_ms{0};
    std::atomic<uint64_t> min_response_time_ms{UINT64_MAX};
    std::atomic<uint64_t> max_response_time_ms{0};
    
    void reset() {
        total_requests = 0;
        successful_requests = 0;
        failed_requests = 0;
        bytes_sent = 0;
        bytes_received = 0;
        active_connections = 0;
        total_connections = 0;
        start_time = std::chrono::steady_clock::now();
        avg_response_time_ms = 0;
        min_response_time_ms = UINT64_MAX;
        max_response_time_ms = 0;
    }
    
    double get_success_rate() const {
        auto total = total_requests.load();
        return total > 0 ? (double)successful_requests.load() / total * 100.0 : 0.0;
    }
    
    double get_requests_per_second() const {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        return uptime.count() > 0 ? (double)total_requests.load() / uptime.count() : 0.0;
    }
};

/**
 * @class HttpServer
 * @brief High-performance HTTP server with coroutine support
 */
class HttpServer {
public:
    explicit HttpServer(const ServerConfig& config = {}, uv_loop_t* loop = nullptr);
    ~HttpServer();

    // Route registration
    void get(const std::string& pattern, HttpHandler handler);
    void post(const std::string& pattern, HttpHandler handler);
    void put(const std::string& pattern, HttpHandler handler);
    void delete_(const std::string& pattern, HttpHandler handler);
    void patch(const std::string& pattern, HttpHandler handler);
    void head(const std::string& pattern, HttpHandler handler);
    void options(const std::string& pattern, HttpHandler handler);
    void route(const std::string& method, const std::string& pattern, HttpHandler handler);
    
    // Middleware registration
    void use(MiddlewareHandler middleware);
    void use(const std::string& pattern, MiddlewareHandler middleware);
    
    // Static file serving
    void static_files(const std::string& mount_path, const std::string& root_dir);
    
    // Server control
    bool start();
    void stop();
    bool is_running() const { return running_; }
    
    // Statistics
    ServerStats get_stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }
    
    // Configuration
    const ServerConfig& get_config() const { return config_; }
    void set_config(const ServerConfig& config);

private:
    struct Connection;
    struct RequestParser;
    
    ServerConfig config_;
    uv_loop_t* loop_;
    bool loop_owned_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    uv_tcp_t server_;
    ServerStats stats_;
    
    std::vector<Route> routes_;
    std::vector<MiddlewareHandler> global_middleware_;
    std::mutex routes_mutex_;
    
    std::vector<std::thread> worker_threads_;
    
    // Connection management
    std::unordered_map<uv_tcp_t*, std::unique_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    
    // Server methods
    static void on_connection(uv_stream_t* server, int status);
    void handle_connection(uv_tcp_t* client);
    void handle_request(std::unique_ptr<Connection> conn, HttpContext& context);
    void send_response(Connection* conn, const HttpResponse& response);
    void close_connection(uv_tcp_t* client);
    
    // Route matching
    const Route* find_route(const std::string& method, const std::string& path) const;
    bool execute_middleware(HttpContext& context, const std::vector<MiddlewareHandler>& middleware) const;
    
    // Utility methods
    void setup_server();
    void cleanup();
    std::string build_response_string(const HttpResponse& response) const;
    void log_request(const HttpContext& context) const;
    void update_stats(const HttpContext& context, std::chrono::milliseconds response_time);
};

} // namespace uv_http

#endif // ATOM_EXTRA_UV_HTTP_SERVER_HPP
