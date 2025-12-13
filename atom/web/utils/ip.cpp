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

auto ipToString(const struct sockaddr* addr, char* strBuf,
                size_t bufSize) -> bool {
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

auto parseCIDR(std::string_view cidr)
    -> std::optional<std::pair<std::string, int>> {
    auto slashPos = cidr.find('/');
    if (slashPos == std::string_view::npos) {
        return std::nullopt;
    }

    std::string ip(cidr.substr(0, slashPos));
    std::string prefixStr(cidr.substr(slashPos + 1));

    int prefix = 0;
    auto [ptr, ec] = std::from_chars(
        prefixStr.data(), prefixStr.data() + prefixStr.size(), prefix);
    if (ec != std::errc{}) {
        return std::nullopt;
    }

    if (isValidIPv4(ip)) {
        if (prefix < 0 || prefix > 32)
            return std::nullopt;
    } else if (isValidIPv6(ip)) {
        if (prefix < 0 || prefix > 128)
            return std::nullopt;
    } else {
        return std::nullopt;
    }

    return std::make_pair(ip, prefix);
}

auto isIPInCIDR(std::string_view ip, std::string_view cidr) -> bool {
    auto parsed = parseCIDR(cidr);
    if (!parsed)
        return false;

    const auto& [networkIp, prefix] = *parsed;

    if (isValidIPv4(ip) && isValidIPv4(networkIp)) {
        struct in_addr ipAddr {
        }, netAddr{};
        if (inet_pton(AF_INET, std::string(ip).c_str(), &ipAddr) != 1 ||
            inet_pton(AF_INET, networkIp.c_str(), &netAddr) != 1) {
            return false;
        }

        uint32_t mask = prefix == 0 ? 0 : (~0U << (32 - prefix));
        mask = htonl(mask);

        return (ipAddr.s_addr & mask) == (netAddr.s_addr & mask);
    }

    // IPv6 CIDR check would be more complex
    return false;
}

auto isPrivateIP(std::string_view ip) -> bool {
    if (!isValidIPv4(ip))
        return false;

    struct in_addr addr {};
    if (inet_pton(AF_INET, std::string(ip).c_str(), &addr) != 1) {
        return false;
    }

    uint32_t hostOrder = ntohl(addr.s_addr);
    uint8_t first = (hostOrder >> 24) & 0xFF;
    uint8_t second = (hostOrder >> 16) & 0xFF;

    // 10.0.0.0/8
    if (first == 10)
        return true;
    // 172.16.0.0/12
    if (first == 172 && second >= 16 && second <= 31)
        return true;
    // 192.168.0.0/16
    if (first == 192 && second == 168)
        return true;

    return false;
}

auto isLoopbackIP(std::string_view ip) -> bool {
    if (isValidIPv4(ip)) {
        struct in_addr addr {};
        if (inet_pton(AF_INET, std::string(ip).c_str(), &addr) != 1) {
            return false;
        }
        uint8_t first = (ntohl(addr.s_addr) >> 24) & 0xFF;
        return first == 127;
    }

    if (isValidIPv6(ip)) {
        return ip == "::1" || ip == "0000:0000:0000:0000:0000:0000:0000:0001";
    }

    return false;
}

auto isMulticastIP(std::string_view ip) -> bool {
    if (isValidIPv4(ip)) {
        struct in_addr addr {};
        if (inet_pton(AF_INET, std::string(ip).c_str(), &addr) != 1) {
            return false;
        }
        uint8_t first = (ntohl(addr.s_addr) >> 24) & 0xFF;
        return first >= 224 && first <= 239;
    }

    if (isValidIPv6(ip)) {
        return ip.starts_with("ff") || ip.starts_with("FF");
    }

    return false;
}

auto getLocalIPAddresses(bool includeLoopback) -> std::vector<std::string> {
    std::vector<std::string> addresses;

#ifdef _WIN32
    // Windows implementation using GetAdaptersAddresses
    // Simplified - would need full implementation
    if (includeLoopback) {
        addresses.push_back("127.0.0.1");
    }
#else
    // Unix implementation using getifaddrs
    // Simplified - would need full implementation
    if (includeLoopback) {
        addresses.push_back("127.0.0.1");
    }
#endif

    return addresses;
}

auto normalizeIP(std::string_view ip) -> std::string {
    if (isValidIPv4(ip)) {
        struct in_addr addr {};
        if (inet_pton(AF_INET, std::string(ip).c_str(), &addr) == 1) {
            char buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
                return buf;
            }
        }
    }

    if (isValidIPv6(ip)) {
        struct in6_addr addr {};
        if (inet_pton(AF_INET6, std::string(ip).c_str(), &addr) == 1) {
            char buf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &addr, buf, sizeof(buf))) {
                return buf;
            }
        }
    }

    return std::string(ip);
}

auto compareIP(std::string_view ip1, std::string_view ip2) -> int {
    bool isV4_1 = isValidIPv4(ip1);
    bool isV4_2 = isValidIPv4(ip2);

    if (isV4_1 && isV4_2) {
        struct in_addr addr1 {
        }, addr2{};
        inet_pton(AF_INET, std::string(ip1).c_str(), &addr1);
        inet_pton(AF_INET, std::string(ip2).c_str(), &addr2);

        uint32_t h1 = ntohl(addr1.s_addr);
        uint32_t h2 = ntohl(addr2.s_addr);

        if (h1 < h2)
            return -1;
        if (h1 > h2)
            return 1;
        return 0;
    }

    bool isV6_1 = isValidIPv6(ip1);
    bool isV6_2 = isValidIPv6(ip2);

    if (isV6_1 && isV6_2) {
        struct in6_addr addr1 {
        }, addr2{};
        inet_pton(AF_INET6, std::string(ip1).c_str(), &addr1);
        inet_pton(AF_INET6, std::string(ip2).c_str(), &addr2);

        return std::memcmp(&addr1, &addr2, sizeof(addr1));
    }

    // Different types - IPv4 < IPv6
    if (isV4_1 && isV6_2)
        return -1;
    if (isV6_1 && isV4_2)
        return 1;

    return 0;
}

}  // namespace atom::web
