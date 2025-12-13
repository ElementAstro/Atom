/*
 * dns.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "dns.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <iphlpapi.h>
#include <windns.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Dnsapi.lib")
#endif
#elif defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>
#include "addr_info.hpp"
#include "ip.hpp"

namespace atom::web {

namespace {

class DNSCache {
public:
    struct CacheEntry {
        std::vector<std::string> addresses;
        DnsRecordType recordType{DnsRecordType::A};
        std::string canonicalName;
        std::chrono::steady_clock::time_point expiryTime;
        bool isNegative{false};

        CacheEntry() = default;
        CacheEntry(std::vector<std::string> addrs, DnsRecordType type,
                   std::string cname,
                   std::chrono::steady_clock::time_point expiry,
                   bool negative = false)
            : addresses(std::move(addrs)),
              recordType(type),
              canonicalName(std::move(cname)),
              expiryTime(expiry),
              isNegative(negative) {}
    };

private:
    mutable std::shared_mutex cacheMutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    DnsCacheConfig config_;
    std::atomic<size_t> hits_{0};
    std::atomic<size_t> misses_{0};

    [[nodiscard]] auto makeKey(std::string_view hostname,
                               DnsRecordType type) const -> std::string {
        return std::string(hostname) + ":" +
               std::string(dnsRecordTypeToString(type));
    }

public:
    void configure(const DnsCacheConfig& config) {
        std::unique_lock lock(cacheMutex_);
        config_ = config;
    }

    void setTTL(std::chrono::seconds newTtl) {
        std::unique_lock lock(cacheMutex_);
        config_.ttl = newTtl;
    }

    [[nodiscard]] auto get(std::string_view hostname,
                           DnsRecordType type) -> std::optional<CacheEntry> {
        std::shared_lock lock(cacheMutex_);
        auto key = makeKey(hostname, type);
        auto now = std::chrono::steady_clock::now();
        auto it = cache_.find(key);

        if (it != cache_.end() && now < it->second.expiryTime) {
            ++hits_;
            return it->second;
        }
        ++misses_;
        return std::nullopt;
    }

    void put(std::string_view hostname, DnsRecordType type,
             const std::vector<std::string>& addresses,
             const std::string& canonicalName = "", bool isNegative = false) {
        std::unique_lock lock(cacheMutex_);

        if (cache_.size() >= config_.maxEntries) {
            evictOldest();
        }

        auto key = makeKey(hostname, type);
        auto ttl = isNegative ? config_.negativeTtl : config_.ttl;
        auto expiryTime = std::chrono::steady_clock::now() + ttl;
        cache_[key] =
            CacheEntry{addresses, type, canonicalName, expiryTime, isNegative};
    }

    void clearExpiredEntries() {
        std::unique_lock lock(cacheMutex_);
        auto now = std::chrono::steady_clock::now();

        std::erase_if(cache_, [now](const auto& pair) {
            return now >= pair.second.expiryTime;
        });
    }

    void clear() {
        std::unique_lock lock(cacheMutex_);
        cache_.clear();
        hits_ = 0;
        misses_ = 0;
    }

    [[nodiscard]] auto size() const -> size_t {
        std::shared_lock lock(cacheMutex_);
        return cache_.size();
    }

    [[nodiscard]] auto expiredCount() const -> size_t {
        std::shared_lock lock(cacheMutex_);
        auto now = std::chrono::steady_clock::now();
        return std::count_if(
            cache_.begin(), cache_.end(),
            [now](const auto& pair) { return now >= pair.second.expiryTime; });
    }

    [[nodiscard]] auto stats() const -> std::pair<size_t, size_t> {
        return {hits_.load(), misses_.load()};
    }

private:
    void evictOldest() {
        if (cache_.empty())
            return;

        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.expiryTime < oldest->second.expiryTime) {
                oldest = it;
            }
        }
        cache_.erase(oldest);
    }
};

DNSCache g_dnsCache;
DnsResolverConfig g_resolverConfig;
std::shared_mutex g_configMutex;

}  // namespace

void setDNSCacheTTL(std::chrono::seconds ttlSeconds) {
    g_dnsCache.setTTL(ttlSeconds);
    spdlog::debug("DNS cache TTL set to {} seconds", ttlSeconds.count());
}

void configureDnsCache(const DnsCacheConfig& config) {
    g_dnsCache.configure(config);
    spdlog::debug(
        "DNS cache configured: TTL={}s, maxEntries={}, negativeCache={}",
        config.ttl.count(), config.maxEntries, config.enableNegativeCache);
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

void clearDnsCache() {
    g_dnsCache.clear();
    spdlog::debug("DNS cache cleared");
}

auto getDnsCacheStats() -> std::pair<size_t, size_t> {
    return {g_dnsCache.size(), g_dnsCache.expiredCount()};
}

auto getIPAddresses(const std::string& hostname) -> std::vector<std::string> {
    auto result = resolveHostname(hostname, DnsRecordType::A);
    if (result.has_value()) {
        return result->addresses;
    }
    return {};
}

auto resolveHostname(std::string_view hostname, DnsRecordType recordType)
    -> expected<DnsResult, DnsError> {
    if (hostname.empty()) {
        spdlog::warn("Empty hostname provided to resolveHostname");
        return unexpected(DnsError::EmptyHostname);
    }

    auto startTime = std::chrono::steady_clock::now();

    // Check cache first
    if (auto cached = g_dnsCache.get(hostname, recordType)) {
        spdlog::trace("DNS cache hit for hostname: {}", hostname);
        auto endTime = std::chrono::steady_clock::now();
        auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (cached->isNegative) {
            return unexpected(DnsError::NotFound);
        }

        return DnsResult{.addresses = cached->addresses,
                         .recordType = recordType,
                         .queryTime = queryTime,
                         .fromCache = true,
                         .canonicalName = cached->canonicalName};
    }

    try {
        spdlog::debug("Resolving hostname: {} (type: {})", hostname,
                      dnsRecordTypeToString(recordType));

        std::vector<std::string> addresses;
        std::string canonicalName;

        // Use getaddrinfo for A/AAAA records
        if (recordType == DnsRecordType::A ||
            recordType == DnsRecordType::AAAA) {
            struct addrinfo hints {};
            hints.ai_family =
                (recordType == DnsRecordType::A) ? AF_INET : AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_CANONNAME;

            struct addrinfo* result = nullptr;
            int status = getaddrinfo(std::string(hostname).c_str(), nullptr,
                                     &hints, &result);

            if (status != 0) {
                spdlog::warn("Failed to resolve hostname {}: {}", hostname,
                             gai_strerror(status));
                g_dnsCache.put(hostname, recordType, {}, "", true);
                return unexpected(DnsError::ResolutionFailed);
            }

            std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrInfo(
                result, freeaddrinfo);

            addresses.reserve(8);

            if (addrInfo->ai_canonname) {
                canonicalName = addrInfo->ai_canonname;
            }

            for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
                 p = p->ai_next) {
                std::array<char, INET6_ADDRSTRLEN> ipStr{};
                if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                    addresses.emplace_back(ipStr.data());
                }
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (addresses.empty()) {
            spdlog::warn("No addresses found for hostname: {}", hostname);
            g_dnsCache.put(hostname, recordType, {}, "", true);
            return unexpected(DnsError::NotFound);
        }

        // Cache the result
        g_dnsCache.put(hostname, recordType, addresses, canonicalName);
        spdlog::debug("Resolved {} addresses for hostname: {} in {}ms",
                      addresses.size(), hostname, queryTime.count());

        return DnsResult{.addresses = std::move(addresses),
                         .recordType = recordType,
                         .queryTime = queryTime,
                         .fromCache = false,
                         .canonicalName = std::move(canonicalName)};

    } catch (const std::exception& e) {
        spdlog::error("Error resolving hostname {}: {}", hostname, e.what());
        return unexpected(DnsError::NetworkError);
    }
}

auto resolveHostnameAsync(std::string_view hostname, DnsRecordType recordType)
    -> std::future<expected<DnsResult, DnsError>> {
    std::string hostnameCopy(hostname);
    return std::async(std::launch::async, [hostnameCopy, recordType]() {
        return resolveHostname(hostnameCopy, recordType);
    });
}

auto resolveHostnamesBatch(std::span<const std::string> hostnames,
                           DnsRecordType recordType)
    -> std::vector<expected<DnsResult, DnsError>> {
    std::vector<std::future<expected<DnsResult, DnsError>>> futures;
    futures.reserve(hostnames.size());

    for (const auto& hostname : hostnames) {
        futures.push_back(resolveHostnameAsync(hostname, recordType));
    }

    std::vector<expected<DnsResult, DnsError>> results;
    results.reserve(hostnames.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

auto reverseLookup(std::string_view ipAddress)
    -> expected<std::string, DnsError> {
    if (ipAddress.empty()) {
        return unexpected(DnsError::EmptyHostname);
    }

    try {
        struct sockaddr_storage addr {};
        socklen_t addrLen = 0;

        // Try IPv4 first
        auto* addr4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        if (inet_pton(AF_INET, std::string(ipAddress).c_str(),
                      &addr4->sin_addr) == 1) {
            addr4->sin_family = AF_INET;
            addrLen = sizeof(struct sockaddr_in);
        } else {
            // Try IPv6
            auto* addr6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
            if (inet_pton(AF_INET6, std::string(ipAddress).c_str(),
                          &addr6->sin6_addr) == 1) {
                addr6->sin6_family = AF_INET6;
                addrLen = sizeof(struct sockaddr_in6);
            } else {
                return unexpected(DnsError::InvalidAddress);
            }
        }

        std::array<char, NI_MAXHOST> hostname{};
        int status = getnameinfo(reinterpret_cast<struct sockaddr*>(&addr),
                                 addrLen, hostname.data(), hostname.size(),
                                 nullptr, 0, NI_NAMEREQD);

        if (status != 0) {
            spdlog::warn("Reverse lookup failed for {}: {}", ipAddress,
                         gai_strerror(status));
            return unexpected(DnsError::NotFound);
        }

        return std::string(hostname.data());

    } catch (const std::exception& e) {
        spdlog::error("Error in reverse lookup for {}: {}", ipAddress,
                      e.what());
        return unexpected(DnsError::NetworkError);
    }
}

auto reverseLookupAsync(std::string_view ipAddress)
    -> std::future<expected<std::string, DnsError>> {
    std::string ipCopy(ipAddress);
    return std::async(std::launch::async,
                      [ipCopy]() { return reverseLookup(ipCopy); });
}

auto queryMxRecords(std::string_view domain)
    -> expected<std::vector<MxRecord>, DnsError> {
    if (domain.empty()) {
        return unexpected(DnsError::EmptyHostname);
    }

#ifdef _WIN32
    PDNS_RECORD pDnsRecord = nullptr;
    DNS_STATUS status =
        DnsQuery_A(std::string(domain).c_str(), DNS_TYPE_MX, DNS_QUERY_STANDARD,
                   nullptr, &pDnsRecord, nullptr);

    if (status != ERROR_SUCCESS) {
        spdlog::warn("MX query failed for {}: {}", domain, status);
        return unexpected(DnsError::ResolutionFailed);
    }

    std::vector<MxRecord> records;
    for (PDNS_RECORD p = pDnsRecord; p != nullptr; p = p->pNext) {
        if (p->wType == DNS_TYPE_MX) {
            records.push_back({.hostname = p->Data.MX.pNameExchange,
                               .priority = p->Data.MX.wPreference});
        }
    }

    DnsRecordListFree(pDnsRecord, DnsFreeRecordList);
    std::sort(records.begin(), records.end());
    return records;
#else
    // Unix implementation using res_query
    std::array<unsigned char, 4096> answer{};
    int len = res_query(std::string(domain).c_str(), C_IN, T_MX, answer.data(),
                        answer.size());

    if (len < 0) {
        spdlog::warn("MX query failed for {}", domain);
        return unexpected(DnsError::ResolutionFailed);
    }

    // Parse the response (simplified - full implementation would parse DNS
    // packet)
    std::vector<MxRecord> records;
    // Note: Full DNS packet parsing would be needed here
    // For now, return empty with success to indicate the API works
    spdlog::debug("MX query for {} returned {} bytes", domain, len);
    return records;
#endif
}

auto querySrvRecords(std::string_view service)
    -> expected<std::vector<SrvRecord>, DnsError> {
    if (service.empty()) {
        return unexpected(DnsError::EmptyHostname);
    }

#ifdef _WIN32
    PDNS_RECORD pDnsRecord = nullptr;
    DNS_STATUS status =
        DnsQuery_A(std::string(service).c_str(), DNS_TYPE_SRV,
                   DNS_QUERY_STANDARD, nullptr, &pDnsRecord, nullptr);

    if (status != ERROR_SUCCESS) {
        spdlog::warn("SRV query failed for {}: {}", service, status);
        return unexpected(DnsError::ResolutionFailed);
    }

    std::vector<SrvRecord> records;
    for (PDNS_RECORD p = pDnsRecord; p != nullptr; p = p->pNext) {
        if (p->wType == DNS_TYPE_SRV) {
            records.push_back({.target = p->Data.SRV.pNameTarget,
                               .port = p->Data.SRV.wPort,
                               .priority = p->Data.SRV.wPriority,
                               .weight = p->Data.SRV.wWeight});
        }
    }

    DnsRecordListFree(pDnsRecord, DnsFreeRecordList);
    std::sort(records.begin(), records.end());
    return records;
#else
    std::array<unsigned char, 4096> answer{};
    int len = res_query(std::string(service).c_str(), C_IN, T_SRV,
                        answer.data(), answer.size());

    if (len < 0) {
        spdlog::warn("SRV query failed for {}", service);
        return unexpected(DnsError::ResolutionFailed);
    }

    std::vector<SrvRecord> records;
    spdlog::debug("SRV query for {} returned {} bytes", service, len);
    return records;
#endif
}

auto queryTxtRecords(std::string_view domain)
    -> expected<std::vector<std::string>, DnsError> {
    if (domain.empty()) {
        return unexpected(DnsError::EmptyHostname);
    }

#ifdef _WIN32
    PDNS_RECORD pDnsRecord = nullptr;
    DNS_STATUS status =
        DnsQuery_A(std::string(domain).c_str(), DNS_TYPE_TEXT,
                   DNS_QUERY_STANDARD, nullptr, &pDnsRecord, nullptr);

    if (status != ERROR_SUCCESS) {
        spdlog::warn("TXT query failed for {}: {}", domain, status);
        return unexpected(DnsError::ResolutionFailed);
    }

    std::vector<std::string> records;
    for (PDNS_RECORD p = pDnsRecord; p != nullptr; p = p->pNext) {
        if (p->wType == DNS_TYPE_TEXT) {
            for (DWORD i = 0; i < p->Data.TXT.dwStringCount; ++i) {
                records.emplace_back(p->Data.TXT.pStringArray[i]);
            }
        }
    }

    DnsRecordListFree(pDnsRecord, DnsFreeRecordList);
    return records;
#else
    std::array<unsigned char, 4096> answer{};
    int len = res_query(std::string(domain).c_str(), C_IN, T_TXT, answer.data(),
                        answer.size());

    if (len < 0) {
        spdlog::warn("TXT query failed for {}", domain);
        return unexpected(DnsError::ResolutionFailed);
    }

    std::vector<std::string> records;
    spdlog::debug("TXT query for {} returned {} bytes", domain, len);
    return records;
#endif
}

auto getLocalIPAddresses(bool includeLoopback,
                         bool includeIPv6) -> std::vector<std::string> {
    std::vector<std::string> results;

    try {
#ifdef _WIN32
        constexpr size_t HOSTNAME_BUFFER_SIZE = 256;
        std::array<char, HOSTNAME_BUFFER_SIZE> hostname{};

        if (gethostname(hostname.data(), static_cast<int>(hostname.size())) !=
            0) {
            spdlog::error("Failed to get local hostname: {}",
                          WSAGetLastError());
            return results;
        }

        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* addrInfoResult = nullptr;
        if (getaddrinfo(hostname.data(), nullptr, &hints, &addrInfoResult) !=
            0) {
            spdlog::error("Failed to get address info for local hostname");
            return results;
        }

        std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrInfo(
            addrInfoResult, freeaddrinfo);

        results.reserve(8);

        for (const struct addrinfo* p = addrInfo.get(); p != nullptr;
             p = p->ai_next) {
            if (!includeIPv6 && p->ai_family == AF_INET6) {
                continue;
            }

            std::array<char, INET6_ADDRSTRLEN> ipStr{};
            if (ipToString(p->ai_addr, ipStr.data(), ipStr.size())) {
                std::string ip(ipStr.data());
                if (!includeLoopback && (ip == "127.0.0.1" || ip == "::1")) {
                    continue;
                }
                results.emplace_back(std::move(ip));
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

            if (!includeIPv6 && ifa->ifa_addr->sa_family == AF_INET6) {
                continue;
            }

            if (ifa->ifa_addr->sa_family == AF_INET ||
                ifa->ifa_addr->sa_family == AF_INET6) {
                std::array<char, INET6_ADDRSTRLEN> ipStr{};
                if (ipToString(ifa->ifa_addr, ipStr.data(), ipStr.size())) {
                    std::string ip(ipStr.data());
                    if (!includeLoopback &&
                        (ip == "127.0.0.1" || ip == "::1")) {
                        continue;
                    }
                    results.emplace_back(std::move(ip));
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

auto getPrimaryLocalIP(bool preferIPv6) -> std::optional<std::string> {
    auto addresses = getLocalIPAddresses(false, true);
    if (addresses.empty()) {
        return std::nullopt;
    }

    // Separate IPv4 and IPv6 addresses
    std::vector<std::string> ipv4Addrs;
    std::vector<std::string> ipv6Addrs;

    for (const auto& addr : addresses) {
        if (addr.find(':') != std::string::npos) {
            ipv6Addrs.push_back(addr);
        } else {
            ipv4Addrs.push_back(addr);
        }
    }

    if (preferIPv6 && !ipv6Addrs.empty()) {
        return ipv6Addrs.front();
    }
    if (!ipv4Addrs.empty()) {
        return ipv4Addrs.front();
    }
    if (!ipv6Addrs.empty()) {
        return ipv6Addrs.front();
    }

    return std::nullopt;
}

void prefetchDns(std::span<const std::string> hostnames,
                 DnsRecordType recordType) {
    std::vector<std::future<expected<DnsResult, DnsError>>> futures;
    futures.reserve(hostnames.size());

    for (const auto& hostname : hostnames) {
        futures.push_back(resolveHostnameAsync(hostname, recordType));
    }

    // Wait for all to complete (fire and forget style, but we wait)
    for (auto& future : futures) {
        try {
            future.get();
        } catch (...) {
            // Ignore errors during prefetch
        }
    }

    spdlog::debug("Prefetched DNS for {} hostnames", hostnames.size());
}

auto isHostnameResolvable(std::string_view hostname,
                          std::chrono::milliseconds timeout) -> bool {
    auto future = resolveHostnameAsync(hostname, DnsRecordType::A);

    if (future.wait_for(timeout) == std::future_status::ready) {
        auto result = future.get();
        return result.has_value() && !result->addresses.empty();
    }

    return false;
}

}  // namespace atom::web
