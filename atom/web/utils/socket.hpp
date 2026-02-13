/*
 * socket.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: Modern C++20 socket utilities with RAII support

**************************************************/

#ifndef ATOM_WEB_UTILS_SOCKET_HPP
#define ATOM_WEB_UTILS_SOCKET_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "atom/type/compat.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

struct sockaddr;

namespace atom::web {

// Use compatibility expected type for cross-compiler support
template <typename T, typename E>
using expected = atom::type::compat::expected<T, E>;
template <typename E>
using unexpected = atom::type::compat::unexpected<E>;

/**
 * @brief Socket error codes
 */
enum class SocketError {
    Success = 0,
    InvalidHandle,
    CreationFailed,
    BindFailed,
    ListenFailed,
    AcceptFailed,
    ConnectFailed,
    SendFailed,
    ReceiveFailed,
    Timeout,
    ConnectionReset,
    ConnectionRefused,
    NetworkUnreachable,
    AddressInUse,
    WouldBlock,
    NotConnected,
    AlreadyConnected,
    InvalidAddress,
    OperationNotSupported,
    Unknown
};

/**
 * @brief Get error message for socket error code
 * @param error The socket error code
 * @return Error message string
 */
[[nodiscard]] constexpr auto socketErrorToString(SocketError error) noexcept
    -> std::string_view {
    switch (error) {
        case SocketError::Success:
            return "Success";
        case SocketError::InvalidHandle:
            return "Invalid socket handle";
        case SocketError::CreationFailed:
            return "Socket creation failed";
        case SocketError::BindFailed:
            return "Socket bind failed";
        case SocketError::ListenFailed:
            return "Socket listen failed";
        case SocketError::AcceptFailed:
            return "Socket accept failed";
        case SocketError::ConnectFailed:
            return "Connection failed";
        case SocketError::SendFailed:
            return "Send failed";
        case SocketError::ReceiveFailed:
            return "Receive failed";
        case SocketError::Timeout:
            return "Operation timed out";
        case SocketError::ConnectionReset:
            return "Connection reset by peer";
        case SocketError::ConnectionRefused:
            return "Connection refused";
        case SocketError::NetworkUnreachable:
            return "Network unreachable";
        case SocketError::AddressInUse:
            return "Address already in use";
        case SocketError::WouldBlock:
            return "Operation would block";
        case SocketError::NotConnected:
            return "Socket not connected";
        case SocketError::AlreadyConnected:
            return "Socket already connected";
        case SocketError::InvalidAddress:
            return "Invalid address";
        case SocketError::OperationNotSupported:
            return "Operation not supported";
        case SocketError::Unknown:
            return "Unknown error";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Socket address family
 */
enum class AddressFamily : int {
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
    Unix = AF_UNIX,
    Unspecified = AF_UNSPEC
};

/**
 * @brief Socket type
 */
enum class SocketType : int {
    Stream = SOCK_STREAM,   ///< TCP
    Datagram = SOCK_DGRAM,  ///< UDP
    Raw = SOCK_RAW          ///< Raw socket
};

/**
 * @brief Socket protocol
 */
enum class SocketProtocol : int {
    TCP = IPPROTO_TCP,
    UDP = IPPROTO_UDP,
    Auto = 0
};

/**
 * @brief Socket options configuration
 */
struct SocketOptions {
    bool reuseAddress{true};                   ///< SO_REUSEADDR
    bool reusePort{false};                     ///< SO_REUSEPORT (Unix only)
    bool keepAlive{false};                     ///< SO_KEEPALIVE
    bool noDelay{true};                        ///< TCP_NODELAY
    bool nonBlocking{false};                   ///< Non-blocking mode
    std::chrono::milliseconds sendTimeout{0};  ///< SO_SNDTIMEO (0 = no timeout)
    std::chrono::milliseconds receiveTimeout{
        0};                       ///< SO_RCVTIMEO (0 = no timeout)
    size_t sendBufferSize{0};     ///< SO_SNDBUF (0 = system default)
    size_t receiveBufferSize{0};  ///< SO_RCVBUF (0 = system default)
    int ttl{0};                   ///< IP_TTL (0 = system default)
    bool broadcast{false};        ///< SO_BROADCAST (UDP only)
};

/**
 * @brief Socket address wrapper
 */
struct SocketAddress {
    std::string host;
    uint16_t port{0};
    AddressFamily family{AddressFamily::IPv4};

    [[nodiscard]] auto toString() const -> std::string;
    [[nodiscard]] static auto parse(std::string_view address)
        -> expected<SocketAddress, SocketError>;
};

/**
 * @brief Connection information
 */
struct ConnectionInfo {
    SocketAddress localAddress;
    SocketAddress remoteAddress;
    std::chrono::steady_clock::time_point connectedAt;
    size_t bytesSent{0};
    size_t bytesReceived{0};
};

/**
 * @class Socket
 * @brief RAII wrapper for socket operations with modern C++ interface
 */
class Socket {
public:
    /**
     * @brief Default constructor - creates an invalid socket
     */
    Socket() noexcept = default;

    /**
     * @brief Create a socket with specified parameters
     * @param family Address family
     * @param type Socket type
     * @param protocol Socket protocol
     */
    explicit Socket(AddressFamily family, SocketType type = SocketType::Stream,
                    SocketProtocol protocol = SocketProtocol::Auto);

    /**
     * @brief Destructor - closes the socket
     */
    ~Socket();

    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /**
     * @brief Create a TCP socket
     * @param family Address family (IPv4 or IPv6)
     * @return Expected containing Socket or SocketError
     */
    [[nodiscard]] static auto createTcp(
        AddressFamily family = AddressFamily::IPv4)
        -> expected<Socket, SocketError>;

    /**
     * @brief Create a UDP socket
     * @param family Address family (IPv4 or IPv6)
     * @return Expected containing Socket or SocketError
     */
    [[nodiscard]] static auto createUdp(
        AddressFamily family = AddressFamily::IPv4)
        -> expected<Socket, SocketError>;

    /**
     * @brief Apply socket options
     * @param options The options to apply
     * @return Success or error
     */
    [[nodiscard]] auto applyOptions(const SocketOptions& options)
        -> expected<void, SocketError>;

    /**
     * @brief Bind socket to an address
     * @param address The address to bind to
     * @return Success or error
     */
    [[nodiscard]] auto bind(const SocketAddress& address)
        -> expected<void, SocketError>;

    /**
     * @brief Bind socket to a port on all interfaces
     * @param port The port to bind to
     * @return Success or error
     */
    [[nodiscard]] auto bind(uint16_t port) -> expected<void, SocketError>;

    /**
     * @brief Start listening for connections
     * @param backlog Maximum pending connections
     * @return Success or error
     */
    [[nodiscard]] auto listen(int backlog = SOMAXCONN)
        -> expected<void, SocketError>;

    /**
     * @brief Accept an incoming connection
     * @param timeout Maximum time to wait (0 = no timeout)
     * @return Expected containing new Socket or SocketError
     */
    [[nodiscard]] auto accept(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            0}) -> expected<Socket, SocketError>;

    /**
     * @brief Connect to a remote address
     * @param address The address to connect to
     * @param timeout Maximum time to wait
     * @return Success or error
     */
    [[nodiscard]] auto connect(
        const SocketAddress& address,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            5000}) -> expected<void, SocketError>;

    /**
     * @brief Connect to a host and port
     * @param host The hostname or IP address
     * @param port The port number
     * @param timeout Maximum time to wait
     * @return Success or error
     */
    [[nodiscard]] auto connect(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            5000}) -> expected<void, SocketError>;

    /**
     * @brief Send data
     * @param data The data to send
     * @return Number of bytes sent or error
     */
    [[nodiscard]] auto send(std::span<const std::byte> data)
        -> expected<size_t, SocketError>;

    /**
     * @brief Send string data
     * @param data The string to send
     * @return Number of bytes sent or error
     */
    [[nodiscard]] auto send(std::string_view data)
        -> expected<size_t, SocketError>;

    /**
     * @brief Send all data (blocking until complete)
     * @param data The data to send
     * @param timeout Maximum time to wait
     * @return Success or error
     */
    [[nodiscard]] auto sendAll(
        std::span<const std::byte> data,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            30000}) -> expected<void, SocketError>;

    /**
     * @brief Receive data
     * @param buffer Buffer to receive into
     * @return Number of bytes received or error
     */
    [[nodiscard]] auto receive(std::span<std::byte> buffer)
        -> expected<size_t, SocketError>;

    /**
     * @brief Receive data with timeout
     * @param buffer Buffer to receive into
     * @param timeout Maximum time to wait
     * @return Number of bytes received or error
     */
    [[nodiscard]] auto receive(std::span<std::byte> buffer,
                               std::chrono::milliseconds timeout)
        -> expected<size_t, SocketError>;

    /**
     * @brief Receive exact amount of data
     * @param buffer Buffer to receive into
     * @param timeout Maximum time to wait
     * @return Success or error
     */
    [[nodiscard]] auto receiveExact(
        std::span<std::byte> buffer,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            30000}) -> expected<void, SocketError>;

    /**
     * @brief Receive a line of text (until newline)
     * @param maxLength Maximum line length
     * @param timeout Maximum time to wait
     * @return The received line or error
     */
    [[nodiscard]] auto receiveLine(
        size_t maxLength = 4096,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{
            30000}) -> expected<std::string, SocketError>;

    /**
     * @brief Shutdown the socket
     * @param how How to shutdown (read, write, or both)
     */
    void shutdown(int how = 2);  // 2 = SHUT_RDWR

    /**
     * @brief Close the socket
     */
    void close();

    /**
     * @brief Check if socket is valid
     * @return True if socket handle is valid
     */
    [[nodiscard]] auto isValid() const noexcept -> bool;

    /**
     * @brief Check if socket is connected
     * @return True if connected
     */
    [[nodiscard]] auto isConnected() const noexcept -> bool;

    /**
     * @brief Get the native socket handle
     * @return The native handle
     */
    [[nodiscard]] auto native() const noexcept -> SocketHandle;

    /**
     * @brief Release ownership of the socket handle
     * @return The native handle (caller takes ownership)
     */
    [[nodiscard]] auto release() noexcept -> SocketHandle;

    /**
     * @brief Get local address
     * @return The local address or error
     */
    [[nodiscard]] auto getLocalAddress() const
        -> expected<SocketAddress, SocketError>;

    /**
     * @brief Get remote address
     * @return The remote address or error
     */
    [[nodiscard]] auto getRemoteAddress() const
        -> expected<SocketAddress, SocketError>;

    /**
     * @brief Get connection information
     * @return Connection info or error
     */
    [[nodiscard]] auto getConnectionInfo() const
        -> expected<ConnectionInfo, SocketError>;

    /**
     * @brief Set non-blocking mode
     * @param nonBlocking True for non-blocking
     * @return Success or error
     */
    [[nodiscard]] auto setNonBlocking(bool nonBlocking = true)
        -> expected<void, SocketError>;

    /**
     * @brief Wait for socket to be readable
     * @param timeout Maximum time to wait
     * @return True if readable, false if timeout
     */
    [[nodiscard]] auto waitReadable(std::chrono::milliseconds timeout)
        -> expected<bool, SocketError>;

    /**
     * @brief Wait for socket to be writable
     * @param timeout Maximum time to wait
     * @return True if writable, false if timeout
     */
    [[nodiscard]] auto waitWritable(std::chrono::milliseconds timeout)
        -> expected<bool, SocketError>;

    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }

private:
    SocketHandle handle_{INVALID_SOCKET_HANDLE};
    bool connected_{false};
    AddressFamily family_{AddressFamily::IPv4};
    SocketType type_{SocketType::Stream};
    std::chrono::steady_clock::time_point connectedAt_;
    size_t bytesSent_{0};
    size_t bytesReceived_{0};
};

// ============================================================================
// Legacy API (for backward compatibility)
// ============================================================================

/**
 * @brief Initialize networking subsystem (Windows-specific)
 * @return true if initialization succeeded, false otherwise
 * @throws std::runtime_error if initialization fails
 */
auto initializeWindowsSocketAPI() -> bool;

/**
 * @brief Create a new socket
 * @return int Socket file descriptor, -1 on error
 * @throws std::runtime_error if socket creation fails
 * @deprecated Use Socket class instead
 */
[[deprecated("Use Socket class instead")]]
auto createSocket() -> int;

/**
 * @brief Bind a socket to a specific port
 * @param sockfd Socket file descriptor
 * @param port Port number to bind to
 * @return bool True if binding succeeded, false otherwise
 * @deprecated Use Socket class instead
 */
[[deprecated("Use Socket class instead")]]
auto bindSocket(int sockfd, uint16_t port) -> bool;

/**
 * @brief Set a socket to non-blocking mode
 * @param sockfd Socket file descriptor
 * @return bool True if setting succeeded, false otherwise
 * @deprecated Use Socket class instead
 */
[[deprecated("Use Socket class instead")]]
auto setSocketNonBlocking(int sockfd) -> bool;

/**
 * @brief Connect to a remote address with a timeout
 * @param sockfd Socket file descriptor
 * @param addr Remote address to connect to
 * @param addrlen Length of the address structure
 * @param timeout Maximum time to wait for connection
 * @return bool True if connection succeeded, false otherwise
 * @deprecated Use Socket class instead
 */
[[deprecated("Use Socket class instead")]]
auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen,
                        std::chrono::milliseconds timeout) -> bool;

/**
 * @brief Get the last socket error as SocketError enum
 * @return The socket error
 */
[[nodiscard]] auto getLastSocketError() -> SocketError;

/**
 * @brief Get the last socket error message
 * @return Error message string
 */
[[nodiscard]] auto getLastSocketErrorMessage() -> std::string;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_SOCKET_HPP
