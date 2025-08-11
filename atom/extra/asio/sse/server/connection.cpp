#include "connection.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

namespace atom::extra::asio::sse {

#ifdef USE_SSL
SSEConnection::pointer SSEConnection::create(
    net::io_context& io_context, ssl_context& ssl_ctx, EventQueue& event_queue,
    EventStore& event_store, AuthService& auth_service, ServerMetrics& metrics,
    const ServerConfig& config) {
    return pointer(new SSEConnection(io_context, ssl_ctx, event_queue,
                                     event_store, auth_service, metrics,
                                     config));
}

ssl::stream<tcp::socket>& SSEConnection::socket() { return ssl_socket_; }

SSEConnection::SSEConnection(net::io_context& io_context, ssl_context& ssl_ctx,
                             EventQueue& event_queue, EventStore& event_store,
                             AuthService& auth_service, ServerMetrics& metrics,
                             const ServerConfig& config)
    : ssl_socket_(io_context, ssl_ctx),
      event_queue_(event_queue),
      event_store_(event_store),
      auth_service_(auth_service),
      metrics_(metrics),
      config_(config) {
    client_id_ =
        "client-" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
}

#else
SSEConnection::pointer SSEConnection::create(net::io_context& io_context,
                                             EventQueue& event_queue,
                                             EventStore& event_store,
                                             AuthService& auth_service,
                                             ServerMetrics& metrics,
                                             const ServerConfig& config) {
    return pointer(new SSEConnection(io_context, event_queue, event_store,
                                     auth_service, metrics, config));
}

tcp::socket& SSEConnection::socket() { return socket_; }

SSEConnection::SSEConnection(net::io_context& io_context,
                             EventQueue& event_queue, EventStore& event_store,
                             AuthService& auth_service, ServerMetrics& metrics,
                             const ServerConfig& config)
    : socket_(io_context),
      event_queue_(event_queue),
      event_store_(event_store),
      auth_service_(auth_service),
      metrics_(metrics),
      config_(config) {
    client_id_ =
        "client-" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
}
#endif

void SSEConnection::start() {
    metrics_.increment_connection_count();
    last_activity_ = std::chrono::steady_clock::now();

    co_spawn(
#ifdef USE_SSL
        ssl_socket_.get_executor(),
#else
        socket_.get_executor(),
#endif
        [self = shared_from_this()]() -> net::awaitable<void> {
            try {
#ifdef USE_SSL
                auto [ec] = co_await as_tuple_awaitable(
                    self->ssl_socket_.async_handshake(
                        ssl::stream_base::server));

                if (ec) {
                    spdlog::error("SSL handshake failed: {}", ec.message());
                    co_return;
                }
#endif
                co_await self->process_connection();
            } catch (const std::exception& e) {
                spdlog::error("Connection error: {}", e.what());
            }
            self->metrics_.decrement_connection_count();
        },
        detached);
}

bool SSEConnection::is_connected() const {
#ifdef USE_SSL
    return ssl_socket_.lowest_layer().is_open();
#else
    return socket_.is_open();
#endif
}

bool SSEConnection::is_timed_out() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now -
                                                            last_activity_)
               .count() > config_.connection_timeout_seconds;
}

void SSEConnection::close() {
    error_code ec;
#ifdef USE_SSL
    if (ssl_socket_.lowest_layer().close(ec); ec) {
#else
    if (socket_.close(ec); ec) {
#endif
        spdlog::error("Error closing connection: {}", ec.message());
    }
}

net::awaitable<void> SSEConnection::process_connection() {
    HttpRequest request = co_await read_http_request();

    if (!request.want_events()) {
        co_await handle_regular_http_request(request);
        co_return;
    }

    if (config_.require_auth && !authenticate_client(request)) {
        co_await send_unauthorized_response();
        co_return;
    }

    if (request.path.find("/events/") == 0) {
        subscribed_channel_ = request.path.substr(8);
    }

    co_await send_headers();

    auto last_event_id = request.get_last_event_id();
    if (last_event_id) {
        co_await send_missed_events(*last_event_id);
    }

    co_await event_loop();
}

net::awaitable<HttpRequest> SSEConnection::read_http_request() {
    HttpRequest request;

    auto [ec, bytes] = co_await as_tuple_awaitable(
#ifdef USE_SSL
        net::async_read_until(ssl_socket_, buffer_, "\r\n\r\n")
#else
        net::async_read_until(socket_, buffer_, "\r\n\r\n")
#endif
    );

    if (ec) {
        spdlog::error("Error reading HTTP request: {}", ec.message());
        co_return request;
    }

    last_activity_ = std::chrono::steady_clock::now();

    std::string header_data(net::buffers_begin(buffer_.data()),
                            net::buffers_begin(buffer_.data()) + bytes);
    buffer_.consume(bytes);

    std::istringstream stream(header_data);
    std::string line;

    if (std::getline(stream, line)) {
        line.pop_back();  // Remove \r
        std::istringstream request_line(line);
        request_line >> request.method >> request.path >> request.version;
    }

    while (std::getline(stream, line) && line != "\r") {
        line.pop_back();  // Remove \r

        auto colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            value.erase(0, value.find_first_not_of(' '));
            request.headers[name] = value;
        }
    }

    if (request.method == "POST" || request.method == "PUT") {
        auto content_length_it = request.headers.find("Content-Length");
        if (content_length_it != request.headers.end()) {
            size_t content_length = std::stoul(content_length_it->second);

            if (content_length > 0) {
                std::vector<char> body_data(content_length);

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

    SPDLOG_DEBUG("Received HTTP request: {} {}", request.method, request.path);
    co_return request;
}

net::awaitable<void> SSEConnection::handle_regular_http_request(
    const HttpRequest& request) {
    std::string response;

    if (request.path == "/health" && request.method == "GET") {
        response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"status\":\"ok\"}";
    } else if (request.path == "/metrics" && request.method == "GET") {
        nlohmann::json metrics_json = metrics_.get_metrics();
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
        if (!config_.require_auth || authenticate_client(request)) {
            try {
                nlohmann::json event_json = nlohmann::json::parse(request.body);

                std::string id = event_json.value(
                    "id",
                    "auto-" + std::to_string(std::chrono::system_clock::now()
                                                 .time_since_epoch()
                                                 .count()));
                std::string event_type = event_json.value("type", "message");
                nlohmann::json data = event_json["data"];

                Event event(id, event_type, data);

                if (event_json.contains("metadata") &&
                    event_json["metadata"].is_object()) {
                    for (auto it = event_json["metadata"].begin();
                         it != event_json["metadata"].end(); ++it) {
                        event.add_metadata(it.key(), it.value());
                    }
                }

                if (config_.enable_compression) {
                    event.compress();
                }

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
        response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "404 Not Found";
    }

    auto [ec, _] = co_await as_tuple_awaitable(
#ifdef USE_SSL
        net::async_write(ssl_socket_, net::buffer(response))
#else
        net::async_write(socket_, net::buffer(response))
#endif
    );

    if (ec) {
        spdlog::error("Error sending HTTP response: {}", ec.message());
    }
}

bool SSEConnection::authenticate_client(const HttpRequest& request) {
    std::string api_key = request.get_api_key();
    if (!api_key.empty()) {
        bool result = auth_service_.authenticate(api_key);
        if (result) {
            metrics_.record_auth_success();
            authenticated_ = true;
            return true;
        }
    }

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

net::awaitable<void> SSEConnection::send_unauthorized_response() {
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
        spdlog::error("Error sending unauthorized response: {}", ec.message());
    }
}

net::awaitable<void> SSEConnection::send_headers() {
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

net::awaitable<void> SSEConnection::send_missed_events(
    const std::string& last_event_id) {
    auto events = event_store_.get_events(10, subscribed_channel_);

    if (events.empty()) {
        co_return;
    }

    SPDLOG_DEBUG("Sending {} missed events to client {}", events.size(),
                 client_id_);

    std::reverse(events.begin(), events.end());

    for (const auto& event : events) {
        co_await send_event(event);
    }
}

net::awaitable<void> SSEConnection::event_loop() {
    auto last_heartbeat = std::chrono::steady_clock::now();

    while (is_connected()) {
        if (event_queue_.has_events()) {
            auto event_opt = event_queue_.pop_event();
            if (event_opt) {
                if (subscribed_channel_.empty() ||
                    event_opt->get_metadata("channel").value_or("") ==
                        subscribed_channel_) {
                    co_await send_event(*event_opt);
                }
            }
            last_activity_ = std::chrono::steady_clock::now();
        } else {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now -
                                                                 last_heartbeat)
                    .count() >= config_.heartbeat_interval_seconds) {
                co_await send_event(HeartbeatEvent());
                last_heartbeat = now;
                last_activity_ = now;
            }

            co_await net::steady_timer(
#ifdef USE_SSL
                ssl_socket_.get_executor(),
#else
                socket_.get_executor(),
#endif
                std::chrono::milliseconds(100))
                .async_wait(net::use_awaitable);
        }
    }
}

net::awaitable<void> SSEConnection::send_event(const Event& event) {
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

}  // namespace atom::extra::asio::sse
