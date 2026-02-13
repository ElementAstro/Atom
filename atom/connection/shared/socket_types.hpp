// socket_types.hpp
/*
 * socket_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-29

Description: Common types for socket connection management

**************************************************/

#ifndef ATOM_CONNECTION_SOCKET_TYPES_HPP
#define ATOM_CONNECTION_SOCKET_TYPES_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace atom::connection {

/**
 * @enum LogLevel
 * @brief Log severity levels for socket operations
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG_LEVEL,
    INFO_LEVEL,
    WARNING_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL
};

/**
 * @struct ClientInfo
 * @brief Information about a connected client
 */
struct ClientInfo {
    int id{0};
    std::string address;
    std::chrono::steady_clock::time_point connected_time;
    uint64_t bytes_received{0};
    uint64_t bytes_sent{0};
    uint64_t messages_received{0};
    uint64_t messages_sent{0};
    bool is_authenticated{false};
};

/**
 * @struct Message
 * @brief Structure for socket message data exchange
 */
struct Message {
    enum class Type { TEXT, BINARY, PING, PONG, CLOSE };

    Type type{Type::TEXT};
    std::vector<char> data;
    size_t sender_id{0};

    /**
     * @brief Create a text message
     */
    static Message createText(std::string_view text, size_t sender = 0) {
        Message msg;
        msg.type = Type::TEXT;
        msg.data.assign(text.begin(), text.end());
        msg.sender_id = sender;
        return msg;
    }

    /**
     * @brief Create a binary message
     */
    static Message createBinary(const std::vector<char>& binary_data,
                                size_t sender = 0) {
        Message msg;
        msg.type = Type::BINARY;
        msg.data = binary_data;
        msg.sender_id = sender;
        return msg;
    }

    /**
     * @brief Create a binary message (move semantics)
     */
    static Message createBinary(std::vector<char>&& binary_data,
                                size_t sender = 0) {
        Message msg;
        msg.type = Type::BINARY;
        msg.data = std::move(binary_data);
        msg.sender_id = sender;
        return msg;
    }

    /**
     * @brief Get message data as string
     */
    [[nodiscard]] std::string asString() const {
        return std::string(data.begin(), data.end());
    }

    /**
     * @brief Check if message is empty
     */
    [[nodiscard]] bool empty() const noexcept { return data.empty(); }

    /**
     * @brief Get message size
     */
    [[nodiscard]] size_t size() const noexcept { return data.size(); }
};

/**
 * @struct SocketHubStats
 * @brief Statistics for monitoring socket hub performance
 */
struct SocketHubStats {
    std::atomic<size_t> total_connections{0};
    std::atomic<size_t> active_connections{0};
    std::atomic<size_t> messages_received{0};
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> bytes_sent{0};
    std::chrono::system_clock::time_point start_time{
        std::chrono::system_clock::now()};

    SocketHubStats() = default;

    SocketHubStats(const SocketHubStats& other)
        : total_connections(
              other.total_connections.load(std::memory_order_relaxed)),
          active_connections(
              other.active_connections.load(std::memory_order_relaxed)),
          messages_received(
              other.messages_received.load(std::memory_order_relaxed)),
          messages_sent(other.messages_sent.load(std::memory_order_relaxed)),
          bytes_received(other.bytes_received.load(std::memory_order_relaxed)),
          bytes_sent(other.bytes_sent.load(std::memory_order_relaxed)),
          start_time(other.start_time) {}

    SocketHubStats& operator=(const SocketHubStats& other) {
        if (this != &other) {
            total_connections.store(
                other.total_connections.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            active_connections.store(
                other.active_connections.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            messages_received.store(
                other.messages_received.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            messages_sent.store(
                other.messages_sent.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytes_received.store(
                other.bytes_received.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytes_sent.store(other.bytes_sent.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
            start_time = other.start_time;
        }
        return *this;
    }

    SocketHubStats(SocketHubStats&& other) noexcept
        : total_connections(
              other.total_connections.load(std::memory_order_relaxed)),
          active_connections(
              other.active_connections.load(std::memory_order_relaxed)),
          messages_received(
              other.messages_received.load(std::memory_order_relaxed)),
          messages_sent(other.messages_sent.load(std::memory_order_relaxed)),
          bytes_received(other.bytes_received.load(std::memory_order_relaxed)),
          bytes_sent(other.bytes_sent.load(std::memory_order_relaxed)),
          start_time(other.start_time) {}

    SocketHubStats& operator=(SocketHubStats&& other) noexcept {
        if (this != &other) {
            total_connections.store(
                other.total_connections.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            active_connections.store(
                other.active_connections.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            messages_received.store(
                other.messages_received.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            messages_sent.store(
                other.messages_sent.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytes_received.store(
                other.bytes_received.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytes_sent.store(other.bytes_sent.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
            start_time = other.start_time;
        }
        return *this;
    }

    /**
     * @brief Reset all statistics
     */
    void reset() noexcept {
        total_connections.store(0, std::memory_order_relaxed);
        active_connections.store(0, std::memory_order_relaxed);
        messages_received.store(0, std::memory_order_relaxed);
        messages_sent.store(0, std::memory_order_relaxed);
        bytes_received.store(0, std::memory_order_relaxed);
        bytes_sent.store(0, std::memory_order_relaxed);
        start_time = std::chrono::system_clock::now();
    }

    /**
     * @brief Get uptime duration
     */
    [[nodiscard]] std::chrono::seconds uptime() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time);
    }
};

/**
 * @struct SocketHubConfig
 * @brief Configuration for SocketHub
 */
struct SocketHubConfig {
    bool use_ssl{false};
    int backlog_size{10};
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds client_timeout{60};
    bool keep_alive{true};
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_dh_file;
    std::string ssl_password;
    bool enable_rate_limiting{false};
    int max_connections_per_ip{10};
    int max_messages_per_minute{100};
    LogLevel log_level{LogLevel::INFO_LEVEL};
    size_t read_buffer_size{16384};
    unsigned int thread_pool_size{0};  // 0 = auto-detect
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SOCKET_TYPES_HPP
