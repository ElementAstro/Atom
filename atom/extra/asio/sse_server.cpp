#include "asio_compatibility.hpp"
#include "sse_event.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;
using json = nlohmann::json;

// Configuration structure
struct ServerConfig {
    uint16_t port = 8080;
    std::string address = "0.0.0.0";
    bool enable_ssl = false;
    std::string cert_file = "server.crt";
    std::string key_file = "server.key";
    std::string auth_file = "auth.json";
    bool require_auth = false;
    size_t max_event_history = 1000;
    bool persist_events = true;
    std::string event_store_path = "events";
    int heartbeat_interval_seconds = 30;
    int max_connections = 1000;
    bool enable_compression = false;
    int connection_timeout_seconds = 300;

    // Load from JSON file
    static ServerConfig from_file(const std::string& filename) {
        ServerConfig config;
        try {
            std::ifstream file(filename);
            if (file.is_open()) {
                json j;
                file >> j;

                // Apply configuration from JSON
                if (j.contains("port"))
                    config.port = j["port"];
                if (j.contains("address"))
                    config.address = j["address"];
                if (j.contains("enable_ssl"))
                    config.enable_ssl = j["enable_ssl"];
                if (j.contains("cert_file"))
                    config.cert_file = j["cert_file"];
                if (j.contains("key_file"))
                    config.key_file = j["key_file"];
                if (j.contains("auth_file"))
                    config.auth_file = j["auth_file"];
                if (j.contains("require_auth"))
                    config.require_auth = j["require_auth"];
                if (j.contains("max_event_history"))
                    config.max_event_history = j["max_event_history"];
                if (j.contains("persist_events"))
                    config.persist_events = j["persist_events"];
                if (j.contains("event_store_path"))
                    config.event_store_path = j["event_store_path"];
                if (j.contains("heartbeat_interval_seconds"))
                    config.heartbeat_interval_seconds =
                        j["heartbeat_interval_seconds"];
                if (j.contains("max_connections"))
                    config.max_connections = j["max_connections"];
                if (j.contains("enable_compression"))
                    config.enable_compression = j["enable_compression"];
                if (j.contains("connection_timeout_seconds"))
                    config.connection_timeout_seconds =
                        j["connection_timeout_seconds"];
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error loading config file {}: {}", filename,
                         e.what());
        }
        return config;
    }

    // Save to JSON file
    void save_to_file(const std::string& filename) const {
        try {
            json j;
            j["port"] = port;
            j["address"] = address;
            j["enable_ssl"] = enable_ssl;
            j["cert_file"] = cert_file;
            j["key_file"] = key_file;
            j["auth_file"] = auth_file;
            j["require_auth"] = require_auth;
            j["max_event_history"] = max_event_history;
            j["persist_events"] = persist_events;
            j["event_store_path"] = event_store_path;
            j["heartbeat_interval_seconds"] = heartbeat_interval_seconds;
            j["max_connections"] = max_connections;
            j["enable_compression"] = enable_compression;
            j["connection_timeout_seconds"] = connection_timeout_seconds;

            std::ofstream file(filename);
            file << j.dump(4);  // Pretty print with 4-space indent
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error saving config file {}: {}", filename, e.what());
        }
    }
};

// Authentication service
class AuthService {
public:
    explicit AuthService(const std::string& auth_file) : auth_file_(auth_file) {
        load_auth_data();
    }

    bool authenticate(const std::string& api_key) const {
        std::shared_lock lock(mutex_);
        return api_keys_.find(api_key) != api_keys_.end();
    }

    bool authenticate(const std::string& username,
                      const std::string& password) const {
        std::shared_lock lock(mutex_);
        auto it = user_credentials_.find(username);
        return it != user_credentials_.end() && it->second == password;
    }

    void add_api_key(const std::string& api_key) {
        std::unique_lock lock(mutex_);
        api_keys_.insert(api_key);
        save_auth_data();
    }

    void remove_api_key(const std::string& api_key) {
        std::unique_lock lock(mutex_);
        api_keys_.erase(api_key);
        save_auth_data();
    }

    void add_user(const std::string& username, const std::string& password) {
        std::unique_lock lock(mutex_);
        user_credentials_[username] = password;
        save_auth_data();
    }

    void remove_user(const std::string& username) {
        std::unique_lock lock(mutex_);
        user_credentials_.erase(username);
        save_auth_data();
    }

private:
    std::string auth_file_;
    std::unordered_set<std::string> api_keys_;
    std::unordered_map<std::string, std::string> user_credentials_;
    mutable std::shared_mutex mutex_;

    void load_auth_data() {
        try {
            std::ifstream file(auth_file_);
            if (file.is_open()) {
                json j;
                file >> j;

                // Load API keys
                if (j.contains("api_keys") && j["api_keys"].is_array()) {
                    for (const auto& key : j["api_keys"]) {
                        api_keys_.insert(key);
                    }
                }

                // Load user credentials
                if (j.contains("users") && j["users"].is_object()) {
                    for (auto it = j["users"].begin(); it != j["users"].end();
                         ++it) {
                        user_credentials_[it.key()] = it.value();
                    }
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error loading auth data: {}", e.what());
        }
    }

    void save_auth_data() {
        try {
            json j;

            // Save API keys
            j["api_keys"] = json::array();
            for (const auto& key : api_keys_) {
                j["api_keys"].push_back(key);
            }

            // Save user credentials
            j["users"] = json::object();
            for (const auto& [username, password] : user_credentials_) {
                j["users"][username] = password;
            }

            std::ofstream file(auth_file_);
            file << j.dump(4);  // Pretty print with 4-space indent
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error saving auth data: {}", e.what());
        }
    }
};

// Event store for persistence
class EventStore {
public:
    explicit EventStore(const std::string& store_path, size_t max_events = 1000)
        : store_path_(store_path), max_events_(max_events) {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(store_path_);

        // Load existing events
        load_events();
    }

    void store_event(const Event& event) {
        std::unique_lock lock(mutex_);

        // Add to in-memory store
        events_.push_back(event);

        // Enforce maximum size
        if (events_.size() > max_events_) {
            events_.pop_front();
        }

        // Persist to disk
        persist_event(event);
    }

    std::vector<Event> get_events(
        size_t limit = std::numeric_limits<size_t>::max(),
        const std::string& event_type = "") const {
        std::shared_lock lock(mutex_);

        std::vector<Event> result;
        size_t count = 0;

        // Iterate in reverse order (newest first)
        for (auto it = events_.rbegin(); it != events_.rend() && count < limit;
             ++it) {
            if (event_type.empty() || it->event_type() == event_type) {
                result.push_back(*it);
                ++count;
            }
        }

        return result;
    }

    std::vector<Event> get_events_since(
        uint64_t timestamp, const std::string& event_type = "") const {
        std::shared_lock lock(mutex_);

        std::vector<Event> result;

        for (const auto& event : events_) {
            if (event.timestamp() > timestamp &&
                (event_type.empty() || event.event_type() == event_type)) {
                result.push_back(event);
            }
        }

        return result;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        events_.clear();

        // Clear persisted events
        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator(store_path_)) {
                std::filesystem::remove(entry.path());
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error clearing event store: {}", e.what());
        }
    }

private:
    std::string store_path_;
    size_t max_events_;
    std::deque<Event> events_;
    mutable std::shared_mutex mutex_;

    void load_events() {
        try {
            // List all files in the directory
            std::vector<std::filesystem::path> event_files;
            for (const auto& entry :
                 std::filesystem::directory_iterator(store_path_)) {
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".json") {
                    event_files.push_back(entry.path());
                }
            }

            // Sort by filename (which includes timestamp)
            std::sort(event_files.begin(), event_files.end());

            // Load events, but respect the maximum limit
            size_t count = 0;
            for (auto it = event_files.rbegin();
                 it != event_files.rend() && count < max_events_;
                 ++it, ++count) {
                try {
                    std::ifstream file(*it);
                    json j;
                    file >> j;

                    std::string id = j["id"];
                    std::string event_type = j["event_type"];
                    std::string data = j["data"];

                    Event event(id, event_type, data);

                    // Add any metadata
                    if (j.contains("metadata") && j["metadata"].is_object()) {
                        for (auto meta_it = j["metadata"].begin();
                             meta_it != j["metadata"].end(); ++meta_it) {
                            event.add_metadata(meta_it.key(), meta_it.value());
                        }
                    }

                    events_.push_front(event);
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("Error loading event from {}: {}",
                                 it->string(), e.what());
                }
            }

            SPDLOG_INFO("Loaded {} events from storage", events_.size());

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error loading events: {}", e.what());
        }
    }

    void persist_event(const Event& event) {
        try {
            // Create a JSON representation of the event
            json j;
            j["id"] = event.id();
            j["event_type"] = event.event_type();
            j["data"] = event.data();
            j["timestamp"] = event.timestamp();

            // Add metadata
            j["metadata"] = json::object();
            // We'd need to add a method to get all metadata, but for now we'll
            // leave this empty

            // Save to file
            std::string filename =
                std::format("{}/event_{}_{}_{}.json", store_path_,
                            event.timestamp(), event.event_type(), event.id());

            std::ofstream file(filename);
            file << j.dump();

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error persisting event: {}", e.what());
        }
    }
};

// Thread-safe event queue
class EventQueue {
public:
    explicit EventQueue(EventStore& event_store, bool persist_events)
        : event_store_(event_store), persist_events_(persist_events) {}

    void push_event(Event event) {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push(std::move(event));
        event_available_.store(true);

        // Optionally persist the event
        if (persist_events_) {
            event_store_.store_event(events_.back());
        }
    }

    bool has_events() const { return event_available_.load(); }

    std::optional<Event> pop_event() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (events_.empty()) {
            event_available_.store(false);
            return std::nullopt;
        }

        Event event = std::move(events_.front());
        events_.pop();
        event_available_.store(!events_.empty());
        return event;
    }

private:
    std::queue<Event> events_;
    std::mutex mutex_;
    std::atomic<bool> event_available_{false};
    EventStore& event_store_;
    bool persist_events_;
};

// Metrics tracking
class ServerMetrics {
public:
    void increment_connection_count() {
        ++total_connections_;
        ++current_connections_;
        update_max_concurrent();
    }

    void decrement_connection_count() {
        if (current_connections_ > 0) {
            --current_connections_;
        }
    }

    void increment_event_count() { ++total_events_; }

    void record_event_size(size_t size_bytes) {
        total_bytes_sent_ += size_bytes;
    }

    void record_auth_failure() { ++auth_failures_; }

    void record_auth_success() { ++auth_successes_; }

    json get_metrics() const {
        json metrics;
        metrics["total_connections"] = total_connections_.load();
        metrics["current_connections"] = current_connections_.load();
        metrics["max_concurrent_connections"] =
            max_concurrent_connections_.load();
        metrics["total_events_sent"] = total_events_.load();
        metrics["total_bytes_sent"] = total_bytes_sent_.load();
        metrics["auth_successes"] = auth_successes_.load();
        metrics["auth_failures"] = auth_failures_.load();
        metrics["uptime_seconds"] =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time_)
                .count();
        return metrics;
    }

private:
    std::atomic<uint64_t> total_connections_{0};
    std::atomic<uint64_t> current_connections_{0};
    std::atomic<uint64_t> max_concurrent_connections_{0};
    std::atomic<uint64_t> total_events_{0};
    std::atomic<uint64_t> total_bytes_sent_{0};
    std::atomic<uint64_t> auth_successes_{0};
    std::atomic<uint64_t> auth_failures_{0};
    std::chrono::steady_clock::time_point start_time_{
        std::chrono::steady_clock::now()};

    void update_max_concurrent() {
        uint64_t current = current_connections_.load();
        uint64_t max = max_concurrent_connections_.load();
        while (current > max) {
            if (max_concurrent_connections_.compare_exchange_weak(max,
                                                                  current)) {
                break;
            }
        }
    }
};

// HTTP request parser
struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    bool want_events() const {
        // Check Accept header for text/event-stream
        auto it = headers.find("Accept");
        return it != headers.end() &&
               it->second.find("text/event-stream") != std::string::npos;
    }

    bool has_auth() const {
        return headers.find("Authorization") != headers.end();
    }

    std::string get_api_key() const {
        auto it = headers.find("X-API-Key");
        if (it != headers.end()) {
            return it->second;
        }

        it = headers.find("Authorization");
        if (it != headers.end() && it->second.starts_with("Bearer ")) {
            return it->second.substr(7);
        }

        return "";
    }

    std::pair<std::string, std::string> get_basic_auth() const {
        auto it = headers.find("Authorization");
        if (it != headers.end() && it->second.starts_with("Basic ")) {
            // Base64 decode
            std::string encoded = it->second.substr(6);
            // We'd need a base64 decode function here - for simplicity, we'll
            // just return empty
            std::string decoded =
                "user:pass";  // Placeholder for real base64 decoding

            size_t colon_pos = decoded.find(':');
            if (colon_pos != std::string::npos) {
                return {decoded.substr(0, colon_pos),
                        decoded.substr(colon_pos + 1)};
            }
        }

        return {"", ""};
    }

    std::optional<std::string> get_last_event_id() const {
        auto it = headers.find("Last-Event-ID");
        if (it != headers.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// Connection class using coroutines
class SSEConnection : public std::enable_shared_from_this<SSEConnection> {
public:
    using pointer = std::shared_ptr<SSEConnection>;

#ifdef USE_SSL
    static pointer create(net::io_context& io_context, ssl_context& ssl_ctx,
                          EventQueue& event_queue, EventStore& event_store,
                          AuthService& auth_service, ServerMetrics& metrics,
                          const ServerConfig& config) {
        return pointer(new SSEConnection(io_context, ssl_ctx, event_queue,
                                         event_store, auth_service, metrics,
                                         config));
    }

    ssl::stream<tcp::socket>& socket() { return ssl_socket_; }
#else
    static pointer create(net::io_context& io_context, EventQueue& event_queue,
                          EventStore& event_store, AuthService& auth_service,
                          ServerMetrics& metrics, const ServerConfig& config) {
        return pointer(new SSEConnection(io_context, event_queue, event_store,
                                         auth_service, metrics, config));
    }

    tcp::socket& socket() { return socket_; }
#endif

    // Start processing client connection
    void start() {
        metrics_.increment_connection_count();

        // Set the connection timeout
        last_activity_ = std::chrono::steady_clock::now();

        // Start the connection processing
        co_spawn(
#ifdef USE_SSL
            ssl_socket_.get_executor(),
#else
            socket_.get_executor(),
#endif
            [self = shared_from_this()]() -> net::awaitable<void> {
                try {
#ifdef USE_SSL
                    // Perform SSL handshake
                    auto [ec] = co_await as_tuple_awaitable(
                        self->ssl_socket_.async_handshake(
                            ssl::stream_base::server));

                    if (ec) {
                        SPDLOG_ERROR("SSL handshake failed: {}", ec.message());
                        co_return;
                    }
#endif

                    co_await self->process_connection();
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("Connection error: {}", e.what());
                }

                self->metrics_.decrement_connection_count();
            },
            detached);
    }

    bool is_connected() const {
#ifdef USE_SSL
        return ssl_socket_.lowest_layer().is_open();
#else
        return socket_.is_open();
#endif
    }

    bool is_timed_out() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now -
                                                                last_activity_)
                   .count() > config_.connection_timeout_seconds;
    }

    void close() {
        error_code ec;
#ifdef USE_SSL
        if (error_code close_ec; ssl_socket_.lowest_layer().close(close_ec)) {
            // 处理关闭错误
            SPDLOG_ERROR("Error closing SSL socket: {}", close_ec.message());
        }
#else
        if (error_code close_ec; socket_.close(close_ec)) {
            // 处理关闭错误
            SPDLOG_ERROR("Error closing socket: {}", close_ec.message());
        }
#endif
    }

private:
#ifdef USE_SSL
    ssl::stream<tcp::socket> ssl_socket_;
#else
    tcp::socket socket_;
#endif
    net::streambuf buffer_;
    EventQueue& event_queue_;
    EventStore& event_store_;
    AuthService& auth_service_;
    ServerMetrics& metrics_;
    const ServerConfig& config_;
    bool headers_sent_ = false;
    bool authenticated_ = false;
    std::chrono::steady_clock::time_point last_activity_;
    std::string client_id_;
    std::string subscribed_channel_;

#ifdef USE_SSL
    explicit SSEConnection(net::io_context& io_context, ssl_context& ssl_ctx,
                           EventQueue& event_queue, EventStore& event_store,
                           AuthService& auth_service, ServerMetrics& metrics,
                           const ServerConfig& config)
        : ssl_socket_(io_context, ssl_ctx),
          event_queue_(event_queue),
          event_store_(event_store),
          auth_service_(auth_service),
          metrics_(metrics),
          config_(config) {
        // Generate a unique client ID
        client_id_ =
            "client-" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
    }
#else
    explicit SSEConnection(net::io_context& io_context, EventQueue& event_queue,
                           EventStore& event_store, AuthService& auth_service,
                           ServerMetrics& metrics, const ServerConfig& config)
        : socket_(io_context),
          event_queue_(event_queue),
          event_store_(event_store),
          auth_service_(auth_service),
          metrics_(metrics),
          config_(config) {
        // Generate a unique client ID
        client_id_ =
            "client-" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
    }
#endif

    // Main connection processing coroutine
    net::awaitable<void> process_connection() {
        // First read and parse the HTTP request
        HttpRequest request = co_await read_http_request();

        // Check if the client wants SSE
        if (!request.want_events()) {
            co_await handle_regular_http_request(request);
            co_return;
        }

        // Authenticate the client if required
        if (config_.require_auth && !authenticate_client(request)) {
            co_await send_unauthorized_response();
            co_return;
        }

        // Extract channel from path
        if (request.path.find("/events/") == 0) {
            subscribed_channel_ = request.path.substr(8);
        }

        // Send SSE headers
        co_await send_headers();

        // Check if client requested replay from a specific event ID
        auto last_event_id = request.get_last_event_id();
        if (last_event_id) {
            co_await send_missed_events(*last_event_id);
        }

        // Keep connection open and process events
        co_await event_loop();
    }

    // Read the HTTP request
    net::awaitable<HttpRequest> read_http_request() {
        HttpRequest request;

        // Read until we get the headers
        auto [ec, bytes] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            net::async_read_until(ssl_socket_, buffer_, "\r\n\r\n")
#else
            net::async_read_until(socket_, buffer_, "\r\n\r\n")
#endif
        );

        if (ec) {
            SPDLOG_ERROR("Error reading HTTP request: {}", ec.message());
            co_return request;
        }

        // Update last activity timestamp
        last_activity_ = std::chrono::steady_clock::now();

        // Convert buffer to string for parsing
        std::string header_data(net::buffers_begin(buffer_.data()),
                                net::buffers_begin(buffer_.data()) + bytes);

        // Consume the read data
        buffer_.consume(bytes);

        // Parse request line and headers
        std::istringstream stream(header_data);
        std::string line;

        // Parse request line
        if (std::getline(stream, line)) {
            line.pop_back();  // Remove \r
            std::istringstream request_line(line);
            request_line >> request.method >> request.path >> request.version;
        }

        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            line.pop_back();  // Remove \r

            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                // Trim leading spaces from value
                value.erase(0, value.find_first_not_of(' '));

                request.headers[name] = value;
            }
        }

        // Read body if necessary (for POST requests)
        if (request.method == "POST" || request.method == "PUT") {
            auto content_length_it = request.headers.find("Content-Length");
            if (content_length_it != request.headers.end()) {
                size_t content_length = std::stoul(content_length_it->second);

                if (content_length > 0) {
                    std::vector<char> body_data(content_length);

                    // Read the body
                    auto [body_ec, body_bytes] = co_await as_tuple_awaitable(
#ifdef USE_SSL
                        net::async_read(ssl_socket_,
#else
                        net::async_read(socket_,
#endif
                                        net::buffer(body_data, content_length),
                                        net::transfer_exactly(content_length)));

                    if (!body_ec) {
                        request.body.assign(body_data.begin(), body_data.end());
                    }
                }
            }
        }

        SPDLOG_DEBUG("Received HTTP request: {} {}", request.method,
                     request.path);
        co_return request;
    }

    // Handle regular HTTP request (non-SSE)
    net::awaitable<void> handle_regular_http_request(
        const HttpRequest& request) {
        std::string response;

        // Check the path and method to determine what to do
        if (request.path == "/health" && request.method == "GET") {
            // Health check endpoint
            response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "\r\n"
                "{\"status\":\"ok\"}";
        } else if (request.path == "/metrics" && request.method == "GET") {
            // Metrics endpoint
            json metrics_json = metrics_.get_metrics();
            std::string metrics_str = metrics_json.dump(2);

            response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "Content-Length: " +
                std::to_string(metrics_str.size()) +
                "\r\n"
                "\r\n" +
                metrics_str;
        } else if (request.path == "/events" && request.method == "POST") {
            // Event publication endpoint
            if (!config_.require_auth || authenticate_client(request)) {
                // Parse the event from the request body
                try {
                    json event_json = json::parse(request.body);

                    std::string id = event_json.value(
                        "id", "auto-" + std::to_string(
                                            std::chrono::system_clock::now()
                                                .time_since_epoch()
                                                .count()));
                    std::string event_type =
                        event_json.value("type", "message");
                    json data = event_json["data"];

                    // Create and broadcast the event
                    Event event(id, event_type, data);

                    // Add any metadata
                    if (event_json.contains("metadata") &&
                        event_json["metadata"].is_object()) {
                        for (auto it = event_json["metadata"].begin();
                             it != event_json["metadata"].end(); ++it) {
                            event.add_metadata(it.key(), it.value());
                        }
                    }

                    // Check if compression is requested
                    if (config_.enable_compression) {
                        event.compress();
                    }

                    // Queue the event for broadcast
                    event_queue_.push_event(event);

                    response =
                        "HTTP/1.1 202 Accepted\r\n"
                        "Content-Type: application/json\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "{\"success\":true,\"id\":\"" +
                        id + "\"}";
                } catch (const std::exception& e) {
                    response =
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: application/json\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "{\"error\":\"Invalid event format: " +
                        std::string(e.what()) + "\"}";
                }
            } else {
                response =
                    "HTTP/1.1 401 Unauthorized\r\n"
                    "Content-Type: application/json\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "{\"error\":\"Authentication required\"}";
            }
        } else {
            // Unknown endpoint
            response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found";
        }

        // Send the response
        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            net::async_write(ssl_socket_, net::buffer(response))
#else
            net::async_write(socket_, net::buffer(response))
#endif
        );

        if (ec) {
            SPDLOG_ERROR("Error sending HTTP response: {}", ec.message());
        }
    }

    // Authenticate a client based on the request
    bool authenticate_client(const HttpRequest& request) {
        // Check API key first
        std::string api_key = request.get_api_key();
        if (!api_key.empty()) {
            bool result = auth_service_.authenticate(api_key);
            if (result) {
                metrics_.record_auth_success();
                authenticated_ = true;
                return true;
            }
        }

        // Then check basic auth
        auto [username, password] = request.get_basic_auth();
        if (!username.empty()) {
            bool result = auth_service_.authenticate(username, password);
            if (result) {
                metrics_.record_auth_success();
                authenticated_ = true;
                return true;
            }
        }

        metrics_.record_auth_failure();
        return false;
    }

    // Send unauthorized response
    net::awaitable<void> send_unauthorized_response() {
        std::string response =
            "HTTP/1.1 401 Unauthorized\r\n"
            "Content-Type: application/json\r\n"
            "WWW-Authenticate: Basic realm=\"SSE Server\"\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"error\":\"Authentication required\"}";

        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            net::async_write(ssl_socket_, net::buffer(response))
#else
            net::async_write(socket_, net::buffer(response))
#endif
        );

        if (ec) {
            SPDLOG_ERROR("Error sending unauthorized response: {}",
                         ec.message());
        }
    }

    // Send SSE headers to establish the connection
    net::awaitable<void> send_headers() {
        if (headers_sent_)
            co_return;

        std::string headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";

        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            net::async_write(ssl_socket_, net::buffer(headers))
#else
            net::async_write(socket_, net::buffer(headers))
#endif
        );

        if (ec) {
            throw std::runtime_error("Failed to send headers: " + ec.message());
        }

        headers_sent_ = true;
        SPDLOG_DEBUG("Sent SSE headers to client {}", client_id_);
    }

    // Send missed events to client
    net::awaitable<void> send_missed_events(const std::string& last_event_id) {
        // For simplicity, we'll just get the last 10 events
        // In a real implementation, you'd want to find events after the
        // specified ID
        auto events = event_store_.get_events(10, subscribed_channel_);

        if (events.empty()) {
            co_return;
        }

        SPDLOG_DEBUG("Sending {} missed events to client {}", events.size(),
                     client_id_);

        // Send the events in chronological order
        std::reverse(events.begin(), events.end());

        for (const auto& event : events) {
            co_await send_event(event);
        }
    }

    // Main event processing loop
    net::awaitable<void> event_loop() {
        // Heartbeat timer
        auto last_heartbeat = std::chrono::steady_clock::now();

        // Keep connection open and process events
        while (is_connected()) {
            // Check for events to send
            if (event_queue_.has_events()) {
                auto event_opt = event_queue_.pop_event();
                if (event_opt) {
                    // Only send event if it matches the subscribed channel (if
                    // any)
                    if (subscribed_channel_.empty() ||
                        event_opt->get_metadata("channel").value_or("") ==
                            subscribed_channel_) {
                        co_await send_event(*event_opt);
                    }
                }

                // Update last activity timestamp
                last_activity_ = std::chrono::steady_clock::now();
            } else {
                // Check if it's time to send a heartbeat
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(
                        now - last_heartbeat)
                        .count() >= config_.heartbeat_interval_seconds) {
                    co_await send_event(HeartbeatEvent());
                    last_heartbeat = now;
                    last_activity_ = now;
                }

                // 使用 co_await delay 替代 schedule
                co_await net::steady_timer(socket_.get_executor(),
                                           std::chrono::milliseconds(100))
                    .async_wait(net::use_awaitable);
            }
        }
    }

    // Send a single event to the client
    net::awaitable<void> send_event(const Event& event) {
        auto serialized = event.serialize();
        metrics_.increment_event_count();
        metrics_.record_event_size(serialized.size());

        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            net::async_write(ssl_socket_, net::buffer(serialized))
#else
            net::async_write(socket_, net::buffer(serialized))
#endif
        );

        if (ec) {
            throw std::runtime_error("Failed to send event: " + ec.message());
        }

        SPDLOG_DEBUG("Sent event of type {} to client {}", event.event_type(),
                     client_id_);
    }
};

// SSE Server class with coroutine-based acceptor
class SSEServer {
public:
    SSEServer(net::io_context& io_context, const ServerConfig& config)
        : io_context_(io_context),
          // 修复初始化顺序，匹配声明顺序
          acceptor_(io_context,
                    tcp::endpoint(net::ip::make_address(config.address),
                                  config.port)),
          event_queue_(event_store_, config.persist_events),
          event_store_(config.event_store_path, config.max_event_history),
          auth_service_(config.auth_file),
          config_(config) {
        // Initialize logging
        init_logging();

        // Initialize SSL context if enabled
#ifdef USE_SSL
        if (config.enable_ssl) {
            ssl_context_ = std::make_unique<ssl_context>(ssl_context::sslv23);
            configure_ssl();
        }
#endif

        // Start the connection monitor
        start_connection_monitor();

        // Start accepting connections
        co_spawn(
            acceptor_.get_executor(),
            [this]() -> net::awaitable<void> { co_await accept_connections(); },
            detached);

        SPDLOG_INFO("SSE Server started on {}:{}", config_.address,
                    config_.port);
        if (config_.require_auth) {
            SPDLOG_INFO("Authentication is required");
        }
    }

    // Send an event to all connected clients
    template <EventType E>
    void broadcast_event(E&& event) {
        // Add event to queue to be sent
        event_queue_.push_event(std::forward<E>(event));

        // Clean up disconnected clients periodically
        clean_connections();
    }

    // Get server metrics
    json get_metrics() const { return metrics_.get_metrics(); }

private:
    net::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::vector<SSEConnection::pointer> connections_;
    std::mutex connections_mutex_;
    EventQueue event_queue_;
    EventStore event_store_;
    AuthService auth_service_;
    ServerMetrics metrics_;
    ServerConfig config_;
    std::chrono::steady_clock::time_point last_cleanup_ =
        std::chrono::steady_clock::now();
    net::steady_timer connection_monitor_timer_{io_context_};

#ifdef USE_SSL
    std::unique_ptr<ssl_context> ssl_context_;

    void configure_ssl() {
        if (!ssl_context_)
            return;

        ssl_context_->set_options(ssl::context::default_workarounds |
                                  ssl::context::no_sslv2 |
                                  ssl::context::single_dh_use);

        try {
            ssl_context_->use_certificate_chain_file(config_.cert_file);
            ssl_context_->use_private_key_file(config_.key_file,
                                               ssl::context::pem);

            SPDLOG_INFO("SSL configured with cert: {} and key: {}",
                        config_.cert_file, config_.key_file);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("SSL configuration error: {}", e.what());
            throw;
        }
    }
#endif

    // Initialize logging
    void init_logging() {
        try {
            // Create console sink
            auto console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);

            // Create file sink with rotation
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    "logs/sse_server.log", 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::debug);

            // Create logger with both sinks
            auto logger = std::make_shared<spdlog::logger>(
                "sse_server", spdlog::sinks_init_list{console_sink, file_sink});
            logger->set_level(spdlog::level::debug);

            // Set as default logger
            spdlog::set_default_logger(logger);
            spdlog::flush_on(spdlog::level::err);

            SPDLOG_INFO("Logging initialized");
        } catch (const std::exception& e) {
            std::cerr << "Logger initialization failed: " << e.what()
                      << std::endl;
        }
    }

    // Start the connection monitor
    void start_connection_monitor() {
        connection_monitor_timer_.expires_after(std::chrono::seconds(10));
        connection_monitor_timer_.async_wait([this](const error_code& ec) {
            if (!ec) {
                monitor_connections();
                start_connection_monitor();
            }
        });
    }

    // Monitor connections for timeouts
    void monitor_connections() {
        std::lock_guard<std::mutex> lock(connections_mutex_);

        // Find timed out connections
        std::vector<SSEConnection::pointer> timed_out;
        for (const auto& conn : connections_) {
            if (conn->is_timed_out()) {
                timed_out.push_back(conn);
            }
        }

        // Close timed out connections
        for (auto& conn : timed_out) {
            SPDLOG_INFO("Closing timed out connection");
            conn->close();
        }

        // Remove closed connections
        clean_connections();
    }

    // Accept new connections
    net::awaitable<void> accept_connections() {
        for (;;) {
            // Check if we've hit the connection limit
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                if (connections_.size() >=
                    static_cast<size_t>(config_.max_connections)) {
                    SPDLOG_WARN(
                        "Connection limit reached ({}), waiting for slots to "
                        "free up",
                        config_.max_connections);
                    co_await net::steady_timer(acceptor_.get_executor(),
                                               std::chrono::seconds(1))
                        .async_wait(net::use_awaitable);
                    continue;
                }
            }

            // Accept a new connection
            auto [ec, socket] =
                co_await as_tuple_awaitable(acceptor_.async_accept());

            if (ec) {
                SPDLOG_ERROR("Accept error: {}", ec.message());
                continue;
            }

            // Create and start a new connection
            SSEConnection::pointer connection;

#ifdef USE_SSL
            if (config_.enable_ssl && ssl_context_) {
                connection = SSEConnection::create(
                    io_context_, *ssl_context_, event_queue_, event_store_,
                    auth_service_, metrics_, config_);
                connection->socket().lowest_layer() = std::move(socket);
            } else {
                connection = SSEConnection::create(io_context_, event_queue_,
                                                   event_store_, auth_service_,
                                                   metrics_, config_);
                connection->socket() = std::move(socket);
            }
#else
            connection =
                SSEConnection::create(io_context_, event_queue_, event_store_,
                                      auth_service_, metrics_, config_);
            connection->socket() = std::move(socket);
#endif

            // Store connection
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.push_back(connection);
            }

            // Start processing the connection
            connection->start();

            SPDLOG_INFO("New client connected. Total clients: {}",
                        connections_.size());
        }
    }

    // Remove disconnected clients
    void clean_connections() {
        auto now = std::chrono::steady_clock::now();

        // Only clean every 5 seconds to avoid too much locking
        if (now - last_cleanup_ < 5s) {
            return;
        }

        last_cleanup_ = now;

        std::lock_guard<std::mutex> lock(connections_mutex_);

        // Use C++20 erase_if for cleaner code
        auto before_size = connections_.size();
        std::erase_if(connections_,
                      [](const auto& conn) { return !conn->is_connected(); });

        auto removed = before_size - connections_.size();
        if (removed > 0) {
            SPDLOG_INFO("Removed {} disconnected clients. Total clients: {}",
                        removed, connections_.size());
        }
    }
};

// Helper to generate unique IDs
std::string generate_id() {
    static std::atomic<uint64_t> counter(0);
    return std::to_string(counter++);
}

void display_help() {
    std::cout
        << "SSE Server Commands:\n"
        << "  m <message>         - Send a message event\n"
        << "  u <json_data>       - Send an update event with JSON\n"
        << "  a <message>         - Send an alert\n"
        << "  c <channel> <msg>   - Send a message to a specific channel\n"
        << "  metrics             - Show server metrics\n"
        << "  clients             - Show number of connected clients\n"
        << "  compress <on/off>   - Toggle compression\n"
        << "  help                - Show this help\n"
        << "  q                   - Quit the server\n";
}

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        std::string config_file = "config.json";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0]
                          << " [--config <config_file>] [--help]\n";
                return 0;
            }
        }

        // Load configuration
        ServerConfig config = ServerConfig::from_file(config_file);

        // Create io_context with multiple threads
        net::io_context io_context(4);  // Use 4 threads

        // Create server
        SSEServer server(io_context, config);

        // Run server in the background
        std::vector<std::jthread> io_threads;
        for (int i = 0; i < 4; ++i) {
            io_threads.emplace_back([&io_context]() { io_context.run(); });
        }

        std::cout << "SSE Server started on " << config.address << ":"
                  << config.port << "\n";
        display_help();

        // Command loop
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "q")
                break;

            if (input == "help") {
                display_help();
            } else if (input == "metrics") {
                auto metrics = server.get_metrics();
                std::cout << "Server metrics:\n"
                          << metrics.dump(2) << std::endl;
            } else if (input == "clients") {
                auto metrics = server.get_metrics();
                std::cout << "Connected clients: "
                          << metrics["current_connections"] << std::endl;
            } else if (input.starts_with("compress ")) {
                auto value = input.substr(9);
                if (value == "on") {
                    config.enable_compression = true;
                    std::cout << "Compression enabled" << std::endl;
                } else if (value == "off") {
                    config.enable_compression = false;
                    std::cout << "Compression disabled" << std::endl;
                } else {
                    std::cout << "Invalid option. Use 'on' or 'off'"
                              << std::endl;
                }

                // Save updated config
                config.save_to_file(config_file);
            } else if (input.starts_with("m ") && input.length() > 2) {
                auto message = input.substr(2);
                auto id = generate_id();
                server.broadcast_event(MessageEvent(id, message));
                std::cout << "Sent message event with ID: " << id << std::endl;
            } else if (input.starts_with("u ") && input.length() > 2) {
                try {
                    auto data = input.substr(2);
                    auto json_data = json::parse(data);
                    auto id = generate_id();
                    server.broadcast_event(UpdateEvent(id, json_data));
                    std::cout << "Sent update event with ID: " << id
                              << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error parsing JSON: " << e.what()
                              << std::endl;
                }
            } else if (input.starts_with("a ") && input.length() > 2) {
                auto message = input.substr(2);
                auto id = generate_id();
                server.broadcast_event(AlertEvent(id, message));
                std::cout << "Sent alert event with ID: " << id << std::endl;
            } else if (input.starts_with("c ") && input.length() > 2) {
                auto remainder = input.substr(2);
                auto space_pos = remainder.find(' ');

                if (space_pos != std::string::npos) {
                    auto channel = remainder.substr(0, space_pos);
                    auto message = remainder.substr(space_pos + 1);
                    auto id = generate_id();

                    MessageEvent event(id, message);
                    event.add_metadata("channel", channel);

                    server.broadcast_event(event);
                    std::cout << "Sent message to channel '" << channel
                              << "' with ID: " << id << std::endl;
                } else {
                    std::cout << "Invalid format. Use 'c <channel> <message>'"
                              << std::endl;
                }
            } else {
                std::cout
                    << "Unknown command. Type 'help' for available commands."
                    << std::endl;
            }
        }

        // Stop the server
        io_context.stop();

        std::cout << "Server shutting down..." << std::endl;

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}