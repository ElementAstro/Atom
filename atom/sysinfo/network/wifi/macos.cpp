/*
 * macos.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef __APPLE__

#include "macos.hpp"
#include <SystemConfiguration/CaptiveNetwork.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <memory>


namespace atom::system::macos {

auto isConnectedToInternet_impl() -> bool {
    spdlog::debug("Checking internet connection");

    static constexpr const char* TEST_HOST = "8.8.8.8";
    static constexpr int TEST_PORT = 80;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        spdlog::error("Failed to create socket");
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(TEST_PORT);

    bool connected = false;
    if (inet_pton(AF_INET, TEST_HOST, &server.sin_addr) == 1) {
        connected =
            connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0;
    }

    close(sock);

    spdlog::debug("Internet connection: {}",
                  connected ? "available" : "unavailable");
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    spdlog::debug("Getting current WiFi connection");

    CFArrayRef interfaces = CNCopySupportedInterfaces();
    if (!interfaces) {
        spdlog::error("CNCopySupportedInterfaces failed");
        return {};
    }

    std::unique_ptr<const __CFArray, decltype(&CFRelease)> interfacesGuard(
        interfaces, CFRelease);

    if (CFArrayGetCount(interfaces) == 0) {
        spdlog::debug("No WiFi interfaces found");
        return {};
    }

    CFStringRef interfaceName =
        static_cast<CFStringRef>(CFArrayGetValueAtIndex(interfaces, 0));
    CFDictionaryRef info = CNCopyCurrentNetworkInfo(interfaceName);

    if (!info) {
        spdlog::debug("No current network info available");
        return {};
    }

    std::unique_ptr<const __CFDictionary, decltype(&CFRelease)> infoGuard(
        info, CFRelease);

    CFStringRef ssid = static_cast<CFStringRef>(
        CFDictionaryGetValue(info, kCNNetworkInfoKeySSID));
    if (!ssid) {
        spdlog::debug("No SSID found in network info");
        return {};
    }

    char buffer[256];
    if (!CFStringGetCString(ssid, buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        spdlog::error("Failed to convert SSID to C string");
        return {};
    }

    std::string wifiName = buffer;
    spdlog::debug("Current WiFi: {}", wifiName);
    return wifiName;
}

auto getCurrentWiredNetwork_impl() -> std::string {
    spdlog::debug("Getting current wired network connection");

    ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("getifaddrs failed");
        return {};
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifaddrGuard(ifaddr,
                                                                 freeifaddrs);

    static const std::vector<std::string> wiredPrefixes = {"en", "eth"};

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_name || !(ifa->ifa_flags & IFF_UP) ||
            (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }

        std::string interface = ifa->ifa_name;
        bool isWired = false;

        for (const auto& prefix : wiredPrefixes) {
            if (interface.substr(0, prefix.length()) == prefix) {
                isWired = true;
                break;
            }
        }

        if (isWired && ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            spdlog::debug("Current wired network: {}", interface);
            return interface;
        }
    }

    spdlog::debug("No active wired connection found");
    return {};
}

auto isHotspotConnected_impl() -> bool {
    spdlog::debug("Checking if connected to a hotspot");

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("networksetup -listallhardwareports 2>/dev/null | grep -A 1 "
              "'Wi-Fi' | grep 'Device:' | awk '{print $2}'",
              "r"),
        pclose);

    if (!pipe) {
        spdlog::error("Failed to execute networksetup command");
        return false;
    }

    char interface[64];
    if (fgets(interface, sizeof(interface), pipe.get())) {
        interface[strcspn(interface, "\n")] = 0;

        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "networksetup -getairportnetwork %s 2>/dev/null", interface);

        std::unique_ptr<FILE, decltype(&pclose)> wifiPipe(popen(cmd, "r"),
                                                          pclose);
        if (wifiPipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), wifiPipe.get())) {
                std::string output = buffer;
                static const std::vector<std::string> hotspotPatterns = {
                    "iPhone", "Android", "Hotspot", "DIRECT-"};

                for (const auto& pattern : hotspotPatterns) {
                    if (output.find(pattern) != std::string::npos) {
                        spdlog::debug(
                            "Hotspot detected: SSID pattern match ({})",
                            pattern);
                        return true;
                    }
                }
            }
        }
    }

    spdlog::debug("No hotspot connection detected");
    return false;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    spdlog::debug("Getting host IP addresses");

    std::vector<std::string> hostIPs;
    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("getifaddrs failed");
        return hostIPs;
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifaddrGuard(ifaddr,
                                                                 freeifaddrs);

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;

        int family = ifa->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6)
            continue;

        char ipstr[INET6_ADDRSTRLEN];
        void* addr;

        if (family == AF_INET) {
            addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
        } else {
            addr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
        }

        if (inet_ntop(family, addr, ipstr, sizeof(ipstr))) {
            std::string ip = ipstr;
            if (ip != "127.0.0.1" && ip != "::1") {
                hostIPs.emplace_back(std::move(ip));
                spdlog::debug("Found IP address: {}", hostIPs.back());
            }
        }
    }

    return hostIPs;
}

auto getInterfaceNames_impl() -> std::vector<std::string> {
    spdlog::debug("Getting interface names");

    std::vector<std::string> interfaceNames;
    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("getifaddrs failed");
        return interfaceNames;
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifaddrGuard(ifaddr,
                                                                 freeifaddrs);

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name) {
            std::string name = ifa->ifa_name;
            if (std::find(interfaceNames.begin(), interfaceNames.end(), name) ==
                interfaceNames.end()) {
                interfaceNames.emplace_back(std::move(name));
                spdlog::debug("Found interface: {}", interfaceNames.back());
            }
        }
    }

    return interfaceNames;
}

auto measurePing_impl(const std::string& host, int timeout) -> float {
    spdlog::debug("Measuring ping to host: {}, timeout: {} ms", host, timeout);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -t %d %s 2>/dev/null",
             std::max(1, timeout / 1000), host.c_str());

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to execute ping command");
        return -1.0f;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        char* timePos = strstr(buffer, "time=");
        if (!timePos) {
            timePos = strstr(buffer, "time ");
        }

        if (timePos) {
            timePos += (timePos[4] == '=') ? 5 : 5;
            float latency = std::strtof(timePos, nullptr);
            spdlog::debug("Ping successful, latency: {:.1f} ms", latency);
            return latency;
        }
    }

    spdlog::error("Ping failed for host: {}", host);
    return -1.0f;
}

auto getNetworkStats_impl() -> NetworkStats {
    spdlog::debug("Getting network statistics");

    NetworkStats stats{};

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("netstat -ibn | grep -v Name | awk '{if($1!=\"lo0\") {recv+=$7; "
              "sent+=$10}} END {print recv, sent}'",
              "r"),
        pclose);

    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe.get())) {
            unsigned long long recv, sent;
            if (sscanf(buffer, "%llu %llu", &recv, &sent) == 2) {
                stats.downloadSpeed = recv / (1024.0 * 1024.0);
                stats.uploadSpeed = sent / (1024.0 * 1024.0);
            }
        }
    }

    stats.latency = measurePing_impl("8.8.8.8", 1000);

    std::unique_ptr<FILE, decltype(&pclose)> wifiPipe(
        popen("system_profiler SPAirPortDataType 2>/dev/null | grep 'Signal / "
              "Noise' | head -1 | awk '{print $4}'",
              "r"),
        pclose);

    if (wifiPipe) {
        char buffer[64];
        if (fgets(buffer, sizeof(buffer), wifiPipe.get())) {
            stats.signalStrength = std::strtof(buffer, nullptr);
        }
    }

    stats.packetLoss = 0.0;

    spdlog::debug(
        "Network stats - Download: {:.2f} MB/s, Upload: {:.2f} MB/s, "
        "Latency: {:.1f} ms, Signal: {:.1f} dBm",
        stats.downloadSpeed, stats.uploadSpeed, stats.latency,
        stats.signalStrength);

    return stats;
}

}  // namespace atom::system::macos

#endif  // __APPLE__
