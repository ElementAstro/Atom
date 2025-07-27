/**
 * @file uv_utils.hpp
 * @brief Comprehensive utilities and helpers for libuv-based applications
 * @version 2.0
 */

#ifndef ATOM_EXTRA_UV_UTILS_HPP
#define ATOM_EXTRA_UV_UTILS_HPP

#include "coro.hpp"
#include "message_bus.hpp"
#include "subprocess.hpp"
#include "http_server.hpp"
#include "websocket.hpp"
#include "monitor.hpp"

#include <uv.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <optional>
#include <future>

namespace uv_utils {

/**
 * @class UvApplication
 * @brief High-level application framework combining all UV components
 */
class UvApplication {
public:
    struct Config {
        // Core settings
        size_t thread_pool_size = std::thread::hardware_concurrency();
        bool enable_monitoring = true;
        bool enable_message_bus = true;
        
        // HTTP server settings
        bool enable_http_server = false;
        uv_http::ServerConfig http_config;
        
        // WebSocket server settings
        bool enable_websocket_server = false;
        uv_websocket::WebSocketServerConfig websocket_config;
        
        // Message bus settings
        msgbus::MessageBusConfig message_bus_config;
        
        // Monitoring settings
        uv_monitor::MonitorConfig monitor_config;
        
        // Process pool settings
        bool enable_process_pool = false;
        ProcessPool::PoolConfig process_pool_config;
        
        // Graceful shutdown timeout
        std::chrono::seconds shutdown_timeout{30};
    };

    explicit UvApplication(const Config& config = {});
    ~UvApplication();

    // Application lifecycle
    void initialize();
    int run();
    void shutdown();
    bool is_running() const { return running_; }
    
    // Component access
    uv_coro::Scheduler& get_scheduler() { return scheduler_; }
    msgbus::EnhancedMessageBus* get_message_bus() const { return message_bus_.get(); }
    uv_http::HttpServer* get_http_server() const { return http_server_.get(); }
    uv_websocket::WebSocketServer* get_websocket_server() const { return websocket_server_.get(); }
    uv_monitor::Monitor* get_monitor() const { return monitor_.get(); }
    ProcessPool* get_process_pool() const { return process_pool_.get(); }
    
    // Convenience methods
    template<typename T>
    void publish_message(const std::string& topic, T&& message) {
        if (message_bus_) {
            message_bus_->publish(topic, std::forward<T>(message));
        }
    }
    
    template<typename T, typename Handler>
    auto subscribe_message(const std::string& topic, Handler&& handler) {
        if (message_bus_) {
            return message_bus_->subscribe<T>(topic, std::forward<Handler>(handler));
        }
        return msgbus::SubscriptionHandle{};
    }
    
    // HTTP route registration
    void http_get(const std::string& pattern, uv_http::HttpHandler handler) {
        if (http_server_) {
            http_server_->get(pattern, std::move(handler));
        }
    }
    
    void http_post(const std::string& pattern, uv_http::HttpHandler handler) {
        if (http_server_) {
            http_server_->post(pattern, std::move(handler));
        }
    }
    
    // WebSocket event handlers
    void websocket_on_connection(uv_websocket::ConnectionHandler handler) {
        if (websocket_server_) {
            websocket_server_->on_connection(std::move(handler));
        }
    }
    
    void websocket_on_message(uv_websocket::MessageHandler handler) {
        if (websocket_server_) {
            websocket_server_->on_message(std::move(handler));
        }
    }
    
    // Process execution
    std::future<ProcessMetrics> execute_process(const UvProcess::ProcessOptions& options) {
        if (process_pool_) {
            return process_pool_->execute(options);
        }
        
        // Fallback to direct execution
        auto promise = std::make_shared<std::promise<ProcessMetrics>>();
        auto future = promise->get_future();
        
        std::thread([options, promise]() {
            UvProcess process;
            ProcessMetrics metrics;
            
            if (process.spawnWithOptions(options)) {
                process.waitForExit();
                metrics = process.getMetrics();
            }
            
            promise->set_value(metrics);
        }).detach();
        
        return future;
    }
    
    // Signal handling
    void on_signal(int signal, std::function<void()> handler);
    
    // Configuration
    const Config& get_config() const { return config_; }

private:
    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    // Core components
    uv_coro::Scheduler scheduler_;
    std::unique_ptr<msgbus::EnhancedMessageBus> message_bus_;
    std::unique_ptr<uv_http::HttpServer> http_server_;
    std::unique_ptr<uv_websocket::WebSocketServer> websocket_server_;
    std::unique_ptr<uv_monitor::Monitor> monitor_;
    std::unique_ptr<ProcessPool> process_pool_;
    
    // Signal handling
    std::unordered_map<int, uv_signal_t> signal_handlers_;
    std::unordered_map<int, std::function<void()>> signal_callbacks_;
    
    // Internal methods
    void setup_signal_handlers();
    void cleanup_signal_handlers();
    static void signal_callback(uv_signal_t* handle, int signum);
    void handle_shutdown_signal();
};

/**
 * @namespace uv_helpers
 * @brief Utility functions and helpers
 */
namespace helpers {

/**
 * @brief Get current timestamp as string
 */
std::string get_timestamp(const std::string& format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Get system information
 */
struct SystemInfo {
    std::string hostname;
    std::string platform;
    std::string arch;
    std::string version;
    uint32_t cpu_count;
    uint64_t total_memory;
    std::string current_directory;
    std::string executable_path;
};

SystemInfo get_system_info();

/**
 * @brief Network utilities
 */
namespace network {
    std::string get_local_ip();
    std::vector<std::string> get_all_interfaces();
    bool is_port_available(uint16_t port, const std::string& host = "127.0.0.1");
    uint16_t find_available_port(uint16_t start_port = 8000, uint16_t end_port = 9000);
}

/**
 * @brief File system utilities
 */
namespace filesystem {
    bool file_exists(const std::string& path);
    bool directory_exists(const std::string& path);
    bool create_directory(const std::string& path, bool recursive = true);
    std::vector<std::string> list_directory(const std::string& path);
    uint64_t get_file_size(const std::string& path);
    std::string get_file_extension(const std::string& path);
    std::string get_mime_type(const std::string& extension);
}

/**
 * @brief String utilities
 */
namespace string {
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    std::string trim(const std::string& str);
    std::string to_lower(const std::string& str);
    std::string to_upper(const std::string& str);
    bool starts_with(const std::string& str, const std::string& prefix);
    bool ends_with(const std::string& str, const std::string& suffix);
    std::string url_encode(const std::string& str);
    std::string url_decode(const std::string& str);
    std::string base64_encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64_decode(const std::string& str);
}

/**
 * @brief JSON utilities (simple implementation)
 */
namespace json {
    std::string escape_string(const std::string& str);
    std::string object_to_string(const std::unordered_map<std::string, std::string>& obj);
    std::string array_to_string(const std::vector<std::string>& arr);
}

/**
 * @brief Logging utilities
 */
namespace logging {
    enum class Level {
        TRACE, DEBUG, INFO, WARN, ERROR, FATAL
    };
    
    void set_level(Level level);
    void log(Level level, const std::string& message);
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
}

/**
 * @brief Performance utilities
 */
namespace performance {
    class Timer {
    public:
        Timer() : start_time_(std::chrono::high_resolution_clock::now()) {}
        
        void reset() { start_time_ = std::chrono::high_resolution_clock::now(); }
        
        template<typename Duration = std::chrono::milliseconds>
        auto elapsed() const {
            return std::chrono::duration_cast<Duration>(
                std::chrono::high_resolution_clock::now() - start_time_);
        }
        
    private:
        std::chrono::high_resolution_clock::time_point start_time_;
    };
    
    class Profiler {
    public:
        void start(const std::string& name);
        void end(const std::string& name);
        void report() const;
        void clear();
        
    private:
        struct ProfileData {
            std::chrono::high_resolution_clock::time_point start_time;
            std::chrono::microseconds total_time{0};
            size_t call_count = 0;
        };
        
        mutable std::mutex mutex_;
        std::unordered_map<std::string, ProfileData> profiles_;
    };
}

} // namespace helpers

/**
 * @brief Convenience macros for common operations
 */
#define UV_CORO_TASK(name) uv_coro::Task<void> name()
#define UV_CORO_TASK_RETURN(type, name) uv_coro::Task<type> name()
#define UV_AWAIT(expr) co_await (expr)
#define UV_RETURN(expr) co_return (expr)
#define UV_YIELD(expr) co_yield (expr)

#define UV_HTTP_HANDLER(name) void name(uv_http::HttpContext& ctx)
#define UV_WS_HANDLER(name) void name(uv_websocket::WebSocketConnection& conn, const uv_websocket::WebSocketMessage& msg)

#define UV_LOG_TRACE(msg) uv_utils::helpers::logging::trace(msg)
#define UV_LOG_DEBUG(msg) uv_utils::helpers::logging::debug(msg)
#define UV_LOG_INFO(msg) uv_utils::helpers::logging::info(msg)
#define UV_LOG_WARN(msg) uv_utils::helpers::logging::warn(msg)
#define UV_LOG_ERROR(msg) uv_utils::helpers::logging::error(msg)
#define UV_LOG_FATAL(msg) uv_utils::helpers::logging::fatal(msg)

} // namespace uv_utils

#endif // ATOM_EXTRA_UV_UTILS_HPP
