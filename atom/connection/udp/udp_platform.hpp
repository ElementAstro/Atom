/*
 * udp_platform.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************
Date: 2024-5-24
Description: Platform-specific UDP utilities and socket operations
*************************************************/

#ifndef ATOM_CONNECTION_UDP_PLATFORM_HPP
#define ATOM_CONNECTION_UDP_PLATFORM_HPP

#include <string>
#include <string_view>

#include "udp_common.hpp"

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

namespace atom::connection::udp::platform {

// Platform-specific type definitions
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#define UDP_CLOSE_SOCKET(s) closesocket(s)
#else
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#define UDP_CLOSE_SOCKET(s) ::close(s)
#endif

/**
 * @brief Initialize platform networking (Windows WSAStartup)
 * @return true on success
 */
[[nodiscard]] inline bool initNetworking() noexcept {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
#else
    return true;
#endif
}

/**
 * @brief Cleanup platform networking (Windows WSACleanup)
 */
inline void cleanupNetworking() noexcept {
#ifdef _WIN32
    WSACleanup();
#endif
}

/**
 * @brief RAII wrapper for platform networking initialization
 */
class NetworkInitializer {
public:
    NetworkInitializer() : initialized_(initNetworking()) {}
    ~NetworkInitializer() {
        if (initialized_) {
            cleanupNetworking();
        }
    }

    NetworkInitializer(const NetworkInitializer&) = delete;
    NetworkInitializer& operator=(const NetworkInitializer&) = delete;
    NetworkInitializer(NetworkInitializer&&) = delete;
    NetworkInitializer& operator=(NetworkInitializer&&) = delete;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] explicit operator bool() const noexcept {
        return initialized_;
    }

private:
    bool initialized_;
};

/**
 * @brief Get the last socket error code
 */
[[nodiscard]] inline int getLastSocketError() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/**
 * @brief Get the last socket error message
 */
[[nodiscard]] inline std::string getLastErrorMessage() noexcept {
#ifdef _WIN32
    int errorCode = WSAGetLastError();
    char* msgBuf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, errorCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);
    std::string message;
    if (msgBuf) {
        message = msgBuf;
        LocalFree(msgBuf);
        // Remove trailing newline
        while (!message.empty() &&
               (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
    } else {
        message = "Unknown error code: " + std::to_string(errorCode);
    }
    return message;
#else
    return std::strerror(errno);
#endif
}

/**
 * @brief Check if the last error indicates the operation would block
 */
[[nodiscard]] inline bool wouldBlock() noexcept {
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

/**
 * @brief Check if the last error indicates a timeout
 */
[[nodiscard]] inline bool isTimeout() noexcept {
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAETIMEDOUT || error == WSAEWOULDBLOCK;
#else
    return errno == ETIMEDOUT || errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

/**
 * @brief Set socket to non-blocking mode
 */
[[nodiscard]] inline bool setNonBlocking(SocketHandle socket) noexcept {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

/**
 * @brief Set socket to blocking mode
 */
[[nodiscard]] inline bool setBlocking(SocketHandle socket) noexcept {
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(socket, F_SETFL, flags & ~O_NONBLOCK) != -1;
#endif
}

/**
 * @brief Close a socket handle
 */
inline void closeSocket(SocketHandle& socket) noexcept {
    if (socket != INVALID_SOCKET_HANDLE) {
        UDP_CLOSE_SOCKET(socket);
        socket = INVALID_SOCKET_HANDLE;
    }
}

/**
 * @brief Validate an IPv4 address string
 */
[[nodiscard]] inline bool isValidIPv4Address(std::string_view ip) noexcept {
    if (ip.empty()) {
        return false;
    }
    struct sockaddr_in sa;
    return inet_pton(AF_INET, std::string(ip).c_str(), &(sa.sin_addr)) == 1;
}

/**
 * @brief Validate an IPv6 address string
 */
[[nodiscard]] inline bool isValidIPv6Address(std::string_view ip) noexcept {
    if (ip.empty()) {
        return false;
    }
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, std::string(ip).c_str(), &(sa.sin6_addr)) == 1;
}

/**
 * @brief Validate an IP address string (IPv4 or IPv6)
 */
[[nodiscard]] inline bool isValidIPAddress(std::string_view ip) noexcept {
    return isValidIPv4Address(ip) || isValidIPv6Address(ip);
}

/**
 * @brief Check if an IPv4 address is a multicast address
 */
[[nodiscard]] inline bool isMulticastAddress(std::string_view ip) noexcept {
    if (!isValidIPv4Address(ip)) {
        return false;
    }
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, std::string(ip).c_str(), &(sa.sin_addr)) != 1) {
        return false;
    }
    uint32_t addr = ntohl(sa.sin_addr.s_addr);
    // Multicast addresses are in the range 224.0.0.0 to 239.255.255.255
    return (addr & 0xF0000000) == 0xE0000000;
}

/**
 * @brief Check if an address is the broadcast address
 */
[[nodiscard]] inline bool isBroadcastAddress(std::string_view ip) noexcept {
    return ip == "255.255.255.255";
}

/**
 * @brief Set socket option with int value
 */
[[nodiscard]] inline bool setSocketOptInt(SocketHandle socket, int level,
                                          int optname, int value) noexcept {
    return setsockopt(socket, level, optname, reinterpret_cast<const char*>(&value),
                      sizeof(value)) == 0;
}

/**
 * @brief Set socket option with bool value
 */
[[nodiscard]] inline bool setSocketOptBool(SocketHandle socket, int level,
                                           int optname, bool value) noexcept {
    int intValue = value ? 1 : 0;
    return setSocketOptInt(socket, level, optname, intValue);
}

/**
 * @brief Set socket receive timeout
 */
[[nodiscard]] inline bool setReceiveTimeout(
    SocketHandle socket, std::chrono::milliseconds timeout) noexcept {
#ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                      reinterpret_cast<const char*>(&timeoutMs),
                      sizeof(timeoutMs)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                      reinterpret_cast<const char*>(&tv), sizeof(tv)) == 0;
#endif
}

/**
 * @brief Set socket send timeout
 */
[[nodiscard]] inline bool setSendTimeout(
    SocketHandle socket, std::chrono::milliseconds timeout) noexcept {
#ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    return setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                      reinterpret_cast<const char*>(&timeoutMs),
                      sizeof(timeoutMs)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    return setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                      reinterpret_cast<const char*>(&tv), sizeof(tv)) == 0;
#endif
}

/**
 * @brief Apply socket configuration options
 */
[[nodiscard]] inline UdpResult<bool> applySocketConfig(
    SocketHandle socket, const SocketConfig& config) noexcept {
    // Reuse address
    if (config.reuseAddress) {
        if (!setSocketOptBool(socket, SOL_SOCKET, SO_REUSEADDR, true)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // Reuse port (Unix only)
#ifndef _WIN32
    if (config.reusePort) {
        if (!setSocketOptBool(socket, SOL_SOCKET, SO_REUSEPORT, true)) {
            return type::unexpected(UdpError::InternalError);
        }
    }
#endif

    // Broadcast
    if (config.broadcast) {
        if (!setSocketOptBool(socket, SOL_SOCKET, SO_BROADCAST, true)) {
            return type::unexpected(UdpError::BroadcastError);
        }
    }

    // Send buffer size
    if (config.sendBufferSize > 0) {
        if (!setSocketOptInt(socket, SOL_SOCKET, SO_SNDBUF,
                             config.sendBufferSize)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // Receive buffer size
    if (config.receiveBufferSize > 0) {
        if (!setSocketOptInt(socket, SOL_SOCKET, SO_RCVBUF,
                             config.receiveBufferSize)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // TTL
    if (config.ttl > 0) {
        if (!setSocketOptInt(socket, IPPROTO_IP, IP_TTL, config.ttl)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // Multicast TTL
    if (config.multicastTtl > 0) {
        if (!setSocketOptInt(socket, IPPROTO_IP, IP_MULTICAST_TTL,
                             config.multicastTtl)) {
            return type::unexpected(UdpError::MulticastError);
        }
    }

    // Multicast loopback
    if (!setSocketOptBool(socket, IPPROTO_IP, IP_MULTICAST_LOOP,
                          config.multicastLoopback)) {
        // Non-fatal, some systems may not support this
    }

    // Non-blocking mode
    if (config.nonBlocking) {
        if (!setNonBlocking(socket)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // Send timeout
    if (config.sendTimeout.count() > 0) {
        if (!setSendTimeout(socket, config.sendTimeout)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    // Receive timeout
    if (config.receiveTimeout.count() > 0) {
        if (!setReceiveTimeout(socket, config.receiveTimeout)) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    return true;
}

/**
 * @brief Create a UDP socket
 */
[[nodiscard]] inline UdpResult<SocketHandle> createUdpSocket(
    bool ipv6 = false) noexcept {
    int family = ipv6 ? AF_INET6 : AF_INET;
    SocketHandle sock = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_HANDLE) {
        return type::unexpected(UdpError::SocketCreationFailed);
    }
    return sock;
}

/**
 * @brief Bind a UDP socket to a port
 */
[[nodiscard]] inline UdpResult<bool> bindSocket(SocketHandle socket,
                                                 uint16_t port,
                                                 bool ipv6 = false) noexcept {
    if (ipv6) {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        addr.sin6_addr = in6addr_any;
        if (::bind(socket, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(addr)) < 0) {
            return type::unexpected(UdpError::BindFailed);
        }
    } else {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (::bind(socket, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(addr)) < 0) {
            return type::unexpected(UdpError::BindFailed);
        }
    }
    return true;
}

/**
 * @brief Get the local port a socket is bound to
 */
[[nodiscard]] inline UdpResult<uint16_t> getLocalPort(
    SocketHandle socket) noexcept {
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    if (getsockname(socket, reinterpret_cast<struct sockaddr*>(&addr),
                    &addrLen) != 0) {
        return type::unexpected(UdpError::InternalError);
    }
    return ntohs(addr.sin_port);
}

}  // namespace atom::connection::udp::platform

#endif  // ATOM_CONNECTION_UDP_PLATFORM_HPP
