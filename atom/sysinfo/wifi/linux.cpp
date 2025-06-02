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
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>


namespace atom::system::linux {

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

    std::ifstream file("/proc/net/wireless");
    if (!file.is_open()) {
        spdlog::debug("No wireless interfaces found");
        return {};
    }

    std::string line;
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));
        interface.erase(interface.find_last_not_of(" \t") + 1);

        std::string cmd = "iwgetid " + interface + " -r 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                      pclose);

        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe.get())) {
                std::string wifiName = buffer;
                if (!wifiName.empty() && wifiName.back() == '\n') {
                    wifiName.pop_back();
                }

                if (!wifiName.empty()) {
                    spdlog::debug("Current WiFi: {}", wifiName);
                    return wifiName;
                }
            }
        }
    }

    spdlog::debug("No active WiFi connection found");
    return {};
}

auto getCurrentWiredNetwork_impl() -> std::string {
    spdlog::debug("Getting current wired network connection");

    static const std::vector<std::string> wiredPrefixes = {"en", "eth", "em"};

    std::ifstream netDir("/proc/net/dev");
    if (!netDir.is_open()) {
        spdlog::error("Failed to open /proc/net/dev");
        return {};
    }

    std::string line;
    std::getline(netDir, line);
    std::getline(netDir, line);

    while (std::getline(netDir, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));

        bool isWired = false;
        for (const auto& prefix : wiredPrefixes) {
            if (interface.substr(0, prefix.length()) == prefix) {
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
                spdlog::debug("Current wired network: {}", interface);
                return interface;
            }
        }
    }

    spdlog::debug("No active wired connection found");
    return {};
}

auto isHotspotConnected_impl() -> bool {
    spdlog::debug("Checking if connected to a hotspot");

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("iw dev 2>/dev/null | grep -A 2 Interface | grep -i 'type ap'",
              "r"),
        pclose);

    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe.get())) {
            spdlog::debug("Hotspot detected: AP mode interface found");
            return true;
        }
    }

    pipe.reset(popen("iwconfig 2>/dev/null | grep -i 'mode:master'", "r"));
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe.get())) {
            spdlog::debug("Hotspot detected: master mode interface found");
            return true;
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
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W %d %s 2>/dev/null",
             std::max(1, timeout / 1000), host.c_str());

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to execute ping command");
        return -1.0f;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        char* timePos = strstr(buffer, "time=");
        if (timePos) {
            timePos += 5;
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

    std::ifstream netdev("/proc/net/dev");
    if (!netdev.is_open()) {
        spdlog::error("Failed to open /proc/net/dev");
        return stats;
    }

    std::string line;
    std::getline(netdev, line);
    std::getline(netdev, line);

    unsigned long long totalBytesRecv = 0, totalBytesSent = 0;

    while (std::getline(netdev, line)) {
        if (line.find(':') == std::string::npos)
            continue;

        std::string interface = line.substr(0, line.find(':'));
        interface.erase(0, interface.find_first_not_of(" \t"));

        if (interface == "lo")
            continue;

        std::istringstream iss(line.substr(line.find(':') + 1));
        unsigned long long recv, sent, dummy;

        iss >> recv >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >>
            dummy >> sent;

        totalBytesRecv += recv;
        totalBytesSent += sent;
    }

    stats.downloadSpeed = totalBytesRecv / (1024.0 * 1024.0);
    stats.uploadSpeed = totalBytesSent / (1024.0 * 1024.0);

    stats.latency = measurePing_impl("8.8.8.8", 1000);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("iwconfig 2>/dev/null | grep 'Signal level'", "r"), pclose);

    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe.get())) {
            char* levelPos = strstr(buffer, "Signal level=");
            if (levelPos) {
                levelPos += 13;
                stats.signalStrength = std::strtof(levelPos, nullptr);
                break;
            }
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

}  // namespace atom::system::linux

#endif  // __linux__
