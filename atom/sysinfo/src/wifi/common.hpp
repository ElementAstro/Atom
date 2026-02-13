/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Common WiFi Definitions

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP
#define ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

#include "atom/macro.hpp"

// Platform-specific includes
#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <wlanapi.h>
#include <ws2tcpip.h>
#include <pdh.h>
#include <icmpapi.h>
#undef max
#undef min
// clang-format on
#if !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "icmp.lib")
#endif
#elif defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/CaptiveNetwork.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"

// Define common types for cross-platform use
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
using IF_ADDRS = IP_ADAPTER_ADDRESSES*;
using IF_ADDRS_UNICAST = IP_ADAPTER_UNICAST_ADDRESS*;
#else
using IF_ADDRS = struct ifaddrs*;
using IF_ADDRS_UNICAST = struct ifaddrs*;
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

namespace atom::system {

// Configuration constants
namespace config {
    constexpr int DEFAULT_PING_TIMEOUT_MS = 5000;
    constexpr int DEFAULT_CONNECT_TIMEOUT_MS = 5000;
    constexpr size_t DEFAULT_CACHE_SIZE = 100;
    constexpr std::chrono::seconds DEFAULT_CACHE_TTL{30};
    constexpr int MAX_PING_ATTEMPTS = 3;
    constexpr size_t MAX_BUFFER_SIZE = 65536;
}

// Error codes for WiFi operations
enum class WiFiError {
    SUCCESS = 0,
    PLATFORM_NOT_SUPPORTED,
    NETWORK_INTERFACE_ERROR,
    PERMISSION_DENIED,
    TIMEOUT,
    INVALID_PARAMETER,
    MEMORY_ALLOCATION_FAILED,
    SYSTEM_CALL_FAILED,
    NETWORK_UNREACHABLE,
    CACHE_MISS
};

// Network interface information
struct NetworkInterface {
    std::string name;
    std::string description;
    std::vector<std::string> ipv4_addresses;
    std::vector<std::string> ipv6_addresses;
    bool is_up;
    bool is_wireless;
    std::optional<std::string> mac_address;
} ATOM_ALIGNAS(32);

// Cache entry for network data
template<typename T>
struct CacheEntry {
    T data;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::seconds ttl;

    ATOM_NODISCARD auto is_expired() const -> bool {
        return std::chrono::steady_clock::now() - timestamp > ttl;
    }
};

// Thread-safe cache for network information
template<typename T>
class NetworkCache {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CacheEntry<T>> cache_;
    size_t max_size_;

public:
    explicit NetworkCache(size_t max_size = config::DEFAULT_CACHE_SIZE)
        : max_size_(max_size) {}

    void put(const std::string& key, T data,
             std::chrono::seconds ttl = config::DEFAULT_CACHE_TTL) {
        std::lock_guard lock(mutex_);
        if (cache_.size() >= max_size_) {
            // Simple LRU: remove oldest entry
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.timestamp < oldest->second.timestamp) {
                    oldest = it;
                }
            }
            cache_.erase(oldest);
        }
        cache_[key] = {std::move(data), std::chrono::steady_clock::now(), ttl};
    }

    ATOM_NODISCARD auto get(const std::string& key) -> std::optional<T> {
        std::lock_guard lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end() && !it->second.is_expired()) {
            return it->second.data;
        }
        if (it != cache_.end()) {
            cache_.erase(it);  // Remove expired entry
        }
        return std::nullopt;
    }

    void clear() {
        std::lock_guard lock(mutex_);
        cache_.clear();
    }
};

// Free addresses helper function
void freeAddresses(IF_ADDRS addrs);

// Get addresses helper function
ATOM_NODISCARD auto getAddresses(int family, IF_ADDRS* addrs) -> int;

// Helper function to measure ping latency with improved error handling
ATOM_NODISCARD auto measurePing(const std::string& host, int timeout_ms = config::DEFAULT_PING_TIMEOUT_MS) -> std::pair<float, WiFiError>;

// Helper function to check if a string is a valid IP address
ATOM_NODISCARD auto isValidIPAddress(const std::string& ip) -> bool;

// Helper function to get network interface information
ATOM_NODISCARD auto getNetworkInterfaces() -> std::vector<NetworkInterface>;

// Helper function to convert WiFiError to string
ATOM_NODISCARD auto wifiErrorToString(WiFiError error) -> std::string;

// Template function to get IP addresses by family with error handling
template <typename AddressType>
ATOM_NODISCARD auto getIPAddresses(int addressFamily) -> std::pair<std::vector<std::string>, WiFiError>;

// Utility function to execute system commands safely
ATOM_NODISCARD auto executeCommand(const std::string& command, int timeout_ms = 5000) -> std::pair<std::string, WiFiError>;

// Network speed measurement utilities
struct SpeedTestResult {
    double download_mbps;
    double upload_mbps;
    double latency_ms;
    WiFiError error;
} ATOM_ALIGNAS(16);

ATOM_NODISCARD auto measureNetworkSpeed(const std::string& test_host = "8.8.8.8") -> SpeedTestResult;

}  // namespace atom::system

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif  // ATOM_SYSTEM_MODULE_WIFI_COMMON_HPP
