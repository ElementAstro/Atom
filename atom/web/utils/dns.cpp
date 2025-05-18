/*
 * dns.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "dns.hpp"

#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "addr_info.hpp"
#include "ip.hpp"
#include "atom/log/loguru.hpp"

namespace atom::web {

// ====== DNS Cache Implementation ======

class DNSCache {
private:
    struct CacheEntry {
        std::vector<std::string> ipAddresses;
        std::chrono::time_point<std::chrono::steady_clock> expiryTime;
    };

    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex cacheMutex;
    std::chrono::seconds ttl{300};

public:
    void setTTL(std::chrono::seconds newTtl) { ttl = newTtl; }

    bool get(const std::string& hostname,
             std::vector<std::string>& ipAddresses) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::steady_clock::now();
        auto it = cache.find(hostname);

        if (it != cache.end() && now < it->second.expiryTime) {
            ipAddresses = it->second.ipAddresses;
            return true;
        }
        return false;
    }

    void put(const std::string& hostname,
             const std::vector<std::string>& ipAddresses) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto expiryTime = std::chrono::steady_clock::now() + ttl;
        cache[hostname] = {ipAddresses, expiryTime};
    }

    void clearExpiredEntries() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache.begin(); it != cache.end();) {
            if (now >= it->second.expiryTime) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }
};

static DNSCache g_dnsCache;

void setDNSCacheTTL(std::chrono::seconds ttlSeconds) {
    g_dnsCache.setTTL(ttlSeconds);
}

void clearDNSCacheExpiredEntries() {
    g_dnsCache.clearExpiredEntries();
}

auto getIPAddresses(const std::string& hostname) -> std::vector<std::string> {
    try {
        std::vector<std::string> results;
        if (g_dnsCache.get(hostname, results)) {
            return results;
        }

        auto addrInfo = getAddrInfo(hostname, "");
        if (!addrInfo) {
            return {};
        }

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                results.push_back(ipStr.data());
            }
        }

        if (!results.empty()) {
            g_dnsCache.put(hostname, results);
        }

        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting IP addresses for {}: {}", hostname,
              e.what());
        return {};
    }
}

auto getLocalIPAddresses() -> std::vector<std::string> {
    std::vector<std::string> results;
    try {
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            return results;
        }

        auto addrInfo = getAddrInfo(hostname, "");
        if (!addrInfo) {
            return results;
        }

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                results.push_back(ipStr.data());
            }
        }

#elif defined(__linux__) || defined(__APPLE__)
        struct ifaddrs* ifAddrStruct = nullptr;
        if (getifaddrs(&ifAddrStruct) == -1) {
            return results;
        }

        auto cleanup = [&ifAddrStruct](void*) {
            freeifaddrs(ifAddrStruct);
        };
        std::unique_ptr<void, decltype(cleanup)> cleanupGuard(nullptr, cleanup);

        for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr;
             ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            
            if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) {
                std::array<char, INET6_ADDRSTRLEN> ipStr{};
                if (ipToString(ifa->ifa_addr, ipStr.data(), ipStr.size())) {
                    results.push_back(ipStr.data());
                }
            }
        }
#endif
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting local IP addresses: {}", e.what());
        return results;
    }
}

}  // namespace atom::web
