/*
 * ip.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "ip.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::web {

auto isValidIPv4(const std::string& ipAddress) -> bool {
    try {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) == 1;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "IPv4 validation error: {}", e.what());
        return false;
    }
}

auto isValidIPv6(const std::string& ipAddress) -> bool {
    try {
        struct sockaddr_in6 sa;
        return inet_pton(AF_INET6, ipAddress.c_str(), &(sa.sin6_addr)) == 1;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "IPv6 validation error: {}", e.what());
        return false;
    }
}

auto ipToString(const struct sockaddr* addr, char* strBuf, size_t bufSize)
    -> bool {
    if (!addr || !strBuf || bufSize == 0) {
        return false;
    }

    const void* src = nullptr;
    if (addr->sa_family == AF_INET) {
        src = &(reinterpret_cast<const struct sockaddr_in*>(addr))->sin_addr;
    } else if (addr->sa_family == AF_INET6) {
        src = &(reinterpret_cast<const struct sockaddr_in6*>(addr))->sin6_addr;
    } else {
        return false;
    }

    return inet_ntop(addr->sa_family, src, strBuf,
                    static_cast<socklen_t>(bufSize)) != nullptr;
}

}  // namespace atom::web
