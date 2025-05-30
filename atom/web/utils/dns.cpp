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
#include <shared_mutex>
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

#include <spdlog/spdlog.h>
#include "addr_info.hpp"
#include "ip.hpp"


namespace atom::web {

class DNSCache {
private:
    struct CacheEntry {
        std::vector<std::string> ipAddresses;
        std::chrono::time_point<std::chrono::steady_clock> expiryTime;

        CacheEntry() = default;
        CacheEntry(std::vector<std::string> ips,
                   std::chrono::time_point<std::chrono::steady_clock> expiry)
            : ipAddresses(std::move(ips)), expiryTime(expiry) {}
    };

    mutable std::shared_mutex cacheMutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::chrono::seconds ttl_{300};

public:
    void setTTL(std::chrono::seconds newTtl) {
        std::unique_lock<std::shared_mutex> lock(cacheMutex_);
        ttl_ = newTtl;
    }

    bool get(const std::string& hostname,
             std::vector<std::string>& ipAddresses) {
        std::shared_lock<std::shared_mutex> lock(cacheMutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = cache_.find(hostname);

        if (it != cache_.end() && now < it->second.expiryTime) {
            ipAddresses = it->second.ipAddresses;
            return true;
        }
        return false;
    }

    void put(const std::string& hostname,
             const std::vector<std::string>& ipAddresses) {
        std::unique_lock<std::shared_mutex> lock(cacheMutex_);
        auto expiryTime = std::chrono::steady_clock::now() + ttl_;
        cache_.emplace(hostname, CacheEntry{ipAddresses, expiryTime});
    }

    void clearExpiredEntries() {
        std::unique_lock<std::shared_mutex> lock(cacheMutex_);
        auto now = std::chrono::steady_clock::now();

        for (auto it = cache_.begin(); it != cache_.end();) {
            if (now >= it->second.expiryTime) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(cacheMutex_);
        return cache_.size();
    }
};

static DNSCache g_dnsCache;

void setDNSCacheTTL(std::chrono::seconds ttlSeconds) {
    g_dnsCache.setTTL(ttlSeconds);
    spdlog::debug("DNS cache TTL set to {} seconds", ttlSeconds.count());
}

void clearDNSCacheExpiredEntries() {
    size_t oldSize = g_dnsCache.size();
    g_dnsCache.clearExpiredEntries();
    size_t newSize = g_dnsCache.size();

    if (oldSize > newSize) {
        spdlog::debug("Cleared {} expired DNS cache entries",
                      oldSize - newSize);
    }
}

auto getIPAddresses(const std::string& hostname) -> std::vector<std::string> {
    if (hostname.empty()) {
        spdlog::warn("Empty hostname provided to getIPAddresses");
        return {};
    }

    try {
        std::vector<std::string> results;
        if (g_dnsCache.get(hostname, results)) {
            spdlog::trace("DNS cache hit for hostname: {}", hostname);
            return results;
        }

        spdlog::debug("Resolving hostname: {}", hostname);
        auto addrInfo = getAddrInfo(hostname, "");
        if (!addrInfo) {
            spdlog::warn("Failed to resolve hostname: {}", hostname);
            return {};
        }

        results.reserve(8);

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                results.emplace_back(ipStr.data());
            }
        }

        if (!results.empty()) {
            g_dnsCache.put(hostname, results);
            spdlog::debug("Resolved {} IP addresses for hostname: {}",
                          results.size(), hostname);
        } else {
            spdlog::warn("No IP addresses found for hostname: {}", hostname);
        }

        return results;
    } catch (const std::exception& e) {
        spdlog::error("Error getting IP addresses for {}: {}", hostname,
                      e.what());
        return {};
    }
}

auto getLocalIPAddresses() -> std::vector<std::string> {
    std::vector<std::string> results;

    try {
#ifdef _WIN32
        constexpr size_t HOSTNAME_BUFFER_SIZE = 256;
        std::array<char, HOSTNAME_BUFFER_SIZE> hostname{};

        if (gethostname(hostname.data(), hostname.size()) != 0) {
            spdlog::error("Failed to get local hostname: {}",
                          WSAGetLastError());
            return results;
        }

        auto addrInfo = getAddrInfo(hostname.data(), "");
        if (!addrInfo) {
            spdlog::error("Failed to get address info for local hostname");
            return results;
        }

        results.reserve(4);

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                results.emplace_back(ipStr.data());
            }
        }

#elif defined(__linux__) || defined(__APPLE__)
        struct ifaddrs* ifAddrStruct = nullptr;
        if (getifaddrs(&ifAddrStruct) == -1) {
            spdlog::error("Failed to get interface addresses: {}",
                          strerror(errno));
            return results;
        }

        std::unique_ptr<struct ifaddrs, void (*)(struct ifaddrs*)> ifAddrPtr(
            ifAddrStruct, [](struct ifaddrs* ptr) { freeifaddrs(ptr); });

        results.reserve(8);

        for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr;
             ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }

            if (ifa->ifa_addr->sa_family == AF_INET ||
                ifa->ifa_addr->sa_family == AF_INET6) {
                std::array<char, INET6_ADDRSTRLEN> ipStr{};
                if (ipToString(ifa->ifa_addr, ipStr.data(), ipStr.size())) {
                    std::string ip(ipStr.data());
                    if (ip != "127.0.0.1" && ip != "::1") {
                        results.emplace_back(std::move(ip));
                    }
                }
            }
        }
#endif

        spdlog::debug("Found {} local IP addresses", results.size());
        return results;

    } catch (const std::exception& e) {
        spdlog::error("Error getting local IP addresses: {}", e.what());
        return results;
    }
}

}  // namespace atom::web
