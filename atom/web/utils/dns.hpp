/*
 * dns.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: DNS related utilities with modern C++20 features

**************************************************/

#ifndef ATOM_WEB_UTILS_DNS_HPP
#define ATOM_WEB_UTILS_DNS_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "atom/type/compat.hpp"

namespace atom::web {

// Use compatibility expected type for cross-compiler support
template <typename T, typename E>
using expected = atom::type::compat::expected<T, E>;
template <typename E>
using unexpected = atom::type::compat::unexpected<E>;

/**
 * @brief DNS record types enumeration
 */
enum class DnsRecordType : uint8_t {
    A = 1,      ///< IPv4 address record
    AAAA = 28,  ///< IPv6 address record
    CNAME = 5,  ///< Canonical name record
    MX = 15,    ///< Mail exchange record
    TXT = 16,   ///< Text record
    NS = 2,     ///< Name server record
    PTR = 12,   ///< Pointer record (reverse DNS)
    SOA = 6,    ///< Start of authority record
    SRV = 33,   ///< Service record
    ANY = 255   ///< Any record type
};

/**
 * @brief Convert DnsRecordType to string representation
 * @param type The DNS record type
 * @return String representation
 */
[[nodiscard]] constexpr auto dnsRecordTypeToString(DnsRecordType type) noexcept
    -> std::string_view {
    switch (type) {
        case DnsRecordType::A:
            return "A";
        case DnsRecordType::AAAA:
            return "AAAA";
        case DnsRecordType::CNAME:
            return "CNAME";
        case DnsRecordType::MX:
            return "MX";
        case DnsRecordType::TXT:
            return "TXT";
        case DnsRecordType::NS:
            return "NS";
        case DnsRecordType::PTR:
            return "PTR";
        case DnsRecordType::SOA:
            return "SOA";
        case DnsRecordType::SRV:
            return "SRV";
        case DnsRecordType::ANY:
            return "ANY";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief DNS error codes
 */
enum class DnsError {
    Success = 0,
    EmptyHostname,
    ResolutionFailed,
    Timeout,
    NetworkError,
    InvalidAddress,
    CacheError,
    NotFound,
    ServerFailure,
    InvalidQuery
};

/**
 * @brief Get error message for DNS error code
 * @param error The DNS error code
 * @return Error message string
 */
[[nodiscard]] constexpr auto dnsErrorToString(DnsError error) noexcept
    -> std::string_view {
    switch (error) {
        case DnsError::Success:
            return "Success";
        case DnsError::EmptyHostname:
            return "Empty hostname provided";
        case DnsError::ResolutionFailed:
            return "DNS resolution failed";
        case DnsError::Timeout:
            return "DNS query timed out";
        case DnsError::NetworkError:
            return "Network error occurred";
        case DnsError::InvalidAddress:
            return "Invalid address format";
        case DnsError::CacheError:
            return "DNS cache error";
        case DnsError::NotFound:
            return "Host not found";
        case DnsError::ServerFailure:
            return "DNS server failure";
        case DnsError::InvalidQuery:
            return "Invalid DNS query";
        default:
            return "Unknown error";
    }
}

/**
 * @brief DNS resolution result structure
 */
struct DnsResult {
    std::vector<std::string> addresses;          ///< Resolved IP addresses
    DnsRecordType recordType{DnsRecordType::A};  ///< Record type queried
    std::chrono::milliseconds queryTime{0};      ///< Time taken for query
    bool fromCache{false};      ///< Whether result was from cache
    std::string canonicalName;  ///< Canonical name if available

    [[nodiscard]] auto empty() const noexcept -> bool {
        return addresses.empty();
    }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return addresses.size();
    }

    [[nodiscard]] auto begin() const noexcept { return addresses.begin(); }
    [[nodiscard]] auto end() const noexcept { return addresses.end(); }
};

/**
 * @brief MX record structure
 */
struct MxRecord {
    std::string hostname;  ///< Mail server hostname
    uint16_t priority{0};  ///< Priority (lower is higher priority)

    [[nodiscard]] auto operator<=>(const MxRecord& other) const noexcept {
        return priority <=> other.priority;
    }
};

/**
 * @brief SRV record structure
 */
struct SrvRecord {
    std::string target;    ///< Target hostname
    uint16_t port{0};      ///< Port number
    uint16_t priority{0};  ///< Priority
    uint16_t weight{0};    ///< Weight for load balancing

    [[nodiscard]] auto operator<=>(const SrvRecord& other) const noexcept {
        if (priority != other.priority)
            return priority <=> other.priority;
        return other.weight <=> weight;  // Higher weight first
    }
};

/**
 * @brief DNS cache configuration
 */
struct DnsCacheConfig {
    std::chrono::seconds ttl{300};         ///< Time-to-live for cache entries
    size_t maxEntries{1000};               ///< Maximum cache entries
    bool enableNegativeCache{true};        ///< Cache failed lookups
    std::chrono::seconds negativeTtl{60};  ///< TTL for negative cache
};

/**
 * @brief DNS resolver configuration
 */
struct DnsResolverConfig {
    std::chrono::milliseconds timeout{5000};  ///< Query timeout
    size_t maxRetries{3};                     ///< Maximum retry attempts
    bool preferIPv6{false};                   ///< Prefer IPv6 addresses
    bool useCache{true};                      ///< Enable DNS caching
    std::vector<std::string> nameservers;     ///< Custom nameservers (optional)
};

/**
 * @brief Set the Time-To-Live for DNS cache entries
 * @param ttlSeconds The TTL in seconds
 */
void setDNSCacheTTL(std::chrono::seconds ttlSeconds);

/**
 * @brief Configure DNS cache with detailed options
 * @param config Cache configuration
 */
void configureDnsCache(const DnsCacheConfig& config);

/**
 * @brief Get IP addresses for a given hostname through DNS resolution
 * @param hostname The hostname to resolve
 * @return std::vector<std::string> List of IP addresses
 */
[[nodiscard]] auto getIPAddresses(const std::string& hostname)
    -> std::vector<std::string>;

/**
 * @brief Resolve hostname with detailed result information
 * @param hostname The hostname to resolve
 * @param recordType The DNS record type to query
 * @return Expected containing DnsResult or DnsError
 */
[[nodiscard]] auto resolveHostname(std::string_view hostname,
                                   DnsRecordType recordType = DnsRecordType::A)
    -> expected<DnsResult, DnsError>;

/**
 * @brief Resolve hostname asynchronously
 * @param hostname The hostname to resolve
 * @param recordType The DNS record type to query
 * @return Future containing the resolution result
 */
[[nodiscard]] auto resolveHostnameAsync(
    std::string_view hostname, DnsRecordType recordType = DnsRecordType::A)
    -> std::future<expected<DnsResult, DnsError>>;

/**
 * @brief Resolve multiple hostnames concurrently
 * @param hostnames Span of hostnames to resolve
 * @param recordType The DNS record type to query
 * @return Vector of results for each hostname
 */
[[nodiscard]] auto resolveHostnamesBatch(
    std::span<const std::string> hostnames,
    DnsRecordType recordType = DnsRecordType::A)
    -> std::vector<expected<DnsResult, DnsError>>;

/**
 * @brief Perform reverse DNS lookup (PTR record)
 * @param ipAddress The IP address to look up
 * @return Expected containing hostname or DnsError
 */
[[nodiscard]] auto reverseLookup(std::string_view ipAddress)
    -> expected<std::string, DnsError>;

/**
 * @brief Perform reverse DNS lookup asynchronously
 * @param ipAddress The IP address to look up
 * @return Future containing the lookup result
 */
[[nodiscard]] auto reverseLookupAsync(std::string_view ipAddress)
    -> std::future<expected<std::string, DnsError>>;

/**
 * @brief Query MX records for a domain
 * @param domain The domain to query
 * @return Expected containing MX records or DnsError
 */
[[nodiscard]] auto queryMxRecords(std::string_view domain)
    -> expected<std::vector<MxRecord>, DnsError>;

/**
 * @brief Query SRV records for a service
 * @param service The service name (e.g., "_http._tcp.example.com")
 * @return Expected containing SRV records or DnsError
 */
[[nodiscard]] auto querySrvRecords(std::string_view service)
    -> expected<std::vector<SrvRecord>, DnsError>;

/**
 * @brief Query TXT records for a domain
 * @param domain The domain to query
 * @return Expected containing TXT records or DnsError
 */
[[nodiscard]] auto queryTxtRecords(std::string_view domain)
    -> expected<std::vector<std::string>, DnsError>;

/**
 * @brief Get all local IP addresses of the machine
 * @param includeLoopback Whether to include loopback addresses
 * @param includeIPv6 Whether to include IPv6 addresses
 * @return std::vector<std::string> List of local IP addresses
 */
[[nodiscard]] auto getLocalIPAddresses(bool includeLoopback = false,
                                       bool includeIPv6 = true)
    -> std::vector<std::string>;

/**
 * @brief Get the primary local IP address
 * @param preferIPv6 Whether to prefer IPv6 over IPv4
 * @return The primary IP address or nullopt if none found
 */
[[nodiscard]] auto getPrimaryLocalIP(bool preferIPv6 = false)
    -> std::optional<std::string>;

/**
 * @brief Clear expired entries from the DNS cache
 */
void clearDNSCacheExpiredEntries();

/**
 * @brief Clear all entries from the DNS cache
 */
void clearDnsCache();

/**
 * @brief Get current DNS cache statistics
 * @return Pair of (total entries, expired entries)
 */
[[nodiscard]] auto getDnsCacheStats() -> std::pair<size_t, size_t>;

/**
 * @brief Prefetch DNS records for hostnames
 * @param hostnames Span of hostnames to prefetch
 * @param recordType The DNS record type to prefetch
 */
void prefetchDns(std::span<const std::string> hostnames,
                 DnsRecordType recordType = DnsRecordType::A);

/**
 * @brief Check if a hostname is resolvable
 * @param hostname The hostname to check
 * @param timeout Maximum time to wait
 * @return True if hostname can be resolved
 */
[[nodiscard]] auto isHostnameResolvable(
    std::string_view hostname,
    std::chrono::milliseconds timeout = std::chrono::milliseconds{
        3000}) -> bool;

/**
 * @brief Validate a hostname format
 * @param hostname The hostname to validate
 * @return True if hostname format is valid
 */
[[nodiscard]] constexpr auto isValidHostname(std::string_view hostname) noexcept
    -> bool {
    if (hostname.empty() || hostname.length() > 253) {
        return false;
    }

    size_t labelStart = 0;
    for (size_t i = 0; i <= hostname.length(); ++i) {
        if (i == hostname.length() || hostname[i] == '.') {
            size_t labelLen = i - labelStart;
            if (labelLen == 0 || labelLen > 63) {
                return false;
            }
            // Check first and last character of label
            if (i > labelStart) {
                char first = hostname[labelStart];
                char last = hostname[i - 1];
                if (!((first >= 'a' && first <= 'z') ||
                      (first >= 'A' && first <= 'Z') ||
                      (first >= '0' && first <= '9'))) {
                    return false;
                }
                if (!((last >= 'a' && last <= 'z') ||
                      (last >= 'A' && last <= 'Z') ||
                      (last >= '0' && last <= '9'))) {
                    return false;
                }
            }
            labelStart = i + 1;
        } else {
            char c = hostname[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_')) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_DNS_HPP
