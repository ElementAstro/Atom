/**
 * @file websocket.hpp
 * @brief WebSocket server and client implementation with libuv
 * @version 1.0
 */

#ifndef ATOM_EXTRA_UV_WEBSOCKET_HPP
#define ATOM_EXTRA_UV_WEBSOCKET_HPP

#include <uv.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <coroutine>
#include <optional>

namespace uv_websocket {

/**
 * @enum WebSocketOpcode
 * @brief WebSocket frame opcodes
 */
enum class WebSocketOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

/**
 * @enum WebSocketState
 * @brief WebSocket connection states
 */
enum class WebSocketState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

/**
 * @struct WebSocketFrame
 * @brief WebSocket frame representation
 */
struct WebSocketFrame {
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    WebSocketOpcode opcode = WebSocketOpcode::TEXT;
    bool masked = false;
    uint32_t mask = 0;
    std::vector<uint8_t> payload;
    
    // Helper methods
    bool is_control_frame() const {
        return static_cast<uint8_t>(opcode) >= 0x8;
    }
    
    bool is_data_frame() const {
        return !is_control_frame();
    }
    
    std::string to_text() const {
        return std::string(payload.begin(), payload.end());
    }
    
    void from_text(const std::string& text) {
        opcode = WebSocketOpcode::TEXT;
        payload.assign(text.begin(), text.end());
    }
    
    void from_binary(const std::vector<uint8_t>& data) {
        opcode = WebSocketOpcode::BINARY;
        payload = data;
    }
};

/**
 * @struct WebSocketMessage
 * @brief Complete WebSocket message (may span multiple frames)
 */
struct WebSocketMessage {
    WebSocketOpcode opcode;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    
    WebSocketMessage(WebSocketOpcode op = WebSocketOpcode::TEXT)
        : opcode(op), timestamp(std::chrono::steady_clock::now()) {}
    
    std::string to_text() const {
        return std::string(data.begin(), data.end());
    }
    
    void from_text(const std::string& text) {
        opcode = WebSocketOpcode::TEXT;
        data.assign(text.begin(), text.end());
    }
    
    void from_binary(const std::vector<uint8_t>& binary) {
        opcode = WebSocketOpcode::BINARY;
        data = binary;
    }
};

// Forward declarations
class WebSocketConnection;
class WebSocketServer;
class WebSocketClient;

// Handler types
using MessageHandler = std::function<void(WebSocketConnection&, const WebSocketMessage&)>;
using ConnectionHandler = std::function<void(WebSocketConnection&)>;
using ErrorHandler = std::function<void(WebSocketConnection&, const std::string&)>;
using CloseHandler = std::function<void(WebSocketConnection&, uint16_t code, const std::string& reason)>;

/**
 * @class WebSocketConnection
 * @brief Represents a WebSocket connection
 */
class WebSocketConnection {
public:
    explicit WebSocketConnection(uv_tcp_t* tcp, WebSocketServer* server = nullptr);
    ~WebSocketConnection();

    // Connection info
    std::string get_id() const { return connection_id_; }
    WebSocketState get_state() const { return state_; }
    std::string get_remote_address() const;
    uint16_t get_remote_port() const;
    std::chrono::steady_clock::time_point get_connect_time() const { return connect_time_; }
    
    // Message sending
    bool send_text(const std::string& text);
    bool send_binary(const std::vector<uint8_t>& data);
    bool send_ping(const std::vector<uint8_t>& data = {});
    bool send_pong(const std::vector<uint8_t>& data = {});
    bool send_frame(const WebSocketFrame& frame);
    
    // Connection control
    void close(uint16_t code = 1000, const std::string& reason = "");
    bool is_open() const { return state_ == WebSocketState::OPEN; }
    
    // Custom data storage
    template<typename T>
    void set_data(const std::string& key, T&& value) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        user_data_[key] = std::forward<T>(value);
    }
    
    template<typename T>
    std::optional<T> get_data(const std::string& key) const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = user_data_.find(key);
        if (it != user_data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    // Statistics
    struct Stats {
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> ping_count{0};
        std::atomic<uint64_t> pong_count{0};
        std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    };
    
    const Stats& get_stats() const { return stats_; }

private:
    friend class WebSocketServer;
    friend class WebSocketClient;
    
    std::string connection_id_;
    uv_tcp_t* tcp_;
    WebSocketServer* server_;
    WebSocketState state_;
    std::chrono::steady_clock::time_point connect_time_;
    
    // Message handling
    std::vector<uint8_t> receive_buffer_;
    std::queue<WebSocketFrame> incomplete_frames_;
    WebSocketMessage current_message_;
    
    // User data storage
    mutable std::mutex data_mutex_;
    std::unordered_map<std::string, std::any> user_data_;
    
    // Statistics
    Stats stats_;
    
    // Internal methods
    void handle_data(const char* data, ssize_t size);
    void process_frame(const WebSocketFrame& frame);
    void send_frame_internal(const WebSocketFrame& frame);
    std::vector<uint8_t> serialize_frame(const WebSocketFrame& frame) const;
    bool parse_frame(const std::vector<uint8_t>& data, size_t& offset, WebSocketFrame& frame);
    void update_activity();
    
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void on_write(uv_write_t* req, int status);
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
};

/**
 * @struct WebSocketServerConfig
 * @brief WebSocket server configuration
 */
struct WebSocketServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    size_t max_connections = 1000;
    size_t max_message_size = 1024 * 1024; // 1MB
    std::chrono::seconds ping_interval{30};
    std::chrono::seconds pong_timeout{10};
    std::chrono::seconds idle_timeout{300}; // 5 minutes
    bool auto_ping = true;
    bool validate_utf8 = true;
    std::vector<std::string> supported_protocols;
    std::vector<std::string> supported_extensions;
    
    // HTTP upgrade settings
    std::string websocket_path = "/ws";
    std::unordered_map<std::string, std::string> custom_headers;
    
    // Performance settings
    size_t tcp_backlog = 128;
    bool tcp_nodelay = true;
    bool tcp_keepalive = true;
};

/**
 * @class WebSocketServer
 * @brief WebSocket server implementation
 */
class WebSocketServer {
public:
    explicit WebSocketServer(const WebSocketServerConfig& config = {}, uv_loop_t* loop = nullptr);
    ~WebSocketServer();

    // Server control
    bool start();
    void stop();
    bool is_running() const { return running_; }
    
    // Event handlers
    void on_connection(ConnectionHandler handler) { connection_handler_ = std::move(handler); }
    void on_message(MessageHandler handler) { message_handler_ = std::move(handler); }
    void on_close(CloseHandler handler) { close_handler_ = std::move(handler); }
    void on_error(ErrorHandler handler) { error_handler_ = std::move(handler); }
    
    // Connection management
    std::vector<std::shared_ptr<WebSocketConnection>> get_connections() const;
    std::shared_ptr<WebSocketConnection> get_connection(const std::string& id) const;
    size_t get_connection_count() const;
    void close_connection(const std::string& id);
    void close_all_connections();
    
    // Broadcasting
    void broadcast_text(const std::string& text);
    void broadcast_binary(const std::vector<uint8_t>& data);
    void broadcast_to_group(const std::string& group, const std::string& text);
    
    // Group management
    void add_to_group(const std::string& connection_id, const std::string& group);
    void remove_from_group(const std::string& connection_id, const std::string& group);
    std::vector<std::string> get_group_members(const std::string& group) const;
    
    // Configuration
    const WebSocketServerConfig& get_config() const { return config_; }
    
    // Statistics
    struct ServerStats {
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> total_messages{0};
        std::atomic<uint64_t> total_bytes{0};
        std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    };
    
    const ServerStats& get_stats() const { return stats_; }

private:
    WebSocketServerConfig config_;
    uv_loop_t* loop_;
    bool loop_owned_;
    std::atomic<bool> running_{false};
    
    uv_tcp_t server_;
    ServerStats stats_;
    
    // Connection management
    mutable std::mutex connections_mutex_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketConnection>> connections_;
    
    // Group management
    mutable std::mutex groups_mutex_;
    std::unordered_map<std::string, std::unordered_set<std::string>> groups_;
    
    // Event handlers
    ConnectionHandler connection_handler_;
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    ErrorHandler error_handler_;
    
    // Ping/pong management
    uv_timer_t ping_timer_;
    
    // Internal methods
    static void on_new_connection(uv_stream_t* server, int status);
    void handle_new_connection(uv_tcp_t* client);
    bool perform_websocket_handshake(uv_tcp_t* client);
    void add_connection(std::shared_ptr<WebSocketConnection> conn);
    void remove_connection(const std::string& id);
    void start_ping_timer();
    static void ping_timer_callback(uv_timer_t* timer);
    void send_pings();
    
    friend class WebSocketConnection;
};

} // namespace uv_websocket

#endif // ATOM_EXTRA_UV_WEBSOCKET_HPP
