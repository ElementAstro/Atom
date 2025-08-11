/*
 * udpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: UDP Client Class

*************************************************/

#include "udpclient.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET(s) ::close(s)
#endif

namespace {
// Constants
constexpr uint16_t MAX_PORT = 65535;
constexpr size_t MAX_BUFFER_SIZE = 65536;
constexpr char BROADCAST_ADDR[] = "255.255.255.255";

// Utility functions
bool isValidPort(uint16_t port) {
    return port > 0 &&
           port <= MAX_PORT;  // Allow system ports for privileged processes
}

bool setSocketNonBlocking(int socket) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

std::string getLastErrorMsg() {
#ifdef _WIN32
    char* s = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, WSAGetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0,
                   NULL);
    std::string error(s);
    LocalFree(s);
    return error;
#else
    return std::strerror(errno);
#endif
}

bool isValidIpAddress(const std::string& ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) == 1;
}

bool isMulticastAddress(const std::string& ipAddress) {
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 1) {
        return false;
    }

    uint32_t addr = ntohl(sa.sin_addr.s_addr);
    // Multicast addresses are in the range 224.0.0.0 to 239.255.255.255
    return (addr & 0xF0000000) == 0xE0000000;
}
}  // namespace

namespace atom::connection {

class UdpClient::Impl {
public:
    Impl() {
        try {
            initializeSockets();
            createSocket();
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }

    Impl(uint16_t port, const SocketOptions& options = {}) {
        try {
            initializeSockets();
            createSocket();

            // Apply socket options before binding
            applySocketOptions(options);

            if (auto result = bind(port); !result) {
                throw std::runtime_error("Failed to bind UDP socket to port " +
                                         std::to_string(port) + ": " +
                                         getLastErrorMsg());
            }
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }

    ~Impl() { cleanup(); }

    void initializeSockets() {
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " +
                                     std::to_string(result));
        }
#endif
    }

    void createSocket() {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ < 0) {
            throw std::runtime_error("Socket creation failed: " +
                                     getLastErrorMsg());
        }

        // Set socket to non-blocking mode by default
        if (!setSocketNonBlocking(socket_)) {
            throw std::runtime_error(
                "Failed to set socket to non-blocking mode");
        }

#ifdef __linux__
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            throw std::runtime_error("Epoll creation failed: " +
                                     getLastErrorMsg());
        }
#endif
    }

    void cleanup() {
        stopReceiving();

        if (socket_ >= 0) {
            CLOSE_SOCKET(socket_);
            socket_ = -1;
        }

#ifdef __linux__
        if (epoll_fd_ >= 0) {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }
#endif

#ifdef _WIN32
        WSACleanup();
#endif
    }

    Impl(Impl&& other) noexcept
        : socket_(std::exchange(other.socket_, -1)),
#ifdef __linux__
          epoll_fd_(std::exchange(other.epoll_fd_, -1)),
#endif
          bound_(other.bound_.load()),
          receivingStopped_(other.receivingStopped_.load()),
          isReceiving_(other.isReceiving_.load()),
          statistics_(std::move(other.statistics_)) {
        // Move the thread if it's running
        receivingThread_ = std::move(other.receivingThread_);
    }

    UdpResult<bool> bind(uint16_t port) noexcept {
        try {
            if (!isValidPort(port) &&
                port != 0) {  // Allow port 0 for system-assigned port
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            // Set SO_REUSEADDR to prevent "address already in use" errors
            int reuseAddr = 1;
            if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                           reinterpret_cast<char*>(&reuseAddr),
                           sizeof(reuseAddr)) < 0) {
                return type::unexpected(UdpError::BindFailed);
            }

            if (::bind(socket_, reinterpret_cast<struct sockaddr*>(&address),
                       sizeof(address)) < 0) {
                return type::unexpected(UdpError::BindFailed);
            }

            bound_ = true;
            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<bool> applySocketOptions(const SocketOptions& options) noexcept {
        try {
            // Set reuse address
            if (options.reuseAddress) {
                int reuseAddr = 1;
                if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<char*>(&reuseAddr),
                               sizeof(reuseAddr)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
            }

            // Set reuse port (not available on Windows)
#ifndef _WIN32
            if (options.reusePort) {
                int reusePort = 1;
                if (setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT,
                               reinterpret_cast<char*>(&reusePort),
                               sizeof(reusePort)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
            }
#endif

            // Set broadcast permission
            if (options.broadcast) {
                int broadcast = 1;
                if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                               reinterpret_cast<char*>(&broadcast),
                               sizeof(broadcast)) < 0) {
                    return type::unexpected(UdpError::BroadcastError);
                }
            }

            // Set send buffer size
            if (options.sendBufferSize > 0) {
                if (setsockopt(
                        socket_, SOL_SOCKET, SO_SNDBUF,
                        reinterpret_cast<const char*>(&options.sendBufferSize),
                        sizeof(options.sendBufferSize)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
            }

            // Set receive buffer size
            if (options.receiveBufferSize > 0) {
                if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                               reinterpret_cast<const char*>(
                                   &options.receiveBufferSize),
                               sizeof(options.receiveBufferSize)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
            }

            // Set TTL
            if (options.ttl > 0) {
                if (setsockopt(socket_, IPPROTO_IP, IP_TTL,
                               reinterpret_cast<const char*>(&options.ttl),
                               sizeof(options.ttl)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
            }

            // Set non-blocking mode
            if (options.nonBlocking) {
                if (!setSocketNonBlocking(socket_)) {
                    return type::unexpected(UdpError::InternalError);
                }
            }

            // Set send timeout
            if (options.sendTimeout.count() > 0) {
#ifdef _WIN32
                DWORD timeout_ms =
                    static_cast<DWORD>(options.sendTimeout.count());
                if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                               reinterpret_cast<const char*>(&timeout_ms),
                               sizeof(timeout_ms)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
#else
                struct timeval tv;
                tv.tv_sec =
                    static_cast<time_t>(options.sendTimeout.count() / 1000);
                tv.tv_usec = static_cast<suseconds_t>(
                    (options.sendTimeout.count() % 1000) * 1000);
                if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                               reinterpret_cast<const char*>(&tv),
                               sizeof(tv)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
#endif
            }

            // Set receive timeout
            if (options.receiveTimeout.count() > 0) {
#ifdef _WIN32
                DWORD timeout_ms =
                    static_cast<DWORD>(options.receiveTimeout.count());
                if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                               reinterpret_cast<const char*>(&timeout_ms),
                               sizeof(timeout_ms)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
#else
                struct timeval tv;
                tv.tv_sec =
                    static_cast<time_t>(options.receiveTimeout.count() / 1000);
                tv.tv_usec = static_cast<suseconds_t>(
                    (options.receiveTimeout.count() % 1000) * 1000);
                if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                               reinterpret_cast<const char*>(&tv),
                               sizeof(tv)) < 0) {
                    return type::unexpected(UdpError::InternalError);
                }
#endif
            }

            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<size_t> send(const RemoteEndpoint& endpoint,
                           std::span<const char> data) noexcept {
        try {
            if (data.empty() || data.size() > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!isValidPort(endpoint.port)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct addrinfo hints{};
            struct addrinfo* result = nullptr;

            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            // Use getaddrinfo instead of gethostbyname (which is deprecated)
            int status = getaddrinfo(endpoint.host.c_str(),
                                     std::to_string(endpoint.port).c_str(),
                                     &hints, &result);
            if (status != 0) {
                return type::unexpected(UdpError::HostNotFound);
            }

            std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resultGuard(
                result, freeaddrinfo);

            ssize_t bytesSent =
                sendto(socket_, data.data(), data.size(), 0,
                       resultGuard->ai_addr, resultGuard->ai_addrlen);

            if (bytesSent < 0) {
                statistics_.sendErrors++;
                return type::unexpected(UdpError::SendFailed);
            }

            // Update statistics
            statistics_.packetsSent++;
            statistics_.bytesSent += static_cast<size_t>(bytesSent);
            statistics_.lastActivity = std::chrono::system_clock::now();

            return static_cast<size_t>(bytesSent);
        } catch (...) {
            statistics_.sendErrors++;
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<size_t> sendBroadcast(uint16_t port,
                                    std::span<const char> data) noexcept {
        try {
            if (data.empty() || data.size() > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!isValidPort(port)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            // Enable broadcasting if not already enabled
            int broadcast = 1;
            if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                           reinterpret_cast<char*>(&broadcast),
                           sizeof(broadcast)) < 0) {
                return type::unexpected(UdpError::BroadcastError);
            }

            struct sockaddr_in broadcastAddr{};
            broadcastAddr.sin_family = AF_INET;
            broadcastAddr.sin_port = htons(port);

            // Use 255.255.255.255 for broadcast
            if (inet_pton(AF_INET, BROADCAST_ADDR, &broadcastAddr.sin_addr) <=
                0) {
                return type::unexpected(UdpError::InternalError);
            }

            ssize_t bytesSent =
                sendto(socket_, data.data(), data.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&broadcastAddr),
                       sizeof(broadcastAddr));

            if (bytesSent < 0) {
                statistics_.sendErrors++;
                return type::unexpected(UdpError::SendFailed);
            }

            // Update statistics
            statistics_.packetsSent++;
            statistics_.bytesSent += static_cast<size_t>(bytesSent);
            statistics_.lastActivity = std::chrono::system_clock::now();

            return static_cast<size_t>(bytesSent);
        } catch (...) {
            statistics_.sendErrors++;
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<size_t> sendMultiple(const std::vector<RemoteEndpoint>& endpoints,
                                   std::span<const char> data) noexcept {
        try {
            if (data.empty() || data.size() > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (endpoints.empty()) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            size_t successCount = 0;

            for (const auto& endpoint : endpoints) {
                auto result = send(endpoint, data);
                if (result) {
                    successCount++;
                }
            }

            return successCount;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> receive(
        size_t maxSize, std::chrono::milliseconds timeout) noexcept {
        try {
            if (maxSize == 0 || maxSize > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            bool hasTimeout = timeout > std::chrono::milliseconds::zero();

            if (hasTimeout) {
#ifdef _WIN32
                // Set receive timeout on Windows
                DWORD timeout_ms = static_cast<DWORD>(timeout.count());
                if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                               reinterpret_cast<const char*>(&timeout_ms),
                               sizeof(timeout_ms)) != 0) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }
#else
                // Use epoll for timeout on Linux/Unix
                struct epoll_event event{};
                event.events = EPOLLIN;
                event.data.fd = socket_;

                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_, &event) ==
                    -1) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }

                struct epoll_event events[1];
                int nfds = epoll_wait(epoll_fd_, events, 1,
                                      static_cast<int>(timeout.count()));

                if (nfds == 0) {
                    return type::unexpected(UdpError::Timeout);
                } else if (nfds == -1) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }

                // Clean up after epoll
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket_, nullptr);
#endif
            }

            std::vector<char> data(maxSize);
            struct sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);

            ssize_t bytesRead =
                recvfrom(socket_, data.data(), maxSize, 0,
                         reinterpret_cast<struct sockaddr*>(&clientAddress),
                         &clientAddressLength);

            if (bytesRead < 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
                    return type::unexpected(UdpError::Timeout);
                }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return type::unexpected(UdpError::Timeout);
                }
#endif
                statistics_.receiveErrors++;
                return type::unexpected(UdpError::ReceiveFailed);
            }

            data.resize(bytesRead);

            RemoteEndpoint remote;
            remote.host = inet_ntoa(clientAddress.sin_addr);
            remote.port = ntohs(clientAddress.sin_port);

            // Update statistics
            statistics_.packetsReceived++;
            statistics_.bytesReceived += static_cast<size_t>(bytesRead);
            statistics_.lastActivity = std::chrono::system_clock::now();

            return std::make_pair(std::move(data), std::move(remote));
        } catch (...) {
            statistics_.receiveErrors++;
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<bool> joinMulticastGroup(
        const std::string& groupAddress) noexcept {
        try {
            if (!isValidIpAddress(groupAddress) ||
                !isMulticastAddress(groupAddress)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct ip_mreq mreq{};

            // Set the multicast IP address
            if (inet_pton(AF_INET, groupAddress.c_str(), &mreq.imr_multiaddr) <=
                0) {
                return type::unexpected(UdpError::MulticastError);
            }

            // Set the local interface to INADDR_ANY
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            // Join the multicast group
            if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           reinterpret_cast<char*>(&mreq), sizeof(mreq)) < 0) {
                return type::unexpected(UdpError::MulticastError);
            }

            // Store joined multicast groups for later use
            multicastGroups_.push_back(groupAddress);

            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<bool> leaveMulticastGroup(
        const std::string& groupAddress) noexcept {
        try {
            if (!isValidIpAddress(groupAddress) ||
                !isMulticastAddress(groupAddress)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            // Check if we've joined this group
            auto it = std::find(multicastGroups_.begin(),
                                multicastGroups_.end(), groupAddress);
            if (it == multicastGroups_.end()) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct ip_mreq mreq{};

            // Set the multicast IP address
            if (inet_pton(AF_INET, groupAddress.c_str(), &mreq.imr_multiaddr) <=
                0) {
                return type::unexpected(UdpError::MulticastError);
            }

            // Set the local interface to INADDR_ANY
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            // Leave the multicast group
            if (setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                           reinterpret_cast<char*>(&mreq), sizeof(mreq)) < 0) {
                return type::unexpected(UdpError::MulticastError);
            }

            // Remove from our list
            multicastGroups_.erase(it);

            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<size_t> sendToMulticastGroup(
        const std::string& groupAddress, uint16_t port,
        std::span<const char> data) noexcept {
        try {
            if (data.empty() || data.size() > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!isValidPort(port)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!isValidIpAddress(groupAddress) ||
                !isMulticastAddress(groupAddress)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            // Set the TTL for multicast packets (default to 1)
            int ttl = 1;
            if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
                           reinterpret_cast<char*>(&ttl), sizeof(ttl)) < 0) {
                return type::unexpected(UdpError::MulticastError);
            }

            struct sockaddr_in multicastAddr{};
            multicastAddr.sin_family = AF_INET;
            multicastAddr.sin_port = htons(port);

            if (inet_pton(AF_INET, groupAddress.c_str(),
                          &multicastAddr.sin_addr) <= 0) {
                return type::unexpected(UdpError::MulticastError);
            }

            ssize_t bytesSent =
                sendto(socket_, data.data(), data.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&multicastAddr),
                       sizeof(multicastAddr));

            if (bytesSent < 0) {
                statistics_.sendErrors++;
                return type::unexpected(UdpError::SendFailed);
            }

            // Update statistics
            statistics_.packetsSent++;
            statistics_.bytesSent += static_cast<size_t>(bytesSent);
            statistics_.lastActivity = std::chrono::system_clock::now();

            return static_cast<size_t>(bytesSent);
        } catch (...) {
            statistics_.sendErrors++;
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<bool> startReceiving(
        size_t bufferSize,
        const std::function<void(std::span<const char>, const RemoteEndpoint&)>&
            onDataCallback,
        const std::function<void(UdpError, const std::string&)>&
            onErrorCallback,
        const std::function<void(bool)>& onStatusCallback) noexcept {
        try {
            if (bufferSize == 0 || bufferSize > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!onDataCallback) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            {
                std::lock_guard<std::mutex> lock(receivingMutex_);
                if (isReceiving_) {
                    stopReceiving();
                }

                receivingStopped_ = false;
                isReceiving_ = true;

                // Notify status change
                if (onStatusCallback) {
                    onStatusCallback(true);
                }

                receivingThread_ = std::jthread(
                    [this, bufferSize, onDataCallback, onErrorCallback,
                     onStatusCallback](std::stop_token stopToken) {
                        receivingLoop(bufferSize, onDataCallback,
                                      onErrorCallback, onStatusCallback,
                                      stopToken);
                    });
            }

            return true;
        } catch (...) {
            if (onStatusCallback) {
                onStatusCallback(false);
            }
            return type::unexpected(UdpError::InternalError);
        }
    }

    void stopReceiving() noexcept {
        std::lock_guard<std::mutex> lock(receivingMutex_);
        if (isReceiving_) {
            receivingStopped_ = true;

            if (receivingThread_.joinable()) {
                receivingThread_.request_stop();
                receivingThread_.join();
            }

            isReceiving_ = false;
        }
    }

    bool isReceiving() const noexcept { return isReceiving_.load(); }

    bool isBound() const noexcept { return bound_.load(); }

    UdpResult<uint16_t> getLocalPort() const noexcept {
        try {
            if (!bound_) {
                return type::unexpected(UdpError::NotInitialized);
            }

            struct sockaddr_in addr;
            socklen_t addrLen = sizeof(addr);
            if (getsockname(socket_, reinterpret_cast<struct sockaddr*>(&addr),
                            &addrLen) != 0) {
                return type::unexpected(UdpError::InternalError);
            }

            return ntohs(addr.sin_port);
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpStatistics getStatistics() const noexcept {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return statistics_;
    }

    void resetStatistics() noexcept {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_.reset();
    }

    void close() noexcept {
        stopReceiving();

        // Leave all multicast groups
        for (const auto& group : multicastGroups_) {
            leaveMulticastGroup(group);
        }

        if (socket_ >= 0) {
            CLOSE_SOCKET(socket_);
            socket_ = -1;
        }

        bound_ = false;
    }

private:
    void receivingLoop(
        size_t bufferSize,
        const std::function<void(std::span<const char>, const RemoteEndpoint&)>&
            onDataCallback,
        const std::function<void(UdpError, const std::string&)>&
            onErrorCallback,
        const std::function<void(bool)>& onStatusCallback,
        std::stop_token stopToken) {
        std::vector<char> buffer(bufferSize);

        while (!receivingStopped_ && !stopToken.stop_requested()) {
            struct sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);

            ssize_t bytesRead =
                recvfrom(socket_, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<struct sockaddr*>(&clientAddress),
                         &clientAddressLength);

            if (bytesRead > 0) {
                try {
                    RemoteEndpoint remote;
                    remote.host = inet_ntoa(clientAddress.sin_addr);
                    remote.port = ntohs(clientAddress.sin_port);

                    // Update statistics
                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        statistics_.packetsReceived++;
                        statistics_.bytesReceived +=
                            static_cast<size_t>(bytesRead);
                        statistics_.lastActivity =
                            std::chrono::system_clock::now();
                    }

                    onDataCallback(
                        std::span<const char>{buffer.data(),
                                              static_cast<size_t>(bytesRead)},
                        remote);
                } catch (const std::exception& e) {
                    if (onErrorCallback) {
                        onErrorCallback(UdpError::InternalError,
                                        "Exception in data callback: " +
                                            std::string(e.what()));
                    }
                }
            } else if (bytesRead < 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK && error != WSAETIMEDOUT &&
                    onErrorCallback) {
                    onErrorCallback(UdpError::ReceiveFailed,
                                    "Receive error: " + getLastErrorMsg());

                    std::lock_guard<std::mutex> lock(statsMutex_);
                    statistics_.receiveErrors++;
                }
#else
                if (errno != EAGAIN && errno != EWOULDBLOCK &&
                    onErrorCallback) {
                    onErrorCallback(UdpError::ReceiveFailed,
                                    "Receive error: " + getLastErrorMsg());

                    std::lock_guard<std::mutex> lock(statsMutex_);
                    statistics_.receiveErrors++;
                }
#endif
            }

            // Small sleep to avoid busy-waiting and high CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Notify status change
        if (onStatusCallback) {
            onStatusCallback(false);
        }
    }

#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
    int epoll_fd_ = -1;
#endif
    std::atomic<bool> bound_ = false;
    std::jthread receivingThread_;
    std::atomic<bool> receivingStopped_ = false;
    std::atomic<bool> isReceiving_ = false;
    std::mutex receivingMutex_;

    UdpStatistics statistics_;
    mutable std::mutex statsMutex_;

    std::vector<std::string> multicastGroups_;
};

// UdpClient implementation
UdpClient::UdpClient() : impl_(std::make_unique<Impl>()) {}

UdpClient::UdpClient(uint16_t port) : impl_(std::make_unique<Impl>(port)) {}

UdpClient::UdpClient(uint16_t port, const SocketOptions& options)
    : impl_(std::make_unique<Impl>(port, options)) {}

UdpClient::~UdpClient() = default;

UdpClient::UdpClient(UdpClient&&) noexcept = default;
UdpClient& UdpClient::operator=(UdpClient&&) noexcept = default;

UdpResult<bool> UdpClient::bind(uint16_t port) noexcept {
    return impl_->bind(port);
}

UdpResult<size_t> UdpClient::send(const RemoteEndpoint& endpoint,
                                  std::span<const char> data) noexcept {
    return impl_->send(endpoint, data);
}

UdpResult<size_t> UdpClient::send(const RemoteEndpoint& endpoint,
                                  const std::string& data) noexcept {
    return impl_->send(endpoint,
                       std::span<const char>(data.data(), data.size()));
}

UdpResult<size_t> UdpClient::sendBroadcast(
    uint16_t port, std::span<const char> data) noexcept {
    return impl_->sendBroadcast(port, data);
}

UdpResult<size_t> UdpClient::sendBroadcast(uint16_t port,
                                           const std::string& data) noexcept {
    return impl_->sendBroadcast(
        port, std::span<const char>(data.data(), data.size()));
}

UdpResult<size_t> UdpClient::sendMultiple(
    const std::vector<RemoteEndpoint>& endpoints,
    std::span<const char> data) noexcept {
    return impl_->sendMultiple(endpoints, data);
}

UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> UdpClient::receive(
    size_t maxSize, std::chrono::milliseconds timeout) noexcept {
    return impl_->receive(maxSize, timeout);
}

UdpResult<bool> UdpClient::joinMulticastGroup(
    const std::string& groupAddress) noexcept {
    return impl_->joinMulticastGroup(groupAddress);
}

UdpResult<bool> UdpClient::leaveMulticastGroup(
    const std::string& groupAddress) noexcept {
    return impl_->leaveMulticastGroup(groupAddress);
}

UdpResult<size_t> UdpClient::sendToMulticastGroup(
    const std::string& groupAddress, uint16_t port,
    std::span<const char> data) noexcept {
    return impl_->sendToMulticastGroup(groupAddress, port, data);
}

void UdpClient::ReceiveAwaitable::await_suspend(std::coroutine_handle<> h) {
    // Execute the receive operation asynchronously
    std::thread([this, h]() {
        result_ = client.receive(maxSize, timeout);
        h.resume();
    }).detach();
}

UdpResult<std::pair<std::vector<char>, RemoteEndpoint>>
UdpClient::ReceiveAwaitable::await_resume() {
    return result_;
}

UdpResult<bool> UdpClient::startReceiving(size_t bufferSize) noexcept {
    return impl_->startReceiving(
        bufferSize,
        [this](std::span<const char> data, const RemoteEndpoint& endpoint) {
            if (onDataReceivedCallback_) {
                onDataReceivedCallback_(data, endpoint);
            }
        },
        [this](UdpError error, const std::string& message) {
            if (onErrorCallback_) {
                onErrorCallback_(error, message);
            }
        },
        [this](bool status) {
            if (onStatusChangeCallback_) {
                onStatusChangeCallback_(status);
            }
        });
}

void UdpClient::stopReceiving() noexcept { impl_->stopReceiving(); }

bool UdpClient::isReceiving() const noexcept { return impl_->isReceiving(); }

UdpStatistics UdpClient::getStatistics() const noexcept {
    return impl_->getStatistics();
}

void UdpClient::resetStatistics() noexcept { impl_->resetStatistics(); }

UdpResult<bool> UdpClient::setSocketOptions(
    const SocketOptions& options) noexcept {
    return impl_->applySocketOptions(options);
}

void UdpClient::close() noexcept { impl_->close(); }

bool UdpClient::isBound() const noexcept { return impl_->isBound(); }

UdpResult<uint16_t> UdpClient::getLocalPort() const noexcept {
    return impl_->getLocalPort();
}

bool UdpClient::isIPv6Supported() noexcept {
    // Try creating an IPv6 socket to check support
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }

    CLOSE_SOCKET(sock);
    return true;
}

}  // namespace atom::connection
