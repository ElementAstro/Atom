/*
 * socket.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "socket.hpp"

#include <array>
#include <cstring>
#include <format>
#include <stdexcept>

#ifdef _WIN32
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
    strerror_s(buffer.data(), buffer.size(), error);
    return std::format("Error {}: {}", error, buffer.data());
#else
    int error = errno;
    strerror_r(error, buffer.data(), buffer.size());
    return std::format("Error {}: {}", error, buffer.data());
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

        struct sockaddr_in addr{};
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
                        socklen_t addrlen, std::chrono::milliseconds timeout)
    -> bool {
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

}  // namespace atom::web
