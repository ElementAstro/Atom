/*
 * tcp_common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: Common definitions for TCP client implementations

*************************************************/

#ifndef ATOM_CONNECTION_TCP_COMMON_HPP
#define ATOM_CONNECTION_TCP_COMMON_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <system_error>

namespace atom::connection {

/**
 * @brief Enum representing different connection states
 */
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

/**
 * @brief Convert ConnectionState to string for logging
 */
[[nodiscard]] inline constexpr const char* connectionStateToString(
    ConnectionState state) noexcept {
    switch (state) {
        case ConnectionState::Disconnected:
            return "Disconnected";
        case ConnectionState::Connecting:
            return "Connecting";
        case ConnectionState::Connected:
            return "Connected";
        case ConnectionState::Reconnecting:
            return "Reconnecting";
        case ConnectionState::Failed:
            return "Failed";
        default:
            return "Unknown";
    }
}

/**
 * @brief Unified connection statistics structure
 *
 * Uses int64_t for time points to ensure lock-free atomics on all platforms
 */
struct ConnectionStats {
    std::atomic<std::size_t> total_bytes_sent{0};
    std::atomic<std::size_t> total_bytes_received{0};
    std::atomic<std::size_t> connection_attempts{0};
    std::atomic<std::size_t> successful_connections{0};
    std::atomic<std::size_t> failed_connections{0};
    std::atomic<int64_t> last_connected_time_ns{0};
    std::atomic<int64_t> last_activity_time_ns{0};
    std::atomic<int64_t> average_latency_ms{0};

    ConnectionStats() = default;

    ConnectionStats(const ConnectionStats& other)
        : total_bytes_sent(other.total_bytes_sent.load()),
          total_bytes_received(other.total_bytes_received.load()),
          connection_attempts(other.connection_attempts.load()),
          successful_connections(other.successful_connections.load()),
          failed_connections(other.failed_connections.load()),
          last_connected_time_ns(other.last_connected_time_ns.load()),
          last_activity_time_ns(other.last_activity_time_ns.load()),
          average_latency_ms(other.average_latency_ms.load()) {}

    ConnectionStats& operator=(const ConnectionStats& other) {
        if (this != &other) {
            total_bytes_sent.store(other.total_bytes_sent.load());
            total_bytes_received.store(other.total_bytes_received.load());
            connection_attempts.store(other.connection_attempts.load());
            successful_connections.store(other.successful_connections.load());
            failed_connections.store(other.failed_connections.load());
            last_connected_time_ns.store(other.last_connected_time_ns.load());
            last_activity_time_ns.store(other.last_activity_time_ns.load());
            average_latency_ms.store(other.average_latency_ms.load());
        }
        return *this;
    }

    void reset() noexcept {
        total_bytes_sent.store(0);
        total_bytes_received.store(0);
        connection_attempts.store(0);
        successful_connections.store(0);
        failed_connections.store(0);
        last_connected_time_ns.store(0);
        last_activity_time_ns.store(0);
        average_latency_ms.store(0);
    }

    void updateLastConnectedTime() noexcept {
        last_connected_time_ns.store(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void updateLastActivityTime() noexcept {
        last_activity_time_ns.store(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    [[nodiscard]] std::chrono::steady_clock::time_point getLastConnectedTime()
        const noexcept {
        return std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(last_connected_time_ns.load()));
    }

    [[nodiscard]] std::chrono::steady_clock::time_point getLastActivityTime()
        const noexcept {
        return std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(last_activity_time_ns.load()));
    }
};

/**
 * @brief Unified connection configuration
 */
struct TcpClientConfig {
    // Network options
    bool ipv6_enabled{false};
    bool keep_alive{true};
    bool no_delay{true};

    // Buffer sizes
    size_t receive_buffer_size{8192};
    size_t send_buffer_size{8192};

    // Timeouts
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds read_timeout{5000};
    std::chrono::milliseconds write_timeout{5000};

    // Reconnection settings
    bool auto_reconnect{false};
    int reconnect_attempts{3};
    std::chrono::milliseconds reconnect_delay{1000};

    // Heartbeat settings
    std::chrono::milliseconds heartbeat_interval{0};  // 0 = disabled

    // SSL settings
    bool use_ssl{false};
    bool verify_ssl{true};
    std::string ssl_certificate_path{};
    std::string ssl_private_key_path{};
    std::string ca_certificate_path{};
};

/**
 * @brief Proxy configuration
 */
struct ProxyConfig {
    std::string host;
    int port{0};
    std::string username;
    std::string password;
    bool enabled{false};
};

/**
 * @brief Common callback type definitions
 */
struct TcpCallbacks {
    using OnConnecting = std::function<void()>;
    using OnConnected = std::function<void()>;
    using OnDisconnected = std::function<void()>;
    using OnDataReceived = std::function<void(std::span<const char>)>;
    using OnError = std::function<void(const std::system_error&)>;
    using OnStateChanged =
        std::function<void(ConnectionState, ConnectionState)>;
    using OnHeartbeat = std::function<void()>;
};

/**
 * @brief Helper class for exponential backoff with jitter
 */
class BackoffCalculator {
public:
    BackoffCalculator(
        std::chrono::milliseconds initial_delay = std::chrono::seconds(1),
        std::chrono::milliseconds max_delay = std::chrono::seconds(30),
        double factor = 2.0, double jitter = 0.1)
        : initial_delay_(initial_delay),
          max_delay_(max_delay),
          factor_(factor),
          jitter_(jitter),
          current_delay_(initial_delay),
          attempt_(0) {}

    [[nodiscard]] std::chrono::milliseconds nextDelay() noexcept {
        if (attempt_ > 30) {
            reset();
        }

        if (attempt_ > 0) {
            auto new_delay =
                static_cast<double>(current_delay_.count()) * factor_;
            current_delay_ = std::chrono::milliseconds(
                std::min(static_cast<int64_t>(new_delay), max_delay_.count()));
        }

        // Simple jitter: vary by ±jitter_
        double jitter_factor =
            1.0 + (static_cast<double>(attempt_ % 10) / 50.0 - 0.1) * jitter_;
        auto jittered = static_cast<int64_t>(
            static_cast<double>(current_delay_.count()) * jitter_factor);

        attempt_++;
        return std::chrono::milliseconds(jittered);
    }

    void reset() noexcept {
        current_delay_ = initial_delay_;
        attempt_ = 0;
    }

    [[nodiscard]] int getAttemptCount() const noexcept { return attempt_; }

private:
    std::chrono::milliseconds initial_delay_;
    std::chrono::milliseconds max_delay_;
    double factor_;
    double jitter_;
    std::chrono::milliseconds current_delay_;
    int attempt_;
};

namespace detail {

/**
 * @brief Convert milliseconds to timeval components
 */
inline void msToTimeval(std::chrono::milliseconds ms, long& tv_sec,
                        long& tv_usec) noexcept {
    tv_sec = static_cast<long>(ms.count() / 1000);
    tv_usec = static_cast<long>((ms.count() % 1000) * 1000);
}

}  // namespace detail

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_TCP_COMMON_HPP
