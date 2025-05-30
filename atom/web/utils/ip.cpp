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

#include <spdlog/spdlog.h>

namespace atom::web {

auto isValidIPv4(const std::string& ipAddress) -> bool {
    if (ipAddress.empty() || ipAddress.length() > 15) {
        return false;
    }

    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));

    if (result != 1) {
        spdlog::trace("Invalid IPv4 address format: {}", ipAddress);
        return false;
    }

    return true;
}

auto isValidIPv6(const std::string& ipAddress) -> bool {
    if (ipAddress.empty() || ipAddress.length() > 45) {
        return false;
    }

    struct sockaddr_in6 sa;
    int result = inet_pton(AF_INET6, ipAddress.c_str(), &(sa.sin6_addr));

    if (result != 1) {
        spdlog::trace("Invalid IPv6 address format: {}", ipAddress);
        return false;
    }

    return true;
}

auto ipToString(const struct sockaddr* addr, char* strBuf, size_t bufSize)
    -> bool {
    if (!addr || !strBuf || bufSize == 0) {
        spdlog::debug("Invalid parameters passed to ipToString");
        return false;
    }

    const void* src = nullptr;
    int family = addr->sa_family;

    switch (family) {
        case AF_INET: {
            if (bufSize < INET_ADDRSTRLEN) {
                spdlog::warn("Buffer too small for IPv4 address conversion");
                return false;
            }
            src =
                &(reinterpret_cast<const struct sockaddr_in*>(addr))->sin_addr;
            break;
        }
        case AF_INET6: {
            if (bufSize < INET6_ADDRSTRLEN) {
                spdlog::warn("Buffer too small for IPv6 address conversion");
                return false;
            }
            src = &(reinterpret_cast<const struct sockaddr_in6*>(addr))
                       ->sin6_addr;
            break;
        }
        default:
            spdlog::debug("Unsupported address family: {}", family);
            return false;
    }

    const char* result =
        inet_ntop(family, src, strBuf, static_cast<socklen_t>(bufSize));

    if (!result) {
        spdlog::error("Failed to convert IP address to string, errno: {}",
                      errno);
        return false;
    }

    return true;
}

}  // namespace atom::web
