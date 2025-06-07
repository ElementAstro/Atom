#pragma once

/**
 * @file connection.hpp
 * @brief SSE connection handling
 */

#include "../../asio_compatibility.hpp"
#include "../event.hpp"
#include "auth_service.hpp"
#include "event_queue.hpp"
#include "event_store.hpp"
#include "http_request.hpp"
#include "metrics.hpp"
#include "server_config.hpp"


#include <chrono>
#include <memory>
#include <string>

namespace atom::extra::asio::sse {

/**
 * @brief Individual SSE connection handler
 */
class SSEConnection : public std::enable_shared_from_this<SSEConnection> {
public:
    using pointer = std::shared_ptr<SSEConnection>;

#ifdef USE_SSL
    static pointer create(net::io_context& io_context, ssl_context& ssl_ctx,
                          EventQueue& event_queue, EventStore& event_store,
                          AuthService& auth_service, ServerMetrics& metrics,
                          const ServerConfig& config);

    ssl::stream<tcp::socket>& socket();
#else
    static pointer create(net::io_context& io_context, EventQueue& event_queue,
                          EventStore& event_store, AuthService& auth_service,
                          ServerMetrics& metrics, const ServerConfig& config);

    tcp::socket& socket();
#endif

    void start();
    bool is_connected() const;
    bool is_timed_out() const;
    void close();

private:
#ifdef USE_SSL
    explicit SSEConnection(net::io_context& io_context, ssl_context& ssl_ctx,
                           EventQueue& event_queue, EventStore& event_store,
                           AuthService& auth_service, ServerMetrics& metrics,
                           const ServerConfig& config);

    ssl::stream<tcp::socket> ssl_socket_;
#else
    explicit SSEConnection(net::io_context& io_context, EventQueue& event_queue,
                           EventStore& event_store, AuthService& auth_service,
                           ServerMetrics& metrics, const ServerConfig& config);

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

    // Coroutine methods
    net::awaitable<void> process_connection();
    net::awaitable<HttpRequest> read_http_request();
    net::awaitable<void> handle_regular_http_request(
        const HttpRequest& request);
    net::awaitable<void> send_unauthorized_response();
    net::awaitable<void> send_headers();
    net::awaitable<void> send_missed_events(const std::string& last_event_id);
    net::awaitable<void> event_loop();
    net::awaitable<void> send_event(const Event& event);

    // Helper methods
    bool authenticate_client(const HttpRequest& request);
};

}  // namespace atom::extra::asio::sse