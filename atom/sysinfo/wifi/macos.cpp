/*
 * macos.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - macOS WiFi Implementation

**************************************************/

#ifdef __APPLE__

#include "macos.hpp"

namespace atom::system::macos {

auto isConnectedToInternet_impl() -> bool {
    LOG_F(INFO, "Checking internet connection");
    bool connected = false;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(80);
        if (inet_pton(AF_INET, "8.8.8.8", &(server.sin_addr)) != -1) {
            if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != -1) {
                connected = true;
                LOG_F(INFO, "Connected to internet");
            } else {
                LOG_F(ERROR, "Failed to connect to internet");
            }
            close(sock);
        } else {
            LOG_F(ERROR, "inet_pton failed");
            close(sock);
        }
    } else {
        LOG_F(ERROR, "Failed to create socket");
    }
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    LOG_F(INFO, "Getting current WiFi connection");
    std::string wifiName;

    CFArrayRef interfaces = CNCopySupportedInterfaces();
    if (interfaces != nullptr) {
        CFDictionaryRef info =
            CNCopyCurrentNetworkInfo(CFArrayGetValueAtIndex(interfaces, 0));
        if (info != nullptr) {
            CFStringRef ssid = static_cast<CFStringRef>(
                CFDictionaryGetValue(info, kCNNetworkInfoKeySSID));
            if (ssid != nullptr) {
                char buffer[256];
                if (CFStringGetCString(ssid, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                    wifiName = buffer;
                }
            }
            CFRelease(info);
        }
        CFRelease(interfaces);
    } else {
        LOG_F(ERROR, "CNCopySupportedInterfaces failed");
    }
    
    LOG_F(INFO, "Current WiFi: {}", wifiName);
    return wifiName;
}

auto getCurrentWiredNetwork_impl() -> std::string {
    LOG_F(WARNING, "Getting current wired network is not supported on macOS");
    return "";
}

auto isHotspotConnected_impl() -> bool {
    LOG_F(WARNING, "Checking if connected to a hotspot is not supported on macOS");
    return false;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting host IP addresses");
    std::vector<std::string> hostIPs;

    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_F(ERROR, "getifaddrs failed");
        return hostIPs;
    }

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            std::array<char, INET6_ADDRSTRLEN> ipstr{};
            void* addr;

            if (family == AF_INET) {
                addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            } else {
                addr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
            }

            inet_ntop(family, addr, ipstr.data(), ipstr.size());
            hostIPs.emplace_back(ipstr.data());
            LOG_F(INFO, "Found IP address: {}", ipstr.data());
        }
    }

    freeifaddrs(ifaddr);
    return hostIPs;
}

auto getInterfaceNames_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting interface names");
    std::vector<std::string> interfaceNames;
    IF_ADDRS allAddrs = nullptr;

    if (atom::system::getAddresses(AF_UNSPEC, &allAddrs) != 0) {
        LOG_F(ERROR, "getAddresses failed");
        return interfaceNames;
    }

    for (auto* addr = allAddrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_name != nullptr) {
            interfaceNames.emplace_back(addr->ifa_name);
            LOG_F(INFO, "Found interface: {}", addr->ifa_name);
        }
    }

    if (allAddrs != nullptr) {
        atom::system::freeAddresses(allAddrs);
    }
    return interfaceNames;
}

auto getNetworkStats_impl() -> NetworkStats {
    LOG_F(INFO, "Getting network statistics");
    NetworkStats stats{};
    
    // macOS doesn't have /proc/net/dev, we'd need to use IOKit or other APIs
    // This is a simplified placeholder implementation
    
    // 测量网络延迟
    std::string host = "8.8.8.8";
    int timeout = 1000;
    
    // Execute ping command via shell
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -t %d %s 2>/dev/null", timeout / 1000,
             host.c_str());

    FILE* pipe = popen(cmd, "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (strstr(buffer, "time=") || strstr(buffer, "time ")) {
                char* timePos = strstr(buffer, "time=");
                if (!timePos) {
                    timePos = strstr(buffer, "time ");
                }
                if (timePos) {
                    timePos += 5; // Skip "time=" or "time "
                    stats.latency = std::strtof(timePos, nullptr);
                }
            }
        }
        pclose(pipe);
    }
    
    // We'd need to use IOKit/framework APIs for real network stats
    // This just returns placeholder values

    LOG_F(INFO, "Network stats - Latency: {:.1f} ms", stats.latency);

    return stats;
}

} // namespace atom::system::macos

#endif // __APPLE__
