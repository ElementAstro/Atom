/*
 * socket.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "socket.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <mstcpip.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <spdlog/spdlog.h>

namespace atom::web {

namespace {
constexpr size_t ERROR_BUFFER_SIZE = 256;

auto getLastErrorMessage() -> std::string {
    std::array<char, ERROR_BUFFER_SIZE> buffer{};

#ifdef _WIN32
    int error = WSAGetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    return std::format("Error {}: {}", error, buffer.data());
#else
    int error = errno;
    // Use the XSI-compliant version
    if (strerror_r(error, buffer.data(), buffer.size()) == 0) {
        return std::format("Error {}: {}", error, buffer.data());
    }
    return std::format("Error {}: Unknown error", error);
#endif
}

auto mapSystemError() -> SocketError {
#ifdef _WIN32
    switch (WSAGetLastError()) {
        case WSAEWOULDBLOCK:
            return SocketError::WouldBlock;
        case WSAECONNREFUSED:
            return SocketError::ConnectionRefused;
        case WSAECONNRESET:
            return SocketError::ConnectionReset;
        case WSAENETUNREACH:
            return SocketError::NetworkUnreachable;
        case WSAEADDRINUSE:
            return SocketError::AddressInUse;
        case WSAETIMEDOUT:
            return SocketError::Timeout;
        case WSAENOTCONN:
            return SocketError::NotConnected;
        case WSAEISCONN:
            return SocketError::AlreadyConnected;
        default:
            return SocketError::Unknown;
    }
#else
    switch (errno) {
        case EAGAIN:
        case EWOULDBLOCK:
            return SocketError::WouldBlock;
        case ECONNREFUSED:
            return SocketError::ConnectionRefused;
        case ECONNRESET:
            return SocketError::ConnectionReset;
        case ENETUNREACH:
            return SocketError::NetworkUnreachable;
        case EADDRINUSE:
            return SocketError::AddressInUse;
        case ETIMEDOUT:
            return SocketError::Timeout;
        case ENOTCONN:
            return SocketError::NotConnected;
        case EISCONN:
            return SocketError::AlreadyConnected;
        default:
            return SocketError::Unknown;
    }
#endif
}

auto closeSocketHandle(SocketHandle handle) -> void {
    if (handle == INVALID_SOCKET_HANDLE)
        return;
#ifdef _WIN32
    closesocket(handle);
#else
    ::close(handle);
#endif
}

}  // namespace

auto initializeWindowsSocketAPI() -> bool {
#ifdef _WIN32
    static bool initialized = false;
    if (initialized) {
        return true;
    }

    try {
        WSADATA wsaData;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (ret != 0) {
            std::string errorMsg =
                std::format("WSAStartup failed with error: {}", ret);
            spdlog::error(errorMsg);
            throw std::runtime_error(errorMsg);
        }

        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
            WSACleanup();
            std::string errorMsg = "Requested Winsock version not supported";
            spdlog::error(errorMsg);
            throw std::runtime_error(errorMsg);
        }

        initialized = true;
        spdlog::debug("Windows Socket API initialized successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize Windows Socket API: {}", e.what());
        return false;
    }
#else
    return true;
#endif
}

auto createSocket() -> int {
    try {
        int sockfd =
            static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (sockfd < 0) {
            std::string errorMsg = std::format("Socket creation failed: {}",
                                               getLastErrorMessage());
            spdlog::error(errorMsg);
            throw std::runtime_error(errorMsg);
        }

        int reuseAddr = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuseAddr),
                       sizeof(reuseAddr)) != 0) {
            spdlog::warn("Failed to set SO_REUSEADDR: {}",
                         getLastErrorMessage());
        }

        spdlog::trace("Socket created successfully with fd: {}", sockfd);
        return sockfd;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create socket: {}", e.what());
#ifdef _WIN32
        WSACleanup();
#endif
        throw;
    }
}

auto bindSocket(int sockfd, uint16_t port) -> bool {
    try {
        if (sockfd < 0) {
            spdlog::error("Invalid socket file descriptor: {}", sockfd);
            return false;
        }

        struct sockaddr_in addr {};
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) != 0) {
            std::string errorMsg = getLastErrorMessage();
            spdlog::error("Failed to bind socket to port {}: {}", port,
                          errorMsg);
            return false;
        }

        spdlog::trace("Socket bound successfully to port {}", port);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in bindSocket: {}", e.what());
        return false;
    }
}

auto setSocketNonBlocking(int sockfd) -> bool {
    try {
        if (sockfd < 0) {
            spdlog::error("Invalid socket file descriptor: {}", sockfd);
            return false;
        }

#ifdef _WIN32
        unsigned long mode = 1;
        if (ioctlsocket(sockfd, FIONBIO, &mode) != 0) {
            spdlog::error("Failed to set socket non-blocking: {}",
                          getLastErrorMessage());
            return false;
        }
#else
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            spdlog::error("Failed to get socket flags: {}",
                          getLastErrorMessage());
            return false;
        }

        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            spdlog::error("Failed to set socket non-blocking: {}",
                          getLastErrorMessage());
            return false;
        }
#endif

        spdlog::trace("Socket set to non-blocking mode successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in setSocketNonBlocking: {}", e.what());
        return false;
    }
}

auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen,
                        std::chrono::milliseconds timeout) -> bool {
    try {
        if (sockfd < 0 || !addr) {
            spdlog::error("Invalid parameters: sockfd={}, addr={}", sockfd,
                          static_cast<const void*>(addr));
            return false;
        }

        if (!setSocketNonBlocking(sockfd)) {
            spdlog::error(
                "Failed to set socket non-blocking for connect timeout");
            return false;
        }

        int ret = connect(sockfd, addr, addrlen);
        if (ret == 0) {
            spdlog::trace("Connected immediately without timeout");
            return true;
        }

#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            spdlog::error("Connect failed immediately: {}",
                          getLastErrorMessage());
            return false;
        }

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(static_cast<SOCKET>(sockfd), &writefds);

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        ret = select(0, nullptr, &writefds, nullptr, &tv);
#else
        if (errno != EINPROGRESS) {
            spdlog::error("Connect failed immediately: {}",
                          getLastErrorMessage());
            return false;
        }

        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
#endif

        if (ret < 0) {
            spdlog::error("Select/poll failed during connect: {}",
                          getLastErrorMessage());
            return false;
        } else if (ret == 0) {
            spdlog::debug("Connect timeout after {} ms", timeout.count());
            return false;
        }

        int socketError = 0;
        socklen_t len = sizeof(socketError);
        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&socketError), &len);

        if (ret != 0) {
            spdlog::error("getsockopt failed: {}", getLastErrorMessage());
            return false;
        }

        if (socketError != 0) {
            spdlog::error("Socket error during connect: {}", socketError);
            return false;
        }

        spdlog::trace("Connected successfully with timeout");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception in connectWithTimeout: {}", e.what());
        return false;
    }
}

// ============================================================================
// SocketAddress implementation
// ============================================================================

auto SocketAddress::toString() const -> std::string {
    if (family == AddressFamily::IPv6) {
        return std::format("[{}]:{}", host, port);
    }
    return std::format("{}:{}", host, port);
}

auto SocketAddress::parse(std::string_view address)
    -> expected<SocketAddress, SocketError> {
    SocketAddress result;

    // Check for IPv6 format [host]:port
    if (address.starts_with('[')) {
        auto closeBracket = address.find(']');
        if (closeBracket == std::string_view::npos) {
            return unexpected(SocketError::InvalidAddress);
        }
        result.host = std::string(address.substr(1, closeBracket - 1));
        result.family = AddressFamily::IPv6;

        if (closeBracket + 1 < address.size() &&
            address[closeBracket + 1] == ':') {
            auto portStr = address.substr(closeBracket + 2);
            try {
                result.port =
                    static_cast<uint16_t>(std::stoi(std::string(portStr)));
            } catch (...) {
                return unexpected(SocketError::InvalidAddress);
            }
        }
    } else {
        // IPv4 format host:port
        auto colonPos = address.rfind(':');
        if (colonPos != std::string_view::npos) {
            result.host = std::string(address.substr(0, colonPos));
            auto portStr = address.substr(colonPos + 1);
            try {
                result.port =
                    static_cast<uint16_t>(std::stoi(std::string(portStr)));
            } catch (...) {
                return unexpected(SocketError::InvalidAddress);
            }
        } else {
            result.host = std::string(address);
        }
        result.family = AddressFamily::IPv4;
    }

    return result;
}

// ============================================================================
// Socket implementation
// ============================================================================

Socket::Socket(AddressFamily family, SocketType type, SocketProtocol protocol)
    : family_(family), type_(type) {
    initializeWindowsSocketAPI();

    handle_ = socket(static_cast<int>(family), static_cast<int>(type),
                     static_cast<int>(protocol));
    if (handle_ == INVALID_SOCKET_HANDLE) {
        spdlog::error("Socket creation failed: {}", getLastErrorMessage());
    }
}

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_),
      connected_(other.connected_),
      family_(other.family_),
      type_(other.type_),
      connectedAt_(other.connectedAt_),
      bytesSent_(other.bytesSent_),
      bytesReceived_(other.bytesReceived_) {
    other.handle_ = INVALID_SOCKET_HANDLE;
    other.connected_ = false;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        connected_ = other.connected_;
        family_ = other.family_;
        type_ = other.type_;
        connectedAt_ = other.connectedAt_;
        bytesSent_ = other.bytesSent_;
        bytesReceived_ = other.bytesReceived_;
        other.handle_ = INVALID_SOCKET_HANDLE;
        other.connected_ = false;
    }
    return *this;
}

auto Socket::createTcp(AddressFamily family) -> expected<Socket, SocketError> {
    Socket sock(family, SocketType::Stream, SocketProtocol::TCP);
    if (!sock.isValid()) {
        return unexpected(SocketError::CreationFailed);
    }
    return sock;
}

auto Socket::createUdp(AddressFamily family) -> expected<Socket, SocketError> {
    Socket sock(family, SocketType::Datagram, SocketProtocol::UDP);
    if (!sock.isValid()) {
        return unexpected(SocketError::CreationFailed);
    }
    return sock;
}

auto Socket::applyOptions(const SocketOptions& options)
    -> expected<void, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    int optval = 1;

    if (options.reuseAddress) {
        if (setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&optval),
                       sizeof(optval)) != 0) {
            spdlog::warn("Failed to set SO_REUSEADDR");
        }
    }

#ifndef _WIN32
    if (options.reusePort) {
        if (setsockopt(handle_, SOL_SOCKET, SO_REUSEPORT,
                       reinterpret_cast<const char*>(&optval),
                       sizeof(optval)) != 0) {
            spdlog::warn("Failed to set SO_REUSEPORT");
        }
    }
#endif

    if (options.keepAlive) {
        if (setsockopt(handle_, SOL_SOCKET, SO_KEEPALIVE,
                       reinterpret_cast<const char*>(&optval),
                       sizeof(optval)) != 0) {
            spdlog::warn("Failed to set SO_KEEPALIVE");
        }
    }

    if (options.noDelay && type_ == SocketType::Stream) {
        if (setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&optval),
                       sizeof(optval)) != 0) {
            spdlog::warn("Failed to set TCP_NODELAY");
        }
    }

    if (options.broadcast && type_ == SocketType::Datagram) {
        if (setsockopt(handle_, SOL_SOCKET, SO_BROADCAST,
                       reinterpret_cast<const char*>(&optval),
                       sizeof(optval)) != 0) {
            spdlog::warn("Failed to set SO_BROADCAST");
        }
    }

    if (options.sendBufferSize > 0) {
        int bufSize = static_cast<int>(options.sendBufferSize);
        if (setsockopt(handle_, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&bufSize),
                       sizeof(bufSize)) != 0) {
            spdlog::warn("Failed to set SO_SNDBUF");
        }
    }

    if (options.receiveBufferSize > 0) {
        int bufSize = static_cast<int>(options.receiveBufferSize);
        if (setsockopt(handle_, SOL_SOCKET, SO_RCVBUF,
                       reinterpret_cast<const char*>(&bufSize),
                       sizeof(bufSize)) != 0) {
            spdlog::warn("Failed to set SO_RCVBUF");
        }
    }

    if (options.nonBlocking) {
        auto result = setNonBlocking(true);
        if (!result) {
            return result;
        }
    }

    return {};
}

auto Socket::bind(const SocketAddress& address) -> expected<void, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    struct sockaddr_storage addr {};
    socklen_t addrLen = 0;

    if (address.family == AddressFamily::IPv4) {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address.port);
        if (address.host.empty() || address.host == "0.0.0.0") {
            addr4->sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, address.host.c_str(), &addr4->sin_addr);
        }
        addrLen = sizeof(struct sockaddr_in);
    } else {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address.port);
        if (address.host.empty() || address.host == "::") {
            addr6->sin6_addr = in6addr_any;
        } else {
            inet_pton(AF_INET6, address.host.c_str(), &addr6->sin6_addr);
        }
        addrLen = sizeof(struct sockaddr_in6);
    }

    if (::bind(handle_, reinterpret_cast<struct sockaddr*>(&addr), addrLen) !=
        0) {
        spdlog::error("Bind failed: {}", getLastErrorMessage());
        return unexpected(SocketError::BindFailed);
    }

    return {};
}

auto Socket::bind(uint16_t port) -> expected<void, SocketError> {
    return bind(SocketAddress{"", port, family_});
}

auto Socket::listen(int backlog) -> expected<void, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    if (::listen(handle_, backlog) != 0) {
        spdlog::error("Listen failed: {}", getLastErrorMessage());
        return unexpected(SocketError::ListenFailed);
    }

    return {};
}

auto Socket::accept(std::chrono::milliseconds timeout)
    -> expected<Socket, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    if (timeout.count() > 0) {
        auto readable = waitReadable(timeout);
        if (!readable) {
            return unexpected(readable.error());
        }
        if (!*readable) {
            return unexpected(SocketError::Timeout);
        }
    }

    struct sockaddr_storage clientAddr {};
    socklen_t clientAddrLen = sizeof(clientAddr);

    SocketHandle clientHandle =
        ::accept(handle_, reinterpret_cast<struct sockaddr*>(&clientAddr),
                 &clientAddrLen);

    if (clientHandle == INVALID_SOCKET_HANDLE) {
        return unexpected(mapSystemError());
    }

    Socket clientSocket;
    clientSocket.handle_ = clientHandle;
    clientSocket.connected_ = true;
    clientSocket.family_ = family_;
    clientSocket.type_ = type_;
    clientSocket.connectedAt_ = std::chrono::steady_clock::now();

    return clientSocket;
}

auto Socket::connect(const SocketAddress& address,
                     std::chrono::milliseconds timeout)
    -> expected<void, SocketError> {
    return connect(address.host, address.port, timeout);
}

auto Socket::connect(std::string_view host, uint16_t port,
                     std::chrono::milliseconds timeout)
    -> expected<void, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    struct addrinfo hints {};
    hints.ai_family = static_cast<int>(family_);
    hints.ai_socktype = static_cast<int>(type_);

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);

    if (getaddrinfo(std::string(host).c_str(), portStr.c_str(), &hints,
                    &result) != 0) {
        return unexpected(SocketError::InvalidAddress);
    }

    std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrInfo(
        result, freeaddrinfo);

    // Set non-blocking for timeout support
    auto nbResult = setNonBlocking(true);
    if (!nbResult) {
        return nbResult;
    }

    int ret = ::connect(handle_, addrInfo->ai_addr,
                        static_cast<socklen_t>(addrInfo->ai_addrlen));

    if (ret == 0) {
        connected_ = true;
        connectedAt_ = std::chrono::steady_clock::now();
        return {};
    }

#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        return unexpected(mapSystemError());
    }
#else
    if (errno != EINPROGRESS) {
        return unexpected(mapSystemError());
    }
#endif

    auto writable = waitWritable(timeout);
    if (!writable) {
        return unexpected(writable.error());
    }
    if (!*writable) {
        return unexpected(SocketError::Timeout);
    }

    int socketError = 0;
    socklen_t len = sizeof(socketError);
    if (getsockopt(handle_, SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char*>(&socketError), &len) != 0) {
        return unexpected(SocketError::ConnectFailed);
    }

    if (socketError != 0) {
        return unexpected(SocketError::ConnectFailed);
    }

    connected_ = true;
    connectedAt_ = std::chrono::steady_clock::now();
    return {};
}

auto Socket::send(std::span<const std::byte> data)
    -> expected<size_t, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    auto sent = ::send(handle_, reinterpret_cast<const char*>(data.data()),
                       static_cast<int>(data.size()), 0);

    if (sent < 0) {
        return unexpected(mapSystemError());
    }

    bytesSent_ += static_cast<size_t>(sent);
    return static_cast<size_t>(sent);
}

auto Socket::send(std::string_view data) -> expected<size_t, SocketError> {
    return send(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size()));
}

auto Socket::sendAll(std::span<const std::byte> data,
                     std::chrono::milliseconds timeout)
    -> expected<void, SocketError> {
    size_t totalSent = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (totalSent < data.size()) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            return unexpected(SocketError::Timeout);
        }

        auto writable = waitWritable(remaining);
        if (!writable || !*writable) {
            return unexpected(SocketError::Timeout);
        }

        auto result = send(data.subspan(totalSent));
        if (!result) {
            return unexpected(result.error());
        }
        totalSent += *result;
    }

    return {};
}

auto Socket::receive(std::span<std::byte> buffer)
    -> expected<size_t, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    auto received = ::recv(handle_, reinterpret_cast<char*>(buffer.data()),
                           static_cast<int>(buffer.size()), 0);

    if (received < 0) {
        return unexpected(mapSystemError());
    }

    if (received == 0) {
        connected_ = false;
    }

    bytesReceived_ += static_cast<size_t>(received);
    return static_cast<size_t>(received);
}

auto Socket::receive(std::span<std::byte> buffer,
                     std::chrono::milliseconds timeout)
    -> expected<size_t, SocketError> {
    auto readable = waitReadable(timeout);
    if (!readable) {
        return unexpected(readable.error());
    }
    if (!*readable) {
        return unexpected(SocketError::Timeout);
    }
    return receive(buffer);
}

auto Socket::receiveExact(std::span<std::byte> buffer,
                          std::chrono::milliseconds timeout)
    -> expected<void, SocketError> {
    size_t totalReceived = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (totalReceived < buffer.size()) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            return unexpected(SocketError::Timeout);
        }

        auto result = receive(buffer.subspan(totalReceived), remaining);
        if (!result) {
            return unexpected(result.error());
        }
        if (*result == 0) {
            return unexpected(SocketError::ConnectionReset);
        }
        totalReceived += *result;
    }

    return {};
}

auto Socket::receiveLine(size_t maxLength, std::chrono::milliseconds timeout)
    -> expected<std::string, SocketError> {
    std::string line;
    line.reserve(256);
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (line.size() < maxLength) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            return unexpected(SocketError::Timeout);
        }

        std::byte ch;
        auto result = receive(std::span<std::byte>(&ch, 1), remaining);
        if (!result) {
            return unexpected(result.error());
        }
        if (*result == 0) {
            break;
        }

        char c = static_cast<char>(ch);
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            line += c;
        }
    }

    return line;
}

void Socket::shutdown(int how) {
    if (isValid()) {
        ::shutdown(handle_, how);
    }
}

void Socket::close() {
    if (handle_ != INVALID_SOCKET_HANDLE) {
        closeSocketHandle(handle_);
        handle_ = INVALID_SOCKET_HANDLE;
        connected_ = false;
    }
}

auto Socket::isValid() const noexcept -> bool {
    return handle_ != INVALID_SOCKET_HANDLE;
}

auto Socket::isConnected() const noexcept -> bool { return connected_; }

auto Socket::native() const noexcept -> SocketHandle { return handle_; }

auto Socket::release() noexcept -> SocketHandle {
    auto h = handle_;
    handle_ = INVALID_SOCKET_HANDLE;
    connected_ = false;
    return h;
}

auto Socket::getLocalAddress() const -> expected<SocketAddress, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    struct sockaddr_storage addr {};
    socklen_t addrLen = sizeof(addr);

    if (getsockname(handle_, reinterpret_cast<struct sockaddr*>(&addr),
                    &addrLen) != 0) {
        return unexpected(mapSystemError());
    }

    SocketAddress result;
    std::array<char, INET6_ADDRSTRLEN> ipStr{};

    if (addr.ss_family == AF_INET) {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &addr4->sin_addr, ipStr.data(), ipStr.size());
        result.host = ipStr.data();
        result.port = ntohs(addr4->sin_port);
        result.family = AddressFamily::IPv4;
    } else {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, ipStr.data(), ipStr.size());
        result.host = ipStr.data();
        result.port = ntohs(addr6->sin6_port);
        result.family = AddressFamily::IPv6;
    }

    return result;
}

auto Socket::getRemoteAddress() const -> expected<SocketAddress, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    struct sockaddr_storage addr {};
    socklen_t addrLen = sizeof(addr);

    if (getpeername(handle_, reinterpret_cast<struct sockaddr*>(&addr),
                    &addrLen) != 0) {
        return unexpected(mapSystemError());
    }

    SocketAddress result;
    std::array<char, INET6_ADDRSTRLEN> ipStr{};

    if (addr.ss_family == AF_INET) {
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &addr4->sin_addr, ipStr.data(), ipStr.size());
        result.host = ipStr.data();
        result.port = ntohs(addr4->sin_port);
        result.family = AddressFamily::IPv4;
    } else {
        auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, ipStr.data(), ipStr.size());
        result.host = ipStr.data();
        result.port = ntohs(addr6->sin6_port);
        result.family = AddressFamily::IPv6;
    }

    return result;
}

auto Socket::getConnectionInfo() const
    -> expected<ConnectionInfo, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

    ConnectionInfo info;

    auto localAddr = getLocalAddress();
    if (localAddr) {
        info.localAddress = *localAddr;
    }

    auto remoteAddr = getRemoteAddress();
    if (remoteAddr) {
        info.remoteAddress = *remoteAddr;
    }

    info.connectedAt = connectedAt_;
    info.bytesSent = bytesSent_;
    info.bytesReceived = bytesReceived_;

    return info;
}

auto Socket::setNonBlocking(bool nonBlocking) -> expected<void, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

#ifdef _WIN32
    unsigned long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(handle_, FIONBIO, &mode) != 0) {
        return unexpected(mapSystemError());
    }
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags == -1) {
        return unexpected(mapSystemError());
    }

    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(handle_, F_SETFL, flags) == -1) {
        return unexpected(mapSystemError());
    }
#endif

    return {};
}

auto Socket::waitReadable(std::chrono::milliseconds timeout)
    -> expected<bool, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

#ifdef _WIN32
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(handle_, &readfds);

    struct timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    int ret = select(0, &readfds, nullptr, nullptr, &tv);
#else
    struct pollfd pfd;
    pfd.fd = handle_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
#endif

    if (ret < 0) {
        return unexpected(mapSystemError());
    }

    return ret > 0;
}

auto Socket::waitWritable(std::chrono::milliseconds timeout)
    -> expected<bool, SocketError> {
    if (!isValid()) {
        return unexpected(SocketError::InvalidHandle);
    }

#ifdef _WIN32
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(handle_, &writefds);

    struct timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    int ret = select(0, nullptr, &writefds, nullptr, &tv);
#else
    struct pollfd pfd;
    pfd.fd = handle_;
    pfd.events = POLLOUT;

    int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
#endif

    if (ret < 0) {
        return unexpected(mapSystemError());
    }

    return ret > 0;
}

// ============================================================================
// Helper functions
// ============================================================================

auto getLastSocketError() -> SocketError { return mapSystemError(); }

auto getLastSocketErrorMessage() -> std::string {
    return getLastErrorMessage();
}

}  // namespace atom::web
