/*
 * linux.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef __linux__

#include "linux.hpp"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>

namespace atom::system::linux {
using PipeCloser = int (*)(FILE*);

// Static caches for improved performance
static NetworkCache<std::string> wifi_cache(50);
static NetworkCache<std::string> wired_cache(50);
static NetworkCache<std::vector<std::string>> interface_cache(10);
static NetworkCache<NetworkStats> stats_cache(20);

auto isConnectedToInternet_impl() -> bool {
    LOG_F(INFO, "Checking internet connection");

    // Check cache first
    static std::string cache_key = "internet_connection";
    if (auto cached = wifi_cache.get(cache_key)) {
        return *cached == "true";
    }

    static constexpr const char* TEST_HOST = "8.8.8.8";
    static constexpr int TEST_PORT = 80;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        LOG_F(ERROR, "Failed to create socket: {}", strerror(errno));
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_F(WARNING, "Failed to set socket timeout");
    }

    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(TEST_PORT);

    bool connected = false;
    if (inet_pton(AF_INET, TEST_HOST, &server.sin_addr) == 1) {
        connected = connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0;
    }

    close(sock);

    // Cache the result for 30 seconds
    wifi_cache.put(cache_key, connected ? "true" : "false", std::chrono::seconds(30));

    LOG_F(INFO, "Internet connection: {}", connected ? "available" : "unavailable");
    return connected;
}

auto getCurrentWifi_impl() -> std::string {
    LOG_F(INFO, "Getting current WiFi connection");

    // Check cache first
    static std::string cache_key = "current_wifi";
    if (auto cached = wifi_cache.get(cache_key)) {
        return *cached;
    }

    std::ifstream file("/proc/net/wireless");
    if (!file.is_open()) {
        LOG_F(INFO, "No wireless interfaces found");
        return {};
    }

    std::string line;
    std::getline(file, line);  // Skip header lines
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));
        interface.erase(interface.find_last_not_of(" \t") + 1);

        // Use improved command execution
        std::string cmd = "iwgetid " + interface + " -r 2>/dev/null";
        auto [output, error] = executeCommand(cmd, 3000);

        if (error == WiFiError::SUCCESS && !output.empty()) {
            std::string wifiName = output;
            // Remove trailing newline
            if (!wifiName.empty() && wifiName.back() == '\n') {
                wifiName.pop_back();
            }

            if (!wifiName.empty()) {
                // Cache the result for 15 seconds
                wifi_cache.put(cache_key, wifiName, std::chrono::seconds(15));
                LOG_F(INFO, "Current WiFi: {}", wifiName);
                return wifiName;
            }
        }
    }

    LOG_F(INFO, "No active WiFi connection found");
    wifi_cache.put(cache_key, "", std::chrono::seconds(5));  // Cache negative result briefly
    return {};
}

auto getCurrentWiredNetwork_impl() -> std::string {
    LOG_F(INFO, "Getting current wired network connection");

    // Check cache first
    static std::string cache_key = "current_wired";
    if (auto cached = wired_cache.get(cache_key)) {
        return *cached;
    }

    static const std::vector<std::string> wiredPrefixes = {"en", "eth", "em"};

    std::ifstream netDir("/proc/net/dev");
    if (!netDir.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/net/dev");
        return {};
    }

    std::string line;
    std::getline(netDir, line);  // Skip header lines
    std::getline(netDir, line);

    while (std::getline(netDir, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));

        bool isWired = false;
        for (const auto& prefix : wiredPrefixes) {
            if (interface.length() >= prefix.length() &&
                interface.substr(0, prefix.length()) == prefix) {
                isWired = true;
                break;
            }
        }

        if (!isWired)
            continue;

        std::string statePath = "/sys/class/net/" + interface + "/operstate";
        std::ifstream stateFile(statePath);
        if (stateFile.is_open()) {
            std::string state;
            std::getline(stateFile, state);
            if (state == "up") {
                // Cache the result for 20 seconds
                wired_cache.put(cache_key, interface, std::chrono::seconds(20));
                LOG_F(INFO, "Current wired network: {}", interface);
                return interface;
            }
        }
    }

    LOG_F(INFO, "No active wired connection found");
    wired_cache.put(cache_key, "", std::chrono::seconds(5));  // Cache negative result briefly
    return {};
}

auto isHotspotConnected_impl() -> bool {
    LOG_F(INFO, "Checking if connected to a hotspot");

    // Check for AP mode interfaces
    auto [output1, error1] = executeCommand("iw dev 2>/dev/null | grep -A 2 Interface | grep -i 'type ap'", 3000);
    if (error1 == WiFiError::SUCCESS && !output1.empty()) {
        LOG_F(INFO, "Hotspot detected: AP mode interface found");
        return true;
    }

    // Check for master mode interfaces
    auto [output2, error2] = executeCommand("iwconfig 2>/dev/null | grep -i 'mode:master'", 3000);
    if (error2 == WiFiError::SUCCESS && !output2.empty()) {
        LOG_F(INFO, "Hotspot detected: master mode interface found");
        return true;
    }

    LOG_F(INFO, "No hotspot connection detected");
    return false;
}

auto getHostIPs_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting host IP addresses");

    // Check cache first
    static std::string cache_key = "host_ips";
    if (auto cached = interface_cache.get(cache_key)) {
        return *cached;
    }

    std::vector<std::string> hostIPs;
    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_F(ERROR, "getifaddrs failed: {}", strerror(errno));
        return hostIPs;
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifaddrGuard(ifaddr, freeifaddrs);

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
                LOG_F(INFO, "Found IP address: {}", hostIPs.back());
            }
        }
    }

    // Cache the result for 60 seconds
    interface_cache.put(cache_key, hostIPs, std::chrono::seconds(60));
    return hostIPs;
}

auto getInterfaceNames_impl() -> std::vector<std::string> {
    LOG_F(INFO, "Getting interface names");

    // Check cache first
    static std::string cache_key = "interface_names";
    if (auto cached = interface_cache.get(cache_key)) {
        return *cached;
    }

    std::vector<std::string> interfaceNames;
    ifaddrs* ifaddr;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_F(ERROR, "getifaddrs failed: {}", strerror(errno));
        return interfaceNames;
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> ifaddrGuard(ifaddr, freeifaddrs);

    for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name) {
            std::string name = ifa->ifa_name;
            if (std::find(interfaceNames.begin(), interfaceNames.end(), name) ==
                interfaceNames.end()) {
                interfaceNames.emplace_back(std::move(name));
                LOG_F(INFO, "Found interface: {}", interfaceNames.back());
            }
        }
    }

    // Cache the result for 120 seconds
    interface_cache.put(cache_key, interfaceNames, std::chrono::seconds(120));
    return interfaceNames;
}

auto measurePing_impl(const std::string& host, int timeout) -> float {
    LOG_F(INFO, "Measuring ping to host: {}, timeout: {} ms", host, timeout);

    // Use the improved measurePing function from common
    auto [latency, error] = measurePing(host, timeout);
    if (error == WiFiError::SUCCESS) {
        return latency;
    }

    LOG_F(ERROR, "Ping failed for host: {} - {}", host, wifiErrorToString(error));
    return -1.0f;
}

auto getNetworkStats_impl() -> NetworkStats {
    LOG_F(INFO, "Getting network statistics");

    // Check cache first
    static std::string cache_key = "network_stats";
    if (auto cached = stats_cache.get(cache_key)) {
        return *cached;
    }

    NetworkStats stats{};

    std::ifstream netdev("/proc/net/dev");
    if (!netdev.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/net/dev");
        return stats;
    }

    std::string line;
    std::getline(netdev, line);  // Skip header lines
    std::getline(netdev, line);

    unsigned long long totalBytesRecv = 0, totalBytesSent = 0;

    while (std::getline(netdev, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));

        if (interface == "lo")  // Skip loopback
            continue;

        std::istringstream iss(line.substr(line.find(':') + 1));
        unsigned long long recv, sent, dummy;

        // Parse network statistics from /proc/net/dev
        iss >> recv >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> sent;

        totalBytesRecv += recv;
        totalBytesSent += sent;
    }

    // Convert to MB (total accumulated, not per second)
    stats.downloadSpeed = totalBytesRecv / (1024.0 * 1024.0);
    stats.uploadSpeed = totalBytesSent / (1024.0 * 1024.0);

    // Measure latency
    stats.latency = measurePing_impl("8.8.8.8", 1000);

    // Get signal strength for wireless interfaces
    auto [output, error] = executeCommand("iwconfig 2>/dev/null | grep 'Signal level'", 3000);
    if (error == WiFiError::SUCCESS && !output.empty()) {
        std::regex signalRegex(R"(Signal level=(-?\d+(?:\.\d+)?))");
        std::smatch match;
        if (std::regex_search(output, match, signalRegex)) {
            stats.signalStrength = std::stof(match[1].str());
        }
    }

    // TODO: Implement actual packet loss measurement
    stats.packetLoss = 0.0;

    // Cache the result for 10 seconds
    stats_cache.put(cache_key, stats, std::chrono::seconds(10));

    LOG_F(INFO, "Network stats - Download: {:.2f} MB, Upload: {:.2f} MB, "
               "Latency: {:.1f} ms, Signal: {:.1f} dBm",
               stats.downloadSpeed, stats.uploadSpeed, stats.latency, stats.signalStrength);

    return stats;
}

}  // namespace atom::system::linux

#endif  // __linux__
