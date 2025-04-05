#include "asio_compatibility.hpp"
#include "sse_event.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <regex>
#include <string>
#include <thread>
#include <unordered_set>

using namespace std::chrono_literals;
using json = nlohmann::json;

// Client configuration
struct ClientConfig {
    std::string host = "localhost";
    std::string port = "8080";
    std::string path = "/events";
    bool use_ssl = false;
    bool verify_ssl = true;
    std::string ca_cert_file;
    std::string api_key;
    std::string username;
    std::string password;
    bool reconnect = true;
    int max_reconnect_attempts = 10;
    int reconnect_base_delay_ms = 1000;  // Base delay for exponential backoff
    bool store_events = true;
    std::string event_store_path = "client_events";
    std::string last_event_id;
    std::vector<std::string> event_types_filter;

    // Load from JSON file
    static ClientConfig from_file(const std::string& filename) {
        ClientConfig config;
        try {
            std::ifstream file(filename);
            if (file.is_open()) {
                json j;
                file >> j;

                // Apply configuration from JSON
                if (j.contains("host"))
                    config.host = j["host"];
                if (j.contains("port"))
                    config.port = j["port"];
                if (j.contains("path"))
                    config.path = j["path"];
                if (j.contains("use_ssl"))
                    config.use_ssl = j["use_ssl"];
                if (j.contains("verify_ssl"))
                    config.verify_ssl = j["verify_ssl"];
                if (j.contains("ca_cert_file"))
                    config.ca_cert_file = j["ca_cert_file"];
                if (j.contains("api_key"))
                    config.api_key = j["api_key"];
                if (j.contains("username"))
                    config.username = j["username"];
                if (j.contains("password"))
                    config.password = j["password"];
                if (j.contains("reconnect"))
                    config.reconnect = j["reconnect"];
                if (j.contains("max_reconnect_attempts"))
                    config.max_reconnect_attempts = j["max_reconnect_attempts"];
                if (j.contains("reconnect_base_delay_ms"))
                    config.reconnect_base_delay_ms =
                        j["reconnect_base_delay_ms"];
                if (j.contains("store_events"))
                    config.store_events = j["store_events"];
                if (j.contains("event_store_path"))
                    config.event_store_path = j["event_store_path"];
                if (j.contains("last_event_id"))
                    config.last_event_id = j["last_event_id"];
                if (j.contains("event_types_filter") &&
                    j["event_types_filter"].is_array()) {
                    for (const auto& type : j["event_types_filter"]) {
                        config.event_types_filter.push_back(type);
                    }
                }
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
            j["host"] = host;
            j["port"] = port;
            j["path"] = path;
            j["use_ssl"] = use_ssl;
            j["verify_ssl"] = verify_ssl;
            j["ca_cert_file"] = ca_cert_file;
            j["api_key"] = api_key;
            j["username"] = username;
            j["password"] = password;
            j["reconnect"] = reconnect;
            j["max_reconnect_attempts"] = max_reconnect_attempts;
            j["reconnect_base_delay_ms"] = reconnect_base_delay_ms;
            j["store_events"] = store_events;
            j["event_store_path"] = event_store_path;
            j["last_event_id"] = last_event_id;
            j["event_types_filter"] = event_types_filter;

            std::ofstream file(filename);
            file << j.dump(4);  // Pretty print with 4-space indent
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error saving config file {}: {}", filename, e.what());
        }
    }
};

// Client event storage
class ClientEventStore {
public:
    explicit ClientEventStore(const std::string& store_path)
        : store_path_(store_path) {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(store_path_);

        // Load existing event IDs
        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator(store_path_)) {
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".json") {
                    try {
                        std::ifstream file(entry.path());
                        json j;
                        file >> j;

                        if (j.contains("id")) {
                            std::string id = j["id"];
                            event_ids_.insert(id);
                        }
                    } catch (const std::exception& e) {
                        SPDLOG_ERROR("Error reading event file {}: {}",
                                     entry.path().string(), e.what());
                    }
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error scanning event directory: {}", e.what());
        }
    }

    void store_event(const Event& event) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if we've already seen this event
        if (event_ids_.find(event.id()) != event_ids_.end()) {
            return;
        }

        // Add to our set of known event IDs
        event_ids_.insert(event.id());

        // Save to file
        try {
            json j;
            j["id"] = event.id();
            j["event_type"] = event.event_type();
            j["data"] = event.data();
            j["timestamp"] = event.timestamp();

            std::string filename =
                std::format("{}/event_{}_{}_{}.json", store_path_,
                            event.timestamp(), event.event_type(), event.id());

            std::ofstream file(filename);
            file << j.dump(2);

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error storing event: {}", e.what());
        }
    }

    bool has_seen_event(const std::string& event_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return event_ids_.find(event_id) != event_ids_.end();
    }

    std::string get_latest_event_id() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string latest_id;
        uint64_t latest_timestamp = 0;

        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator(store_path_)) {
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".json") {
                    try {
                        std::ifstream file(entry.path());
                        json j;
                        file >> j;

                        if (j.contains("id") && j.contains("timestamp")) {
                            uint64_t timestamp = j["timestamp"];
                            if (timestamp > latest_timestamp) {
                                latest_timestamp = timestamp;
                                latest_id = j["id"];
                            }
                        }
                    } catch (const std::exception& e) {
                        // Skip files with errors
                    }
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error finding latest event: {}", e.what());
        }

        return latest_id;
    }

private:
    std::string store_path_;
    std::unordered_set<std::string> event_ids_;
    mutable std::mutex mutex_;
};

// Callback type for event handlers
using EventCallback = std::function<void(const Event&)>;

// SSE Client with enhanced features
class SSEClient {
public:
    SSEClient(net::io_context& io_context, const ClientConfig& config)
        : io_context_(io_context),
          resolver_(io_context),
          config_(config),
          reconnect_timer_(io_context),
          event_store_(config.store_events ? std::make_unique<ClientEventStore>(
                                                 config.event_store_path)
                                           : nullptr) {
        // Initialize the event filters
        for (const auto& type : config.event_types_filter) {
            event_filters_.insert(type);
        }

        // Initialize SSL context if needed
#ifdef USE_SSL
        if (config.use_ssl) {
            ssl_context_ = std::make_unique<ssl_context>(ssl_context::sslv23);
            configure_ssl();
        }
#endif
    }

    // Set event handler callback
    void set_event_handler(EventCallback handler) {
        event_handler_ = std::move(handler);
    }

    // Set connection status handler
    void set_connection_handler(
        std::function<void(bool, const std::string&)> handler) {
        connection_handler_ = std::move(handler);
    }

    // Start the client
    void start() {
        // Reset reconnect count
        reconnect_count_ = 0;

        // Find the latest event ID if requested and not already set
        if (config_.last_event_id.empty() && event_store_) {
            config_.last_event_id = event_store_->get_latest_event_id();

            if (!config_.last_event_id.empty()) {
                SPDLOG_INFO("Resuming from last event ID: {}",
                            config_.last_event_id);
            }
        }

        // Start the connect process
        co_spawn(
            io_context_,
            [this]() -> net::awaitable<void> { co_await connect(); }, detached);
    }

    // Stop the client
    void stop() {
        // Cancel reconnection timer
        error_code ec;
        reconnect_timer_.cancel(ec);

        // Close the socket
#ifdef USE_SSL
        if (ssl_socket_) {
            error_code ec;
            ssl_socket_->lowest_layer().close(ec);
            ssl_socket_.reset();
        }
#else
        if (socket_) {
            error_code ec;
            socket_->close(ec);
            socket_.reset();
        }
#endif

        SPDLOG_INFO("Client stopped");
    }

    // Add a filter for specific event types
    void add_event_filter(const std::string& event_type) {
        event_filters_.insert(event_type);
    }

    // Remove a filter
    void remove_event_filter(const std::string& event_type) {
        event_filters_.erase(event_type);
    }

    // Clear all filters
    void clear_event_filters() { event_filters_.clear(); }

    // Reconnect to the server
    void reconnect() {
        // Stop any existing connection
        stop();

        // Reset state
        parsing_headers_ = true;
        buffer_.consume(buffer_.size());
        current_event_lines_.clear();

        // Start reconnection
        schedule_reconnect();
    }

private:
    net::io_context& io_context_;
    tcp::resolver resolver_;
    ClientConfig config_;
    std::unique_ptr<ClientEventStore> event_store_;

#ifdef USE_SSL
    std::unique_ptr<ssl_context> ssl_context_;
    std::unique_ptr<ssl::stream<tcp::socket>> ssl_socket_;
#else
    std::unique_ptr<tcp::socket> socket_;
#endif

    net::streambuf buffer_;
    std::vector<std::string> current_event_lines_;
    bool parsing_headers_ = true;
    EventCallback event_handler_;
    std::function<void(bool, const std::string&)> connection_handler_;
    net::steady_timer reconnect_timer_;
    int reconnect_count_ = 0;
    std::unordered_set<std::string> event_filters_;

#ifdef USE_SSL
    void configure_ssl() {
        if (!ssl_context_)
            return;

        ssl_context_->set_default_verify_paths();

        if (!config_.ca_cert_file.empty()) {
            ssl_context_->load_verify_file(config_.ca_cert_file);
        }

        ssl_context_->set_verify_mode(config_.verify_ssl ? ssl::verify_peer
                                                         : ssl::verify_none);
    }
#endif

    // Connect to the SSE server
    net::awaitable<void> connect() {
        try {
            SPDLOG_INFO("Connecting to {}:{}{}", config_.host, config_.port,
                        config_.path);

            // Resolve the server address
            auto [ec, endpoints] = co_await as_tuple_awaitable(
                resolver_.async_resolve(config_.host, config_.port));

            if (ec) {
                handle_connection_error("Failed to resolve host: " +
                                        ec.message());
                co_return;
            }

#ifdef USE_SSL
            if (config_.use_ssl && ssl_context_) {
                // Create SSL socket
                ssl_socket_ = std::make_unique<ssl::stream<tcp::socket>>(
                    io_context_, *ssl_context_);

                // Set SNI hostname for verification
                if (!SSL_set_tlsext_host_name(ssl_socket_->native_handle(),
                                              config_.host.c_str())) {
                    error_code ec{static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category()};
                    handle_connection_error("SSL SNI error: " + ec.message());
                    co_return;
                }

                // Connect to the server
                auto [connect_ec, _] = co_await as_tuple_awaitable(
                    net::async_connect(ssl_socket_->lowest_layer(), endpoints));

                if (connect_ec) {
                    handle_connection_error("Failed to connect: " +
                                            connect_ec.message());
                    co_return;
                }

                // Perform SSL handshake
                auto [handshake_ec] = co_await as_tuple_awaitable(
                    ssl_socket_->async_handshake(ssl::stream_base::client));

                if (handshake_ec) {
                    handle_connection_error("SSL handshake failed: " +
                                            handshake_ec.message());
                    co_return;
                }
            } else {
                // Create regular socket
                socket_ = std::make_unique<tcp::socket>(io_context_);

                // Connect to the server
                auto [connect_ec, _] = co_await as_tuple_awaitable(
                    net::async_connect(*socket_, endpoints));

                if (connect_ec) {
                    handle_connection_error("Failed to connect: " +
                                            connect_ec.message());
                    co_return;
                }
            }
#else
            // Create socket
            socket_ = std::make_unique<tcp::socket>(io_context_);

            // Connect to the server
            auto [connect_ec, _] = co_await as_tuple_awaitable(
                net::async_connect(*socket_, endpoints));

            if (connect_ec) {
                handle_connection_error("Failed to connect: " +
                                        connect_ec.message());
                co_return;
            }
#endif

            // Send HTTP request
            co_await send_request();

            // Reset reconnect count on successful connection
            reconnect_count_ = 0;

            // Start reading the response
            co_await read_response();

        } catch (const std::exception& e) {
            handle_connection_error(std::string("Exception: ") + e.what());
        }

        // If we get here, the connection is closed
        if (config_.reconnect &&
            reconnect_count_ < config_.max_reconnect_attempts) {
            schedule_reconnect();
        }
    }

    // Send the HTTP request to establish the SSE connection
    net::awaitable<void> send_request() {
        std::string request = "GET " + config_.path +
                              " HTTP/1.1\r\n"
                              "Host: " +
                              config_.host + ":" + config_.port +
                              "\r\n"
                              "Accept: text/event-stream\r\n"
                              "Cache-Control: no-cache\r\n"
                              "Connection: keep-alive\r\n";

        // Add authorization if provided
        if (!config_.api_key.empty()) {
            request += "X-API-Key: " + config_.api_key + "\r\n";
        }

        if (!config_.username.empty() && !config_.password.empty()) {
            // In a real application, you would Base64 encode this properly
            std::string auth = config_.username + ":" + config_.password;
            std::string encoded_auth = "TODO: Base64 encode here";
            request += "Authorization: Basic " + encoded_auth + "\r\n";
        }

        // Add Last-Event-ID header if we have a previous event ID
        if (!config_.last_event_id.empty()) {
            request += "Last-Event-ID: " + config_.last_event_id + "\r\n";
        }

        // End the request
        request += "\r\n";

        auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
            config_.use_ssl && ssl_socket_
                ? net::async_write(*ssl_socket_, net::buffer(request))
                : net::async_write(*socket_, net::buffer(request))
#else
            net::async_write(*socket_, net::buffer(request))
#endif
        );

        if (ec) {
            throw std::runtime_error("Failed to send request: " + ec.message());
        }

        SPDLOG_DEBUG("Sent HTTP request");
    }

    // Read and process the SSE response
    net::awaitable<void> read_response() {
        // Keep reading until the connection is closed
        while (is_connected()) {
            auto [ec, bytes] = co_await as_tuple_awaitable(
#ifdef USE_SSL
                config_.use_ssl && ssl_socket_
                    ? net::async_read_until(*ssl_socket_, buffer_, "\n")
                    : net::async_read_until(*socket_, buffer_, "\n")
#else
                net::async_read_until(*socket_, buffer_, "\n")
#endif
            );

            if (ec) {
                if (ec == net::error::eof) {
                    if (connection_handler_) {
                        connection_handler_(false,
                                            "Connection closed by server");
                    }
                } else {
                    if (connection_handler_) {
                        connection_handler_(false,
                                            "Read error: " + ec.message());
                    }
                }
                break;
            }

            co_await process_data(bytes);
        }
    }

    // Process data received from the server
    net::awaitable<void> process_data(std::size_t bytes) {
        // Convert buffer to string
        std::string line(net::buffers_begin(buffer_.data()),
                         net::buffers_begin(buffer_.data()) + bytes);

        // Consume the data from the buffer
        buffer_.consume(bytes);

        // Parse HTTP headers first
        if (parsing_headers_) {
            if (line == "\r\n" || line == "\n") {
                parsing_headers_ = false;
                if (connection_handler_) {
                    connection_handler_(true, "Connected to SSE stream");
                }
            }
            co_return;
        }

        // Process SSE protocol data
        std::string line_str(line);
        line_str = std::regex_replace(line_str, std::regex("\r\n|\n"), "");

        if (line_str.empty()) {
            // Empty line marks the end of an event
            if (!current_event_lines_.empty()) {
                auto event_opt = Event::deserialize(current_event_lines_);

                if (event_opt) {
                    // Update last event ID
                    config_.last_event_id = event_opt->id();

                    // Store event if needed
                    if (config_.store_events && event_store_) {
                        event_store_->store_event(*event_opt);
                    }

                    // Check if this event type passes our filters
                    bool passes_filter =
                        event_filters_.empty() ||
                        event_filters_.find(event_opt->event_type()) !=
                            event_filters_.end();

                    if (passes_filter) {
                        // Decompress if needed
                        if (event_opt->is_compressed()) {
                            event_opt->decompress();
                        }

                        // Call the event handler
                        if (event_handler_) {
                            event_handler_(*event_opt);
                        }
                    }
                }

                // Clear for next event
                current_event_lines_.clear();
            }
        } else {
            // Add to current event
            current_event_lines_.push_back(line_str);
        }

        co_return;
    }

    // Schedule reconnection with exponential backoff
    void schedule_reconnect() {
        if (!config_.reconnect ||
            reconnect_count_ >= config_.max_reconnect_attempts) {
            if (connection_handler_) {
                connection_handler_(false, "Max reconnection attempts reached");
            }
            return;
        }

        // Calculate backoff delay
        int delay = config_.reconnect_base_delay_ms *
                    (1 << std::min(reconnect_count_, 10));

        // Add some jitter (±20%)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(-delay / 5, delay / 5);
        delay += dist(gen);

        // Cap at 30 seconds
        delay = std::min(delay, 30000);

        SPDLOG_INFO("Scheduling reconnect attempt {} in {} ms",
                    reconnect_count_ + 1, delay);

        reconnect_timer_.expires_after(std::chrono::milliseconds(delay));
        reconnect_timer_.async_wait([this](const error_code& ec) {
            if (!ec) {
                reconnect_count_++;

                // Actually attempt the reconnect
                co_spawn(
                    io_context_,
                    [this]() -> net::awaitable<void> { co_await connect(); },
                    detached);
            }
        });
    }

    // Check if we're connected
    bool is_connected() const {
#ifdef USE_SSL
        if (config_.use_ssl && ssl_socket_) {
            return ssl_socket_->lowest_layer().is_open();
        } else if (socket_) {
            return socket_->is_open();
        }
#else
        if (socket_) {
            return socket_->is_open();
        }
#endif
        return false;
    }

    // Handle connection errors
    void handle_connection_error(const std::string& message) {
        SPDLOG_ERROR("Connection error: {}", message);

        if (connection_handler_) {
            connection_handler_(false, message);
        }
    }
};

// Initialize logging
void init_logging() {
    try {
        // Create console sink
        auto console_sink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Create file sink with rotation
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/sse_client.log", 1024 * 1024 * 5, 3);
        file_sink->set_level(spdlog::level::debug);

        // Create logger with both sinks
        auto logger = std::make_shared<spdlog::logger>(
            "sse_client", spdlog::sinks_init_list{console_sink, file_sink});
        logger->set_level(spdlog::level::debug);

        // Set as default logger
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::err);

        SPDLOG_INFO("Logging initialized");
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
    }
}

void display_help() {
    std::cout << "SSE Client Commands:\n"
              << "  connect              - Connect to the server\n"
              << "  disconnect           - Disconnect from the server\n"
              << "  reconnect            - Force a reconnection\n"
              << "  filter add <type>    - Add event type filter\n"
              << "  filter remove <type> - Remove event type filter\n"
              << "  filter clear         - Clear all filters\n"
              << "  filter list          - List active filters\n"
              << "  config               - Show current configuration\n"
              << "  config set host <host>     - Set server host\n"
              << "  config set port <port>     - Set server port\n"
              << "  config set path <path>     - Set server path\n"
              << "  config set apikey <key>    - Set API key\n"
              << "  config save          - Save configuration\n"
              << "  help                 - Show this help\n"
              << "  q                    - Quit the client\n";
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logging
        init_logging();

        // Parse command line arguments
        std::string config_file = "client_config.json";

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
        ClientConfig config = ClientConfig::from_file(config_file);

        // Create io_context
        net::io_context io_context;

        // Create client but don't start it yet
        SSEClient client(io_context, config);

        // Set event handler
        client.set_event_handler([](const Event& event) {
            auto timestamp =
                std::format("{:%T}", std::chrono::system_clock::now());

            std::cout << "\n===== Event at " << timestamp << " =====\n";

            if (!event.id().empty()) {
                std::cout << "ID: " << event.id() << "\n";
            }

            std::cout << "Type: " << event.event_type() << "\n";

            // Handle JSON data specially
            if (event.is_json()) {
                try {
                    auto json = event.parse_json();
                    std::cout << "Data (JSON): " << json.dump(2) << "\n";
                } catch (const std::exception& e) {
                    std::cout << "Data (invalid JSON): " << event.data()
                              << "\n";
                }
            } else {
                std::cout << "Data: " << event.data() << "\n";
            }

            std::cout << "============================\n\n";
        });

        // Set connection status handler
        client.set_connection_handler(
            [](bool connected, const std::string& message) {
                if (connected) {
                    std::cout << "Connected: " << message << std::endl;
                } else {
                    std::cout << "Connection status: " << message << std::endl;
                }
            });

        // Start io_context in a separate thread
        std::jthread io_thread([&io_context]() { io_context.run(); });

        std::cout << "SSE Client initialized. Type 'help' for commands or "
                     "'connect' to start.\n";

        // Apply filters from config
        for (const auto& filter : config.event_types_filter) {
            client.add_event_filter(filter);
        }

        // Command loop
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "q")
                break;

            if (input == "help") {
                display_help();
            } else if (input == "connect") {
                client.start();
            } else if (input == "disconnect") {
                client.stop();
                std::cout << "Disconnected from server" << std::endl;
            } else if (input == "reconnect") {
                std::cout << "Forcing reconnection..." << std::endl;
                client.reconnect();
            } else if (input.starts_with("filter ")) {
                auto cmd = input.substr(7);
                if (cmd.starts_with("add ") && cmd.length() > 4) {
                    auto type = cmd.substr(4);
                    client.add_event_filter(type);
                    std::cout << "Added filter for event type: " << type
                              << std::endl;
                } else if (cmd.starts_with("remove ") && cmd.length() > 7) {
                    auto type = cmd.substr(7);
                    client.remove_event_filter(type);
                    std::cout << "Removed filter for event type: " << type
                              << std::endl;
                } else if (cmd == "clear") {
                    client.clear_event_filters();
                    std::cout << "Cleared all filters" << std::endl;
                } else if (cmd == "list") {
                    std::cout << "Active filters:" << std::endl;
                    if (config.event_types_filter.empty()) {
                        std::cout
                            << "  No filters active (receiving all events)"
                            << std::endl;
                    } else {
                        for (const auto& filter : config.event_types_filter) {
                            std::cout << "  - " << filter << std::endl;
                        }
                    }
                } else {
                    std::cout << "Unknown filter command. Use 'filter add "
                                 "<type>', 'filter remove <type>', 'filter "
                                 "clear', or 'filter list'"
                              << std::endl;
                }
            } else if (input == "config") {
                std::cout << "Current configuration:" << std::endl;
                std::cout << "  Host: " << config.host << std::endl;
                std::cout << "  Port: " << config.port << std::endl;
                std::cout << "  Path: " << config.path << std::endl;
                std::cout << "  SSL: "
                          << (config.use_ssl ? "enabled" : "disabled")
                          << std::endl;
                std::cout << "  API Key: "
                          << (config.api_key.empty() ? "not set" : "set")
                          << std::endl;
                std::cout << "  Auth: "
                          << (config.username.empty() ? "not set" : "set")
                          << std::endl;
                std::cout << "  Reconnect: "
                          << (config.reconnect ? "enabled" : "disabled")
                          << std::endl;
                std::cout << "  Max reconnect attempts: "
                          << config.max_reconnect_attempts << std::endl;
                std::cout << "  Store events: "
                          << (config.store_events ? "enabled" : "disabled")
                          << std::endl;
            } else if (input.starts_with("config set ")) {
                auto cmd = input.substr(11);
                if (cmd.starts_with("host ") && cmd.length() > 5) {
                    config.host = cmd.substr(5);
                    std::cout << "Set host to: " << config.host << std::endl;
                } else if (cmd.starts_with("port ") && cmd.length() > 5) {
                    config.port = cmd.substr(5);
                    std::cout << "Set port to: " << config.port << std::endl;
                } else if (cmd.starts_with("path ") && cmd.length() > 5) {
                    config.path = cmd.substr(5);
                    std::cout << "Set path to: " << config.path << std::endl;
                } else if (cmd.starts_with("apikey ") && cmd.length() > 7) {
                    config.api_key = cmd.substr(7);
                    std::cout << "Set API key" << std::endl;
                } else {
                    std::cout << "Unknown config command" << std::endl;
                }
            } else if (input == "config save") {
                config.save_to_file(config_file);
                std::cout << "Configuration saved to " << config_file
                          << std::endl;
            } else {
                std::cout
                    << "Unknown command. Type 'help' for available commands."
                    << std::endl;
            }
        }

        // Clean up
        client.stop();
        io_context.stop();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}