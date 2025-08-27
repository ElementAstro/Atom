/*
 * wifi.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#include "wifi.hpp"
#include <spdlog/spdlog.h>
#include "common.hpp"

#ifdef _WIN32
#include "windows.hpp"
namespace impl = atom::system::windows;
#elif defined(__linux__)
#include "linux.hpp"
namespace impl = atom::system::linux;
#elif defined(__APPLE__)
#include "macos.hpp"
namespace impl = atom::system::macos;
#endif

namespace atom::system {

auto isConnectedToInternet() -> bool {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::isConnectedToInternet_impl();
#else
    spdlog::error( "Unsupported operating system");
    return false;
#endif
}

auto getCurrentWifi() -> std::string {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getCurrentWifi_impl();
#else
    spdlog::error( "Unsupported operating system");
    return {};
#endif
}

auto getCurrentWiredNetwork() -> std::string {
#if defined(_WIN32) || defined(__linux__)
    return impl::getCurrentWiredNetwork_impl();
#elif defined(__APPLE__)
    spdlog::warn( "Getting current wired network is not supported on macOS");
    return {};
#else
    spdlog::error( "Unsupported operating system");
    return {};
#endif
}

auto isHotspotConnected() -> bool {
#if defined(_WIN32) || defined(__linux__)
    return impl::isHotspotConnected_impl();
#elif defined(__APPLE__)
    spdlog::warn( "Checking if connected to a hotspot is not supported on macOS");
    return false;
#else
    spdlog::error( "Unsupported operating system");
    return false;
#endif
}

auto getHostIPs() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getHostIPs_impl();
#else
    spdlog::error( "Unsupported operating system");
    return {};
#endif
}

// Implementation of the template function for IP addresses
template <typename AddressType>
auto getIPAddresses(int addressFamily) -> std::vector<std::string> {
    spdlog::info( "Getting IP addresses for address family: {}", addressFamily);
    std::vector<std::string> addresses;

#ifdef _WIN32
    ULONG bufferSize = 0;
    if (GetAdaptersAddresses(addressFamily, 0, nullptr, nullptr, &bufferSize) !=
        ERROR_BUFFER_OVERFLOW) {
        return addresses;
    }

    auto adapterAddresses =
        std::make_unique<IP_ADAPTER_ADDRESSES[]>(bufferSize);
    if (GetAdaptersAddresses(addressFamily, 0, nullptr, adapterAddresses.get(),
                             &bufferSize) == ERROR_SUCCESS) {
        for (auto adapter = adapterAddresses.get(); adapter;
             adapter = adapter->Next) {
            for (auto ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
                if (ua->Address.lpSockaddr->sa_family == addressFamily) {
                    char ipStr[INET6_ADDRSTRLEN] = {0};
                    void* addr = nullptr;

                    if (addressFamily == AF_INET) {
                        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(ua->Address.lpSockaddr);
                        addr = &(ipv4->sin_addr);
                    } else {
                        struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ua->Address.lpSockaddr);
                        addr = &(ipv6->sin6_addr);
                    }

                    inet_ntop(addressFamily, addr, ipStr, sizeof(ipStr));
                    addresses.emplace_back(ipStr);
                    spdlog::info( "Found IP address: {}", ipStr);
                }
            }
        }
    }
#else
    struct ifaddrs* ifAddrList = nullptr;

    if (getifaddrs(&ifAddrList) == -1) {
        spdlog::error( "getifaddrs failed");
        return addresses;
    }

    // Use smart pointer to automatically manage the lifecycle of ifAddrList
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifAddrListGuard(
        ifAddrList, freeifaddrs);

    for (auto* ifa = ifAddrList; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == addressFamily) {
            char ipStr[INET6_ADDRSTRLEN] = {0};
            void* addr = nullptr;

            if (addressFamily == AF_INET) {
                struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                addr = &(ipv4->sin_addr);
            } else {
                struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
                addr = &(ipv6->sin6_addr);
            }

            inet_ntop(addressFamily, addr, ipStr, sizeof(ipStr));
            addresses.emplace_back(ipStr);
            spdlog::info( "Found IP address: {}", ipStr);
        }
    }
#endif

    return addresses;
}

auto getIPv4Addresses() -> std::vector<std::string> {
    spdlog::info( "Getting IPv4 addresses");
    return getIPAddresses<sockaddr_in>(AF_INET);
}

auto getIPv6Addresses() -> std::vector<std::string> {
    spdlog::info( "Getting IPv6 addresses");
    return getIPAddresses<sockaddr_in6>(AF_INET6);
}

auto getInterfaceNames() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getInterfaceNames_impl();
#else
    spdlog::error( "Unsupported operating system");
    return {};
#endif
}

auto getNetworkStats() -> NetworkStats {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getNetworkStats_impl();
#else
    spdlog::error( "Unsupported operating system");
    return {};
#endif
}

// Placeholder implementations for functions declared in header but not implemented in original file
auto getNetworkHistory(std::chrono::minutes duration) -> std::vector<NetworkStats> {
    spdlog::info( "Getting network history for duration: {} minutes", duration.count());
    // Placeholder implementation
    return {};
}

auto scanAvailableNetworks() -> std::vector<std::string> {
    spdlog::info( "Scanning available networks");
    // Placeholder implementation
    return {};
}

auto getNetworkSecurity() -> std::string {
    spdlog::info( "Getting network security information");
    // Placeholder implementation
    return {};
}

auto measureBandwidth() -> std::pair<double, double> {
    spdlog::info( "Measuring bandwidth");
    // Placeholder implementation
    return {0.0, 0.0};
}

auto analyzeNetworkQuality() -> std::string {
    spdlog::info( "Analyzing network quality");
    // Placeholder implementation
    return {};
}

auto getConnectedDevices() -> std::vector<std::string> {
    spdlog::info( "Getting connected devices");
    // Placeholder implementation
    return {};
}

}  // namespace atom::system
