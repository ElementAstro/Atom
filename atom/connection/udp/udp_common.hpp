/*
 * udp_common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************
Date: 2024-5-24
Description: Common UDP types and utilities shared across UDP components
*************************************************/

#ifndef ATOM_CONNECTION_UDP_COMMON_HPP
#define ATOM_CONNECTION_UDP_COMMON_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include "atom/type/expected.hpp"

namespace atom::connection::udp {

/**
 * @brief Error codes for UDP operations
 */
enum class UdpError {
    None = 0,
    SocketCreationFailed,
    BindFailed,
    SendFailed,
    ReceiveFailed,
    HostNotFound,
    Timeout,
    InvalidParameter,
    InternalError,
    MulticastError,
    BroadcastError,
    NotInitialized,
    NotSupported,
    NetworkInitFailed,
    NotRunning,
    InvalidAddress,
    InvalidPort,
    AlreadyRunning,
    OperationAborted
};

/**
 * @brief Convert UdpError to string for logging/debugging
 */
[[nodiscard]] constexpr const char* udpErrorToString(UdpError error) noexcept {
    switch (error) {
        case UdpError::None:
            return "None";
        case UdpError::SocketCreationFailed:
            return "SocketCreationFailed";
        case UdpError::BindFailed:
            return "BindFailed";
        case UdpError::SendFailed:
            return "SendFailed";
        case UdpError::ReceiveFailed:
            return "ReceiveFailed";
        case UdpError::HostNotFound:
            return "HostNotFound";
        case UdpError::Timeout:
            return "Timeout";
        case UdpError::InvalidParameter:
            return "InvalidParameter";
        case UdpError::InternalError:
            return "InternalError";
        case UdpError::MulticastError:
            return "MulticastError";
        case UdpError::BroadcastError:
            return "BroadcastError";
        case UdpError::NotInitialized:
            return "NotInitialized";
        case UdpError::NotSupported:
            return "NotSupported";
        case UdpError::NetworkInitFailed:
            return "NetworkInitFailed";
        case UdpError::NotRunning:
            return "NotRunning";
        case UdpError::InvalidAddress:
            return "InvalidAddress";
        case UdpError::InvalidPort:
            return "InvalidPort";
        case UdpError::AlreadyRunning:
            return "AlreadyRunning";
        case UdpError::OperationAborted:
            return "OperationAborted";
        default:
            return "Unknown";
    }
}

/**
 * @brief UDP result type that can contain either a value or an error
 */
template <typename T>
using UdpResult = type::expected<T, UdpError>;

/**
 * @brief Socket options that can be configured for UDP sockets
 */
enum class SocketOption {
    Broadcast,         ///< Enable/disable broadcast
    ReuseAddress,      ///< Enable/disable address reuse
    ReusePort,         ///< Enable/disable port reuse (Unix only)
    ReceiveBufferSize, ///< Set receive buffer size
    SendBufferSize,    ///< Set send buffer size
    ReceiveTimeout,    ///< Set receive timeout
    SendTimeout,       ///< Set send timeout
    TTL,               ///< Time-to-live for packets
    MulticastTTL,      ///< TTL for multicast packets
    MulticastLoopback, ///< Enable/disable multicast loopback
    NonBlocking        ///< Set non-blocking mode
};

/**
 * @brief Structure holding information about a remote endpoint
 */
struct RemoteEndpoint {
    std::string host;
    uint16_t port{0};

    RemoteEndpoint() = default;
    RemoteEndpoint(std::string h, uint16_t p) : host(std::move(h)), port(p) {}

    [[nodiscard]] bool operator==(const RemoteEndpoint&) const = default;

    [[nodiscard]] bool isValid() const noexcept {
        return !host.empty() && port > 0;
    }
};

/**
 * @brief UDP packet statistics with atomic counters for thread-safety
 */
struct UdpStatistics {
    std::atomic<std::size_t> packetsReceived{0};
    std::atomic<std::size_t> packetsSent{0};
    std::atomic<std::size_t> bytesReceived{0};
    std::atomic<std::size_t> bytesSent{0};
    std::atomic<std::size_t> receiveErrors{0};
    std::atomic<std::size_t> sendErrors{0};
    std::atomic<std::size_t> totalErrors{0};
    std::chrono::steady_clock::time_point startTime{
        std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point lastActivity{
        std::chrono::steady_clock::now()};

    UdpStatistics() = default;

    UdpStatistics(const UdpStatistics& other)
        : packetsReceived(other.packetsReceived.load(std::memory_order_relaxed)),
          packetsSent(other.packetsSent.load(std::memory_order_relaxed)),
          bytesReceived(other.bytesReceived.load(std::memory_order_relaxed)),
          bytesSent(other.bytesSent.load(std::memory_order_relaxed)),
          receiveErrors(other.receiveErrors.load(std::memory_order_relaxed)),
          sendErrors(other.sendErrors.load(std::memory_order_relaxed)),
          totalErrors(other.totalErrors.load(std::memory_order_relaxed)),
          startTime(other.startTime),
          lastActivity(other.lastActivity) {}

    UdpStatistics& operator=(const UdpStatistics& other) {
        if (this != &other) {
            packetsReceived.store(
                other.packetsReceived.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            packetsSent.store(
                other.packetsSent.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytesReceived.store(
                other.bytesReceived.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            bytesSent.store(other.bytesSent.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
            receiveErrors.store(
                other.receiveErrors.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            sendErrors.store(other.sendErrors.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
            totalErrors.store(other.totalErrors.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
            startTime = other.startTime;
            lastActivity = other.lastActivity;
        }
        return *this;
    }

    /**
     * @brief Resets all statistical counters to zero
     */
    void reset() noexcept {
        packetsReceived.store(0, std::memory_order_relaxed);
        packetsSent.store(0, std::memory_order_relaxed);
        bytesReceived.store(0, std::memory_order_relaxed);
        bytesSent.store(0, std::memory_order_relaxed);
        receiveErrors.store(0, std::memory_order_relaxed);
        sendErrors.store(0, std::memory_order_relaxed);
        totalErrors.store(0, std::memory_order_relaxed);
        startTime = std::chrono::steady_clock::now();
        lastActivity = startTime;
    }

    /**
     * @brief Update last activity timestamp
     */
    void touch() noexcept { lastActivity = std::chrono::steady_clock::now(); }

    /**
     * @brief Record a successful send
     */
    void recordSend(std::size_t bytes) noexcept {
        packetsSent.fetch_add(1, std::memory_order_relaxed);
        bytesSent.fetch_add(bytes, std::memory_order_relaxed);
        touch();
    }

    /**
     * @brief Record a successful receive
     */
    void recordReceive(std::size_t bytes) noexcept {
        packetsReceived.fetch_add(1, std::memory_order_relaxed);
        bytesReceived.fetch_add(bytes, std::memory_order_relaxed);
        touch();
    }

    /**
     * @brief Record a send error
     */
    void recordSendError() noexcept {
        sendErrors.fetch_add(1, std::memory_order_relaxed);
        totalErrors.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Record a receive error
     */
    void recordReceiveError() noexcept {
        receiveErrors.fetch_add(1, std::memory_order_relaxed);
        totalErrors.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Get uptime in seconds
     */
    [[nodiscard]] double getUptimeSeconds() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - startTime).count();
    }

    /**
     * @brief Get seconds since last activity
     */
    [[nodiscard]] double getIdleSeconds() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - lastActivity).count();
    }
};

/**
 * @brief Socket configuration options
 */
struct SocketConfig {
    bool reuseAddress = true;
    bool reusePort = false;
    bool broadcast = false;
    bool nonBlocking = true;
    int sendBufferSize = 0;     ///< 0 means use system default
    int receiveBufferSize = 0;  ///< 0 means use system default
    int ttl = 0;                ///< 0 means use system default
    int multicastTtl = 1;       ///< Default multicast TTL
    bool multicastLoopback = false;
    std::chrono::milliseconds sendTimeout{0};     ///< 0 means no timeout
    std::chrono::milliseconds receiveTimeout{0};  ///< 0 means no timeout
};

// Constants
namespace constants {
constexpr uint16_t MIN_USER_PORT = 1024;
constexpr uint16_t MAX_PORT = 65535;
constexpr std::size_t DEFAULT_BUFFER_SIZE = 8192;
constexpr std::size_t MAX_UDP_PACKET_SIZE = 65507;  // Max UDP payload
constexpr std::size_t MAX_QUEUE_SIZE = 1000;
constexpr int DEFAULT_MULTICAST_TTL = 1;
}  // namespace constants

/**
 * @brief Validate port number
 */
[[nodiscard]] constexpr bool isValidPort(uint16_t port) noexcept {
    return port > 0 && port <= constants::MAX_PORT;
}

/**
 * @brief Validate user port number (non-privileged)
 */
[[nodiscard]] constexpr bool isValidUserPort(uint16_t port) noexcept {
    return port >= constants::MIN_USER_PORT && port <= constants::MAX_PORT;
}

}  // namespace atom::connection::udp

#endif  // ATOM_CONNECTION_UDP_COMMON_HPP
