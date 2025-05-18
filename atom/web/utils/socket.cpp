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
#define close closesocket
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
#include <cstdio>
#endif

#include "atom/log/loguru.hpp"

namespace atom::web {

// ====== Windows Socket API Initialization ======

auto initializeWindowsSocketAPI() -> bool {
#ifdef _WIN32
    try {
        WSADATA wsaData;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (ret != 0) {
            throw std::runtime_error(
                std::format("WSAStartup failed with error: {}", ret));
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize Windows Socket API: {}", e.what());
        return false;
    }
#endif
    return true;
}

auto createSocket() -> int {
    try {
        int sockfd =
            static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (sockfd < 0) {
            std::array<char, 256> buf{};
#ifdef _WIN32
            strerror_s(buf.data(), buf.size(), errno);
#else
            snprintf(buf.data(), buf.size(), "%s", strerror(errno));
#endif
            throw std::runtime_error(
                std::format("Socket creation failed: {}", buf.data()));
        }
        return sockfd;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to create socket: {}", e.what());
#ifdef _WIN32
        WSACleanup();
#endif
        throw;
    }
}

auto bindSocket(int sockfd, uint16_t port) -> bool {
    try {
        struct sockaddr_in addr{};
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) != 0) {
            std::array<char, 256> buf{};
#ifdef _WIN32
            strerror_s(buf.data(), buf.size(), errno);
#else
            snprintf(buf.data(), buf.size(), "%s", strerror(errno));
#endif
            LOG_F(ERROR, "Failed to bind socket: {}", buf.data());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to bind socket: {}", e.what());
        return false;
    }
}

auto setSocketNonBlocking(int sockfd) -> bool {
    try {
#ifdef _WIN32
        unsigned long mode = 1;
        return ioctlsocket(sockfd, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to set socket non-blocking: {}", e.what());
        return false;
    }
}

auto connectWithTimeout(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen, std::chrono::milliseconds timeout)
    -> bool {
    try {
        if (!setSocketNonBlocking(sockfd)) {
            LOG_F(ERROR,
                  "Failed to set socket non-blocking for connect timeout");
            return false;
        }

        int ret = connect(sockfd, addr, addrlen);
        if (ret == 0) {
            return true;
        }

#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            return false;
        }
#endif

#ifdef _WIN32
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sockfd, &writefds);

        struct timeval tv;
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

        ret = select(sockfd + 1, nullptr, &writefds, nullptr, &tv);
#else
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
#endif

        if (ret <= 0) {
            return false;
        }

        int error = 0;
        socklen_t len = sizeof(error);
        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&error), &len);

        return ret == 0 && error == 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Connect with timeout failed: {}", e.what());
        return false;
    }
}

}  // namespace atom::web
