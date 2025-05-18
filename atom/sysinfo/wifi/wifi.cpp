/*
 * wifi.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#include "atom/sysinfo/wifi/wifi.hpp"
#include "atom/sysinfo/wifi/common.hpp"

#ifdef _WIN32
#include "atom/sysinfo/wifi/windows.hpp"
namespace impl = atom::system::windows;
#elif defined(__linux__)
#include "atom/sysinfo/wifi/linux.hpp"
namespace impl = atom::system::linux;
#elif defined(__APPLE__)
#include "atom/sysinfo/wifi/macos.hpp"
namespace impl = atom::system::macos;
#endif

namespace atom::system {

auto isConnectedToInternet() -> bool {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::isConnectedToInternet_impl();
#else
    LOG_F(ERROR, "Unsupported operating system");
    return false;
#endif
}

auto getCurrentWifi() -> std::string {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getCurrentWifi_impl();
#else
    LOG_F(ERROR, "Unsupported operating system");
    return {};
#endif
}

auto getCurrentWiredNetwork() -> std::string {
#if defined(_WIN32) || defined(__linux__)
    return impl::getCurrentWiredNetwork_impl();
#elif defined(__APPLE__)
    LOG_F(WARNING, "Getting current wired network is not supported on macOS");
    return {};
#else
    LOG_F(ERROR, "Unsupported operating system");
    return {};
#endif
}

auto isHotspotConnected() -> bool {
#if defined(_WIN32) || defined(__linux__)
    return impl::isHotspotConnected_impl();
#elif defined(__APPLE__)
    LOG_F(WARNING, "Checking if connected to a hotspot is not supported on macOS");
    return false;
#else
    LOG_F(ERROR, "Unsupported operating system");
    return false;
#endif
}

auto getHostIPs() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getHostIPs_impl();
#else
    LOG_F(ERROR, "Unsupported operating system");
    return {};
#endif
}

// Implementation of the template function for IP addresses
template <typename AddressType>
auto getIPAddresses(int addressFamily) -> std::vector<std::string> {
    LOG_F(INFO, "Getting IP addresses for address family: {}", addressFamily);
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
                    LOG_F(INFO, "Found IP address: {}", ipStr);
                }
            }
        }
    }
#else
    struct ifaddrs* ifAddrList = nullptr;

    if (getifaddrs(&ifAddrList) == -1) {
        LOG_F(ERROR, "getifaddrs failed");
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
            LOG_F(INFO, "Found IP address: {}", ipStr);
        }
    }
#endif

    return addresses;
}

auto getIPv4Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "Getting IPv4 addresses");
    return getIPAddresses<sockaddr_in>(AF_INET);
}

auto getIPv6Addresses() -> std::vector<std::string> {
    LOG_F(INFO, "Getting IPv6 addresses");
    return getIPAddresses<sockaddr_in6>(AF_INET6);
}

auto getInterfaceNames() -> std::vector<std::string> {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getInterfaceNames_impl();
#else
    LOG_F(ERROR, "Unsupported operating system");
    return {};
#endif
}

auto getNetworkStats() -> NetworkStats {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return impl::getNetworkStats_impl();
#else
    LOG_F(ERROR, "Unsupported operating system");
    return {};
#endif
}

// Placeholder implementations for functions declared in header but not implemented in original file
auto getNetworkHistory(std::chrono::minutes duration) -> std::vector<NetworkStats> {
    LOG_F(INFO, "Getting network history for duration: {} minutes", duration.count());
    // Placeholder implementation
    return {};
}

auto scanAvailableNetworks() -> std::vector<std::string> {
    LOG_F(INFO, "Scanning available networks");
    // Placeholder implementation
    return {};
}

auto getNetworkSecurity() -> std::string {
    LOG_F(INFO, "Getting network security information");
    // Placeholder implementation
    return {};
}

auto measureBandwidth() -> std::pair<double, double> {
    LOG_F(INFO, "Measuring bandwidth");
    // Placeholder implementation
    return {0.0, 0.0};
}

auto analyzeNetworkQuality() -> std::string {
    LOG_F(INFO, "Analyzing network quality");
    // Placeholder implementation
    return {};
}

auto getConnectedDevices() -> std::vector<std::string> {
    LOG_F(INFO, "Getting connected devices");
    // Placeholder implementation
    return {};
}

}  // namespace atom::system
